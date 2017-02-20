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

/* $Id: mysqlnd_ms.c 334521 2014-08-07 12:03:10Z uw $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
/* TODO XA: move down */
#include "mysqlnd_ms_xa.h"

#include "php.h"
#include "ext/standard/info.h"
#include "ext/mysqlnd/mysqlnd.h"
#include "ext/mysqlnd/mysqlnd_debug.h"
#include "ext/mysqlnd/mysqlnd_priv.h"
#include "ext/mysqlnd/mysqlnd_charset.h"
#if PHP_VERSION_ID >= 50400
#include "ext/mysqlnd/mysqlnd_ext_plugin.h"
#endif
#ifndef mnd_emalloc
#include "ext/mysqlnd/mysqlnd_alloc.h"
#endif
#ifndef mnd_sprintf
#define mnd_sprintf spprintf
#endif
#include "mysqlnd_ms.h"
#include "mysqlnd_ms_config_json.h"

#include "mysqlnd_ms_enum_n_def.h"
#include "mysqlnd_ms_switch.h"

// BEGIN HACK
#include "mysqlnd_ms_filter_qos.h"
// END HACK
#include "fabric/mysqlnd_fabric.h"

#include "mysqlnd_ms_conn_pool.h"


#if PHP_VERSION_ID < 50400
#define mnd_vsprintf vspprintf
#define mnd_sprintf_free efree
#endif

#if PHP_VERSION_ID < 50400
#define CONN_GET_OPTION(conn, option) (conn)->options.option
#else
#define CONN_GET_OPTION(conn, option) (conn)->options->option
#endif

#if MYSQLND_VERSION_ID >= 50010
#define MS_LOAD_AND_COPY_CONN_HANDLE_METHODS(orig_methods, ms_methods) \
	(orig_methods) = mysqlnd_conn_get_methods(); \
	memcpy(&(ms_methods), (orig_methods), sizeof(struct st_mysqlnd_conn_methods));

#define MS_SET_CONN_HANDLE_METHODS(ms_methods) mysqlnd_conn_set_methods((ms_methods));

#define MS_LOAD_AND_COPY_CONN_DATA_METHODS(orig_methods, ms_methods) \
	(orig_methods) = mysqlnd_conn_data_get_methods(); \
	memcpy(&(ms_methods), (orig_methods), sizeof(struct st_mysqlnd_conn_data_methods));

#define MS_SET_CONN_DATA_METHODS(ms_methods) mysqlnd_conn_data_set_methods((ms_methods));

#define MS_GET_CONN_DATA_FROM_CONN(conn) (conn)->data

struct st_mysqlnd_conn_data_methods * ms_orig_mysqlnd_conn_methods;
static struct st_mysqlnd_conn_methods my_mysqlnd_conn_handle_methods;

struct st_mysqlnd_conn_methods * ms_orig_mysqlnd_conn_handle_methods;
static struct st_mysqlnd_conn_data_methods my_mysqlnd_conn_methods;

#else

#define MS_LOAD_AND_COPY_CONN_HANDLE_METHODS(orig_methods, ms_methods)
#define MS_SET_CONN_HANDLE_METHODS(ms_methods)

#define MS_LOAD_AND_COPY_CONN_DATA_METHODS(orig_methods, ms_methods) \
	(orig_methods) = mysqlnd_conn_get_methods(); \
	memcpy(&(ms_methods), (orig_methods), sizeof(struct st_mysqlnd_conn_methods));

#define MS_SET_CONN_DATA_METHODS(ms_methods) mysqlnd_conn_set_methods((ms_methods));

#define MS_GET_CONN_DATA_FROM_CONN(conn) (conn)

struct st_mysqlnd_conn_methods * ms_orig_mysqlnd_conn_methods;
static struct st_mysqlnd_conn_methods my_mysqlnd_conn_methods;
#endif


struct st_mysqlnd_stmt_methods * ms_orig_mysqlnd_stmt_methods;
static struct st_mysqlnd_stmt_methods my_mysqlnd_stmt_methods;

static void mysqlnd_ms_conn_free_plugin_data(MYSQLND_CONN_DATA * conn TSRMLS_DC);

MYSQLND_STATS * mysqlnd_ms_stats = NULL;


#define CONN_DATA_NOT_SET(conn_data) (!(conn_data) || !*(conn_data) || !(*(conn_data))->initialized || (*(conn_data))->skip_ms_calls)
#define CONN_DATA_TRX_SET(conn_data) ((conn_data) && (*(conn_data)) && (!(*(conn_data))->skip_ms_calls))
// BEGIN HACK
// #define CONN_DATA_TRY_TRX_INJECTION(conn_data, conn) ((CONN_GET_STATE(conn) > CONN_ALLOCED) && ((FALSE == (*(conn_data))->skip_ms_calls)) && ((*(conn_data))->global_trx.on_commit) && (TRUE == (*(conn_data))->global_trx.is_master))
// #define MS_TRX_INJECT(ret, connection, conn_data) \
//	if (PASS == (ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(send_query)((connection), ((*(conn_data))->global_trx.on_commit), ((*(conn_data))->global_trx.on_commit_len) TSRMLS_CC))) \
//		(ret) = MS_CALL_ORIGINAL_CONN_DATA_METHOD(reap_query)((connection) TSRMLS_CC);
#define CONN_DATA_TRY_TRX_INJECTION(conn_data, conn) ((CONN_GET_STATE(conn) > CONN_ALLOCED) && ((FALSE == (*(conn_data))->skip_ms_calls)) && (((*(conn_data))->global_trx.on_commit) || ((*(conn_data))->global_trx.memcached_key)) && (TRUE == (*(conn_data))->global_trx.is_master))
#define MS_TRX_INJECT(ret, connection, conn_data) (ret) =  mysqlnd_ms_trx_inject((connection), (*(conn_data)));

/* {{{ mysqlnd_ms_set_tx */
enum_func_status
mysqlnd_ms_set_tx(MYSQLND_CONN_DATA * conn, zend_bool mode TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	enum_func_status ret = FAIL;
	DBG_ENTER("mysqlnd_ms_set_tx");

	if (FALSE == CONN_DATA_NOT_SET(conn_data)) {
#ifndef MYSQLND_HAS_INJECTION_FEATURE
		if ((TRUE == (*conn_data)->stgy.in_transaction) &&
			(CONN_DATA_TRY_TRX_INJECTION(conn_data, conn)))
		{
			/*
			Implicit commit when begin() ..query().. begin().
			Must inject before second=current begin() call.
			*/
			MS_TRX_INJECT(ret, conn, conn_data);
			MYSQLND_MS_INC_STATISTIC((PASS == ret) ? MS_STAT_GTID_IMPLICIT_COMMIT_SUCCESS :
				MS_STAT_GTID_IMPLICIT_COMMIT_FAILURE);

			if (FAIL == ret) {
				if (TRUE == (*conn_data)->global_trx.report_error) {
					DBG_RETURN(ret);
				}

				ret = PASS;
				SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(conn));
			}
		}
#endif
		ret = PASS;
		(*conn_data)->stgy.in_transaction = TRUE;

		if	((*conn_data)->stgy.trx_stickiness_strategy != TRX_STICKINESS_STRATEGY_DISABLED) {
			/* filter will set this after choosing an initial connection */
			(*conn_data)->stgy.trx_stop_switching = FALSE;
			if (mode) {
				(*conn_data)->stgy.trx_read_only = TRUE;
				DBG_INF("In read only transaction, stop switching.");
			} else {
				(*conn_data)->stgy.trx_read_only = FALSE;
				DBG_INF("In transaction, stop switching.");
			}
		}
	}
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_unset_tx */
enum_func_status
mysqlnd_ms_unset_tx(MYSQLND_CONN_DATA * proxy_conn, zend_bool commit TSRMLS_DC)
{
	MYSQLND_CONN_DATA * conn;
	MYSQLND_MS_CONN_DATA ** conn_data;
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);
	enum_func_status ret = PASS;


	DBG_ENTER("mysqlnd_ms_unset_tx");
	if (proxy_conn_data && *proxy_conn_data && (*proxy_conn_data)->stgy.last_used_conn) {
		conn = (*proxy_conn_data)->stgy.last_used_conn;
		MS_LOAD_CONN_DATA(conn_data, conn);
	} else {
		conn = proxy_conn;
		conn_data = proxy_conn_data;
	}
	DBG_INF_FMT("conn="MYSQLND_LLU_SPEC, conn->thread_id);

	if (CONN_GET_STATE(conn) == CONN_ALLOCED && !CONN_DATA_NOT_SET(conn_data)) {
		/* TODO: what is this good for ? */
		DBG_RETURN(PASS);
	}

	/* Must add query before committing ... */
#ifndef MYSQLND_HAS_INJECTION_FEATURE
	if ((conn_data && *conn_data && TRUE == commit) &&
		((TRUE == (*conn_data)->stgy.in_transaction)) &&
		CONN_DATA_TRY_TRX_INJECTION(conn_data, conn))
	{
		MS_TRX_INJECT(ret, conn, conn_data);
		MYSQLND_MS_INC_STATISTIC((PASS == ret) ? MS_STAT_GTID_COMMIT_SUCCESS : MS_STAT_GTID_COMMIT_FAILURE);

		if (FAIL == ret) {
			if (TRUE == (*conn_data)->global_trx.report_error) {
				DBG_RETURN(ret);
			}

			SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(conn));
		}
	}
#endif

	if (conn_data && *conn_data) {
		if (PASS == ret) {
			/* proxy conn stgy controls the filter */
			(*proxy_conn_data)->stgy.in_transaction = FALSE;
			(*proxy_conn_data)->stgy.trx_stop_switching = FALSE;
			(*proxy_conn_data)->stgy.trx_read_only = FALSE;

			/* clean up actual line as well to be on the safe side */
			(*conn_data)->stgy.in_transaction = FALSE;
			(*conn_data)->stgy.trx_stop_switching = FALSE;
			(*conn_data)->stgy.trx_read_only = FALSE;
		}
	}

	DBG_RETURN(ret);
}
/* }}} */

// END HACK

/* {{{ mysqlnd_ms_client_n_php_error */
void
mysqlnd_ms_client_n_php_error(MYSQLND_ERROR_INFO * error_info,
							  unsigned int client_error_code,
							  const char * const client_error_state,
							  unsigned int php_error_level TSRMLS_DC,
							  const char * const format, ...)
{
	char * error_buf;
	va_list args;
	DBG_ENTER("mysqlnd_ms_client_n_php_error");

	va_start(args, format);
	mnd_vsprintf(&error_buf, 0, format, args);
	va_end(args);

	if (error_info) {
		SET_CLIENT_ERROR((*error_info), client_error_code, client_error_state, error_buf);
	}
	if (php_error_level) {
		php_error_docref(NULL TSRMLS_CC, php_error_level, "%s", error_buf);
	}

	DBG_ERR_FMT("%s", error_buf);

	mnd_sprintf_free(error_buf);

	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_get_scheme_from_list_data */
static int
mysqlnd_ms_get_scheme_from_list_data(MYSQLND_MS_LIST_DATA * el, char ** scheme, zend_bool persistent TSRMLS_DC)
{
	char * tmp = NULL;
	int scheme_len;
	*scheme = NULL;
#ifndef PHP_WIN32
	if (el->host && !strcasecmp("localhost", el->host)) {
		scheme_len = mnd_sprintf(&tmp, 0, "unix://%s", el->socket? el->socket : "/tmp/mysql.sock");
#else
	if (el->host && !strcmp(".", el->host)) {
		scheme_len = mnd_sprintf(&tmp, 0, "pipe://%s", el->socket? el->socket : "\\\\.\\pipe\\MySQL");
#endif
	} else {
		if (!el->port) {
			el->port = 3306;
		}
		scheme_len = mnd_sprintf(&tmp, 0, "tcp://%s:%u", el->host? el->host:"localhost", el->port);
	}
	if (tmp) {
		*scheme = mnd_pestrndup(tmp, scheme_len, persistent);
		efree(tmp); /* allocated by spprintf */
	}
	return scheme_len;
}
/* }}} */


/* {{{ mysqlnd_ms_conn_list_dtor */
void
mysqlnd_ms_conn_list_dtor(void * pDest)
{
	MYSQLND_MS_LIST_DATA * element = pDest? *(MYSQLND_MS_LIST_DATA **) pDest : NULL;
	TSRMLS_FETCH();
	DBG_ENTER("mysqlnd_ms_conn_list_dtor");
	DBG_INF_FMT("conn=%p", element->conn);
	if (!element) {
		DBG_VOID_RETURN;
	}
	if (element->name_from_config) {
		mnd_pefree(element->name_from_config, element->persistent);
		element->name_from_config = NULL;
	}
	if (element->conn) {
		element->conn->m->free_reference(element->conn TSRMLS_CC);
		element->conn = NULL;
	}
	if (element->host) {
		mnd_pefree(element->host, element->persistent);
		element->host = NULL;
	}
	if (element->user) {
		mnd_pefree(element->user, element->persistent);
		element->user = NULL;
	}
	if (element->passwd) {
		mnd_pefree(element->passwd, element->persistent);
		element->passwd = NULL;
	}
	if (element->db) {
		mnd_pefree(element->db, element->persistent);
		element->db = NULL;
	}
	if (element->socket) {
		mnd_pefree(element->socket, element->persistent);
		element->socket = NULL;
	}
	if (element->emulated_scheme) {
		mnd_pefree(element->emulated_scheme, element->persistent);
		element->emulated_scheme = NULL;
	}

	if (element->pool_hash_key.len) {
		smart_str_free(&(element->pool_hash_key));
	}

	mnd_pefree(element, element->persistent);
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_lazy_connect */
enum_func_status
mysqlnd_ms_lazy_connect(MYSQLND_MS_LIST_DATA * element, zend_bool master TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	MYSQLND_CONN_DATA * connection = element->conn;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, connection);
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, (*conn_data)->proxy_conn);

	DBG_ENTER("mysqlnd_ms_lazy_connect");
	/*
		We may get called by the load balancing filters (random, roundrobin) while they setup
		a new connection to be used for running a transaction. If transaction stickiness is
		enabled, the filters will have set all flags to block connection switches and try
		to use the last used connection (the one which we are to open yet) when setting
		up the connection in execute_init_commands(). To prevent this recursion we have to
		skip MS for the connect itself.
	*/
	(*conn_data)->skip_ms_calls = TRUE;
	if ((*proxy_conn_data)->server_charset && !CONN_GET_OPTION(connection, charset_name) &&
		FAIL == (ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(connection, MYSQL_SET_CHARSET_NAME,
																	(*proxy_conn_data)->server_charset->name TSRMLS_CC)))
	{
		mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(connection), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
										MYSQLND_MS_ERROR_PREFIX " Couldn't force charset to '%s'",
										(*proxy_conn_data)->server_charset->name);
	} else {


		ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(connect)(connection, element->host, element->user,
													   element->passwd, element->passwd_len,
													   element->db, element->db_len,
													   element->port, element->socket, element->connect_flags TSRMLS_CC);

	}
	(*conn_data)->skip_ms_calls = FALSE;

	if (PASS == ret) {
		DBG_INF("Connected");

		MYSQLND_MS_INC_STATISTIC(master? MS_STAT_LAZY_CONN_MASTER_SUCCESS:MS_STAT_LAZY_CONN_SLAVE_SUCCESS);
#ifndef MYSQLND_HAS_INJECTION_FEATURE
		/* TODO: without this the global trx id injection logic will fail on recently opened lazy connections */
		if (conn_data && *conn_data) {
			(*conn_data)->initialized = TRUE;
		}
#endif
	} else {
		MYSQLND_MS_INC_STATISTIC(master? MS_STAT_LAZY_CONN_MASTER_FAILURE:MS_STAT_LAZY_CONN_SLAVE_FAILURE);
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_connect_init_global_trx */
static void
mysqlnd_ms_init_connection_global_trx(struct st_mysqlnd_ms_global_trx_injection * new_global_trx,
									  struct st_mysqlnd_ms_global_trx_injection * orig_global_trx,
									  zend_bool is_master, zend_bool persistent TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_init_connection_global_trx");
	if (new_global_trx == orig_global_trx) {
		orig_global_trx->is_master = is_master;
		DBG_VOID_RETURN;
	}

	if (TRUE == is_master) {
		new_global_trx->on_commit_len = orig_global_trx->on_commit_len;
		new_global_trx->on_commit = (orig_global_trx->on_commit) ?
			mnd_pestrndup(orig_global_trx->on_commit, orig_global_trx->on_commit_len, persistent) : NULL;
	} else {
		new_global_trx->on_commit_len = 0;
		new_global_trx->on_commit = NULL;
	}

	new_global_trx->fetch_last_gtid_len = orig_global_trx->fetch_last_gtid_len;
	new_global_trx->fetch_last_gtid = (orig_global_trx->fetch_last_gtid) ?
		mnd_pestrndup(orig_global_trx->fetch_last_gtid, orig_global_trx->fetch_last_gtid_len, persistent) : NULL;

	new_global_trx->check_for_gtid_len = orig_global_trx->check_for_gtid_len;
	new_global_trx->check_for_gtid = (orig_global_trx->check_for_gtid) ?
		mnd_pestrndup(orig_global_trx->check_for_gtid, orig_global_trx->check_for_gtid_len, persistent) : NULL;

	new_global_trx->is_master = is_master;
	new_global_trx->report_error = orig_global_trx->report_error;

	new_global_trx->wait_for_gtid_timeout = orig_global_trx->wait_for_gtid_timeout;
// BEGIN HACK
	new_global_trx->memcached_key_len = orig_global_trx->memcached_key_len;
	new_global_trx->memcached_key = (orig_global_trx->memcached_key) ?
		mnd_pestrndup(orig_global_trx->memcached_key, orig_global_trx->memcached_key_len, persistent) : NULL;
	new_global_trx->memcached_port = orig_global_trx->memcached_port;
	new_global_trx->memcached_port_add_hack = orig_global_trx->memcached_port_add_hack;
#ifndef PHP_WIN32
	new_global_trx->memc = NULL;
#endif
	new_global_trx->gtid_conn_elm = NULL;
	new_global_trx->last_gtid = NULL;
	new_global_trx->last_gtid_len = 0;
// END HACK
	DBG_VOID_RETURN;
}
/* }}} */

//BEGIN HACK
/* {{{ mysqlnd_ms_connect_to_host_aux_elm */
static enum_func_status
mysqlnd_ms_connect_to_host_aux_elm(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_CONN_DATA * conn, const char * name_from_config,
							   zend_bool is_master,
							   const char * host, unsigned int port,
							   MYSQLND_MS_LIST_DATA ** new_element,
							   struct st_mysqlnd_ms_conn_credentials * cred,
							   zend_bool lazy_connections,
							   zend_bool persistent, zend_bool skip_ms_calls TSRMLS_DC)
{
	enum_func_status ret = FAIL;

	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);

	DBG_ENTER("mysqlnd_ms_connect_to_host_aux_elm");
	DBG_INF_FMT("conn:%p host:%s port:%d socket:%s", conn, host, cred->port, cred->socket);

	if (lazy_connections) {
		DBG_INF("Lazy connection");
		ret = PASS;
	} else {
		if ((*proxy_conn_data)->server_charset &&
			FAIL == (ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(conn, MYSQL_SET_CHARSET_NAME,
																		(*proxy_conn_data)->server_charset->name TSRMLS_CC)))
		{
			mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " Couldn't force charset to '%s'", (*proxy_conn_data)->server_charset->name);
		} else {
			ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(connect)(conn, host, cred->user, cred->passwd, cred->passwd_len, cred->db, cred->db_len,
															 cred->port, cred->socket, cred->mysql_flags TSRMLS_CC);
		}

		if (PASS == ret) {
			DBG_INF_FMT("Connection "MYSQLND_LLU_SPEC" established", conn->thread_id);
		}
	}

	if (ret == PASS) {
		(*new_element) = mnd_pecalloc(1, sizeof(MYSQLND_MS_LIST_DATA), persistent);
		(*new_element)->name_from_config = mnd_pestrdup(name_from_config? name_from_config:"", conn->persistent);
#if MYSQLND_VERSION_ID >= 50010
		(*new_element)->conn = conn->m->get_reference(conn TSRMLS_CC);
#else
		(*new_element)->conn = conn;
#endif
		(*new_element)->host = host? mnd_pestrdup(host, persistent) : NULL;
		(*new_element)->persistent = persistent;
		(*new_element)->port = port;

		(*new_element)->user = cred->user? mnd_pestrdup(cred->user, conn->persistent) : NULL;

		(*new_element)->passwd_len = cred->passwd_len;
		(*new_element)->passwd = cred->passwd? mnd_pestrndup(cred->passwd, cred->passwd_len, conn->persistent) : NULL;

		(*new_element)->db_len = cred->db_len;
		(*new_element)->db = cred->db? mnd_pestrndup(cred->db, cred->db_len, conn->persistent) : NULL;

		(*new_element)->connect_flags = cred->mysql_flags;

		(*new_element)->socket = cred->socket? mnd_pestrdup(cred->socket, conn->persistent) : NULL;
		(*new_element)->emulated_scheme_len = mysqlnd_ms_get_scheme_from_list_data((*new_element), &(*new_element)->emulated_scheme,
																					persistent TSRMLS_CC);
/*		(*proxy_conn_data)->pool->init_pool_hash_key((*new_element));
		if (is_master) {
			if (PASS != (*proxy_conn_data)->pool->add_master((*proxy_conn_data)->pool, &(*new_element)->pool_hash_key,
					(*new_element), conn->persistent TSRMLS_CC)) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
					MYSQLND_MS_ERROR_PREFIX " Failed to add master to connection pool");
				ret = FAIL;
			}
		} else {
			if (PASS != (*proxy_conn_data)->pool->add_slave((*proxy_conn_data)->pool, &(*new_element)->pool_hash_key,
					(*new_element), conn->persistent TSRMLS_CC)) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
					MYSQLND_MS_ERROR_PREFIX " Failed to add slave to connection pool");
				ret = FAIL;
			}
		}
*/
		{
			MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
			// initialize for every connection, even for slaves and secondary masters
			if (proxy_conn != conn) {
			// otherwise we will overwrite ourselves
				*conn_data = mnd_pecalloc(1, sizeof(MYSQLND_MS_CONN_DATA), conn->persistent);
				if (!(*conn_data)) {
					MYSQLND_MS_WARN_OOM();
					ret = FAIL;
				}
			}
			if (PASS == ret) {
				(*conn_data)->skip_ms_calls = skip_ms_calls;
				(*conn_data)->proxy_conn = proxy_conn;
#ifndef MYSQLND_HAS_INJECTION_FEATURE
				mysqlnd_ms_init_connection_global_trx(&(*conn_data)->global_trx, &(*proxy_conn_data)->global_trx, is_master, conn->persistent TSRMLS_CC);
#endif
			}
		}
	}
	DBG_INF_FMT("ret=%s", ret == PASS? "PASS":"FAIL");
	DBG_RETURN(ret);
}
/* }}} */
//END HACK

