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

/* $Id: mysqlnd_ms.h 311510 2011-05-27 19:36:39Z andrey $ */
#ifndef MYSQLND_MS_FILTER_QOS_H
#define MYSQLND_MS_FILTER_QOS_H
struct mysqlnd_ms_lb_strategies;

enum_func_status mysqlnd_ms_choose_connection_qos(MYSQLND_CONN_DATA * conn, void * f_data,
		const char * connect_host, char ** query, size_t * query_len, zend_bool * free_query,
		zend_llist * master_list, zend_llist * slave_list,
		zend_llist * selected_masters, zend_llist * selected_slaves,
		struct mysqlnd_ms_lb_strategies * stgy, MYSQLND_ERROR_INFO * error_info TSRMLS_DC);

MYSQLND_MS_FILTER_DATA * mysqlnd_ms_qos_filter_ctor(struct st_mysqlnd_ms_config_json_entry * section,
													zend_llist * master_connections, zend_llist * slave_connections,
													MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC);
#if PHP_VERSION_ID > 50399
enum_func_status mysqlnd_ms_section_filters_set_gtid_qos(MYSQLND_CONN_DATA * conn, const char * gtid, size_t gtid_len TSRMLS_DC);
enum_func_status mysqlnd_ms_section_filters_is_gtid_qos(MYSQLND_CONN_DATA * conn TSRMLS_DC);

enum_func_status mysqlnd_ms_section_filters_prepend_qos(MYSQLND * proxy_conn,
		enum mysqlnd_ms_filter_qos_consistency consistency,
		enum mysqlnd_ms_filter_qos_option option,
		MYSQLND_MS_FILTER_QOS_OPTION_DATA * option_data TSRMLS_DC);
#endif

#endif	/* MYSQLND_MS_FILTER_QOS_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
