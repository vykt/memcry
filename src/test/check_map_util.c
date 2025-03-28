//standard library
#include <stdlib.h>
#include <stdint.h>

//system headers
#include <unistd.h>

//external libraries
#include <cmore.h>
#include <check.h>

//local headers
#include "suites.h"
#include "target_helper.h"

//test target headers
#include "../lib/memcry.h"



/*
 *  [BASIC TEST]
 */


//globals
static mc_vm_map m;
static mc_session s;
static pid_t pid;



/*
 *  --- [FIXTURES] ---
 */

//initialise the target
static void _setup_target() {

    int ret;


    pid = start_target();
    ck_assert_int_ne(pid, -1);

    ret = mc_open(&s, PROCFS, pid);
    ck_assert_int_eq(ret, 0);

    mc_new_vm_map(&m);
    ret = mc_update_map(&s, &m);
    ck_assert_int_eq(ret, 0);

    return;
}


//teardown the target
static void _teardown_target() {

    int ret;


    ret = mc_del_vm_map(&m);
    ck_assert_int_eq(ret, 0);
 
    ret = mc_close(&s);
    ck_assert_int_eq(ret, 0);

    end_target(pid);

    return;
}



/*
 *  --- [UNIT TESTS] ---
 */

//mc_get_area_off() [target fixture]
START_TEST(test_mc_get_area_off) {

    off_t off;

    mc_vm_obj * o;
    cm_lst_node * a_n;


    //first test: typical offset
    o = MC_GET_NODE_OBJ(m.vm_objs.head);
    a_n = MC_GET_NODE_PTR(o->last_vm_area_node_ps.head);

    off = mc_get_area_off(a_n, 0x10800);
    ck_assert_int_eq(off, 0x800);


    //second test: address is lower than area's starting address
    off = mc_get_area_off(a_n, 0xF800);
    ck_assert_int_eq(off, -0x800);

    return;
   
} END_TEST


//mc_get_obj_off() [target fixture]
START_TEST(test_mc_get_obj_off) {

    off_t off;

    cm_lst_node * o_n;
    mc_vm_obj * o;


    //first test: typical offset
    o_n = m.vm_objs.head;
    o = MC_GET_NODE_OBJ(o_n);

    off = mc_get_obj_off(o_n, 0x800);
    ck_assert_int_eq(off, 0x800);


    //second test: address is lower than obj's starting address
    o->end_addr = o->start_addr = 0x1000;
    off = mc_get_obj_off(o_n, 0x800);
    ck_assert_int_eq(off, -0x800);
    
    return;
    
} END_TEST


//mc_get_area_off_bnd() [target fixture]
START_TEST(test_mc_get_area_off_bnd) {

    off_t off;

    mc_vm_obj * o;
    cm_lst_node * a_n;


    //setup test
    o = MC_GET_NODE_OBJ(m.vm_objs.head);
    a_n = MC_GET_NODE_PTR(o->last_vm_area_node_ps.head);


    //first test: typical offset
    off = mc_get_area_off_bnd(a_n, 0x10800);
    ck_assert_int_eq(off, 0x800);


    //second test: address is lower than area's starting address
    off = mc_get_area_off_bnd(a_n, 0xF800);
    ck_assert_int_eq(off, -1);

    return;
   
} END_TEST


//mc_get_obj_off_bnd() [target fixture]
START_TEST(test_mc_get_obj_off_bnd) {

    off_t off;

    cm_lst_node * o_n;
    mc_vm_obj * o;
    

    //setup test
    o_n = m.vm_objs.head;
    o = MC_GET_NODE_OBJ(o_n);


    //first test: typical offset
    o->end_addr = 0x1000;
    off = mc_get_obj_off_bnd(o_n, 0x800);
    ck_assert_int_eq(off, 0x800);


    //second test: address is lower than obj's starting address
    o->start_addr = 0x1000;
    off = mc_get_obj_off_bnd(o_n, 0x800);
    ck_assert_int_eq(off, -1);
    
    return;
    
} END_TEST


//mc_get_area_by_addr [target fixture]
START_TEST(test_mc_get_area_by_addr) {

    cm_lst_node * ret_n;
    off_t off;

    mc_vm_obj * o;
    mc_vm_area * a;
    cm_lst_node * a_n;


    //first test: mapped address
    a_n = m.vm_areas.head->next->next;
    a   = MC_GET_NODE_AREA(a_n);

    ret_n = mc_get_area_by_addr(&m, a->start_addr + 0x200, &off);
    ck_assert_ptr_eq(ret_n, a_n);
    ck_assert_int_eq(off, 0x200);


    //second test: unmapped address
    off = 0x0;

    ret_n = mc_get_area_by_addr(&m, 0x1337, &off);
    ck_assert_ptr_null(ret_n);
    ck_assert_int_eq(off, 0);

    return;
    
} END_TEST


//mc_get_obj_by_addr [target fixture]
START_TEST(test_mc_get_obj_by_addr) {

    cm_lst_node * ret_n;
    off_t off;

    mc_vm_obj * o;
    cm_lst_node * o_n;


    //first test: mapped address
    o_n = m.vm_objs.head->next->next;
    o   = MC_GET_NODE_OBJ(o_n);

    ret_n = mc_get_obj_by_addr(&m, o->start_addr + 0x200, &off);
    ck_assert_ptr_eq(ret_n, o_n);
    ck_assert_int_eq(off, 0x200);


    //second test: unmapped address
    off = 0x0;

    ret_n = mc_get_obj_by_addr(&m, 0x1337, &off);
    ck_assert_ptr_null(ret_n);
    ck_assert_int_eq(off, 0);

    return;

} END_TEST


