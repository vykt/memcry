//standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//system headers
#include <unistd.h>
#include <time.h>

#include <linux/limits.h>

//external libraries
#include <cmore.h>
#include <check.h>

//local headers
#include "suites.h"
#include "map_helper.h"

//test target headers
#include "../lib/memcry.h"
#include "../lib/map.h"



/*
 *  [ADVANCED TEST]
 */

/*
 *  NOTE: The virtual memory map management code is complicated and as such,
 *        almost every internal function has independent tests. For these
 *        tests to run, the debug target must be built.
 *
 *        The following functions do not have unit tests:
 *
 *            _map_obj_add_area_insert():
 *
 *                > Tested through `_map_obj_add_area()` 
 *                  and `_map_obj_add_last_area()`
 *
 *            _map_obj_find_area_outer_node():
 *
 *                > Tested through `_map_obj_rmv_area()` 
 *                  and `_map_obj_rmv_last_area()`
 *
 *            _map_obj_rmv_area_fast():
 *
 *                > Tested through `map_obj_rmv_area()`
 *                  which calls this function.
 *
 *            _map_obj_rmv_last_area_fast():
 *
 *                > Tested through `map_obj_rmv_last_area()`
 *                  which calls this function.
 */

/*
 *  NOTE: Functions are not tested in the same order as they appear in
 *        the map.c source file. This assists with bootstrapping many tests.
 *        The order of testing remains close.
 */
 

//globals - map

/*
 *  Contains all structures inside a map, artificially allocated statically.
 */

#define STUB_MAP_AREA_NUM 10
#define STUB_MAP_OBJ_NUM 4
static mc_vm_map m;

static mc_vm_area m_a[STUB_MAP_AREA_NUM];
static cm_lst_node m_a_n[STUB_MAP_AREA_NUM];

static mc_vm_obj m_o[STUB_MAP_OBJ_NUM];
static cm_lst_node m_o_n[STUB_MAP_OBJ_NUM];


//globals - standalone object
#define AREAS_NUM 4
#define LAST_AREAS_NUM 2
static mc_vm_obj o;
static cm_lst_node o_n;

static mc_vm_area o_a[AREAS_NUM];
static cm_lst_node o_a_n[AREAS_NUM];

static mc_vm_area o_a_l[LAST_AREAS_NUM];
static cm_lst_node o_a_l_n[LAST_AREAS_NUM];



/*
 *  --- [HELPERS] ---
 */

//initialise a _traverse_state
static void _init__traverse_state(_traverse_state * state, 
                                  cm_lst_node * next_area_node,
                                  cm_lst_node * prev_obj_node) {

    state->next_area_node = next_area_node;
    state->prev_obj_node = prev_obj_node;

    return;
}


//initialise a vm_entry stub
static void _init_vm_entry(struct vm_entry * entry, unsigned long vm_start, 
                           unsigned long vm_end, unsigned long file_off, 
                           krncry_pgprot_t prot, char * file_path) {

    //set entry vars
    entry->vm_start = vm_start;
    entry->vm_end   = vm_end;
    entry->file_off = file_off;
    entry->prot     = prot;

    //set file path
    if (file_path != NULL) {
        strncpy(entry->file_path, file_path, PATH_MAX);
    
    } else {
        entry->file_path[0] = '\0';
    }
    
    return;
}


//connect nodes
static void _connect_nodes(cm_lst_node * node_1, cm_lst_node * node_2) {

    if (node_1 != NULL) {
        node_1->next = node_2;
    }

    if (node_2 != NULL) {
        node_2->prev = node_1;
    }

    return;
}


//check if a pointer points to a static allocation
static bool _map_check_static(void * ptr, void * arr, int len, size_t ent_sz) {

    for (int i = 0; i < len; ++i) {
        if ((cm_byte *) ptr == (((cm_byte *) arr) + (i * ent_sz))) return true;
    }

    return false;
}


//remove static pointers from a list
static void _map_remove_static(cm_lst * lst, void * arr,
                               int len, size_t ent_sz, bool unmapped) {

    cm_lst_node * temp_node, * node, * cmp_node;
    node = lst->head;
    

    for (int i = 0; i < lst->len; ++i) {

        //determine if comparing node or inner node
        cmp_node = unmapped ? MC_GET_NODE_PTR(node) : node;

        //static node
        if (_map_check_static(cmp_node, arr, len, ent_sz)) {

            //adjust iteration (part 1)
            temp_node = node->next;

            //handle node
            if (unmapped)  cm_lst_rmv_n(lst, node);
            if (!unmapped) cm_lst_uln_n(lst, node);

            //adjust iteration (part 2)
            node = temp_node;
            i -= 1;

        //dynamic node
        } else {            
            node = node->next;

        } //end if 

    } //end for

    return;
}


/*
 *  --- [FIXTURES] ---
 */

//empty map fixture
static void _setup_empty_vm_map() {

    //construct the map
    mc_new_vm_map(&m);

    return;
}


#ifdef DEBUG

/*
 *  NOTE: I recognise setting this up and tearing it down is an enormous pain,
 *        and a time sink, however without it the majority of internal
 *        functions can't be tested.
 */

//stub map fixture
static void _setup_stub_vm_map() {

    /*
     *  Stub map:
     *
     *  0) 0x1000 - 0x2000 /bin/cat  r--
     *  1) 0x2000 - 0x3000 /bin/cat  rw-
     *  2) 0x3000 - 0x4000 /bin/cat  r-x
     *  3) 0x4000 - 0x5000 [heap]    rw- <- gap
     *  4) 0x6000 - 0x7000           rw- <- gap
     *  5) 0x8000 - 0x9000 /lib/foo  r--
     *  6) 0x9000 - 0xA000 /lib/foo  rw-
     *  7) 0xA000 - 0xB000 /lib/foo  r-x <- gap
     *  8) 0xC000 - 0xD000           rw- <- gap
     *  9) 0xE000 - 0xF000 [stack]   rw-
     */

    int ret;
    struct vm_entry entry;


    //construct the map
    mc_new_vm_map(&m);

    //construct objects
    _map_new_vm_obj(&m_o[0], &m, "/bin/cat");
    _map_new_vm_obj(&m_o[1], &m, "[heap]");
    _map_new_vm_obj(&m_o[2], &m, "/lib/foo");
    _map_new_vm_obj(&m_o[3], &m, "[stack]");

    //construct object nodes
    for (int i = 0; i < STUB_MAP_OBJ_NUM; ++i) {

        create_lst_wrapper(&m_o_n[i], &m_o[i]);
    }


    //construct areas
    _init_vm_entry(&entry, 0x1000, 0x2000, 0x100, 
                   MC_ACCESS_READ, "/bin/cat");
    _map_init_vm_area(&m_a[0], &entry, &m_o_n[0], NULL, &m);

    _init_vm_entry(&entry, 0x2000, 0x3000, 0x200, 
                   MC_ACCESS_READ | MC_ACCESS_WRITE, "/bin/cat");
    _map_init_vm_area(&m_a[1], &entry, &m_o_n[0], NULL, &m);

    _init_vm_entry(&entry, 0x3000, 0x4000, 0x300, 
                   MC_ACCESS_READ | MC_ACCESS_EXEC, "/bin/cat");
    _map_init_vm_area(&m_a[2], &entry, &m_o_n[0], NULL, &m);

    _init_vm_entry(&entry, 0x4000, 0x5000, 0x0, 
                   MC_ACCESS_READ | MC_ACCESS_WRITE, "[heap]");
    _map_init_vm_area(&m_a[3], &entry, &m_o_n[1], NULL, &m);

    _init_vm_entry(&entry, 0x6000, 0x7000, 0x0, 
                   MC_ACCESS_READ | MC_ACCESS_WRITE, NULL);
    _map_init_vm_area(&m_a[4], &entry, NULL, &m_o_n[1], &m);

    _init_vm_entry(&entry, 0x8000, 0x9000, 0x100, 
                   MC_ACCESS_READ, "/lib/foo");
    _map_init_vm_area(&m_a[5], &entry, &m_o_n[2], NULL, &m);

    _init_vm_entry(&entry, 0x9000, 0xA000, 0x200, 
                   MC_ACCESS_READ | MC_ACCESS_WRITE, "/lib/foo");
    _map_init_vm_area(&m_a[6], &entry, &m_o_n[2], NULL, &m);

    _init_vm_entry(&entry, 0xA000, 0xB000, 0x300, 
                   MC_ACCESS_READ | MC_ACCESS_EXEC, "/lib/foo");
    _map_init_vm_area(&m_a[7], &entry, &m_o_n[2], NULL, &m);

    _init_vm_entry(&entry, 0xC000, 0xD000, 0x0, 
                   MC_ACCESS_READ | MC_ACCESS_WRITE, NULL);
    _map_init_vm_area(&m_a[8], &entry, NULL, &m_o_n[2], &m);

    _init_vm_entry(&entry, 0xE000, 0xF000, 0x0, 
                   MC_ACCESS_READ | MC_ACCESS_WRITE, "[stack]");
    _map_init_vm_area(&m_a[9], &entry, &m_o_n[3], NULL, &m);
    
    //construct area nodes
    for (int i = 0; i < STUB_MAP_AREA_NUM; ++i) {

        create_lst_wrapper(&m_a_n[i], &m_a[i]);
    }


    //connect areas to objects
    ret = _map_obj_add_area(&m_o[0], &m_a_n[0]);
    ck_assert_int_eq(ret, 0);

    ret = _map_obj_add_area(&m_o[0], &m_a_n[1]);
    ck_assert_int_eq(ret, 0);
    
    ret = _map_obj_add_area(&m_o[0], &m_a_n[2]);
    ck_assert_int_eq(ret, 0);

    ret = _map_obj_add_area(&m_o[1], &m_a_n[3]);
    ck_assert_int_eq(ret, 0);

    ret = _map_obj_add_last_area(&m_o[1], &m_a_n[4]);
    ck_assert_int_eq(ret, 0);

    ret = _map_obj_add_area(&m_o[2], &m_a_n[5]);
    ck_assert_int_eq(ret, 0);

    ret = _map_obj_add_area(&m_o[2], &m_a_n[6]);
    ck_assert_int_eq(ret, 0);

    ret = _map_obj_add_area(&m_o[2], &m_a_n[7]);
    ck_assert_int_eq(ret, 0);

    ret = _map_obj_add_last_area(&m_o[2], &m_a_n[8]);
    ck_assert_int_eq(ret, 0);

    ret = _map_obj_add_area(&m_o[3], &m_a_n[9]);
    ck_assert_int_eq(ret, 0);


    //connect area nodes
    for (int i = 0; i < STUB_MAP_AREA_NUM - 1; ++i) {
        _connect_nodes(&m_a_n[i], &m_a_n[i+1]);
    }
    _connect_nodes(&m_a_n[STUB_MAP_AREA_NUM-1], &m_a_n[0]);

    //connect object nodes
    for (int i = 0; i < STUB_MAP_OBJ_NUM - 1; ++i) {
        _connect_nodes(&m_o_n[i], &m_o_n[i+1]);
    }
    _connect_nodes(m.vm_objs.head, &m_o_n[0]);
    _connect_nodes(&m_o_n[STUB_MAP_OBJ_NUM-1], m.vm_objs.head);


    //connect area nodes and object nodes to the map
    m.vm_areas.head = &m_a_n[0];

    m.vm_areas.len += STUB_MAP_AREA_NUM;
    m.vm_objs.len  += STUB_MAP_OBJ_NUM;

    return;
}
#endif


