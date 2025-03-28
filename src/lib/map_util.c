//standard library
#include <string.h>

//external libraries
#include <cmore.h>

//local headers
#include "memcry.h"
#include "map_util.h"
#include "debug.h"


#define _MAP_UTIL_GET_AREA 0
#define _MAP_UTIL_GET_OBJ 1

#define _MAP_UTIL_USE_PATHNAME 0
#define _MAP_UTIL_USE_BASENAME 1



/*
 *  --- [INTERNAL] ---
 */

DBG_STATIC DBG_INLINE
bool _is_map_empty(const mc_vm_map * vm_map) {

    if (vm_map->vm_areas.len == 0) return true;
    if (vm_map->vm_objs.len == 0) return true;

    return false;
}


/*
 *  Determines starting object for iteration. 
 *  Will skip the pseudo-object if it is empty.
 */

DBG_STATIC DBG_INLINE
cm_lst_node * _get_starting_obj(const mc_vm_map * vm_map) {

    cm_lst_node * obj_node;
    mc_vm_obj * vm_obj;

    obj_node = vm_map->vm_objs.head;
    vm_obj = MC_GET_NODE_OBJ(obj_node);

    //if pseudo object has no areas
    if (vm_obj->start_addr == MC_UNDEF_ADDR || vm_obj->start_addr == 0x0) {
        obj_node = obj_node->next;
    }

    return obj_node;
}


DBG_STATIC DBG_INLINE
cm_lst_node * _get_obj_last_area(const mc_vm_obj * vm_obj) {

    cm_lst_node * last_node;

    //if this object has multiple areas
    if (vm_obj->vm_area_node_ps.len > 1) {
        last_node = MC_GET_NODE_PTR(vm_obj->vm_area_node_ps.head->prev);
    
    //else this object only has one area
    } else { 
        last_node = MC_GET_NODE_PTR(vm_obj->vm_area_node_ps.head);
    }

    return last_node;
}


DBG_STATIC
cm_lst_node * _fast_addr_find(const mc_vm_map * vm_map, 
                               const uintptr_t addr, const int mode) {

    cm_lst_node * iter_obj_node;
    cm_lst_node * iter_area_node;

    mc_vm_obj * iter_vm_obj;
    mc_vm_area * iter_vm_area;

    mc_vm_area * prev_area;


    //check map is not empty
    if (_is_map_empty(vm_map)) return NULL;

    //init object iteration
    iter_obj_node = _get_starting_obj(vm_map);
    iter_vm_obj = MC_GET_NODE_OBJ(iter_obj_node);

    iter_area_node = MC_GET_NODE_PTR(iter_vm_obj->vm_area_node_ps.head);
    iter_vm_area = MC_GET_NODE_AREA(iter_area_node);


    //if the address is in the very first object
    if (!(iter_vm_obj->end_addr > addr)) {

        //while can still iterate through objects
        for (int i = 1; i < vm_map->vm_objs.len; ++i) {

            //check if addr may be in this object
            if (iter_vm_obj->end_addr > addr) break;

            iter_area_node = _get_obj_last_area(iter_vm_obj);
            iter_vm_area = MC_GET_NODE_AREA(iter_area_node);

            iter_obj_node = iter_obj_node->next;
            iter_vm_obj = MC_GET_NODE_OBJ(iter_obj_node);

        } //end object search   
    
    }//end if

    //if an obj was requested
    if (mode == _MAP_UTIL_GET_OBJ) {
        
        //if the address is in range of the obj
        if (iter_vm_obj->start_addr <= addr && iter_vm_obj->end_addr > addr) {
            return iter_obj_node;
        } else {
            return NULL;
        }
    } //end if an obj was requested


    //switch to area search and continue the search from last object's end area
    while (1) {

        //if found a matching area
        if (iter_vm_area->start_addr <= addr 
            && iter_vm_area->end_addr > addr) {
            return iter_area_node;
        }

        //move to next object
        prev_area = iter_vm_area;
        iter_area_node = iter_area_node->next;
        iter_vm_area = MC_GET_NODE_AREA(iter_area_node);

        //check if back to start of dl-list, and backtrack if true
        if (prev_area->start_addr > iter_vm_area->end_addr) {
            break;
        }

    } //end area search


    //exhausted all areas and address was not found
    return NULL;
}