/* {{{ mysqlnd_ms_connect_to_host_aux */
enum_func_status
mysqlnd_ms_connect_to_host_aux(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_CONN_DATA * conn, const char * name_from_config,
							   zend_bool is_master,
							   const char * host, unsigned int port,
							   struct st_mysqlnd_ms_conn_credentials * cred,
							   struct st_mysqlnd_ms_global_trx_injection * global_trx,
							   zend_bool lazy_connections,
							   zend_bool persistent TSRMLS_DC)
{
	// BEGIN HACK
	/*
	enum_func_status ret = FAIL;

	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);

	DBG_ENTER("mysqlnd_ms_connect_to_host_aux");
	DBG_INF_FMT("conn:%p host:%s port:%d socket:%s", conn, host, cred->port, cred->socket);

	if (lazy_connections) {
		DBG_INF("Lazy connection");
		ret = PASS;
	} else {
		if ((*proxy_conn_data)->server_charset &&
			FAIL == (ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(conn, MYSQL_SET_CHARSET_NAME,
																		(*proxy_conn_data)->server_charset->name TSRMLS_CC)))
		{
			mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " Couldn't force charset to '%s'", (*proxy_conn_data)->server_charset->name);
		} else {
			ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(connect)(conn, host, cred->user, cred->passwd, cred->passwd_len, cred->db, cred->db_len,
															 cred->port, cred->socket, cred->mysql_flags TSRMLS_CC);
		}

		if (PASS == ret) {
			DBG_INF_FMT("Connection "MYSQLND_LLU_SPEC" established", conn->thread_id);
		}
	}

	if (ret == PASS) {
		MYSQLND_MS_LIST_DATA * new_element = mnd_pecalloc(1, sizeof(MYSQLND_MS_LIST_DATA), persistent);
		if (!new_element) {
			MYSQLND_MS_WARN_OOM();
			ret = FAIL;
		} else {
			new_element->name_from_config = mnd_pestrdup(name_from_config? name_from_config:"", conn->persistent);
#if MYSQLND_VERSION_ID >= 50010
			new_element->conn = conn->m->get_reference(conn TSRMLS_CC);
#else
			new_element->conn = conn;
#endif
			new_element->host = host? mnd_pestrdup(host, persistent) : NULL;
			new_element->persistent = persistent;
			new_element->port = port;

			new_element->user = cred->user? mnd_pestrdup(cred->user, conn->persistent) : NULL;

			new_element->passwd_len = cred->passwd_len;
			new_element->passwd = cred->passwd? mnd_pestrndup(cred->passwd, cred->passwd_len, conn->persistent) : NULL;

			new_element->db_len = cred->db_len;
			new_element->db = cred->db? mnd_pestrndup(cred->db, cred->db_len, conn->persistent) : NULL;

			new_element->connect_flags = cred->mysql_flags;

			new_element->socket = cred->socket? mnd_pestrdup(cred->socket, conn->persistent) : NULL;
			new_element->emulated_scheme_len = mysqlnd_ms_get_scheme_from_list_data(new_element, &new_element->emulated_scheme,
																						persistent TSRMLS_CC);
			(*proxy_conn_data)->pool->init_pool_hash_key(new_element);
			if (is_master) {
				if (PASS != (*proxy_conn_data)->pool->add_master((*proxy_conn_data)->pool, &new_element->pool_hash_key,
													 new_element, conn->persistent TSRMLS_CC)) {
					mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " Failed to add master to connection pool");
					ret = FAIL;
				}
			} else {
				if (PASS != (*proxy_conn_data)->pool->add_slave((*proxy_conn_data)->pool, &new_element->pool_hash_key,
													new_element, conn->persistent TSRMLS_CC)) {
					mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " Failed to add slave to connection pool");
					ret = FAIL;
				}
			}

			{
				MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
				// initialize for every connection, even for slaves and secondary masters
				if (proxy_conn != conn) {
				// otherwise we will overwrite ourselves
					*conn_data = mnd_pecalloc(1, sizeof(MYSQLND_MS_CONN_DATA), conn->persistent);
					if (!(*conn_data)) {
						MYSQLND_MS_WARN_OOM();
						ret = FAIL;
					}
				}
				if (PASS == ret) {
					(*conn_data)->skip_ms_calls = FALSE;
					(*conn_data)->proxy_conn = proxy_conn;
#ifndef MYSQLND_HAS_INJECTION_FEATURE
					mysqlnd_ms_init_connection_global_trx(&(*conn_data)->global_trx, global_trx, is_master, conn->persistent TSRMLS_CC);
#endif
				}
			}
		}
	} */
	enum_func_status ret = FAIL;
	MYSQLND_MS_LIST_DATA * new_element = NULL;
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);
	DBG_ENTER("mysqlnd_ms_connect_to_host_aux");
	ret = mysqlnd_ms_connect_to_host_aux_elm(proxy_conn, conn, name_from_config, is_master, host, port, &new_element, cred, lazy_connections, persistent, FALSE TSRMLS_CC);
	if (ret == PASS) {
		(*proxy_conn_data)->pool->init_pool_hash_key(new_element);
		if (is_master) {
			if (PASS != (*proxy_conn_data)->pool->add_master((*proxy_conn_data)->pool, &new_element->pool_hash_key,
					new_element, conn->persistent TSRMLS_CC)) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
					MYSQLND_MS_ERROR_PREFIX " Failed to add master to connection pool");
				ret = FAIL;
			}
		} else {
			if (PASS != (*proxy_conn_data)->pool->add_slave((*proxy_conn_data)->pool, &new_element->pool_hash_key,
					new_element, conn->persistent TSRMLS_CC)) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
					MYSQLND_MS_ERROR_PREFIX " Failed to add slave to connection pool");
				ret = FAIL;
			}
		}
	}
#ifndef MYSQLND_HAS_INJECTION_FEATURE
	if (ret == PASS && (is_master || global_trx->memcached_key_len > 0)) {
		MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, new_element->conn);
#ifndef PHP_WIN32
		if (global_trx->memcached_key_len > 0) {
			memcached_st *memc;
			unsigned int mcport = 11211;
			memcached_return_t rc;
			if (global_trx->memcached_port) {
				mcport = global_trx->memcached_port;
			} else if (global_trx->memcached_port_add_hack) {
				mcport = (new_element->port ? new_element->port : 3306) + global_trx->memcached_port_add_hack;
			}
			DBG_INF("Start gtid_memcached creation");
			memc = memcached_create(NULL);
			if (memc) {
				DBG_INF_FMT("Connect to Memcached %s %d", new_element->host, mcport);
				rc = memcached_server_add(memc, new_element->host, mcport);
				if (rc == MEMCACHED_SUCCESS) {
					if (is_master) {
						rc = memcached_add(memc, global_trx->memcached_key, global_trx->memcached_key_len, "0", 1, (time_t)0, (uint32_t)0);
						DBG_INF_FMT("Add Memcached key %s result %d", global_trx->memcached_key, rc);
					}
					(*conn_data)->global_trx.memc = memc;
				} else {
					memcached_free(memc);
					php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Failed gtid memcached connect to host.");
					ret = FAIL;
				}
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Failed gtid memcached create.");
				ret = FAIL;
			}
		}
#endif
		if (global_trx->memcached_key_len == 0 && global_trx->on_commit_len > 0) {
			DBG_INF("Start gtid_conn_elm creation");
			MYSQLND_MS_LIST_DATA * gtid_conn_elm = NULL;
			MYSQLND * gtid_conn_handle = NULL;
			MYSQLND_CONN_DATA * gtid_conn = NULL;
	#if PHP_VERSION_ID < 50600
			gtid_conn_handle = mysqlnd_init(persistent);
	#else
			gtid_conn_handle = mysqlnd_init(proxy_conn->m->get_client_api_capabilities(proxy_conn TSRMLS_CC), persistent);
	#endif
			if (gtid_conn_handle) {
				gtid_conn = MS_GET_CONN_DATA_FROM_CONN(gtid_conn_handle);
			}
			if (!gtid_conn || PASS != mysqlnd_ms_connect_to_host_aux_elm(proxy_conn, gtid_conn, name_from_config,
					is_master, host, port, &gtid_conn_elm, cred, lazy_connections, persistent, TRUE TSRMLS_CC)) {
				DBG_INF("Failed gtid_conn_elm creation");
				ret = FAIL;
			}
			if (gtid_conn_handle) {
				gtid_conn_handle->m->dtor(gtid_conn_handle TSRMLS_CC);
			}
			if (ret == PASS) {
				(*conn_data)->global_trx.gtid_conn_elm = gtid_conn_elm;
			}
		}
	}