/*
 *  NOTE: The map starts out containing only statically allocated areas,
 *        objects, and nodes. After tests are carried out, it can contain
 *        a mix of statically and dynamically allocated data. The map 
 *        destructor expects exclusively dynamically allocated data. As such,
 *        it is important to remove all statically allocated data from the map
 *        before calling the destructor.
 */

static void _teardown_vm_map() {

    cm_lst_node * node;
    mc_vm_obj * obj;
    mc_vm_area * area;


    //setup iteration
    node = m.vm_objs.head;

    //empty all object lists
    for (int i = 0; i < m.vm_objs.len; ++i) {

        //empty lists of this object (always dynamic)
        obj = MC_GET_NODE_OBJ(node);
        cm_lst_emp(&obj->vm_area_node_ps);
        cm_lst_emp(&obj->last_vm_area_node_ps);
        node = node->next;
    }

    //deallocate names of objects
    for (int i = 0; i < STUB_MAP_OBJ_NUM; ++i) {
        free(m_o[i].pathname);
    }

    //remove static objects & areas
    _map_remove_static(&m.vm_objs, m_o_n,
                       STUB_MAP_OBJ_NUM, sizeof(cm_lst_node), false);
    _map_remove_static(&m.vm_areas, m_a_n,
                       STUB_MAP_AREA_NUM, sizeof(cm_lst_node), false);
    _map_remove_static(&m.vm_objs_unmapped, m_o_n,
                       STUB_MAP_OBJ_NUM, sizeof(cm_lst_node), true);
    _map_remove_static(&m.vm_areas_unmapped, m_a_n,
                       STUB_MAP_AREA_NUM, sizeof(cm_lst_node), true);


    //call regular destructor
    mc_del_vm_map(&m);

    return;
}


#ifdef DEBUG
//empty object fixture
static void _setup_empty_vm_obj() {

    //super of _setup_empty_vm_map
    _setup_empty_vm_map();

    //construct the new object
    _map_new_vm_obj(&o, &m, "/foo/bar");

    //populate the object node
    create_lst_wrapper(&o_n, &o);

    return;
}
#endif


#ifdef DEBUG
//stub object fixture
static void _setup_stub_vm_obj() {

    /*
     *  Object will have an address range of 0x1000 - 0x5000.
     */

    struct vm_entry entry;
    uintptr_t addr     = 0x1000;
    uintptr_t last_addr = 0x5000;
    uintptr_t file_off = 0x200;

    //super of _setup_empty_vm_obj
    _setup_empty_vm_obj();
    

    //initialise areas
    for (int i = 0; i < AREAS_NUM; ++i) {

        //setup area
        _init_vm_entry(&entry, addr, addr + 0x1000, 
                       file_off, MC_ACCESS_READ, "/foo/bar");
        _map_init_vm_area(&o_a[i], &entry, &o_n, NULL, &m);
        create_lst_wrapper(&o_a_n[i], &o_a[i]);
        _map_obj_add_area(&o, &o_a_n[i]);

        //advance iteration
        addr += 0x1000;
        file_off += 0x200;
    }


    //initialise last areas
    for (int i = 0; i < LAST_AREAS_NUM; ++i) {

        //setup last area
        _init_vm_entry(&entry, last_addr, last_addr + 0x1000,
                       0x0, MC_ACCESS_READ, NULL);
        _map_init_vm_area(&o_a_l[i], &entry, NULL, &o_n, &m);
        create_lst_wrapper(&o_a_l_n[i], &o_a_l[i]);
        _map_obj_add_last_area(&o, &o_a_l_n[i]);
    
        //advance iteration
        last_addr += 0x1000;
    }

    return;
}
#endif


#ifdef DEBUG
static void _teardown_vm_obj() {

    //destroy the object
    _map_del_vm_obj(&o);

    //super of _teardown_vm_map()
    _teardown_vm_map();

    return;
}
#endif



/*
 *  --- [UNIT TESTS] ---
 */

//mc_new_vm_map() & mc_del_vm_map() [no fixture]
START_TEST(test_mc_new_del_vm_map) {

    /*
     *  Using ASAN to check for memory leaks in the destructor.
     */

    mc_vm_obj * zero_obj;


    //only test: construct the map
    mc_new_vm_map(&m);

    assert_vm_map(&m, 0, 1, 0, 0, 0, 0);

    //check the pseudo object is present
    zero_obj = MC_GET_NODE_OBJ(m.vm_objs.head);
    assert_vm_obj(zero_obj, "0x0", "0x0",
                  0x0, 0x0, 0, 0, MC_ZERO_OBJ_ID, true);

    mc_del_vm_map(&m);

    return;
    
} END_TEST


#ifdef DEBUG
//_map_new_vm_obj() & _map_del_vm_obj() [empty map fixture]
START_TEST(test__map_new_del_vm_obj) {

    mc_vm_obj obj;

    //only test: construct the object
    _map_new_vm_obj(&obj, &m, "/foo/bar");

    assert_vm_obj(&obj, "/foo/bar", "bar",
                  MC_UNDEF_ADDR, MC_UNDEF_ADDR, 0, 0, 0, true);
    assert_vm_map(&m, 0, 1, 0, 0, 0, 1);

    _map_del_vm_obj(&obj);

    return;
}


//_map_make_zero_obj() [empty map fixture]
START_TEST(test__map_make_zero_obj) {

    mc_vm_obj zero_obj;


    //create new object
    _map_new_vm_obj(&zero_obj, &m, "0x0");

    //only test: convert new object to pseudo object 
    _map_make_zero_obj(&zero_obj);
    
    assert_vm_obj(&zero_obj, "0x0", "0x0",
                  0x0, 0x0, 0, 0, MC_ZERO_OBJ_ID, true);
    assert_vm_map(&m, 0, 1, 0, 0, 0, 1);

    //destroy pseudo object
    _map_del_vm_obj(&zero_obj);

    return;
}


//_map_init_vm_area() [empty object fixture]
START_TEST(test__map_init_vm_area) {

    mc_vm_area area;
    struct vm_entry entry;


    //first test: create a stub entry & initialise the new area
    _init_vm_entry(&entry, 0x1000, 0x2000, 0x800, 
                   MC_ACCESS_READ, "/foo/bar");
    _map_init_vm_area(&area, &entry, &o_n, NULL, &m);

    assert_vm_area(&area, "/foo/bar", "bar", 0x1000, 0x2000, 
                    MC_ACCESS_READ, &o_n, NULL, 0, true);
    assert_vm_map(&m, 0, 1, 0, 0, 1, 1);


    //second test: create a stub entry & initialise another new area
    _init_vm_entry(&entry, 0x2000, 0x4000, 0x800, 
                   MC_ACCESS_READ | MC_ACCESS_WRITE, "/purr/meow");
    _map_init_vm_area(&area, &entry, NULL, &o_n, &m);

    assert_vm_area(&area, NULL, NULL, 0x2000, 0x4000, 
                    MC_ACCESS_READ | MC_ACCESS_WRITE, NULL, &o_n, 1, true);
    assert_vm_map(&m, 0, 1, 0, 0, 2, 1);

    return;
 
} END_TEST