DBG_STATIC
cm_lst_node * _obj_name_find(const mc_vm_map * vm_map, 
                              const char * name, const int mode) {

    int ret;

    cm_lst_node * iter_obj_node;
    mc_vm_obj * iter_vm_obj;

    //check map is not empty
    if (_is_map_empty(vm_map)) return NULL;


    //init search
    iter_obj_node = vm_map->vm_objs.head;
    iter_vm_obj = MC_GET_NODE_OBJ(iter_obj_node);

    //while can still iterate through objects
    for (int i = 0; i < vm_map->vm_objs.len; ++i) {

        //carry out comparison
        if (mode == _MAP_UTIL_USE_PATHNAME) {
            ret = strncmp(name, iter_vm_obj->pathname, PATH_MAX);
        } else {
            ret = strncmp(name, iter_vm_obj->basename, NAME_MAX);
        }

        //if found a match
        if (ret == 0) {
            return iter_obj_node;
        }

        iter_obj_node = iter_obj_node->next;
        iter_vm_obj = MC_GET_NODE_OBJ(iter_obj_node);

    } //end object search

    return NULL;
}



/*
 *  --- [EXTERNAL] ---
 */

off_t mc_get_area_off(const cm_lst_node * area_node,
                      const uintptr_t addr) {

    mc_vm_area * vm_area = MC_GET_NODE_AREA(area_node);

    return (addr - vm_area->start_addr);
}


off_t mc_get_obj_off(const cm_lst_node * obj_node,
                     const uintptr_t addr) {

    mc_vm_obj * vm_obj = MC_GET_NODE_OBJ(obj_node);

    return (addr - vm_obj->start_addr);
}


off_t mc_get_area_off_bnd(const cm_lst_node * area_node, 
                          const uintptr_t addr) {

    mc_vm_area * vm_area = MC_GET_NODE_AREA(area_node);

    if ((addr >= vm_area->end_addr) || (addr < vm_area->start_addr)) return -1;
    return (addr - vm_area->start_addr);
}


off_t mc_get_obj_off_bnd(const cm_lst_node * obj_node, 
                         const uintptr_t addr) {

    mc_vm_obj * vm_obj = MC_GET_NODE_OBJ(obj_node);

    if ((addr >= vm_obj->end_addr) || (addr < vm_obj->start_addr)) return -1;
    return (addr - vm_obj->start_addr);
}


cm_lst_node * mc_get_area_by_addr(const mc_vm_map * vm_map, 
                                  const uintptr_t addr, off_t * offset) {

    cm_lst_node * area_node;

    area_node = _fast_addr_find(vm_map, addr, _MAP_UTIL_GET_AREA);
    if (!area_node) return NULL;

    if (offset != NULL) *offset = mc_get_area_off(area_node, addr);

    return area_node;
}


cm_lst_node * mc_get_obj_by_addr(const mc_vm_map * vm_map, 
                                 const uintptr_t addr, off_t * offset) {

    cm_lst_node * obj_node;

    obj_node = _fast_addr_find(vm_map, addr, _MAP_UTIL_GET_OBJ);
    if (!obj_node) return NULL;

    if (offset != NULL) *offset = mc_get_obj_off(obj_node, addr);

    return obj_node;
}


cm_lst_node * mc_get_obj_by_pathname(const mc_vm_map * vm_map, 
                                     const char * pathname) {

    cm_lst_node * obj_node;

    obj_node = _obj_name_find(vm_map, pathname, _MAP_UTIL_USE_PATHNAME);
    if (!obj_node) return NULL;

    return obj_node;
}


cm_lst_node * mc_get_obj_by_basename(const mc_vm_map * vm_map, 
                                     const char * basename) {

    cm_lst_node * obj_node;

    obj_node = _obj_name_find(vm_map, basename, _MAP_UTIL_USE_BASENAME);
    if (!obj_node) return NULL;

    return obj_node;
}