//mc_get_obj_by_pathname() [target fixture]
START_TEST(test_mc_get_obj_by_pathname) {

    cm_lst_node * ret_n;

    mc_vm_obj * o;
    cm_lst_node * o_n;
    char * pathname;


    //first test: pathname exists
    o_n = m.vm_objs.head->next->next;
    o = MC_GET_NODE_OBJ(o_n); 

    pathname = o->pathname;
    ret_n = mc_get_obj_by_pathname(&m, pathname);
    ck_assert_ptr_eq(ret_n, o_n);


    //second test: pathname doesn't exist
    pathname = "/foo/bar";
    ret_n = mc_get_obj_by_pathname(&m, pathname);
    ck_assert_ptr_null(ret_n);

    return;
    
} END_TEST


//mc_get_obj_by_basename() [target_fixture]
START_TEST(test_mc_get_obj_by_basename) {


    cm_lst_node * ret_n;

    mc_vm_obj * o;
    cm_lst_node * o_n;
    char * basename;


    //first test: pathname exists
    o_n = m.vm_objs.head->next->next;
    o = MC_GET_NODE_OBJ(o_n); 

    basename = o->basename;
    ret_n = mc_get_obj_by_basename(&m, basename);
    ck_assert_ptr_eq(ret_n, o_n);


    //second test: pathname doesn't exist
    basename = "1337";
    ret_n = mc_get_obj_by_basename(&m, basename);
    ck_assert_ptr_null(ret_n);

    return;

} END_TEST



/*
 *  --- [SUITE] ---
 */

Suite * map_util_suite() {

    //test cases
    TCase * tc_get_area_off;
    TCase * tc_get_obj_off;
    TCase * tc_get_area_off_bnd;
    TCase * tc_get_obj_off_bnd;
    TCase * tc_get_area_by_addr;
    TCase * tc_get_obj_by_addr;
    TCase * tc_get_obj_by_pathname;
    TCase * tc_get_obj_by_basename;

    Suite * s = suite_create("map_util");


    //tc_get_area_off
    tc_get_area_off = tcase_create("get_area_off");
    tcase_add_checked_fixture(tc_get_area_off,
                              _setup_target, _teardown_target);
    tcase_add_test(tc_get_area_off, test_mc_get_area_off);

    //tc_get_obj_off
    tc_get_obj_off = tcase_create("get_obj_off");
    tcase_add_checked_fixture(tc_get_obj_off,
                              _setup_target, _teardown_target);
    tcase_add_test(tc_get_obj_off, test_mc_get_obj_off);

    //tc_get_area_off_bnd
    tc_get_area_off_bnd = tcase_create("get_area_off_bnd");
    tcase_add_checked_fixture(tc_get_area_off_bnd,
                              _setup_target, _teardown_target);
    tcase_add_test(tc_get_area_off_bnd, test_mc_get_area_off_bnd);

    //tc_get_obj_off_bnd
    tc_get_obj_off_bnd = tcase_create("get_obj_off_bnd");
    tcase_add_checked_fixture(tc_get_obj_off_bnd,
                              _setup_target, _teardown_target);
    tcase_add_test(tc_get_obj_off_bnd, test_mc_get_obj_off_bnd);

    //tc_get_area_by_addr
    tc_get_area_by_addr = tcase_create("get_area_by_addr");
    tcase_add_checked_fixture(tc_get_area_by_addr,
                              _setup_target, _teardown_target);
    tcase_add_test(tc_get_area_by_addr, test_mc_get_area_by_addr);

    //tc_get_area_by_addr
    tc_get_obj_by_addr = tcase_create("get_obj_by_addr");
    tcase_add_checked_fixture(tc_get_obj_by_addr,
                              _setup_target, _teardown_target);
    tcase_add_test(tc_get_obj_by_addr, test_mc_get_obj_by_addr);

    //tc_get_obj_by_pathname
    tc_get_obj_by_pathname = tcase_create("get_obj_by_pathname");
    tcase_add_checked_fixture(tc_get_obj_by_pathname,
                              _setup_target, _teardown_target);
    tcase_add_test(tc_get_obj_by_pathname,
                   test_mc_get_obj_by_pathname);

    //tc_get_obj_by_basename
    tc_get_obj_by_basename = tcase_create("get_obj_by_basename");
    tcase_add_checked_fixture(tc_get_obj_by_basename,
                              _setup_target, _teardown_target);
    tcase_add_test(tc_get_obj_by_basename,
                   test_mc_get_obj_by_basename);


    //add test cases to map util test suite
    suite_add_tcase(s, tc_get_area_off);
    suite_add_tcase(s, tc_get_obj_off);
    suite_add_tcase(s, tc_get_area_off_bnd);
    suite_add_tcase(s, tc_get_obj_off_bnd);
    suite_add_tcase(s, tc_get_area_by_addr);
    suite_add_tcase(s, tc_get_obj_by_addr);
    suite_add_tcase(s, tc_get_obj_by_pathname);
    suite_add_tcase(s, tc_get_obj_by_basename);

    return s;
}