//_map_obj_add_area() [empty object fixture]
START_TEST(test__map_obj_add_area) {

    int ret;

    mc_vm_area area[4];
    cm_lst_node area_node[4], * area_node_ptr;    
    struct vm_entry entry;

    uintptr_t state_first[1] = {0x2000};
    uintptr_t state_lower[2] = {0x1000, 0x2000};
    uintptr_t state_higher[3] = {0x1000, 0x2000, 0x4000};
    uintptr_t state_middle[4] = {0x1000, 0x2000, 0x3000, 0x4000};


    //initialise first area
    _init_vm_entry(&entry, 0x2000, 0x3000, 0x800, MC_ACCESS_READ, "/foo/bar");
    _map_init_vm_area(&area[0], &entry, &o_n, NULL, &m);
    create_lst_wrapper(&area_node[0], &area[0]);


    //first test: add first area to the backing object
    _map_obj_add_area(&o, &area_node[0]);

    assert_vm_obj(&o, "/foo/bar", "bar", 0x2000, 0x3000, 1, 0, 0, true);
    assert_vm_obj_list(&o.vm_area_node_ps, state_first, 1);
    area_node_ptr = MC_GET_NODE_PTR(o.vm_area_node_ps.head);
    assert_vm_area(MC_GET_NODE_AREA(area_node_ptr), "/foo/bar", "bar", 
                    0x2000, 0x3000, MC_ACCESS_READ, &o_n, NULL, 0, true);


    //initialise lower area
    _init_vm_entry(&entry, 0x1000, 0x2000, 0x600, MC_ACCESS_WRITE, "/foo/bar");
    _map_init_vm_area(&area[1], &entry, &o_n, NULL, &m);
    create_lst_wrapper(&area_node[1], &area[1]);

    //second test: add lower area to the backing object
    _map_obj_add_area(&o, &area_node[1]);

    assert_vm_obj(&o, "/foo/bar", "bar", 0x1000, 0x3000, 2, 0, 0, true);
    assert_vm_obj_list(&o.vm_area_node_ps, state_lower, 2);
    area_node_ptr = MC_GET_NODE_PTR(o.vm_area_node_ps.head);
    assert_vm_area(MC_GET_NODE_AREA(area_node_ptr), "/foo/bar", "bar", 
                   0x1000, 0x2000, MC_ACCESS_WRITE, &o_n, NULL, 1, true);
    
    
    //initialise higher area
    _init_vm_entry(&entry, 0x4000, 0x5000, 0x900, MC_ACCESS_EXEC, "/foo/bar");
    _map_init_vm_area(&area[2], &entry, &o_n, NULL, &m);
    create_lst_wrapper(&area_node[2], &area[2]);

    //third test: add lower area to the backing object
    _map_obj_add_area(&o, &area_node[2]);

    assert_vm_obj(&o, "/foo/bar", "bar", 0x1000, 0x5000, 3, 0, 0, true);
    assert_vm_obj_list(&o.vm_area_node_ps, state_higher, 3);
    area_node_ptr = MC_GET_NODE_PTR(o.vm_area_node_ps.head->prev);
    assert_vm_area(MC_GET_NODE_AREA(area_node_ptr), "/foo/bar", "bar", 
                   0x4000, 0x5000, MC_ACCESS_EXEC, &o_n, NULL, 2, true);


    //initialise middle area
    _init_vm_entry(&entry, 0x3000, 0x4000, 0x880, MC_ACCESS_READ, "/foo/bar");
    _map_init_vm_area(&area[3], &entry, &o_n, NULL, &m);
    create_lst_wrapper(&area_node[3], &area[3]);

    //fourth test: add middle area to the backing object
    _map_obj_add_area(&o, &area_node[3]);

    assert_vm_obj(&o, "/foo/bar", "bar", 0x1000, 0x5000, 4, 0, 0, true);
    assert_vm_obj_list(&o.vm_area_node_ps, state_middle, 4);
    area_node_ptr = MC_GET_NODE_PTR(o.vm_area_node_ps.head->next->next);
    assert_vm_area(MC_GET_NODE_AREA(area_node_ptr), "/foo/bar", "bar", 
                   0x3000, 0x4000, MC_ACCESS_READ, &o_n, NULL, 3, true);

    return;

} END_TEST


//_map_obj_add_last_area() [empty object fixture]
START_TEST(test__map_obj_add_last_area) {

    int ret;
    
    mc_vm_area last_area[4];
    cm_lst_node last_area_node[4], * last_area_node_ptr;
    struct vm_entry entry;

    uintptr_t state_first[1] = {0x2000};
    uintptr_t state_lower[2] = {0x1000, 0x2000};
    uintptr_t state_higher[3] = {0x1000, 0x2000, 0x4000};
    uintptr_t state_middle[4] = {0x1000, 0x2000, 0x3000, 0x4000};


    //initialise first area
    _init_vm_entry(&entry, 0x2000, 0x3000, 0x800, MC_ACCESS_READ, NULL);
    _map_init_vm_area(&last_area[0], &entry, NULL, &o_n, &m);
    create_lst_wrapper(&last_area_node[0], &last_area[0]);

    //first test: add first area to the backing object
    _map_obj_add_area_insert(&o.last_vm_area_node_ps, &last_area_node[0]);

    assert_vm_obj(&o, "/foo/bar", "bar",
                  MC_UNDEF_ADDR, MC_UNDEF_ADDR, 0, 1, 0, true);
    assert_vm_obj_list(&o.last_vm_area_node_ps, state_first, 1);
    last_area_node_ptr = MC_GET_NODE_PTR(o.last_vm_area_node_ps.head);
    assert_vm_area(MC_GET_NODE_AREA(last_area_node_ptr), NULL, NULL, 
                   0x2000, 0x3000, MC_ACCESS_READ, NULL, &o_n, 0, true);


    //initialise lower area
    _init_vm_entry(&entry, 0x1000, 0x2000, 0x600, MC_ACCESS_WRITE, NULL);
    _map_init_vm_area(&last_area[1], &entry, NULL, &o_n, &m);
    create_lst_wrapper(&last_area_node[1], &last_area[1]);

    //second test: add lower area to the backing object
    _map_obj_add_area_insert(&o.last_vm_area_node_ps, &last_area_node[1]);

    assert_vm_obj(&o, "/foo/bar", "bar",
                  MC_UNDEF_ADDR, MC_UNDEF_ADDR, 0, 2, 0, true);
    assert_vm_obj_list(&o.last_vm_area_node_ps, state_lower, 2);
    last_area_node_ptr = MC_GET_NODE_PTR(o.last_vm_area_node_ps.head);
    assert_vm_area(MC_GET_NODE_AREA(last_area_node_ptr), NULL, NULL,
                   0x1000, 0x2000, MC_ACCESS_WRITE, NULL, &o_n, 1, true);
    
    
    //initialise higher area
    _init_vm_entry(&entry, 0x4000, 0x5000, 0x900, MC_ACCESS_EXEC, NULL);
    _map_init_vm_area(&last_area[2], &entry, NULL, &o_n, &m);
    create_lst_wrapper(&last_area_node[2], &last_area[2]);

    //third test: add lower area to the backing object
    _map_obj_add_area_insert(&o.last_vm_area_node_ps, &last_area_node[2]);

    assert_vm_obj(&o, "/foo/bar", "bar",
                  MC_UNDEF_ADDR, MC_UNDEF_ADDR, 0, 3, 0, true);
    assert_vm_obj_list(&o.last_vm_area_node_ps, state_higher, 3);
    last_area_node_ptr = MC_GET_NODE_PTR(o.last_vm_area_node_ps.head->prev);
    assert_vm_area(MC_GET_NODE_AREA(last_area_node_ptr), NULL, NULL, 
                   0x4000, 0x5000, MC_ACCESS_EXEC, NULL, &o_n, 2, true);


    //initialise middle area
    _init_vm_entry(&entry, 0x3000, 0x4000, 0x880, MC_ACCESS_READ, NULL);
    _map_init_vm_area(&last_area[3], &entry, NULL, &o_n, &m);
    create_lst_wrapper(&last_area_node[3], &last_area[3]);

    //fourth test: add middle area to the backing object
    _map_obj_add_area_insert(&o.last_vm_area_node_ps, &last_area_node[3]);

    assert_vm_obj(&o, "/foo/bar", "bar",
                  MC_UNDEF_ADDR, MC_UNDEF_ADDR, 0, 4, 0, true);
    assert_vm_obj_list(&o.last_vm_area_node_ps, state_middle, 4);
    last_area_node_ptr
        = MC_GET_NODE_PTR(o.last_vm_area_node_ps.head->next->next);
    assert_vm_area(MC_GET_NODE_AREA(last_area_node_ptr), NULL, NULL, 
                    0x3000, 0x4000, MC_ACCESS_READ, NULL, &o_n, 3, true);
    
    return;

} END_TEST


//_map_obj_rmv_area() [stub object fixture]
START_TEST(test__map_obj_rmv_area) {
    
    int ret;
    
    uintptr_t state_middle[3] = {0x1000, 0x3000, 0x4000};
    uintptr_t state_first[2] = {0x3000, 0x4000};
    uintptr_t state_last[1] = {0x3000};


    //first test: remove middle area
    ret = _map_obj_rmv_area(&o, &o_a_n[1]);
    ck_assert_int_eq(ret, 0);

    assert_vm_obj(&o, "/foo/bar", "bar", 0x1000, 0x5000, 3, 2, 0, true);
    assert_vm_obj_list(&o.vm_area_node_ps, state_middle, 3);
    

    //second test: remove first area
    ret = _map_obj_rmv_area(&o, &o_a_n[0]);
    ck_assert_int_eq(ret, 0);

    assert_vm_obj(&o, "/foo/bar", "bar", 0x3000, 0x5000, 2, 2, 0, true);
    assert_vm_obj_list(&o.vm_area_node_ps, state_first, 2);


    //third test: remove last area
    ret = _map_obj_rmv_area(&o, &o_a_n[3]);
    ck_assert_int_eq(ret, 0);

    assert_vm_obj(&o, "/foo/bar", "bar", 0x3000, 0x4000, 1, 2, 0, true);
    assert_vm_obj_list(&o.vm_area_node_ps, state_last, 1);


    //fourth test: remove only remaining area
    ret = _map_obj_rmv_area(&o, &o_a_n[2]);
    ck_assert_int_eq(ret, 0);

    assert_vm_obj(&o, "/foo/bar", "bar",
                  MC_UNDEF_ADDR, MC_UNDEF_ADDR, 0, 2, 0, true);
    assert_vm_obj_list(&o.vm_area_node_ps, NULL, 0);

    return;
    
} END_TEST


//_map_obj_rmv_last_area [stub object fixture]
START_TEST(test__map_obj_rmv_last_area) {

    int ret;

    uintptr_t state_first[1] = {0x6000};


    //first test: remove first last area
    ret = _map_obj_rmv_last_area(&o, &o_a_l_n[0]);
    ck_assert_int_eq(ret, 0);

    assert_vm_obj(&o, "/foo/bar", "bar", 0x1000, 0x5000, 4, 1, 0, true);
    assert_vm_obj_list(&o.last_vm_area_node_ps, state_first, 1);


    //second test: remove only remaining last area
    ret = _map_obj_rmv_last_area(&o, &o_a_l_n[1]);
    ck_assert_int_eq(ret, 0);

    assert_vm_obj(&o, "/foo/bar", "bar", 0x1000, 0x5000, 4, 0, 0, true);
    assert_vm_obj_list(&o.last_vm_area_node_ps, NULL, 0);
    
    return;
}