#endif
//END HACK
	DBG_INF_FMT("ret=%s", ret == PASS? "PASS":"FAIL");
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_connect_to_host */
static enum_func_status
mysqlnd_ms_connect_to_host(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_CONN_DATA * conn,
						   zend_bool is_master,
						   struct st_mysqlnd_ms_conn_credentials * master_credentials,
						   struct st_mysqlnd_ms_global_trx_injection * master_global_trx,
						   struct st_mysqlnd_ms_config_json_entry * main_section,
						   const char * const subsection_name, size_t subsection_name_len,
						   zend_bool lazy_connections, zend_bool persistent,
						   zend_bool process_all_list_values,
						   unsigned int success_stat, unsigned int fail_stat,
						   MYSQLND_ERROR_INFO * error_info TSRMLS_DC)
{
	zend_bool value_exists = FALSE, is_list_value = FALSE;
	struct st_mysqlnd_ms_config_json_entry * subsection = NULL, * parent_subsection = NULL;
	zend_bool recursive = FALSE;
	unsigned int i = 0;
	unsigned int failures = 0;
	DBG_ENTER("mysqlnd_ms_connect_to_host");
	DBG_INF_FMT("conn:%p", conn);

	if (TRUE == mysqlnd_ms_config_json_sub_section_exists(main_section, subsection_name, subsection_name_len, 0 TSRMLS_CC)) {
		subsection =
			parent_subsection =
				mysqlnd_ms_config_json_sub_section(main_section, subsection_name, subsection_name_len, &value_exists TSRMLS_CC);

		recursive =	(TRUE == mysqlnd_ms_config_json_section_is_list(subsection TSRMLS_CC)
					&&
					TRUE == mysqlnd_ms_config_json_section_is_object_list(subsection TSRMLS_CC));
	} else {
		mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
									  MYSQLND_MS_ERROR_PREFIX " Cannot find %s section in config", subsection_name);
	}
	do {
		struct st_mysqlnd_ms_conn_credentials cred = *master_credentials;
		char * socket_to_use = NULL;
		char * user_to_use = NULL;
		char * pass_to_use = NULL;
		char * db_to_use = NULL;
		char * host = NULL;
		int64_t port, flags;

		char * current_subsection_name = NULL;
		size_t current_subsection_name_len = 0;

		if (recursive) {
			subsection = mysqlnd_ms_config_json_next_sub_section(parent_subsection, &current_subsection_name,
																 &current_subsection_name_len, NULL TSRMLS_CC);
		}
		if (!subsection) {
			break;
		}

		flags = mysqlnd_ms_config_json_int_from_section(subsection, SECT_CONNECT_FLAGS_NAME, sizeof(SECT_CONNECT_FLAGS_NAME)-1, 0,
														&value_exists, &is_list_value TSRMLS_CC);
		if (is_list_value) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Invalid value for "SECT_CONNECT_FLAGS_NAME". Cannot be a list/hash' . Stopping");
			failures++;
		} else if (value_exists) {
			if (flags < 0) {
				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
								MYSQLND_MS_ERROR_PREFIX " Invalid value for "SECT_CONNECT_FLAGS_NAME" '%i' . Stopping", flags);
				failures++;
			} else {
				cred.mysql_flags = flags;
			}
		}

		port = mysqlnd_ms_config_json_int_from_section(subsection, SECT_PORT_NAME, sizeof(SECT_PORT_NAME) - 1, 0,
													   &value_exists, &is_list_value TSRMLS_CC);
		if (is_list_value) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Invalid value for "SECT_PORT_NAME". Cannot be a list/hash' . Stopping");
			failures++;
		} else if (value_exists) {
			if (port < 0 || port > 65535) {
				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
								MYSQLND_MS_ERROR_PREFIX " Invalid value for "SECT_PORT_NAME" '%i' . Stopping", port);
				failures++;
			} else {
				cred.port = port;
			}
		}

		socket_to_use = mysqlnd_ms_config_json_string_from_section(subsection, SECT_SOCKET_NAME, sizeof(SECT_SOCKET_NAME) - 1, 0,
																   &value_exists, &is_list_value TSRMLS_CC);
		if (is_list_value) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Invalid value for "SECT_SOCKET_NAME". Cannot be a list/hash' . Stopping");
			failures++;
		} else if (value_exists) {
			cred.socket = socket_to_use;
		}

		user_to_use = mysqlnd_ms_config_json_string_from_section(subsection, SECT_USER_NAME, sizeof(SECT_USER_NAME) - 1, 0,
																 &value_exists, &is_list_value TSRMLS_CC);
		if (is_list_value) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Invalid value for "SECT_USER_NAME". Cannot be a list/hash' . Stopping");
			failures++;
		} else if (value_exists) {
			cred.user = user_to_use;
		}
		pass_to_use = mysqlnd_ms_config_json_string_from_section(subsection, SECT_PASS_NAME, sizeof(SECT_PASS_NAME) - 1, 0,
																 &value_exists, &is_list_value TSRMLS_CC);
		if (is_list_value) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Invalid value for "SECT_PASS_NAME". Cannot be a list/hash' . Stopping");
			failures++;
		} else if (value_exists) {
			cred.passwd = pass_to_use;
			cred.passwd_len = strlen(cred.passwd);
		}

		db_to_use = mysqlnd_ms_config_json_string_from_section(subsection, SECT_DB_NAME, sizeof(SECT_DB_NAME) - 1, 0,
															   &value_exists, &is_list_value TSRMLS_CC);
		if (is_list_value) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Invalid value for "SECT_DB_NAME". Cannot be a list/hash' . Stopping");
			failures++;
		} else if (value_exists) {
			cred.db = db_to_use;
			cred.db_len = strlen(cred.db);
		}

		host = mysqlnd_ms_config_json_string_from_section(subsection, SECT_HOST_NAME, sizeof(SECT_HOST_NAME) - 1, 0,
														  &value_exists, &is_list_value TSRMLS_CC);
		if (is_list_value) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Invalid value for "SECT_HOST_NAME". Cannot be a list/hash' . Stopping");
			failures++;
		} else if (FALSE == value_exists) {
			DBG_ERR_FMT("Cannot find ["SECT_HOST_NAME"] in [%s] section in config", subsection_name);
			php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR,
							 MYSQLND_MS_ERROR_PREFIX " Cannot find ["SECT_HOST_NAME"] in [%s] section in config", subsection_name);
			SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
							 MYSQLND_MS_ERROR_PREFIX " Cannot find ["SECT_HOST_NAME"] in section in config");
			failures++;
		} else {
#if MYSQLND_VERSION_ID >= 50010
			MYSQLND * tmp_conn_handle = NULL;
			MYSQLND_CONN_DATA * tmp_conn = NULL;
			if (conn && i==0) {
				tmp_conn = conn;
			} else {
#if PHP_VERSION_ID < 50600
				tmp_conn_handle = mysqlnd_init(persistent);
#else
				tmp_conn_handle = mysqlnd_init(proxy_conn->m->get_client_api_capabilities(proxy_conn TSRMLS_CC), persistent);
#endif
				if (tmp_conn_handle) {
					tmp_conn = MS_GET_CONN_DATA_FROM_CONN(tmp_conn_handle);
				}
			}
#else
			MYSQLND * tmp_conn = (conn && i==0)? conn->m->get_reference(conn TSRMLS_CC) : mysqlnd_init(persistent);
#endif
			if (tmp_conn) {
				enum_func_status status =
					mysqlnd_ms_connect_to_host_aux(proxy_conn, tmp_conn, current_subsection_name, is_master, host, cred.port, &cred,
												   master_global_trx, lazy_connections, persistent TSRMLS_CC);
				if (status != PASS) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Cannot connect to %s", host);
					(*error_info) = MYSQLND_MS_ERROR_INFO(tmp_conn);
					failures++;
					/* let's free the handle, if there is one. The underlying object will stay alive */
					MYSQLND_MS_INC_STATISTIC(fail_stat);
#if MYSQLND_VERSION_ID < 50010
					if (tmp_conn != conn) {
						tmp_conn->m->dtor(tmp_conn TSRMLS_CC);
					} else {
						conn->m->free_reference(conn TSRMLS_CC);
					}
#endif
				} else {
					if (!lazy_connections) {
						MYSQLND_MS_INC_STATISTIC(success_stat);
					}
				}
#if MYSQLND_VERSION_ID >= 50010
				if (tmp_conn_handle) {
					tmp_conn_handle->m->dtor(tmp_conn_handle TSRMLS_CC);
				}
#endif

			} else {
				failures++;
				/* Handle OOM!! */
				MYSQLND_MS_INC_STATISTIC(fail_stat);
			}
		}
		i++; /* to pass only the first conn handle */

		if (socket_to_use) {
			mnd_efree(socket_to_use);
		}
		if (user_to_use) {
			mnd_efree(user_to_use);
		}
		if (pass_to_use) {
			mnd_efree(pass_to_use);
		}
		if (db_to_use) {
			mnd_efree(db_to_use);
		}
		if (host) {
			mnd_efree(host);
			host = NULL;
		}
	} while (TRUE == process_all_list_values && TRUE == recursive /* && failures == 0 */ );

	DBG_RETURN(failures==0 ? PASS:FAIL);
}
/* }}} */


/* {{{ mysqlnd_ms_init_trx_to_null */
static void
mysqlnd_ms_init_trx_to_null(struct st_mysqlnd_ms_global_trx_injection * trx TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_init_trx_to_null");

	trx->on_commit = NULL;
	trx->on_commit_len = (size_t)0;
	trx->fetch_last_gtid = NULL;
	trx->fetch_last_gtid_len = (size_t)0;
	trx->check_for_gtid = NULL;
	trx->check_for_gtid_len = (size_t)0;
	trx->wait_for_gtid_timeout = 0;
	trx->is_master = FALSE;
	trx->report_error = TRUE;
	//BEGIN HACK
	trx->memcached_key = NULL;
	trx->memcached_key_len = 0;
	trx->memcached_port = 0;
	trx->memcached_port_add_hack = 0;
#ifndef PHP_WIN32
	trx->memc = NULL;
#endif
	trx->gtid_conn_elm = NULL;
	trx->last_gtid = NULL;
	trx->last_gtid_len = 0;
	//END HACK
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_load_trx_config */
static void
mysqlnd_ms_load_trx_config(struct st_mysqlnd_ms_config_json_entry * main_section,
						   struct st_mysqlnd_ms_global_trx_injection * trx,
						   MYSQLND_CONN_DATA *conn,
						   zend_bool persistent TSRMLS_DC)
{
	zend_bool entry_exists;
	zend_bool entry_is_list;
	struct st_mysqlnd_ms_config_json_entry * g_trx_section;
	DBG_ENTER("mysqlnd_ms_load_trx_config");

	g_trx_section =	mysqlnd_ms_config_json_sub_section(main_section, SECT_G_TRX_NAME, sizeof(SECT_G_TRX_NAME) - 1, &entry_exists TSRMLS_CC);

	if (entry_exists && g_trx_section) {
		char * json_value = NULL;
		size_t json_value_len;
		int64_t json_int;
		// BEGIN HACK
		zval * session = mysqlnd_ms_get_php_session(TSRMLS_C);
		// END HACK
		json_value = mysqlnd_ms_config_json_string_from_section(g_trx_section, SECT_G_TRX_ON_COMMIT, sizeof(SECT_G_TRX_ON_COMMIT) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists && json_value) {
			if (entry_is_list) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
								MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_G_TRX_ON_COMMIT, SECT_G_TRX_NAME);
			} else {
				// BEGIN HACK
				//json_value_len = strlen(json_value);
				//trx->on_commit = mnd_pestrndup(json_value, json_value_len, persistent);
				trx->on_commit = mysqlnd_ms_str_replace(json_value, "#SID", Z_STRVAL_P(session), persistent TSRMLS_CC);
				// END HACK
				trx->on_commit_len = strlen(trx->on_commit);
			}
			mnd_efree(json_value);
		}

		json_value = mysqlnd_ms_config_json_string_from_section(g_trx_section, SECT_G_TRX_FETCH_LAST_GTID, sizeof(SECT_G_TRX_FETCH_LAST_GTID) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists && json_value) {
			if (entry_is_list) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_G_TRX_FETCH_LAST_GTID, SECT_G_TRX_NAME);
			} else {
				// BEGIN HACK
				//json_value_len = strlen(json_value);
				//trx->fetch_last_gtid = mnd_pestrndup(json_value, json_value_len, persistent);
				trx->fetch_last_gtid = mysqlnd_ms_str_replace(json_value, "#SID", Z_STRVAL_P(session), persistent TSRMLS_CC);
				// END HACK
				trx->fetch_last_gtid_len = strlen(trx->fetch_last_gtid);
			}
			mnd_efree(json_value);
		}

		json_value = mysqlnd_ms_config_json_string_from_section(g_trx_section, SECT_G_TRX_CHECK_FOR_GTID, sizeof(SECT_G_TRX_CHECK_FOR_GTID) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists && json_value) {
			if (entry_is_list) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_G_TRX_CHECK_FOR_GTID, SECT_G_TRX_NAME);
			} else {
				// BEGIN HACK
				//json_value_len = strlen(json_value);
				//trx->check_for_gtid = mnd_pestrndup(json_value, json_value_len, persistent);
				trx->check_for_gtid = mysqlnd_ms_str_replace(json_value, "#SID", Z_STRVAL_P(session), persistent TSRMLS_CC);
				// END HACK
				trx->check_for_gtid_len = strlen(trx->check_for_gtid);
			}
			mnd_efree(json_value);
		}
		// BEGIN HACK
		json_value = mysqlnd_ms_config_json_string_from_section(g_trx_section, SECT_G_TRX_MEMCACHED_KEY, sizeof(SECT_G_TRX_MEMCACHED_KEY) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists && json_value) {
			if (entry_is_list) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_G_TRX_MEMCACHED_KEY, SECT_G_TRX_NAME);
			} else {
				trx->memcached_key = mysqlnd_ms_str_replace(json_value, "#SID", Z_STRVAL_P(session), persistent TSRMLS_CC);
				trx->memcached_key_len = strlen(trx->memcached_key);
			}
			mnd_efree(json_value);
		}

		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_MEMCACHED_PORT, sizeof(SECT_G_TRX_MEMCACHED_PORT) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int <= 0) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater than zero", SECT_G_TRX_MEMCACHED_PORT, SECT_G_TRX_NAME);
			} else if (json_int > 65535) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less than 65536", SECT_G_TRX_MEMCACHED_PORT, SECT_G_TRX_NAME);
			} else {
				trx->memcached_port = (unsigned int)json_int;
			}
		}

		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_MEMCACHED_PORT_ADD_HACK, sizeof(SECT_G_TRX_MEMCACHED_PORT_ADD_HACK) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int <= 0) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater than zero", SECT_G_TRX_MEMCACHED_PORT_ADD_HACK, SECT_G_TRX_NAME);
			} else if (json_int > 65535) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less than 65536", SECT_G_TRX_MEMCACHED_PORT_ADD_HACK, SECT_G_TRX_NAME);
			} else {
				trx->memcached_port_add_hack = (unsigned int)json_int;
			}
		}
		// END HACK

		json_value = mysqlnd_ms_config_json_string_from_section(g_trx_section, SECT_G_TRX_REPORT_ERROR, sizeof(SECT_G_TRX_REPORT_ERROR) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists && json_value) {
			trx->report_error = !mysqlnd_ms_config_json_string_is_bool_false(json_value);
			mnd_efree(json_value);
		}

		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_WAIT_FOR_GTID_TIMEOUT, sizeof(SECT_G_TRX_WAIT_FOR_GTID_TIMEOUT) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int < 0) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater or equal than zero", SECT_G_TRX_WAIT_FOR_GTID_TIMEOUT, SECT_G_TRX_NAME);
			} else if (json_int > 65535) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less than 65536", SECT_G_TRX_WAIT_FOR_GTID_TIMEOUT, SECT_G_TRX_NAME);
			} else {
				trx->wait_for_gtid_timeout = (unsigned int)json_int;
			}
		}
		// BEGIN HACK
		zval_ptr_dtor(&session);
		// END HACK
	}
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_connect_load_charset_aux */
static enum_func_status
mysqlnd_ms_connect_load_charset_aux(struct st_mysqlnd_ms_config_json_entry * the_section,
									 const char * const setting_name, const size_t setting_name_len,
									 const MYSQLND_CHARSET ** out_storage,
									 MYSQLND_ERROR_INFO * error_info TSRMLS_DC)
{
	enum_func_status ret = PASS;
	char * charset_name;
	zend_bool value_exists = FALSE;
	const MYSQLND_CHARSET * config_charset = NULL;
	DBG_ENTER("mysqlnd_ms_connect_load_charset_aux");

	charset_name = mysqlnd_ms_config_json_string_from_section(the_section, setting_name, setting_name_len, 0, &value_exists, NULL TSRMLS_CC);
	if (charset_name) {
		DBG_INF_FMT("%s=%s", setting_name, charset_name);
		config_charset = mysqlnd_find_charset_name(charset_name);
		if (!config_charset) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " Erroneous %s [%s]", setting_name, charset_name);
			ret = FAIL;
		}
		mnd_efree(charset_name);
		charset_name = NULL;
	}
	*out_storage = config_charset;
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_connect_load_charset */
static enum_func_status
mysqlnd_ms_connect_load_charset(MYSQLND_MS_CONN_DATA ** conn_data, struct st_mysqlnd_ms_config_json_entry * the_section,
								 MYSQLND_ERROR_INFO * error_info TSRMLS_DC)
{
	enum_func_status ret = FAIL;

	DBG_ENTER("mysqlnd_ms_connect_load_charset");

	ret = mysqlnd_ms_connect_load_charset_aux(the_section, SECT_SERVER_CHARSET_NAME, sizeof(SECT_SERVER_CHARSET_NAME) - 1,
											  &(*conn_data)->server_charset, error_info TSRMLS_CC);
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_init_with_master_slave */
static enum_func_status
mysqlnd_ms_init_without_fabric(struct st_mysqlnd_ms_config_json_entry * the_section, MYSQLND_CONN_DATA * conn, MYSQLND_MS_CONN_DATA *conn_data, const char * host TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	DBG_ENTER("mysqlnd_ms_init_without_fabric");

	zend_bool use_lazy_connections = TRUE;
	/* create master connection */

	SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(conn));
#ifndef MYSQLND_HAS_INJECTION_FEATURE
	mysqlnd_ms_load_trx_config(the_section, &conn_data->global_trx, conn, conn->persistent TSRMLS_CC);
#endif
	{
		char * lazy_connections = mysqlnd_ms_config_json_string_from_section(the_section, LAZY_NAME, sizeof(LAZY_NAME) - 1, 0,
												&use_lazy_connections, NULL TSRMLS_CC);
		/* ignore if lazy_connections ini entry exists or not */
		use_lazy_connections = TRUE;
		if (lazy_connections) {
			/* lazy_connections ini entry exists, disabled? */
			use_lazy_connections = !mysqlnd_ms_config_json_string_is_bool_false(lazy_connections);
			mnd_efree(lazy_connections);
			lazy_connections = NULL;
		}
	}

	if (FAIL == mysqlnd_ms_connect_load_charset(&conn_data, the_section, &MYSQLND_MS_ERROR_INFO(conn) TSRMLS_CC)) {
		DBG_RETURN(FAIL);
	}

	{
		const char * const sects_to_check[] = {MASTER_NAME, SLAVE_NAME};
		unsigned int i = 0;
		for (; i < sizeof(sects_to_check) / sizeof(sects_to_check[0]); ++i) {
			size_t sect_len = strlen(sects_to_check[i]);
			if (FALSE == mysqlnd_ms_config_json_sub_section_exists(the_section, sects_to_check[i], sect_len, 0 TSRMLS_CC)) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
					MYSQLND_MS_ERROR_PREFIX " Section [%s] doesn't exist for host [%s]", sects_to_check[i], host);
			}
		}
	}

	DBG_INF("-------------------- MASTER CONNECTIONS ------------------");
	ret = mysqlnd_ms_connect_to_host(conn, conn,
									 TRUE, &conn_data->cred,
									 &conn_data->global_trx, the_section,
									 MASTER_NAME, sizeof(MASTER_NAME) - 1,
									 use_lazy_connections,
									 conn->persistent, MYSQLND_MS_G(multi_master) /* multimaster*/,
									 MS_STAT_NON_LAZY_CONN_MASTER_SUCCESS,
									 MS_STAT_NON_LAZY_CONN_MASTER_FAILURE,
									 &MYSQLND_MS_ERROR_INFO(conn) TSRMLS_CC);
	if (FAIL == ret || (MYSQLND_MS_ERROR_INFO(conn).error_no)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Error while connecting to the master(s)");
		DBG_RETURN(ret);
	}

	SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(conn));

	DBG_INF("-------------------- SLAVE CONNECTIONS ------------------");
	ret = mysqlnd_ms_connect_to_host(conn, NULL,
									 FALSE, &conn_data->cred,
									 &conn_data->global_trx, the_section,
									 SLAVE_NAME, sizeof(SLAVE_NAME) - 1,
									 use_lazy_connections,
									 conn->persistent, TRUE /* multi*/,
									 MS_STAT_NON_LAZY_CONN_SLAVE_SUCCESS,
									 MS_STAT_NON_LAZY_CONN_SLAVE_FAILURE,
									 &MYSQLND_MS_ERROR_INFO(conn) TSRMLS_CC);

	if (FAIL == ret || (MYSQLND_MS_ERROR_INFO(conn).error_no)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Error while connecting to the slaves");
		DBG_RETURN(ret);
	}
	DBG_INF_FMT("master_list=%p count=%d",
				conn_data->pool->get_active_masters(conn_data->pool TSRMLS_CC),
				zend_llist_count(conn_data->pool->get_active_masters(conn_data->pool TSRMLS_CC)));
	DBG_INF_FMT("slave_list=%p count=%d",
				conn_data->pool->get_active_slaves(conn_data->pool TSRMLS_CC),
				zend_llist_count(conn_data->pool->get_active_slaves(conn_data->pool TSRMLS_CC)));

	conn_data->stgy.filters = mysqlnd_ms_load_section_filters(the_section, &MYSQLND_MS_ERROR_INFO(conn),
																	 conn_data->pool->get_active_masters(conn_data->pool TSRMLS_CC),
																	 conn_data->pool->get_active_slaves(conn_data->pool TSRMLS_CC),
																	 TRUE /* load all config persistently */ TSRMLS_CC);
	if (!conn_data->stgy.filters) {
		DBG_RETURN(FAIL);
	}
	mysqlnd_ms_lb_strategy_setup(&conn_data->stgy, the_section, &MYSQLND_MS_ERROR_INFO(conn), conn->persistent TSRMLS_CC);
	conn_data->fabric = NULL;


	mysqlnd_ms_load_xa_config(the_section, conn_data->xa_trx, &MYSQLND_MS_ERROR_INFO(conn), conn->persistent TSRMLS_CC);

    // BEGIN HACK
	if (conn_data->stgy.gtid_on_connect && mysqlnd_ms_section_filters_is_gtid_qos(conn TSRMLS_CC) == PASS) {
		zend_llist_position	pos;
		MYSQLND_MS_LIST_DATA ** element_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(conn_data->pool->get_active_masters(conn_data->pool TSRMLS_CC), &pos);
		char * gtid;
		DBG_INF("gtid_on_connect and qos consistency found");
		if ((gtid = mysqlnd_ms_get_last_gtid_aux((*element_pp)->conn TSRMLS_CC))) {
			ret = mysqlnd_ms_section_filters_set_gtid_qos(((*element_pp)->conn), gtid, strlen(gtid) TSRMLS_CC);
			efree(gtid);
		}
	}
    // END HACK
	DBG_RETURN(ret);
}
/* }}} */

