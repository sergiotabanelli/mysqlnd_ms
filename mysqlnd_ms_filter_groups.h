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
#ifndef MYSQLND_MS_FILTER_GROUPS_H
#define MYSQLND_MS_FILTER_GROUPS_H
struct mysqlnd_ms_lb_strategies;

enum_func_status mysqlnd_ms_choose_connection_groups(MYSQLND_CONN_DATA * conn, void * f_data,
		const char * connect_host, char ** query, size_t * query_len,
		zend_llist * master_list, zend_llist * slave_list,
		zend_llist * selected_masters, zend_llist * selected_slaves,
		struct mysqlnd_ms_lb_strategies * stgy, MYSQLND_ERROR_INFO * error_info TSRMLS_DC);

MYSQLND_MS_FILTER_DATA * mysqlnd_ms_groups_filter_ctor(struct st_mysqlnd_ms_config_json_entry * section,
													zend_llist * master_connections, zend_llist * slave_connections,
													MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC);

#endif	/* MYSQLND_MS_FILTER_GROUPS_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