//_map_is_pathname_in_obj() [empty object fixture]
START_TEST(test__map_is_pathname_in_obj) {

    bool ret;


    //first test: path is in the object
    ret = _map_is_pathname_in_obj("/foo/bar", &o);
    ck_assert(ret);    

    //second test: path is not in the object
    ret = _map_is_pathname_in_obj("anonmap", &o);
    ck_assert(!ret);

    return;

} END_TEST


//_map_find_obj_for_area [empty map fixture]
START_TEST(test__map_find_obj_for_area) {

    int ret;

    mc_vm_obj objs[3];
    cm_lst_node obj_nodes[3];
    char * obj_paths[3] = {"/lib/libc", "/lib/libpthread", "anonmap"};

    struct vm_entry entry;
    _traverse_state state;    


    //construct test objects
    for (int i = 0; i < 3; ++i) {
        _map_new_vm_obj(&objs[i], &m, obj_paths[i]);
        create_lst_wrapper(&obj_nodes[i], &objs[i]);
    }

    //connect test objects
    _connect_nodes(&obj_nodes[0], &obj_nodes[1]);
    _connect_nodes(&obj_nodes[1], &obj_nodes[2]);
    

    //first test: new object, state empty
    _init__traverse_state(&state, NULL, NULL);
    _init_vm_entry(&entry, 0x1000, 0x2000, 0x500, 
                   MC_ACCESS_READ, "/lib/libc");

    ret = _map_find_obj_for_area(&entry, &state);
    ck_assert_int_eq(ret, _MAP_OBJ_NEW);

    
    //second test: new object, state full
    _init__traverse_state(&state, NULL, &obj_nodes[0]);
    _init_vm_entry(&entry, 0x1000, 0x2000, 0x500, 
                   MC_ACCESS_READ, "anonmap");

    ret = _map_find_obj_for_area(&entry, &state);
    ck_assert_int_eq(ret, _MAP_OBJ_NEW);


    //third test: previous object 
    _init__traverse_state(&state, NULL, &obj_nodes[0]);
    _init_vm_entry(&entry, 0x1000, 0x2000, 0x500, 
                   MC_ACCESS_READ, "/lib/libc");

    ret = _map_find_obj_for_area(&entry, &state);
    ck_assert_int_eq(ret, _MAP_OBJ_PREV);


    //fourth test: next object 
    _init__traverse_state(&state, NULL, &obj_nodes[0]);
    _init_vm_entry(&entry, 0x1000, 0x2000, 0x500, 
                   MC_ACCESS_READ, "/lib/libpthread");

    ret = _map_find_obj_for_area(&entry, &state);
    ck_assert_int_eq(ret, _MAP_OBJ_NEXT);


    //destruct test objects
    for (int i = 0; i < 3; ++i) {
        _map_del_vm_obj(&objs[i]);
    }

    return;
    
} END_TEST


//_map_backtrack_unmapped_obj_last_vm_areas() [stub map fixture]
START_TEST(test__map_backtrack_unmapped_obj_last_vm_areas) {

    /*
     *  NOTE: For this test, `m_o{_n}` arrays are used to refer to objects
     *        in the stub map. Note `m_o{_n}[0]` is not the first object in
     *        in the map; the map contains a pseudo object that is not in
     *        the `m_o{_n}` arrays.
     */

    int ret;

    mc_vm_obj * zero_obj;
    cm_lst_node * zero_node;

    uintptr_t first_state[2]  = {0x6000, 0xC000};
    uintptr_t second_state[2] = {0x6000, 0xC000};
    uintptr_t third_state[2]  = {0x6000, 0xC000};
    

    //first test: backtrack `/lib/foo`'s last area (index: 8)
    ret = _map_backtrack_unmapped_obj_last_vm_areas(&m_o_n[2]);
    ck_assert_int_eq(ret, 0);
    
    //check `/lib/foo` no longer has any last areas associated with it
    assert_vm_obj(&m_o[2], "/lib/foo", "foo", 0x8000, 0xB000, 3, 0, 2, true);

    //check `[heap]` now has two last areas associated with it
    assert_vm_obj(&m_o[1], "[heap]", "[heap]", 0x4000, 0x5000, 1, 2, 1, true);
    assert_vm_obj_list(&m_o[1].last_vm_area_node_ps, first_state, 2);

    //check the transfered last area (index: 8) now points to `[heap]`
    assert_vm_area(&m_a[8], NULL, NULL, 0xC000, 0xD000,
                    MC_ACCESS_READ | MC_ACCESS_WRITE, 
                    NULL, &m_o_n[1], 8, true);


    //second test: backtrack `[heap]`'s two last areas (indeces: 4, 8)
    ret = _map_backtrack_unmapped_obj_last_vm_areas(&m_o_n[1]);
    ck_assert_int_eq(ret, 0);

    //check `[heap]` no longer has any last areas associated with it
    assert_vm_obj(&m_o[1], "[heap]", "[heap]", 0x4000, 0x5000, 1, 0, 1, true);

    //check `/bin/cat` now has two last areas associated with it
    assert_vm_obj(&m_o[0], "/bin/cat", "cat", 0x1000, 0x4000, 3, 2, 0, true);
    assert_vm_obj_list(&m_o[0].last_vm_area_node_ps, first_state, 2);

    //check the transfered last areas (indeces: 4, 8) now point to `/bin/cat`
    assert_vm_area(&m_a[4], NULL, NULL, 0x6000, 0x7000,
                    MC_ACCESS_READ | MC_ACCESS_WRITE, NULL, &m_o_n[0], 4, true);
    
    assert_vm_area(&m_a[8], NULL, NULL, 0xC000, 0xD000,
                    MC_ACCESS_READ | MC_ACCESS_WRITE, NULL, &m_o_n[0], 8, true);

    
    //third test: backtrack `/bin/cat`'s two lat areas (indeces: 4, 8)
    ret = _map_backtrack_unmapped_obj_last_vm_areas(&m_o_n[0]);
    ck_assert_int_eq(ret, 0);

    //get the pseudo object
    zero_node = cm_lst_get_n(&m.vm_objs, 0);
    zero_obj = MC_GET_NODE_OBJ(zero_node);
    
    //check `/bin/cat` no longer has any last areas associated with it
    assert_vm_obj(&m_o[0], "/bin/cat", "cat", 0x1000, 0x4000, 3, 0, 0, true);

    //check the pseudo object now has two last areas associated with it
    assert_vm_obj(zero_obj, "0x0", "0x0", 0x0, 0x0, 
                   0, 2, MC_ZERO_OBJ_ID, true);
    assert_vm_obj_list(&zero_obj->last_vm_area_node_ps, third_state, 2);

    //check the transfered last areas (indeces: 4, 8) now point to `/bin/cat`
    assert_vm_area(&m_a[4], NULL, NULL, 0x6000, 0x7000,
                    MC_ACCESS_READ | MC_ACCESS_WRITE, NULL, zero_node, 4, true);
    
    assert_vm_area(&m_a[8], NULL, NULL, 0xC000, 0xD000,
                    MC_ACCESS_READ | MC_ACCESS_WRITE, NULL, zero_node, 8, true);
    
    return;
    
} END_TEST


//_map_forward_unmapped_obj_last_vm_areas() [stub map fixture]
START_TEST(test__map_forward_unmapped_obj_last_vm_areas) {

    int ret;

    uintptr_t first_heap_state[2] = {0x6000, 0xC000};

    uintptr_t second_heap_state[1]  = {0x6000};
    uintptr_t second_lib_foo_state[1] = {0xC000};


    //setup the test by backtracking `/lib/foo`'s and `[heap]`'s last areas.
    ret = _map_backtrack_unmapped_obj_last_vm_areas(&m_o_n[2]);
    ck_assert_int_eq(ret, 0);

    ret = _map_backtrack_unmapped_obj_last_vm_areas(&m_o_n[1]);
    ck_assert_int_eq(ret, 0);


    //first test: pretend `[heap]` object was just inserted
    ret = _map_forward_unmapped_obj_last_vm_areas(&m_o_n[1]);
    ck_assert_int_eq(ret, 0);

    //check `/bin/cat` has no last areas associated with it
    assert_vm_obj(&m_o[0], "/bin/cat", "cat", 0x1000, 0x4000, 3, 0, 0, true);
    assert_vm_obj_list(&m_o[0].last_vm_area_node_ps, NULL, 0);

    //check the transferred last areas (indeces 4, 8) now point to  correct objs
    assert_vm_area(&m_a[4], NULL, NULL, 0x6000, 0x7000,
                    MC_ACCESS_READ | MC_ACCESS_WRITE, NULL, &m_o_n[1], 4, true);
    
    assert_vm_area(&m_a[8], NULL, NULL, 0xC000, 0xD000,
                    MC_ACCESS_READ | MC_ACCESS_WRITE, NULL, &m_o_n[1], 8, true);


    //second test: pretend the `/lib/foo` object was just inserted
    ret = _map_forward_unmapped_obj_last_vm_areas(&m_o_n[2]);
    ck_assert_int_eq(ret, 0);
    
    //check `[heap]` has only one last area associated with it
    assert_vm_obj(&m_o[1], "[heap]", "[heap]", 0x4000, 0x5000, 1, 1, 1, true);
    assert_vm_obj_list(&m_o[1].last_vm_area_node_ps, second_heap_state, 1);

    //check `/lib/foo` now has one last area associated with it
    assert_vm_obj(&m_o[2], "/lib/foo", "foo", 0x8000, 0xB000, 3, 1, 2, true);
    assert_vm_obj_list(&m_o[2].last_vm_area_node_ps, second_lib_foo_state, 1);

    //check the transferred last areas (indeces: 4, 8) now point to correct objs
    assert_vm_area(&m_a[4], NULL, NULL, 0x6000, 0x7000,
                    MC_ACCESS_READ | MC_ACCESS_WRITE, NULL, &m_o_n[1], 4, true);
    
    assert_vm_area(&m_a[8], NULL, NULL, 0xC000, 0xD000,
                    MC_ACCESS_READ | MC_ACCESS_WRITE, NULL, &m_o_n[2], 8, true);

    return;
    
} END_TEST