static enum_func_status
mysqlnd_ms_init_with_fabric(struct st_mysqlnd_ms_config_json_entry * group_section, MYSQLND_CONN_DATA * conn, MYSQLND_MS_CONN_DATA *conn_data TSRMLS_DC)
{
	unsigned int host_entry_counter = 0;
	mysqlnd_fabric *fabric;
	zend_bool value_exists = FALSE, is_list_value = FALSE;

	char *strategy_str;
	enum mysqlnd_fabric_strategy strategy = DUMP;
	struct st_mysqlnd_ms_config_json_entry *hostlist_section = NULL, *host;
	struct st_mysqlnd_ms_config_json_entry *fabric_section = mysqlnd_ms_config_json_sub_section(group_section, "fabric", sizeof("fabric")-1, &value_exists TSRMLS_CC);
	unsigned int timeout = 5; /* TODO: Is this an acceptable default timeout? - We should rather take global stream value */
	zend_bool trx_warn = 0;

	conn_data->fabric = NULL;

	fabric_section = mysqlnd_ms_config_json_sub_section(group_section, SECT_FABRIC_NAME, sizeof(SECT_FABRIC_NAME)-1, &value_exists TSRMLS_CC);
	if (!value_exists) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "MySQL Fabric configuration detected but no Fabric section found. This is a bug, please report. Terminating");
	}

	/* Do we need those checks: will there ever be a direct host/slave config?  Well, given how picky I'm about reporting... */
	if (TRUE == mysqlnd_ms_config_json_sub_section_exists(group_section, MASTER_NAME, sizeof(MASTER_NAME)-1, 0 TSRMLS_CC)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Section [" MASTER_NAME "] exists. Ignored for MySQL Fabric based configuration");
	}
	if (TRUE == mysqlnd_ms_config_json_sub_section_exists(group_section, SLAVE_NAME, sizeof(SLAVE_NAME)-1, 0 TSRMLS_CC)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Section [" SLAVE_NAME "] exists. Ignored for MySQL Fabric based configuration");
	}

	if ((TRUE == mysqlnd_ms_config_json_section_is_list(fabric_section TSRMLS_CC)))
	{
		struct st_mysqlnd_ms_config_json_entry * subsection = NULL;
		/* fabric => array(hosts => array(), timeout => string) */
		do {
			char * current_subsection_name = NULL;
			size_t current_subsection_name_len = 0;

			subsection = mysqlnd_ms_config_json_next_sub_section(fabric_section,
																&current_subsection_name,
																&current_subsection_name_len,
																NULL TSRMLS_CC);
			if (!subsection || !current_subsection_name_len) {
				break;
			}

			if (!strncmp(current_subsection_name, SECT_FABRIC_HOSTS, current_subsection_name_len)) {
				if ((FALSE == mysqlnd_ms_config_json_section_is_list(subsection TSRMLS_CC))) {
					mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " Section [" SECT_FABRIC_HOSTS "] is not a list. This is needed for MySQL Fabric");
				}

				hostlist_section = subsection;
			} else if (!strncmp(current_subsection_name, SECT_FABRIC_TIMEOUT, current_subsection_name_len)) {
				int new_timeout = mysqlnd_ms_config_json_int_from_section(fabric_section, current_subsection_name,
														 current_subsection_name_len, 0,
														 &value_exists, &is_list_value TSRMLS_CC);

				if (value_exists) {
					if ((timeout < 0) || (timeout > 65535)) {
						mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
							E_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Invalid value '%i' for [" SECT_FABRIC_TIMEOUT "]. Stopping", timeout);
					} else {
						timeout = (unsigned int)new_timeout;
					}
				}
			} else if (!strncmp(current_subsection_name, SECT_FABRIC_TRX_BOUNDARY_WARNING, current_subsection_name_len)) {
				char *trx_warn_value;
				trx_warn_value = mysqlnd_ms_config_json_string_from_section(fabric_section, current_subsection_name,
														current_subsection_name_len, 0,
														&value_exists, &is_list_value TSRMLS_CC);
				if (value_exists && trx_warn_value) {
					trx_warn = !mysqlnd_ms_config_json_string_is_bool_false(trx_warn_value);
					mnd_efree(trx_warn_value);
				}
			}

		} while (1);
	}

	strategy_str = mysqlnd_ms_config_json_string_from_section(fabric_section, "strategy", sizeof("strategy")-1, 0, &value_exists, NULL TSRMLS_CC);
	if (value_exists && strategy_str) {
		if (!strcmp(strategy_str, "dump")) {
			strategy = DUMP;
		} else if (!strcmp(strategy_str, "direct")) {
			strategy = DIRECT;
		} else {
			mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " Unknown MySQL Fabric strategy %s selected, falling back to default dump", strategy_str);
		}
		efree(strategy_str);
	}

	if (FAIL == mysqlnd_ms_connect_load_charset(&conn_data, group_section, &MYSQLND_MS_ERROR_INFO(conn) TSRMLS_CC)) {
		return FAIL;
	}

	fabric = mysqlnd_fabric_init(strategy, timeout, trx_warn);
	while (hostlist_section && (host = mysqlnd_ms_config_json_next_sub_section(hostlist_section, NULL, NULL, NULL TSRMLS_CC))) {
		host_entry_counter++;
		char *url = mysqlnd_ms_config_json_string_from_section(host, "url", sizeof("url")-1, 0, NULL, NULL TSRMLS_CC);
		if (!url) {
			/* Fallback for 1.6.0-alpha compatibility */
			char *hostname = mysqlnd_ms_config_json_string_from_section(host, "host", sizeof("host")-1, 0, NULL, NULL TSRMLS_CC);
			int port = mysqlnd_ms_config_json_int_from_section(host, "port", sizeof("port")-1, 0, NULL, NULL TSRMLS_CC);

			if (!hostname) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
					MYSQLND_MS_ERROR_PREFIX " Section [" SECT_FABRIC_HOSTS "] lists contains an entry which has an empty [url] value. This is needed for MySQL Fabric");
				continue;
			}

			spprintf(&url, 0, "http://%s:%d/", hostname, port);
			mysqlnd_fabric_add_rpc_host(fabric, url);
			mnd_efree(hostname);
			efree(url);
		} else {
			mysqlnd_fabric_add_rpc_host(fabric, url);
			mnd_efree(url);
		}
	}

	if (0 == host_entry_counter) {
		mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
			MYSQLND_MS_ERROR_PREFIX " Section [" SECT_FABRIC_HOSTS "] doesn't exist. This is needed for MySQL Fabric");
	}

	conn_data->fabric = fabric;

	conn_data->stgy.filters = mysqlnd_ms_load_section_filters(group_section, &MYSQLND_MS_ERROR_INFO(conn),
																	conn_data->pool->get_active_masters(conn_data->pool TSRMLS_CC),
																	conn_data->pool->get_active_slaves(conn_data->pool TSRMLS_CC),
																	TRUE /* load all config persistently */ TSRMLS_CC);
	if (!conn_data->stgy.filters) {
		return FAIL;
	}
	mysqlnd_ms_lb_strategy_setup(&conn_data->stgy, group_section, &MYSQLND_MS_ERROR_INFO(conn), conn->persistent TSRMLS_CC);

	return SUCCESS;
}

static void mysqlnd_ms_filter_notify_pool_update(MYSQLND_MS_POOL * pool, void * data TSRMLS_DC) {
	DBG_ENTER("mysqlnd_ms_filter_notify_pool_update");
	if (data) {
		MYSQLND_CONN_DATA * conn = (MYSQLND_CONN_DATA *)data;
		MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
		DBG_INF_FMT("conn_data=%p *conn_data=%p", conn_data, conn_data? *conn_data : NULL);
		if (conn_data && *conn_data) {
			struct mysqlnd_ms_lb_strategies * stgy = &(*conn_data)->stgy;
			zend_llist * filters = stgy->filters;
			MYSQLND_MS_FILTER_DATA * filter, ** filter_pp;
			zend_llist_position	pos;

			for (filter_pp = (MYSQLND_MS_FILTER_DATA **) zend_llist_get_first_ex(filters, &pos);
				filter_pp && (filter = *filter_pp);
				filter_pp = (MYSQLND_MS_FILTER_DATA **) zend_llist_get_next_ex(filters, &pos))
			{
				if (filter->filter_conn_pool_replaced) {
					filter->filter_conn_pool_replaced(filter,
													  pool->get_active_masters(pool TSRMLS_CC),
													  pool->get_active_slaves(pool TSRMLS_CC),
													  &MYSQLND_MS_ERROR_INFO(conn), conn->persistent TSRMLS_CC);
				}
			}

			/* After switching from one shard group to another,
			 * there's no valid last used connection */
			stgy->last_used_conn = NULL;

			/* TODO: last used connection */
		}
	}
	DBG_VOID_RETURN;
}


/* {{{ mysqlnd_ms::connect */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, connect)(MYSQLND_CONN_DATA * conn,
									const char * host,
									const char * user,
									const char * passwd,
									unsigned int passwd_len,
									const char * db,
									unsigned int db_len,
									unsigned int port,
									const char * socket,
									unsigned int mysql_flags TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	MYSQLND_MS_CONN_DATA ** conn_data;
	size_t host_len = host? strlen(host) : 0;
	zend_bool section_found;
	zend_bool hotloading = MYSLQND_MS_HOTLOADING;

	DBG_ENTER("mysqlnd_ms::connect");
	if (hotloading) {
		MYSQLND_MS_CONFIG_JSON_LOCK(mysqlnd_ms_json_config);
	}

	section_found = mysqlnd_ms_config_json_section_exists(mysqlnd_ms_json_config, host, host_len, 0, hotloading? FALSE:TRUE TSRMLS_CC);
	if (MYSQLND_MS_G(force_config_usage)) {
		if (MYSQLND_MS_G(config_startup_error)) {
			/* TODO: May bark before a hot loading (disabled) attempt is made.
			Same should be true about force config usage */
			php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR,
									  MYSQLND_MS_ERROR_PREFIX " %s", MYSQLND_MS_G(config_startup_error));
		}

		if (FALSE == section_found) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX
			" Exclusive usage of configuration enforced but did not find the correct INI file section (%s)", host);
			if (hotloading) {
				MYSQLND_MS_CONFIG_JSON_UNLOCK(mysqlnd_ms_json_config);
			}
			SET_CLIENT_ERROR(MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, MYSQLND_MS_ERROR_PREFIX
			" Exclusive usage of configuration enforced but did not find the correct INI file section");
			DBG_RETURN(FAIL);
		}
	} else {
		if (MYSQLND_MS_G(config_startup_error)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,
									MYSQLND_MS_ERROR_PREFIX " %s", MYSQLND_MS_G(config_startup_error));
		}
	}
	mysqlnd_ms_conn_free_plugin_data(conn TSRMLS_CC);

	if (FALSE == section_found) {
		DBG_INF("section not found");
		ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(connect)(conn, host, user, passwd, passwd_len, db, db_len, port, socket, mysql_flags TSRMLS_CC);
	} else {
		struct st_mysqlnd_ms_config_json_entry * the_section;
		zend_bool value_exists = FALSE;
		MS_LOAD_CONN_DATA(conn_data, conn);

		*conn_data = mnd_pecalloc(1, sizeof(MYSQLND_MS_CONN_DATA), conn->persistent);
		if (!(*conn_data)) {
			MYSQLND_MS_WARN_OOM();
			ret = FAIL;
			goto end_connect;
		}

		/* Initialize connection pool */
		(*conn_data)->pool = mysqlnd_ms_pool_ctor((llist_dtor_func_t) mysqlnd_ms_conn_list_dtor, conn->persistent TSRMLS_CC);
		if (!(*conn_data)->pool) {
			MYSQLND_MS_WARN_OOM();
			ret = FALSE;
			goto end_connect;
		}
		/* FIXME could be too early, prior to filter setup may cause issues */
		(*conn_data)->pool->register_replace_listener((*conn_data)->pool, mysqlnd_ms_filter_notify_pool_update, (void *)conn TSRMLS_CC);

		(*conn_data)->cred.user = user? mnd_pestrdup(user, conn->persistent) : NULL;
		(*conn_data)->cred.passwd_len = passwd_len;
		(*conn_data)->cred.passwd = passwd? mnd_pestrndup(passwd, passwd_len, conn->persistent) : NULL;
		(*conn_data)->cred.db_len = db_len;
		(*conn_data)->cred.db = db? mnd_pestrndup(db, db_len, conn->persistent) : NULL;
		(*conn_data)->cred.port = port;
		(*conn_data)->cred.socket = socket? mnd_pestrdup(socket, conn->persistent) : NULL;
		(*conn_data)->cred.mysql_flags = mysql_flags;
#ifndef MYSQLND_HAS_INJECTION_FEATURE
		mysqlnd_ms_init_trx_to_null(&(*conn_data)->global_trx TSRMLS_CC);
#endif
		(*conn_data)->xa_trx = mysqlnd_ms_xa_proxy_conn_init(host, host_len, conn->persistent TSRMLS_CC);
		(*conn_data)->initialized = TRUE;

		if (!hotloading) {
			MYSQLND_MS_CONFIG_JSON_LOCK(mysqlnd_ms_json_config);
		}

		the_section = mysqlnd_ms_config_json_section(mysqlnd_ms_json_config, host, host_len, &value_exists TSRMLS_CC);

		if (mysqlnd_ms_config_json_sub_section_exists(the_section, SECT_FABRIC_NAME, sizeof(SECT_FABRIC_NAME)-1, 0 TSRMLS_CC)) {
			ret = mysqlnd_ms_init_with_fabric(the_section, conn, *conn_data TSRMLS_CC);
		} else {
			ret = mysqlnd_ms_init_without_fabric(the_section, conn, *conn_data, host TSRMLS_CC);
		}

		mysqlnd_ms_config_json_reset_section(the_section, TRUE TSRMLS_CC);

		if (!hotloading) {
			MYSQLND_MS_CONFIG_JSON_UNLOCK(mysqlnd_ms_json_config);
		}

		if (ret == PASS) {
			(*conn_data)->connect_host = host? mnd_pestrdup(host, conn->persistent) : NULL;
		}
	}


	if (hotloading) {
		MYSQLND_MS_CONFIG_JSON_UNLOCK(mysqlnd_ms_json_config);
	}
end_connect:
	DBG_INF_FMT("conn=%llu old_refcount=%u", conn->thread_id, conn->refcount);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_do_send_query(MYSQLND_CONN_DATA * conn, const char * query, size_t query_len, zend_bool pick_server TSRMLS_DC) */
static enum_func_status
mysqlnd_ms_do_send_query(MYSQLND_CONN_DATA * conn, const char * query, size_t query_len, zend_bool pick_server TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	enum_func_status ret = PASS;
	DBG_ENTER("mysqlnd_ms::do_send_query");

	if (CONN_DATA_NOT_SET(conn_data)) {
	} else if (pick_server && (!(*conn_data)->skip_ms_calls)) {
		DBG_INF("Must be async query, blocking and failing");
		if (conn) {
			mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " Asynchronous queries are not supported");
			DBG_RETURN(FAIL);
		}
	}

	ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(send_query)(conn, query, query_len TSRMLS_CC);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ MYSQLND_METHOD(mysqlnd_ms, send_query) */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, send_query)(MYSQLND_CONN_DATA * conn, const char * query, unsigned int query_len TSRMLS_DC)
{
	return mysqlnd_ms_do_send_query(conn, query, query_len, TRUE TSRMLS_CC);
}
/* }}} */


