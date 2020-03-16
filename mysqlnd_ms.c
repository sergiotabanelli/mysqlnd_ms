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
#include "ext/mysqlnd/mysqlnd_wireprotocol.h"
#if PHP_VERSION_ID >= 70100
#include "ext/mysqlnd/mysqlnd_connection.h"
#endif
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

#include "mysqlnd_ms_filter_qos.h"
#ifdef MYSQLND_MS_HAVE_MYSQLND_QC
#include "ext/mysqlnd_qc/mysqlnd_qc.h"
#endif
#include "fabric/mysqlnd_fabric.h"

#include "mysqlnd_ms_conn_pool.h"



#define CONN_GET_OPTION(conn, option) (conn)->options->option

#define MS_LOAD_AND_COPY_CONN_HANDLE_METHODS(orig_methods, ms_methods) \
	(orig_methods) = _ms_mysqlnd_conn_get_methods(); \
	ms_methods = *(orig_methods);

#define MS_SET_CONN_HANDLE_METHODS(ms_methods) mysqlnd_conn_set_methods((ms_methods));

#define MS_LOAD_AND_COPY_CONN_DATA_METHODS(orig_methods, ms_methods) \
	(orig_methods) = _ms_mysqlnd_conn_data_get_methods(); \
	ms_methods = *(orig_methods);

#define MS_SET_CONN_DATA_METHODS(ms_methods) mysqlnd_conn_data_set_methods((ms_methods));

#define MS_LOAD_AND_COPY_STMT_METHODS(orig_methods, ms_methods) \
	(orig_methods) = _ms_mysqlnd_stmt_get_methods(); \
	ms_methods = *(orig_methods);

#define MS_SET_STMT_METHODS(ms_methods) mysqlnd_stmt_set_methods((ms_methods));

#define MS_LOAD_AND_COPY_PROTOCOL_METHODS(orig_methods, ms_methods) \
	(orig_methods) = _ms_mysqlnd_protocol_get_methods(); \
	ms_methods = *(orig_methods);

#define MS_SET_PROTOCOL_METHODS(ms_methods) mysqlnd_protocol_set_methods((ms_methods));

#define MS_GET_CONN_DATA_FROM_CONN(conn) (conn)->data

struct st_mysqlnd_conn_data_methods * ms_orig_mysqlnd_conn_methods;
static struct st_mysqlnd_conn_methods my_mysqlnd_conn_handle_methods;

struct st_mysqlnd_conn_methods * ms_orig_mysqlnd_conn_handle_methods;
static struct st_mysqlnd_conn_data_methods my_mysqlnd_conn_methods;

struct st_mysqlnd_stmt_methods * ms_orig_mysqlnd_stmt_methods = NULL;
static struct st_mysqlnd_stmt_methods my_mysqlnd_stmt_methods;

static void mysqlnd_ms_conn_free_plugin_data(MYSQLND_CONN_DATA * conn TSRMLS_DC);

MYSQLND_STATS * mysqlnd_ms_stats = NULL;


#define CONN_DATA_NOT_SET(conn_data) (!(conn_data) || !*(conn_data) || !(*(conn_data))->initialized || (*(conn_data))->skip_ms_calls)
#define CONN_DATA_TRX_SET(conn_data) ((conn_data) && (*(conn_data)) && (!(*(conn_data))->skip_ms_calls) && ((*(conn_data))->global_trx.type != GTID_NONE))

//CLONED FROM mysqlnd_wireprotocol.c
#if PHP_VERSION_ID < 70100
static struct st_mysqlnd_protocol_methods * ms_orig_mysqlnd_protocol_methods;
static struct st_mysqlnd_protocol_methods my_mysqlnd_protocol_methods;

#define _MS_PROTOCOL_TYPE MYSQLND_PROTOCOL
#define _MS_PROTOCOL_CONN_LOAD_CONN_D
#define _MS_PROTOCOL_CONN_LOAD_NET_D MYSQLND_NET * net = conn->net
#define _MS_PROTOCOL_CONN_LOAD_VIO_D
#define _MS_PROTOCOL_CONN_READ_D void * _packet, MYSQLND_CONN_DATA * conn
#define _MS_PROTOCOL_CONN_READ_A _packet, conn
#define _MS_PROTOCOL_CONN_READ_NET_D MYSQLND_NET * net
#define _MS_PROTOCOL_CONN_READ_NET_A net
#define _ms_net_receive net->data->m.receive_ex
#define _ms_net_packet_no net->packet_no
#define _MS_PROTOCOL_CONN_DATA_LOAD_NET_D(conn, cnet) MYSQLND_NET * cnet = conn->net
#define _MS_PROTOCOL_CONN_DATA_LOAD_VIO_D(conn, cvio) MYSQLND_NET * cvio = conn->net
#define _MS_PROTOCOL_CONN_DATA_NET_SPK(cnet) cnet->data->options.sha256_server_public_key
#else
static struct st_mysqlnd_protocol_payload_decoder_factory_methods * ms_orig_mysqlnd_protocol_methods;
static struct st_mysqlnd_protocol_payload_decoder_factory_methods my_mysqlnd_protocol_methods;

#define _MS_PROTOCOL_TYPE MYSQLND_PROTOCOL_PAYLOAD_DECODER_FACTORY
#if PHP_VERSION_ID < 70300
#define _MS_PROTOCOL_CONN_LOAD_CONN_D MYSQLND_CONN_DATA * conn = header->factory->conn
#define _MS_PROTOCOL_CONN_LOAD_NET_D MYSQLND_PFC * net = header->protocol_frame_codec
#define _MS_PROTOCOL_CONN_READ_D void * _packet
#define _MS_PROTOCOL_CONN_READ_A _packet
#define _MS_PROTOCOL_CONN_LOAD_VIO_D MYSQLND_VIO * vio = header->vio
#else
#define _MS_PROTOCOL_CONN_LOAD_CONN_D
#define _MS_PROTOCOL_CONN_LOAD_NET_D MYSQLND_PFC * net = conn->protocol_frame_codec
#define _MS_PROTOCOL_CONN_READ_D MYSQLND_CONN_DATA * conn, void * _packet 
#define _MS_PROTOCOL_CONN_READ_A conn, _packet 
#define _MS_PROTOCOL_CONN_LOAD_VIO_D MYSQLND_VIO * vio = conn->vio
#endif
#define _MS_PROTOCOL_CONN_READ_NET_D MYSQLND_PFC * net, MYSQLND_VIO * vio
#define _MS_PROTOCOL_CONN_READ_NET_A net, vio
#define _ms_net_receive net->data->m.receive
#define _ms_net_packet_no net->data->packet_no
#define _MS_PROTOCOL_CONN_DATA_LOAD_NET_D(conn, cnet) MYSQLND_PFC * cnet = conn->protocol_frame_codec
#define _MS_PROTOCOL_CONN_DATA_LOAD_VIO_D(conn, cvio) MYSQLND_VIO * cvio = conn->vio
#define _MS_PROTOCOL_CONN_DATA_NET_SPK(cnet) cnet->data->sha256_server_public_key
#endif
static enum_func_status	(*ms_orig_ok_read)(_MS_PROTOCOL_CONN_READ_D TSRMLS_DC);
static enum_func_status	(*ms_orig_rset_header_read)(_MS_PROTOCOL_CONN_READ_D TSRMLS_DC);

#define ERROR_MARKER 0xFF
#define OK_BUFFER_SIZE 2048

static enum_mysqlnd_collected_stats packet_type_to_statistic_byte_count[PROT_LAST] =
{
	STAT_LAST,
	STAT_LAST,
	STAT_BYTES_RECEIVED_OK,
	STAT_BYTES_RECEIVED_EOF,
	STAT_LAST,
	STAT_BYTES_RECEIVED_RSET_HEADER,
	STAT_BYTES_RECEIVED_RSET_FIELD_META,
	STAT_BYTES_RECEIVED_RSET_ROW,
	STAT_BYTES_RECEIVED_PREPARE_RESPONSE,
	STAT_BYTES_RECEIVED_CHANGE_USER,
};

static enum_mysqlnd_collected_stats packet_type_to_statistic_packet_count[PROT_LAST] =
{
	STAT_LAST,
	STAT_LAST,
	STAT_PACKETS_RECEIVED_OK,
	STAT_PACKETS_RECEIVED_EOF,
	STAT_LAST,
	STAT_PACKETS_RECEIVED_RSET_HEADER,
	STAT_PACKETS_RECEIVED_RSET_FIELD_META,
	STAT_PACKETS_RECEIVED_RSET_ROW,
	STAT_PACKETS_RECEIVED_PREPARE_RESPONSE,
	STAT_PACKETS_RECEIVED_CHANGE_USER,
};

#define BAIL_IF_NO_MORE_DATA \
	if ((size_t)(p - begin) > packet->header.size) { \
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Premature end of data (mysqlnd_wireprotocol.c:%u)", __LINE__); \
		goto premature_end; \
	} \

#define CLIENT_SESSION_TRACK					(1UL << 23)
#define SERVER_SESSION_STATE_CHANGED (1UL << 14)
enum enum_session_state_type
{
  SESSION_TRACK_SYSTEM_VARIABLES,                       /* Session system variables */
  SESSION_TRACK_SCHEMA,                          /* Current schema */
  SESSION_TRACK_STATE_CHANGE,                  /* track session state changes */
  SESSION_TRACK_GTIDS,
  SESSION_TRACK_TRANSACTION_CHARACTERISTICS,  /* Transaction chistics */
  SESSION_TRACK_TRANSACTION_STATE             /* Transaction state */
};


/* {{{ mysqlnd_ms_set_tx */
enum_func_status
mysqlnd_ms_set_tx(MYSQLND_CONN_DATA * conn, zend_bool mode TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	DBG_ENTER("mysqlnd_ms_set_tx");
	if (FALSE == CONN_DATA_NOT_SET(conn_data)) {
		ret = conn->m->tx_begin(conn, mode, NULL TSRMLS_CC);
		(*conn_data)->stgy.trx_begin_required = FALSE; // START TRANSACTION IS SENT EXPLICITLY BY APPLICATION OR PDO
		(*conn_data)->stgy.trx_begin_mode = 0;
	}
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_unset_tx */
enum_func_status
mysqlnd_ms_unset_tx(MYSQLND_CONN_DATA * proxy_conn, zend_bool commit TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_CONN_DATA * conn;
	MYSQLND_MS_CONN_DATA ** conn_data;
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);
	DBG_ENTER("mysqlnd_ms_unset_tx");
	if (proxy_conn_data && *proxy_conn_data && (*proxy_conn_data)->stgy.last_used_conn) {
		conn = (*proxy_conn_data)->stgy.last_used_conn;
		MS_LOAD_CONN_DATA(conn_data, conn);
	} else {
		conn = proxy_conn;
		conn_data = proxy_conn_data;
	}
	if (conn_data && *conn_data) {
		if (CONN_DATA_TRX_SET(conn_data) && TRUE == commit && TRUE == (*proxy_conn_data)->stgy.in_transaction  && (*conn_data)->global_trx.is_master) {
			enum_func_status jret = MYSQLND_MS_GTID_CALL_PASS((*conn_data)->global_trx.m->gtid_inject_after, conn, (ret == PASS && commit == TRUE ? PASS : FAIL) TSRMLS_CC);
			MYSQLND_MS_INC_STATISTIC((PASS == jret) ? MS_STAT_GTID_COMMIT_SUCCESS : MS_STAT_GTID_COMMIT_FAILURE);
			if (FAIL == jret) {
				if (TRUE == (*conn_data)->global_trx.report_error) {
					if ((MYSQLND_MS_ERROR_INFO(conn)).error_no == 0) {
						SET_CLIENT_ERROR(_ms_p_ei (conn->error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Error on gtid_inject_after");
					}
					ret = FAIL;
				}
				SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(conn));
			}
		}
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
	DBG_RETURN(ret);
}
/* }}} */

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
		SET_CLIENT_ERROR(_ms_p_ei (error_info), client_error_code, client_error_state, error_buf);
	}
	if (php_error_level) {
		DBG_INF_FMT("error level %d %s", php_error_level, error_buf);
		php_error_docref(NULL TSRMLS_CC, php_error_level, "%s", error_buf);
	}

	DBG_INF_FMT("%s", error_buf);

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
	if (MYSQLND_MS_CONN_STRING(el->host) && !strcasecmp("localhost", MYSQLND_MS_CONN_STRING(el->host))) {
		scheme_len = mnd_sprintf(&tmp, 0, "unix://%s", MYSQLND_MS_CONN_STRING(el->socket) ? MYSQLND_MS_CONN_STRING(el->socket) : "/tmp/mysql.sock");
#else
	if (MYSQLND_MS_CONN_STRING(el->host) && !strcmp(".", MYSQLND_MS_CONN_STRING(el->host))) {
		scheme_len = mnd_sprintf(&tmp, 0, "pipe://%s", MYSQLND_MS_CONN_STRING(el->socket) ? MYSQLND_MS_CONN_STRING(el->socket) : "\\\\.\\pipe\\MySQL");
#endif
	} else {
		if (!el->port) {
			el->port = 3306;
		}
		scheme_len = mnd_sprintf(&tmp, 0, "tcp://%s:%u", MYSQLND_MS_CONN_STRING(el->host)? MYSQLND_MS_CONN_STRING(el->host):"localhost", el->port);
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
	DBG_INF_FMT("conn=%llu", element->conn->thread_id);
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
	MYSQLND_MS_CONN_STRING_FREE(element->host, element->persistent);
	MYSQLND_MS_CONN_STRING_FREE(element->user, element->persistent);
	MYSQLND_MS_CONN_STRINGL_FREE(element->passwd, element->persistent);
	MYSQLND_MS_CONN_STRINGL_FREE(element->db, element->persistent);
	MYSQLND_MS_CONN_STRING_FREE(element->socket, element->persistent);

	if (element->emulated_scheme) {
		mnd_pefree(element->emulated_scheme, element->persistent);
		element->emulated_scheme = NULL;
	}

	if (element->pool_hash_key.len) {
		_ms_smart_method(free, &(element->pool_hash_key));
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
	zend_bool skip_ms_calls = (*conn_data)->skip_ms_calls;
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


		ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(connect)(connection, MYSQLND_MS_CONN_A_CSTRING(element->host), MYSQLND_MS_CONN_A_CSTRING(element->user),
				MYSQLND_MS_CONN_A_CSTRINGL(element->passwd),
				MYSQLND_MS_CONN_A_CSTRINGL(element->db),
				element->port, MYSQLND_MS_CONN_A_CSTRING(element->socket), element->connect_flags TSRMLS_CC);

	}
	(*conn_data)->skip_ms_calls = skip_ms_calls;

	if (PASS == ret) {
		DBG_INF_FMT("Connection "MYSQLND_LLU_SPEC" established SESSION TRACK %lu", connection->thread_id, (element->connect_flags & CLIENT_SESSION_TRACK) );

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
	new_global_trx->type = orig_global_trx->type;
	new_global_trx->m = orig_global_trx->m;
	new_global_trx->owned_token = 0;
	new_global_trx->memcached_host_len = orig_global_trx->memcached_host_len;
	new_global_trx->memcached_host = (orig_global_trx->memcached_host) ?
		mnd_pestrndup(orig_global_trx->memcached_host, orig_global_trx->memcached_host_len, persistent) : NULL;
	new_global_trx->memcached_key_len = orig_global_trx->memcached_key_len;
	new_global_trx->memcached_key = (orig_global_trx->memcached_key) ?
		mnd_pestrndup(orig_global_trx->memcached_key, orig_global_trx->memcached_key_len, persistent) : NULL;
	new_global_trx->memcached_wkey_len = orig_global_trx->memcached_wkey_len;
	new_global_trx->memcached_wkey = (orig_global_trx->memcached_wkey) ?
		mnd_pestrndup(orig_global_trx->memcached_wkey, orig_global_trx->memcached_wkey_len, persistent) : NULL;
	new_global_trx->wc_uuid_len = orig_global_trx->wc_uuid_len;
	new_global_trx->wc_uuid = (orig_global_trx->wc_uuid) ?
		mnd_pestrndup(orig_global_trx->wc_uuid, orig_global_trx->wc_uuid_len, persistent) : NULL;
	new_global_trx->memcached_port = orig_global_trx->memcached_port;
	new_global_trx->memcached_port_add_hack = orig_global_trx->memcached_port_add_hack;
	new_global_trx->gtid_on_connect = orig_global_trx->gtid_on_connect;
	new_global_trx->auto_clean = orig_global_trx->auto_clean;
	new_global_trx->injectable_query = FALSE;
	new_global_trx->is_prepare = FALSE;
	new_global_trx->memc = NULL;
	new_global_trx->gtid_conn_elm = NULL;
	new_global_trx->last_gtid = NULL;
	new_global_trx->last_gtid_len = 0;
	new_global_trx->last_wgtid = NULL;
	new_global_trx->last_wgtid_len = 0;
	new_global_trx->last_ckgtid = NULL;
	new_global_trx->last_ckgtid_len = 0;
	new_global_trx->last_wckgtid = NULL;
	new_global_trx->last_wckgtid_len = 0;
	new_global_trx->gtid_block_size = orig_global_trx->gtid_block_size;
	new_global_trx->running_ttl = orig_global_trx->running_ttl;
	new_global_trx->memcached_debug_ttl = orig_global_trx->memcached_debug_ttl;
	new_global_trx->wait_for_wgtid_timeout = orig_global_trx->wait_for_wgtid_timeout;
	new_global_trx->throttle_wgtid_timeout = orig_global_trx->throttle_wgtid_timeout;
	new_global_trx->race_avoid_strategy = orig_global_trx->race_avoid_strategy;
// BEGIN NOWAIT
	new_global_trx->memcached = (orig_global_trx->memcached) ?
			mnd_pestrndup(orig_global_trx->memcached, orig_global_trx->memcached_len, persistent) : NULL;
	new_global_trx->memcached_len = orig_global_trx->memcached_len;
	new_global_trx->module = orig_global_trx->module;
	new_global_trx->running_depth = orig_global_trx->running_depth;
	new_global_trx->running_wdepth = orig_global_trx->running_wdepth;
	new_global_trx->qc_ttl = orig_global_trx->qc_ttl;
	new_global_trx->use_get = orig_global_trx->use_get;

	new_global_trx->owned_wtoken = 0;
	new_global_trx->last_chk_token = 0;
	new_global_trx->last_chk_wtoken = 0;
	new_global_trx->first_read = TRUE;
	new_global_trx->executed = FALSE;
	new_global_trx->prev_found = 0;
	new_global_trx->prev_wfound = 0;
	new_global_trx->last_whost = NULL;
// END NOWAIT

	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_connect_to_host_aux_elm */
static enum_func_status
mysqlnd_ms_connect_to_host_aux_elm(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_CONN_DATA * conn, const char * name_from_config,
							   zend_bool is_master,
							   const MYSQLND_MS_CONN_D_STRING(host), unsigned int port,
							   MYSQLND_MS_LIST_DATA ** new_element,
							   struct st_mysqlnd_ms_conn_credentials * cred,
							   zend_bool lazy_connections,
							   zend_bool persistent, zend_bool skip_ms_calls TSRMLS_DC)
{
	enum_func_status ret = FAIL;

	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);
	/* For gtid_conn_elm we have skip_ms_calls true and we do not need OK read */
	unsigned long mysql_flags = is_master && !skip_ms_calls && CONN_DATA_TRX_SET(proxy_conn_data) ? cred->mysql_flags | CLIENT_SESSION_TRACK : cred->mysql_flags;

	DBG_ENTER("mysqlnd_ms_connect_to_host_aux_elm");
	DBG_INF_FMT("conn:%p host:%s port:%d socket:%s", conn, MYSQLND_MS_CONN_STRING(host), cred->port, MYSQLND_MS_CONN_STRING(cred->socket));

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
			ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(connect)(conn, MYSQLND_MS_CONN_A_CSTRING(host), MYSQLND_MS_CONN_A_CSTRING(cred->user), MYSQLND_MS_CONN_A_CSTRINGL(cred->passwd), MYSQLND_MS_CONN_A_CSTRINGL(cred->db),
															 cred->port, MYSQLND_MS_CONN_A_CSTRING(cred->socket), mysql_flags TSRMLS_CC);
		}
		if (PASS == ret) {
			DBG_INF_FMT("Connection "MYSQLND_LLU_SPEC" established SESSION TRACK %lu", conn->thread_id, (mysql_flags & CLIENT_SESSION_TRACK) );
		}
	}

	if (ret == PASS) {
		(*new_element) = mnd_pecalloc(1, sizeof(MYSQLND_MS_LIST_DATA), persistent);
		memset((*new_element), 0, sizeof(MYSQLND_MS_LIST_DATA));
		(*new_element)->name_from_config = mnd_pestrdup(name_from_config? name_from_config:"", conn->persistent);
#if MYSQLND_VERSION_ID >= 50010
		(*new_element)->conn = conn->m->get_reference(conn TSRMLS_CC);
#else
		(*new_element)->conn = conn;
#endif
		MYSQLND_MS_CONN_STRING_DUP((*new_element)->host, host, persistent);
		(*new_element)->persistent = persistent;
		(*new_element)->port = port;

		MYSQLND_MS_CONN_STRING_DUP((*new_element)->user, cred->user, conn->persistent);

		MYSQLND_MS_CONN_STRINGL_DUP((*new_element)->passwd, cred->passwd, conn->persistent);

		MYSQLND_MS_CONN_STRINGL_DUP((*new_element)->db, cred->db, conn->persistent);

		(*new_element)->connect_flags = mysql_flags;

		MYSQLND_MS_CONN_STRING_DUP((*new_element)->socket, cred->socket, conn->persistent);

		(*new_element)->emulated_scheme_len = mysqlnd_ms_get_scheme_from_list_data((*new_element), &(*new_element)->emulated_scheme,
																					persistent TSRMLS_CC);
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
				mysqlnd_ms_init_connection_global_trx(&(*conn_data)->global_trx, &(*proxy_conn_data)->global_trx, is_master, conn->persistent TSRMLS_CC);
			}
		}
	}
	DBG_INF_FMT("ret=%s", ret == PASS? "PASS":"FAIL");
	DBG_RETURN(ret);
}
/* }}} */