//_map_unlink_unmapped_obj() [stub map fixture]
START_TEST(test__map_unlink_unmapped_obj) {

    int ret;

    _traverse_state state;
    mc_vm_obj * obj;
    
    uintptr_t heap_state[2]  = {0x6000, 0xC000};
    
    struct obj_check obj_state[4] = {          //start index: 0
        {"0x0",     0x0,    0x0},
        {"cat",     0x1000, 0x4000},
        {"[heap]",  0x4000, 0x5000},
        {"[stack]", 0xE000, 0xF000}
    };
    
    struct obj_check unmapped_obj_state[1] = { //start index: 0
        {"foo",     MC_UNDEF_ADDR, MC_UNDEF_ADDR}
    };

                                
    //only test: unlink `/lib/foo`
    state.prev_obj_node  = &m_o_n[2];

    //clear constituent areas
    ret = cm_lst_emp(&m_o[2].vm_area_node_ps);
    ck_assert_int_eq(ret, 0);
    
    ret = _map_unlink_unmapped_obj(&m_o_n[2], &state, &m);
    ck_assert_int_eq(ret, 0);

    //check `/lib/foo` has no last areas associated with it, and is unmapped
    assert_vm_obj(&m_o[2], "/lib/foo", "foo",
                  MC_UNDEF_ADDR, MC_UNDEF_ADDR, 0, 0, 2, false);
    assert_vm_obj_list(&m_o[2].last_vm_area_node_ps, NULL, 0);

    //check `[heap]` has 2 last areas associated with it
    assert_vm_obj(&m_o[1], "[heap]", "[heap]", 0x4000, 0x5000, 1, 2, 1, true);
    assert_vm_obj_list(&m_o[1].last_vm_area_node_ps, heap_state, 2);

    //check state of mapped objects
    assert_vm_map_objs(&m.vm_objs, obj_state, 0, 4, true);

    //check state of unmapped objects
    assert_vm_map_objs(&m.vm_objs_unmapped, unmapped_obj_state, 0, 1, false);

    //check removed object has no links to other objects anymore
    ck_assert(m_o[2].mapped == false);
    ck_assert_int_eq(m_o[2].start_addr, MC_UNDEF_ADDR);
    ck_assert_int_eq(m_o[2].end_addr, MC_UNDEF_ADDR);
    ck_assert_ptr_null(m_o_n[2].next);
    ck_assert_ptr_null(m_o_n[2].prev);

    //check previous object state has been backtracked
    ck_assert_ptr_eq(state.prev_obj_node, &m_o_n[1]);

    return;
    
} END_TEST


//_map_unlink_unmapped_area() [stub map fixture]
START_TEST(test__map_unlink_unmapped_area) {
    
    int ret;
    _traverse_state state;

    //remove /lib/foo:1: object state
    struct obj_check first_objs[3] = {             //start index: 2
        {"[heap]",  0x4000, 0x5000},
        {"foo",     0x9000, 0xB000},
        {"[stack]", 0xE000, 0xF000}
    };

    //remove /lib/foo:1: area state
    struct area_check first_areas[3] = {           //start index: 4
        {"",        0x6000, 0x7000},
        {"foo",     0x9000, 0xA000},
        {"foo",     0xA000, 0xB000}
    };

    struct area_check first_areas_unmapped[1] = {  //start index: 0
        {"foo",     0x8000, 0x9000}
    };


    //remove [heap]: object state
    struct obj_check second_objs[2] = {            //start index: 1 
        {"cat",     0x1000, 0x4000},
        {"foo",     0x9000, 0xB000}
    };
    
    struct obj_check second_objs_unmapped[1] = {   //start index: 0
        {"[heap]",  MC_UNDEF_ADDR, MC_UNDEF_ADDR}
    };

    //remove [heap]: area state:
    struct area_check second_areas[2] = {          //start index: 2
        {"cat",     0x3000, 0x4000},
        {"",        0x6000, 0x7000}
    };

    struct area_check second_areas_unmapped[2] = { //start index: 0
        {"foo",     0x8000, 0x9000},
        {"[heap]",  0x4000, 0x5000}
    };
    

    //first test: remove first area of `/lib/foo`
    state.prev_obj_node = &m_o_n[1];

    ret = _map_unlink_unmapped_area(&m_a_n[5], &state, &m);
    ck_assert_int_eq(ret, 0);

    assert_vm_area(&m_a[5], "/lib/foo", "foo", 
                   0x8000, 0x9000, MC_ACCESS_READ, 
                   NULL, NULL, 5, false);
    
    assert_vm_map_objs(&m.vm_objs, first_objs, 2, 3, true);
    assert_vm_map_objs(&m.vm_objs_unmapped,
                       NULL, 0, 0, false);
    assert_vm_map_areas(&m.vm_areas, first_areas, 4, 3, true);
    assert_vm_map_areas(&m.vm_areas_unmapped,
                        first_areas_unmapped, 0, 1, false);

    ck_assert_ptr_eq(state.prev_obj_node, &m_o_n[1]);


    //second test: remove only area of '[heap]'
    /*state.prev_obj_node = &m_o_n[0];
    
    ret = _map_unlink_unmapped_area(&m_a_n[3], &state, &m);
    ck_assert_int_eq(ret, 0);

    assert_vm_area(&m_a[3], "[heap]", "[heap]", 
                    0x4000, 0x5000, MC_ACCESS_READ | MC_ACCESS_WRITE, 
                    NULL, NULL, 3, false);
    
    assert_vm_map_objs(&m.vm_objs, second_objs, 1, 2, true);
    assert_vm_map_objs(&m.vm_objs_unmapped,
                       second_objs_unmapped, 0, 1, false);
    assert_vm_map_areas(&m.vm_areas, second_areas, 2, 2, true);
    assert_vm_map_areas(&m.vm_areas_unmapped,
                        second_areas_unmapped, 0, 2, false);

    ck_assert_ptr_eq(state.prev_obj_node, &m_o_n[0]);
    */
    return;
    
} END_TEST


//_map_check_area_eql() [empty map fixture]
START_TEST(test__map_check_area_eql) {
    
    int ret;
    
    struct vm_entry entry;
    mc_vm_area area;
    cm_lst_node area_node;

    mc_vm_obj obj;
    cm_lst_node obj_node;


    //construct a vm_obj
    _map_new_vm_obj(&obj, &m, "/bin/cat");
    create_lst_wrapper(&obj_node, &obj);

    //create a vm_area
    _init_vm_entry(&entry, 0x1000, 0x2000, 0x0, MC_ACCESS_READ, "/bin/cat");
    _map_init_vm_area(&area, &entry, &obj_node, NULL, &m);
    create_lst_wrapper(&area_node, &area);


    //first test: entry same as vm_area
    ret = _map_check_area_eql(&entry, &area_node);
    ck_assert_int_eq(ret, 0);


    //second test: entry start address is different
    entry.vm_start = 0x500;
    ret = _map_check_area_eql(&entry, &area_node);
    ck_assert_int_eq(ret, -1);
    entry.vm_start = area.start_addr;


    //third test: entry end address is different
    entry.vm_end = 0x2500;
    ret = _map_check_area_eql(&entry, &area_node);
    ck_assert_int_eq(ret, -1);
    entry.vm_end = area.end_addr;


    //fourth test: entry permissions are different
    entry.prot = MC_ACCESS_READ | MC_ACCESS_WRITE;
    ret = _map_check_area_eql(&entry, &area_node);
    ck_assert_int_eq(ret, -1);
    entry.prot = area.access;


    //fifth test: both entry and area don't have a path
    entry.file_path[0] = '\0';
    area.pathname = NULL;
    ret = _map_check_area_eql(&entry, &area_node);
    ck_assert_int_eq(ret, 0);
    entry.file_path[0] = '/';
    area.pathname = obj.pathname;


    //sixth test: entry has a path, area does not
    area.pathname = NULL;
    ret = _map_check_area_eql(&entry, &area_node);
    ck_assert_int_eq(ret, -1);
    area.pathname = obj.pathname;


    //seventh test: entry does not have a path, area does
    entry.file_path[0] = '\0';
    ret = _map_check_area_eql(&entry, &area_node);
    ck_assert_int_eq(ret, -1);
    entry.file_path[0] = '/';


    //destroy pseudo object
    _map_del_vm_obj(&obj);

    return;
    
} END_TEST


//_map_state_inc_area() [stub map fixture]
START_TEST(test__map_state_inc_area) {

    _traverse_state state;


    //first test: keep
    state.next_area_node = &m_a_n[0];
    _map_state_inc_area(&state, _STATE_AREA_NODE_KEEP, NULL, &m);
    ck_assert_int_eq(MC_GET_NODE_AREA(state.next_area_node)->id, 0);

    //second test: advance - success
    state.next_area_node = &m_a_n[0];
    _map_state_inc_area(&state, _STATE_AREA_NODE_ADVANCE, NULL, &m);
    ck_assert_int_eq(MC_GET_NODE_AREA(state.next_area_node)->id, 1);

    //third test: advance - refuse (reached the end)
    state.next_area_node = &m_a_n[9];
    _map_state_inc_area(&state, _STATE_AREA_NODE_ADVANCE, NULL, &m);
    ck_assert_ptr_null(state.next_area_node);

    //fourth test: reassign
    state.next_area_node = &m_a_n[0];
    _map_state_inc_area(&state, _STATE_AREA_NODE_REASSIGN, &m_a_n[2], &m);
    ck_assert_int_eq(MC_GET_NODE_AREA(state.next_area_node)->id, 2);

    return;
    
} END_TEST