/* {{{ MYSQLND_METHOD(mysqlnd_ms, query) */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, query)(MYSQLND_CONN_DATA * conn, const char * query, unsigned int q_len TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	MYSQLND_CONN_DATA * connection;
	enum_func_status ret = FAIL;
	zend_bool free_query = FALSE, switched_servers = FALSE;
	size_t query_len = q_len;
	uint transient_error_no = 0, transient_error_retries = 0;
#ifdef ALL_SERVER_DISPATCH
	zend_bool use_all = 0;
#endif
	DBG_ENTER("mysqlnd_ms::query");
	DBG_INF_FMT("query=%s", query);

	if (CONN_DATA_NOT_SET(conn_data)) {
		ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(query)(conn, query, q_len TSRMLS_CC);
		DBG_RETURN(ret);
	}

	connection = mysqlnd_ms_pick_server_ex(conn, (char**)&query, &query_len, &free_query, &switched_servers TSRMLS_CC);
	DBG_INF_FMT("Connection %p error_no=%d", connection, connection? (MYSQLND_MS_ERROR_INFO(connection).error_no) : -1);
	/*
	  Beware : error_no is set to 0 in original->query. This, this might be a problem,
	  as we dump a connection from usage till the end of the script.
	  Lazy connections can generate connection failures, thus we need to check for them.
	  If we skip these checks we will get 2014 from original->query.
	*/
	if (!connection || (MYSQLND_MS_ERROR_INFO(connection).error_no)) {
		/* Connect error to be handled by failover logic, not a transient error */
		if (TRUE == free_query) {
			efree((void *)query);
		}
		DBG_RETURN(ret);
	}

	ret = mysqlnd_ms_xa_inject_query(conn, connection, switched_servers TSRMLS_CC);
	if (FAIL == ret) {
		DBG_RETURN(ret);
	}

#ifdef ALL_SERVER_DISPATCH
	if (use_all) {
		MYSQLND_MS_CONN_DATA ** conn_data = (MYSQLND_MS_CONN_DATA **) mysqlnd_plugin_get_plugin_connection_data_data(conn, mysqlnd_ms_plugin_id);
		zend_llist * master_connections = (conn_data && *conn_data)? &(*conn_data)->master_connections : NULL;
		zend_llist * slave_connections = (conn_data && *conn_data)? &(*conn_data)->slave_connections : NULL;

		mysqlnd_ms_query_all(conn, query, query_len, master_connections, slave_connections TSRMLS_CC);
	}
#endif

	DBG_INF_FMT("conn="MYSQLND_LLU_SPEC" query=%s", connection->thread_id, query);

	/* TODO:
	  This is wrong - we should do injection *after* successful query.
	  Doing it here - for now - to avoid trouble with result sets.
	  How expensive is the load?
	*/
	MS_LOAD_CONN_DATA(conn_data, connection);
//BEGIN HACK
//#ifndef MYSQLND_HAS_INJECTION_FEATURE
//	if (CONN_DATA_TRX_SET(conn_data) && CONN_DATA_TRY_TRX_INJECTION(conn_data, connection))
//	{
		/*
		 We don't need to care about transient errors.
		 GTID injection makes only sense for MySQL Replication but transient errors
		 are a MySQL Cluster specific thing. As using GTID injection with MySQL Cluster
		 is pointless, we don't care about transient errors as part of GTID injection.
		*/
//		if (FALSE == (*conn_data)->stgy.in_transaction) {
			/* autocommit mode */
/*			MS_TRX_INJECT(ret, connection, conn_data);
			MYSQLND_MS_INC_STATISTIC((PASS == ret) ? MS_STAT_GTID_AUTOCOMMIT_SUCCESS :
				MS_STAT_GTID_AUTOCOMMIT_FAILURE);

			if (FAIL == ret) {
				if (TRUE == (*conn_data)->global_trx.report_error) {
					if (TRUE == free_query) {
						efree((void *)query);
					}
					DBG_RETURN(ret);
				}
				SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(connection));
			}
		}
	}
#endif
*/
//END HACK
    do {
		if ((PASS == (ret = mysqlnd_ms_do_send_query(connection, query, query_len, FALSE TSRMLS_CC))) &&
			(PASS == (ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(reap_query)(connection TSRMLS_CC))))
		{
			if (connection->last_query_type == QUERY_UPSERT && (MYSQLND_MS_UPSERT_STATUS(connection).affected_rows)) {
				MYSQLND_INC_CONN_STATISTIC_W_VALUE(connection->stats, STAT_ROWS_AFFECTED_NORMAL, MYSQLND_MS_UPSERT_STATUS(connection).affected_rows);
			}
		}
		/* Is there a transient error that we shall ignore? */
		MS_CHECK_CONN_FOR_TRANSIENT_ERROR(connection, conn_data, transient_error_no);
		if (transient_error_no) {
			DBG_INF_FMT("Transient error "MYSQLND_LLU_SPEC, transient_error_no);
			transient_error_retries++;
			if (transient_error_retries <= (*conn_data)->stgy.transient_error_max_retries) {
				MYSQLND_MS_INC_STATISTIC(MS_STAT_TRANSIENT_ERROR_RETRIES);
				DBG_INF_FMT("Retry attempt %i/%i. Sleeping for "MYSQLND_LLU_SPEC" ms and retrying.",
							transient_error_retries,
							(*conn_data)->stgy.transient_error_max_retries,
							(*conn_data)->stgy.transient_error_usleep_before_retry);
#if HAVE_USLEEP
				usleep((*conn_data)->stgy.transient_error_usleep_before_retry);
#endif
			} else {
				DBG_INF("No more transient error retries allowed");
				break;
			}
		}
	} while (transient_error_no);
//BEGIN HACK
#ifndef MYSQLND_HAS_INJECTION_FEATURE
	if (ret == PASS && CONN_DATA_TRX_SET(conn_data) && CONN_DATA_TRY_TRX_INJECTION(conn_data, connection))
	{
		{
			zend_bool forced = FALSE;
			(*conn_data)->stgy.injectable_query = ((*conn_data)->stgy.in_transaction ? FALSE : mysqlnd_ms_query_is_injectable_query(query, query_len, &forced TSRMLS_CC));
			if (forced) {
				(*conn_data)->stgy.stop_inject = (*conn_data)->stgy.injectable_query ? FALSE : TRUE;
			}
		}
		if (FALSE == (*conn_data)->stgy.in_transaction && FALSE == (*conn_data)->stgy.stop_inject && (*conn_data)->stgy.injectable_query == TRUE) {
			enum_func_status retx = FAIL;
			/* autocommit mode */
			MS_TRX_INJECT(retx, connection, conn_data);
			MYSQLND_MS_INC_STATISTIC((PASS == retx) ? MS_STAT_GTID_AUTOCOMMIT_SUCCESS :
				MS_STAT_GTID_AUTOCOMMIT_FAILURE);

			if (FAIL == retx) {
				if (TRUE == (*conn_data)->global_trx.report_error) {
					if (TRUE == free_query) {
						efree((void *)query);
					}
					DBG_RETURN(retx);
				}
				SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(connection));
			}
		}
	}
#endif
//END HACK
	if (TRUE == free_query) {
		efree((void *)query);
	}
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms::use_result */
static MYSQLND_RES *
#if PHP_VERSION_ID < 50600
MYSQLND_METHOD(mysqlnd_ms, use_result)(MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
#else
MYSQLND_METHOD(mysqlnd_ms, use_result)(MYSQLND_CONN_DATA * const proxy_conn, const unsigned int flags TSRMLS_DC)
#endif
{
	MYSQLND_RES * result;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;
	DBG_ENTER("mysqlnd_ms::use_result");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, conn->thread_id);
#if PHP_VERSION_ID < 50600
	result = MS_CALL_ORIGINAL_CONN_DATA_METHOD(use_result)(conn TSRMLS_CC);
#else
	result = MS_CALL_ORIGINAL_CONN_DATA_METHOD(use_result)(conn, flags TSRMLS_CC);
#endif
	DBG_RETURN(result);
}
/* }}} */


/* {{{ mysqlnd_ms::store_result */
static MYSQLND_RES *
#if PHP_VERSION_ID < 50600
MYSQLND_METHOD(mysqlnd_ms, store_result)(MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
#else
MYSQLND_METHOD(mysqlnd_ms, store_result)(MYSQLND_CONN_DATA * const proxy_conn, const unsigned int flags TSRMLS_DC)
#endif
{
	MYSQLND_RES * result;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;
	DBG_ENTER("mysqlnd_ms::store_result");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, conn->thread_id);
#if PHP_VERSION_ID < 50600
	result = MS_CALL_ORIGINAL_CONN_DATA_METHOD(store_result)(conn TSRMLS_CC);
#else
	result = MS_CALL_ORIGINAL_CONN_DATA_METHOD(store_result)(conn, flags TSRMLS_CC);
#endif
	DBG_RETURN(result);
}
/* }}} */


/* {{{ mysqlnd_ms_conn_free_plugin_data */
static void
mysqlnd_ms_conn_free_plugin_data(MYSQLND_CONN_DATA * conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(data_pp, conn);
	DBG_ENTER("mysqlnd_ms_conn_free_plugin_data");

	DBG_INF_FMT("data_pp=%p", data_pp);
	if (data_pp && *data_pp) {
		if ((*data_pp)->connect_host) {
			mnd_pefree((*data_pp)->connect_host, conn->persistent);
			(*data_pp)->connect_host = NULL;
		}

		if ((*data_pp)->cred.user) {
			mnd_pefree((*data_pp)->cred.user, conn->persistent);
			(*data_pp)->cred.user = NULL;
		}

		if ((*data_pp)->cred.passwd) {
			mnd_pefree((*data_pp)->cred.passwd, conn->persistent);
			(*data_pp)->cred.passwd = NULL;
		}
		(*data_pp)->cred.passwd_len = 0;

		if ((*data_pp)->cred.db) {
			mnd_pefree((*data_pp)->cred.db, conn->persistent);
			(*data_pp)->cred.db = NULL;
		}
		(*data_pp)->cred.db_len = 0;

		if ((*data_pp)->cred.socket) {
			mnd_pefree((*data_pp)->cred.socket, conn->persistent);
			(*data_pp)->cred.socket = NULL;
		}

		(*data_pp)->cred.port = 0;
		(*data_pp)->cred.mysql_flags = 0;
#ifndef MYSQLND_HAS_INJECTION_FEATURE
		if ((*data_pp)->global_trx.on_commit) {
			mnd_pefree((*data_pp)->global_trx.on_commit, conn->persistent);
			(*data_pp)->global_trx.on_commit = NULL;
			(*data_pp)->global_trx.on_commit_len = 0;
		}
		if ((*data_pp)->global_trx.fetch_last_gtid) {
			mnd_pefree((*data_pp)->global_trx.fetch_last_gtid, conn->persistent);
			(*data_pp)->global_trx.fetch_last_gtid = NULL;
			(*data_pp)->global_trx.fetch_last_gtid_len = (size_t)0;
		}
		if ((*data_pp)->global_trx.check_for_gtid) {
			mnd_pefree((*data_pp)->global_trx.check_for_gtid, conn->persistent);
			(*data_pp)->global_trx.check_for_gtid = NULL;
			(*data_pp)->global_trx.check_for_gtid_len = (size_t)0;
		}
//BEGIN HACK
#ifndef PHP_WIN32
		if ((*data_pp)->global_trx.memc) {
			memcached_free((*data_pp)->global_trx.memc);
			(*data_pp)->global_trx.memc = NULL;
		}
#endif
		if ((*data_pp)->global_trx.gtid_conn_elm) {
			mysqlnd_ms_conn_list_dtor((void *) &((*data_pp)->global_trx.gtid_conn_elm));
			(*data_pp)->global_trx.gtid_conn_elm = NULL;
		}
		if ((*data_pp)->global_trx.memcached_key) {
			mnd_pefree((*data_pp)->global_trx.memcached_key, conn->persistent);
			(*data_pp)->global_trx.memcached_key = NULL;
			(*data_pp)->global_trx.memcached_key_len = (size_t)0;
		}
		if ((*data_pp)->global_trx.last_gtid) {
			mnd_pefree((*data_pp)->global_trx.last_gtid, conn->persistent);
			(*data_pp)->global_trx.last_gtid = NULL;
			(*data_pp)->global_trx.last_gtid_len = (size_t)0;
		}
//END HACK
#endif
		DBG_INF_FMT("cleaning the section filters");
		if ((*data_pp)->stgy.filters) {
			DBG_INF_FMT("%d loaded filters", zend_llist_count((*data_pp)->stgy.filters));
			zend_llist_clean((*data_pp)->stgy.filters);
			mnd_pefree((*data_pp)->stgy.filters, TRUE /* all filters were loaded persistently */);
			(*data_pp)->stgy.filters = NULL;
		}

		if ((*data_pp)->stgy.failover_remember_failed) {
			zend_hash_destroy(&((*data_pp)->stgy.failed_hosts));
		}

		if ((*data_pp)->stgy.trx_begin_name) {
			mnd_pefree((*data_pp)->stgy.trx_begin_name, conn->persistent);
			(*data_pp)->stgy.trx_begin_name = NULL;
		}

		if (TRANSIENT_ERROR_STRATEGY_ON == (*data_pp)->stgy.transient_error_strategy) {
			zend_llist_clean(&((*data_pp)->stgy.transient_error_codes));
		}

		if ((*data_pp)->fabric) {
			mysqlnd_fabric_free((*data_pp)->fabric);
		}

		if ((*data_pp)->xa_trx) {
			mysqlnd_ms_xa_proxy_conn_free((*data_pp), conn->persistent TSRMLS_CC);
		}

		/* XA is using the pool */
		if ((*data_pp)->pool) {
			(*data_pp)->pool->dtor((*data_pp)->pool TSRMLS_CC);
		}


		mnd_pefree(*data_pp, conn->persistent);
		*data_pp = NULL;
	}
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms::dtor */
static void
MYSQLND_METHOD_PRIVATE(mysqlnd_ms, dtor)(MYSQLND_CONN_DATA * conn TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms::dtor");

	mysqlnd_ms_conn_free_plugin_data(conn TSRMLS_CC);
	MS_CALL_ORIGINAL_CONN_DATA_METHOD(dtor)(conn TSRMLS_CC);
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms::escape_string */
static ulong
MYSQLND_METHOD(mysqlnd_ms, escape_string)(MYSQLND_CONN_DATA * const proxy_conn, char * newstr, const char * escapestr, size_t escapestr_len TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;
	ulong ret = 0;

	DBG_ENTER("mysqlnd_ms::escape_string");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, conn->thread_id);
	if (CONN_GET_STATE(conn) > CONN_ALLOCED && CONN_GET_STATE(conn) != CONN_QUIT_SENT) {
		if (conn_data && *conn_data) {
			(*conn_data)->skip_ms_calls = TRUE;
		}
		ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(escape_string)(conn, newstr, escapestr, escapestr_len TSRMLS_CC);
		if (conn_data && *conn_data) {
			(*conn_data)->skip_ms_calls = FALSE;
		}
	} else if (CONN_GET_STATE(conn) == CONN_ALLOCED && ((*conn_data)->server_charset || CONN_GET_OPTION(conn, charset_name))) {
		const MYSQLND_CHARSET * orig_charset = conn->charset;

		conn->charset = (*conn_data)->server_charset;
		/* must not happen but put sentinels */
		if (!(*conn_data)->server_charset && CONN_GET_OPTION(conn, charset_name)) {
			conn->charset = mysqlnd_find_charset_name(CONN_GET_OPTION(conn, charset_name));
		}

		if (conn_data && *conn_data) {
			(*conn_data)->skip_ms_calls = TRUE;
		}
		ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(escape_string)(conn, newstr, escapestr, escapestr_len TSRMLS_CC);
		if (conn_data && *conn_data) {
			(*conn_data)->skip_ms_calls = FALSE;
		}
		conn->charset = orig_charset;
	} else {
		/* broken connection or no "server_charset" setting */
		newstr[0] = '\0';
		mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_COMMANDS_OUT_OF_SYNC, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
			MYSQLND_MS_ERROR_PREFIX " string escaping doesn't work without established connection. Possible solution is to add "
			SECT_SERVER_CHARSET_NAME" to your configuration");
	}
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms::change_user */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, change_user)(MYSQLND_CONN_DATA * const proxy_conn,
										  const char *user,
										  const char *passwd,
										  const char *db,
										  zend_bool silent
#if PHP_VERSION_ID >= 50399
										  ,size_t passwd_len
#endif
										  TSRMLS_DC)
{
	enum_func_status ret = PASS, last = PASS;
	uint transient_error_no = 0, transient_error_retries = 0;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);

	DBG_ENTER("mysqlnd_ms::change_user");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, proxy_conn->thread_id);
	if (CONN_DATA_NOT_SET(conn_data)) {
#if PHP_VERSION_ID >= 50399
		DBG_RETURN(MS_CALL_ORIGINAL_CONN_DATA_METHOD(change_user)(proxy_conn, user, passwd, db, silent, passwd_len TSRMLS_CC));
#else
		DBG_RETURN(MS_CALL_ORIGINAL_CONN_DATA_METHOD(change_user)(proxy_conn, user, passwd, db, silent TSRMLS_CC));
#endif
	} else {
		MYSQLND_MS_LIST_DATA * el;

		ret = (*conn_data)->pool->dispatch_change_user((*conn_data)->pool,
													MYSQLND_METHOD(mysqlnd_ms, change_user),
													user, passwd, db, silent
#if PHP_VERSION_ID >= 50399
													, passwd_len
#endif
													TSRMLS_CC);

		BEGIN_ITERATE_OVER_SERVER_LISTS(el, (*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC), (*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC));
		{
			MS_DECLARE_AND_LOAD_CONN_DATA(el_conn_data, el->conn);
			if (el_conn_data && *el_conn_data) {
				(*el_conn_data)->skip_ms_calls = TRUE;
			}
			if (CONN_GET_STATE(el->conn) == CONN_ALLOCED) {
				/* lazy connection */
				if (el->user) {
					mnd_pefree(el->user, el->persistent);
				}
				el->user = user? mnd_pestrdup(user, el->persistent) : NULL;

				if (el->passwd) {
					mnd_pefree(el->passwd, el->persistent);
				}
#if PHP_VERSION_ID >= 50399
				el->passwd_len = passwd_len;
#else
				el->passwd_len = strlen(passwd);
#endif
				el->passwd = passwd? mnd_pestrndup(passwd, el->passwd_len, el->persistent) : NULL;
				if (el->db) {
					mnd_pefree(el->db, el->persistent);
				}
				el->db_len = strlen(db);
				el->db = db? mnd_pestrndup(db, el->db_len, el->persistent) : NULL;
			} else {

				/* reset retry counter for every connection */
				transient_error_retries = 0;
				do {

					last = MS_CALL_ORIGINAL_CONN_DATA_METHOD(change_user)(el->conn, user, passwd, db, silent
#if PHP_VERSION_ID >= 50399
																		,passwd_len
#endif
																	TSRMLS_CC);
					if (PASS == last) {
						break;
					}
					MS_CHECK_CONN_FOR_TRANSIENT_ERROR(el->conn, conn_data, transient_error_no);
					if (transient_error_no) {
						DBG_INF_FMT("Transient error "MYSQLND_LLU_SPEC, transient_error_no);
						transient_error_retries++;
						if (transient_error_retries <= (*conn_data)->stgy.transient_error_max_retries) {
							MYSQLND_MS_INC_STATISTIC(MS_STAT_TRANSIENT_ERROR_RETRIES);
							DBG_INF_FMT("Retry attempt %i/%i. Sleeping for "MYSQLND_LLU_SPEC" ms and retrying.",
								transient_error_retries,
								(*conn_data)->stgy.transient_error_max_retries,
								(*conn_data)->stgy.transient_error_usleep_before_retry);
#if HAVE_USLEEP
							usleep((*conn_data)->stgy.transient_error_usleep_before_retry);
#endif
						} else {
							DBG_INF("No more transient error retries allowed");
							ret = FAIL;
							break;
						}
					} else {
						/* an error that is not considered transient */
						ret = FAIL;
						break;
					}

				} while (transient_error_no);

			}
			if (el_conn_data && *el_conn_data) {
				(*el_conn_data)->skip_ms_calls = FALSE;
			}
		}
		END_ITERATE_OVER_SERVER_LISTS;
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms::ping */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, ping)(MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	enum_func_status ret;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;
	DBG_ENTER("mysqlnd_ms::ping");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, conn->thread_id);
	ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(ping)(conn TSRMLS_CC);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms::kill */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, kill)(MYSQLND_CONN_DATA * proxy_conn, unsigned int pid TSRMLS_DC)
{
	enum_func_status ret;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;
	DBG_ENTER("mysqlnd_ms::kill");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, conn->thread_id);
	ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(kill_connection)(conn, pid TSRMLS_CC);
	DBG_RETURN(ret);
}
/* }}} */


