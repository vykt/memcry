#ifndef MAP_H
#define MAP_H

//local headers
#include "memcry.h"
#include "krncry.h"


//TODO REMOVE
#define DEBUG



#define _MAP_OBJ_PREV 0
#define _MAP_OBJ_NEW  1
#define _MAP_OBJ_NEXT 2

/*
 * Initialise _traverse_state manually on the first call to 
 * send_map_node() for a map generated by a memory interface.
 *
 * send_map_node() will automatically update it for use by 
 * all subsequent calls.
 */

typedef struct {

    cm_lst_node * next_area_node; //mc_vm_area
    cm_lst_node * prev_obj_node;  //mc_vm_obj

} _traverse_state;


#ifdef DEBUG
//internal
void _map_init_vm_area(mc_vm_area * vm_area, mc_vm_map * vm_map, 
                       const cm_lst_node * obj_node, 
                       const cm_lst_node * last_obj_node,
                       const struct vm_entry * entry);
void _map_new_vm_obj(mc_vm_obj * vm_obj, 
                     mc_vm_map * vm_map, const char * pathname);
void _map_del_vm_obj(mc_vm_obj * vm_obj);

void _map_make_zero_obj(mc_vm_obj * vm_obj);

int _map_obj_add_area_insert(cm_lst * obj_area_lst, 
                             const cm_lst_node * area_node);
int _map_obj_add_area(mc_vm_obj * obj, const cm_lst_node * area_node);
int _map_obj_add_last_area(mc_vm_obj * obj, const cm_lst_node * last_area_node);

bool _map_is_pathname_in_obj(const char * pathname, const mc_vm_obj * obj);
int _map_find_obj_for_area(const _traverse_state * state, 
                           const struct vm_entry * entry);

int _map_update_obj_addr_range(mc_vm_obj * obj);
int _map_backtrack_unmapped_obj_last_vm_areas(cm_lst_node * obj_node);
int _map_forward_unmapped_obj_last_vm_areas(cm_lst_node * obj_node);

int _map_unlink_unmapped_obj(cm_lst_node * node, 
                             const _traverse_state * state, 
                             mc_vm_map * vm_map);
int _map_unlink_unmapped_area(cm_lst_node * node, 
                              const _traverse_state * state, 
                              mc_vm_map * vm_map);

int _map_check_area_eql(const struct vm_entry * entry, 
                        const cm_lst_node * area_node);
int _map_resync_area(mc_vm_map * vm_map,
                     _traverse_state * state, const struct vm_entry * entry);

void _map_state_inc_area(mc_vm_map * vm_map, _traverse_state * state, 
                         const cm_lst_node * assign_node, const int inc_type);
void _map_state_inc_obj(mc_vm_map * vm_map, _traverse_state * state);

cm_lst_node * _map_add_obj(mc_vm_map * vm_map, _traverse_state * state, 
                           const struct vm_entry * entry);
int _map_add_area(mc_vm_map * vm_map, _traverse_state * state, 
                  const struct vm_entry * entry, const int inc_type);
#endif


//interface
int map_send_entry(mc_vm_map * vm_map, 
                   _traverse_state * state, const struct vm_entry * entry);
void map_init_traverse_state(const mc_vm_map * vm_map, 
                              _traverse_state * state);


//external
void mc_new_vm_map(mc_vm_map * vm_map);
int mc_del_vm_map(mc_vm_map * vm_map);
int mc_map_clean_unmapped(mc_vm_map * vm_map);


#endif