//_map_state_inc_obj() [stub map fixture]
START_TEST(test__map_state_inc_obj) {

    _traverse_state state;


    //first test: advance from pseudo object
    state.prev_obj_node = m.vm_objs.head;
    _map_state_inc_obj(&state, &m);
    ck_assert_int_eq(MC_GET_NODE_OBJ(state.prev_obj_node)->id, 0);
    
    //second test: advance from regular object
    state.prev_obj_node = &m_o_n[0];
    _map_state_inc_obj(&state, &m);
    ck_assert_int_eq(MC_GET_NODE_OBJ(state.prev_obj_node)->id, 1);

    //third test: advance from last object
    state.prev_obj_node = &m_o_n[3];
    _map_state_inc_obj(&state, &m);
    ck_assert_int_eq(MC_GET_NODE_OBJ(state.prev_obj_node)->id, 3);

    return;
    
} END_TEST


//_map_resync_area() [stub map fixture]
START_TEST(test__map_resync_area) {

    //remove [heap]: object state
    struct obj_check first_objs[2] = {             //start index: 1
        {"cat",     0x1000, 0x4000},
        {"foo",     0x8000, 0xB000}
    };

    struct obj_check first_objs_unmapped[1] = {    //start index: 0
        {"[heap]",  MC_UNDEF_ADDR, MC_UNDEF_ADDR}
    };
    
    //remove [heap]: area state
    struct area_check first_areas[2] = {           //start index: 2
        {"cat",     0x3000, 0x4000},
        {"",        0x6000, 0x7000}
    };

    struct area_check first_areas_unmapped[1] = {  //start index 0
        {"[heap]",  0x4000, 0x5000}
    };



    //remove /lib/foo:1,2: object state
    struct obj_check second_objs[3] = {            //start index: 1
        {"cat",     0x1000, 0x4000},
        {"foo",     0xA000, 0xB000},
        {"[stack]", 0xE000, 0xF000}
    };

    struct obj_check second_objs_unmapped[1] = {   //start index 0
        {"[heap]",  MC_UNDEF_ADDR, MC_UNDEF_ADDR}
    };
    
    //remove /lib/foo:1,2: area state
    struct area_check second_areas[3] = {          //start index 3
        {"",        0x6000, 0x7000},
        {"foo",     0xA000, 0xB000},
        {"",        0xC000, 0xD000}
    };

    struct area_check second_areas_unmapped[3] = { //start index 0
        {"[heap]",  0x4000, 0x5000},
        {"foo",     0x8000, 0x9000},
        {"foo",     0x9000, 0xA000},
    };



    //remove /bin/cat:1,2,3: object state
    struct obj_check third_objs[2] = {             //start index: 1
        {"foo",     0xA000, 0xB000},
        {"[stack]", 0xE000, 0xF000}
    };

    struct obj_check third_objs_unmapped[2] = {    //start index: 0
        {"[heap]", MC_UNDEF_ADDR, MC_UNDEF_ADDR},
        {"cat",    MC_UNDEF_ADDR, MC_UNDEF_ADDR}
    };
    
    //remove /bin/cat:1,2,3: area state
    struct area_check third_areas[2] = {           //start index: 0
        {"",    0x6000, 0x7000},
        {"foo", 0xA000, 0xB000}
    };

    struct area_check third_areas_unmapped[6] = {  //start index: 0
        {"[heap]", 0x4000, 0x5000},
        {"foo", 0x8000, 0x9000},
        {"foo", 0x9000, 0xA000},
        {"cat", 0x1000, 0x2000},
        {"cat", 0x2000, 0x3000},
        {"cat", 0x3000, 0x4000}
    };
    
    int ret;
    
    struct vm_entry entry;
    _traverse_state state;


    //correct by removing `[heap]`
    _init_vm_entry(&entry, 0x6000, 0x7000, 0x0, 
                   MC_ACCESS_READ | MC_ACCESS_WRITE, NULL);
    _init__traverse_state(&state, &m_a_n[3], &m_o_n[0]);

    ret = _map_resync_area(&entry, &state, &m);
    ck_assert_int_eq(ret, 1);

    //check state
    assert_vm_area(MC_GET_NODE_AREA(state.next_area_node), NULL, NULL,
                   0x6000, 0x7000, MC_ACCESS_READ | MC_ACCESS_WRITE, 
                   NULL, &m_o_n[0], 4, true);
    assert_vm_obj(MC_GET_NODE_OBJ(state.prev_obj_node), 
                  "/bin/cat", "cat", 0x1000, 0x4000, 3, 1, 0, true);
    
    assert_vm_map_objs(&m.vm_objs, first_objs, 1, 2, true);
    assert_vm_map_objs(&m.vm_objs_unmapped,
                       first_objs_unmapped, 0, 1, false);
    assert_vm_map_areas(&m.vm_areas, first_areas, 2, 2, true);
    assert_vm_map_areas(&m.vm_areas_unmapped,
                        first_areas_unmapped, 0, 1, false);
    


    //correct by removing first 2 areas of `/lib/foo`
    _init_vm_entry(&entry, 0xA000, 0xB000, 0x0, 
                   MC_ACCESS_READ | MC_ACCESS_EXEC, "/lib/foo");
    _init__traverse_state(&state, &m_a_n[5], &m_o_n[0]);

    ret = _map_resync_area(&entry, &state, &m);
    ck_assert_int_eq(ret, 1);

    //check state
    assert_vm_area(MC_GET_NODE_AREA(state.next_area_node), "/lib/foo", "foo",
                   0xA000, 0xB000, MC_ACCESS_READ | MC_ACCESS_EXEC, 
                   &m_o_n[2], NULL, 7, true);

    assert_vm_obj(MC_GET_NODE_OBJ(state.prev_obj_node), "/bin/cat", "cat",
                   0x1000, 0x4000, 3, 1, 0, true);

    assert_vm_map_objs(&m.vm_objs, second_objs, 1, 3, true);
    assert_vm_map_objs(&m.vm_objs_unmapped,
                       second_objs_unmapped, 0, 1, false);
    assert_vm_map_areas(&m.vm_areas, second_areas, 3, 3, true);
    assert_vm_map_areas(&m.vm_areas_unmapped,
                        second_areas_unmapped, 0, 3, false);



    //correct by removing entirety of `/bin/cat`
    _init_vm_entry(&entry, 0x6000, 0x7000, 0x0, 
                   MC_ACCESS_READ | MC_ACCESS_WRITE, NULL);
    _init__traverse_state(&state, &m_a_n[0], m.vm_objs.head);

    ret = _map_resync_area(&entry, &state, &m);
    ck_assert_int_eq(ret, 1);

    //check state
    assert_vm_area(MC_GET_NODE_AREA(state.next_area_node),
                   NULL, NULL, 0x6000, 0x7000,
                   MC_ACCESS_READ | MC_ACCESS_WRITE,
                   NULL, m.vm_objs.head, 4, true);

    assert_vm_obj(MC_GET_NODE_OBJ(m.vm_objs.head), "0x0", "0x0", 
                   0x0, 0x0, 0, 1, MC_ZERO_OBJ_ID, true);

    assert_vm_map_objs(&m.vm_objs, third_objs, 1, 2, true);
    assert_vm_map_objs(&m.vm_objs_unmapped,
                       third_objs_unmapped, 0, 2, false);
    assert_vm_map_areas(&m.vm_areas, third_areas, 0, 2, true);
    assert_vm_map_areas(&m.vm_areas_unmapped,
                        third_areas_unmapped, 0, 6, false);

    return;
    
} END_TEST


//_map_add_obj() [stub map fixture]
START_TEST(test__map_add_obj) {

    //test data
    struct obj_check first_objs[3] = {  //start index: 3
        {"foo",     0x8000, 0xB000},
        {"bar",     MC_UNDEF_ADDR, MC_UNDEF_ADDR},
        {"[stack]", 0xE000, 0xF000}
    };

    struct obj_check second_objs[3] = { //start index: 1
        {"[heap]",  0x4000, 0x5000},
        {"dog",     MC_UNDEF_ADDR, MC_UNDEF_ADDR},
        {"foo",     0x8000, 0xB000}
    };


    //test vars
    cm_lst_node * ret_node;

    struct vm_entry entry;
    _traverse_state state;


    //first test: no forwarding last memory areas
    _init_vm_entry(&entry, 0xD000, 0xE000, 0x0, MC_ACCESS_READ, "/lib/bar");
    _init__traverse_state(&state, NULL, &m_o_n[2]);

    ret_node = _map_add_obj(&entry, &state, &m);
    ck_assert_ptr_nonnull(ret_node);

    assert_vm_map_objs(&m.vm_objs, first_objs, 3, 3, true);
    assert_lst_len(&MC_GET_NODE_OBJ(ret_node)->last_vm_area_node_ps, 0);


    //second test: forwarding last memory areas
    _init_vm_entry(&entry, 0x5000, 0x6000, 0x0, MC_ACCESS_READ, "/bin/dog");
    _init__traverse_state(&state, NULL, &m_o_n[1]);

    ret_node = _map_add_obj(&entry, &state, &m);
    ck_assert_ptr_nonnull(ret_node);

    assert_vm_map_objs(&m.vm_objs, second_objs, 2, 3, true);
    assert_lst_len(&MC_GET_NODE_OBJ(ret_node)->last_vm_area_node_ps, 0);
    
    return;
    
} END_TEST


