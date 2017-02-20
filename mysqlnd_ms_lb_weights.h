/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2008 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Andrey Hristov <andrey@php.net>                              |
  |         Ulf Wendel <uw@php.net>                                      |
  +----------------------------------------------------------------------+
*/

/* $Id: $ */
#ifndef MYSQLND_MS_LB_WEIGHTS_H
#define MYSQLND_MS_LB_WEIGHTS_H
struct st_mysqlnd_ms_config_json_entry;

void mysqlnd_ms_filter_lb_weigth_dtor(void * pDest);
void mysqlnd_ms_filter_ctor_load_weights_config(HashTable * lb_weights_list, const char * filter_name, struct st_mysqlnd_ms_config_json_entry * section, zend_llist * master_connections, zend_llist * slave_connections, MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC);
enum_func_status mysqlnd_ms_populate_weights_sort_list(HashTable * lb_weights_list, zend_llist * lb_sort_list, const zend_llist * const server_list TSRMLS_DC);
int mysqlnd_ms_sort_weights_context_list(const zend_llist_element ** el1, const zend_llist_element ** el2 TSRMLS_DC);

void mysqlnd_ms_weight_list_init(zend_llist * wl TSRMLS_DC);
void mysqlnd_ms_weight_list_sort(zend_llist * wl TSRMLS_DC);
void mysqlnd_ms_weight_list_deinit(zend_llist * wl TSRMLS_DC);

#endif	/* MYSQLND_MS_LB_WEIGHTS_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