#if 0
/* {{{ mysqlnd_ms::get_errors */
static zval *
MYSQLND_METHOD(mysqlnd_ms, get_errors)(MYSQLND_CONN_DATA * const proxy_conn, const char * const db, unsigned int db_len TSRMLS_DC)
{
	zval * ret = NULL;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);

	DBG_ENTER("mysqlnd_ms::get_errors");
	if (conn_data && *conn_data) {
		MYSQLND_MS_LIST_DATA * el;
		array_init(ret);
		BEGIN_ITERATE_OVER_SERVER_LISTS(el, (*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC), (*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC));
		{
			MS_DECLARE_AND_LOAD_CONN_DATA(el_conn_data, el->conn);
			zval * row = NULL;
			char * scheme;
			size_t scheme_len;

			if (el_conn_data && *el_conn_data) {
				(*el_conn_data)->skip_ms_calls = TRUE;
			}

			if (CONN_GET_STATE(el->conn) == CONN_ALLOCED) {
				scheme = el->emulated_scheme;
				scheme_len = el->emulated_scheme_len;
			} else {
				scheme = el->conn->scheme;
				scheme_len = el->conn->scheme_len;
			}
			array_init(row);
			add_assoc_long_ex(row, "errno", sizeof("errno") - 1, MS_CALL_ORIGINAL_CONN_DATA_METHOD(get_error_no)(el->conn TSRMLS_CC));
			{
				const char * err = MS_CALL_ORIGINAL_CONN_DATA_METHOD(get_error_str)(el->conn TSRMLS_CC);
				add_assoc_stringl_ex(row, "error", sizeof("error") - 1, (char*) err, strlen(err), 1 /*dup*/);
			}
			{
				const char * sqlstate = MS_CALL_ORIGINAL_CONN_DATA_METHOD(get_sqlstate)(el->conn TSRMLS_CC);
				add_assoc_stringl_ex(row, "sqlstate", sizeof("sqlstate") - 1, (char*) sqlstate, strlen(sqlstate), 1 /*dup*/);
			}
			add_assoc_zval_ex(ret, scheme, scheme_len, row);

			if (el_conn_data && *el_conn_data) {
				(*el_conn_data)->skip_ms_calls = FALSE;
			}
		}
		END_ITERATE_OVER_SERVER_LISTS;
	}

	DBG_RETURN(ret);
}
/* }}} */
#endif


/* {{{ mysqlnd_ms::select_db */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, select_db)(MYSQLND_CONN_DATA * const proxy_conn, const char * const db, unsigned int db_len TSRMLS_DC)
{
	enum_func_status last = PASS, ret = PASS;
	uint transient_error_no = 0, transient_error_retries = 0;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);

	DBG_ENTER("mysqlnd_ms::select_db");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, proxy_conn->thread_id);
	if (CONN_DATA_NOT_SET(conn_data)) {
		DBG_RETURN(MS_CALL_ORIGINAL_CONN_DATA_METHOD(select_db)(proxy_conn, db, db_len TSRMLS_CC));
	} else {
		MYSQLND_MS_LIST_DATA * el;

		ret = (*conn_data)->pool->dispatch_select_db((*conn_data)->pool, MYSQLND_METHOD(mysqlnd_ms, select_db), db, db_len TSRMLS_CC);

		BEGIN_ITERATE_OVER_SERVER_LISTS(el, (*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC), (*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC));
		{
			if (CONN_GET_STATE(el->conn) > CONN_ALLOCED && CONN_GET_STATE(el->conn) != CONN_QUIT_SENT) {
				MS_DECLARE_AND_LOAD_CONN_DATA(el_conn_data, el->conn);

				if (el_conn_data && *el_conn_data) {
					(*el_conn_data)->skip_ms_calls = TRUE;
				}

				/* reset retry counter for every connection */
				transient_error_retries = 0;
				do {
					last = MS_CALL_ORIGINAL_CONN_DATA_METHOD(select_db)(el->conn, db, db_len TSRMLS_CC);

					if (PASS == last) {
						break;
					}
					MS_CHECK_CONN_FOR_TRANSIENT_ERROR(el->conn, conn_data, transient_error_no);
					if (transient_error_no) {
						DBG_INF_FMT("Transient error "MYSQLND_LLU_SPEC, transient_error_no);
						transient_error_retries++;
						if (transient_error_retries <= (*conn_data)->stgy.transient_error_max_retries) {
							MYSQLND_MS_INC_STATISTIC(MS_STAT_TRANSIENT_ERROR_RETRIES);
							DBG_INF_FMT("Retry attempt %i/%i. Sleeping for "MYSQLND_LLU_SPEC" ms and retrying.",
								transient_error_retries,
								(*conn_data)->stgy.transient_error_max_retries,
								(*conn_data)->stgy.transient_error_usleep_before_retry);
#if HAVE_USLEEP
							usleep((*conn_data)->stgy.transient_error_usleep_before_retry);
#endif
						} else {
							DBG_INF("No more transient error retries allowed");
							ret = FAIL;
							break;
						}
					} else {
						/* an error that is not considered transient */
						ret = FAIL;
						break;
					}

				} while (transient_error_no);

				if (el_conn_data && *el_conn_data) {
					(*el_conn_data)->skip_ms_calls = FALSE;
				}
			} else if (CONN_GET_STATE(el->conn) == CONN_ALLOCED) {
				/* lazy connection */
				if (el->db) {
					mnd_pefree(el->db, el->persistent);
				}
				el->db_len = db_len;
				el->db = db? mnd_pestrndup(db, db_len, el->persistent) : NULL;
			}

		}
		END_ITERATE_OVER_SERVER_LISTS;
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms::set_charset */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, set_charset)(MYSQLND_CONN_DATA * const proxy_conn, const char * const csname TSRMLS_DC)
{
	enum_func_status last = PASS, ret = PASS;
	uint transient_error_no = 0, transient_error_retries = 0;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);

	DBG_ENTER("mysqlnd_ms::set_charset");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, proxy_conn->thread_id);
	if (CONN_DATA_NOT_SET(conn_data)) {
		DBG_RETURN(MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_charset)(proxy_conn, csname TSRMLS_CC));
	} else {
		MYSQLND_MS_LIST_DATA * el;

		ret = (*conn_data)->pool->dispatch_set_charset((*conn_data)->pool, MYSQLND_METHOD(mysqlnd_ms, set_charset), csname TSRMLS_CC);

		BEGIN_ITERATE_OVER_SERVER_LISTS(el, (*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC), (*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC));
		{
			enum_mysqlnd_connection_state state = CONN_GET_STATE(el->conn);
			if (state != CONN_QUIT_SENT) {
				MS_DECLARE_AND_LOAD_CONN_DATA(el_conn_data, el->conn);

				if (el_conn_data && *el_conn_data) {
					(*el_conn_data)->skip_ms_calls = TRUE;
				}

				transient_error_retries = 0;
				do {
					if (state == CONN_ALLOCED) {
						last = MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(el->conn, MYSQL_SET_CHARSET_NAME, csname TSRMLS_CC);
						if (PASS == last) {
							(*el_conn_data)->server_charset = mysqlnd_find_charset_name(CONN_GET_OPTION(el->conn, charset_name));
							if (!(*el_conn_data)->server_charset) {
								mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(el->conn), CR_UNKNOWN_ERROR,
															UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
															MYSQLND_MS_ERROR_PREFIX " unknown to the connector charset '%s'. Please report to the developers",
															CONN_GET_OPTION(el->conn, charset_name));
							}
						}
					} else {
						last = MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_charset)(el->conn, csname TSRMLS_CC);
					}

					if (PASS == last) {
						break;
					}
					MS_CHECK_CONN_FOR_TRANSIENT_ERROR(el->conn, conn_data, transient_error_no);
					if (transient_error_no) {
						DBG_INF_FMT("Transient error "MYSQLND_LLU_SPEC, transient_error_no);
						transient_error_retries++;
						if (transient_error_retries <= (*conn_data)->stgy.transient_error_max_retries) {
								MYSQLND_MS_INC_STATISTIC(MS_STAT_TRANSIENT_ERROR_RETRIES);
								DBG_INF_FMT("Retry attempt %i/%i. Sleeping for "MYSQLND_LLU_SPEC" ms and retrying.",
									transient_error_retries,
									(*conn_data)->stgy.transient_error_max_retries,
									(*conn_data)->stgy.transient_error_usleep_before_retry);
#if HAVE_USLEEP
							usleep((*conn_data)->stgy.transient_error_usleep_before_retry);
#endif
						} else {
							DBG_INF("No more transient error retries allowed");
							ret = FAIL;
							break;
						}
					} else {
						/* an error that is not considered transient */
						ret = FAIL;
						break;
					}

				} while (transient_error_no);

				if (el_conn_data && *el_conn_data) {
					(*el_conn_data)->skip_ms_calls = FALSE;
				}
			}


		}
		END_ITERATE_OVER_SERVER_LISTS;
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms::set_server_option */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, set_server_option)(MYSQLND_CONN_DATA * const proxy_conn, enum_mysqlnd_server_option option TSRMLS_DC)
{
	enum_func_status last = PASS, ret = PASS;
	uint transient_error_no = 0, transient_error_retries = 0;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);

	DBG_ENTER("mysqlnd_ms::set_server_option");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, proxy_conn->thread_id);
	if (CONN_DATA_NOT_SET(conn_data)) {
		DBG_RETURN(MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_server_option)(proxy_conn, option TSRMLS_CC));
	} else {
		MYSQLND_MS_LIST_DATA * el;

		ret = (*conn_data)->pool->dispatch_set_server_option((*conn_data)->pool, MYSQLND_METHOD(mysqlnd_ms, set_server_option), option TSRMLS_CC);

		BEGIN_ITERATE_OVER_SERVER_LISTS(el, (*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC), (*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC));
		{

			if (CONN_GET_STATE(el->conn) > CONN_ALLOCED && CONN_GET_STATE(el->conn) != CONN_QUIT_SENT) {
				MS_DECLARE_AND_LOAD_CONN_DATA(el_conn_data, el->conn);

				if (el_conn_data && *el_conn_data) {
					(*el_conn_data)->skip_ms_calls = TRUE;
				}

				transient_error_retries = 0;
				do {
					last = MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_server_option)(el->conn, option TSRMLS_CC);
					if (PASS == last) {
						break;
					}
					MS_CHECK_CONN_FOR_TRANSIENT_ERROR(el->conn, conn_data, transient_error_no);
					if (transient_error_no) {
						DBG_INF_FMT("Transient error "MYSQLND_LLU_SPEC, transient_error_no);
						transient_error_retries++;
						if (transient_error_retries <= (*conn_data)->stgy.transient_error_max_retries) {
							MYSQLND_MS_INC_STATISTIC(MS_STAT_TRANSIENT_ERROR_RETRIES);
							DBG_INF_FMT("Retry attempt %i/%i. Sleeping for "MYSQLND_LLU_SPEC" ms and retrying.",
								transient_error_retries,
								(*conn_data)->stgy.transient_error_max_retries,
								(*conn_data)->stgy.transient_error_usleep_before_retry);
#if HAVE_USLEEP
							usleep((*conn_data)->stgy.transient_error_usleep_before_retry);
#endif
						} else {
							DBG_INF("No more transient error retries allowed");
							ret = FAIL;
							break;
						}
					} else {
						/* an error that is not considered transient */
						ret = FAIL;
						break;
					}

				} while (transient_error_no);

				if (el_conn_data && *el_conn_data) {
					(*el_conn_data)->skip_ms_calls = FALSE;
				}
			}

		}
		END_ITERATE_OVER_SERVER_LISTS;
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms::set_client_option */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, set_client_option)(MYSQLND_CONN_DATA * const proxy_conn, enum_mysqlnd_option option, const char * const value TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);

	DBG_ENTER("mysqlnd_ms::set_client_option");
	if (CONN_DATA_NOT_SET(conn_data)) {
		DBG_RETURN(MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(proxy_conn, option, value TSRMLS_CC));
	} else {
		MYSQLND_MS_LIST_DATA * el;

		ret = (*conn_data)->pool->dispatch_set_client_option((*conn_data)->pool, MYSQLND_METHOD(mysqlnd_ms, set_client_option), option, value TSRMLS_CC);

		BEGIN_ITERATE_OVER_SERVER_LISTS(el, (*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC), (*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC));
		{
			MS_DECLARE_AND_LOAD_CONN_DATA(el_conn_data, el->conn);

			if (el_conn_data && *el_conn_data) {
				(*el_conn_data)->skip_ms_calls = TRUE;
			}

			/* This is buffered and replies come later, thus we cannot add transient error loop */
			if (PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(el->conn, option, value TSRMLS_CC)) {
				ret = FAIL;
			}

			if (el_conn_data && *el_conn_data) {
				(*el_conn_data)->skip_ms_calls = FALSE;
			}
		}
		END_ITERATE_OVER_SERVER_LISTS;
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms::next_result */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, next_result)(MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	enum_func_status ret;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	DBG_ENTER("mysqlnd_ms::next_result");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, conn->thread_id);
	ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(next_result)(conn TSRMLS_CC);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms::more_results */
static zend_bool
MYSQLND_METHOD(mysqlnd_ms, more_results)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	zend_bool ret;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	DBG_ENTER("mysqlnd_ms::more_results");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, conn->thread_id);
	ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(more_results)(conn TSRMLS_CC);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms::errno */
static unsigned int
MYSQLND_METHOD(mysqlnd_ms, error_no)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	return MYSQLND_MS_ERROR_INFO(conn).error_no;
}
/* }}} */


/* {{{ mysqlnd_ms::error */
static const char *
MYSQLND_METHOD(mysqlnd_ms, error)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	return MYSQLND_MS_ERROR_INFO(conn).error;
}
/* }}} */


/* {{{ mysqlnd_conn::sqlstate */
static const char *
MYSQLND_METHOD(mysqlnd_ms, sqlstate)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	return (MYSQLND_MS_ERROR_INFO(conn).sqlstate[0]) ? (MYSQLND_MS_ERROR_INFO(conn).sqlstate): MYSQLND_SQLSTATE_NULL;
}
/* }}} */


/* {{{ mysqlnd_ms::field_count */
static unsigned int
MYSQLND_METHOD(mysqlnd_ms, field_count)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	return conn->field_count;
}
/* }}} */


/* {{{ mysqlnd_conn::thread_id */
static uint64_t
MYSQLND_METHOD(mysqlnd_ms, thread_id)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	return conn->thread_id;
}
/* }}} */


/* {{{ mysqlnd_ms::insert_id */
static uint64_t
MYSQLND_METHOD(mysqlnd_ms, insert_id)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	return MYSQLND_MS_UPSERT_STATUS(conn).last_insert_id;
}
/* }}} */


/* {{{ mysqlnd_ms::affected_rows */
static uint64_t
MYSQLND_METHOD(mysqlnd_ms, affected_rows)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	return MYSQLND_MS_UPSERT_STATUS(conn).affected_rows;
}
/* }}} */


/* {{{ mysqlnd_ms::warning_count */
static unsigned int
MYSQLND_METHOD(mysqlnd_ms, warning_count)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	return MYSQLND_MS_UPSERT_STATUS(conn).warning_count;
}
/* }}} */


/* {{{ mysqlnd_ms::info */
static const char *
MYSQLND_METHOD(mysqlnd_ms, info)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	return conn->last_message;
}
/* }}} */


#if MYSQLND_VERSION_ID >= 50009
/* {{{ MYSQLND_METHOD(mysqlnd_ms, set_autocommit) */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, set_autocommit)(MYSQLND_CONN_DATA * proxy_conn, unsigned int mode TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);

	DBG_ENTER("mysqlnd_ms::set_autocommit");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, proxy_conn->thread_id);
	if (CONN_DATA_NOT_SET(conn_data)) {
		DBG_INF("Using original");
		ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_autocommit)(proxy_conn, mode TSRMLS_CC);
		DBG_RETURN(ret);
	} else {
		MYSQLND_MS_LIST_DATA * el;

		ret = (*conn_data)->pool->dispatch_set_autocommit((*conn_data)->pool, MYSQLND_METHOD(mysqlnd_ms, set_autocommit), mode TSRMLS_CC);

#ifndef MYSQLND_HAS_INJECTION_FEATURE
		if (((TRUE == (*conn_data)->stgy.in_transaction) && mode) &&
			CONN_DATA_TRY_TRX_INJECTION(conn_data, proxy_conn))
		{
			/*
			Implicit commit when autocommit(false) ..query().. autocommit(true).
			Must inject before second=current autocommit() call.
			*/
			MS_TRX_INJECT(ret, proxy_conn, conn_data);
			MYSQLND_MS_INC_STATISTIC((PASS == ret) ? MS_STAT_GTID_IMPLICIT_COMMIT_SUCCESS :
				MS_STAT_GTID_IMPLICIT_COMMIT_FAILURE);

			if (FAIL == ret) {
				if (TRUE == (*conn_data)->global_trx.report_error) {
					DBG_RETURN(ret);
				}

				ret = PASS;
				SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(proxy_conn));
			}
		}