//_map_add_area() [stub map fixture]
START_TEST(test__map_add_area) {
    
    //test data
    struct area_check first_areas[3] = {  //start_index: 7
        {"foo",     0xA000, 0xB000},
        {"foo",     0xB000, 0xC000},
        {"",        0xC000, 0xD000}
    };
    
    struct obj_check first_objs[2] = {    //start index: 3
        {"foo",     0x8000, 0xC000},
        {"[stack]", 0xE000, 0xF000}
    };

    struct area_check second_areas[3] = { //start_index: 4
        {"",        0x6000, 0x7000},
        {"",        0x7000, 0x8000},
        {"foo",     0x8000, 0x9000}
    };

    struct obj_check second_objs[2] = {   //start index: 2
        {"[heap]",  0x4000, 0x5000},
        {"foo",     0x8000, 0xC000},
    };

    struct area_check third_areas[3] = {  //start_index: 3
        {"[heap]",  0x4000, 0x5000},
        {"bar",     0x5000, 0x6000},
        {"",        0x6000, 0x7000}
    };
    
    struct obj_check third_objs[3] = {    //start index: 2
        {"[heap]",  0x4000, 0x5000},
        {"bar",     0x5000, 0x6000},
        {"foo",     0x8000, 0xC000}
    };


    //test vars
    int ret;

    struct vm_entry entry;
    _traverse_state state;


    //first test: add additional area to the end of `/lib/foo`
    _init_vm_entry(&entry, 0xB000, 0xC000, 0x0, MC_ACCESS_READ, "/lib/foo");
    _init__traverse_state(&state, &m_a_n[8], &m_o_n[2]);

    ret = _map_add_area(&entry, &state, &m);
    ck_assert_int_eq(ret, 0);

    assert_vm_map_areas(&m.vm_areas, first_areas, 7, 3, true);
    assert_vm_map_objs(&m.vm_objs, first_objs, 3, 2, true);


    //second test: add area without a backing object before `/lib/foo`
    _init_vm_entry(&entry, 0x7000, 0x8000, 0x0, MC_ACCESS_READ, NULL);
    _init__traverse_state(&state, &m_a_n[5], &m_o_n[1]);

    ret = _map_add_area(&entry, &state, &m);
    ck_assert_int_eq(ret, 0);

    assert_vm_map_areas(&m.vm_areas, second_areas, 4, 3, true);
    assert_vm_map_objs(&m.vm_objs, second_objs, 2, 2, true);


    //third test: add an area that creates a new object `/lib/bar`
    _init_vm_entry(&entry, 0x5000, 0x6000, 0x0, MC_ACCESS_READ, "/lib/bar");
    _init__traverse_state(&state, &m_a_n[4], &m_o_n[1]);

    ret = _map_add_area(&entry, &state, &m);
    ck_assert_int_eq(ret, 0);

    assert_vm_map_areas(&m.vm_areas, third_areas, 3, 3, true);
    assert_vm_map_objs(&m.vm_objs, third_objs, 2, 3, true);
    
    return;
    
} END_TEST



//map_send_entry() [stub map fixture]
START_TEST(test_map_send_entry) {

    //test data
    struct area_check first_areas[2] = {           //start_index: 9
        {"[stack]", 0x0E000, 0x0F000},
        {"dog",     0x0F000, 0x10000},
    };
    
    struct obj_check first_objs[2] = {             //start index: 4
        {"[stack]", 0x0E000, 0x0F000},
        {"dog",     0x0F000, 0x10000}
    };


    struct area_check second_areas[3] = {          //start_index: 9
        {"[stack]", 0x0E000, 0x0F000},
        {"dog",     0x0F000, 0x10000},
        {"dog",     0x10000, 0x11000}
    };

    struct obj_check second_objs[2] = {            //start index: 4
        {"[stack]", 0x0E000, 0x0F000},
        {"dog",     0x0F000, 0x11000}
    };


    struct area_check third_areas[2] = {           //start_index: 0
        {"[heap]",  0x04000, 0x05000},
        {"",        0x06000, 0x07000}
    };
    
    struct obj_check third_objs[3] = {             //start index: 0
        {"0x0",     0x0,     0x0},
        {"[heap]",  0x04000, 0x05000},
        {"foo",     0x08000, 0x0B000}
    };

    struct area_check third_areas_unmapped[3] = {  //start_index: 0
        {"cat",     0x01000, 0x02000},
        {"cat",     0x02000, 0x03000},
        {"cat",     0x03000, 0x04000}
    };
    
    struct obj_check third_objs_unmapped[1] = {    //start index: 0
        {"cat",     MC_UNDEF_ADDR, MC_UNDEF_ADDR},
    };


    struct area_check fourth_areas[3] = {          //start_index: 0
        {"[heap]",  0x04000, 0x05000},
        {"",        0x06000, 0x07000},
        {"foo",     0x08000, 0x09000},
    };
    
    struct obj_check fourth_objs[3] = {            //start index: 0
        {"0x0",     0x0,     0x0},
        {"[heap]",  0x04000, 0x05000},
        {"foo",     0x08000, 0x0B000}
    };

    struct area_check fourth_areas_unmapped[3] = { //start_index: 0
        {"cat",     0x01000, 0x02000},
        {"cat",     0x02000, 0x03000},
        {"cat",     0x03000, 0x04000}
    };
    
    struct obj_check fourth_objs_unmapped[1] = {   //start index: 0
        {"cat",     MC_UNDEF_ADDR, MC_UNDEF_ADDR},
    };

    int ret;

    struct vm_entry entry;
    _traverse_state state;
    

    //first test: send a "/bin/dog" entry to the end of the map
    _init_vm_entry(&entry, 0x0F000, 0x10000, 0x0, MC_ACCESS_READ, "/bin/dog");
    _init__traverse_state(&state, NULL, m.vm_objs.head->prev);

    ret = map_send_entry(&entry, &state, &m);
    ck_assert_int_eq(ret, 0);

    assert_vm_map_areas(&m.vm_areas, first_areas, 9, 2, true);
    assert_vm_map_objs(&m.vm_objs, first_objs, 4, 2, true);


    //second test: send another "/bin/dog" area to the end of the map
    _init_vm_entry(&entry, 0x10000, 0x11000, 0x0, MC_ACCESS_READ, "/bin/dog");
    _init__traverse_state(&state, NULL, m.vm_objs.head->prev);

    ret = map_send_entry(&entry, &state, &m);
    ck_assert_int_eq(ret, 0);

    assert_vm_map_areas(&m.vm_areas, second_areas, 9, 3, true);
    assert_vm_map_objs(&m.vm_objs, second_objs, 4, 2, true);


    //third test: send a "[heap]" entry to the start of the map
    _init_vm_entry(&entry, 0x04000, 0x05000, 0x0, 
                   MC_ACCESS_READ | MC_ACCESS_WRITE, "[heap]");
    _init__traverse_state(&state, m.vm_areas.head, m.vm_objs.head);

    ret = map_send_entry(&entry, &state, &m);
    ck_assert_int_eq(ret, 0);

    assert_vm_map_areas(&m.vm_areas, third_areas, 0, 2, true);
    assert_vm_map_areas(&m.vm_areas_unmapped,
                        third_areas_unmapped, 0, 3, false);
    assert_vm_map_objs(&m.vm_objs, third_objs, 0, 3, true);
    assert_vm_map_objs(&m.vm_objs_unmapped,
                       third_objs_unmapped, 0, 1, false);


    //fourth test: send a correct entry after the "[heap]" entry
    _init_vm_entry(&entry, 0x06000, 0x07000, 0x0, 
                   MC_ACCESS_READ | MC_ACCESS_WRITE, NULL);
    _init__traverse_state(&state, m.vm_areas.head->next, m.vm_objs.head->next);

    ret = map_send_entry(&entry, &state, &m);
    ck_assert_int_eq(ret, 0);

    assert_vm_map_areas(&m.vm_areas, fourth_areas, 0, 3, true);
    assert_vm_map_areas(&m.vm_areas_unmapped,
                        fourth_areas_unmapped, 0, 3, false);
    assert_vm_map_objs(&m.vm_objs, fourth_objs, 0, 3, true);
    assert_vm_map_objs(&m.vm_objs_unmapped,
                       fourth_objs_unmapped, 0, 1, false);

    return;
    
} END_TEST


//map_init_traverse_state() [empty map fixture]
START_TEST(test_map_init_traverse_state) {

    struct vm_entry entry;
    _traverse_state state;

    
    //first test: empty map
    map_init_traverse_state(&state, &m);
    ck_assert_ptr_null(state.next_area_node);
    ck_assert_ptr_eq(state.prev_obj_node, m.vm_objs.head);


    //add an area
    _init_vm_entry(&entry, 0x1000, 0x2000, 0x0, MC_ACCESS_READ, "/bin/cat");
    map_send_entry(&entry, &state, &m);


    //second test: existing map
    map_init_traverse_state(&state, &m);
    ck_assert_ptr_eq(state.next_area_node, m.vm_areas.head);
    ck_assert_ptr_eq(state.prev_obj_node, m.vm_objs.head);    

    return;
    
} END_TEST


//mc_map_clean_unmapped() [stub map fixture]
START_TEST(test_mc_map_clean_unmapped) {

    int ret;

    struct vm_entry entry;
    _traverse_state state;


    //first test: add /bin/dog and unmap it
    _init_vm_entry(&entry, 0x0000, 0x1000, 0x0, 
                   MC_ACCESS_READ | MC_ACCESS_WRITE, "/bin/dog");
    _init__traverse_state(&state, m.vm_areas.head, m.vm_objs.head);
    ret = map_send_entry(&entry, &state, &m);
    ck_assert_int_eq(ret, 0);

    _init_vm_entry(&entry, 0x1000, 0x2000, 0x0, 
                   MC_ACCESS_READ, "/bin/cat");
    _init__traverse_state(&state, m.vm_areas.head, m.vm_objs.head->next);
    ret = map_send_entry(&entry, &state, &m);
    ck_assert_int_eq(ret, 0);

    ret = mc_map_clean_unmapped(&m);
    ck_assert_int_eq(ret, 0);

    assert_lst_len(&m.vm_areas_unmapped, 0);
    assert_lst_len(&m.vm_objs_unmapped, 0);


    //second test: try cleaning unmapped areas when none are present
    ret = mc_map_clean_unmapped(&m);
    ck_assert_int_eq(ret, 0);
    
    return;
    
} END_TEST
#endif


