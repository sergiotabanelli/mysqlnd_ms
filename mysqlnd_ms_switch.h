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

/* $Id: mysqlnd_ms_enum_n_def.h 311091 2011-05-16 15:42:48Z andrey $ */
#ifndef MYSQLND_MS_SWITCH_H
#define MYSQLND_MS_SWITCH_H

struct mysqlnd_ms_lb_strategies;
struct st_mysqlnd_ms_config_json_entry;

// BEGIN HACK
#ifndef MYSQLND_HAS_INJECTION_FEATURE
enum_func_status
mysqlnd_ms_trx_inject(MYSQLND_CONN_DATA * connection, MYSQLND_MS_CONN_DATA * conn_data TSRMLS_DC);
char *
mysqlnd_ms_get_last_gtid_aux(MYSQLND_CONN_DATA * connection TSRMLS_DC);
#endif
zend_bool
mysqlnd_ms_query_is_injectable_query(const char * query, size_t query_len, zend_bool *forced TSRMLS_DC);
enum mysqlnd_ms_filter_qos_consistency
mysqlnd_ms_query_which_qos(const char * query, size_t query_len, zend_bool * forced TSRMLS_DC);
zval *
mysqlnd_ms_get_php_session(TSRMLS_D);
char *
mysqlnd_ms_str_replace(const char *orig, const char *rep, const char *with, zend_bool persistent TSRMLS_DC);
// END HACK

PHP_MYSQLND_MS_API enum enum_which_server mysqlnd_ms_query_is_select(const char * query, size_t query_len, zend_bool * forced TSRMLS_DC);

zend_llist * mysqlnd_ms_load_section_filters(struct st_mysqlnd_ms_config_json_entry * section, MYSQLND_ERROR_INFO * error_info,
											 zend_llist * master_connections, zend_llist * slave_connections,
											 zend_bool persistent TSRMLS_DC);

void mysqlnd_ms_lb_strategy_setup(struct mysqlnd_ms_lb_strategies * strategies, struct st_mysqlnd_ms_config_json_entry * the_section, MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC);

MYSQLND_CONN_DATA * mysqlnd_ms_pick_server_ex(MYSQLND_CONN_DATA * conn,
											  char ** query,
											  size_t * query_len,
											  zend_bool * free_query,
											  zend_bool * switched_servers TSRMLS_DC);

void mysqlnd_ms_get_fingerprint(smart_str * context, zend_llist * list TSRMLS_DC);
void mysqlnd_ms_get_fingerprint_connection(smart_str * context, MYSQLND_MS_LIST_DATA ** d TSRMLS_DC);

enum_func_status
mysqlnd_ms_select_servers_all(zend_llist * master_list, zend_llist * slave_list,
							  zend_llist * selected_masters, zend_llist * selected_slaves TSRMLS_DC);

MYSQLND_CONN_DATA * mysqlnd_ms_pick_first_master_or_slave(const MYSQLND_CONN_DATA * const conn TSRMLS_DC);
#endif	/* MYSQLND_MS_SWITCH_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