#endif

		/* No need to handle transient errors
		 set_autocommit() calls query() if connected. query() is covered.
		 If client is not connected to server, then set_autocimmit() calls
		 set_client_option() which is buffered. We cannot handle buffered
		 connect options through the transient error retry logic. in sum:
		 set_autocommit() handled transient errors if connected, otherwise not.
		*/
		BEGIN_ITERATE_OVER_SERVER_LISTS(el, (*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC), (*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC));
		{
			if (CONN_GET_STATE(el->conn) != CONN_QUIT_SENT) {
				MYSQLND_MS_CONN_DATA ** el_conn_data = (MYSQLND_MS_CONN_DATA **) mysqlnd_plugin_get_plugin_connection_data_data(el->conn, mysqlnd_ms_plugin_id);
				if (el_conn_data && *el_conn_data) {
					(*el_conn_data)->skip_ms_calls = TRUE;
				}
				if (CONN_GET_STATE(el->conn) == CONN_ALLOCED) {
					ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(el->conn, MYSQL_INIT_COMMAND,
																		  (mode) ? "SET AUTOCOMMIT=1":"SET AUTOCOMMIT=0"
																		  TSRMLS_CC);
				} else if (PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_autocommit)(el->conn, mode TSRMLS_CC)) {
					ret = FAIL;
				}
				if (el_conn_data && *el_conn_data) {
					(*el_conn_data)->skip_ms_calls = FALSE;
				}
			}
		}
		END_ITERATE_OVER_SERVER_LISTS;

		if (PASS == ret) {
			/*
			If toggling autocommit fails for any line, we do not touch the plugins transaction
			detection status. The user is supposed to handle the failed autocommit mode switching
			function call.
			*/
			BEGIN_ITERATE_OVER_SERVER_LISTS(el, (*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC), (*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC));
			{
				if (CONN_GET_STATE(el->conn) != CONN_QUIT_SENT) {
					MYSQLND_MS_CONN_DATA ** el_conn_data = (MYSQLND_MS_CONN_DATA **) mysqlnd_plugin_get_plugin_connection_data_data(el->conn, mysqlnd_ms_plugin_id);
					if (el_conn_data && *el_conn_data) {
						if (mode) {
							(*el_conn_data)->stgy.in_transaction = FALSE;
							(*el_conn_data)->stgy.trx_stop_switching = FALSE;
							(*el_conn_data)->stgy.trx_read_only = FALSE;
							(*el_conn_data)->stgy.trx_autocommit_off = FALSE;
						} else {
							(*el_conn_data)->stgy.in_transaction = TRUE;
							(*el_conn_data)->stgy.trx_autocommit_off = TRUE;
						}
					}
				}
			}
			END_ITERATE_OVER_SERVER_LISTS;

			if ((!(*conn_data)->stgy.last_used_conn) && (CONN_GET_STATE(proxy_conn) == CONN_ALLOCED)) {
				/*
				 * Lazy connection and no connection has been opened yet.
				 * If this was a regular/non-MS connection and there would have been no error during
				 * set_autocommit(), then it any previous error code had been unset. Now, this is
				 * like a regular connection and there was no error, hence we must unset */
				SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(proxy_conn));
			}
		}
	}

	MYSQLND_MS_INC_STATISTIC(mode? MS_STAT_TRX_AUTOCOMMIT_ON:MS_STAT_TRX_AUTOCOMMIT_OFF);
	DBG_INF_FMT("in_transaction = %d", (*conn_data)->stgy.in_transaction);
	DBG_INF_FMT("trx_stop_switching = %d", (*conn_data)->stgy.trx_stop_switching);

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_tx_commit_or_rollback */
#if MYSQLND_VERSION_ID >= 50011
static enum_func_status
mysqlnd_ms_tx_commit_or_rollback(MYSQLND_CONN_DATA * proxy_conn, zend_bool commit, const unsigned int flags, const char * const name TSRMLS_DC)
#else
static enum_func_status
mysqlnd_ms_tx_commit_or_rollback(MYSQLND_CONN_DATA * proxy_conn, zend_bool commit TSRMLS_DC)
#endif
{
	MYSQLND_CONN_DATA * conn;
	MYSQLND_MS_CONN_DATA ** conn_data;
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);
	enum_func_status ret = PASS;


	DBG_ENTER("mysqlnd_ms_tx_commit_or_rollback");
	if (proxy_conn_data && *proxy_conn_data && (*proxy_conn_data)->stgy.last_used_conn) {
		conn = (*proxy_conn_data)->stgy.last_used_conn;
		MS_LOAD_CONN_DATA(conn_data, conn);
	} else {
		conn = proxy_conn;
		conn_data = proxy_conn_data;
	}
	DBG_INF_FMT("conn="MYSQLND_LLU_SPEC, conn->thread_id);

	if (CONN_GET_STATE(conn) == CONN_ALLOCED && !CONN_DATA_NOT_SET(conn_data)) {
		/* TODO: what is this good for ? */
		DBG_RETURN(PASS);
	}
//BEGIN HACK
/*
	// Must add query before committing ...
#ifndef MYSQLND_HAS_INJECTION_FEATURE
	if ((conn_data && *conn_data && TRUE == commit) &&
		((TRUE == (*conn_data)->stgy.in_transaction)) &&
		CONN_DATA_TRY_TRX_INJECTION(conn_data, conn))
	{
		MS_TRX_INJECT(ret, conn, conn_data);
		MYSQLND_MS_INC_STATISTIC((PASS == ret) ? MS_STAT_GTID_COMMIT_SUCCESS : MS_STAT_GTID_COMMIT_FAILURE);

		if (FAIL == ret) {
			if (TRUE == (*conn_data)->global_trx.report_error) {
				DBG_RETURN(ret);
			}

			SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(conn));
		}
	}
#endif
*/
//END HACK
	if (conn_data && *conn_data) {
		(*conn_data)->skip_ms_calls = TRUE;
	}
	/* TODO: the recursive rattle tail is terrible, we should optimize and call query() directly */
	/* Transient error retry covered as long as query() is used */
#if MYSQLND_VERSION_ID >= 50011
		ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(tx_commit_or_rollback)(conn, commit, flags, name TSRMLS_CC);
#else
		ret = commit? MS_CALL_ORIGINAL_CONN_DATA_METHOD(tx_commit)(conn TSRMLS_CC) :
					MS_CALL_ORIGINAL_CONN_DATA_METHOD(tx_rollback)(conn TSRMLS_CC);
#endif

	if (conn_data && *conn_data) {
		(*conn_data)->skip_ms_calls = FALSE;
		//BEGIN HACK
#ifndef MYSQLND_HAS_INJECTION_FEATURE
		if (TRUE == commit && TRUE == (*conn_data)->stgy.in_transaction && CONN_DATA_TRY_TRX_INJECTION(conn_data, conn))
		{
			MS_TRX_INJECT(ret, conn, conn_data);
			MYSQLND_MS_INC_STATISTIC((PASS == ret) ? MS_STAT_GTID_COMMIT_SUCCESS : MS_STAT_GTID_COMMIT_FAILURE);

			if (FAIL == ret) {
				if (TRUE == (*conn_data)->global_trx.report_error) {
					DBG_RETURN(ret);
				}

				SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(conn));
			}
		}
#endif
		//END HACK

		if (PASS == ret) {
			if (FALSE == (*conn_data)->stgy.trx_autocommit_off)  {
				/* autocommit(true) -> in_trx = 0; begin() -> in_trx = 1; commit() or rollback() -> in_trx = 0; */

				/* proxy conn stgy controls the filter */
				(*proxy_conn_data)->stgy.in_transaction = FALSE;
				(*proxy_conn_data)->stgy.trx_stop_switching = FALSE;
				(*proxy_conn_data)->stgy.trx_read_only = FALSE;

				/* clean up actual line as well to be on the safe side */
				(*conn_data)->stgy.in_transaction = FALSE;
				(*conn_data)->stgy.trx_stop_switching = FALSE;
				(*conn_data)->stgy.trx_read_only = FALSE;
			} else if ((*conn_data)->stgy.trx_autocommit_off && (*proxy_conn_data)->stgy.in_transaction) {
				/* autocommit(false); query()|begin() -> pick server; query() -> keep server; commit()|rollback/() -> keep server; query()|begin() --> pick new server */
				(*proxy_conn_data)->stgy.trx_stop_switching = FALSE;
				(*conn_data)->stgy.trx_stop_switching = FALSE;
			}
		}
	}

	DBG_RETURN(ret);
}
/* }}} */


#if MYSQLND_VERSION_ID >= 50011
/* {{{ MYSQLND_METHOD(mysqlnd_ms, tx_commit_or_rollback) */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, tx_commit_or_rollback)(MYSQLND_CONN_DATA * conn, const zend_bool commit, const unsigned int flags, const char * const name TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	DBG_ENTER("mysqlnd_ms::tx_commit_or_rollback");
	ret = mysqlnd_ms_tx_commit_or_rollback(conn, commit, flags, name TSRMLS_CC);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ MYSQLND_METHOD(mysqlnd_ms, tx_begin) */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, tx_begin)(MYSQLND_CONN_DATA * conn, const unsigned int mode, const char * const name TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	enum_func_status ret = FAIL;
	DBG_ENTER("mysqlnd_ms::tx_begin");

	/*
	 HACK KLUDGE

	 The way MS has been designed an tx_begin(read_only_trx) don't go together.
	 If a user starts a transaction using tx_begin() it is yet unknown where
	 this transaction will be executed. This is even true if the sticky
	 flag has been used because at best the sticky flag reduces the list
	 of servers down to two for a typical setup: one master, one slave.

	 tx_begin() hints us whether there will be a ready only transaction or not.
	 Ideally, we keep a read only transaction on a MySQL Replication slave. It is
	 likely the slave has been marked read only and performance is better on a slave.
	 Also, its always great to use slaves whenever possible.

	 Now, if read only and if there are slaves and if those slaves qualify for the
	 execution of the transaction (e.g. their lag is within limits set), then we
	 may end up on a slave. Otherwise we may end up on a master.

	 We simply do not know at this point which connection will be choosen.
	 Load balancing/filter chain is yet to come. Thus, we do not know yet on
	 which server BEGIN shall be executed.

	 As a solution for now, BEGIN is delayed until a connection has been picked.
	 Once picked, we stay on the same server until the end of the transaction
	 is recognized.

	 Note that we set the request for BEGIN, the message to the LB filter, on
	 the proxy connection. We also set in_transaction on the proxy connection.
	 Whereas the actual BEGIN has to be send on another connection.
	 The proxy connection stgy object is passed to the filters.
	 When intercepting a COMMIT/ROLLBACK we have to send the command on the
	 last used connection but reset in_transaction on the proxy connection.
	*/

	if (FALSE == CONN_DATA_NOT_SET(conn_data)) {
#ifndef MYSQLND_HAS_INJECTION_FEATURE
		if ((TRUE == (*conn_data)->stgy.in_transaction) &&
			(CONN_DATA_TRY_TRX_INJECTION(conn_data, conn)))
		{
			/*
			Implicit commit when begin() ..query().. begin().
			Must inject before second=current begin() call.
			*/
			MS_TRX_INJECT(ret, conn, conn_data);
			MYSQLND_MS_INC_STATISTIC((PASS == ret) ? MS_STAT_GTID_IMPLICIT_COMMIT_SUCCESS :
				MS_STAT_GTID_IMPLICIT_COMMIT_FAILURE);

			if (FAIL == ret) {
				if (TRUE == (*conn_data)->global_trx.report_error) {
					DBG_RETURN(ret);
				}

				ret = PASS;
				SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(conn));
			}
		}
#endif

		if	((*conn_data)->stgy.trx_stickiness_strategy != TRX_STICKINESS_STRATEGY_DISABLED) {

			/* the true answer is delayed... unfortunately :-/ */
			ret = PASS;

			/* reundant if autocommit(false) -> in_trx = 1 but does not harm */
			(*conn_data)->stgy.in_transaction = TRUE;
			/* filter will set this after choosing an initial connection */
			(*conn_data)->stgy.trx_stop_switching = FALSE;
			/* message to filter: call tx_begin */
			(*conn_data)->stgy.trx_begin_required = TRUE;
			(*conn_data)->stgy.trx_begin_mode = mode;
			if ((*conn_data)->stgy.trx_begin_name) {
				mnd_pefree((*conn_data)->stgy.trx_begin_name, conn->persistent);
			}
			(*conn_data)->stgy.trx_begin_name = (name) ? mnd_pestrdup(name, conn->persistent) : NULL;

			if (mode & TRANS_START_READ_ONLY) {
				(*conn_data)->stgy.trx_read_only = TRUE;
				DBG_INF("In read only transaction, stop switching.");
			} else {
				(*conn_data)->stgy.trx_read_only = FALSE;
				DBG_INF("In transaction, stop switching.");
			}
		} else {
			/* Note: we are dealing with the proxy connection */
			ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(tx_begin)(conn, mode, name TSRMLS_CC);
			if (PASS == ret)
				(*conn_data)->stgy.in_transaction = TRUE;
			DBG_INF_FMT("in_transaction=%d", (*conn_data)->stgy.in_transaction);
		}
	} else {
		/* Note: we are dealing with the proxy connection */
		ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(tx_begin)(conn, mode, name TSRMLS_CC);
	}

	DBG_RETURN(ret);
}
/* }}} */
#endif

/* {{{ MYSQLND_METHOD(mysqlnd_ms, tx_commit) */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, tx_commit)(MYSQLND_CONN_DATA * conn TSRMLS_DC)
{
	enum_func_status ret = FAIL;

	DBG_ENTER("mysqlnd_ms::tx_commit");
#if MYSQLND_VERSION_ID >= 50011
	ret = mysqlnd_ms_tx_commit_or_rollback(conn, TRUE, TRANS_COR_NO_OPT, NULL TSRMLS_CC);
#else
	ret = mysqlnd_ms_tx_commit_or_rollback(conn, TRUE TSRMLS_CC);
#endif
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ MYSQLND_METHOD(mysqlnd_ms, tx_rollback) */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, tx_rollback)(MYSQLND_CONN_DATA * conn TSRMLS_DC)
{
	enum_func_status ret = FAIL;

	DBG_ENTER("mysqlnd_ms::tx_rollback");
#if MYSQLND_VERSION_ID >= 50011
	ret = mysqlnd_ms_tx_commit_or_rollback(conn, FALSE, TRANS_COR_NO_OPT, NULL TSRMLS_CC);
#else
	ret = mysqlnd_ms_tx_commit_or_rollback(conn, FALSE TSRMLS_CC);
#endif
	DBG_RETURN(ret);
}
/* }}} */
#endif


/* {{{ mysqlnd_ms::statistic */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, get_server_statistics)(MYSQLND_CONN_DATA * proxy_conn, char **message, unsigned int * message_len TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	DBG_ENTER("mysqlnd_ms::statistic");
	DBG_INF_FMT("conn="MYSQLND_LLU_SPEC, conn->thread_id);
	if (CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY) {
		conn = mysqlnd_ms_pick_first_master_or_slave(proxy_conn TSRMLS_CC);
		if (!conn || (CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY)) {
			DBG_INF("No connection");
			DBG_RETURN(ret);
		}
	}
	ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(get_server_statistics)(conn, message, message_len TSRMLS_CC);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms::get_server_version */
static unsigned long
MYSQLND_METHOD(mysqlnd_ms, get_server_version)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;
	if (CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY) {
		conn = mysqlnd_ms_pick_first_master_or_slave(proxy_conn TSRMLS_CC);
	}
	return MS_CALL_ORIGINAL_CONN_DATA_METHOD(get_server_version)(conn TSRMLS_CC);
}
/* }}} */


/* {{{ mysqlnd_ms::get_server_info */
static const char *
MYSQLND_METHOD(mysqlnd_ms, get_server_info)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;
	if (CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY) {
		conn = mysqlnd_ms_pick_first_master_or_slave(proxy_conn TSRMLS_CC);
	}
	return MS_CALL_ORIGINAL_CONN_DATA_METHOD(get_server_information)(conn TSRMLS_CC);
}
/* }}} */


/* {{{ mysqlnd_ms::get_host_info */
static const char *
MYSQLND_METHOD(mysqlnd_ms, get_host_info)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	if (CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY) {
		conn = mysqlnd_ms_pick_first_master_or_slave(proxy_conn TSRMLS_CC);
	}
	return MS_CALL_ORIGINAL_CONN_DATA_METHOD(get_host_information)(conn TSRMLS_CC);
}
/* }}} */


/* {{{ mysqlnd_ms::get_proto_info */
static unsigned int
MYSQLND_METHOD(mysqlnd_ms, get_proto_info)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	if (CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY) {
		conn = mysqlnd_ms_pick_first_master_or_slave(proxy_conn TSRMLS_CC);
	}
	return MS_CALL_ORIGINAL_CONN_DATA_METHOD(get_protocol_information)(conn TSRMLS_CC);
}
/* }}} */


/* {{{ mysqlnd_ms::charset_name */
static const char *
MYSQLND_METHOD(mysqlnd_ms, charset_name)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	if (CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY) {
		conn = mysqlnd_ms_pick_first_master_or_slave(proxy_conn TSRMLS_CC);
	}
	return MS_CALL_ORIGINAL_CONN_DATA_METHOD(charset_name)(conn TSRMLS_CC);
}
/* }}} */