/*
 *  --- [SUITE] ---
 */

 Suite * map_suite() {

    //test cases
    TCase * tc_new_del_vm_map;

    #ifdef DEBUG
    TCase * tc__new_del_vm_obj;
    TCase * tc__make_zero_obj;
    TCase * tc__init_vm_area;
    TCase * tc__obj_add_area;
    TCase * tc__obj_add_last_area;
    TCase * tc__obj_rmv_area;
    TCase * tc__obj_rmv_last_area;
    TCase * tc__is_pathname_in_obj;
    TCase * tc__find_obj_for_area;
    TCase * tc__backtrack_unmapped_obj_last_vm_areas;
    TCase * tc__forward_unmapped_obj_last_vm_areas;
    TCase * tc__unlink_unmapped_obj;
    TCase * tc__unlink_unmapped_area;
    TCase * tc__check_area_eql;
    TCase * tc__state_inc_area;
    TCase * tc__state_inc_obj;
    TCase * tc__resync_area;
    TCase * tc__add_obj;
    TCase * tc__add_area;
    TCase * tc_send_entry;
    TCase * tc_init_traverse_state;
    TCase * tc_clean_unmapped;
    #endif
    
    Suite * s = suite_create("map");


    //tc_new_del_vm_map
    tc_new_del_vm_map = tcase_create("new_del_vm_map");
    tcase_add_test(tc_new_del_vm_map, test_mc_new_del_vm_map);

    #ifdef DEBUG
    //tc__new_del_vm_obj
    tc__new_del_vm_obj = tcase_create("_new_del_vm_obj");
    tcase_add_checked_fixture(tc__new_del_vm_obj, 
                              _setup_empty_vm_map, _teardown_vm_map);
    tcase_add_test(tc__new_del_vm_obj, test__map_new_del_vm_obj);

    //tc__make_zero_obj
    tc__make_zero_obj = tcase_create("_make_zero_obj");
    tcase_add_checked_fixture(tc__make_zero_obj, 
                              _setup_empty_vm_map, _teardown_vm_map);
    tcase_add_test(tc__make_zero_obj, test__map_make_zero_obj);

    //tc__init_vm_area
    tc__init_vm_area = tcase_create("_init_vm_area");
    tcase_add_checked_fixture(tc__init_vm_area, 
                              _setup_empty_vm_obj, _teardown_vm_obj);
    tcase_add_test(tc__init_vm_area, test__map_init_vm_area);

    //tc__obj_add_area
    tc__obj_add_area = tcase_create("_obj_add_area");
    tcase_add_checked_fixture(tc__obj_add_area, 
                              _setup_empty_vm_obj, _teardown_vm_obj);
    tcase_add_test(tc__obj_add_area, test__map_obj_add_area);

    //tc__obj_add_last_area
    tc__obj_add_last_area = tcase_create("_obj_add_last_area");
    tcase_add_checked_fixture(tc__obj_add_last_area, 
                              _setup_empty_vm_obj, _teardown_vm_obj);
    tcase_add_test(tc__obj_add_last_area, test__map_obj_add_last_area);
    
    //tc__obj_rmv_area
    tc__obj_rmv_area = tcase_create("_obj_rmv_area");
    tcase_add_checked_fixture(tc__obj_rmv_area, 
                              _setup_stub_vm_obj, _teardown_vm_obj);
    tcase_add_test(tc__obj_rmv_area, test__map_obj_rmv_area);

    //tc__obj_rmv_last_area
    tc__obj_rmv_last_area = tcase_create("_obj_rmv_last_area");
    tcase_add_checked_fixture(tc__obj_rmv_last_area, 
                              _setup_stub_vm_obj, _teardown_vm_obj);
    tcase_add_test(tc__obj_rmv_last_area, test__map_obj_rmv_last_area);

    //tc__is_pathname_in_obj
    tc__is_pathname_in_obj = tcase_create("_is_pathname_in_obj");
    tcase_add_checked_fixture(tc__is_pathname_in_obj, 
                              _setup_empty_vm_obj, _teardown_vm_obj);
    tcase_add_test(tc__is_pathname_in_obj, test__map_is_pathname_in_obj);

    //tc__find_obj_for_area
    tc__find_obj_for_area = tcase_create("_find_obj_for_area");
    tcase_add_checked_fixture(tc__find_obj_for_area, 
                              _setup_empty_vm_map, _teardown_vm_map);
    tcase_add_test(tc__find_obj_for_area, test__map_find_obj_for_area);

    //tc__backtrack_unmapped_obj_last_vm_areas
    tc__backtrack_unmapped_obj_last_vm_areas 
        = tcase_create("_backtrack_unmapped_obj_last_vm_areas");
    tcase_add_checked_fixture(tc__backtrack_unmapped_obj_last_vm_areas, 
                              _setup_stub_vm_map, _teardown_vm_map);
    tcase_add_test(tc__backtrack_unmapped_obj_last_vm_areas, 
                   test__map_backtrack_unmapped_obj_last_vm_areas);

    //tc__forward_unmapped_obj_last_vm_areas
    tc__forward_unmapped_obj_last_vm_areas 
        = tcase_create("_forward_unmapped_obj_last_vm_areas");
    tcase_add_checked_fixture(tc__forward_unmapped_obj_last_vm_areas, 
                              _setup_stub_vm_map, _teardown_vm_map);
    tcase_add_test(tc__forward_unmapped_obj_last_vm_areas, 
                   test__map_forward_unmapped_obj_last_vm_areas);

    //tc__unlink_unmapped_obj
    tc__unlink_unmapped_obj = tcase_create("_unlink_unmapped_obj");
    tcase_add_checked_fixture(tc__unlink_unmapped_obj, 
                              _setup_stub_vm_map, _teardown_vm_map);
    tcase_add_test(tc__unlink_unmapped_obj, test__map_unlink_unmapped_obj);

    //tc__unlink_unmapped_area
    tc__unlink_unmapped_area = tcase_create("_unlink_unmapped_area");
    tcase_add_checked_fixture(tc__unlink_unmapped_area, 
                              _setup_stub_vm_map, _teardown_vm_map);
    tcase_add_test(tc__unlink_unmapped_area, test__map_unlink_unmapped_area);
    
    //tc__check_area_eql
    tc__check_area_eql = tcase_create("_check_area_eql");
    tcase_add_checked_fixture(tc__check_area_eql, 
                              _setup_empty_vm_map, _teardown_vm_map);
    tcase_add_test(tc__check_area_eql, test__map_check_area_eql);

    //tc__state_inc_area
    tc__state_inc_area = tcase_create("_state_inc_area");
    tcase_add_checked_fixture(tc__state_inc_area, 
                              _setup_stub_vm_map, _teardown_vm_map);
    tcase_add_test(tc__state_inc_area, test__map_state_inc_area);

    //tc__state_inc_obj
    tc__state_inc_obj = tcase_create("_state_inc_obj");
    tcase_add_checked_fixture(tc__state_inc_obj, 
                              _setup_stub_vm_map, _teardown_vm_map);
    tcase_add_test(tc__state_inc_obj, test__map_state_inc_obj);

    //tc__resync_area
    tc__resync_area = tcase_create("_resync_area");
    tcase_add_checked_fixture(tc__resync_area, 
                              _setup_stub_vm_map, _teardown_vm_map);
    tcase_add_test(tc__resync_area, test__map_resync_area);

    //tc__add_obj
    tc__add_obj = tcase_create("_add_obj");
    tcase_add_checked_fixture(tc__add_obj, 
                              _setup_stub_vm_map, _teardown_vm_map);
    tcase_add_test(tc__add_obj, test__map_add_obj);

    //tc__add_area
    tc__add_area = tcase_create("_add_area");
    tcase_add_checked_fixture(tc__add_area, 
                              _setup_stub_vm_map, _teardown_vm_map);
    tcase_add_test(tc__add_area, test__map_add_area);

    //tc_send_entry
    tc_send_entry = tcase_create("send_entry");
    tcase_add_checked_fixture(tc_send_entry, 
                              _setup_stub_vm_map, _teardown_vm_map);
    tcase_add_test(tc_send_entry, test_map_send_entry);

    //tc_init_traverse_state
    tc_init_traverse_state = tcase_create("init_traverse_state");
    tcase_add_checked_fixture(tc_init_traverse_state, 
                              _setup_empty_vm_map, _teardown_vm_map);
    tcase_add_test(tc_init_traverse_state, test_map_init_traverse_state);

    //tc_clean_unmapped
    tc_clean_unmapped = tcase_create("clean_unmapped");
    tcase_add_checked_fixture(tc_clean_unmapped, 
                              _setup_stub_vm_map, _teardown_vm_map);
    tcase_add_test(tc_clean_unmapped, test_mc_map_clean_unmapped);
    #endif

    
    //add test cases to map test suite
    suite_add_tcase(s, tc_new_del_vm_map);
    
    #ifdef DEBUG
    suite_add_tcase(s, tc__new_del_vm_obj);
    suite_add_tcase(s, tc__make_zero_obj);
    suite_add_tcase(s, tc__init_vm_area);
    suite_add_tcase(s, tc__obj_add_area);
    suite_add_tcase(s, tc__obj_add_last_area);
    suite_add_tcase(s, tc__obj_rmv_area);
    suite_add_tcase(s, tc__obj_rmv_last_area);
    suite_add_tcase(s, tc__is_pathname_in_obj);
    suite_add_tcase(s, tc__find_obj_for_area);
    suite_add_tcase(s, tc__backtrack_unmapped_obj_last_vm_areas);
    suite_add_tcase(s, tc__forward_unmapped_obj_last_vm_areas);
    suite_add_tcase(s, tc__unlink_unmapped_obj);
    suite_add_tcase(s, tc__unlink_unmapped_area);
    suite_add_tcase(s, tc__check_area_eql);
    suite_add_tcase(s, tc__state_inc_area);
    suite_add_tcase(s, tc__state_inc_obj);
    suite_add_tcase(s, tc__resync_area);
    suite_add_tcase(s, tc__add_obj);
    suite_add_tcase(s, tc__add_area);
    suite_add_tcase(s, tc_send_entry);
    suite_add_tcase(s, tc_init_traverse_state);
    suite_add_tcase(s, tc_clean_unmapped);
    #endif

    return s; 
 }
