/* Buzztard
 * Copyright (C) 2006 Buzztard team <buzztard-devel@lists.sf.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "m-bt-core.h"

//-- globals

//-- fixtures

static void suite_setup(void) {
  bt_core_setup();
}

static void suite_teardown(void) {
  bt_core_teardown();
}


//-- tests

// test init with wrong arg usage
BT_START_TEST(test_btcore_init0  ) {
  /* arrange */
  gchar *test_argv[] = { "check_buzzard", "--bt-version=5" };
  gchar **test_argvptr = test_argv;
  gint test_argc=G_N_ELEMENTS(test_argv);

  /* act */
  bt_init(&test_argc,&test_argvptr);
  
  /* assert */
  mark_point();
}
BT_END_TEST

// test init with nonsense args
BT_START_TEST(test_btcore_init1) {
  /* arrange */
  gchar *test_argv[] = { "check_buzzard", "--bt-non-sense" };
  gchar **test_argvptr = test_argv;
  gint test_argc=G_N_ELEMENTS(test_argv);
  
  /* arrange */
  GOptionContext *ctx = g_option_context_new(NULL);
  bt_init_add_option_groups(ctx);
  g_option_context_set_ignore_unknown_options(ctx, TRUE);

  /* act & assert */
  fail_unless(g_option_context_parse(ctx, &test_argc, &test_argvptr, NULL));
  ck_assert_int_eq(test_argc, 2);

  /* cleanup */
  g_option_context_free(ctx);
}
BT_END_TEST

TCase *bt_core_test_case(void) {
  TCase *tc = tcase_create("BtCoreTests");

  tcase_add_test(tc,test_btcore_init0);
  tcase_add_test(tc,test_btcore_init1);
  tcase_add_unchecked_fixture(tc,suite_setup, suite_teardown);
  return(tc);
}
