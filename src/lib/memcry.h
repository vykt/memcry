#ifndef MEMCRY_H
#define MEMCRY_H

#ifdef __cplusplus
extern "C"{
#endif

//standard library
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

//system headers
#include <unistd.h>
#include <linux/limits.h>

//external libraries
#include <cmore.h>


//easy extract from a cm_list_node pointer
#define MC_GET_NODE_AREA(node)  ((mc_vm_area *) (node->data))
#define MC_GET_NODE_OBJ(node)   ((mc_vm_obj *) (node->data))
#define MC_GET_NODE_PTR(node)   *((cm_lst_node **) (node->data))

//mc_vm_area.access bitmasks
#define MC_ACCESS_READ    0x01
#define MC_ACCESS_WRITE   0x02
#define MC_ACCESS_EXEC    0x04
#define MC_ACCESS_SHARED  0x08

//pseudo object id
#define MC_ZERO_OBJ_ID -1

//-Wsign-compare '-1'
#define MC_UNDEF_ADDR UINTPTR_MAX


//interface types
enum mc_iface_type {
    PROCFS = 0,
    KRNCRY = 1
};


/*
 *  --- [DATA TYPES] ---
 */

// [memory area]
typedef struct {

    char * pathname;
    char * basename;

    uintptr_t start_addr;
    uintptr_t end_addr;

    cm_byte access; //logically AND with MC_ACCESS macros

    //mutually exclusive
    cm_lst_node * obj_node_p;      //STORES: parent mc_vm_obj *
    cm_lst_node * last_obj_node_p; //STORES: last encountered mc_vm_obj *

    int id;
    bool mapped; //set to false when a map update discovers area to be unmapped

} mc_vm_area;


// [backing object]
typedef struct {

    uintptr_t start_addr;
    uintptr_t end_addr;

    cm_lst vm_area_node_ps;      //STORES: cm_lst_node * of mc_vm_area
    cm_lst last_vm_area_node_ps; //STORES: cm_lst_node * of mc_vm_area

    int id;
    bool mapped; //set to false when a map update discovers obj. to be unmapped

    char * basename; //[restrict .NAME_MAX]
    char * pathname; //[restrict .PATH_MAX]

} mc_vm_obj;


// [memory map]
typedef struct {

    //up to date entries
    cm_lst vm_areas;   //STORES: mc_vm_area
    cm_lst vm_objs;    //STORES: mc_vm_obj

    //unmapped entries (storage for future deallocation)
    cm_lst vm_areas_unmapped; //STORES: cm_list_node * of mc_vm_area
    cm_lst vm_objs_unmapped;  //STORES: cm_list_node * of mc_vm_obj

    // [internal]
    int next_id_area;
    int next_id_obj;

} mc_vm_map;



// [session]
struct _mc_session;

typedef struct {

    int (*open)(struct _mc_session *, const int);
    int (*close)(struct _mc_session *);
    int (*update_map)(const struct _mc_session *, mc_vm_map *);
    int (*read)(const struct _mc_session *,
                const uintptr_t, cm_byte *, const size_t);
    int (*write)(const struct _mc_session *,
                 const uintptr_t, const cm_byte *, const size_t);

} mc_iface;


struct _mc_session {

    union {
        struct {
            int fd_mem;
            pid_t pid;
        }; //procfs_data
        struct {
            char major;
            int fd_dev_krncry;
        }; //krncry_data
    };

    long page_size;
    mc_iface iface;

}; 
typedef struct _mc_session mc_session;



/*
 *  --- [FUNCTIONS] ---
 */

// [util]
//return: basename = success, NULL = fail/error
extern char * mc_pathname_to_basename(const char * pathname);
//must destroy 'pid_vector' manually on success | pid = success, -1 = fail/error
extern pid_t mc_pid_by_name(const char * comm, cm_vct * pid_vector);
//`name_buf` must be at least NAME_MAX in size.
//return: 0 = success, -1 = fail/error
extern int mc_name_by_pid(const pid_t pid, char * name_buf);
//'out' must have space for double the length of 'inp' + 1
extern void mc_bytes_to_hex(const cm_byte * inp, const int inp_len, char * out);


// [virtual interface]
//all return 0 = success, -1 = fail/error
extern int mc_open(mc_session * session,
                   const enum mc_iface_type iface, const pid_t pid);
extern int mc_close(mc_session * session);
extern int mc_update_map(const mc_session * session, mc_vm_map * vm_map);
extern int mc_read(const mc_session * session, const uintptr_t addr, 
                   cm_byte * buf, const size_t buf_sz);
extern int mc_write(const mc_session * session, const uintptr_t addr,
                    const cm_byte * buf, const size_t buf_sz);

// --- [map]
//all return 0 = success, -1 = fail/error
extern void mc_new_vm_map(mc_vm_map * map);
extern int mc_del_vm_map(mc_vm_map * map);
extern int mc_map_clean_unmapped(mc_vm_map * map);


// --- [map util]
//return: offset from start of area/object
extern off_t mc_get_area_off(const cm_lst_node * area_node,
                             const uintptr_t addr);
extern off_t mc_get_obj_off(const cm_lst_node * obj_node,
                            const uintptr_t addr);
//return: offset from start of area/object = success, -1 = not in area/object
extern off_t mc_get_area_off_bnd(const cm_lst_node * area_node, 
                                 const uintptr_t addr);
extern off_t mc_get_obj_off_bnd(const cm_lst_node * obj_node, 
                                const uintptr_t addr);

//return: area node * = success, NULL = fail/error
extern cm_lst_node * mc_get_area_by_addr(const mc_vm_map * vm_map, 
                                         const uintptr_t addr, off_t * offset);
//return: obj node * = success, NULL = fail/error
extern cm_lst_node * mc_get_obj_by_addr(const mc_vm_map * vm_map, 
                                        const uintptr_t addr, off_t * offset);
extern cm_lst_node * mc_get_obj_by_pathname(const mc_vm_map * vm_map, 
                                            const char * pathname);
extern cm_lst_node * mc_get_obj_by_basename(const mc_vm_map * vm_map, 
                                            const char * basename);
//`str_buf` must be at least 5 bytes in size.
//return: 0 = success, -1 = fail/error
extern void mc_access_to_str(const cm_byte access, char * str_buf);


// --- [error handling]
//void return
extern void mc_perror(const char * prefix);
extern const char * mc_strerror(const int mc_errnum);



/*
 *  --- [ERROR HANDLING] ---
 */

extern __thread int mc_errno;


// [error codes]

// 1XX - user errors
#define MC_ERR_PROC_MEM         2100
#define MC_ERR_PROC_MAP         2101
#define MC_ERR_SEEK_ADDR        2102

// 2XX - internal errors
#define MC_ERR_INTERNAL_INDEX   2200
#define MC_ERR_AREA_IN_OBJ      2201
#define MC_ERR_UNEXPECTED_NULL  2202
#define MC_ERR_CMORE            2203
#define MC_ERR_READ_WRITE       2204
#define MC_ERR_MEMU_TARGET      2205
#define MC_ERR_MEMU_MAP_SZ      2206
#define MC_ERR_MEMU_MAP_GET     2207
#define MC_ERR_PROC_STATUS      2208
#define MC_ERR_PROC_NAV         2209

// 3XX - environmental errors
#define MC_ERR_MEM              2300
#define MC_ERR_PAGESIZE         2301
#define MC_ERR_KRNCRY_MAJOR     2302
#define MC_ERR_MEMU_OPEN        2303


// [error code messages]

// 1XX - user errors
#define MC_ERR_PROC_MEM_MSG \
    "Could not open /proc/<pid>/mem for specified pid.\n"
#define MC_ERR_PROC_MAP_MSG \
    "Could not open /proc/<pid>/maps for specified pid.\n"
#define MC_ERR_SEEK_ADDR_MSG \
    "Could not seek to specified address.\n"

// 2XX - internal errors
#define MC_ERR_INTERNAL_INDEX_MSG \
    "Internal: Indexing error.\n"
#define MC_ERR_AREA_IN_OBJ_MSG \
    "Internal: Area is not in object when it should be.\n"
#define MC_ERR_UNEXPECTED_NULL_MSG \
    "Internal: Unexpected NULL pointer.\n"
#define MC_ERR_CMORE_MSG \
    "Internal: CMore error. See cm_perror().\n"
#define MC_ERR_READ_WRITE_MSG \
    "Internal: Read/write failed.\n"
#define MC_ERR_MEMU_TARGET_MSG \
    "Internal: Krncry target open failed.\n"
#define MC_ERR_MEMU_MAP_SZ_MSG \
    "Internal: Krncry map size fetch failed..\n"
#define MC_ERR_MEMU_MAP_GET_MSG \
    "Internal: Krncry map transfer failed.\n"
#define MC_ERR_PROC_STATUS_MSG \
    "Internal: Failed to use /proc/<pid>/status.\n"
#define MC_ERR_PROC_NAV_MSG \
    "Internal: Failed to navigate /proc directories.\n"

// 3XX - environmental errors
#define MC_ERR_MEM_MSG \
    "Failed to acquire the necessary memory.\n"
#define MC_ERR_PAGESIZE_MSG \
    "Unable to fetch pagesize through sysconf().\n"
#define MC_ERR_KRNCRY_MAJOR_MSG \
    "Could not fetch krncry's major number.\n"
#define MC_ERR_MEMU_OPEN_MSG \
    "Could not open krncry device.\n"


#ifdef __cplusplus
}
#endif

#endif
