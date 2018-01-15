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
  |         Johannes Schlueter <johannes@php.net>                        |
  +----------------------------------------------------------------------+
*/

/* $Id: $ */

#ifndef MYSQLND_MS_XA_H
#define MYSQLND_MS_XA_H

/* TODO - may include a bit too much ... */
#include "php.h"
#include "ext/standard/info.h"
#include "ext/mysqlnd/mysqlnd.h"
#include "ext/mysqlnd/mysqlnd_debug.h"
#include "ext/mysqlnd/mysqlnd_priv.h"
#include "mysqlnd_ms_enum_n_def.h"

#ifdef ZTS
#include "TSRM.h"
#endif

#define MYSQLND_MS_XA_ID_RESET(id) \
{ \
	 (id).gtrid = 0; \
	 (id).format_id = 0; \
	 (id).store_id = NULL; \
}

struct st_mysqlnd_ms_config_json_entry;

extern unsigned int mysqlnd_ms_plugin_id;
void mysqlnd_ms_xa_gc_hash_dtor(_ms_hash_zval_type * pDest);



enum_func_status mysqlnd_ms_xa_monitor_begin(MYSQLND_CONN_DATA * conn, MYSQLND_MS_CONN_DATA * conn_data, unsigned int gtrid, unsigned int timeout TSRMLS_DC);
enum_func_status mysqlnd_ms_xa_monitor_direct_commit(MYSQLND_CONN_DATA * conn, MYSQLND_MS_CONN_DATA * conn_data, unsigned int gtrid TSRMLS_DC);
enum_func_status mysqlnd_ms_xa_monitor_direct_rollback(MYSQLND_CONN_DATA * conn, MYSQLND_MS_CONN_DATA * conn_data, unsigned int gtrid TSRMLS_DC);
enum_func_status mysqlnd_ms_xa_gc_one(MYSQLND_CONN_DATA * conn, MYSQLND_MS_CONN_DATA * conn_data, unsigned int gtrid, zend_bool ignore_max_retries TSRMLS_DC);
enum_func_status mysqlnd_ms_xa_gc_all(MYSQLND_CONN_DATA * conn, MYSQLND_MS_CONN_DATA * conn_data, zend_bool ignore_max_retries TSRMLS_DC);

void mysqlnd_ms_xa_state_to_string(enum mysqlnd_ms_xa_state state, _ms_smart_type * str);
void mysqlnd_ms_load_xa_config(struct st_mysqlnd_ms_config_json_entry * main_section, MYSQLND_MS_XA_TRX * xa_trx, MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC);

MYSQLND_MS_XA_TRX * mysqlnd_ms_xa_proxy_conn_init(const char * host, size_t host_len, zend_bool persistent TSRMLS_DC);
void mysqlnd_ms_xa_proxy_conn_free(MYSQLND_MS_CONN_DATA * proxy_conn_data, zend_bool persistent TSRMLS_DC);

enum_func_status mysqlnd_ms_xa_inject_query(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_CONN_DATA * next_conn, zend_bool switched_servers TSRMLS_DC);
enum_func_status mysqlnd_ms_xa_conn_close(MYSQLND_CONN_DATA * proxy_conn TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