/* {{{ mysqlnd_ms::get_connection_stats */
static void
MYSQLND_METHOD(mysqlnd_ms, get_connection_stats)(const MYSQLND_CONN_DATA * const proxy_conn, zval * return_value TSRMLS_DC ZEND_FILE_LINE_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	MS_CALL_ORIGINAL_CONN_DATA_METHOD(get_statistics)(conn, return_value TSRMLS_CC ZEND_FILE_LINE_CC);
}
/* }}} */


/* {{{ mysqlnd_ms::dump_debug_info */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, dump_debug_info)(MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	DBG_ENTER("mysqlnd_ms::dump_debug_info");
	if (CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY) {
		conn = mysqlnd_ms_pick_first_master_or_slave(proxy_conn TSRMLS_CC);
	}
	DBG_RETURN(MS_CALL_ORIGINAL_CONN_DATA_METHOD(server_dump_debug_information)(conn TSRMLS_CC));
}
/* }}} */


/* {{{ mysqlnd_ms_stmt::prepare */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms_stmt, prepare)(MYSQLND_STMT * const s, const char * const query, unsigned int query_len TSRMLS_DC)
{
	MYSQLND_MS_CONN_DATA ** conn_data = NULL;
	MYSQLND_CONN_DATA * connection = NULL;
	enum_func_status ret = PASS;
	zend_bool free_query = FALSE, switched_servers = FALSE;
	uint transient_error_no = 0, transient_error_retries = 0;
	DBG_ENTER("mysqlnd_ms_stmt::prepare");
	DBG_INF_FMT("query=%s", query);

	if (!s || !s->data || !s->data->conn ||
		!(MS_LOAD_CONN_DATA(conn_data, s->data->conn)) ||
		!*conn_data || (*conn_data)->skip_ms_calls)
	{
		DBG_INF("skip MS");
		DBG_RETURN(ms_orig_mysqlnd_stmt_methods->prepare(s, query, query_len TSRMLS_CC));
	}

	/* this can possibly reroute us to another server */
	connection = mysqlnd_ms_pick_server_ex((*conn_data)->proxy_conn, (char **)&query, (size_t *)&query_len, &free_query, &switched_servers TSRMLS_CC);
	DBG_INF_FMT("Connection %p, query=%s", connection, query);

	if (connection != s->data->conn) {
		/* free what we have */
		s->m->net_close(s, TRUE TSRMLS_CC);
		mnd_pefree(s->data, s->data->persistent);

		/* new handle */
		{
			MYSQLND_STMT * new_handle = MS_CALL_ORIGINAL_CONN_DATA_METHOD(stmt_init)(connection TSRMLS_CC);
			if (!new_handle || !new_handle->data) {
				DBG_ERR("new_handle is null");
				if (TRUE == free_query) {
					efree((void *)query);
				}
				DBG_RETURN(FAIL);
			}
			s->data = new_handle->data;
			mnd_pefree(new_handle, new_handle->data->persistent);
		}
	}

    // BEGIN HACK
#ifndef MYSQLND_HAS_INJECTION_FEATURE
	{
		zend_bool forced = FALSE;
		(*conn_data)->stgy.injectable_query = ((*conn_data)->stgy.in_transaction ? FALSE : mysqlnd_ms_query_is_injectable_query(query, query_len, &forced TSRMLS_CC));
		if (forced) {
			(*conn_data)->stgy.stop_inject = (*conn_data)->stgy.injectable_query ? FALSE : TRUE;
		}
	}
#endif
	// END HACK
	do {
		ret = ms_orig_mysqlnd_stmt_methods->prepare(s, query, query_len TSRMLS_CC);
		if (PASS == ret) {
			break;
		}
		MS_CHECK_CONN_FOR_TRANSIENT_ERROR(connection, conn_data, transient_error_no);
		if (transient_error_no) {
			DBG_INF_FMT("Transient error "MYSQLND_LLU_SPEC, transient_error_no);
			transient_error_retries++;
			if (transient_error_retries <= (*conn_data)->stgy.transient_error_max_retries) {
				MYSQLND_MS_INC_STATISTIC(MS_STAT_TRANSIENT_ERROR_RETRIES);
				DBG_INF_FMT("Retry attempt %i/%i. Sleeping for "MYSQLND_LLU_SPEC" ms and retrying.",
					transient_error_retries,
					(*conn_data)->stgy.transient_error_max_retries,
					(*conn_data)->stgy.transient_error_usleep_before_retry);
#if HAVE_USLEEP
				usleep((*conn_data)->stgy.transient_error_usleep_before_retry);
#endif
			} else {
				DBG_INF("No more transient error retries allowed");
				ret = FAIL;
				break;
			}
		} else {
			/* an error that is not considered transient */
			ret = FAIL;
			break;
		}
	} while (transient_error_no);


	if (TRUE == free_query) {
		efree((void *)query);
	}
	DBG_RETURN(ret);
}
/* }}} */


#ifndef MYSQLND_HAS_INJECTION_FEATURE
/* {{{ mysqlnd_ms_stmt::execute */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms_stmt, execute)(MYSQLND_STMT * const s TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_CONN_DATA ** conn_data = NULL;
	uint transient_error_no = 0, transient_error_retries = 0;
	unsigned int stmt_errno;
	MYSQLND_CONN_DATA * connection = NULL;
	MYSQLND_STMT_DATA * stmt = s? s->data:NULL;

	DBG_ENTER("mysqlnd_ms_stmt::execute");

	if (!stmt ||
		!s || !s->data || !s->data->conn ||
		!(MS_LOAD_CONN_DATA(conn_data, s->data->conn)) ||
		!conn_data || !*conn_data || (*conn_data)->skip_ms_calls)
	{
		DBG_INF("skip MS");
		ret = ms_orig_mysqlnd_stmt_methods->execute(s TSRMLS_CC);
		DBG_RETURN(ret);
	}
	connection = s->data->conn;
	DBG_INF_FMT("conn="MYSQLND_LLU_SPEC, connection->thread_id);

//BEGIN HACK
	/*
	if (CONN_DATA_TRY_TRX_INJECTION(conn_data, connection) &&
		(FALSE == (*conn_data)->stgy.in_transaction))
	{
	*/
		/* autocommit mode */
	/*
	    MS_TRX_INJECT(ret, connection, conn_data);
		MYSQLND_MS_INC_STATISTIC((PASS == ret) ? MS_STAT_GTID_AUTOCOMMIT_SUCCESS : MS_STAT_GTID_AUTOCOMMIT_FAILURE);

		if (FAIL == ret) {
			if (TRUE == (*conn_data)->global_trx.report_error) {
	*/
				/* user stmt returns false and shall have error set */

	/*			SET_STMT_ERROR(stmt,
					(MYSQLND_MS_ERROR_INFO(connection)).error_no,
					(MYSQLND_MS_ERROR_INFO(connection)).sqlstate,
					(MYSQLND_MS_ERROR_INFO(connection)).error);
				DBG_RETURN(ret);
			}
			SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(connection));
		}
	}

	*/
//END HACK
	do {
		ret = ms_orig_mysqlnd_stmt_methods->execute(s TSRMLS_CC);
		stmt_errno = ms_orig_mysqlnd_stmt_methods->get_error_no(s TSRMLS_CC);
		MS_CHECK_FOR_TRANSIENT_ERROR(stmt_errno, conn_data, transient_error_no);
		if (transient_error_no) {
			DBG_INF_FMT("Transient error "MYSQLND_LLU_SPEC, transient_error_no);
			transient_error_retries++;
			if (transient_error_retries <= (*conn_data)->stgy.transient_error_max_retries) {
				MYSQLND_MS_INC_STATISTIC(MS_STAT_TRANSIENT_ERROR_RETRIES);
				DBG_INF_FMT("Retry attempt %i/%i. Sleeping for "MYSQLND_LLU_SPEC" ms and retrying.",
							transient_error_retries,
							(*conn_data)->stgy.transient_error_max_retries,
							(*conn_data)->stgy.transient_error_usleep_before_retry);
#if HAVE_USLEEP
				usleep((*conn_data)->stgy.transient_error_usleep_before_retry);
#endif
			} else {
				DBG_INF("No more transient error retries allowed");
				break;
			}
		}
	} while (transient_error_no);
	// BEGIN HACK
	if (CONN_DATA_TRY_TRX_INJECTION(conn_data, connection) &&
		FALSE == (*conn_data)->stgy.in_transaction && FALSE == (*conn_data)->stgy.stop_inject && TRUE == (*conn_data)->stgy.injectable_query)
	{
		enum_func_status retx = FAIL;
		/* autocommit mode */
		MS_TRX_INJECT(retx, connection, conn_data);
		MYSQLND_MS_INC_STATISTIC((PASS == retx) ? MS_STAT_GTID_AUTOCOMMIT_SUCCESS : MS_STAT_GTID_AUTOCOMMIT_FAILURE);

		if (FAIL == ret) {
			if (TRUE == (*conn_data)->global_trx.report_error) {
				/* user stmt returns false and shall have error set */
				SET_STMT_ERROR(stmt,
					(MYSQLND_MS_ERROR_INFO(connection)).error_no,
					(MYSQLND_MS_ERROR_INFO(connection)).sqlstate,
					(MYSQLND_MS_ERROR_INFO(connection)).error);
				DBG_RETURN(retx);
			}
			SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(connection));
		}
	}
	// END HACK
	DBG_RETURN(ret);
}
/* }}} */
#endif

/* {{{ mysqlnd_conn::ssl_set */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, ssl_set)(MYSQLND_CONN_DATA * const proxy_conn, const char * key, const char * const cert,
									const char * const ca, const char * const capath, const char * const cipher TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);

	DBG_ENTER("mysqlnd_ms::ssl_set");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, proxy_conn->thread_id);
	if (CONN_DATA_NOT_SET(conn_data)) {
		DBG_RETURN(MS_CALL_ORIGINAL_CONN_DATA_METHOD(ssl_set)(proxy_conn, key, cert, ca, capath, cipher TSRMLS_CC));
	} else {
		MYSQLND_MS_LIST_DATA * el;

		ret = (*conn_data)->pool->dispatch_ssl_set((*conn_data)->pool, MYSQLND_METHOD(mysqlnd_ms, ssl_set),
												   key, cert, ca, capath, cipher TSRMLS_CC);

		BEGIN_ITERATE_OVER_SERVER_LISTS(el, (*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC), (*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC));
		{
			if (PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(ssl_set)(el->conn, key, cert, ca, capath, cipher TSRMLS_CC)) {
				ret = FAIL;
			}
		}
		END_ITERATE_OVER_SERVER_LISTS;
	}
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms::close */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, close)(MYSQLND * conn, enum_connection_close_type close_type TSRMLS_DC)
{
	enum_func_status ret;
	DBG_ENTER("mysqlnd_ms::close");

	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, MS_GET_CONN_DATA_FROM_CONN(conn)->thread_id);

	/* Let XA check for unfinished global transactions */
	ret = mysqlnd_ms_xa_conn_close(MS_GET_CONN_DATA_FROM_CONN(conn) TSRMLS_CC);
	/*
	  Force cleaning of the master and slave lists.
	  In the master list this connection is present and free_reference will be called, and later
	  in the original `close` the data of this connection will be destructed as refcount will become 0.
	*/
	mysqlnd_ms_conn_free_plugin_data(MS_GET_CONN_DATA_FROM_CONN(conn) TSRMLS_CC);
	ret = MS_CALL_ORIGINAL_CONN_HANDLE_METHOD(close)(conn, close_type TSRMLS_CC);
	DBG_RETURN(ret);
}
/* }}} */



/* {{{ mysqlnd_ms_register_hooks*/
void
mysqlnd_ms_register_hooks()
{
	MS_LOAD_AND_COPY_CONN_DATA_METHODS(ms_orig_mysqlnd_conn_methods, my_mysqlnd_conn_methods);

	my_mysqlnd_conn_methods.connect				= MYSQLND_METHOD(mysqlnd_ms, connect);
	my_mysqlnd_conn_methods.query				= MYSQLND_METHOD(mysqlnd_ms, query);
	my_mysqlnd_conn_methods.send_query			= MYSQLND_METHOD(mysqlnd_ms, send_query);
	my_mysqlnd_conn_methods.use_result			= MYSQLND_METHOD(mysqlnd_ms, use_result);
	my_mysqlnd_conn_methods.store_result		= MYSQLND_METHOD(mysqlnd_ms, store_result);
	my_mysqlnd_conn_methods.dtor				= MYSQLND_METHOD_PRIVATE(mysqlnd_ms, dtor);
	my_mysqlnd_conn_methods.escape_string		= MYSQLND_METHOD(mysqlnd_ms, escape_string);
	my_mysqlnd_conn_methods.change_user			= MYSQLND_METHOD(mysqlnd_ms, change_user);
	my_mysqlnd_conn_methods.ping				= MYSQLND_METHOD(mysqlnd_ms, ping);
	my_mysqlnd_conn_methods.kill_connection		= MYSQLND_METHOD(mysqlnd_ms, kill);
	my_mysqlnd_conn_methods.get_thread_id		= MYSQLND_METHOD(mysqlnd_ms, thread_id);
	my_mysqlnd_conn_methods.select_db			= MYSQLND_METHOD(mysqlnd_ms, select_db);
	my_mysqlnd_conn_methods.set_charset			= MYSQLND_METHOD(mysqlnd_ms, set_charset);
	my_mysqlnd_conn_methods.set_server_option	= MYSQLND_METHOD(mysqlnd_ms, set_server_option);
	my_mysqlnd_conn_methods.set_client_option	= MYSQLND_METHOD(mysqlnd_ms, set_client_option);
	my_mysqlnd_conn_methods.next_result			= MYSQLND_METHOD(mysqlnd_ms, next_result);
	my_mysqlnd_conn_methods.more_results		= MYSQLND_METHOD(mysqlnd_ms, more_results);
	my_mysqlnd_conn_methods.get_error_no		= MYSQLND_METHOD(mysqlnd_ms, error_no);
	my_mysqlnd_conn_methods.get_error_str		= MYSQLND_METHOD(mysqlnd_ms, error);
	my_mysqlnd_conn_methods.get_sqlstate		= MYSQLND_METHOD(mysqlnd_ms, sqlstate);

	my_mysqlnd_conn_methods.ssl_set				= MYSQLND_METHOD(mysqlnd_ms, ssl_set);


	my_mysqlnd_conn_methods.get_field_count		= MYSQLND_METHOD(mysqlnd_ms, field_count);
	my_mysqlnd_conn_methods.get_last_insert_id	= MYSQLND_METHOD(mysqlnd_ms, insert_id);
	my_mysqlnd_conn_methods.get_affected_rows	= MYSQLND_METHOD(mysqlnd_ms, affected_rows);
	my_mysqlnd_conn_methods.get_warning_count	= MYSQLND_METHOD(mysqlnd_ms, warning_count);
	my_mysqlnd_conn_methods.get_last_message	= MYSQLND_METHOD(mysqlnd_ms, info);
#if MYSQLND_VERSION_ID >= 50009
	my_mysqlnd_conn_methods.set_autocommit		= MYSQLND_METHOD(mysqlnd_ms, set_autocommit);
	my_mysqlnd_conn_methods.tx_commit			= MYSQLND_METHOD(mysqlnd_ms, tx_commit);
	my_mysqlnd_conn_methods.tx_rollback			= MYSQLND_METHOD(mysqlnd_ms, tx_rollback);
#endif
#if MYSQLND_VERSION_ID >= 50011
	my_mysqlnd_conn_methods.tx_commit_or_rollback 	= MYSQLND_METHOD(mysqlnd_ms, tx_commit_or_rollback);
	my_mysqlnd_conn_methods.tx_begin 				= MYSQLND_METHOD(mysqlnd_ms, tx_begin);
#endif

	my_mysqlnd_conn_methods.get_server_statistics	= MYSQLND_METHOD(mysqlnd_ms, get_server_statistics);
	my_mysqlnd_conn_methods.get_server_version		= MYSQLND_METHOD(mysqlnd_ms, get_server_version);
	my_mysqlnd_conn_methods.get_server_information	= MYSQLND_METHOD(mysqlnd_ms, get_server_info);
	my_mysqlnd_conn_methods.get_host_information	= MYSQLND_METHOD(mysqlnd_ms, get_host_info);
	my_mysqlnd_conn_methods.get_protocol_information= MYSQLND_METHOD(mysqlnd_ms, get_proto_info);
	my_mysqlnd_conn_methods.charset_name			= MYSQLND_METHOD(mysqlnd_ms, charset_name);
	my_mysqlnd_conn_methods.get_statistics			= MYSQLND_METHOD(mysqlnd_ms, get_connection_stats);
	my_mysqlnd_conn_methods.server_dump_debug_information = MYSQLND_METHOD(mysqlnd_ms, dump_debug_info);

	MS_SET_CONN_DATA_METHODS(&my_mysqlnd_conn_methods);

#if PHP_VERSION_ID < 50399
	my_mysqlnd_conn_methods.close				= MYSQLND_METHOD(mysqlnd_ms, close);
#else
	MS_LOAD_AND_COPY_CONN_HANDLE_METHODS(ms_orig_mysqlnd_conn_handle_methods, my_mysqlnd_conn_handle_methods);
	my_mysqlnd_conn_handle_methods.close		= MYSQLND_METHOD(mysqlnd_ms, close);
	MS_SET_CONN_HANDLE_METHODS(&my_mysqlnd_conn_handle_methods);
#endif

	ms_orig_mysqlnd_stmt_methods = mysqlnd_stmt_get_methods();
	memcpy(&my_mysqlnd_stmt_methods, ms_orig_mysqlnd_stmt_methods, sizeof(struct st_mysqlnd_stmt_methods));

	my_mysqlnd_stmt_methods.prepare = MYSQLND_METHOD(mysqlnd_ms_stmt, prepare);
#ifndef MYSQLND_HAS_INJECTION_FEATURE
	my_mysqlnd_stmt_methods.execute = MYSQLND_METHOD(mysqlnd_ms_stmt, execute);
#endif

	mysqlnd_stmt_set_methods(&my_mysqlnd_stmt_methods);
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