#ifdef MYSQLND_MS_HAVE_MYSQLND_QC
/* {{{ mysqlnd_ms_aux_gtid_is_cached */
static zend_bool mysqlnd_ms_aux_gtid_is_cached(MYSQLND_CONN_DATA * conn, const char * gtid, char * query, size_t query_len TSRMLS_DC)
{
	char * server_id = NULL;
	int server_id_len;
	zend_bool ret = FALSE;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	struct st_mysqlnd_ms_conn_credentials * cred = &(*conn_data)->cred;
	DBG_ENTER("mysqlnd_ms_aux_gtid_is_cached");

	server_id_len = spprintf(&server_id, 0, "%s|%d|%s|%s|%s", (*conn_data)->connect_host, cred->port, MYSQLND_MS_CONN_STRING(cred->user), MYSQLND_MS_CONN_STRING(cred->db), gtid);
	ret = mysqlnd_qc_query_is_cached(conn, (const char *)query, query_len, server_id, server_id_len TSRMLS_CC);
	DBG_INF_FMT("Server id %s(%d) %p is_cached %d", server_id, server_id_len, server_id, ret);
	efree(server_id);
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_aux_gtid_check_and_cache */
static zend_bool mysqlnd_ms_aux_gtid_check_and_cache(MYSQLND_CONN_DATA * conn, const char * gtid, char ** query, size_t * query_len,
		zend_llist * slave_list, zend_llist * master_list, zend_llist * selected_slaves, zend_llist * selected_masters TSRMLS_DC)
{
	zend_bool ret = FALSE;
	char * new_query = NULL;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	struct st_mysqlnd_ms_conn_credentials * cred = &(*conn_data)->cred;
	DBG_ENTER("mysqlnd_ms_aux_gtid_check_and_cache");
	ret = mysqlnd_ms_aux_gtid_is_cached(conn, gtid, *query, *query_len);
	if (ret) {
		MYSQLND_MS_LIST_DATA * element;
		BEGIN_ITERATE_OVER_SERVER_LIST(element, master_list)
			zend_llist_add_element(selected_masters, &element);
		END_ITERATE_OVER_SERVER_LIST;
		BEGIN_ITERATE_OVER_SERVER_LIST(element, slave_list)
			zend_llist_add_element(selected_slaves, &element);
		END_ITERATE_OVER_SERVER_LIST;
	}
	if ((*conn_data)->global_trx.qc_ttl) {
		*query_len = spprintf(&new_query, 0, "/*" ENABLE_SWITCH "*//*" ENABLE_SWITCH_TTL"%u*//*" SERVER_ID_SWITCH "%s|%d|%s|%s|%s*/%s",
			(*conn_data)->global_trx.qc_ttl, (*conn_data)->connect_host, cred->port, MYSQLND_MS_CONN_STRING(cred->user), MYSQLND_MS_CONN_STRING(cred->db), gtid, *query);
		DBG_INF_FMT("Cache query ttl %s(%d) %p is cached %d", new_query, *query_len, new_query, ret);
	} else {
		*query_len = spprintf(&new_query, 0, "/*" SERVER_ID_SWITCH "%s|%d|%s|%s|%s*/%s",
				(*conn_data)->connect_host, cred->port, MYSQLND_MS_CONN_STRING(cred->user), MYSQLND_MS_CONN_STRING(cred->db), gtid, *query);
		DBG_INF_FMT("Cache query %s(%d) %p is cached %d", new_query, *query_len, new_query, ret);
	}
	*query = new_query;
	DBG_RETURN(ret);
}
#endif


#define MAXGTIDSIZE 60

static char *
mysqlnd_ms_aux_gtid_strnstr (const char *s1, const char *s2, size_t s2_len)
{
	const char *p = s1;
	for (; (p = strchr (p, *s2)) != 0; p++)
	{
		if (strncmp (p, s2, s2_len) == 0)
			return (char *)p;
	}
	return NULL;
}
/* }}} */

/* {{{ mysqlnd_ms_aux_gtid_last_in_set */
static uintmax_t
mysqlnd_ms_aux_gtid_last_in_set(const char * gtid_set, size_t gtid_set_len, uintmax_t gtid)
{
	size_t i = 0;
	const char * p;
	uintmax_t max = 0, min = UINTMAX_MAX, cutoff = UINTMAX_MAX/10;
	int cutlim = UINTMAX_MAX%10;
	if (gtid == UINTMAX_MAX) {
		errno = ERANGE;
		return gtid;
	}
	for (p = gtid_set; p[i] != 0 && p[i] != ',' && p+i < gtid_set+gtid_set_len; i++) {
		if (max > 0 && !isdigit(p[i])) {
			if (gtid == max || (gtid >= min && gtid <= max)) {
				return max;
			} else if (p[i] == '-') {
				min = max;
			} else if (min != UINTMAX_MAX) {
				min = UINTMAX_MAX;
			}
			max = 0;
		} else if (isdigit(p[i])){
			if (max > cutoff || (max == cutoff && (p[i] - '0') > cutlim)) {
				errno = ERANGE;
				return UINTMAX_MAX;
			} else {
				max *= (uintmax_t)10;
				max += (p[i] - '0');
			}
		}
	}
	return gtid == max || (gtid >= min && gtid <= max) ? max : 0;
}
/* }}} */

/* {{{ mysqlnd_ms_aux_gtid_extract */
static uintmax_t
mysqlnd_ms_aux_gtid_extract(const char * gtid)
{
	char * p = strchr(gtid, ':');
	return strtoumax(p ? p + 1 : gtid, NULL, 10);
}
/* }}} */


/* {{{ mysqlnd_ms_aux_gtid_chk_exec */
static enum_func_status
mysqlnd_ms_aux_gtid_chk_exec(const char * gtid_set, const char * gtid)
{
	char * p = strchr(gtid, ':');
	size_t len = p ? p - gtid : strlen(gtid);
	DBG_ENTER("mysqlnd_ms_aux_gtid_chk_exec");
	if (!strchr(gtid_set, ':') || !strchr(gtid, ':')) {
		uintmax_t ngtid = mysqlnd_ms_aux_gtid_extract(gtid);
		uintmax_t lgtid = mysqlnd_ms_aux_gtid_extract(gtid_set);
		DBG_INF_FMT("No colons found uuid gtid_set %s, gtid %s, p %s gtid %" PRIu64 " last %" PRIu64, gtid_set, gtid, p, ngtid, lgtid);
		if (errno == ERANGE || ngtid == UINTMAX_MAX || lgtid == UINTMAX_MAX || ngtid > lgtid) {
			DBG_RETURN(FAIL);
		}
		DBG_RETURN(PASS);
	} else if (p = mysqlnd_ms_aux_gtid_strnstr(gtid_set, gtid, len)) {
		uintmax_t ngtid = mysqlnd_ms_aux_gtid_extract(gtid);
		uintmax_t lgtid = mysqlnd_ms_aux_gtid_last_in_set(p+len+1, strlen(p+len+1), ngtid);
		DBG_INF_FMT("Found uuid gtid_set %s, gtid %s, p %s gtid %" PRIu64 " last %" PRIu64, gtid_set, gtid, p, ngtid, lgtid);
		if (errno == ERANGE || ngtid == UINTMAX_MAX || lgtid == UINTMAX_MAX || ngtid > lgtid) {
			DBG_RETURN(FAIL);
		}
		DBG_RETURN(PASS);
	} else {
		DBG_INF_FMT("No gtid extracted %s %s", gtid_set, gtid);
	}
	DBG_RETURN(FAIL);
}
/* }}} */

/* {{{ mysqlnd_ms_aux_gtid_chk_last */
PHP_MYSQLND_MS_API enum_func_status
mysqlnd_ms_aux_gtid_chk_last(const char * last_gtid, size_t last_gtid_len,
		const char * gtid, size_t gtid_len)
{
	DBG_ENTER("mysqlnd_ms_aux_gtid_chk_last");
	if (last_gtid && last_gtid_len == gtid_len && memcmp(last_gtid, gtid, gtid_len) == 0) {
		DBG_RETURN(PASS);
	} else if (gtid && gtid_len && last_gtid && last_gtid_len) {
		enum_func_status ret = mysqlnd_ms_aux_gtid_chk_exec(last_gtid, gtid);
		DBG_RETURN(ret);
	}
	DBG_RETURN(FAIL);
}
/* }}} */

/* {{{ mysqlnd_ms_aux_gtid_get_last */
static enum_func_status
mysqlnd_ms_aux_gtid_get_last(MYSQLND_MS_LIST_DATA * gtid_conn_elm, char ** gtid TSRMLS_DC)
{
	MYSQLND_CONN_DATA * conn = gtid_conn_elm->conn;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
  	enum_func_status ret = FAIL;
	MYSQLND_RES * res = NULL;
	zval _ms_p_zval row;
	zval _ms_p_zval * zgtid;
	DBG_ENTER("mysqlnd_ms_aux_gtid_get_last");
	DBG_INF_FMT("gtid_get_last for server %s %u %s", MYSQLND_MS_CONN_STRING(gtid_conn_elm->host), gtid_conn_elm->port, MYSQLND_MS_CONN_STRING(gtid_conn_elm->socket));
	if ((_MS_CONN_GET_STATE(conn) == CONN_ALLOCED && PASS != mysqlnd_ms_lazy_connect(gtid_conn_elm, (*conn_data)->global_trx.is_master TSRMLS_CC))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Error failed lazy connection.");
		DBG_RETURN(ret);
	}
	if (!gtid && (*conn_data)->global_trx.last_gtid) {
		mnd_pefree((*conn_data)->global_trx.last_gtid, conn->persistent);
		(*conn_data)->global_trx.last_gtid = NULL;
		(*conn_data)->global_trx.last_gtid_len = 0;
	}
	if (PASS == MS_CALL_ORIGINAL_CONN_DATA_METHOD(send_query)(conn, (*conn_data)->global_trx.fetch_last_gtid, (*conn_data)->global_trx.fetch_last_gtid_len _MS_SEND_QUERY_AD_EXT TSRMLS_CC)
			&& PASS == MS_CALL_ORIGINAL_CONN_DATA_METHOD(reap_query)(conn _MS_REAP_QUERY_AD_EXT TSRMLS_CC) &&
#if PHP_VERSION_ID < 50600
			(res = MS_CALL_ORIGINAL_CONN_DATA_METHOD(store_result)(conn TSRMLS_CC))) {
#else
			(res = MS_CALL_ORIGINAL_CONN_DATA_METHOD(store_result)(conn, MYSQLND_STORE_NO_COPY TSRMLS_CC))) {
#endif
		MAKE_STD_ZVAL(row);
		mysqlnd_fetch_into(res, MYSQLND_FETCH_NUM, _ms_a_zval row, MYSQLND_MYSQL);
		DBG_INF_FMT("fetch last gtid row type %d", Z_TYPE(_ms_p_zval row));
		if (Z_TYPE(_ms_p_zval row) == IS_ARRAY && SUCCESS == _MS_HASH_GET_ZR_FUNC_PTR_1(zend_hash_index_find, Z_ARRVAL(_ms_p_zval row), 0, zgtid) && Z_TYPE_P(_ms_p_zval zgtid) == IS_STRING) {
			if (!gtid) {
				(*conn_data)->global_trx.last_gtid = mnd_pestrndup(Z_STRVAL_P(_ms_p_zval zgtid), Z_STRLEN_P(_ms_p_zval zgtid), conn->persistent);
				(*conn_data)->global_trx.last_gtid_len = Z_STRLEN_P(_ms_p_zval zgtid);
				DBG_INF_FMT("fetch global_trx.last_gtid %s", (*conn_data)->global_trx.last_gtid);
			} else {
				*gtid = mnd_pestrndup(Z_STRVAL_P(_ms_p_zval zgtid), Z_STRLEN_P(_ms_p_zval zgtid), conn->persistent);
				DBG_INF_FMT("fetch last SQL gtid %s", *gtid);
			}
		} else {
			DBG_INF("Failed to read gtid from SQL");
		}
		ret = PASS;
		_ms_zval_dtor(row);
		res->m.free_result(res, FALSE TSRMLS_CC);
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Error on gtid SQL connection.");
	}
	DBG_INF_FMT("ret=%s", ret == PASS? "PASS":"FAIL");
	DBG_RETURN(ret);
}
/* }}} */
/* {{{ mysqlnd_ms_aux_gtid_server_stage1 */
enum_func_status
mysqlnd_ms_aux_gtid_server_stage1(MYSQLND_CONN_DATA * conn, MYSQLND_MS_CONN_DATA ** conn_data, const char * chkquery, size_t chkquery_len TSRMLS_DC)
{
	enum_func_status ret;

	DBG_ENTER("mysqlnd_ms_aux_gtid_server_stage1");

	(*conn_data)->skip_ms_calls = TRUE;

	ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(send_query)(conn, chkquery , chkquery_len _MS_SEND_QUERY_AD_EXT TSRMLS_CC);

	(*conn_data)->skip_ms_calls = FALSE;


	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_aux_gtid_server_stage2 */
static enum_func_status
mysqlnd_ms_aux_gtid_server_stage2(MYSQLND_CONN_DATA * conn, MYSQLND_MS_CONN_DATA ** conn_data, char **retval TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	MYSQLND_RES * res = NULL;
	zval _ms_p_zval * zret;

	DBG_ENTER("mysqlnd_ms_aux_gtid_server_stage2");

	(*conn_data)->skip_ms_calls = TRUE;

	if ((PASS == MS_CALL_ORIGINAL_CONN_DATA_METHOD(reap_query)(conn _MS_REAP_QUERY_AD_EXT TSRMLS_CC)) &&
#if PHP_VERSION_ID < 50600
		(res = MS_CALL_ORIGINAL_CONN_DATA_METHOD(store_result)(conn TSRMLS_CC))
#else
		(res = MS_CALL_ORIGINAL_CONN_DATA_METHOD(store_result)(conn, MYSQLND_STORE_NO_COPY TSRMLS_CC))
#endif
	)
	{
		zval _ms_p_zval row;
		MAKE_STD_ZVAL(row);
		mysqlnd_fetch_into(res, MYSQLND_FETCH_NUM, _ms_a_zval row, MYSQLND_MYSQL);
		DBG_INF_FMT("fetch stage2 row type %d", Z_TYPE_P(_ms_a_zval row));
		if (Z_TYPE_P(_ms_a_zval row) == IS_ARRAY && SUCCESS == _MS_HASH_GET_ZR_FUNC_PTR_1(zend_hash_index_find, Z_ARRVAL_P(_ms_a_zval row), 0, zret) && Z_TYPE_P(_ms_p_zval zret) == IS_STRING) {
			if (retval) {
				*retval = mnd_pestrndup(Z_STRVAL_P(_ms_p_zval zret), Z_STRLEN_P(_ms_p_zval zret), conn->persistent);
				DBG_INF_FMT("fetch stage2 returned %s", *retval);
			}
			ret = PASS;
		} else {
			DBG_INF("Failed to read gtid from SQL");
		}
		_ms_zval_ptr_dtor(_ms_a_zval row);
		res->m.free_result(res, FALSE TSRMLS_CC);
		res = NULL;
	}

	(*conn_data)->skip_ms_calls = FALSE;

	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_aux_gtid_add_active */
static void
mysqlnd_ms_aux_gtid_add_active(MYSQLND_CONN_DATA * conn, zend_llist * server_list, zend_llist * selected_servers, zend_bool is_write TSRMLS_DC)
{
	MYSQLND_MS_LIST_DATA * element;
	zend_llist stage1_slaves;
	zend_llist_init(&stage1_slaves, sizeof(MYSQLND_MS_LIST_DATA *), NULL /*dtor*/, 0);
	const char *query = is_write ? TEST_ACTIVE_WQUERY : TEST_ACTIVE_QUERY;
	size_t query_len = is_write ? sizeof(TEST_ACTIVE_WQUERY) - 1 : sizeof(TEST_ACTIVE_QUERY) - 1;
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, conn);
	DBG_ENTER("mysqlnd_ms_aux_gtid_add_active");
	if ((*proxy_conn_data)->global_trx.fetch_last_gtid_len) {
		query_len = (*proxy_conn_data)->global_trx.fetch_last_gtid_len;
		query = (*proxy_conn_data)->global_trx.fetch_last_gtid;
	}
	/* Stage 1 - just fire the queries and forget them for a moment */
	BEGIN_ITERATE_OVER_SERVER_LIST(element, server_list)
		MYSQLND_CONN_DATA * connection = element->conn;
		MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, connection);
		if (!conn_data || !*conn_data) {
			continue;
		}

		if ((_MS_CONN_GET_STATE(connection) != CONN_QUIT_SENT) &&
			(
				(_MS_CONN_GET_STATE(connection) > CONN_ALLOCED) ||
				(PASS == mysqlnd_ms_lazy_connect(element, TRUE TSRMLS_CC))
			))
		{

			DBG_INF_FMT("Checking connection "MYSQLND_LLU_SPEC"", connection->thread_id);

			if (PASS == mysqlnd_ms_aux_gtid_server_stage1(connection, conn_data, query, query_len TSRMLS_CC)) {
				zend_llist_add_element(&stage1_slaves, &element);
			} else if (connection->error_info->error_no) {
				mysqlnd_ms_client_n_php_error(NULL, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " SQL error while checking active server at stage 1: %d/'%s'",
						connection->error_info->error_no, connection->error_info->error);
			}
		}
	END_ITERATE_OVER_SERVER_LIST;
	/* Stage 2 - Now, after all servers have something to do, try to fetch the result, in the same order */
	BEGIN_ITERATE_OVER_SERVER_LIST(element, &stage1_slaves)
		enum_func_status ret = FAIL;
		MYSQLND_CONN_DATA * connection = element->conn;
		MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, connection);

		if ((*conn_data)->global_trx.fetch_last_gtid_len && (*conn_data)->global_trx.last_gtid) {
			mnd_pefree((*conn_data)->global_trx.last_gtid, conn->persistent);
			(*conn_data)->global_trx.last_gtid = NULL;
			(*conn_data)->global_trx.last_gtid_len = 0;
		}
		ret = mysqlnd_ms_aux_gtid_server_stage2(connection, conn_data, (*conn_data)->global_trx.fetch_last_gtid_len ? &(*conn_data)->global_trx.last_gtid : NULL TSRMLS_CC);
		if (ret == PASS && (*conn_data)->global_trx.fetch_last_gtid_len && (*conn_data)->global_trx.last_gtid) {
			(*conn_data)->global_trx.last_gtid_len = strlen((*conn_data)->global_trx.last_gtid);
		}
		if (connection->error_info->error_no) {
			mysqlnd_ms_client_n_php_error(NULL, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
								MYSQLND_MS_ERROR_PREFIX " SQL error while checking active server at stage 2 (%d): %d/'%s'",
								ret, connection->error_info->error_no, connection->error_info->error);
			continue;
		}

		if (ret == PASS) {
			zend_llist_add_element(selected_servers, &element);
		}
	END_ITERATE_OVER_SERVER_LIST;
	zend_llist_clean(&stage1_slaves);
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_aux_gtid_choose_connection */
static void
mysqlnd_ms_aux_gtid_choose_connection(MYSQLND_CONN_DATA * conn, const char * gtid,
								 zend_llist * server_list, zend_llist * selected_servers, zend_bool is_write TSRMLS_DC)
{
	MYSQLND_MS_LIST_DATA * element;

	MYSQLND_MS_DBG_ENTER("mysqlnd_ms_aux_gtid_choose_connection");
	// If there is no gtid and we have multiple masters or slaves then add only actives.
	if (zend_llist_count(server_list) > 1 && (!gtid || !strcmp(gtid, "0"))) {
		mysqlnd_ms_aux_gtid_add_active(conn, server_list, selected_servers, is_write TSRMLS_CC);
		MYSQLND_MS_DBG_VOID_RETURN;
	}
	BEGIN_ITERATE_OVER_SERVER_LIST(element, server_list)
		MYSQLND_CONN_DATA * connection = element->conn;
		MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, connection);
		if (!conn_data || !*conn_data) {
			continue;
		}
		if ((*conn_data)->global_trx.last_ckgtid) {
			mnd_pefree((*conn_data)->global_trx.last_ckgtid, connection->persistent);
			(*conn_data)->global_trx.last_ckgtid = NULL;
			(*conn_data)->global_trx.last_ckgtid_len = 0;
		}
		(*conn_data)->global_trx.last_ckgtid_len = gtid ? strlen(gtid) : 0;
		(*conn_data)->global_trx.last_ckgtid = gtid ? mnd_pestrndup(gtid, strlen(gtid), connection->persistent) : NULL;
		if (!gtid || !strcmp(gtid, "0")) {
			MYSQLND_MS_DBG_INF_FMT("Empty gtid %s, valid server rgtid %s wgtid %s %s %u %s", gtid, (*conn_data)->global_trx.last_gtid, (*conn_data)->global_trx.last_wgtid, MYSQLND_MS_CONN_STRING(element->host), element->port, MYSQLND_MS_CONN_STRING(element->socket));
			zend_llist_add_element(selected_servers, &element);
		} else if (_MS_CONN_GET_STATE(element->conn) != CONN_QUIT_SENT &&
				_MS_CONN_GET_STATE(element->conn) > CONN_ALLOCED && (mysqlnd_ms_aux_gtid_chk_last((*conn_data)->global_trx.last_gtid, (*conn_data)->global_trx.last_gtid_len, gtid, strlen(gtid)) == PASS
				|| mysqlnd_ms_aux_gtid_chk_last((*conn_data)->global_trx.last_wgtid, (*conn_data)->global_trx.last_wgtid_len, gtid, strlen(gtid)) == PASS)) {
			MYSQLND_MS_DBG_INF_FMT("Gtid %s already checked, valid server rgtid %s wgtid %s %s %u %s", gtid, (*conn_data)->global_trx.last_gtid, (*conn_data)->global_trx.last_wgtid, MYSQLND_MS_CONN_STRING(element->host), element->port, MYSQLND_MS_CONN_STRING(element->socket));
			zend_llist_add_element(selected_servers, &element);
		} else if (PASS == MYSQLND_MS_GTID_CALL_PASS((*conn_data)->global_trx.m->gtid_get_last, element, NULL TSRMLS_CC) && PASS == mysqlnd_ms_aux_gtid_chk_last((*conn_data)->global_trx.last_gtid, (*conn_data)->global_trx.last_gtid_len, gtid, strlen(gtid))) {
			MYSQLND_MS_DBG_INF_FMT("Gtid %s found, valid server %s %s %u %s", gtid, (*conn_data)->global_trx.last_gtid, MYSQLND_MS_CONN_STRING(element->host), element->port, MYSQLND_MS_CONN_STRING(element->socket));
			zend_llist_add_element(selected_servers, &element);
		} else {
			MYSQLND_MS_DBG_INF_FMT("Gtid %s not found, invalid server %s %s %u %s", gtid, (*conn_data)->global_trx.last_gtid, MYSQLND_MS_CONN_STRING(element->host), element->port, MYSQLND_MS_CONN_STRING(element->socket));
		}
	END_ITERATE_OVER_SERVER_LIST;
	MYSQLND_MS_DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_aux_gtid_validate */
static MYSQLND_CONN_DATA *
mysqlnd_ms_aux_gtid_validate(MYSQLND_CONN_DATA * conn, zend_bool *retry, const char * query, size_t query_len TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, (*conn_data)->proxy_conn);
	MYSQLND_CONN_DATA * ret = NULL;
	DBG_ENTER("mysqlnd_ms_aux_gtid_validate");
	(*proxy_conn_data)->global_trx.m->gtid_validate = NULL;
	*retry = FALSE;
	if ((*proxy_conn_data)->global_trx.memcached_wkey_len > 0)  {
		memcached_st *memc = (*proxy_conn_data)->global_trx.memc;
		if (memc) {
			memcached_return_t rc;
			uint32_t flags;
			size_t master_len = 0;
			char * master = NULL;
			_ms_smart_type * hash_key = (*conn_data)->elm_pool_hash_key;
			master = memcached_get(memc, (*proxy_conn_data)->global_trx.memcached_wkey, (*proxy_conn_data)->global_trx.memcached_wkey_len, &master_len, &flags, &rc);
			if (rc == MEMCACHED_NOTFOUND) {
				rc = memcached_add(memc, (*proxy_conn_data)->global_trx.memcached_wkey, (*proxy_conn_data)->global_trx.memcached_wkey_len,
						hash_key->c, hash_key->len - 1, (time_t)(*proxy_conn_data)->global_trx.running_ttl, (uint32_t)0);
				DBG_INF_FMT("Add master key %s value %s return %d", (*proxy_conn_data)->global_trx.memcached_wkey, hash_key->c, rc);
				master = memcached_get(memc, (*proxy_conn_data)->global_trx.memcached_wkey, (*proxy_conn_data)->global_trx.memcached_wkey_len, &master_len, &flags, &rc);
			}
			if (rc == MEMCACHED_SUCCESS) {
				if  (master_len == (hash_key->len - 1) && strncmp(hash_key->c, master, master_len) == 0) {
					ret = conn;
				} else {
					zend_bool exists = FALSE, is_master = FALSE, is_active = FALSE, is_removed = FALSE;
					_ms_smart_type ph = {master, master_len + 1 ,  master_len + 1 }; // Include null in host hash key
					MYSQLND_MS_LIST_DATA * data;
					DBG_INF_FMT("Found master key %s value %s", (*proxy_conn_data)->global_trx.memcached_wkey, master);
					exists = (*proxy_conn_data)->pool->connection_exists((*proxy_conn_data)->pool, &ph, &data, &is_master, &is_active, &is_removed TSRMLS_CC);
					DBG_INF_FMT("hash_key=%s exists=%d is_master=%d is_active=%d is_removed=%d ", master, exists, is_master, is_active, is_removed);
					if (exists && is_active && !is_removed && is_master &&
						_MS_CONN_GET_STATE(data->conn) != CONN_QUIT_SENT &&
						(_MS_CONN_GET_STATE(data->conn) > CONN_ALLOCED || PASS == mysqlnd_ms_lazy_connect(data, TRUE TSRMLS_CC))) {
						ret = data->conn;
					}
				}
			}
		}
	}
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_aux_gtid_choose_write_master */
static void
mysqlnd_ms_aux_gtid_choose_write_master(MYSQLND_CONN_DATA * conn, zend_llist * master_list, zend_llist * selected_masters TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	DBG_ENTER("mysqlnd_ms_aux_gtid_choose_write_master");

	if ((*conn_data)->global_trx.memcached_wkey_len > 0)  {
		memcached_st *memc = (*conn_data)->global_trx.memc;
		if (memc) {
			memcached_return_t rc;
			uint32_t flags;
			size_t master_len = 0;
			char * master = NULL;
			master = memcached_get(memc, (*conn_data)->global_trx.memcached_wkey, (*conn_data)->global_trx.memcached_wkey_len, &master_len, &flags, &rc);
			if (rc == MEMCACHED_SUCCESS && master_len > 1) {
				zend_bool exists = FALSE, is_master = FALSE, is_active = FALSE, is_removed = FALSE;
				_ms_smart_type ph = {master, master_len + 1 ,  master_len + 1 }; // Include null in host hash key
				MYSQLND_MS_LIST_DATA * data;
				DBG_INF_FMT("Found master key %s value %s", (*conn_data)->global_trx.memcached_wkey, master);
				exists = (*conn_data)->pool->connection_exists((*conn_data)->pool, &ph, &data, &is_master, &is_active, &is_removed TSRMLS_CC);
				DBG_INF_FMT("hash_key=%s exists=%d is_master=%d is_active=%d is_removed=%d ", master, exists, is_master, is_active, is_removed);
				if (exists && is_active && !is_removed && is_master &&
					_MS_CONN_GET_STATE(data->conn) != CONN_QUIT_SENT &&
					(_MS_CONN_GET_STATE(data->conn) > CONN_ALLOCED || PASS == mysqlnd_ms_lazy_connect(data, TRUE TSRMLS_CC))) {
					if ((*conn_data)->global_trx.race_avoid_strategy) {
						zend_llist stage1_servers;
						zend_llist_init(&stage1_servers, sizeof(MYSQLND_MS_LIST_DATA *), NULL /*dtor*/, 0);
						zend_llist_add_element(&stage1_servers, &data);
						mysqlnd_ms_aux_gtid_add_active(conn, &stage1_servers, selected_masters, TRUE TSRMLS_CC);
						zend_llist_clean(&stage1_servers);
					} else {
						zend_llist_add_element(selected_masters, &data);
					}
				}
				if (zend_llist_count(selected_masters) < 1) { // Possible race condition this is an hack to reduce it
					char * master1 = memcached_get(memc, (*conn_data)->global_trx.memcached_wkey, (*conn_data)->global_trx.memcached_wkey_len, &master_len, &flags, &rc);
					if  (rc == MEMCACHED_SUCCESS && master_len == (ph.len - 1) && !strncmp(ph.c, master1, master_len)) {
						memcached_delete(memc, (*conn_data)->global_trx.memcached_wkey, (*conn_data)->global_trx.memcached_wkey_len, (time_t)0);
					}
					if (master1) free(master1);
				} else {
					memcached_touch(memc, (*conn_data)->global_trx.memcached_wkey, (*conn_data)->global_trx.memcached_wkey_len, (time_t)(*conn_data)->global_trx.running_ttl);
				}
			}
			if (zend_llist_count(selected_masters) < 1) {
				(*conn_data)->global_trx.m->gtid_validate = mysqlnd_ms_aux_gtid_validate;
				mysqlnd_ms_aux_gtid_add_active(conn, master_list, selected_masters, TRUE TSRMLS_CC);
			}
			if (master) free(master);
		}
	}
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_aux_gtid_filter */
static void
mysqlnd_ms_aux_gtid_filter(MYSQLND_CONN_DATA * conn, const char * gtid, char ** query, size_t * query_len, zend_bool  * free_query,
								 zend_llist * slave_list, zend_llist * master_list, zend_llist * selected_slaves, zend_llist * selected_masters, zend_bool is_write TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	unsigned int wait_time = (*conn_data)->global_trx.wait_for_gtid_timeout;
	uint64_t total_time = 0, run_time = 0, my_wait_time = wait_time * 1000000;
	DBG_ENTER("mysqlnd_ms_aux_gtid_filter");
#ifdef MYSQLND_MS_HAVE_MYSQLND_QC
	if (!is_write) {
		zend_bool is_cached = mysqlnd_ms_aux_gtid_check_and_cache(conn, gtid, query, query_len, slave_list, master_list, selected_slaves, selected_masters TSRMLS_CC);
		*free_query = TRUE;
		if (is_cached) {
			DBG_VOID_RETURN;
		}
	}
#endif
	if (!is_write && zend_llist_count(slave_list))
	{
		if (wait_time) {
			MS_TIME_SET(run_time);
		}
		do {
			mysqlnd_ms_aux_gtid_choose_connection(conn, gtid, slave_list, selected_slaves, is_write TSRMLS_CC);
			if (wait_time && !zend_llist_count(selected_slaves)) {
				MS_TIME_DIFF(run_time);
				total_time += run_time;
				if (my_wait_time > total_time) {
					/*
					Server has not caught up yet but we are told to wait (throttle ourselves)
					and there is wait time left. NOTE: If the user is using any kind of SQL-level waits
					we will not notice and loop until the external
					*/
					DBG_INF_FMT("sleep and retry, time left=" MYSQLND_LLU_SPEC, (my_wait_time - total_time));
					MS_TIME_SET(run_time);
	#ifdef PHP_WIN32
					Sleep(1);
	#else
					sleep(1);
	#endif
					continue;
				}
			}
			break;
		} while (1);
	}
	if (zend_llist_count(master_list) > 1) {
		if (is_write && MYSQLND_MS_G(multi_master) && (*conn_data)->global_trx.injectable_query && (*conn_data)->global_trx.memc && (*conn_data)->global_trx.memcached_wkey_len) {
			mysqlnd_ms_aux_gtid_choose_write_master(conn, master_list, selected_masters);
		} else {
			mysqlnd_ms_aux_gtid_choose_connection(conn, gtid, master_list, selected_masters, is_write TSRMLS_CC);
		}
	} else {
		MYSQLND_MS_LIST_DATA * element;
		BEGIN_ITERATE_OVER_SERVER_LIST(element, master_list)
			zend_llist_add_element(selected_masters, &element);
		END_ITERATE_OVER_SERVER_LIST;
	}
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_aux_gtid_trace */
static void
mysqlnd_ms_aux_gtid_trace(MYSQLND_CONN_DATA * conn, const char * key, size_t key_len, unsigned int ttl, const char * query, size_t query_len TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, (*conn_data)->proxy_conn);
	struct st_mysqlnd_ms_global_trx_injection * trx = &(*conn_data)->global_trx;
	_ms_smart_type * hash_key = (*conn_data)->elm_pool_hash_key;
    char th[40];
	size_t thl = snprintf(th, 40, "%llu:%llu:%llu", (*proxy_conn_data)->global_trx.owned_wtoken, (*proxy_conn_data)->global_trx.owned_token, (*conn_data)->proxy_conn->thread_id);
	size_t l = hash_key->len + thl + 1 + trx->memcached_key_len + 1 + trx->memcached_wkey_len + 1 + trx->last_wgtid_len + 1 + trx->last_gtid_len + 1 + trx->last_ckgtid_len + 1 + query_len + 1;
    char * ret, * val;
	char ot[MAXGTIDSIZE];
	memcached_st *memc = (*proxy_conn_data)->global_trx.memc;
	memcached_return_t rc;
	uint64_t value = 0;
	size_t ol = 0;
	DBG_ENTER("mysqlnd_ms_aux_gtid_trace");
	if (!memc) {
		DBG_VOID_RETURN;
	}
	if ((rc = memcached_increment(memc, key, key_len, 1, &value)) != MEMCACHED_SUCCESS) {
		rc = memcached_add(memc, key, key_len, "0", 1, (time_t)0, (uint32_t)0);
		rc = memcached_increment(memc, key, key_len, 1, &value);
	}
	if (rc == MEMCACHED_SUCCESS) {
		ret = val = emalloc(l);
		memcpy(val, hash_key->c, hash_key->len - 1);
		val += hash_key->len - 1; //hash_key->len include null termination
		*val = GTID_GTID_MARKER;
		val++;
		if (trx->memcached_key_len)
			memcpy(val, trx->memcached_key, trx->memcached_key_len);
		val +=trx->memcached_key_len;
		*val = GTID_GTID_MARKER;
		val++;
		if (thl)
			memcpy(val, th, thl);
		val += thl;
		*val = GTID_GTID_MARKER;
		val++;
		if (trx->memcached_wkey_len)
			memcpy(val, trx->memcached_wkey, trx->memcached_wkey_len);
		val +=trx->memcached_wkey_len;
		*val = GTID_GTID_MARKER;
		val++;
		if (trx->last_gtid_len)
			memcpy(val, trx->last_gtid, trx->last_gtid_len);
		val += trx->last_gtid_len;
		*val = GTID_GTID_MARKER;
		val++;
		if (trx->last_wgtid_len)
			memcpy(val, trx->last_wgtid, trx->last_wgtid_len);
		val += trx->last_wgtid_len;
		*val = GTID_GTID_MARKER;
		val++;
		if (trx->last_ckgtid_len)
			memcpy(val, trx->last_ckgtid, trx->last_ckgtid_len);
		val += trx->last_ckgtid_len;
		*val = GTID_GTID_MARKER;
		val++;
		if (query_len)
			memcpy(val, query, query_len);
		val += query_len;
		*val = 0;
		ol = snprintf(ot, MAXGTIDSIZE, "%s:%" PRIuMAX, key, value);
		rc = memcached_set(memc, ot, ol, ret, strlen(ret), (time_t)ttl, (uint32_t)0);
		efree(ret);
	}
	DBG_VOID_RETURN;
}
/* }}} */

static unsigned uint_len(uint64_t n)
{
    if (n < 10) return 1;
    return 1 + uint_len(n / 10);
}

static uint64_t umodule(int64_t a, uint64_t module)
{
	int64_t ret = module ? a % module : a;
	if(ret < 0)
		ret+=module;
	return (uint64_t)ret;
}

/* {{{ mysqlnd_ms_aux_ss_gtid_build_val */
static char *
mysqlnd_ms_aux_ss_gtid_build_val(MYSQLND_MS_CONN_DATA * conn_data, const char *gtid, const char * query, size_t query_len)
{
	struct st_mysqlnd_ms_global_trx_injection * trx = &conn_data->global_trx;
	_ms_smart_type * hash_key = conn_data->elm_pool_hash_key;
	size_t gl = strlen(gtid);
	const char * g = gtid;
    char th[80];
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, conn_data->proxy_conn);
	size_t thl = snprintf(th, 80, "%llu:%llu:%llu:%llu:%llu:%llu:%llu|", conn_data->proxy_conn->thread_id,
			(*proxy_conn_data)->global_trx.prev_wfound, (*proxy_conn_data)->global_trx.last_chk_wtoken, (*proxy_conn_data)->global_trx.owned_wtoken,
			(*proxy_conn_data)->global_trx.prev_found, (*proxy_conn_data)->global_trx.last_chk_token, (*proxy_conn_data)->global_trx.owned_token);
	size_t l = hash_key->len + gl + 1 + trx->last_gtid_len + 1 + trx->last_ckgtid_len + 1 + thl + 1 + query_len + 1;
    char * ret, * val;
    DBG_ENTER("mysqlnd_ms_aux_ss_gtid_build_val");
    ret = val = malloc(l);
	memcpy(val, hash_key->c, hash_key->len - 1);
	val += hash_key->len - 1; //hash_key->len include null termination
	*val = GTID_GTID_MARKER;
	val++;
	if (gl)
		memcpy(val, g, gl);
	val +=gl;
	*val = GTID_GTID_MARKER;
	val++;
	if (trx->last_ckgtid_len)
		memcpy(val, trx->last_ckgtid, trx->last_ckgtid_len);
	val += trx->last_ckgtid_len;
	// BEGIN TEMPORARY HACK
	*val = GTID_GTID_MARKER;
	val++;
	if (trx->last_gtid_len)
		memcpy(val, trx->last_gtid, trx->last_gtid_len);
	val += trx->last_gtid_len;
	*val = GTID_GTID_MARKER;
	val++;
	if (query_len)
		memcpy(val, query, query_len);
	val += query_len;
	*val = GTID_GTID_MARKER;
	val++;
	if (thl)
		memcpy(val, th, thl);
	val += thl;
	// END TEMPORARY HACK
	*val = 0;
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_aux_ss_gtid_mtoken */
static enum_func_status
mysqlnd_ms_aux_ss_gtid_mtoken(memcached_st *memc, const char *key, uint64_t *owned_token, uint64_t module, zend_bool inc)
{
	memcached_return_t rc = MEMCACHED_SUCCESS;
	size_t key_len = strlen(key);
	uint64_t token = 0;
	uint32_t flags;
	DBG_ENTER("mysqlnd_ms_aux_ss_gtid_mtoken");
	if (inc) {
		if ((rc = memcached_increment_by_key(memc, key, key_len, key, key_len, 1, &token)) == MEMCACHED_SUCCESS) {
			if (module && token == module + 1) {
				uint64_t t;
				rc = memcached_decrement_by_key(memc, key, key_len, key, key_len, module, &t);
			}
			DBG_INF_FMT("Increment key %s token %llu return %d", key, token, rc);
		}
	} else {
		size_t value_len;
		char *value = memcached_get_by_key(memc, key, key_len, key, key_len, &value_len, &flags, &rc);
		if (value && rc == MEMCACHED_SUCCESS) {
			token = strtoumax(value, NULL, 10);
			free(value);
		}
		DBG_INF_FMT("Get key %s token %llu return %d", key, token, rc);
	}
	if (rc == MEMCACHED_SUCCESS) {
		*owned_token = umodule(((int64_t)token  - 1), module);
	} else {
		DBG_INF_FMT("Something wrong increment returned %d token %llu",  rc, token);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Something wrong increment returned %d token %llu",  rc, token);
	}
	DBG_RETURN(rc == MEMCACHED_SUCCESS ? SUCCESS : FAIL);
}
/* }}} */

/* {{{ mysqlnd_ms_aux_ss_gtid_mget */
static enum_func_status
mysqlnd_ms_aux_ss_gtid_mget(memcached_st *memc, char **value, char **gtid, uint64_t *last_chk, const char *key,
		unsigned int depth, uint64_t token, uint64_t module, uint64_t *found, zend_bool use_get, zend_bool *is_gtid)
{
	enum_func_status ret = FAIL;
	memcached_return_t rc = MEMCACHED_SUCCESS;
	memcached_return_t rcf = MEMCACHED_SUCCESS;
	size_t key_len = strlen(key);
	size_t module_len = module ? uint_len(module) : uint_len(token) + uint_len(depth);
	size_t max_key_len = (key_len + 2 + module_len);
	unsigned int limit = module == 0 && token < depth ? token : depth + 1;
	void * mg = emalloc(sizeof(size_t) * limit + sizeof(char *) * limit + max_key_len * limit);
	size_t * keys_len = (size_t *) mg;
	char * * keys = (char * *) ((void *)keys_len + sizeof(size_t) * limit);
	char * pkey = (char *) ((void *)keys + sizeof(char *) * limit);
	unsigned int i = 0;
	unsigned int tlimit = depth == 0 ? 0 : limit;
	MYSQLND_MS_DBG_ENTER("mysqlnd_ms_aux_ss_gtid_mget");
	*value = NULL;
	if (limit > 0) {
		for (; i < limit; i++) {
			keys[i] = pkey + max_key_len * i;
			keys_len[i] = snprintf(keys[i], max_key_len, "%s:%" PRIuMAX, key, umodule((int64_t)token - tlimit + i, module));
			MYSQLND_MS_DBG_INF_FMT("Token %llu Key %d is %s", token, i, keys[i]);
		}
		if (use_get == FALSE) {
			rc = memcached_mget_by_key(memc, key, key_len, (const char * const*)keys, keys_len, limit);
		}
		if (rc == MEMCACHED_SUCCESS) {
			char * retval = NULL;
			size_t retval_len = 0;
			uint32_t flags;
			char * last_r = NULL;
			char * last_e = NULL;
			char * last_eg = NULL;
			uintmax_t max_e = 0;
			*found = 0;
			for (i = 0; i < limit; i++) {
				if (use_get == FALSE) {
					 //TODO: Here we assume mecached_fetch returns keys in same order of the mget array
					// but is this true in all memcached implementations?
					retval = memcached_fetch(memc, keys[i], &keys_len[i], &retval_len, &flags, &rcf);
					if (rcf == MEMCACHED_END || rcf == MEMCACHED_NOTFOUND) {
						break;
					}
				} else {
					retval = memcached_get_by_key(memc, key, key_len, keys[i], keys_len[i], &retval_len, &flags, &rcf);
				}
				if (retval && rcf == MEMCACHED_SUCCESS)
				{
					*last_chk = mysqlnd_ms_aux_gtid_extract(keys[i]);
					MYSQLND_MS_DBG_INF_FMT("Found counter %d Key %llu is %s value %s last_r %s last_e %s last_eg %s fetch result %d", i, *last_chk, keys[i], retval, last_r, last_e, last_eg, rcf);
					if (*retval == GTID_RUNNING_MARKER) {
						if (last_r)
							free(last_r);
						last_r = retval;
						retval = NULL;
						*found = *last_chk + 1;
					} else if (*retval == GTID_EXECUTED_MARKER) {
						char * tgid = strchr(retval, GTID_GTID_MARKER);
						if (tgid && *(tgid + 1)) {
							char * p = strchr(tgid + 1, GTID_GTID_MARKER);
							uintmax_t ngtid = 0;
							*p = 0;
							*tgid = 0;
							tgid++;
							if (*tgid) {
								if (last_e && strcmp(last_e, retval)) {
									max_e = 0;
								}
								ngtid = mysqlnd_ms_aux_gtid_extract(tgid);
								if (ngtid > max_e) {
									if (last_e)
										free(last_e);
									last_e = retval;
									last_eg = tgid;
									max_e = ngtid;
									retval = NULL;
								}
							} else if (!last_eg) { // if no effective gtid is set fallback to checked gtid
								tgid = strchr(p + 1, GTID_GTID_MARKER);
								if (tgid && *(tgid + 1)) {
									p = strchr(tgid + 1, GTID_GTID_MARKER);
									*p = 0;
									*tgid = 0;
									tgid++;
									if (*tgid) {
										if (last_e)
											free(last_e);
										last_e = retval;
										last_eg = tgid;
										retval = NULL;
									}
								}
							}
						}
					} else if (*retval == GTID_WAIT_MARKER && *found != 0) { //If we have a wait marker in the middle of found keys something is wrong
						php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Something wrong found wait token %s for key %s but it should not be there",  retval, keys[i]);
					}
					if (*found == 0)
						*found = *last_chk + 1;
					// TEMPORARY HACK
//					else if (*found + i != *last_chk)
//						*found = *last_chk;
					if (retval) {
						free(retval);
						retval = NULL;
					}
				} else {
					MYSQLND_MS_DBG_INF_FMT("Not found Key %d is %s last_r %s last_e %s last_eg %s fetch result %d", i, keys[i], last_r, last_e, last_eg, rcf);
					if (retval) {
						free(retval);
						retval = NULL;
					}
					if (*found != 0) {
						break;
					}
				}
			}
			if (*found == 0) {
				*last_chk = token > 0 ? umodule((int64_t)token - 1, module) : 0;
			} else {
				*last_chk = umodule((int64_t)*last_chk + 1, module);
			}
			*value = NULL;
			*gtid = NULL;
			*is_gtid = TRUE;
			if (last_r) {
				char * p = strchr(last_r, GTID_GTID_MARKER);
				if (p)
					*p = 0;
				*value = last_r;
				last_r = NULL;
				*is_gtid = FALSE;

			}
			if (last_e && last_eg && *last_eg) {
				size_t len = strlen(last_eg);
				*gtid = malloc(len + 1);
				strcpy(*gtid, last_eg);
				if (!last_r) {
					*value = last_e;
					last_e = NULL;
				}
			} 
			if (last_r)
				free(last_r);
			if (last_e)
				free(last_e);
			ret = PASS;
			MYSQLND_MS_DBG_INF_FMT("Return value %s gtid %s last_chk %llu depth %d mget result %d fetch result %d", *value, *gtid, *last_chk, depth, rc, rcf);
		}
	} else {
		*last_chk =  0;
		MYSQLND_MS_DBG_INF_FMT("Limit 0 return value %s gtid %s last_chk %llu depth %d token %llu", *value, *gtid, *last_chk, depth, token);
		ret = PASS;
	}
	if (mg)
		efree(mg);
	MYSQLND_MS_DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_aux_ss_gtid_madd */
static char *
mysqlnd_ms_aux_ss_gtid_madd(memcached_st *memc, char *value, const char *key, uint64_t last_chk_token, uint64_t token, uint64_t module, unsigned int ttl, uint64_t found)
{
	memcached_return_t rc = MEMCACHED_SUCCESS;
  	char * ret = NULL;
  	uint64_t i = found == 0 && token > 0 ? token - 1 : last_chk_token;
	size_t key_len = strlen(key);
	char ot[MAXGTIDSIZE];
	size_t l;
	size_t value_len = strlen(value);
	uint64_t limit = umodule(token + 1, module);
	uint32_t flags;
	DBG_ENTER("mysqlnd_ms_aux_ss_gtid_madd");
	DBG_INF_FMT("Init value %s key %s last_chk_token %llu token %llu limit %llu i %llu found %llu", value, key, last_chk_token, token, limit, i, found);
	if (i < limit) {
		if (found == 0 && token > 0)
			*value = GTID_WAIT_MARKER;
		for (; i != limit; i = umodule(++i, module)) {
			l = snprintf(ot, MAXGTIDSIZE, "%s:%" PRIuMAX, key, i);
			rc = memcached_add_by_key(memc,
					key, key_len,
					ot, l,
					value, value_len, ttl, (uint32_t)0);
			if (rc == MEMCACHED_NOTSTORED) {
				if (ret)
					free(ret);
				value = ret = memcached_get_by_key(memc, key, key_len, ot, l, &value_len, &flags, &rc);
				DBG_INF_FMT("Get token on add notstored %s value %s return %s rc %d", ot, value, ret, rc);
				if (ret && *ret == GTID_EXECUTED_MARKER)
					*ret = GTID_RUNNING_MARKER;
			} else if (found == 0 && token > 0) {
				DBG_INF_FMT("Added wait token %s value %s return %s rc %d", ot, value, ret, rc);
				*value = GTID_RUNNING_MARKER;
				found = 1;
			} else {
				DBG_INF_FMT("Add token %s value %s return %s rc %d", ot, value, ret, rc);
			}
		}
	}
	DBG_INF_FMT("Return value %s key %s last_chk_token %llu token %llu limit %llu i %llu found %d ret %s", value, key, last_chk_token, token, limit, i, found, ret);
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_aux_ss_gtid_validate */
static MYSQLND_CONN_DATA *
mysqlnd_ms_aux_ss_gtid_validate(MYSQLND_CONN_DATA * conn, zend_bool *retry, const char * query, size_t query_len TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, (*conn_data)->proxy_conn);
	MYSQLND_CONN_DATA * ret = conn;
  	zend_bool check;
  	zend_bool checkw;
	memcached_st *memc = (*proxy_conn_data)->global_trx.memc;
	DBG_ENTER("mysqlnd_ms_aux_ss_gtid_validate");
	(*proxy_conn_data)->global_trx.m->gtid_validate = NULL;
	*retry = FALSE;
	checkw = (*proxy_conn_data)->global_trx.memc && (*proxy_conn_data)->global_trx.memcached_wkey &&
			((*proxy_conn_data)->global_trx.running_wdepth > 0 || !(*proxy_conn_data)->global_trx.last_whost) ? TRUE : FALSE;
	check = (*proxy_conn_data)->global_trx.memc && (*proxy_conn_data)->global_trx.memcached_key && (*proxy_conn_data)->global_trx.running_depth ? TRUE : FALSE;
	if (checkw || check) {
		char * value = mysqlnd_ms_aux_ss_gtid_build_val(*conn_data, NULL,
				(*proxy_conn_data)->global_trx.auto_clean ? NULL : query, (*proxy_conn_data)->global_trx.auto_clean ? 0 : query_len);
		char * host = NULL;
		char * hostw = NULL;
		char * hostchk = NULL;
		if (checkw) {
			hostw = mysqlnd_ms_aux_ss_gtid_madd((*proxy_conn_data)->global_trx.memc, value,
					(*proxy_conn_data)->global_trx.memcached_wkey, (*proxy_conn_data)->global_trx.last_chk_wtoken,
					(*proxy_conn_data)->global_trx.owned_wtoken, (*proxy_conn_data)->global_trx.module,
					(*proxy_conn_data)->global_trx.running_ttl, (*proxy_conn_data)->global_trx.prev_wfound);
			DBG_INF_FMT("Validate try wvalue %s added wvalue %s", value, hostw);
			if (hostw && *hostw == GTID_WAIT_MARKER) {
				*retry = TRUE;
				ret = NULL;
				(*proxy_conn_data)->global_trx.executed = TRUE; // Do not set executed marker
				goto end;
			}
			if ((*proxy_conn_data)->global_trx.running_wdepth == 0 && !(*proxy_conn_data)->global_trx.last_whost) {
				(*proxy_conn_data)->global_trx.last_whost = mnd_pestrdup(hostw ? hostw : value, conn->persistent);
			}
		}
		if (check) {
			host = mysqlnd_ms_aux_ss_gtid_madd((*proxy_conn_data)->global_trx.memc, hostw ? hostw : value,
					(*proxy_conn_data)->global_trx.memcached_key, (*proxy_conn_data)->global_trx.last_chk_token,
					(*proxy_conn_data)->global_trx.owned_token, (*proxy_conn_data)->global_trx.module,
					(*proxy_conn_data)->global_trx.running_ttl, (*proxy_conn_data)->global_trx.prev_found);
			DBG_INF_FMT("Validate try value %s added value %s", value, host);
			if (!checkw && host && *host == GTID_WAIT_MARKER) {
				*retry = TRUE;
				ret = NULL;
				(*proxy_conn_data)->global_trx.executed = TRUE; // Do not set executed marker
				goto end;
			}
		}
		hostchk = hostw ? hostw : host;
		if (hostchk) {
			char * p = strchr(hostchk, GTID_GTID_MARKER);
			if (p)
				*p = 0;
			if  (strcmp((*conn_data)->elm_pool_hash_key->c, hostchk) != 0) {
				zend_bool exists = FALSE, is_master = FALSE, is_active = FALSE, is_removed = FALSE;
				size_t hl = strlen(hostchk);
				_ms_smart_type ph = {hostchk, hl + 1 ,  hl + 1 }; // Include null in host hash key
				MYSQLND_MS_LIST_DATA * data;
				exists = (*proxy_conn_data)->pool->connection_exists((*proxy_conn_data)->pool, &ph, &data, &is_master, &is_active, &is_removed TSRMLS_CC);
				DBG_INF_FMT("Change to host hash_key=%s exists=%d is_master=%d is_active=%d is_removed=%d ", hostchk, exists, is_master, is_active, is_removed);
				if (exists && is_active && !is_removed && is_master &&
					_MS_CONN_GET_STATE(data->conn) != CONN_QUIT_SENT &&
					(_MS_CONN_GET_STATE(data->conn) > CONN_ALLOCED || PASS == mysqlnd_ms_lazy_connect(data, TRUE TSRMLS_CC))) {
					ret = data->conn;
				}
			} else {
				DBG_INF_FMT("Hosts match %s %s", (*conn_data)->elm_pool_hash_key->c, hostchk);
			}
		}
end:
		if (value)
			free(value);
		if (host)
			free(host);
		if (hostw)
			free(hostw);
	}
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_aux_ss_gtid_filter */
static void
mysqlnd_ms_aux_ss_gtid_filter(MYSQLND_CONN_DATA * conn, const char * gtid, char ** query, size_t * query_len, zend_bool * free_query,
								 zend_llist * slave_list, zend_llist * master_list,
								 zend_llist * selected_slaves, zend_llist * selected_masters, zend_bool is_write TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
  	enum_func_status ret = SUCCESS;
  	zend_bool is_gtid = TRUE;
  	zend_bool check;
  	zend_bool checkw;
  	char * value = NULL;
  	char * valuew = NULL;
  	char * mgtid = NULL;
  	char * mgtidw = NULL;
  	char * host = NULL;
	MYSQLND_MS_DBG_ENTER("mysqlnd_ms_aux_ss_gtid_filter");
	MYSQLND_MS_DBG_INF_FMT("Query is %s gtid is %s", *query, gtid);
	if ((*conn_data)->proxy_conn != conn) {
		MS_LOAD_CONN_DATA(conn_data, (*conn_data)->proxy_conn);
	}
	(*conn_data)->global_trx.executed = FALSE;
	check = (*conn_data)->global_trx.memc && (*conn_data)->global_trx.memcached_key &&
			((*conn_data)->global_trx.running_depth > 0 || ((*conn_data)->global_trx.first_read && (!gtid || *gtid == 0))) ? TRUE : FALSE;
	if (check) {
		if (((*conn_data)->global_trx.running_depth <= 0 ||
				(ret = mysqlnd_ms_aux_ss_gtid_mtoken((*conn_data)->global_trx.memc, (*conn_data)->global_trx.memcached_key,
				&(*conn_data)->global_trx.owned_token, (*conn_data)->global_trx.module,
				(*conn_data)->global_trx.injectable_query && (*conn_data)->global_trx.running_depth)) == SUCCESS) &&
			(ret = mysqlnd_ms_aux_ss_gtid_mget((*conn_data)->global_trx.memc, &value, &mgtid, &(*conn_data)->global_trx.last_chk_token,
				(*conn_data)->global_trx.memcached_key, (*conn_data)->global_trx.running_depth,
				(*conn_data)->global_trx.owned_token, (*conn_data)->global_trx.module,
				&(*conn_data)->global_trx.prev_found, (*conn_data)->global_trx.use_get, &is_gtid)) == SUCCESS) {
			MYSQLND_MS_DBG_INF_FMT("For key %s increment token %llu last found %llu with value %s gtid %s", (*conn_data)->global_trx.memcached_key, (*conn_data)->global_trx.owned_token, (*conn_data)->global_trx.last_chk_token, value, mgtid);
			gtid = mgtid;
			host = value;
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Something wrong could not get owned token %s",  (*conn_data)->global_trx.memcached_key);
		}
	}
	if (!is_write && (*conn_data)->global_trx.first_read) {
		(*conn_data)->global_trx.first_read = FALSE;
		if (gtid)
			mysqlnd_ms_section_filters_set_gtid_qos(conn, gtid, strlen(gtid) TSRMLS_CC);
	}
	if ((*conn_data)->global_trx.injectable_query) {
		checkw = (*conn_data)->global_trx.memc && (*conn_data)->global_trx.memcached_wkey &&
				((*conn_data)->global_trx.running_wdepth > 0 || !(*conn_data)->global_trx.last_whost) ? TRUE : FALSE;
		if (checkw) {
			if (((*conn_data)->global_trx.running_wdepth <= 0 || (ret = mysqlnd_ms_aux_ss_gtid_mtoken((*conn_data)->global_trx.memc, (*conn_data)->global_trx.memcached_wkey,
					&(*conn_data)->global_trx.owned_wtoken, (*conn_data)->global_trx.module,
					(*conn_data)->global_trx.injectable_query && (*conn_data)->global_trx.running_wdepth)) == SUCCESS) &&
				(ret = mysqlnd_ms_aux_ss_gtid_mget((*conn_data)->global_trx.memc, &valuew, &mgtidw, &(*conn_data)->global_trx.last_chk_wtoken,
					(*conn_data)->global_trx.memcached_wkey, (*conn_data)->global_trx.running_wdepth,
					(*conn_data)->global_trx.owned_wtoken, (*conn_data)->global_trx.module,
					&(*conn_data)->global_trx.prev_wfound, (*conn_data)->global_trx.use_get, &is_gtid)) == SUCCESS) {
				MYSQLND_MS_DBG_INF_FMT("For wkey %s increment wtoken %llu last found %llu with value %s gtid %s", (*conn_data)->global_trx.memcached_wkey, (*conn_data)->global_trx.owned_wtoken, (*conn_data)->global_trx.last_chk_wtoken, valuew, mgtidw);
				gtid = mgtidw;
				host = valuew;
			} else {
				MYSQLND_MS_DBG_INF_FMT("Something wrong could not get owned token for key %s",  (*conn_data)->global_trx.memcached_wkey);
				php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Something wrong could not get owned token %s",  (*conn_data)->global_trx.memcached_wkey);
			}
		} else {
		  	is_gtid = (*conn_data)->global_trx.last_whost ? FALSE : TRUE;
		  	host = (*conn_data)->global_trx.last_whost;
		}
		if (checkw || (check && (*conn_data)->global_trx.running_depth)) {
			MYSQLND_MS_DBG_INF("Set running validate function");
			(*conn_data)->global_trx.m->gtid_validate = mysqlnd_ms_aux_ss_gtid_validate;
		}
	}
#ifdef MYSQLND_MS_HAVE_MYSQLND_QC
	if (!is_write) {
		zend_bool is_cached = mysqlnd_ms_aux_gtid_check_and_cache(conn, gtid, query, query_len, slave_list, master_list, selected_slaves, selected_masters TSRMLS_CC);
		*free_query = TRUE;
		if (is_cached) {
			ret = FAIL; // Hack to skip next if
		}
	}
#endif
	if (ret == SUCCESS) {
		MYSQLND_MS_LIST_DATA * data = NULL;
		if (host) {
			zend_bool exists = FALSE, is_master = FALSE, is_active = FALSE, is_removed = FALSE;
			size_t value_len = strlen(host) + 1; // Include null in host hash key
			_ms_smart_type ph = {(char *)host, value_len, value_len};
			exists = (*conn_data)->pool->connection_exists((*conn_data)->pool, &ph, &data, &is_master, &is_active, &is_removed TSRMLS_CC);
			MYSQLND_MS_DBG_INF_FMT("Get host from pool hash_key=%s exists=%d is_master=%d is_active=%d is_removed=%d ", host, exists, is_master, is_active, is_removed);
			if (exists && is_active && !is_removed && is_master &&
					_MS_CONN_GET_STATE(data->conn) != CONN_QUIT_SENT &&
					(_MS_CONN_GET_STATE(data->conn) > CONN_ALLOCED || PASS == mysqlnd_ms_lazy_connect(data, TRUE TSRMLS_CC))) {
				MS_DECLARE_AND_LOAD_CONN_DATA(dconn_data, data->conn);
				if ((*conn_data)->global_trx.race_avoid_strategy) {
					zend_llist stage1_servers;
					zend_llist_init(&stage1_servers, sizeof(MYSQLND_MS_LIST_DATA *), NULL /*dtor*/, 0);
					zend_llist_add_element(&stage1_servers, &data);
					mysqlnd_ms_aux_gtid_add_active(conn, &stage1_servers, selected_masters, TRUE TSRMLS_CC);
					zend_llist_clean(&stage1_servers);
				} else {
					zend_llist_add_element(selected_masters, &data);
				}
				if (gtid && *gtid) {
					if ((*dconn_data)->global_trx.last_ckgtid) {
						mnd_pefree((*dconn_data)->global_trx.last_ckgtid, data->conn->persistent);
					}
					(*dconn_data)->global_trx.last_ckgtid_len = strlen(gtid);
					(*dconn_data)->global_trx.last_ckgtid = mnd_pestrndup(gtid, strlen(gtid), data->conn->persistent);
				}
			}
		}
		if (is_gtid) {
			if (host && data) { // The master of the last gtid is already selected or is not available so we don't need to check it
				MYSQLND_MS_LIST_DATA * element;
				zend_llist check_masters;
				zend_llist_init(&check_masters, sizeof(MYSQLND_MS_LIST_DATA *), NULL /*dtor*/, 0);
				BEGIN_ITERATE_OVER_SERVER_LIST(element, master_list)
					if (element != data) {
						zend_llist_add_element(&check_masters, &element);
					}
				END_ITERATE_OVER_SERVER_LIST;
				mysqlnd_ms_aux_gtid_choose_connection(conn, gtid, &check_masters, selected_masters, is_write TSRMLS_CC);
				zend_llist_clean(&check_masters);
			} else {
				mysqlnd_ms_aux_gtid_choose_connection(conn, gtid, master_list, selected_masters, is_write TSRMLS_CC);
			}
			if (!is_write) {
				mysqlnd_ms_aux_gtid_choose_connection(conn, gtid, slave_list, selected_slaves, is_write TSRMLS_CC);
			}
		}
	}
	if (value) free(value);
	if (valuew) free(valuew);
	if (mgtid) free(mgtid);
	if (mgtidw) free(mgtidw);
	MYSQLND_MS_DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_aux_ss_gtid_set_last_write */
static enum_func_status
mysqlnd_ms_aux_ss_gtid_set_last_write(MYSQLND_CONN_DATA * connection, char * gtid TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, connection);
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, (*conn_data)->proxy_conn);
  	enum_func_status ret = PASS;
	size_t gl = strlen(gtid);
	char * g = gtid;
	DBG_ENTER("mysqlnd_ms_aux_ss_gtid_set_last_write");
	if ((*conn_data)->global_trx.last_wgtid) {
		mnd_pefree((*conn_data)->global_trx.last_wgtid, connection->persistent);
		(*conn_data)->global_trx.last_wgtid_len = 0;
	}
	(*conn_data)->global_trx.last_wgtid = mnd_pestrndup(gtid, strlen(gtid), connection->persistent);
	(*conn_data)->global_trx.last_wgtid_len = strlen(gtid);
	if ((*proxy_conn_data)->global_trx.memc && ((*proxy_conn_data)->global_trx.memcached_key_len > 0 ||
			((*proxy_conn_data)->global_trx.memcached_wkey_len > 0 && (*proxy_conn_data)->global_trx.running_wdepth > 0))) {
		memcached_st *memc = (*proxy_conn_data)->global_trx.memc;
		memcached_return_t rc = MEMCACHED_SUCCESS;
		char ot[MAXGTIDSIZE];
		char *val = mysqlnd_ms_aux_ss_gtid_build_val(*conn_data, gtid, NULL, 0);
	  	size_t l;
	  	size_t val_len = strlen(val);
		*val = GTID_EXECUTED_MARKER;
		(*proxy_conn_data)->global_trx.executed = TRUE;
		if ((*proxy_conn_data)->global_trx.memcached_wkey_len > 0 && (*proxy_conn_data)->global_trx.running_wdepth > 0) {
			l = snprintf(ot, MAXGTIDSIZE, "%s:%" PRIuMAX, (*proxy_conn_data)->global_trx.memcached_wkey, (*proxy_conn_data)->global_trx.owned_wtoken);
			if ((*proxy_conn_data)->global_trx.auto_clean) {
				rc = memcached_set_by_key(memc,
						(*proxy_conn_data)->global_trx.memcached_wkey, (*proxy_conn_data)->global_trx.memcached_wkey_len,
						ot, l,
						val, val_len, (time_t)(*proxy_conn_data)->global_trx.running_ttl, (uint32_t)0);
			} else {
				rc = memcached_prepend_by_key(memc,
						(*proxy_conn_data)->global_trx.memcached_wkey, (*proxy_conn_data)->global_trx.memcached_wkey_len,
						ot, l,
						val, val_len, (time_t)(*proxy_conn_data)->global_trx.running_ttl, (uint32_t)0);
			}
			DBG_INF_FMT("Set wkey %s value %s returned %d", ot, val, rc);
		}
		if (rc != MEMCACHED_SUCCESS) {
			ret = FAIL;
			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Error setting memcached last write %s %s.", ot, gtid);
		}
		if ((*proxy_conn_data)->global_trx.memcached_key_len > 0) {
			l = snprintf(ot, MAXGTIDSIZE, "%s:%" PRIuMAX, (*proxy_conn_data)->global_trx.memcached_key, (*proxy_conn_data)->global_trx.owned_token);
			if ((*proxy_conn_data)->global_trx.auto_clean || (*proxy_conn_data)->global_trx.running_depth == 0) {
				rc = memcached_set_by_key(memc,
						(*proxy_conn_data)->global_trx.memcached_key, (*proxy_conn_data)->global_trx.memcached_key_len,
						ot, l,
						val, val_len, (time_t)(*proxy_conn_data)->global_trx.running_ttl, (uint32_t)0);
			} else {
				rc = memcached_prepend_by_key(memc,
						(*proxy_conn_data)->global_trx.memcached_key, (*proxy_conn_data)->global_trx.memcached_key_len,
						ot, l,
						val, val_len, (time_t)(*proxy_conn_data)->global_trx.running_ttl, (uint32_t)0);

			}
			DBG_INF_FMT("Set key %s value %s returned %d", ot, val, rc);
		}
		if (rc != MEMCACHED_SUCCESS) {
			ret = FAIL;
			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Error setting memcached last write %s %s.", ot, gtid);
		}
		if (val)
			free(val);
	}
	if (mysqlnd_ms_section_filters_set_gtid_qos(connection, g, gl TSRMLS_CC) == FAIL)
		ret = FAIL;
	DBG_INF_FMT("ret=%s", ret == PASS? "PASS":"FAIL");
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_ss_gtid_connect */
static enum_func_status
mysqlnd_ms_aux_ss_gtid_connect(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_MS_LIST_DATA * new_element, zend_bool is_master,
                               zend_bool lazy_connections TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);
	struct st_mysqlnd_ms_global_trx_injection * global_trx = &(*proxy_conn_data)->global_trx;
	struct st_mysqlnd_ms_conn_credentials * cred =  &(*proxy_conn_data)->cred;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, new_element->conn);
	DBG_ENTER("mysqlnd_ms_ss_gtid_connect");
	if (is_master && (global_trx->memcached_key_len > 0 || global_trx->memcached_wkey_len > 0)) {
		memcached_st *memc = global_trx->memc;
		memcached_return_t rc;
		if (!memc)  {
			char * mchost = MYSQLND_MS_CONN_STRING(new_element->host);
			unsigned int mcport = 11211;
			if (global_trx->memcached) {
				memc = memcached(global_trx->memcached, global_trx->memcached_len);
				DBG_INF_FMT("Memcached config creation %s returns %p", global_trx->memcached, memc);
			} else {
				if (global_trx->memcached_port) {
					mcport = global_trx->memcached_port;
				}
				if (global_trx->memcached_host) {
					mchost = global_trx->memcached_host;
				}
				memc = memcached_create(NULL);
				if (memc) {
					rc = memcached_server_add(memc, mchost, mcport);
					if (rc != MEMCACHED_SUCCESS) {
						memcached_free(memc);
						php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Failed gtid memcached connect to host.");
						ret = FAIL;
					}
				}
			}
		}
		if (memc) {
			global_trx->memc = memc;
			if (global_trx->memcached_key_len > 0 && global_trx->running_depth > 0) {
				rc = memcached_add_by_key(memc,
						global_trx->memcached_key, global_trx->memcached_key_len,
						global_trx->memcached_key, global_trx->memcached_key_len,
						"0", 1, (time_t)0, (uint32_t)0);
				DBG_INF_FMT("Add key %s returned %d", global_trx->memcached_key, rc);
			}
			if (global_trx->memcached_wkey_len > 0 && global_trx->running_wdepth > 0) {
				rc = memcached_add_by_key(memc,
						global_trx->memcached_wkey, global_trx->memcached_wkey_len,
						global_trx->memcached_wkey, global_trx->memcached_wkey_len,
						"0", 1, (time_t)0, (uint32_t)0);
				DBG_INF_FMT("Add wkey %s returned %d", global_trx->memcached_wkey, rc);
			}
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Failed memcached creation.");
			ret = FAIL;
		}
	}
	DBG_INF_FMT("ret=%s", ret == PASS? "PASS":"FAIL");
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_aux_ss_gtid_clean */
static enum_func_status
mysqlnd_ms_aux_ss_gtid_clean(MYSQLND_CONN_DATA * conn, enum_func_status status TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, (*conn_data)->proxy_conn);
  	enum_func_status ret = PASS;
	char ot[MAXGTIDSIZE];
  	size_t l;
	DBG_ENTER("mysqlnd_ms_aux_ss_gtid_clean");
	if ((*proxy_conn_data)->global_trx.memc && (((*proxy_conn_data)->global_trx.memcached_key_len > 0 && (*proxy_conn_data)->global_trx.running_depth > 0)||
			((*proxy_conn_data)->global_trx.memcached_wkey_len > 0 && (*proxy_conn_data)->global_trx.running_wdepth > 0))) {
		memcached_st *memc = (*proxy_conn_data)->global_trx.memc;
		memcached_return_t rc = MEMCACHED_SUCCESS;
	  	char *val = NULL;
	  	if (!(*proxy_conn_data)->global_trx.executed) {
			val = mysqlnd_ms_aux_ss_gtid_build_val(*conn_data, NULL, NULL, 0);
		  	size_t val_len = strlen(val);
			*val = GTID_EXECUTED_MARKER;
			if ((*proxy_conn_data)->global_trx.memcached_wkey_len > 0  && (*proxy_conn_data)->global_trx.running_wdepth > 0) {
				l = snprintf(ot, MAXGTIDSIZE, "%s:%" PRIuMAX, (*proxy_conn_data)->global_trx.memcached_wkey, (*proxy_conn_data)->global_trx.owned_wtoken);
				if ((*proxy_conn_data)->global_trx.auto_clean) {
					rc = memcached_set_by_key(memc,
							(*proxy_conn_data)->global_trx.memcached_wkey, (*proxy_conn_data)->global_trx.memcached_wkey_len,
							ot, l,
							val, val_len, (time_t)(*proxy_conn_data)->global_trx.running_ttl, (uint32_t)0);

				} else {
					rc = memcached_prepend_by_key(memc,
							(*proxy_conn_data)->global_trx.memcached_wkey, (*proxy_conn_data)->global_trx.memcached_wkey_len,
							ot, l,
							val, val_len, (time_t)(*proxy_conn_data)->global_trx.running_ttl, (uint32_t)0);
				}
				DBG_INF_FMT("Prepend executed marker for wkey %s returned %d", ot, rc);
			}
			if (rc != MEMCACHED_SUCCESS) {
				ret = FAIL;
				php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Error setting memcached executed flag %s.", ot);
			}
			if ((*proxy_conn_data)->global_trx.memcached_key_len > 0 && (*proxy_conn_data)->global_trx.running_depth > 0) {
				l = snprintf(ot, MAXGTIDSIZE, "%s:%" PRIuMAX, (*proxy_conn_data)->global_trx.memcached_key, (*proxy_conn_data)->global_trx.owned_token);
				if ((*proxy_conn_data)->global_trx.auto_clean) {
					rc = memcached_set_by_key(memc,
							(*proxy_conn_data)->global_trx.memcached_key, (*proxy_conn_data)->global_trx.memcached_key_len,
							ot, l,
							val, val_len, (time_t)(*proxy_conn_data)->global_trx.running_ttl, (uint32_t)0);
				} else {
					rc = memcached_prepend_by_key(memc,
							(*proxy_conn_data)->global_trx.memcached_key, (*proxy_conn_data)->global_trx.memcached_key_len,
							ot, l,
							val, val_len, (time_t)(*proxy_conn_data)->global_trx.running_ttl, (uint32_t)0);

				}
				DBG_INF_FMT("Prepend executed marker for key %s returned %d", ot, rc);
			}
			if (rc != MEMCACHED_SUCCESS) {
				ret = FAIL;
				php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Error setting memcached executed flag %s.", ot);
			}
			if (val)
				free(val);
	  	} else {
	  		(*proxy_conn_data)->global_trx.executed = FALSE;
	  	}
	  	if ((*proxy_conn_data)->global_trx.auto_clean) {
	  		uint64_t token;
			if ((*proxy_conn_data)->global_trx.memcached_wkey_len > 0 && (*proxy_conn_data)->global_trx.running_wdepth > 0) {
				if ((*proxy_conn_data)->global_trx.module != 0 || (*proxy_conn_data)->global_trx.owned_wtoken >= (2 * (*proxy_conn_data)->global_trx.running_wdepth)) {
					token = umodule((*proxy_conn_data)->global_trx.owned_wtoken - (2 * (*proxy_conn_data)->global_trx.running_wdepth), (*proxy_conn_data)->global_trx.module);
					l = snprintf(ot, MAXGTIDSIZE, "%s:%" PRIuMAX, (*proxy_conn_data)->global_trx.memcached_wkey, token);
					rc = memcached_delete_by_key(memc,
							(*proxy_conn_data)->global_trx.memcached_wkey, (*proxy_conn_data)->global_trx.memcached_wkey_len,
							ot, l,
							(time_t)0);
					DBG_INF_FMT("Delete wkey %s returned %d", ot, rc);
				}
			}
			if ((*proxy_conn_data)->global_trx.memcached_key_len > 0  && (*proxy_conn_data)->global_trx.running_depth > 0) {
				if ((*proxy_conn_data)->global_trx.module != 0 || (*proxy_conn_data)->global_trx.owned_token >= (2 * (*proxy_conn_data)->global_trx.running_depth)) {
					token = umodule((*proxy_conn_data)->global_trx.owned_token - (2 * (*proxy_conn_data)->global_trx.running_depth), (*proxy_conn_data)->global_trx.module);
					l = snprintf(ot, MAXGTIDSIZE, "%s:%" PRIuMAX, (*proxy_conn_data)->global_trx.memcached_key, token);
					rc = memcached_delete_by_key(memc,
							(*proxy_conn_data)->global_trx.memcached_key, (*proxy_conn_data)->global_trx.memcached_key_len,
							ot, l,
							(time_t)0);
					DBG_INF_FMT("Delete key %s returned %d", ot, rc);
				}
			}
	  	}
	}
	// This is for simple write consistency
	if ((*proxy_conn_data)->global_trx.memc && (*proxy_conn_data)->global_trx.memcached_wkey_len > 0 && (*proxy_conn_data)->global_trx.running_wdepth == 0) {
		l = snprintf(ot, MAXGTIDSIZE, "%s:%" PRIuMAX, (*proxy_conn_data)->global_trx.memcached_wkey, (*proxy_conn_data)->global_trx.owned_wtoken);
		memcached_touch_by_key((*proxy_conn_data)->global_trx.memc, (*proxy_conn_data)->global_trx.memcached_wkey, (*proxy_conn_data)->global_trx.memcached_wkey_len, ot, l, (time_t)(*proxy_conn_data)->global_trx.running_ttl);
	}
	(*proxy_conn_data)->global_trx.owned_wtoken = 0;
	(*proxy_conn_data)->global_trx.owned_token = 0;
	DBG_INF_FMT("ret=%s", ret == PASS? "PASS":"FAIL");
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_aux_ss_gtid_inject_after */
static enum_func_status
mysqlnd_ms_aux_ss_gtid_inject_after(MYSQLND_CONN_DATA * conn, enum_func_status status TSRMLS_DC)
{
	enum_func_status ret = PASS;
	DBG_ENTER("mysqlnd_ms_aux_ss_gtid_inject_after");
	ret = mysqlnd_ms_aux_ss_gtid_clean(conn, status);
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_aux_ss_gtid_reset */
static void
mysqlnd_ms_aux_ss_gtid_reset(MYSQLND_CONN_DATA * conn, enum_func_status status TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_aux_ss_gtid_reset");
	mysqlnd_ms_aux_ss_gtid_clean(conn, status);
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_aux_ss_gtid_trace */
static void
mysqlnd_ms_aux_ss_gtid_trace(MYSQLND_CONN_DATA * conn, const char * key, size_t key_len, unsigned int ttl, const char * query, size_t query_len TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_aux_ss_gtid_trace");
	mysqlnd_ms_aux_gtid_trace(conn, key, key_len, ttl, query, query_len TSRMLS_CC);
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_aux_ss_gtid_get_last */
static enum_func_status
mysqlnd_ms_aux_ss_gtid_get_last(MYSQLND_MS_LIST_DATA * conn_elm, char ** gtid TSRMLS_DC)
{
  	enum_func_status ret = FAIL;
	DBG_ENTER("mysqlnd_ms_aux_ss_gtid_get_last");
	ret = mysqlnd_ms_aux_gtid_get_last(conn_elm, gtid TSRMLS_CC);
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_cs_gtid_get_last */
static enum_func_status
mysqlnd_ms_cs_gtid_get_last(MYSQLND_MS_LIST_DATA * gtid_conn_elm, char ** gtid TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, gtid_conn_elm->conn);
  	enum_func_status ret = FAIL;
	DBG_ENTER("mysqlnd_ms_cs_gtid_get_last");

	if (!gtid && (*conn_data)->global_trx.last_gtid) {
		mnd_pefree((*conn_data)->global_trx.last_gtid, gtid_conn_elm->conn->persistent);
		(*conn_data)->global_trx.last_gtid = NULL;
		(*conn_data)->global_trx.last_gtid_len = 0;
	}
	if ((*conn_data)->global_trx.memcached_key_len == 0 && (*conn_data)->global_trx.fetch_last_gtid_len > 0)  {
		if (!gtid_conn_elm || FAIL == (ret = mysqlnd_ms_aux_gtid_get_last(gtid_conn_elm, gtid))) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Error no gtid_conn_elm or failed lazy connection.");
			DBG_RETURN(ret);
		}
	}
	if ((*conn_data)->global_trx.memcached_key_len > 0)  {
		memcached_st *memc = (*conn_data)->global_trx.memc;
		if (memc) {
			memcached_return_t rc;
			uint32_t flags;
			size_t last_gtid_len = 0;
			char * mgtid = NULL;
			mgtid = memcached_get(memc, (*conn_data)->global_trx.memcached_key, (*conn_data)->global_trx.memcached_key_len, &last_gtid_len, &flags, &rc);
			if (rc == MEMCACHED_SUCCESS && last_gtid_len > 1) {
				if (!gtid) {
					(*conn_data)->global_trx.last_gtid = mnd_pestrndup(mgtid, strlen(mgtid), gtid_conn_elm->conn->persistent);
					(*conn_data)->global_trx.last_gtid_len = strlen(mgtid);
					DBG_INF_FMT("Fetch memcached global_trx.last_gtid %s", (*conn_data)->global_trx.last_gtid);
				} else {
					*gtid = mnd_pestrndup(mgtid, strlen(mgtid), gtid_conn_elm->conn->persistent);
					DBG_INF_FMT("Fetch last memcached gtid %s", *gtid);
				}
			} else {
				DBG_INF("Failed to read gtid from Memcached");
			}
			if (mgtid) free(mgtid);
			ret = PASS;
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Error on memcached connection.");
		}
	}
	DBG_INF_FMT("ret=%s", ret == PASS? "PASS":"FAIL");
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_cs_gtid_set_last_write */
static enum_func_status
mysqlnd_ms_cs_gtid_set_last_write(MYSQLND_CONN_DATA * connection, char * gtid TSRMLS_DC)
{
	enum_func_status ret = PASS;
	DBG_ENTER("mysqlnd_ms_cs_gtid_set_last_write");
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_cs_gtid_init */
static enum_func_status
mysqlnd_ms_cs_gtid_init(MYSQLND_CONN_DATA * proxy_conn TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);
	MYSQLND_MS_LIST_DATA * element;
	char * gtid = NULL;
	DBG_ENTER("mysqlnd_ms_cs_gtid_init");
	BEGIN_ITERATE_OVER_SERVER_LIST(element, (*proxy_conn_data)->pool->get_active_masters((*proxy_conn_data)->pool TSRMLS_CC))
		MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, element->conn);
		if ((PASS == (ret = mysqlnd_ms_cs_gtid_get_last(element, NULL TSRMLS_CC)))) {
			if (!gtid) {
				DBG_INF_FMT("First master %s gtid %s", MYSQLND_MS_CONN_STRING(element->host), (*conn_data)->global_trx.last_gtid);
				gtid = (*conn_data)->global_trx.last_gtid;
			} else if (mysqlnd_ms_aux_gtid_chk_last((*conn_data)->global_trx.last_gtid, (*conn_data)->global_trx.last_gtid_len, gtid, strlen(gtid)) == PASS) {
				DBG_INF_FMT("Found bigger master %s gtid %s", MYSQLND_MS_CONN_STRING(element->host), (*conn_data)->global_trx.last_gtid);
				gtid = (*conn_data)->global_trx.last_gtid;
			} else {
				DBG_INF_FMT("Master not in sync %s gtid %s", MYSQLND_MS_CONN_STRING(element->host), (*conn_data)->global_trx.last_gtid);
			}
		} else {
			break;
		}
	END_ITERATE_OVER_SERVER_LIST;
	if (ret == PASS) {
		if (gtid) {
			DBG_INF_FMT("Max gtid from masters %s", gtid);
			if (strcmp(gtid, "0"))
				ret = mysqlnd_ms_section_filters_set_gtid_qos(proxy_conn, gtid, strlen(gtid) TSRMLS_CC);
		} else {
			DBG_INF("No gtid found on masters.");
		}
	}
	DBG_INF_FMT("ret=%s", ret == PASS? "PASS":"FAIL");
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_cs_gtid_connect */
static enum_func_status
mysqlnd_ms_cs_gtid_connect(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_MS_LIST_DATA * new_element, zend_bool is_master,
                               zend_bool lazy_connections TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);
	struct st_mysqlnd_ms_global_trx_injection * global_trx = &(*proxy_conn_data)->global_trx;
	struct st_mysqlnd_ms_conn_credentials * cred =  &(*proxy_conn_data)->cred;
	DBG_ENTER("mysqlnd_ms_cs_gtid_connect");
	if (is_master || global_trx->memcached_key_len > 0) {
		MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, new_element->conn);
		if (global_trx->on_commit_len > 0) {
			DBG_INF("Start gtid_conn_elm creation");
			MYSQLND_MS_LIST_DATA * gtid_conn_elm = NULL;
			MYSQLND * gtid_conn_handle = NULL;
			MYSQLND_CONN_DATA * gtid_conn = NULL;
#if PHP_VERSION_ID < 50600
			gtid_conn_handle = mysqlnd_init(new_element->persistent);
#else
			gtid_conn_handle = mysqlnd_init(proxy_conn->m->get_client_api_capabilities(proxy_conn TSRMLS_CC), new_element->persistent);
#endif
			if (gtid_conn_handle) {
				gtid_conn = MS_GET_CONN_DATA_FROM_CONN(gtid_conn_handle);
			}
			if (!gtid_conn || PASS != mysqlnd_ms_connect_to_host_aux_elm(proxy_conn, gtid_conn, new_element->name_from_config,
					is_master, MYSQLND_MS_CONN_A_STRING(new_element->host), new_element->port, &gtid_conn_elm, cred, lazy_connections, new_element->persistent, TRUE TSRMLS_CC)) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Failed gtid_conn_elm creation.");
				ret = FAIL;
			}
			if (gtid_conn_handle) {
				gtid_conn_handle->m->dtor(gtid_conn_handle TSRMLS_CC);
			}
			if (ret == PASS) {
				MS_DECLARE_AND_LOAD_CONN_DATA(gtid_conn_data, gtid_conn_elm->conn);
				(*gtid_conn_data)->skip_ms_calls = TRUE; // always skip gtid connection
				(*conn_data)->global_trx.gtid_conn_elm = gtid_conn_elm;
			}
		} else if (global_trx->memcached_key_len > 0) {
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
				DBG_INF_FMT("Connect to Memcached %s %d", MYSQLND_MS_CONN_STRING(new_element->host), mcport);
				rc = memcached_server_add(memc, MYSQLND_MS_CONN_STRING(new_element->host), mcport);
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
				php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Failed gtid memcached connection create.");
				ret = FAIL;
			}
		}
	}
	DBG_INF_FMT("ret=%s", ret == PASS? "PASS":"FAIL");
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_cs_gtid_inject_before */
static enum_func_status
mysqlnd_ms_cs_gtid_inject_before(MYSQLND_CONN_DATA * conn TSRMLS_DC)
{
	enum_func_status ret = PASS;
	DBG_ENTER("mysqlnd_ms_cs_gtid_inject_before");
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_cs_gtid_inject_after */
static enum_func_status
mysqlnd_ms_cs_gtid_inject_after(MYSQLND_CONN_DATA * conn, enum_func_status status TSRMLS_DC)
{
	enum_func_status ret = PASS;
	DBG_ENTER("mysqlnd_ms_cs_gtid_inject_after");
	if (status == PASS) {
		char * gtid = NULL;
		MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
		if ((*conn_data)->global_trx.on_commit_len > 0) {
			MYSQLND_MS_LIST_DATA * gtid_conn_elm = (*conn_data)->global_trx.gtid_conn_elm;
			DBG_INF_FMT("on_commit value %s", (*conn_data)->global_trx.on_commit);
			if (!gtid_conn_elm || (_MS_CONN_GET_STATE(gtid_conn_elm->conn) == CONN_ALLOCED && PASS != mysqlnd_ms_lazy_connect(gtid_conn_elm, TRUE TSRMLS_CC))) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Error on gtid_conn_elm SQL connection.");
				ret = FAIL;
			} else if (PASS == (ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(send_query)(gtid_conn_elm->conn, (*conn_data)->global_trx.on_commit, (*conn_data)->global_trx.on_commit_len _MS_SEND_QUERY_AD_EXT TSRMLS_CC))) {
				ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(reap_query)(gtid_conn_elm->conn _MS_REAP_QUERY_AD_EXT TSRMLS_CC);
				if (ret == PASS && (ret = mysqlnd_ms_cs_gtid_get_last(gtid_conn_elm, &gtid TSRMLS_CC)) == PASS) {
					if (gtid) {
						if ((*conn_data)->global_trx.last_gtid)
							mnd_pefree((*conn_data)->global_trx.last_gtid, gtid_conn_elm->conn->persistent);
						(*conn_data)->global_trx.last_gtid = gtid;
						(*conn_data)->global_trx.last_gtid_len = strlen(gtid);
						DBG_INF_FMT("SQL last gtid Success after inject %s", gtid);
					} else {
						ret = FAIL;
						DBG_INF("SQL empty gtid after inject! FAIL");
					}
				}
			}
			if (ret == FAIL) {
				if (TRUE == (*conn_data)->global_trx.report_error) {
					COPY_CLIENT_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(conn), MYSQLND_MS_ERROR_INFO(gtid_conn_elm->conn));
				}
				php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Error on SQL injection.");
			}
		} else if ((*conn_data)->global_trx.memcached_key_len > 0) {
			memcached_st *memc = (*conn_data)->global_trx.memc;
			if (memc) {
				memcached_return_t rc;
				uint64_t value;
				DBG_INF_FMT("memcached_key value %s", (*conn_data)->global_trx.memcached_key);
				rc = memcached_increment(memc, (*conn_data)->global_trx.memcached_key, (*conn_data)->global_trx.memcached_key_len,  1, &value);
				if (rc == MEMCACHED_SUCCESS) {
					size_t n = (snprintf(NULL, 0, "%" PRIu64 "", value)) + 1;
					gtid = emalloc(n);
					snprintf(gtid, n, "%" PRIu64 "", value);
					DBG_INF_FMT("Memcached last gtid %s", gtid);
					(*conn_data)->global_trx.last_gtid = mnd_pestrndup(gtid, strlen(gtid), conn->persistent);
					(*conn_data)->global_trx.last_gtid_len = strlen(gtid);
					efree(gtid);
					ret = PASS;
				} else {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Error memcached increment.");
					ret = FAIL;
				}
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Error no memcached connection.");
				ret = FAIL;
			}
		}
		if (ret == PASS && (*conn_data)->global_trx.last_gtid) {
			if ((*conn_data)->global_trx.last_wgtid)
				mnd_pefree((*conn_data)->global_trx.last_wgtid, conn->persistent);
			(*conn_data)->global_trx.last_wgtid = mnd_pestrndup((*conn_data)->global_trx.last_gtid, (*conn_data)->global_trx.last_gtid_len, conn->persistent);
			(*conn_data)->global_trx.last_wgtid_len = (*conn_data)->global_trx.last_gtid_len;
			ret = mysqlnd_ms_section_filters_set_gtid_qos(conn, (*conn_data)->global_trx.last_gtid, (*conn_data)->global_trx.last_gtid_len TSRMLS_CC);
		}
		DBG_INF_FMT("ret=%s", ret == PASS? "PASS":"FAIL");
	}
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_cs_gtid_filter */
static void
mysqlnd_ms_cs_gtid_filter(MYSQLND_CONN_DATA * conn, const char * gtid, char ** query, size_t * query_len, zend_bool * free_query,
								 zend_llist * slave_list, zend_llist * master_list, zend_llist * selected_slaves, zend_llist * selected_masters, zend_bool is_write TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_cs_gtid_filter");
	mysqlnd_ms_aux_gtid_filter(conn, gtid,  query, query_len, free_query, slave_list, master_list, selected_slaves, selected_masters, is_write TSRMLS_CC);
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_cs_gtid_reset */
static void
mysqlnd_ms_cs_gtid_reset(MYSQLND_CONN_DATA * conn, enum_func_status status TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_cs_gtid_reset");
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_cs_gtid_trace */
static void
mysqlnd_ms_cs_gtid_trace(MYSQLND_CONN_DATA * conn, const char * key, size_t key_len, unsigned int ttl, const char * query, size_t query_len TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_cs_gtid_trace");
	mysqlnd_ms_aux_gtid_trace(conn, key, key_len, ttl, query, query_len TSRMLS_CC);
	DBG_VOID_RETURN;
}
/* }}} */


static
MYSQLND_MS_GTID_TRX_METHODS gtid_methods[GTID_LAST_ENUM_ENTRY] =
{
	{
		GTID_NONE, /* type */
		NULL, /* gtid_get_last */
		NULL, /* gtid_set_last_write */
		NULL, /* gtid_init */
		NULL, /* gtid_connect */
		NULL, /* gtid_inject_before */
		NULL, /* gtid_inject_after */
		NULL, /* gtid_filter */
		NULL, /* gtid_reset */
		NULL, /* gtid_trace */
		NULL, /* gtid_race_add_active */
		NULL, /* gtid_validate */
	},
	{
		GTID_CLIENT, /* type */
		mysqlnd_ms_cs_gtid_get_last, /* gtid_get_last */
		NULL, /* gtid_set_last_write */
		mysqlnd_ms_cs_gtid_init, /* gtid_init */
		mysqlnd_ms_cs_gtid_connect, /* gtid_connect */
		mysqlnd_ms_cs_gtid_inject_before, /* gtid_inject_before */
		mysqlnd_ms_cs_gtid_inject_after, /* gtid_inject_after */
		mysqlnd_ms_cs_gtid_filter, /* gtid_filter */
		mysqlnd_ms_cs_gtid_reset, /* gtid_reset */
		mysqlnd_ms_cs_gtid_trace, /* gtid_trace */
		mysqlnd_ms_aux_gtid_add_active, /* gtid_race_add_active */
		NULL, /* gtid_validate */
	},
	{
		GTID_SERVER, /* type */
		mysqlnd_ms_aux_ss_gtid_get_last, /* gtid_get_last */
		mysqlnd_ms_aux_ss_gtid_set_last_write, /* gtid_set_last_write */
		NULL, /* gtid_init */
		mysqlnd_ms_aux_ss_gtid_connect, /* gtid_connect */
		NULL, /* gtid_inject_before */
		mysqlnd_ms_aux_ss_gtid_inject_after, /* gtid_inject_after */
		mysqlnd_ms_aux_ss_gtid_filter, /* gtid_filter */
		mysqlnd_ms_aux_ss_gtid_reset, /* gtid_reset */
		mysqlnd_ms_aux_ss_gtid_trace, /* gtid_trace */
		mysqlnd_ms_aux_gtid_add_active, /* gtid_race_add_active */
		NULL, /* gtid_validate */
	},
	{
		GTID_SERVER_COMPAT_OLD, /* SAME AS GTID_SERVER, FOR CONFIG COMPATIBILITY WITH 1.7.0 */
		mysqlnd_ms_aux_ss_gtid_get_last, /* gtid_get_last */
		mysqlnd_ms_aux_ss_gtid_set_last_write, /* gtid_set_last_write */
		NULL, /* gtid_init */
		mysqlnd_ms_aux_ss_gtid_connect, /* gtid_connect */
		NULL, /* gtid_inject_before */
		mysqlnd_ms_aux_ss_gtid_inject_after, /* gtid_inject_after */
		mysqlnd_ms_aux_ss_gtid_filter, /* gtid_filter */
		mysqlnd_ms_aux_ss_gtid_reset, /* gtid_reset */
		mysqlnd_ms_aux_ss_gtid_trace, /* gtid_trace */
		mysqlnd_ms_aux_gtid_add_active, /* gtid_race_add_active */
		NULL, /* gtid_validate */
	},
};
/* }}} */

#define MS_CHECK_CONN_FOR_TRANSIENT_ERROR(connection, conn_data, transient_error_no) \
	if ((connection) && MYSQLND_MS_ERROR_INFO((connection)).error_no) { \
		MS_CHECK_FOR_TRANSIENT_ERROR((MYSQLND_MS_ERROR_INFO((connection)).error_no), (conn_data), (transient_error_no)); \
	} \

/* {{{ mysqlnd_ms_clone_client_options */
/*
TODO: This should be done better, i.e. alloc mysqlnd_ms struct before connect and use the pool reply_cmds features ...
probably a usefull solution also for reconnection and failover.
*/
enum_func_status
mysqlnd_ms_clone_client_options(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_CONN_DATA * conn TSRMLS_DC)
{
	_MS_PROTOCOL_CONN_DATA_LOAD_NET_D(proxy_conn, proxy_net);
	_MS_PROTOCOL_CONN_DATA_LOAD_VIO_D(proxy_conn, proxy_vio);
	_MS_PROTOCOL_CONN_DATA_LOAD_NET_D(conn, net);
	_MS_PROTOCOL_CONN_DATA_LOAD_VIO_D(conn, vio);

	DBG_ENTER("mysqlnd_ms_clone_client_options");
	MS_CALL_ORIGINAL_CONN_DATA_METHOD(free_options)(conn);
	/* We don't clone connect_attr hashtable */
	if (proxy_conn->options->auth_protocol && (PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(conn, 
		MYSQLND_OPT_AUTH_PROTOCOL, proxy_conn->options->auth_protocol TSRMLS_CC))) {
		DBG_RETURN(FAIL);
	}
	if (proxy_conn->options->charset_name && PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(conn,
		MYSQL_SET_CHARSET_NAME, proxy_conn->options->charset_name TSRMLS_CC)) {
		DBG_RETURN(FAIL);
	}
	if (proxy_conn->options->num_commands) {
		unsigned int i;
		for (i = 0; i < proxy_conn->options->num_commands; i++) {
			char * new_command;
			if (PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(conn,
				MYSQL_INIT_COMMAND, proxy_conn->options->init_commands[i] TSRMLS_CC)) {
				DBG_RETURN(FAIL);
			}
		}
	}
	conn->options->flags = proxy_conn->options->flags;
#ifdef MYSQLND_STRING_TO_INT_CONVERSION
	conn->options->int_and_float_native = proxy_conn->options->int_and_float_native;
#endif
	conn->options->max_allowed_packet = proxy_conn->options->max_allowed_packet;
	conn->options->protocol = proxy_conn->options->protocol;
	vio->data->options.net_read_buffer_size = proxy_vio->data->options.net_read_buffer_size;
	vio->data->options.timeout_connect = proxy_vio->data->options.timeout_connect;
	vio->data->options.timeout_read = proxy_vio->data->options.timeout_read;
	vio->data->options.timeout_write = proxy_vio->data->options.timeout_write;
	vio->data->options.ssl_verify_peer = proxy_vio->data->options.ssl_verify_peer;
	if (proxy_vio->data->options.ssl_ca && PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(conn,
		MYSQLND_OPT_SSL_CA, proxy_vio->data->options.ssl_ca TSRMLS_CC)) {
		DBG_RETURN(FAIL);
	}
	if (proxy_vio->data->options.ssl_capath && PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(conn,
		MYSQLND_OPT_SSL_CAPATH, proxy_vio->data->options.ssl_capath TSRMLS_CC)) {
		DBG_RETURN(FAIL);
	}
	if (proxy_vio->data->options.ssl_cert && PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(conn,
		MYSQLND_OPT_SSL_CERT, proxy_vio->data->options.ssl_cert TSRMLS_CC)) {
		DBG_RETURN(FAIL);
	}
	if (proxy_vio->data->options.ssl_cipher && PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(conn,
		MYSQLND_OPT_SSL_CIPHER, proxy_vio->data->options.ssl_cipher TSRMLS_CC)) {
		DBG_RETURN(FAIL);
	}
	if (proxy_vio->data->options.ssl_key && PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(conn,
		MYSQLND_OPT_SSL_KEY, proxy_vio->data->options.ssl_key TSRMLS_CC)) {
		DBG_RETURN(FAIL);
	}
	if (proxy_vio->data->options.ssl_passphrase && PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(conn,
		MYSQLND_OPT_SSL_PASSPHRASE, proxy_vio->data->options.ssl_passphrase TSRMLS_CC)) {
		DBG_RETURN(FAIL);
	}
	if (_MS_PROTOCOL_CONN_DATA_NET_SPK(proxy_net) && PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(conn,
		MYSQL_SERVER_PUBLIC_KEY, _MS_PROTOCOL_CONN_DATA_NET_SPK(proxy_net) TSRMLS_CC)) {
		DBG_RETURN(FAIL);
	}
	if (proxy_net->cmd_buffer.length && PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(conn,
		MYSQLND_OPT_NET_CMD_BUFFER_SIZE, (const char * const) &proxy_net->cmd_buffer.length  TSRMLS_CC)) {
		DBG_RETURN(FAIL);
	}
	DBG_RETURN(PASS);
}
/* }}} */

/* {{{ mysqlnd_ms_connect_to_host_aux */
enum_func_status
mysqlnd_ms_connect_to_host_aux(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_CONN_DATA * conn, const char * name_from_config,
							   zend_bool is_master,
							   const MYSQLND_MS_CONN_D_STRING(host), unsigned int port,
							   struct st_mysqlnd_ms_conn_credentials * cred,
							   struct st_mysqlnd_ms_global_trx_injection * global_trx,
							   zend_bool lazy_connections,
							   zend_bool persistent TSRMLS_DC)
{
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
	if (ret == PASS) {
		MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, new_element->conn);
		(*conn_data)->elm_pool_hash_key = &(new_element->pool_hash_key);
		if (global_trx->type != GTID_NONE) {
			ret = MYSQLND_MS_GTID_CALL_PASS(global_trx->m->gtid_connect, proxy_conn, new_element, is_master, lazy_connections);
		}
	}
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
	DBG_INF_FMT("conn:%p process all %d", conn, process_all_list_values);

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
		char * host_to_use = NULL;
		MYSQLND_MS_CONN_DV_STRING(host);
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
		} else if (value_exists && socket_to_use) {
			DBG_INF_FMT("Assign socket :%p", socket_to_use);
			MYSQLND_MS_S_TO_CONN_STRING(cred.socket, socket_to_use);
		}

		user_to_use = mysqlnd_ms_config_json_string_from_section(subsection, SECT_USER_NAME, sizeof(SECT_USER_NAME) - 1, 0,
																 &value_exists, &is_list_value TSRMLS_CC);
		if (is_list_value) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Invalid value for "SECT_USER_NAME". Cannot be a list/hash' . Stopping");
			failures++;
		} else if (value_exists && user_to_use) {
			MYSQLND_MS_S_TO_CONN_STRING(cred.user, user_to_use);
		}
		pass_to_use = mysqlnd_ms_config_json_string_from_section(subsection, SECT_PASS_NAME, sizeof(SECT_PASS_NAME) - 1, 0,
																 &value_exists, &is_list_value TSRMLS_CC);
		if (is_list_value) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Invalid value for "SECT_PASS_NAME". Cannot be a list/hash' . Stopping");
			failures++;
		} else if (value_exists && pass_to_use) {
			MYSQLND_MS_S_TO_CONN_STRINGL(cred.passwd, pass_to_use, strlen(pass_to_use));
		}

		db_to_use = mysqlnd_ms_config_json_string_from_section(subsection, SECT_DB_NAME, sizeof(SECT_DB_NAME) - 1, 0,
															   &value_exists, &is_list_value TSRMLS_CC);
		if (is_list_value) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Invalid value for "SECT_DB_NAME". Cannot be a list/hash' . Stopping");
			failures++;
		} else if (value_exists && db_to_use) {
			MYSQLND_MS_S_TO_CONN_STRINGL(cred.db, db_to_use, strlen(db_to_use));
		}

		host_to_use = mysqlnd_ms_config_json_string_from_section(subsection, SECT_HOST_NAME, sizeof(SECT_HOST_NAME) - 1, 0,
														  &value_exists, &is_list_value TSRMLS_CC);
		if (is_list_value) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Invalid value for "SECT_HOST_NAME". Cannot be a list/hash' . Stopping");
			failures++;
		} else if (FALSE == value_exists) {
			DBG_ERR_FMT("Cannot find ["SECT_HOST_NAME"] in [%s] section in config", subsection_name);
			php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR,
							 MYSQLND_MS_ERROR_PREFIX " Cannot find ["SECT_HOST_NAME"] in [%s] section in config", subsection_name);
			SET_CLIENT_ERROR(_ms_p_ei (error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
							 MYSQLND_MS_ERROR_PREFIX " Cannot find ["SECT_HOST_NAME"] in section in config");
			failures++;
		} else {
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
				mysqlnd_ms_clone_client_options(proxy_conn, tmp_conn TSRMLS_CC);
			}
			MYSQLND_MS_S_TO_CONN_STRING(host, host_to_use);
			if (tmp_conn) {
				enum_func_status status =
					mysqlnd_ms_connect_to_host_aux(proxy_conn, tmp_conn, current_subsection_name, is_master, host, cred.port, &cred,
												   master_global_trx, lazy_connections, persistent TSRMLS_CC);
				if (status != PASS) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Cannot connect to %s", MYSQLND_MS_CONN_STRING(host));
					COPY_CLIENT_ERROR(_ms_p_ei (error_info), MYSQLND_MS_ERROR_INFO(tmp_conn));
//					(*error_info) = MYSQLND_MS_ERROR_INFO(tmp_conn);
					failures++;
					/* let's free the handle, if there is one. The underlying object will stay alive */
					MYSQLND_MS_INC_STATISTIC(fail_stat);
				} else {
					if (!lazy_connections) {
						MYSQLND_MS_INC_STATISTIC(success_stat);
					}
				}
				if (tmp_conn_handle) {
					tmp_conn_handle->m->dtor(tmp_conn_handle TSRMLS_CC);
				}
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
		if (host_to_use) {
			mnd_efree(host_to_use);
			host_to_use = NULL;
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

	trx->memcached_host = NULL;
	trx->memcached_host_len = (size_t)0;
	trx->memcached_key = NULL;
	trx->memcached_key_len = (size_t)0;
	trx->memcached_wkey = NULL;
	trx->memcached_wkey_len = (size_t)0;
	trx->wc_uuid = NULL;
	trx->wc_uuid_len = (size_t)0;
	trx->memcached_port = 0;
	trx->memcached_port_add_hack = 0;
	trx->type = GTID_NONE;
	trx->m = &gtid_methods[GTID_NONE];
	trx->memc = NULL;
	trx->owned_token = 0;
	trx->gtid_conn_elm = NULL;
	trx->last_gtid = NULL;
	trx->last_gtid_len = 0;
	trx->last_wgtid = NULL;
	trx->last_wgtid_len = 0;
	trx->last_ckgtid = NULL;
	trx->last_ckgtid_len = 0;
	trx->last_wckgtid = NULL;
	trx->last_wckgtid_len = 0;
	trx->gtid_block_size = 0;
	trx->running_ttl = 600; // 10 minutes
	trx->memcached_debug_ttl = 0; // Disabled
	trx->wait_for_wgtid_timeout = 6; // 6 seconds
	trx->throttle_wgtid_timeout = 0;
	trx->race_avoid_strategy = GTID_RACE_AVOID_DISABLED;
	trx->injectable_query = FALSE;
	trx->is_prepare = FALSE;
	trx->gtid_on_connect = FALSE;
	trx->auto_clean = TRUE;

	// BEGIN NOWAIT
	trx->memcached = NULL;
	trx->memcached_len = 0;
	trx->module = 0;
	trx->running_depth = 0;
	trx->running_wdepth = 20;
	trx->use_get = FALSE;
	trx->qc_ttl = 0;

	trx->owned_wtoken = 0;
	trx->last_chk_token = 0;
	trx->last_chk_wtoken = 0;
	trx->first_read = TRUE;
	trx->executed = FALSE;
	trx->prev_found = 0;
	trx->prev_wfound = 0;
	trx->last_whost = NULL;
	// END NOWAIT

	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_parse_gtid_string */
static char *
mysqlnd_ms_parse_gtid_string(MYSQLND_CONN_DATA *conn, char * json_value,
						   zend_bool persistent TSRMLS_DC)
{
	char * ret = NULL, * tmpret = NULL;
	zval session;
	zval _ms_p_zval * skey = NULL;
	zval _ms_p_zval * swkey = NULL;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	DBG_ENTER("mysqlnd_ms_parse_gtid_string");
	if (FAIL == mysqlnd_ms_get_php_svar("mysqlnd_ms_gtid_skey", &skey TSRMLS_CC))
		skey = NULL;
	if ( FAIL == mysqlnd_ms_get_php_svar("mysqlnd_ms_gtid_swkey", &swkey TSRMLS_CC))
		swkey = NULL;
	INIT_ZVAL(session);
	ZVAL_NULL(&session);
	if (mysqlnd_ms_get_php_session(&session TSRMLS_CC) == SUCCESS && Z_STRVAL(session) && strlen(Z_STRVAL(session)) && strstr(json_value, "#SID")) {
		ret = tmpret = mysqlnd_ms_str_replace(json_value, "#SID", Z_STRVAL(session), persistent TSRMLS_CC);
	}
	zval_dtor(&session);
	if (skey && strstr(json_value, "#SKEY")) {
		ret = mysqlnd_ms_str_replace(tmpret ? tmpret : json_value, "#SKEY", Z_STRVAL_P(_ms_p_zval skey), persistent TSRMLS_CC);
		if (tmpret)
			mnd_pefree(tmpret, persistent);
		tmpret = ret;
	}
	if (swkey && strstr(json_value, "#SWKEY")) {
		ret = mysqlnd_ms_str_replace(tmpret ? tmpret : json_value, "#SWKEY", Z_STRVAL_P(_ms_p_zval swkey), persistent TSRMLS_CC);
		if (tmpret)
			mnd_pefree(tmpret, persistent);
		tmpret = ret;
	}
	if (MYSQLND_MS_CONN_STRING((*conn_data)->cred.db) && strstr(json_value, "#DB")) {
		ret = mysqlnd_ms_str_replace(tmpret ? tmpret : json_value, "#DB", MYSQLND_MS_CONN_STRING((*conn_data)->cred.db), persistent TSRMLS_CC);
		if (tmpret)
			mnd_pefree(tmpret, persistent);
		tmpret = ret;
	}
	if (MYSQLND_MS_CONN_STRING((*conn_data)->cred.user) && strstr(json_value, "#USER")) {
		ret = mysqlnd_ms_str_replace(tmpret ? tmpret : json_value, "#USER", MYSQLND_MS_CONN_STRING((*conn_data)->cred.user), persistent TSRMLS_CC);
		if (tmpret)
			mnd_pefree(tmpret, persistent);
		tmpret = ret;
	}
	if (!ret)
		ret = mnd_pestrndup(json_value, strlen(json_value), persistent);

	DBG_RETURN(ret);
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
		json_value = mysqlnd_ms_config_json_string_from_section(g_trx_section, SECT_G_TRX_ON_COMMIT, sizeof(SECT_G_TRX_ON_COMMIT) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists && json_value) {
			if (entry_is_list) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
								MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_G_TRX_ON_COMMIT, SECT_G_TRX_NAME);
			} else {
				trx->on_commit = mysqlnd_ms_parse_gtid_string(conn, json_value, persistent);
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
				trx->fetch_last_gtid = mysqlnd_ms_parse_gtid_string(conn, json_value, persistent);
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
				trx->check_for_gtid = mysqlnd_ms_parse_gtid_string(conn, json_value, persistent);
				trx->check_for_gtid_len = strlen(trx->check_for_gtid);
			}
			mnd_efree(json_value);
		}
		json_value = mysqlnd_ms_config_json_string_from_section(g_trx_section, SECT_G_TRX_MEMCACHED, sizeof(SECT_G_TRX_MEMCACHED) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists && json_value) {
			if (entry_is_list) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_G_TRX_MEMCACHED, SECT_G_TRX_NAME);
			} else {
				json_value_len = strlen(json_value);
				trx->memcached = mnd_pestrndup(json_value, json_value_len, persistent);
				trx->memcached_len = strlen(trx->memcached_host);
			}
			mnd_efree(json_value);
		}
		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_MODULE, sizeof(SECT_G_TRX_MODULE) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int < UINT16_MAX) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater or equal than %d", SECT_G_TRX_MODULE, SECT_G_TRX_NAME, UINT16_MAX);
			} else {
				trx->module = (unsigned int)json_int;
			}
		}
		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_RUNNING_DEPTH, sizeof(SECT_G_TRX_RUNNING_DEPTH) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int < 0) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater or equal than zero", SECT_G_TRX_RUNNING_DEPTH, SECT_G_TRX_NAME);
			} else if (json_int > UINT8_MAX) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less than %d", SECT_G_TRX_RUNNING_DEPTH, SECT_G_TRX_NAME, UINT8_MAX);
			} else {
				trx->running_depth = (unsigned int)json_int;
			}
		}
		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_RUNNING_WDEPTH, sizeof(SECT_G_TRX_RUNNING_WDEPTH) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int < 0) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater or equal than zero", SECT_G_TRX_RUNNING_WDEPTH, SECT_G_TRX_NAME);
			} else if (json_int > UINT8_MAX) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less than %d", SECT_G_TRX_RUNNING_WDEPTH, SECT_G_TRX_NAME, UINT8_MAX);
			} else {
				trx->running_wdepth = (unsigned int)json_int;
			}
		}
		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_QC_TTL, sizeof(SECT_G_TRX_QC_TTL) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int < 0) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater or equal than zero", SECT_G_TRX_QC_TTL, SECT_G_TRX_NAME);
			} else if (json_int > UINT8_MAX) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less than %d", SECT_G_TRX_QC_TTL, SECT_G_TRX_NAME, UINT8_MAX);
			} else {
				trx->qc_ttl = (unsigned int)json_int;
			}
		}
		json_value = mysqlnd_ms_config_json_string_from_section(g_trx_section, SECT_G_TRX_USE_GET, sizeof(SECT_G_TRX_USE_GET) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists && json_value) {
			trx->use_get = !mysqlnd_ms_config_json_string_is_bool_false(json_value);
			mnd_efree(json_value);
		}

		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_TYPE, sizeof(SECT_G_TRX_TYPE) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int < 0) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater or equal than zero", SECT_G_TRX_TYPE, SECT_G_TRX_NAME);
			} else if (json_int >= GTID_LAST_ENUM_ENTRY) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less than %u", SECT_G_TRX_TYPE, SECT_G_TRX_NAME, GTID_LAST_ENUM_ENTRY);
			} else {
				trx->type = (enum mysqlnd_ms_gtid_type)json_int;
				trx->m = &gtid_methods[trx->type];
			}
		}
		json_value = mysqlnd_ms_config_json_string_from_section(g_trx_section, SECT_G_TRX_ON_CONNECT, sizeof(SECT_G_TRX_ON_CONNECT) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists && json_value) {
			trx->gtid_on_connect = !mysqlnd_ms_config_json_string_is_bool_false(json_value);
			mnd_efree(json_value);
		}

		json_value = mysqlnd_ms_config_json_string_from_section(g_trx_section, SECT_G_TRX_AUTO_CLEAN, sizeof(SECT_G_TRX_AUTO_CLEAN) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists && json_value) {
			trx->auto_clean = !mysqlnd_ms_config_json_string_is_bool_false(json_value);
			mnd_efree(json_value);
		}

		json_value = mysqlnd_ms_config_json_string_from_section(g_trx_section, SECT_G_TRX_MEMCACHED_HOST, sizeof(SECT_G_TRX_MEMCACHED_HOST) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists && json_value) {
			if (entry_is_list) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_G_TRX_MEMCACHED_HOST, SECT_G_TRX_NAME);
			} else {
				json_value_len = strlen(json_value);
				trx->memcached_host = mnd_pestrndup(json_value, json_value_len, persistent);
				trx->memcached_host_len = strlen(trx->memcached_host);
			}
			mnd_efree(json_value);
		}

		json_value = mysqlnd_ms_config_json_string_from_section(g_trx_section, SECT_G_TRX_MEMCACHED_KEY, sizeof(SECT_G_TRX_MEMCACHED_KEY) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists && json_value) {
			if (entry_is_list) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_G_TRX_MEMCACHED_KEY, SECT_G_TRX_NAME);
			} else {
				trx->memcached_key = mysqlnd_ms_parse_gtid_string(conn, json_value, persistent);
				trx->memcached_key_len = strlen(trx->memcached_key);
			}
			mnd_efree(json_value);
		}

		json_value = mysqlnd_ms_config_json_string_from_section(g_trx_section, SECT_G_TRX_MEMCACHED_WKEY, sizeof(SECT_G_TRX_MEMCACHED_WKEY) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists && json_value) {
			if (entry_is_list) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_G_TRX_MEMCACHED_WKEY, SECT_G_TRX_NAME);
			} else {
				trx->memcached_wkey = mysqlnd_ms_parse_gtid_string(conn, json_value, persistent);
				trx->memcached_wkey_len = strlen(trx->memcached_wkey);
			}
			mnd_efree(json_value);
		}

		json_value = mysqlnd_ms_config_json_string_from_section(g_trx_section, SECT_G_TRX_WC_UUID, sizeof(SECT_G_TRX_WC_UUID) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists && json_value) {
			if (entry_is_list) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_G_TRX_WC_UUID, SECT_G_TRX_NAME);
			} else {
				trx->wc_uuid = mysqlnd_ms_parse_gtid_string(conn, json_value, persistent);
				trx->wc_uuid_len = strlen(trx->wc_uuid);
			}
			mnd_efree(json_value);
		}

		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_MEMCACHED_PORT, sizeof(SECT_G_TRX_MEMCACHED_PORT) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int <= 0) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater than zero", SECT_G_TRX_MEMCACHED_PORT, SECT_G_TRX_NAME);
			} else if (json_int > UINT16_MAX) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less than %d", SECT_G_TRX_MEMCACHED_PORT, SECT_G_TRX_NAME, UINT16_MAX);
			} else {
				trx->memcached_port = (unsigned int)json_int;
			}
		}
		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_MEMCACHED_PORT_ADD_HACK, sizeof(SECT_G_TRX_MEMCACHED_PORT_ADD_HACK) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int <= 0) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater than zero", SECT_G_TRX_MEMCACHED_PORT_ADD_HACK, SECT_G_TRX_NAME);
			} else if (json_int > UINT16_MAX) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less than %d", SECT_G_TRX_MEMCACHED_PORT_ADD_HACK, SECT_G_TRX_NAME, UINT16_MAX);
			} else {
				trx->memcached_port_add_hack = (unsigned int)json_int;
			}
		}
		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_WAIT_FOR_WGTID_TIMEOUT, sizeof(SECT_G_TRX_WAIT_FOR_WGTID_TIMEOUT) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int < 0) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater or equal than zero", SECT_G_TRX_WAIT_FOR_WGTID_TIMEOUT, SECT_G_TRX_NAME);
			} else if (json_int > UINT16_MAX) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less than %d", SECT_G_TRX_WAIT_FOR_WGTID_TIMEOUT, SECT_G_TRX_NAME, UINT16_MAX);
			} else {
				trx->wait_for_wgtid_timeout = (unsigned int)json_int;
			}
		}
		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_THROTTLE_WGTID_TIMEOUT, sizeof(SECT_G_TRX_THROTTLE_WGTID_TIMEOUT) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int < 0) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater or equal than zero", SECT_G_TRX_THROTTLE_WGTID_TIMEOUT, SECT_G_TRX_NAME);
			} else if (json_int > UINT16_MAX) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less than %d", SECT_G_TRX_THROTTLE_WGTID_TIMEOUT, SECT_G_TRX_NAME, UINT16_MAX);
			} else {
				trx->throttle_wgtid_timeout = (unsigned int)json_int;
			}
		}
		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_RACE_AVOID, sizeof(SECT_G_TRX_RACE_AVOID) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int < 0) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater or equal than zero", SECT_G_TRX_RACE_AVOID, SECT_G_TRX_NAME);
			} else if (json_int > GTID_RACE_AVOID_MAX_VALUE) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less or equal %d", SECT_G_TRX_RACE_AVOID, SECT_G_TRX_NAME, GTID_RACE_AVOID_MAX_VALUE);
			} else {
				trx->race_avoid_strategy = json_int;
			}
		}
		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_GTID_BLOCK_SIZE, sizeof(SECT_G_TRX_GTID_BLOCK_SIZE) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int < 0) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater or equal than zero", SECT_G_TRX_GTID_BLOCK_SIZE, SECT_G_TRX_NAME);
			} else if (json_int >= SIZE_MAX) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less than %d", SECT_G_TRX_GTID_BLOCK_SIZE, SECT_G_TRX_NAME, SIZE_MAX);
			} else {
				trx->gtid_block_size = (size_t)json_int;
			}
		}
		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_RUNNING_TTL, sizeof(SECT_G_TRX_RUNNING_TTL) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int < 0) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater or equal than zero", SECT_G_TRX_RUNNING_TTL, SECT_G_TRX_NAME);
			} else if (json_int > (60*60*24*30)) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less or equal than %d", SECT_G_TRX_RUNNING_TTL, SECT_G_TRX_NAME, (60*60*24*30));
			} else {
				trx->running_ttl = (unsigned int)json_int;
			}
		}
		json_int = mysqlnd_ms_config_json_int_from_section(g_trx_section, SECT_G_TRX_MEMCACHED_DEBUG_TTL, sizeof(SECT_G_TRX_MEMCACHED_DEBUG_TTL) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists) {
			if (json_int < 0) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be greater or equal than zero", SECT_G_TRX_MEMCACHED_DEBUG_TTL, SECT_G_TRX_NAME);
			} else if (json_int > (60*60*24*30)) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less or equal than %d", SECT_G_TRX_MEMCACHED_DEBUG_TTL, SECT_G_TRX_NAME, (60*60*24*30));
			} else {
				trx->memcached_debug_ttl = (unsigned int)json_int;
			}
		}

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
			} else if (json_int > UINT16_MAX) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be less than %d", SECT_G_TRX_WAIT_FOR_GTID_TIMEOUT, SECT_G_TRX_NAME, UINT16_MAX);
			} else {
				trx->wait_for_gtid_timeout = (unsigned int)json_int;
			}
		}
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
mysqlnd_ms_init_without_fabric(struct st_mysqlnd_ms_config_json_entry * the_section, struct st_mysqlnd_ms_config_json_entry * hosts_section, MYSQLND_CONN_DATA * conn, MYSQLND_MS_CONN_DATA *conn_data, const char * host TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	DBG_ENTER("mysqlnd_ms_init_without_fabric");

	zend_bool use_lazy_connections = TRUE;
	/* create master connection */
	SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(conn));
	mysqlnd_ms_load_trx_config(the_section, &conn_data->global_trx, conn, conn->persistent TSRMLS_CC);
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
			if (FALSE == mysqlnd_ms_config_json_sub_section_exists(hosts_section, sects_to_check[i], sect_len, 0 TSRMLS_CC)) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_ERROR TSRMLS_CC,
					MYSQLND_MS_ERROR_PREFIX " Section [%s] doesn't exist for host [%s]", sects_to_check[i], host);
			}
		}
	}

	DBG_INF("-------------------- MASTER CONNECTIONS ------------------");
	ret = mysqlnd_ms_connect_to_host(conn, conn,
									 TRUE, &conn_data->cred,
									 &conn_data->global_trx, hosts_section,
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

	SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(conn));

	DBG_INF("-------------------- SLAVE CONNECTIONS ------------------");
	ret = mysqlnd_ms_connect_to_host(conn, NULL,
									 FALSE, &conn_data->cred,
									 &conn_data->global_trx, hosts_section,
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

	if (ret == PASS && conn_data->global_trx.gtid_on_connect && mysqlnd_ms_section_filters_is_gtid_qos(conn TSRMLS_CC) == PASS) {
		if (MYSQLND_MS_GTID_CALL_PASS(conn_data->global_trx.m->gtid_init, conn TSRMLS_CC) == FAIL && conn_data->global_trx.report_error == TRUE) {
			if ((MYSQLND_MS_ERROR_INFO(conn)).error_no == 0) {
				SET_CLIENT_ERROR(_ms_p_ei (conn->error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Error on gtid_init");
			}
			ret = FAIL;
		}
	}
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


static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, connect)(MYSQLND_CONN_DATA * conn,
									const MYSQLND_MS_CONN_D_CSTRING(host),
									const MYSQLND_MS_CONN_D_CSTRING(user),
									const MYSQLND_MS_CONN_D_CSTRINGL(passwd),
									const MYSQLND_MS_CONN_D_CSTRINGL(db),
									unsigned int port,
									const MYSQLND_MS_CONN_D_CSTRING(socket),
									unsigned int mysql_flags TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	MYSQLND_MS_CONN_DATA ** conn_data;
	size_t host_len = MYSQLND_MS_CONN_STRING(host) ? strlen(MYSQLND_MS_CONN_STRING(host)) : 0;
	zend_bool section_found = FALSE;
	zend_bool hotloading = MYSLQND_MS_HOTLOADING;
	struct st_mysqlnd_ms_config_json_entry * the_section = NULL;

	DBG_ENTER("mysqlnd_ms::connect");
	the_section = mysqlnd_ms_config_json_load_host_configuration(MYSQLND_MS_CONN_STRING(host));
	if (!the_section) {
		if (hotloading) {
			MYSQLND_MS_CONFIG_JSON_LOCK(mysqlnd_ms_json_config);
		}

		section_found = mysqlnd_ms_config_json_section_exists(mysqlnd_ms_json_config, MYSQLND_MS_CONN_STRING(host), host_len, 0, hotloading? FALSE:TRUE TSRMLS_CC);
	}
	if (MYSQLND_MS_G(force_config_usage)) {
		if (MYSQLND_MS_G(config_startup_error)) {
			/* TODO: May bark before a hot loading (disabled) attempt is made.
			Same should be true about force config usage */
			php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR,
									  MYSQLND_MS_ERROR_PREFIX " %s", MYSQLND_MS_G(config_startup_error));
		}

		if (FALSE == section_found && !the_section) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX
			" Exclusive usage of configuration enforced but did not find the correct INI file section (%s)", MYSQLND_MS_CONN_STRING(host));
			if (hotloading) {
				MYSQLND_MS_CONFIG_JSON_UNLOCK(mysqlnd_ms_json_config);
			}
			SET_CLIENT_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, MYSQLND_MS_ERROR_PREFIX
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

	if (FALSE == section_found && !the_section) {
		DBG_INF("section not found");
		ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(connect)(conn, MYSQLND_MS_CONN_A_STRING(host), MYSQLND_MS_CONN_A_STRING(user), MYSQLND_MS_CONN_A_STRINGL(passwd), MYSQLND_MS_CONN_A_STRINGL(db), port, MYSQLND_MS_CONN_A_STRING(socket), mysql_flags TSRMLS_CC);
	} else {
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

		MYSQLND_MS_CONN_STRING_DUP((*conn_data)->cred.user, user, conn->persistent);
		MYSQLND_MS_CONN_STRINGL_DUP((*conn_data)->cred.passwd, passwd, conn->persistent);
		MYSQLND_MS_CONN_STRINGL_DUP((*conn_data)->cred.db, db, conn->persistent);
		(*conn_data)->cred.port = port;
		MYSQLND_MS_CONN_STRING_DUP((*conn_data)->cred.socket, socket, conn->persistent);
		(*conn_data)->cred.mysql_flags = mysql_flags;
		mysqlnd_ms_init_trx_to_null(&(*conn_data)->global_trx TSRMLS_CC);
		(*conn_data)->xa_trx = mysqlnd_ms_xa_proxy_conn_init(MYSQLND_MS_CONN_STRING(host), host_len, conn->persistent TSRMLS_CC);
		(*conn_data)->initialized = TRUE;
		if (!hotloading && section_found == TRUE) {
			MYSQLND_MS_CONFIG_JSON_LOCK(mysqlnd_ms_json_config);
		}

		if (section_found == TRUE)
			the_section = mysqlnd_ms_config_json_section(mysqlnd_ms_json_config, MYSQLND_MS_CONN_STRING(host), host_len, &value_exists TSRMLS_CC);

		if (mysqlnd_ms_config_json_sub_section_exists(the_section, SECT_FABRIC_NAME, sizeof(SECT_FABRIC_NAME)-1, 0 TSRMLS_CC)) {
			ret = mysqlnd_ms_init_with_fabric(the_section, conn, *conn_data TSRMLS_CC);
		} else {
			char * hosts_group = NULL, * json_value = mysqlnd_ms_config_json_string_from_section(the_section, HOSTS_GROUP, sizeof(HOSTS_GROUP) - 1, 0,
													NULL, NULL TSRMLS_CC);
			if (json_value) {
				hosts_group = mysqlnd_ms_parse_gtid_string(conn, json_value, FALSE);
				mnd_efree(json_value);
			}
			/* ignore if lazy_connections ini entry exists or not */
			if (hosts_group) {
				struct st_mysqlnd_ms_config_json_entry * hosts_section = mysqlnd_ms_config_json_section(mysqlnd_ms_json_config, hosts_group, strlen(hosts_group), NULL TSRMLS_CC);
				if (section_found == FALSE && hosts_section) {
					MYSQLND_MS_CONFIG_JSON_LOCK(mysqlnd_ms_json_config);
				}
				ret = mysqlnd_ms_init_without_fabric(the_section, hosts_section, conn, *conn_data, hosts_group TSRMLS_CC);
				mysqlnd_ms_config_json_reset_section(hosts_section, TRUE TSRMLS_CC);
				if (section_found == FALSE && hosts_section) {
					MYSQLND_MS_CONFIG_JSON_UNLOCK(mysqlnd_ms_json_config);
				}
				mnd_efree(hosts_group);
			} else {
				ret = mysqlnd_ms_init_without_fabric(the_section, the_section, conn, *conn_data, MYSQLND_MS_CONN_STRING(host) TSRMLS_CC);
			}
		}

		if (section_found == TRUE) {
			mysqlnd_ms_config_json_reset_section(the_section, TRUE TSRMLS_CC);

			if (!hotloading) {
				MYSQLND_MS_CONFIG_JSON_UNLOCK(mysqlnd_ms_json_config);
			}
		} else {
			mysqlnd_ms_config_json_section_dtor(the_section);
		}
		if (ret == PASS) {
			(*conn_data)->connect_host = MYSQLND_MS_CONN_STRING(host)? mnd_pestrdup(MYSQLND_MS_CONN_STRING(host), conn->persistent) : NULL;
		}
	}

	if (hotloading && section_found == TRUE) {
		MYSQLND_MS_CONFIG_JSON_UNLOCK(mysqlnd_ms_json_config);
	}
end_connect:
	DBG_INF_FMT("conn=%llu old_refcount=%u", conn->thread_id, conn->refcount);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_do_send_query(MYSQLND_CONN_DATA * conn, const char * query, size_t query_len, zend_bool pick_server TSRMLS_DC) */
static enum_func_status
mysqlnd_ms_do_send_query(MYSQLND_CONN_DATA * conn, const char * query, size_t query_len, zend_bool pick_server _MS_SEND_QUERY_D_EXT TSRMLS_DC)
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

	ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(send_query)(conn, query, query_len _MS_SEND_QUERY_A_EXT TSRMLS_CC);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ MYSQLND_METHOD(mysqlnd_ms, send_query) */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, send_query)(MYSQLND_CONN_DATA * conn, const char * query, _ms_csize_type query_len _MS_SEND_QUERY_D_EXT TSRMLS_DC)
{
	return mysqlnd_ms_do_send_query(conn, query, query_len, TRUE _MS_SEND_QUERY_A_EXT TSRMLS_CC);
}
/* }}} */


/* {{{ MYSQLND_METHOD(mysqlnd_ms, query) */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, query)(MYSQLND_CONN_DATA * conn, const char * query, _ms_csize_type q_len TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	MYSQLND_CONN_DATA * connection;
	MYSQLND_MS_CONN_DATA ** proxy_conn_data	= conn_data;
	enum_func_status ret = FAIL;
	zend_bool free_query = FALSE, switched_servers = FALSE;
	size_t query_len = q_len;
	uint transient_error_no = 0, transient_error_retries = 0;
	zend_bool inject = FALSE;
#ifdef ALL_SERVER_DISPATCH
	zend_bool use_all = 0;
#endif
	MYSQLND_MS_DBG_ENTER("mysqlnd_ms::query");
	MYSQLND_MS_DBG_INF_FMT("query=%s Using thread "MYSQLND_LLU_SPEC, query, conn->thread_id);

	if (CONN_DATA_NOT_SET(conn_data)) {
		ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(query)(conn, query, q_len TSRMLS_CC);
		MYSQLND_MS_DBG_RETURN(ret);
	}

	connection = mysqlnd_ms_pick_server_ex(conn, (char**)&query, &query_len, &free_query, &switched_servers TSRMLS_CC);
	if (connection && CONN_DATA_TRX_SET(conn_data) && (*conn_data)->global_trx.m->gtid_validate) {
		zend_bool retry = FALSE;
		connection = (*conn_data)->global_trx.m->gtid_validate(connection, &retry, query, query_len TSRMLS_CC);
		if (!connection && retry) {
			MYSQLND_MS_GTID_CALL((*conn_data)->global_trx.m->gtid_reset, (*conn_data)->proxy_conn, ret TSRMLS_CC);
			connection = mysqlnd_ms_pick_server_ex(conn, (char**)&query, &query_len, &free_query, &switched_servers TSRMLS_CC);
			if (connection) {
				connection = (*conn_data)->global_trx.m->gtid_validate(connection, &retry, query, query_len TSRMLS_CC);
				if (!connection)
					mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
											  MYSQLND_MS_ERROR_PREFIX " Not valid concistency connection selected by the last filter");
			}
		}
	}
	MYSQLND_MS_DBG_INF_FMT("Connection %p error_no=%d", connection, connection? (MYSQLND_MS_ERROR_INFO(connection).error_no) : -1);
	/*
	  Beware : error_no is set to 0 in original->query. This, this might be a problem,
	  as we dump a connection from usage till the end of the script.
	  Lazy connections can generate connection failures, thus we need to check for them.
	  If we skip these checks we will get 2014 from original->query.
	*/
	if (!connection || (MYSQLND_MS_ERROR_INFO(connection).error_no)) {
		/* Connect error to be handled by failover logic, not a transient error */
		if (connection && connection != conn) {
			COPY_CLIENT_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(conn), MYSQLND_MS_ERROR_INFO(connection));
		}
		if (TRUE == free_query) {
			MYSQLND_MS_DBG_INF_FMT("Free query %p", query);
			efree((void *)query);
		}
		if (CONN_DATA_TRX_SET(conn_data)) {
			MYSQLND_MS_GTID_CALL((*conn_data)->global_trx.m->gtid_reset, (*conn_data)->proxy_conn, ret TSRMLS_CC);
		}
		MYSQLND_MS_DBG_RETURN(ret);
	}
	if (CONN_DATA_TRX_SET(conn_data) && (*conn_data)->global_trx.memcached_debug_ttl) {
		MYSQLND_MS_GTID_CALL((*conn_data)->global_trx.m->gtid_trace, connection, MEMCACHED_DEBUG_KEY, sizeof(MEMCACHED_DEBUG_KEY) - 1, (*conn_data)->global_trx.memcached_debug_ttl, query, query_len TSRMLS_CC);
	}

	ret = mysqlnd_ms_xa_inject_query(conn, connection, switched_servers TSRMLS_CC);
	if (FAIL == ret) {
		if (TRUE == free_query) {
			MYSQLND_MS_DBG_INF_FMT("Free query %p", query);
			efree((void *)query);
		}
		if (CONN_DATA_TRX_SET(conn_data)) {
			MYSQLND_MS_GTID_CALL((*conn_data)->global_trx.m->gtid_reset, (*conn_data)->proxy_conn, ret TSRMLS_CC);
		}
		MYSQLND_MS_DBG_RETURN(ret);
	}

#ifdef ALL_SERVER_DISPATCH
	if (use_all) {
		MYSQLND_MS_CONN_DATA ** conn_data = (MYSQLND_MS_CONN_DATA **) _ms_mysqlnd_plugin_get_plugin_connection_data_data(conn, mysqlnd_ms_plugin_id);
		zend_llist * master_connections = (conn_data && *conn_data)? &(*conn_data)->master_connections : NULL;
		zend_llist * slave_connections = (conn_data && *conn_data)? &(*conn_data)->slave_connections : NULL;

		mysqlnd_ms_query_all(conn, query, query_len, master_connections, slave_connections TSRMLS_CC);
	}
#endif

	MYSQLND_MS_DBG_INF_FMT("conn="MYSQLND_LLU_SPEC" query=%s", connection->thread_id, query);

	MS_LOAD_CONN_DATA(conn_data, connection);
	if (CONN_DATA_TRX_SET(proxy_conn_data) && (*conn_data)->global_trx.is_master) {
		MYSQLND_MS_DBG_INF_FMT("in_transaction %u injectable %u trx type %u", (*proxy_conn_data)->stgy.in_transaction, (*proxy_conn_data)->global_trx.injectable_query, (*proxy_conn_data)->global_trx.type);
		if (FALSE == (*proxy_conn_data)->stgy.in_transaction && (*proxy_conn_data)->global_trx.injectable_query == TRUE) {
			/* autocommit mode */
			inject = TRUE;
			ret = MYSQLND_MS_GTID_CALL_PASS((*proxy_conn_data)->global_trx.m->gtid_inject_before, connection TSRMLS_CC);
			if (FAIL == ret) {
				inject = FALSE;
				MYSQLND_MS_INC_STATISTIC(MS_STAT_GTID_AUTOCOMMIT_FAILURE);

				if (TRUE == (*proxy_conn_data)->global_trx.report_error) {
					if (TRUE == free_query) {
						MYSQLND_MS_DBG_INF_FMT("Free query %p", query);
						efree((void *)query);
					}
					if ((MYSQLND_MS_ERROR_INFO(connection)).error_no == 0) {
						SET_CLIENT_ERROR(_ms_p_ei (connection->error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Error on gtid_inject_before");
					}
					MYSQLND_MS_DBG_RETURN(ret);
				}
				SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(connection));
				ret = PASS;
			}
		}
	}
    do {
		if ((PASS == (ret = mysqlnd_ms_do_send_query(connection, query, query_len, FALSE _MS_SEND_QUERY_AD_EXT TSRMLS_CC))) &&
			(PASS == (ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(reap_query)(connection _MS_REAP_QUERY_AD_EXT TSRMLS_CC))))
		{
			if (connection->last_query_type == QUERY_UPSERT && (MYSQLND_MS_UPSERT_STATUS(connection).affected_rows)) {
				MYSQLND_INC_CONN_STATISTIC_W_VALUE(connection->stats, STAT_ROWS_AFFECTED_NORMAL, MYSQLND_MS_UPSERT_STATUS(connection).affected_rows);
			}
		}
		/* Is there a transient error that we shall ignore? */
		MS_CHECK_CONN_FOR_TRANSIENT_ERROR(connection, conn_data, transient_error_no);
		if (transient_error_no) {
			MYSQLND_MS_DBG_INF_FMT("Transient error "MYSQLND_LLU_SPEC, transient_error_no);
			transient_error_retries++;
			if (transient_error_retries <= (*conn_data)->stgy.transient_error_max_retries) {
				MYSQLND_MS_INC_STATISTIC(MS_STAT_TRANSIENT_ERROR_RETRIES);
				MYSQLND_MS_DBG_INF_FMT("Retry attempt %i/%i. Sleeping for "MYSQLND_LLU_SPEC" ms and retrying.",
							transient_error_retries,
							(*conn_data)->stgy.transient_error_max_retries,
							(*conn_data)->stgy.transient_error_usleep_before_retry);
#if HAVE_USLEEP
				usleep((*conn_data)->stgy.transient_error_usleep_before_retry);
#endif
			} else {
				MYSQLND_MS_DBG_INF("No more transient error retries allowed");
				break;
			}
		}
	} while (transient_error_no);
	if (CONN_DATA_TRX_SET(proxy_conn_data) && inject == TRUE) {
		/* autocommit mode */
		enum_func_status jret = MYSQLND_MS_GTID_CALL_PASS((*proxy_conn_data)->global_trx.m->gtid_inject_after, connection, ret TSRMLS_CC);
		if (ret == PASS) {
			MYSQLND_MS_INC_STATISTIC((PASS == jret) ? MS_STAT_GTID_AUTOCOMMIT_SUCCESS :
				MS_STAT_GTID_AUTOCOMMIT_FAILURE);

			if (FAIL == jret) {
				if (TRUE == (*proxy_conn_data)->global_trx.report_error) {
					if (TRUE == free_query) {
						MYSQLND_MS_DBG_INF_FMT("Free query %p", query);
						efree((void *)query);
					}
					if ((MYSQLND_MS_ERROR_INFO(connection)).error_no == 0) {
						SET_CLIENT_ERROR(_ms_p_ei (connection->error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Error on gtid_inject_after");
					}
					MYSQLND_MS_DBG_RETURN(jret);
				}
				SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(connection));
			}
		}
	}
	if (TRUE == free_query) {
		MYSQLND_MS_DBG_INF_FMT("Free query %p", query);
		efree((void *)query);
	}
	MYSQLND_MS_DBG_RETURN(ret);
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

/* {{{ mysqlnd_ms_stmt_free_plugin_data */
static void
mysqlnd_ms_stmt_free_plugin_data(MYSQLND_STMT * s TSRMLS_DC)
{
	MYSQLND_MS_STMT_DATA ** stmt_data = NULL;
	DBG_ENTER("mysqlnd_ms_stmt_free_plugin_data");
	MS_LOAD_STMT_DATA(stmt_data, s);
	if (stmt_data && *stmt_data) {
		if ((*stmt_data)->query) {
#if PHP_VERSION_ID < 70300
			mnd_pefree((*stmt_data)->query, s->persistent);
#else
			mnd_pefree((*stmt_data)->query, s->data->conn->persistent);
#endif
			(*stmt_data)->query = NULL;
			(*stmt_data)->query_len = 0;
		}
	}
#if PHP_VERSION_ID < 70300
	mnd_pefree(*stmt_data, s->persistent);
#else
	mnd_pefree(*stmt_data, s->data->conn->persistent);
#endif
	*stmt_data = NULL;
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_conn_free_plugin_data */
static void
mysqlnd_ms_conn_free_plugin_data(MYSQLND_CONN_DATA * conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(data_pp, conn);
	DBG_ENTER("mysqlnd_ms_conn_free_plugin_data");

	DBG_INF_FMT("data_pp=%p *data_pp=%p", data_pp, data_pp ? *data_pp : NULL);
	if (data_pp && *data_pp) {
		// Clean pending if exists
		if (CONN_DATA_TRX_SET(data_pp) && !(*data_pp)->global_trx.executed && ((*data_pp)->global_trx.owned_wtoken || (*data_pp)->global_trx.owned_token))
			MYSQLND_MS_GTID_CALL((*data_pp)->global_trx.m->gtid_reset, (*data_pp)->proxy_conn, FAIL TSRMLS_CC);
		if ((*data_pp)->connect_host) {
			mnd_pefree((*data_pp)->connect_host, conn->persistent);
			(*data_pp)->connect_host = NULL;
		}
		MYSQLND_MS_CONN_STRING_FREE((*data_pp)->cred.user, conn->persistent);
		MYSQLND_MS_CONN_STRINGL_FREE((*data_pp)->cred.passwd, conn->persistent);
		MYSQLND_MS_CONN_STRINGL_FREE((*data_pp)->cred.db, conn->persistent);
		MYSQLND_MS_CONN_STRING_FREE((*data_pp)->cred.socket, conn->persistent);

		(*data_pp)->cred.port = 0;
		(*data_pp)->cred.mysql_flags = 0;
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
		if ((*data_pp)->global_trx.memc) {
			memcached_free((*data_pp)->global_trx.memc);
			(*data_pp)->global_trx.memc = NULL;
		}
		if ((*data_pp)->global_trx.gtid_conn_elm) {
			mysqlnd_ms_conn_list_dtor((void *) &((*data_pp)->global_trx.gtid_conn_elm));
			(*data_pp)->global_trx.gtid_conn_elm = NULL;
		}
		if ((*data_pp)->global_trx.memcached_host) {
			mnd_pefree((*data_pp)->global_trx.memcached_host, conn->persistent);
			(*data_pp)->global_trx.memcached_host = NULL;
			(*data_pp)->global_trx.memcached_host_len = (size_t)0;
		}
		if ((*data_pp)->global_trx.memcached_key) {
			mnd_pefree((*data_pp)->global_trx.memcached_key, conn->persistent);
			(*data_pp)->global_trx.memcached_key = NULL;
			(*data_pp)->global_trx.memcached_key_len = (size_t)0;
		}
		if ((*data_pp)->global_trx.memcached_wkey) {
			mnd_pefree((*data_pp)->global_trx.memcached_wkey, conn->persistent);
			(*data_pp)->global_trx.memcached_wkey = NULL;
			(*data_pp)->global_trx.memcached_wkey_len = (size_t)0;
		}
		if ((*data_pp)->global_trx.wc_uuid) {
			mnd_pefree((*data_pp)->global_trx.wc_uuid, conn->persistent);
			(*data_pp)->global_trx.wc_uuid = NULL;
			(*data_pp)->global_trx.wc_uuid_len = (size_t)0;
		}
		if ((*data_pp)->global_trx.last_gtid) {
			mnd_pefree((*data_pp)->global_trx.last_gtid, conn->persistent);
			(*data_pp)->global_trx.last_gtid = NULL;
			(*data_pp)->global_trx.last_gtid_len = (size_t)0;
		}
		if ((*data_pp)->global_trx.last_wgtid) {
			mnd_pefree((*data_pp)->global_trx.last_wgtid, conn->persistent);
			(*data_pp)->global_trx.last_wgtid = NULL;
			(*data_pp)->global_trx.last_wgtid_len = (size_t)0;
		}
		if ((*data_pp)->global_trx.last_ckgtid) {
			mnd_pefree((*data_pp)->global_trx.last_ckgtid, conn->persistent);
			(*data_pp)->global_trx.last_ckgtid = NULL;
			(*data_pp)->global_trx.last_ckgtid_len = (size_t)0;
		}
		if ((*data_pp)->global_trx.last_wckgtid) {
			mnd_pefree((*data_pp)->global_trx.last_wckgtid, conn->persistent);
			(*data_pp)->global_trx.last_wckgtid = NULL;
			(*data_pp)->global_trx.last_wckgtid_len = (size_t)0;
		}
		if ((*data_pp)->global_trx.memcached) {
			mnd_pefree((*data_pp)->global_trx.memcached, conn->persistent);
			(*data_pp)->global_trx.memcached = NULL;
			(*data_pp)->global_trx.memcached_len = (size_t)0;
		}
		if ((*data_pp)->global_trx.last_whost) {
			mnd_pefree((*data_pp)->global_trx.last_whost, conn->persistent);
			(*data_pp)->global_trx.last_whost = NULL;
		}
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

/* {{{ mysqlnd_ms_stmt::dtor */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms_stmt, dtor)(MYSQLND_STMT * const s, zend_bool implicit TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_stmt::dtor");
	mysqlnd_ms_stmt_free_plugin_data(s TSRMLS_CC);
	DBG_RETURN(ms_orig_mysqlnd_stmt_methods->dtor(s, implicit TSRMLS_CC));
}
/* }}} */

/* {{{ mysqlnd_ms::escape_string */
static _ms_ulong
MYSQLND_METHOD(mysqlnd_ms, escape_string)(MYSQLND_CONN_DATA * const proxy_conn, char * newstr, const char * escapestr, size_t escapestr_len TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;
	ulong ret = 0;
	zend_bool skip_ms_calls = (*conn_data) ? (*conn_data)->skip_ms_calls : FALSE;

	DBG_ENTER("mysqlnd_ms::escape_string");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, conn->thread_id);
	if (_MS_CONN_GET_STATE(conn) > CONN_ALLOCED && _MS_CONN_GET_STATE(conn) != CONN_QUIT_SENT) {
		if (conn_data && *conn_data) {
			(*conn_data)->skip_ms_calls = TRUE;
		}
		ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(escape_string)(conn, newstr, escapestr, escapestr_len TSRMLS_CC);
		if (conn_data && *conn_data) {
			(*conn_data)->skip_ms_calls = skip_ms_calls;
		}
	} else if (_MS_CONN_GET_STATE(conn) == CONN_ALLOCED && ((*conn_data)->server_charset || CONN_GET_OPTION(conn, charset_name))) {
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
			(*conn_data)->skip_ms_calls = skip_ms_calls;
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
										  const char *nuser,
										  const char *npasswd,
										  const char *ndb,
										  zend_bool silent
										  ,size_t npasswd_len
										  TSRMLS_DC)
{
	enum_func_status ret = PASS, last = PASS;
	uint transient_error_no = 0, transient_error_retries = 0;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);

	DBG_ENTER("mysqlnd_ms::change_user");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, proxy_conn->thread_id);
	if (CONN_DATA_NOT_SET(conn_data)) {
		DBG_RETURN(MS_CALL_ORIGINAL_CONN_DATA_METHOD(change_user)(proxy_conn, nuser, npasswd, ndb, silent, npasswd_len TSRMLS_CC));
	} else {
		MYSQLND_MS_LIST_DATA * el;
		MYSQLND_MS_CONN_DV_CSTRING(user);
		MYSQLND_MS_CONN_DV_CSTRINGL(passwd);
		MYSQLND_MS_CONN_DV_CSTRINGL(db);

		ret = (*conn_data)->pool->dispatch_change_user((*conn_data)->pool,
													MYSQLND_METHOD(mysqlnd_ms, change_user),
													nuser, npasswd, ndb, silent
#if PHP_VERSION_ID >= 50399
													, npasswd_len
#endif
													TSRMLS_CC);
		MYSQLND_MS_S_TO_CONN_STRING(user, nuser);
		MYSQLND_MS_S_TO_CONN_STRINGL(db, ndb, strlen(ndb));
		MYSQLND_MS_S_TO_CONN_STRINGL(passwd, npasswd, npasswd_len);

		BEGIN_ITERATE_OVER_SERVER_LISTS(el, (*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC), (*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC));
		{
			MS_DECLARE_AND_LOAD_CONN_DATA(el_conn_data, el->conn);
			zend_bool skip_ms_calls = (*el_conn_data)->skip_ms_calls;
			if (el_conn_data && *el_conn_data) {
				(*el_conn_data)->skip_ms_calls = TRUE;
			}
			if (_MS_CONN_GET_STATE(el->conn) == CONN_ALLOCED) {
				/* lazy connection */
				MYSQLND_MS_CONN_STRING_FREE(el->user, el->persistent);
				MYSQLND_MS_CONN_STRING_DUP(el->user, user, el->persistent);
				MYSQLND_MS_CONN_STRINGL_FREE(el->db, el->persistent);
				MYSQLND_MS_CONN_STRINGL_DUP(el->db, db, el->persistent);
				MYSQLND_MS_CONN_STRINGL_FREE(el->passwd, el->persistent);
				MYSQLND_MS_CONN_STRINGL_DUP(el->passwd, passwd, el->persistent);
			} else {

				/* reset retry counter for every connection */
				transient_error_retries = 0;
				do {

					last = MS_CALL_ORIGINAL_CONN_DATA_METHOD(change_user)(el->conn, nuser, npasswd, ndb, silent
																		,npasswd_len
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
				(*el_conn_data)->skip_ms_calls = skip_ms_calls;
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
			zend_bool skip_ms_calls = (*el_conn_data)->skip_ms_calls;
			zval * row = NULL;
			char * scheme;
			size_t scheme_len;

			if (el_conn_data && *el_conn_data) {
				(*el_conn_data)->skip_ms_calls = TRUE;
			}

			if (_MS_CONN_GET_STATE(el->conn) == CONN_ALLOCED) {
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
				(*el_conn_data)->skip_ms_calls = skip_ms_calls;
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
MYSQLND_METHOD(mysqlnd_ms, select_db)(MYSQLND_CONN_DATA * const proxy_conn, const char * const ndb, _ms_csize_type ndb_len TSRMLS_DC)
{
	enum_func_status last = PASS, ret = PASS;
	uint transient_error_no = 0, transient_error_retries = 0;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);

	DBG_ENTER("mysqlnd_ms::select_db");
	DBG_INF_FMT("Using thread "MYSQLND_LLU_SPEC, proxy_conn->thread_id);
	if (CONN_DATA_NOT_SET(conn_data)) {
		DBG_RETURN(MS_CALL_ORIGINAL_CONN_DATA_METHOD(select_db)(proxy_conn, ndb, ndb_len TSRMLS_CC));
	} else {
		MYSQLND_MS_LIST_DATA * el;
		MYSQLND_MS_CONN_DV_CSTRINGL(db);
		MYSQLND_MS_S_TO_CONN_STRINGL(db, ndb, ndb_len);

		ret = (*conn_data)->pool->dispatch_select_db((*conn_data)->pool, MYSQLND_METHOD(mysqlnd_ms, select_db), ndb, ndb_len TSRMLS_CC);

		BEGIN_ITERATE_OVER_SERVER_LISTS(el, (*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC), (*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC));
		{
			if (_MS_CONN_GET_STATE(el->conn) > CONN_ALLOCED && _MS_CONN_GET_STATE(el->conn) != CONN_QUIT_SENT) {
				MS_DECLARE_AND_LOAD_CONN_DATA(el_conn_data, el->conn);
				zend_bool skip_ms_calls = (*el_conn_data)->skip_ms_calls;

				if (el_conn_data && *el_conn_data) {
					(*el_conn_data)->skip_ms_calls = TRUE;
				}

				/* reset retry counter for every connection */
				transient_error_retries = 0;
				do {
					last = MS_CALL_ORIGINAL_CONN_DATA_METHOD(select_db)(el->conn, ndb, ndb_len TSRMLS_CC);

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
					(*el_conn_data)->skip_ms_calls = skip_ms_calls;
				}
			} else if (_MS_CONN_GET_STATE(el->conn) == CONN_ALLOCED) {
				/* lazy connection */
				MYSQLND_MS_CONN_STRINGL_FREE(el->db, el->persistent);
				MYSQLND_MS_CONN_STRINGL_DUP(el->db, db, el->persistent);
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
			enum_mysqlnd_connection_state state = _MS_CONN_GET_STATE(el->conn);
			if (state != CONN_QUIT_SENT) {
				MS_DECLARE_AND_LOAD_CONN_DATA(el_conn_data, el->conn);
				zend_bool skip_ms_calls = (*el_conn_data)->skip_ms_calls;

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
					(*el_conn_data)->skip_ms_calls = skip_ms_calls;
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

			if (_MS_CONN_GET_STATE(el->conn) > CONN_ALLOCED && _MS_CONN_GET_STATE(el->conn) != CONN_QUIT_SENT) {
				MS_DECLARE_AND_LOAD_CONN_DATA(el_conn_data, el->conn);
				zend_bool skip_ms_calls = (*el_conn_data)->skip_ms_calls;

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
					(*el_conn_data)->skip_ms_calls = skip_ms_calls;
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
MYSQLND_METHOD(mysqlnd_ms, set_client_option)(MYSQLND_CONN_DATA * const proxy_conn, enum_mysqlnd_client_option option, const char * const value TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	DBG_ENTER("mysqlnd_ms::set_client_option");

	if (CONN_DATA_NOT_SET(conn_data)) {
		ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(proxy_conn, option, value TSRMLS_CC);
	} else {
		MYSQLND_MS_LIST_DATA * el;

		ret = (*conn_data)->pool->dispatch_set_client_option((*conn_data)->pool, MYSQLND_METHOD(mysqlnd_ms, set_client_option), option, value TSRMLS_CC);

		BEGIN_ITERATE_OVER_SERVER_LISTS(el, (*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC), (*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC));
		{
			MS_DECLARE_AND_LOAD_CONN_DATA(el_conn_data, el->conn);
			zend_bool skip_ms_calls = (*el_conn_data)->skip_ms_calls;

			if (el_conn_data && *el_conn_data) {
				(*el_conn_data)->skip_ms_calls = TRUE;
			}

			/* This is buffered and replies come later, thus we cannot add transient error loop */
			if (PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(el->conn, option, value TSRMLS_CC)) {
				ret = FAIL;
			}

			if (el_conn_data && *el_conn_data) {
				(*el_conn_data)->skip_ms_calls = skip_ms_calls;
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

	return MYSQLND_MS_CONN_STRING(conn->last_message);
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


		if (CONN_DATA_TRX_SET(conn_data)) {
			if ((*conn_data)->stgy.trx_autocommit_off != (mode ? FALSE : TRUE)) {
				if (mode && TRUE == (*conn_data)->stgy.in_transaction) {
					/*
					Implicit commit when autocommit(false) ..query().. autocommit(true).
					Must inject before second=current autocommit() call.
					Make it explicit and send a real commit
					*/
					ret = proxy_conn->m->tx_commit(proxy_conn TSRMLS_CC);

					MYSQLND_MS_INC_STATISTIC((PASS == ret) ? MS_STAT_GTID_IMPLICIT_COMMIT_SUCCESS :
						MS_STAT_GTID_IMPLICIT_COMMIT_FAILURE);

					if (FAIL == ret) {
						DBG_INF("Explicit commit fails from implicit");
						if (TRUE == (*conn_data)->global_trx.report_error) {
							DBG_RETURN(ret);
						}
						ret = PASS;
						SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(proxy_conn));
					}
				}
			}
		}
		/* No need to handle transient errors
		 set_autocommit() calls query() if connected. query() is covered.
		 If client is not connected to server, then set_autocimmit() calls
		 set_client_option() which is buffered. We cannot handle buffered
		 connect options through the transient error retry logic. in sum:
		 set_autocommit() handled transient errors if connected, otherwise not.
		*/
		BEGIN_ITERATE_OVER_SERVER_LISTS(el, (*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC), (*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC));
		{
			if (_MS_CONN_GET_STATE(el->conn) != CONN_QUIT_SENT) {
				MYSQLND_MS_CONN_DATA ** el_conn_data = (MYSQLND_MS_CONN_DATA **) _ms_mysqlnd_plugin_get_plugin_connection_data_data(el->conn, mysqlnd_ms_plugin_id);
				zend_bool skip_ms_calls = (*el_conn_data)->skip_ms_calls;
				if (el_conn_data && *el_conn_data) {
					(*el_conn_data)->skip_ms_calls = TRUE;
				}
				if (_MS_CONN_GET_STATE(el->conn) == CONN_ALLOCED) {
					ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_client_option)(el->conn, MYSQL_INIT_COMMAND,
																		  (mode) ? "SET AUTOCOMMIT=1":"SET AUTOCOMMIT=0"
																		  TSRMLS_CC);
				} else if (PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(set_autocommit)(el->conn, mode TSRMLS_CC)) {
					ret = FAIL;
				}
				if (el_conn_data && *el_conn_data) {
					(*el_conn_data)->skip_ms_calls = skip_ms_calls;
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
				if (_MS_CONN_GET_STATE(el->conn) != CONN_QUIT_SENT) {
					MYSQLND_MS_CONN_DATA ** el_conn_data = (MYSQLND_MS_CONN_DATA **) _ms_mysqlnd_plugin_get_plugin_connection_data_data(el->conn, mysqlnd_ms_plugin_id);
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

			if ((!(*conn_data)->stgy.last_used_conn) && (_MS_CONN_GET_STATE(proxy_conn) == CONN_ALLOCED)) {
				/*
				 * Lazy connection and no connection has been opened yet.
				 * If this was a regular/non-MS connection and there would have been no error during
				 * set_autocommit(), then it any previous error code had been unset. Now, this is
				 * like a regular connection and there was no error, hence we must unset */
				SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(proxy_conn));
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
	zend_bool skip_ms_calls;


	DBG_ENTER("mysqlnd_ms_tx_commit_or_rollback");
	if (proxy_conn_data && *proxy_conn_data && (*proxy_conn_data)->stgy.last_used_conn) {
		conn = (*proxy_conn_data)->stgy.last_used_conn;
		MS_LOAD_CONN_DATA(conn_data, conn);
	} else {
		conn = proxy_conn;
		conn_data = proxy_conn_data;
	}
	skip_ms_calls = (*conn_data) ? (*conn_data)->skip_ms_calls : FALSE;
	DBG_INF_FMT("conn="MYSQLND_LLU_SPEC, conn->thread_id);

	if (_MS_CONN_GET_STATE(conn) == CONN_ALLOCED && !CONN_DATA_NOT_SET(conn_data)) {
		/* TODO: what is this good for ? */
		DBG_RETURN(PASS);
	}
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
		(*conn_data)->skip_ms_calls = skip_ms_calls;
		DBG_INF_FMT("commit=%d in_transaction=%d is_master=%d", commit, (*proxy_conn_data)->stgy.in_transaction, (*conn_data)->global_trx.is_master);
		if (CONN_DATA_TRX_SET(conn_data) && TRUE == (*proxy_conn_data)->stgy.in_transaction  && (*conn_data)->global_trx.is_master) {
			enum_func_status jret = MYSQLND_MS_GTID_CALL_PASS((*conn_data)->global_trx.m->gtid_inject_after, conn, (ret == PASS && commit == TRUE ? PASS : FAIL) TSRMLS_CC);
			if (ret == PASS) {
				MYSQLND_MS_INC_STATISTIC((PASS == jret) ? MS_STAT_GTID_COMMIT_SUCCESS : MS_STAT_GTID_COMMIT_FAILURE);
				if (FAIL == jret) {
					if (TRUE == (*conn_data)->global_trx.report_error) {
						if ((MYSQLND_MS_ERROR_INFO(conn)).error_no == 0) {
							SET_CLIENT_ERROR(_ms_p_ei (conn->error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Error on gtid_inject_after");
						}
						DBG_RETURN(jret);
					}

					SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(conn));
				}
			}
		}

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
	DBG_INF_FMT("ret=%s", ret == PASS? "PASS":"FAIL");
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
		if (CONN_DATA_TRX_SET(conn_data) && TRUE == (*conn_data)->stgy.in_transaction) {
			/*
			Implicit commit when begin() ..query().. begin().
			Must inject before second=current begin() call.
			Make it explicit and send a real commit
			*/
			ret = conn->m->tx_commit(conn TSRMLS_CC);

			MYSQLND_MS_INC_STATISTIC((PASS == ret) ? MS_STAT_GTID_IMPLICIT_COMMIT_SUCCESS :
				MS_STAT_GTID_IMPLICIT_COMMIT_FAILURE);

			if (FAIL == ret) {
				DBG_INF("Explicit commit fails from implicit");
				if (TRUE == (*conn_data)->global_trx.report_error) {
					DBG_RETURN(ret);
				}
				ret = PASS;
				SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(conn));
			}
		}
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
MYSQLND_METHOD(mysqlnd_ms, get_server_statistics)(MYSQLND_CONN_DATA * proxy_conn, _MS_STR_D_P(message) TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;

	DBG_ENTER("mysqlnd_ms::statistic");
	DBG_INF_FMT("conn="MYSQLND_LLU_SPEC, conn->thread_id);
	if (_MS_CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY) {
		conn = mysqlnd_ms_pick_first_master_or_slave(proxy_conn TSRMLS_CC);
		if (!conn || (_MS_CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY)) {
			DBG_INF("No connection");
			DBG_RETURN(ret);
		}
	}
	ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(get_server_statistics)(conn, _MS_STR_A(message) TSRMLS_CC);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms::get_server_version */
static _ms_ulong
MYSQLND_METHOD(mysqlnd_ms, get_server_version)(const MYSQLND_CONN_DATA * const proxy_conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	const MYSQLND_CONN_DATA * conn = ((*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn;
	if (_MS_CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY) {
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
	if (_MS_CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY) {
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

	if (_MS_CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY) {
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

	if (_MS_CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY) {
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

	if (_MS_CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY) {
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
	if (_MS_CONN_GET_STATE((MYSQLND_CONN_DATA *) conn) < CONN_READY) {
		conn = mysqlnd_ms_pick_first_master_or_slave(proxy_conn TSRMLS_CC);
	}
	DBG_RETURN(MS_CALL_ORIGINAL_CONN_DATA_METHOD(server_dump_debug_information)(conn TSRMLS_CC));
}
/* }}} */


/* {{{ mysqlnd_ms_stmt::prepare */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms_stmt, prepare)(MYSQLND_STMT * const s, const char * const query, _ms_csize_type query_len TSRMLS_DC)
{
	MYSQLND_MS_CONN_DATA ** conn_data = NULL;
	MYSQLND_CONN_DATA * connection = NULL;
	enum_func_status ret = PASS;
	zend_bool free_query = FALSE, switched_servers = FALSE;

	uint transient_error_no = 0, transient_error_retries = 0;
	MYSQLND_MS_DBG_ENTER("mysqlnd_ms_stmt::prepare");
	MYSQLND_MS_DBG_INF_FMT("query=%s", query);

	if (!s || !s->data || !s->data->conn ||
		!(MS_LOAD_CONN_DATA(conn_data, s->data->conn)) ||
		!*conn_data || (*conn_data)->skip_ms_calls)
	{
		MYSQLND_MS_DBG_INF("skip MS");
		MYSQLND_MS_DBG_RETURN(ms_orig_mysqlnd_stmt_methods->prepare(s, query, query_len TSRMLS_CC));
	}
	if ((*conn_data)->proxy_conn != s->data->conn) {
		MS_LOAD_CONN_DATA(conn_data, (*conn_data)->proxy_conn);
	}
	(*conn_data)->global_trx.is_prepare = TRUE;

//	this can possibly reroute us to another server
	connection = mysqlnd_ms_pick_server_ex((*conn_data)->proxy_conn, (char **)&query, (size_t *)&query_len, &free_query, &switched_servers TSRMLS_CC);
	(*conn_data)->global_trx.is_prepare = FALSE; /* Disable session consistency */
	/*
	  Beware : error_no is set to 0 in original->query. This, this might be a problem,
	  as we dump a connection from usage till the end of the script.
	  Lazy connections can generate connection failures, thus we need to check for them.
	  If we skip these checks we will get 2014 from original->query.
	*/
	if (!connection || (MYSQLND_MS_ERROR_INFO(connection).error_no)) {
		/* Connect error to be handled by failover logic, not a transient error */
		if (connection && connection != s->data->conn) {
			COPY_CLIENT_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(s->data->conn), MYSQLND_MS_ERROR_INFO(connection));
		}
		if (TRUE == free_query) {
			MYSQLND_MS_DBG_INF_FMT("Free query %p", query);
			efree((void *)query);
		}
		MYSQLND_MS_DBG_RETURN(FAIL);
	}
	MYSQLND_MS_DBG_INF_FMT("Connection %p, query=%s", connection, query);

	if (connection != s->data->conn) {
		// free what we have
#if PHP_VERSION_ID < 70100
		s->m->net_close(s, TRUE TSRMLS_CC);
#else
		s->m->close_on_server(s, TRUE TSRMLS_CC);
#endif
#if PHP_VERSION_ID < 70300
		mnd_pefree(s->data, s->data->persistent);
#else
		mnd_efree(s->data);
#endif

		// new handle
		{
			MYSQLND_STMT * new_handle = MS_CALL_ORIGINAL_CONN_DATA_METHOD(stmt_init)(connection TSRMLS_CC);
			if (!new_handle || !new_handle->data) {
				MYSQLND_MS_DBG_ERR("new_handle is null");
				if (TRUE == free_query) {
					MYSQLND_MS_DBG_INF_FMT("Free query %p", query);
					efree((void *)query);
				}
				MYSQLND_MS_DBG_RETURN(FAIL);
			}
			s->data = new_handle->data;
#if PHP_VERSION_ID < 70300
			mnd_pefree(new_handle, new_handle->data->persistent);
#else
			mnd_efree(new_handle);
#endif
		}
	}

	do {
		ret = ms_orig_mysqlnd_stmt_methods->prepare(s, query, query_len TSRMLS_CC);
		if (PASS == ret) {
			break;
		}
		MS_CHECK_CONN_FOR_TRANSIENT_ERROR(connection, conn_data, transient_error_no);
		if (transient_error_no) {
			MYSQLND_MS_DBG_INF_FMT("Transient error "MYSQLND_LLU_SPEC, transient_error_no);
			transient_error_retries++;
			if (transient_error_retries <= (*conn_data)->stgy.transient_error_max_retries) {
				MYSQLND_MS_INC_STATISTIC(MS_STAT_TRANSIENT_ERROR_RETRIES);
				MYSQLND_MS_DBG_INF_FMT("Retry attempt %i/%i. Sleeping for "MYSQLND_LLU_SPEC" ms and retrying.",
					transient_error_retries,
					(*conn_data)->stgy.transient_error_max_retries,
					(*conn_data)->stgy.transient_error_usleep_before_retry);
#if HAVE_USLEEP
				usleep((*conn_data)->stgy.transient_error_usleep_before_retry);
#endif
			} else {
				MYSQLND_MS_DBG_INF("No more transient error retries allowed");
				ret = FAIL;
				break;
			}
		} else {
			/* an error that is not considered transient */
			ret = FAIL;
			break;
		}
	} while (transient_error_no);
	if (ret == PASS){
		MYSQLND_MS_STMT_DATA ** stmt_data = NULL;
		MS_LOAD_STMT_DATA(stmt_data, s);
		if (!(*stmt_data)) {
#if PHP_VERSION_ID < 70300
			*stmt_data = mnd_pecalloc(1, sizeof(MYSQLND_MS_STMT_DATA), s->persistent);
#else
			*stmt_data = mnd_pecalloc(1, sizeof(MYSQLND_MS_STMT_DATA), s->data->conn->persistent);
#endif
			if (!(*stmt_data)) {
				MYSQLND_MS_WARN_OOM();
				MYSQLND_MS_DBG_RETURN(FAIL);
			}
			(*stmt_data)->query = NULL;
			(*stmt_data)->query_len = 0;
		} else {
			if ((*stmt_data)->query) {
#if PHP_VERSION_ID < 70300
				mnd_pefree((*stmt_data)->query, s->persistent);
#else
				mnd_pefree((*stmt_data)->query, s->data->conn->persistent);
#endif
				(*stmt_data)->query = NULL;
				(*stmt_data)->query_len = 0;
			}
		}
#if PHP_VERSION_ID < 70300
		(*stmt_data)->query = mnd_pestrndup(query, query_len, s->persistent);
#else
		(*stmt_data)->query = mnd_pestrndup(query, query_len, s->data->conn->persistent);
#endif
		(*stmt_data)->query_len = query_len;
	}

	if (TRUE == free_query) {
		MYSQLND_MS_DBG_INF_FMT("Free query %p", query);
		efree((void *)query);
	}
	MYSQLND_MS_DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_stmt::execute */
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms_stmt, execute)(MYSQLND_STMT * const s TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_CONN_DATA ** conn_data = NULL;
	uint transient_error_no = 0, transient_error_retries = 0;
	unsigned int stmt_errno;
	MYSQLND_CONN_DATA * connection = NULL;
	MYSQLND_STMT_DATA * stmt = s ? s->data:NULL;
	zend_bool inject = FALSE;

	MYSQLND_MS_DBG_ENTER("mysqlnd_ms_stmt::execute");

	if (!stmt ||
		!s || !s->data || !s->data->conn ||
		!(MS_LOAD_CONN_DATA(conn_data, s->data->conn)) ||
		!conn_data || !*conn_data || (*conn_data)->skip_ms_calls ||
		stmt->state < MYSQLND_STMT_PREPARED ||
		(stmt->param_count && !stmt->param_bind)
		)
	{
		MYSQLND_MS_DBG_INF("skip MS");
		ret = ms_orig_mysqlnd_stmt_methods->execute(s TSRMLS_CC);
		MYSQLND_MS_DBG_RETURN(ret);
	}
	connection = s->data->conn;
	MYSQLND_MS_DBG_INF_FMT("conn="MYSQLND_LLU_SPEC, connection->thread_id);
	{
		zend_bool free_query = FALSE, switched_servers = FALSE;
		size_t query_len;
		MYSQLND_MS_STMT_DATA ** stmt_data = NULL;
		char *query = NULL;
		MS_LOAD_STMT_DATA(stmt_data, s);
		if (!stmt_data || !*stmt_data || !(*stmt_data)->query) {
			SET_STMT_ERROR(stmt, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Something wrong: No plugin data or query.");
			MYSQLND_MS_DBG_RETURN(FAIL);
		}
		query = (*stmt_data)->query;
		query_len = (*stmt_data)->query_len;
		connection = mysqlnd_ms_pick_server_ex((*conn_data)->proxy_conn, &query, &query_len, &free_query, &switched_servers TSRMLS_CC);
		if (connection && CONN_DATA_TRX_SET(conn_data) && (*conn_data)->global_trx.m->gtid_validate) {
			zend_bool retry = FALSE;
			connection = (*conn_data)->global_trx.m->gtid_validate(connection, &retry, query, query_len TSRMLS_CC);
			if (!connection && retry) {
				MYSQLND_MS_GTID_CALL((*conn_data)->global_trx.m->gtid_reset, (*conn_data)->proxy_conn, ret TSRMLS_CC);
				connection = mysqlnd_ms_pick_server_ex((*conn_data)->proxy_conn, (char**)&query, &query_len, &free_query, &switched_servers TSRMLS_CC);
				if (connection) {
					connection = (*conn_data)->global_trx.m->gtid_validate(connection, &retry, query, query_len TSRMLS_CC);
					if (!connection)
						mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(s->data->conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
												  MYSQLND_MS_ERROR_PREFIX " Not valid concistency connection selected by the last filter");
				}
			}
		}
		/*
		  Beware : error_no is set to 0 in original->query. This, this might be a problem,
		  as we dump a connection from usage till the end of the script.
		  Lazy connections can generate connection failures, thus we need to check for them.
		  If we skip these checks we will get 2014 from original->query.
		*/
		if (!connection || (MYSQLND_MS_ERROR_INFO(connection).error_no)) {
			/* Connect error to be handled by failover logic, not a transient error */
			if (connection && connection != s->data->conn) {
				COPY_CLIENT_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(s->data->conn), MYSQLND_MS_ERROR_INFO(connection));
			}
			if (TRUE == free_query) {
				MYSQLND_MS_DBG_INF_FMT("Free query %p", query);
				efree((void *)query);
			}
			if (CONN_DATA_TRX_SET(conn_data)) {
				MYSQLND_MS_GTID_CALL((*conn_data)->global_trx.m->gtid_reset, (*conn_data)->proxy_conn, FAIL TSRMLS_CC);
			}
			MYSQLND_MS_DBG_RETURN(FAIL);
		}
		if (CONN_DATA_TRX_SET(conn_data) && (*conn_data)->global_trx.memcached_debug_ttl) {
			MYSQLND_MS_GTID_CALL((*conn_data)->global_trx.m->gtid_trace, connection, MEMCACHED_DEBUG_KEY, sizeof(MEMCACHED_DEBUG_KEY) - 1, (*conn_data)->global_trx.memcached_debug_ttl, query, query_len TSRMLS_CC);
		}
		MYSQLND_MS_DBG_INF_FMT("After pick server conn="MYSQLND_LLU_SPEC, connection->thread_id);
		if (connection != s->data->conn) {
			MYSQLND_STMT * s_to_prepare = MS_CALL_ORIGINAL_CONN_DATA_METHOD(stmt_init)(connection TSRMLS_CC);
			MYSQLND_STMT_DATA * stmt_to_prepare = s_to_prepare ? s_to_prepare->data:NULL;
			if (!s_to_prepare || !s_to_prepare->data) {
				MYSQLND_MS_DBG_ERR("s_to_prepare is null!");
				SET_STMT_ERROR(stmt, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Something wrong: switching statement, but s_to_prepare is null.");
				ret = FAIL;
			} else {
				stmt_to_prepare->execute_count = stmt->execute_count;
				stmt_to_prepare->prefetch_rows = stmt->prefetch_rows;
				stmt_to_prepare->flags = stmt->flags;
				stmt_to_prepare->update_max_length = stmt->update_max_length;
			}
			if (ret == PASS && (ret = ms_orig_mysqlnd_stmt_methods->prepare(s_to_prepare, query, query_len TSRMLS_CC)) == FAIL) {
				COPY_CLIENT_ERROR(_ms_p_ei stmt->error_info, *stmt_to_prepare->error_info);
			}
			if (ret == PASS && (stmt_to_prepare->field_count != stmt->field_count || stmt_to_prepare->param_count != stmt->param_count)) {
				SET_STMT_ERROR(stmt, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Something wrong: switching statement, but field or param count does not match.");
				ret = FAIL;
			}
			if (ret == PASS) {
				MYSQLND_MS_STMT_DATA ** stmt_data_to_prepare;
				MS_LOAD_STMT_DATA(stmt_data_to_prepare, s_to_prepare);
				*stmt_data_to_prepare = *stmt_data;
				*stmt_data = NULL;
				stmt_data = stmt_data_to_prepare;
				stmt_to_prepare->param_bind = stmt->param_bind;
				stmt->param_bind = NULL;
				stmt_to_prepare->result_bind = stmt->result_bind;
				if (stmt_to_prepare->param_count) {
					stmt_to_prepare->send_types_to_server = 1;
				}
				stmt->result_bind = NULL;
#if PHP_VERSION_ID < 70400
				stmt_to_prepare->result_zvals_separated_once = stmt->result_zvals_separated_once;
#endif
				{
					MYSQLND_MS_STMT_DATA ** stmt_data;
					size_t real_size = sizeof(MYSQLND_STMT) + mysqlnd_plugin_count() * sizeof(void *);
					char * tmp_swap = mnd_malloc(real_size);
					memcpy(tmp_swap, s, real_size);
					memcpy(s, s_to_prepare, real_size);
					memcpy(s_to_prepare, tmp_swap, real_size);
					mnd_free(tmp_swap);
				}
				if (stmt_to_prepare != s->data || stmt != s_to_prepare->data || (stmt_to_prepare->param_bind && !stmt_to_prepare->send_types_to_server)) {
					MYSQLND_MS_DBG_INF_FMT("Something wrong in stmt switch copy %s", query);
				}
				stmt = stmt_to_prepare;
			}
			if (s_to_prepare) {
				s_to_prepare->m->dtor(s_to_prepare, TRUE TSRMLS_CC);
			}
			if (TRUE == free_query) {
				MYSQLND_MS_DBG_INF_FMT("Free query %p", query);
				efree((void *)query);
			}
			if (ret == FAIL) {
				if (CONN_DATA_TRX_SET(conn_data)) {
					MYSQLND_MS_GTID_CALL((*conn_data)->global_trx.m->gtid_reset, (*conn_data)->proxy_conn, FAIL TSRMLS_CC);
				}
				MYSQLND_MS_DBG_RETURN(FAIL);
			}
		} else if (TRUE == free_query) {
			MYSQLND_MS_DBG_INF_FMT("Free query %p", query);
			efree((void *)query);
		}
	}
	if (CONN_DATA_TRX_SET(conn_data) && FALSE == (*conn_data)->stgy.in_transaction && (*conn_data)->global_trx.is_master) {
		MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, (*conn_data)->proxy_conn);
		if ((*proxy_conn_data)->global_trx.injectable_query == TRUE) {
			inject = TRUE;
			/* autocommit mode */
			ret = MYSQLND_MS_GTID_CALL_PASS((*conn_data)->global_trx.m->gtid_inject_before, connection TSRMLS_CC);
			if (FAIL == ret) {
				inject = FALSE;
				MYSQLND_MS_INC_STATISTIC(MS_STAT_GTID_AUTOCOMMIT_FAILURE);
				if (TRUE == (*conn_data)->global_trx.report_error) {
					/* user stmt returns false and shall have error set */
					if ((MYSQLND_MS_ERROR_INFO(connection)).error_no != 0) {
						SET_STMT_ERROR(stmt,
							(MYSQLND_MS_ERROR_INFO(connection)).error_no,
							(MYSQLND_MS_ERROR_INFO(connection)).sqlstate,
							(MYSQLND_MS_ERROR_INFO(connection)).error);
					} else {
						SET_CLIENT_ERROR(_ms_p_ei (connection->error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Error on gtid_inject_before");
					}
					MYSQLND_MS_DBG_RETURN(ret);
				}
				SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(connection));
				ret = PASS;
			}
		}
	}

	do {
		ret = ms_orig_mysqlnd_stmt_methods->execute(s TSRMLS_CC);
		stmt_errno = ms_orig_mysqlnd_stmt_methods->get_error_no(s TSRMLS_CC);
		MS_CHECK_FOR_TRANSIENT_ERROR(stmt_errno, conn_data, transient_error_no);
		if (transient_error_no) {
			MYSQLND_MS_DBG_INF_FMT("Transient error "MYSQLND_LLU_SPEC, transient_error_no);
			transient_error_retries++;
			if (transient_error_retries <= (*conn_data)->stgy.transient_error_max_retries) {
				MYSQLND_MS_INC_STATISTIC(MS_STAT_TRANSIENT_ERROR_RETRIES);
				MYSQLND_MS_DBG_INF_FMT("Retry attempt %i/%i. Sleeping for "MYSQLND_LLU_SPEC" ms and retrying.",
							transient_error_retries,
							(*conn_data)->stgy.transient_error_max_retries,
							(*conn_data)->stgy.transient_error_usleep_before_retry);
#if HAVE_USLEEP
				usleep((*conn_data)->stgy.transient_error_usleep_before_retry);
#endif
			} else {
				MYSQLND_MS_DBG_INF("No more transient error retries allowed");
				break;
			}
		}
	} while (transient_error_no);

	if (CONN_DATA_TRX_SET(conn_data) && inject == TRUE) {
		/* autocommit mode */
		enum_func_status jret = MYSQLND_MS_GTID_CALL_PASS((*conn_data)->global_trx.m->gtid_inject_after, connection, ret TSRMLS_CC);
		if (ret == PASS) {
			MYSQLND_MS_INC_STATISTIC((PASS == jret) ? MS_STAT_GTID_AUTOCOMMIT_SUCCESS :
				MS_STAT_GTID_AUTOCOMMIT_FAILURE);
			if (FAIL == jret) {
				if (TRUE == (*conn_data)->global_trx.report_error) {
					MYSQLND_MS_DBG_INF("Report Error");
					if ((MYSQLND_MS_ERROR_INFO(connection)).error_no != 0) {
						SET_STMT_ERROR(stmt,
							(MYSQLND_MS_ERROR_INFO(connection)).error_no,
							(MYSQLND_MS_ERROR_INFO(connection)).sqlstate,
							(MYSQLND_MS_ERROR_INFO(connection)).error);
					} else {
						SET_CLIENT_ERROR(_ms_p_ei (connection->error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Error on gtid_inject_after");
					}
					MYSQLND_MS_DBG_RETURN(jret);
				}
				SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(stmt));
				SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(connection));
			}
		}
	}
	MYSQLND_MS_DBG_RETURN(ret);
}
/* }}} */

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

	DBG_INF_FMT("Close type %d Using thread "MYSQLND_LLU_SPEC, close_type, MS_GET_CONN_DATA_FROM_CONN(conn)->thread_id);

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


// CLONED FROM mysqlnd_wireprotocol.c
/* {{{ php_mysqlnd_net_field_length
   Get next field's length */
static unsigned long
my_php_mysqlnd_net_field_length(zend_uchar **packet)
{
	register zend_uchar *p= (zend_uchar *)*packet;

	if (*p < 251) {
		(*packet)++;
		return (unsigned long) *p;
	}

	switch (*p) {
		case 251:
			(*packet)++;
			return MYSQLND_NULL_LENGTH;
		case 252:
			(*packet) += 3;
			return (unsigned long) uint2korr(p+1);
		case 253:
			(*packet) += 4;
			return (unsigned long) uint3korr(p+1);
		default:
			(*packet) += 9;
			return (unsigned long) uint4korr(p+1);
	}
}
/* }}} */


/* {{{ php_mysqlnd_net_field_length_ll
   Get next field's length */
static uint64_t
my_php_mysqlnd_net_field_length_ll(zend_uchar **packet)
{
	register zend_uchar *p= (zend_uchar *)*packet;

	if (*p < 251) {
		(*packet)++;
		return (uint64_t) *p;
	}

	switch (*p) {
		case 251:
			(*packet)++;
			return (uint64_t) MYSQLND_NULL_LENGTH;
		case 252:
			(*packet) += 3;
			return (uint64_t) uint2korr(p + 1);
		case 253:
			(*packet) += 4;
			return (uint64_t) uint3korr(p + 1);
		default:
			(*packet) += 9;
			return (uint64_t) uint8korr(p + 1);
	}
}
/* }}} */

/* {{{ my_php_mysqlnd_read_error_from_line */
static enum_func_status
my_php_mysqlnd_read_error_from_line(zend_uchar *buf, size_t buf_len,
								char *error, int error_buf_len,
								unsigned int *error_no, char *sqlstate TSRMLS_DC)
{
	zend_uchar *p = buf;
	int error_msg_len= 0;

	DBG_ENTER("my_php_mysqlnd_read_error_from_line");

	*error_no = CR_UNKNOWN_ERROR;
	memcpy(sqlstate, "HY000", MYSQLND_SQLSTATE_LENGTH);

	if (buf_len > 2) {
		*error_no = uint2korr(p);
		p+= 2;
		/*
		  sqlstate is following. No need to check for buf_left_len as we checked > 2 above,
		  if it was >=2 then we would need a check
		*/
		if (*p == '#') {
			++p;
			if ((buf_len - (p - buf)) >= MYSQLND_SQLSTATE_LENGTH) {
				memcpy(sqlstate, p, MYSQLND_SQLSTATE_LENGTH);
				p+= MYSQLND_SQLSTATE_LENGTH;
			} else {
				goto end;
			}
		}
		if ((buf_len - (p - buf)) > 0) {
			error_msg_len = MIN((int)((buf_len - (p - buf))), (int) (error_buf_len - 1));
			memcpy(error, p, error_msg_len);
		}
	}
end:
	sqlstate[MYSQLND_SQLSTATE_LENGTH] = '\0';
	error[error_msg_len]= '\0';

	DBG_RETURN(FAIL);
}
/* }}} */

static enum_func_status
mysqlnd_ms_read_header(_MS_PROTOCOL_CONN_READ_NET_D , MYSQLND_PACKET_HEADER * header,
					MYSQLND_STATS * conn_stats, MYSQLND_ERROR_INFO * error_info TSRMLS_DC)
{
	zend_uchar buffer[MYSQLND_HEADER_SIZE];

	DBG_ENTER("mysqlnd_ms_read_header");
	DBG_INF_FMT("compressed=%u", net->data->compressed);
	if (FAIL == _ms_net_receive(_MS_PROTOCOL_CONN_READ_NET_A, buffer, MYSQLND_HEADER_SIZE, conn_stats, error_info TSRMLS_CC)) {
		DBG_RETURN(FAIL);
	}

	header->size = uint3korr(buffer);
	header->packet_no = uint1korr(buffer + 3);

#ifdef MYSQLND_DUMP_HEADER_N_BODY
	DBG_INF_FMT("HEADER: prot_packet_no=%u size=%3u", header->packet_no, header->size);
#endif
	MYSQLND_INC_CONN_STATISTIC_W_VALUE2(conn_stats,
							STAT_PROTOCOL_OVERHEAD_IN, MYSQLND_HEADER_SIZE,
							STAT_PACKETS_RECEIVED, 1);

	if (net->data->compressed || _ms_net_packet_no == header->packet_no) {
		/*
		  Have to increase the number, so we can send correct number back. It will
		  round at 255 as this is unsigned char. The server needs this for simple
		  flow control checking.
		*/
		_ms_net_packet_no++;
		DBG_RETURN(PASS);
	}

	DBG_ERR_FMT("Logical link: packets out of order. Expected %u received %u. Packet size="MYSQLND_SZ_T_SPEC,
			_ms_net_packet_no, header->packet_no, header->size);

	php_error(E_WARNING, "Packets out of order. Expected %u received %u. Packet size="MYSQLND_SZ_T_SPEC,
			_ms_net_packet_no, header->packet_no, header->size);
	DBG_RETURN(FAIL);
}

/* {{{ mysqlnd_read_packet_header_and_body */
static enum_func_status
mysqlnd_ms_read_packet_header_and_body(MYSQLND_PACKET_HEADER * header, MYSQLND_CONN_DATA * conn,
									zend_uchar * buf, size_t buf_size, const char * const packet_type_as_text,
									enum mysqlnd_packet_type packet_type)
{
	_MS_PROTOCOL_CONN_LOAD_NET_D;
	_MS_PROTOCOL_CONN_LOAD_VIO_D;
	DBG_ENTER("mysqlnd_ms_read_packet_header_and_body");
	DBG_INF_FMT("buf=%p size=%u", (buf), (buf_size));
	if (FAIL == mysqlnd_ms_read_header(_MS_PROTOCOL_CONN_READ_NET_A, header, (conn)->stats, ((conn)->error_info) TSRMLS_CC)) {
		_MS_CONN_SET_STATE(conn, CONN_QUIT_SENT);
		SET_CLIENT_ERROR(_ms_p_ei (conn->error_info), CR_SERVER_GONE_ERROR, UNKNOWN_SQLSTATE, mysqlnd_server_gone);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", mysqlnd_server_gone);
		DBG_ERR_FMT("Can't read %s's header", (packet_type_as_text));
		DBG_RETURN(FAIL);
	}
	if ((buf_size) < header->size) {
		DBG_ERR_FMT("Packet buffer %u wasn't big enough %u, %u bytes will be unread",
					(buf_size), header->size, header->size - (buf_size));
					DBG_RETURN(FAIL);
	}
	if (FAIL == _ms_net_receive(_MS_PROTOCOL_CONN_READ_NET_A, (buf), header->size, (conn)->stats, ((conn)->error_info) TSRMLS_CC)) {
		_MS_CONN_SET_STATE(conn, CONN_QUIT_SENT);
		SET_CLIENT_ERROR(_ms_p_ei (conn->error_info), CR_SERVER_GONE_ERROR, UNKNOWN_SQLSTATE, mysqlnd_server_gone);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", mysqlnd_server_gone);
		DBG_ERR_FMT("Empty '%s' packet body", (packet_type_as_text));
		DBG_RETURN(FAIL);
	}
	MYSQLND_INC_CONN_STATISTIC_W_VALUE2(conn->stats, packet_type_to_statistic_byte_count[packet_type],
										MYSQLND_HEADER_SIZE + header->size,
										packet_type_to_statistic_packet_count[packet_type],
										1);
	DBG_RETURN(PASS);

}
/* }}} */


/* {{{ mysqlnd_ms_protocol_rset_header_read */
static enum_func_status
mysqlnd_ms_protocol_rset_header_read(_MS_PROTOCOL_CONN_READ_D TSRMLS_DC)
{
	MYSQLND_PACKET_RSET_HEADER *packet= (MYSQLND_PACKET_RSET_HEADER *) _packet;
	MYSQLND_PACKET_HEADER * header = &packet->header;
	enum_func_status ret = PASS;
	_MS_PROTOCOL_CONN_LOAD_CONN_D;
	_MS_PROTOCOL_CONN_LOAD_NET_D;
	size_t buf_len = net->cmd_buffer.length;
	zend_uchar *buf = (zend_uchar *) net->cmd_buffer.buffer;
	zend_uchar *p = buf;
	zend_uchar *begin = buf;
	size_t len = 0;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);

	DBG_ENTER("mysqlnd_ms_protocol_rset_header_read");
	if (!conn_data || !(*conn_data) || (*conn_data)->global_trx.type == GTID_NONE ||
			!(*conn_data)->global_trx.is_master || !(conn->server_capabilities & CLIENT_SESSION_TRACK) || !(*conn_data)->global_trx.m->gtid_set_last_write) {
		DBG_INF_FMT("call original rset_header_read: client_session_track=%lu", (conn->server_capabilities & CLIENT_SESSION_TRACK));
		ret = ms_orig_rset_header_read(_MS_PROTOCOL_CONN_READ_A TSRMLS_CC);
		DBG_RETURN(ret);
	}

	if (FAIL == mysqlnd_ms_read_packet_header_and_body(header, conn, buf, buf_len, "resultset header", PROT_RSET_HEADER_PACKET)) {
		DBG_RETURN(FAIL);
	}
	BAIL_IF_NO_MORE_DATA;

	/*
	  Don't increment. First byte is ERROR_MARKER on error, but otherwise is starting byte
	  of encoded sequence for length.
	*/
	if (ERROR_MARKER == *p) {
		/* Error */
		p++;
		BAIL_IF_NO_MORE_DATA;
		my_php_mysqlnd_read_error_from_line(p, packet->header.size - 1,
										 packet->error_info.error, sizeof(packet->error_info.error),
										 &packet->error_info.error_no, packet->error_info.sqlstate
										 TSRMLS_CC);
		DBG_INF_FMT("conn->server_status=%u", conn->upsert_status->server_status);
		DBG_RETURN(PASS);
	}

	packet->field_count = my_php_mysqlnd_net_field_length(&p);
	BAIL_IF_NO_MORE_DATA;

	switch (packet->field_count) {
		case MYSQLND_NULL_LENGTH:
			DBG_INF("LOAD LOCAL");
			/*
			  First byte in the packet is the field count.
			  Thus, the name is size - 1. And we add 1 for a trailing \0.
			  Because we have BAIL_IF_NO_MORE_DATA before the switch, we are guaranteed
			  that packet->header.size is > 0. Which means that len can't underflow, that
			  would lead to 0 byte allocation but 2^32 or 2^64 bytes copied.
			*/
			len = packet->header.size - 1;
			MYSQLND_MS_CONN_STRING(packet->info_or_local_file) = mnd_emalloc(len + 1);
			if (MYSQLND_MS_CONN_STRING(packet->info_or_local_file)) {
				memcpy(MYSQLND_MS_CONN_STRING(packet->info_or_local_file), p, len);
				MYSQLND_MS_CONN_STRING(packet->info_or_local_file)[len] = '\0';
				MYSQLND_MS_CONN_STRING_LEN(packet->info_or_local_file) = len;
			} else {
				SET_OOM_ERROR(_ms_p_ei conn->error_info);
				ret = FAIL;
			}
			break;
		case 0x00:
			DBG_INF("UPSERT");
			/*
			 * Verr� chiamata direttamente la read dell'OK
			 * (attenzione il byte iniziale � gi� stato letto)
			 * L'OK avr� le informazioni di SESSION_TRACK le informazioni
			 * I valori estratti verranno copiati nel pacchetto di destinazione
			 * il message verra copiato in info_or_local_file e poi messo a NULL
			 *
			 */

			packet->affected_rows = my_php_mysqlnd_net_field_length_ll(&p);
			BAIL_IF_NO_MORE_DATA;

			packet->last_insert_id = my_php_mysqlnd_net_field_length_ll(&p);
			BAIL_IF_NO_MORE_DATA;

			packet->server_status = uint2korr(p);
			p+=2;
			BAIL_IF_NO_MORE_DATA;

			packet->warning_count = uint2korr(p);
			p+=2;
			BAIL_IF_NO_MORE_DATA;
			if (conn->server_capabilities & CLIENT_SESSION_TRACK) {
				/* Check for additional textual data */
				if (packet->header.size > (size_t) (p - buf) && (len = my_php_mysqlnd_net_field_length(&p))) {
					MYSQLND_MS_CONN_STRING_LEN(packet->info_or_local_file) = MIN(len, buf_len - (p - begin));
					MYSQLND_MS_CONN_STRING(packet->info_or_local_file) = mnd_pestrndup((char *)p, MYSQLND_MS_CONN_STRING_LEN(packet->info_or_local_file), FALSE);
				} else {
					MYSQLND_MS_CONN_STRING(packet->info_or_local_file) = NULL;
					MYSQLND_MS_CONN_STRING_LEN(packet->info_or_local_file) = 0;
				}
				p += MYSQLND_MS_CONN_STRING_LEN(packet->info_or_local_file);
				DBG_INF_FMT("Active session track: header size %lu done %lu session state changed %lu info_or_local_file_len %lu retrieved len %lu", packet->header.size, (size_t) (p - buf), (packet->server_status & SERVER_SESSION_STATE_CHANGED), MYSQLND_MS_CONN_STRING_LEN(packet->info_or_local_file), len);
				if (packet->server_status & SERVER_SESSION_STATE_CHANGED) {
					zend_uchar * sp;
					unsigned long total_len = my_php_mysqlnd_net_field_length(&p);
					unsigned long type;
					BAIL_IF_NO_MORE_DATA;
					DBG_INF_FMT("State changed: total len %lu", total_len);

					while (total_len > 0) {
						sp = p;
						type = (enum enum_session_state_type) my_php_mysqlnd_net_field_length(&p);
						if (type == SESSION_TRACK_GTIDS) {
							char * gtid = NULL;
							/* Move past the total length of the changed entity. */

							(void) my_php_mysqlnd_net_field_length(&p);

							/* read (and ignore for now) the GTIDS encoding specification code */
							(void) my_php_mysqlnd_net_field_length(&p);

							/*
							 For now we ignore the encoding specification, since only one
							 is supported. In the future the decoding of what comes next
							 depends on the specification code.
							 */

							/* read the length of the encoded string. */
							len = my_php_mysqlnd_net_field_length(&p);
							len = MIN(len, buf_len - (p - begin));
							gtid = mnd_pestrndup((char *)p, len, FALSE);
							if (FAIL == (*conn_data)->global_trx.m->gtid_set_last_write(conn, gtid TSRMLS_CC)) {
								php_error_docref(NULL TSRMLS_CC, E_WARNING, "OK packet set GTID failed %s", gtid);
								if ((*conn_data)->global_trx.report_error == TRUE) {
									if ((MYSQLND_MS_ERROR_INFO(conn)).error_no == 0) {
										SET_CLIENT_ERROR(_ms_p_ei (conn->error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Error on gtid_set_last_write");
									}
									ret = FAIL;
								}
							}
							mnd_pefree(gtid, FALSE);
							break;
						} else {
							/*
							   Unneeded/unsupported type received, get the total length and move on
							*/
							len = my_php_mysqlnd_net_field_length(&p);
							DBG_INF_FMT("unsupported type %d len %d", type, len);
						}
						p += len;
						total_len -= (p - sp);
					}
				}
			} else {
				/* Check for additional textual data */
				if (packet->header.size  > (size_t) (p - buf) && (len = my_php_mysqlnd_net_field_length(&p))) {
					MYSQLND_MS_CONN_STRING(packet->info_or_local_file) = mnd_emalloc(len + 1);
					if (MYSQLND_MS_CONN_STRING(packet->info_or_local_file)) {
						memcpy(MYSQLND_MS_CONN_STRING(packet->info_or_local_file), p, len);
						MYSQLND_MS_CONN_STRING(packet->info_or_local_file)[len] = '\0';
						MYSQLND_MS_CONN_STRING_LEN(packet->info_or_local_file) = len;
					} else {
						SET_OOM_ERROR(_ms_p_ei conn->error_info);
						ret = FAIL;
					}
				}
			}
			DBG_INF_FMT("affected_rows=%llu last_insert_id=%llu server_status=%u warning_count=%u",
						packet->affected_rows, packet->last_insert_id,
						packet->server_status, packet->warning_count);
			break;
		default:
			DBG_INF("SELECT");
			/* Result set */
			break;
	}
	BAIL_IF_NO_MORE_DATA;

	DBG_RETURN(ret);
premature_end:
	DBG_ERR_FMT("RSET_HEADER packet %d bytes shorter than expected", p - begin - packet->header.size);
	php_error_docref(NULL TSRMLS_CC, E_WARNING, "RSET_HEADER packet "MYSQLND_SZ_T_SPEC" bytes shorter than expected",
					 p - begin - packet->header.size);
	DBG_RETURN(FAIL);
}
/* }}} */

static enum_func_status
mysqlnd_ms_protocol_ok_read(_MS_PROTOCOL_CONN_READ_D TSRMLS_DC)
{
	register MYSQLND_PACKET_OK *packet= (MYSQLND_PACKET_OK *) _packet;
	MYSQLND_PACKET_HEADER * header = &packet->header;
	enum_func_status ret = PASS;
	_MS_PROTOCOL_CONN_LOAD_CONN_D;
	_MS_PROTOCOL_CONN_LOAD_NET_D;
	zend_uchar local_buf[OK_BUFFER_SIZE];
	size_t buf_len = net->cmd_buffer.buffer? net->cmd_buffer.length : OK_BUFFER_SIZE;
	zend_uchar *buf = net->cmd_buffer.buffer? (zend_uchar *) net->cmd_buffer.buffer : local_buf;
	zend_uchar *p = buf;
	zend_uchar *begin = buf;
	unsigned long i;
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	DBG_ENTER("mysqlnd_ms_ok_read");
	if (!CONN_DATA_TRX_SET(conn_data) || !(*conn_data)->global_trx.is_master || !(conn->server_capabilities & CLIENT_SESSION_TRACK) || !(*conn_data)->global_trx.m->gtid_set_last_write) {
		DBG_INF_FMT("call original read_ok: client_session_track=%lu", (conn->server_capabilities & CLIENT_SESSION_TRACK));
		ret = ms_orig_ok_read(_MS_PROTOCOL_CONN_READ_A TSRMLS_CC);
		DBG_RETURN(ret);
	}

	if (FAIL == mysqlnd_ms_read_packet_header_and_body(header, conn, buf, buf_len, "OK", PROT_OK_PACKET)) {
		DBG_RETURN(FAIL);
	}
	BAIL_IF_NO_MORE_DATA;

	/* Should be always 0x0 or ERROR_MARKER for error */
	packet->field_count = uint1korr(p);
	p++;
	BAIL_IF_NO_MORE_DATA;

	if (ERROR_MARKER == packet->field_count) {
		my_php_mysqlnd_read_error_from_line(p, packet->header.size - 1,
										 packet->error, sizeof(packet->error),
										 &packet->error_no, packet->sqlstate
										 TSRMLS_CC);
		DBG_INF_FMT("conn->server_status=%u", conn->upsert_status->server_status);
		DBG_RETURN(PASS);
	}
	/* Everything was fine! */
	packet->affected_rows  = my_php_mysqlnd_net_field_length_ll(&p);
	BAIL_IF_NO_MORE_DATA;

	packet->last_insert_id = my_php_mysqlnd_net_field_length_ll(&p);
	BAIL_IF_NO_MORE_DATA;

	packet->server_status = uint2korr(p);
	p+= 2;
	BAIL_IF_NO_MORE_DATA;

	packet->warning_count = uint2korr(p);
	p+= 2;
	BAIL_IF_NO_MORE_DATA;

	if (conn->server_capabilities & CLIENT_SESSION_TRACK) {
		/* There is a message */
		if (packet->header.size > (size_t) (p - buf) && (i = my_php_mysqlnd_net_field_length(&p))) {
			packet->message_len = MIN(i, buf_len - (p - begin));
			packet->message = mnd_pestrndup((char *)p, packet->message_len, FALSE);
		} else {
			packet->message = NULL;
			packet->message_len = 0;
		}
		p += packet->message_len;
		DBG_INF_FMT("Active session track: header size %lu done %lu session state changed %lu", packet->header.size, (size_t) (p - buf), (packet->server_status & SERVER_SESSION_STATE_CHANGED));
		if (packet->server_status & SERVER_SESSION_STATE_CHANGED) {
			zend_uchar * sp;
			unsigned long total_len = my_php_mysqlnd_net_field_length(&p);
			unsigned long type;
			BAIL_IF_NO_MORE_DATA;

			while (total_len > 0) {
				sp = p;
				type = (enum enum_session_state_type) my_php_mysqlnd_net_field_length(&p);
				if (type == SESSION_TRACK_GTIDS) {
					char * gtid = NULL;
					/* Move past the total length of the changed entity. */

					(void) my_php_mysqlnd_net_field_length(&p);

					/* read (and ignore for now) the GTIDS encoding specification code */
					(void) my_php_mysqlnd_net_field_length(&p);

					/*
					 For now we ignore the encoding specification, since only one
					 is supported. In the future the decoding of what comes next
					 depends on the specification code.
					 */

					/* read the length of the encoded string. */
					i = my_php_mysqlnd_net_field_length(&p);
					i = MIN(i, buf_len - (p - begin));
					gtid = mnd_pestrndup((char *)p, i, FALSE);
					if (FAIL == (*conn_data)->global_trx.m->gtid_set_last_write(conn, gtid TSRMLS_CC)) {
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "OK packet set GTID failed %s", gtid);
						if ((*conn_data)->global_trx.report_error == TRUE) {
							mnd_pefree(gtid, FALSE);
							if ((MYSQLND_MS_ERROR_INFO(conn)).error_no == 0) {
								SET_CLIENT_ERROR(_ms_p_ei (conn->error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Error on gtid_set_last_write");
							}
							DBG_RETURN(FAIL);
						}
					}
					mnd_pefree(gtid, FALSE);
					break;
				} else {
					/*
					   Unneeded/unsupported type received, get the total length and move
					   past it.
					   */
					i = my_php_mysqlnd_net_field_length(&p);
					DBG_INF_FMT("unsupported type %d len %d", type, i);
				}
				p += i;
				total_len -= (p - sp);
			}
		}
	} else {
		/* There is a message */
		if (packet->header.size > (size_t) (p - buf) && (i = my_php_mysqlnd_net_field_length(&p))) {
			packet->message_len = MIN(i, buf_len - (p - begin));
			packet->message = mnd_pestrndup((char *)p, packet->message_len, FALSE);
		} else {
			packet->message = NULL;
			packet->message_len = 0;
		}
	}
	DBG_INF_FMT("OK packet: aff_rows=%lld last_ins_id=%ld server_status=%u warnings=%u",
				packet->affected_rows, packet->last_insert_id, packet->server_status,
				packet->warning_count);

	BAIL_IF_NO_MORE_DATA;

	DBG_RETURN(PASS);
premature_end:
	DBG_ERR_FMT("OK packet %d bytes shorter than expected", p - begin - packet->header.size);
	php_error_docref(NULL TSRMLS_CC, E_WARNING, "OK packet "MYSQLND_SZ_T_SPEC" bytes shorter than expected",
					 p - begin - packet->header.size);
	DBG_RETURN(FAIL);
}
/* }}} */

#if PHP_VERSION_ID < 70300
/* {{{ mysqlnd_ms_protocol::get_rset_header_packet */
static struct st_mysqlnd_packet_rset_header *
MYSQLND_METHOD(mysqlnd_ms_protocol, get_rset_header_packet)(_MS_PROTOCOL_TYPE * const protocol, zend_bool persistent TSRMLS_DC)
{
	struct st_mysqlnd_packet_rset_header * packet = NULL;
	DBG_ENTER("mysqlnd_ms_protocol::get_rset_header_packet");
	packet = ms_orig_mysqlnd_protocol_methods->get_rset_header_packet(protocol, persistent TSRMLS_CC);
	if (packet && packet->header.m->read_from_net != mysqlnd_ms_protocol_rset_header_read) {
		ms_orig_rset_header_read = packet->header.m->read_from_net;
		packet->header.m->read_from_net = mysqlnd_ms_protocol_rset_header_read;
	}
	DBG_RETURN(packet);
}
/* }}} */

/* {{{ mysqlnd_ms_protocol::get_ok_packet */
static struct st_mysqlnd_packet_ok *
MYSQLND_METHOD(mysqlnd_ms_protocol, get_ok_packet)(_MS_PROTOCOL_TYPE * const protocol, zend_bool persistent TSRMLS_DC)
{
	struct st_mysqlnd_packet_ok * packet = NULL;
	DBG_ENTER("mysqlnd_ms_protocol::get_ok_packet");
	packet = ms_orig_mysqlnd_protocol_methods->get_ok_packet(protocol, persistent TSRMLS_CC);
	if (packet && packet->header.m->read_from_net != mysqlnd_ms_protocol_ok_read) {
		ms_orig_ok_read = packet->header.m->read_from_net;
		packet->header.m->read_from_net = mysqlnd_ms_protocol_ok_read;
	}
	DBG_RETURN(packet);
}
/* }}} */
#else
/* {{{ mysqlnd_ms_protocol::init_rset_header_packet */
static void
MYSQLND_METHOD(mysqlnd_ms_protocol, init_rset_header_packet)(struct st_mysqlnd_packet_rset_header * packet TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_protocol::init_rset_header_packet");
	ms_orig_mysqlnd_protocol_methods->init_rset_header_packet(packet TSRMLS_CC);
	if (packet && packet->header.m->read_from_net != mysqlnd_ms_protocol_rset_header_read) {
		ms_orig_rset_header_read = packet->header.m->read_from_net;
		packet->header.m->read_from_net = mysqlnd_ms_protocol_rset_header_read;
	}
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_protocol::init_ok_packet */
static void
MYSQLND_METHOD(mysqlnd_ms_protocol, init_ok_packet)(struct st_mysqlnd_packet_ok * packet TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_protocol::init_ok_packet");
	ms_orig_mysqlnd_protocol_methods->init_ok_packet(packet TSRMLS_CC);
	if (packet && packet->header.m->read_from_net != mysqlnd_ms_protocol_ok_read) {
		ms_orig_ok_read = packet->header.m->read_from_net;
		packet->header.m->read_from_net = mysqlnd_ms_protocol_ok_read;
	}
	DBG_VOID_RETURN;
}
/* }}} */
#endif

/* {{{ mysqlnd_ms::init */
/*
static enum_func_status
MYSQLND_METHOD(mysqlnd_ms, init)(MYSQLND_CONN_DATA * conn TSRMLS_DC)
{
	enum_func_status ret;
	DBG_ENTER("mysqlnd_ms::init");
	ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(init)(conn TSRMLS_CC);
//	conn->protocol->m.get_ok_packet = MYSQLND_METHOD(mysqlnd_ms_protocol, get_ok_packet);
//	conn->protocol->m.get_rset_header_packet = MYSQLND_METHOD(mysqlnd_ms_protocol, get_rset_header_packet);
	DBG_RETURN(ret);

}
*/
/* }}} */

/* {{{ _mysqlnd_stmt_init */
MYSQLND_STMT *
MYSQLND_METHOD(mysqlnd_ms, stmt_init)(MYSQLND_CONN_DATA * const conn)
{
	MYSQLND_STMT * ret;
	DBG_ENTER("mysqlnd_ms::stmt_init");
	ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(stmt_init)(conn TSRMLS_CC);
	if (!ms_orig_mysqlnd_stmt_methods) {
		ms_orig_mysqlnd_stmt_methods = ret->m;
		my_mysqlnd_stmt_methods = *ms_orig_mysqlnd_stmt_methods;
		ms_orig_mysqlnd_stmt_methods->prepare = MYSQLND_METHOD(mysqlnd_ms_stmt, prepare);
		ms_orig_mysqlnd_stmt_methods->execute = MYSQLND_METHOD(mysqlnd_ms_stmt, execute);
		ms_orig_mysqlnd_stmt_methods->dtor = MYSQLND_METHOD(mysqlnd_ms_stmt, dtor);
		ms_orig_mysqlnd_stmt_methods = &my_mysqlnd_stmt_methods;
	}
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_register_hooks*/
void
mysqlnd_ms_register_hooks()
{
	MS_LOAD_AND_COPY_CONN_HANDLE_METHODS(ms_orig_mysqlnd_conn_handle_methods, my_mysqlnd_conn_handle_methods);
	ms_orig_mysqlnd_conn_handle_methods->close		= MYSQLND_METHOD(mysqlnd_ms, close);
	ms_orig_mysqlnd_conn_handle_methods = &my_mysqlnd_conn_handle_methods;
/*
	MS_LOAD_AND_COPY_CONN_HANDLE_METHODS(ms_orig_mysqlnd_conn_handle_methods, my_mysqlnd_conn_handle_methods);
	my_mysqlnd_conn_handle_methods.close		= MYSQLND_METHOD(mysqlnd_ms, close);
	MS_SET_CONN_HANDLE_METHODS(&my_mysqlnd_conn_handle_methods);
*/
	MS_LOAD_AND_COPY_CONN_DATA_METHODS(ms_orig_mysqlnd_conn_methods, my_mysqlnd_conn_methods);
	/* my_mysqlnd_conn_methods.init				= MYSQLND_METHOD(mysqlnd_ms, init);*/
	ms_orig_mysqlnd_conn_methods->connect				= MYSQLND_METHOD(mysqlnd_ms, connect);
	ms_orig_mysqlnd_conn_methods->query				= MYSQLND_METHOD(mysqlnd_ms, query);
	ms_orig_mysqlnd_conn_methods->send_query			= MYSQLND_METHOD(mysqlnd_ms, send_query);
	ms_orig_mysqlnd_conn_methods->use_result			= MYSQLND_METHOD(mysqlnd_ms, use_result);
	ms_orig_mysqlnd_conn_methods->store_result		= MYSQLND_METHOD(mysqlnd_ms, store_result);
	ms_orig_mysqlnd_conn_methods->dtor				= MYSQLND_METHOD_PRIVATE(mysqlnd_ms, dtor);
	ms_orig_mysqlnd_conn_methods->escape_string		= MYSQLND_METHOD(mysqlnd_ms, escape_string);
	ms_orig_mysqlnd_conn_methods->change_user			= MYSQLND_METHOD(mysqlnd_ms, change_user);
	ms_orig_mysqlnd_conn_methods->ping				= MYSQLND_METHOD(mysqlnd_ms, ping);
	ms_orig_mysqlnd_conn_methods->kill_connection		= MYSQLND_METHOD(mysqlnd_ms, kill);
	ms_orig_mysqlnd_conn_methods->get_thread_id		= MYSQLND_METHOD(mysqlnd_ms, thread_id);
	ms_orig_mysqlnd_conn_methods->select_db			= MYSQLND_METHOD(mysqlnd_ms, select_db);
	ms_orig_mysqlnd_conn_methods->set_charset			= MYSQLND_METHOD(mysqlnd_ms, set_charset);
	ms_orig_mysqlnd_conn_methods->set_server_option	= MYSQLND_METHOD(mysqlnd_ms, set_server_option);
	ms_orig_mysqlnd_conn_methods->set_client_option	= MYSQLND_METHOD(mysqlnd_ms, set_client_option);
	ms_orig_mysqlnd_conn_methods->next_result			= MYSQLND_METHOD(mysqlnd_ms, next_result);
	ms_orig_mysqlnd_conn_methods->more_results		= MYSQLND_METHOD(mysqlnd_ms, more_results);
	ms_orig_mysqlnd_conn_methods->get_error_no		= MYSQLND_METHOD(mysqlnd_ms, error_no);
	ms_orig_mysqlnd_conn_methods->get_error_str		= MYSQLND_METHOD(mysqlnd_ms, error);
	ms_orig_mysqlnd_conn_methods->get_sqlstate		= MYSQLND_METHOD(mysqlnd_ms, sqlstate);

	ms_orig_mysqlnd_conn_methods->ssl_set				= MYSQLND_METHOD(mysqlnd_ms, ssl_set);


	ms_orig_mysqlnd_conn_methods->get_field_count		= MYSQLND_METHOD(mysqlnd_ms, field_count);
	ms_orig_mysqlnd_conn_methods->get_last_insert_id	= MYSQLND_METHOD(mysqlnd_ms, insert_id);
	ms_orig_mysqlnd_conn_methods->get_affected_rows	= MYSQLND_METHOD(mysqlnd_ms, affected_rows);
	ms_orig_mysqlnd_conn_methods->get_warning_count	= MYSQLND_METHOD(mysqlnd_ms, warning_count);
	ms_orig_mysqlnd_conn_methods->get_last_message	= MYSQLND_METHOD(mysqlnd_ms, info);
	ms_orig_mysqlnd_conn_methods->set_autocommit		= MYSQLND_METHOD(mysqlnd_ms, set_autocommit);
	ms_orig_mysqlnd_conn_methods->tx_commit			= MYSQLND_METHOD(mysqlnd_ms, tx_commit);
	ms_orig_mysqlnd_conn_methods->tx_rollback			= MYSQLND_METHOD(mysqlnd_ms, tx_rollback);
	ms_orig_mysqlnd_conn_methods->tx_commit_or_rollback 	= MYSQLND_METHOD(mysqlnd_ms, tx_commit_or_rollback);
	ms_orig_mysqlnd_conn_methods->tx_begin 				= MYSQLND_METHOD(mysqlnd_ms, tx_begin);
	ms_orig_mysqlnd_conn_methods->get_server_statistics	= MYSQLND_METHOD(mysqlnd_ms, get_server_statistics);
	ms_orig_mysqlnd_conn_methods->get_server_version		= MYSQLND_METHOD(mysqlnd_ms, get_server_version);
	ms_orig_mysqlnd_conn_methods->get_server_information	= MYSQLND_METHOD(mysqlnd_ms, get_server_info);
	ms_orig_mysqlnd_conn_methods->get_host_information	= MYSQLND_METHOD(mysqlnd_ms, get_host_info);
	ms_orig_mysqlnd_conn_methods->get_protocol_information= MYSQLND_METHOD(mysqlnd_ms, get_proto_info);
	ms_orig_mysqlnd_conn_methods->charset_name			= MYSQLND_METHOD(mysqlnd_ms, charset_name);
	ms_orig_mysqlnd_conn_methods->get_statistics			= MYSQLND_METHOD(mysqlnd_ms, get_connection_stats);
	ms_orig_mysqlnd_conn_methods->server_dump_debug_information = MYSQLND_METHOD(mysqlnd_ms, dump_debug_info);
#if PHP_VERSION_ID >= 70100
	ms_orig_mysqlnd_conn_methods->stmt_init				= MYSQLND_METHOD(mysqlnd_ms, stmt_init);
#endif
	ms_orig_mysqlnd_conn_methods = &my_mysqlnd_conn_methods;
	/*
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
*/
#if PHP_VERSION_ID < 70100
	MS_LOAD_AND_COPY_STMT_METHODS(ms_orig_mysqlnd_stmt_methods, my_mysqlnd_stmt_methods);
	ms_orig_mysqlnd_stmt_methods->prepare = MYSQLND_METHOD(mysqlnd_ms_stmt, prepare);
	ms_orig_mysqlnd_stmt_methods->execute = MYSQLND_METHOD(mysqlnd_ms_stmt, execute);
	ms_orig_mysqlnd_stmt_methods->dtor = MYSQLND_METHOD(mysqlnd_ms_stmt, dtor);
	ms_orig_mysqlnd_stmt_methods = &my_mysqlnd_stmt_methods;
#endif
/*
	ms_orig_mysqlnd_stmt_methods = mysqlnd_stmt_get_methods();
	memcpy(&my_mysqlnd_stmt_methods, ms_orig_mysqlnd_stmt_methods, sizeof(struct st_mysqlnd_stmt_methods));

	my_mysqlnd_stmt_methods.prepare = MYSQLND_METHOD(mysqlnd_ms_stmt, prepare);
#ifndef MYSQLND_HAS_INJECTION_FEATURE
	my_mysqlnd_stmt_methods.execute = MYSQLND_METHOD(mysqlnd_ms_stmt, execute);
#endif
	my_mysqlnd_stmt_methods.dtor = MYSQLND_METHOD(mysqlnd_ms_stmt, dtor);

	mysqlnd_stmt_set_methods(&my_mysqlnd_stmt_methods);
*/
	MS_LOAD_AND_COPY_PROTOCOL_METHODS(ms_orig_mysqlnd_protocol_methods, my_mysqlnd_protocol_methods);
#if PHP_VERSION_ID < 70300
	ms_orig_mysqlnd_protocol_methods->get_ok_packet = MYSQLND_METHOD(mysqlnd_ms_protocol, get_ok_packet);
	ms_orig_mysqlnd_protocol_methods->get_rset_header_packet = MYSQLND_METHOD(mysqlnd_ms_protocol, get_rset_header_packet);
#else
	ms_orig_mysqlnd_protocol_methods->init_ok_packet = MYSQLND_METHOD(mysqlnd_ms_protocol, init_ok_packet);
	ms_orig_mysqlnd_protocol_methods->init_rset_header_packet = MYSQLND_METHOD(mysqlnd_ms_protocol, init_rset_header_packet);
#endif
	ms_orig_mysqlnd_protocol_methods = &my_mysqlnd_protocol_methods;
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
