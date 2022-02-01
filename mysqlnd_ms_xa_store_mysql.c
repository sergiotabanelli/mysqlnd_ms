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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "ext/mysqlnd/mysqlnd.h"
#include "ext/mysqlnd/mysqlnd_debug.h"
#include "ext/mysqlnd/mysqlnd_priv.h"

#if PHP_VERSION_ID >= 50400
#include "ext/mysqlnd/mysqlnd_ext_plugin.h"
#endif
#ifndef mnd_emalloc
#include "ext/mysqlnd/mysqlnd_alloc.h"
#endif
#include "mysqlnd_ms_xa.h"
#include "mysqlnd_ms.h"
#include "mysqlnd_ms_config_json.h"


#define MYSQL_STORE_DEFAULT_GC_TABLE "mysqlnd_ms_xa_gc"
#define MYSQL_STORE_DEFAULT_TRX_TABLE "mysqlnd_ms_xa_trx"
#define MYSQL_STORE_DEFAULT_PARTICIPANTS_TABLE "mysqlnd_ms_xa_participants"
#if PHP_MAJOR_VERSION > 7
#define ms_convert_to_string_ex(pstring) convert_to_string_ex(pstring);
#define ms_convert_to_long_ex(plong) convert_to_long_ex(plong);
#else
#define ms_convert_to_string_ex(pstring) convert_to_string_ex(pstring)
#define ms_convert_to_long_ex(plong) convert_to_long_ex(plong)
#endif
#define COPY_SQL_ERROR(from_conn, to_error_info) \
	if ((to_error_info)) { \
		mysqlnd_ms_client_n_php_error((to_error_info), \
									  MYSQLND_MS_ERROR_INFO((from_conn)->data).error_no, \
									  MYSQLND_MS_ERROR_INFO((from_conn)->data).sqlstate, \
									  E_WARNING TSRMLS_CC, \
									  MYSQLND_MS_ERROR_PREFIX " MySQL XA state store error: %s", \
									  MYSQLND_MS_ERROR_INFO((from_conn)->data).error); \
	}

#define GET_ZVAL_STRING_FROM_HASH(pprow, pstring) \
	if ((zend_hash_has_more_elements(Z_ARRVAL_P(_ms_p_zval (pprow))) != SUCCESS) || \
		(_MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data, Z_ARRVAL_P(_ms_p_zval (pprow)), (pstring)) != SUCCESS)) { \
		continue; \
	} \
	ms_convert_to_string_ex((pstring)) \
	zend_hash_move_forward(Z_ARRVAL_P(_ms_p_zval (pprow)));

#define GET_ZVAL_LONG_FROM_HASH(pprow, plong) \
	if ((zend_hash_has_more_elements(Z_ARRVAL_P(_ms_p_zval (pprow))) != SUCCESS) || \
		(_MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data, Z_ARRVAL_P(_ms_p_zval (pprow)), (plong)) != SUCCESS)) { \
		continue; \
	} \
	ms_convert_to_long_ex((plong)) \
	zend_hash_move_forward(Z_ARRVAL_P(_ms_p_zval (pprow)));

typedef struct st_mysqlnd_ms_xa_trx_state_store_mysql {
	char * host;
	unsigned int port;
	unsigned int flags;
	char * socket;
	char * user;
	char * password;
	size_t password_len;
	char * db;
	size_t db_len;
	char * global_table;
	char * participant_table;
	char * gc_table;
	MYSQLND *conn;
} MYSQLND_MS_XA_STATE_STORE_MYSQL;

#if PHP_MAJOR_VERSION > 7
#ifndef mysqlnd_fetch_all
/* {{{ mysqlnd_res::fetch_all */
static void
mysqlnd_fetch_all(MYSQLND_RES * result, const unsigned int flags, zval *return_value ZEND_FILE_LINE_DC)
{
	zval  row;
	zend_ulong i = 0;
	MYSQLND_RES_BUFFERED *set = result->stored_data;

	DBG_ENTER("mysqlnd_res::fetch_all");

	if ((!result->unbuf && !set)) {
		php_error_docref(NULL, E_WARNING, "fetch_all can be used only with buffered sets");
		if (result->conn) {
			SET_CLIENT_ERROR(result->conn->error_info, CR_NOT_IMPLEMENTED, UNKNOWN_SQLSTATE, "fetch_all can be used only with buffered sets");
		}
		RETVAL_NULL();
		DBG_VOID_RETURN;
	}

	/* 4 is a magic value. The cast is safe, if larger then the array will be later extended - no big deal :) */
	array_init_size(return_value, set? (unsigned int) set->row_count : 4);

	do {
		mysqlnd_fetch_into(result, flags, &row);
		if (Z_TYPE(row) != IS_ARRAY) {
			zval_ptr_dtor_nogc(&row);
			break;
		}
		add_index_zval(return_value, i++, &row);
	} while (1);

	DBG_VOID_RETURN;
}
/* }}} */
#endif
#endif

/* {{{ mysqlnd_ms_xa_store_mysql_connect */

static enum_func_status
mysqlnd_ms_xa_store_mysql_connect(void * data, MYSQLND_ERROR_INFO *error_info TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	MYSQLND_MS_XA_STATE_STORE_MYSQL * store_data = (MYSQLND_MS_XA_STATE_STORE_MYSQL *)data;
	DBG_ENTER("mysqlnd_ms_xa_store_mysql_connect");

	if (!store_data) {
		DBG_RETURN(ret);
	}

#if PHP_VERSION_ID < 50600
	store_data->conn = mysqlnd_init(FALSE);
	if (mysqlnd_connect(store_data->conn, store_data->host, store_data->user, store_data->password,
		store_data->password_len, store_data->db, store_data->db_len,
		store_data->port, store_data->socket, store_data->flags TSRMLS_CC) == NULL) {
#else
	store_data->conn = mysqlnd_init(MYSQLND_CLIENT_NO_FLAG, FALSE);
	if (mysqlnd_connect(store_data->conn, store_data->host, store_data->user, store_data->password,
		store_data->password_len, store_data->db, store_data->db_len,
		store_data->port, store_data->socket, store_data->flags,
		MYSQLND_CLIENT_NO_FLAG TSRMLS_CC) == NULL) {
#endif

		COPY_SQL_ERROR(store_data->conn, error_info);
		mysqlnd_close(store_data->conn, MYSQLND_CLOSE_DISCONNECTED);
		store_data->conn = NULL;
	} else {
		/*
		Functions that perform more than one query shall use BEGIN/COMMIT,
		everybody else may relay on autocommit. Functions shall not alter the
		autocommit setting */
		ret = mysqlnd_autocommit(store_data->conn, 1);
		if (PASS != ret) {
			mysqlnd_close(store_data->conn, MYSQLND_CLOSE_EXPLICIT);
			store_data->conn = NULL;
		}
	}

	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_xa_store_mysql_load_config */
static void
mysqlnd_ms_xa_store_mysql_load_config(struct st_mysqlnd_ms_config_json_entry * section,
						   void * data ,
						   MYSQLND_ERROR_INFO * error_info,
						   zend_bool persistent TSRMLS_DC)
{
	char * json_value = NULL;
	size_t json_value_len;
	int json_int = 0;
	zend_bool entry_exists;
	zend_bool entry_is_list;
	MYSQLND_MS_XA_STATE_STORE_MYSQL * store_data = (MYSQLND_MS_XA_STATE_STORE_MYSQL *)data;

	DBG_ENTER("mysqlnd_ms_xa_store_mysql_load_config");
	if (!store_data)
		DBG_VOID_RETURN;

	json_int = mysqlnd_ms_config_json_int_from_section(section, SECT_PORT_NAME, sizeof(SECT_PORT_NAME) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
	if (entry_exists) {
		if (entry_is_list) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a number", SECT_PORT_NAME, SECT_XA_NAME);
		} else {
			if (json_int < 0 || json_int > 65535) {
				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
								MYSQLND_MS_ERROR_PREFIX " Invalid value for '"SECT_PORT_NAME"' '%i' in section '%s'", json_int, SECT_XA_NAME);
			} else {
				store_data->port = (unsigned int)json_int;
			}
		}
	}

	json_value = mysqlnd_ms_config_json_string_from_section(section, SECT_SOCKET_NAME, sizeof(SECT_SOCKET_NAME) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
	if (entry_exists && json_value) {
		if (entry_is_list) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_SOCKET_NAME, SECT_XA_NAME);
		} else {
			store_data->socket = mnd_pestrndup(json_value, strlen(json_value), persistent);
		}
		mnd_efree(json_value);
	}

	json_value = mysqlnd_ms_config_json_string_from_section(section, SECT_HOST_NAME, sizeof(SECT_HOST_NAME) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
	if (entry_exists && json_value) {
		if (entry_is_list) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_HOST_NAME, SECT_XA_NAME);

			if (store_data->socket) {
				/* If we have an invalid host but a valid socket, mysqlnd would try a localhost unix socket connect.
				This is not what we want: if wrong host, then end. */
				mnd_pefree(store_data->socket, persistent);
				store_data->socket = NULL;
			}
		} else {
			store_data->host = mnd_pestrndup(json_value, strlen(json_value), persistent);
		}
		mnd_efree(json_value);
	}


	json_value = mysqlnd_ms_config_json_string_from_section(section, SECT_USER_NAME, sizeof(SECT_USER_NAME) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
	if (entry_exists && json_value) {
		if (entry_is_list) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_USER_NAME, SECT_XA_NAME);
		} else {
			store_data->user = mnd_pestrndup(json_value, strlen(json_value), persistent);
		}
		mnd_efree(json_value);
	}

	json_value = mysqlnd_ms_config_json_string_from_section(section, SECT_PASS_NAME, sizeof(SECT_PASS_NAME) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
	if (entry_exists && json_value) {
		if (entry_is_list) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_PASS_NAME, SECT_XA_NAME);
		} else {
			json_value_len = strlen(json_value);
			store_data->password = mnd_pestrndup(json_value, json_value_len, persistent);
			store_data->password_len = json_value_len;
		}
		mnd_efree(json_value);
	}

	json_value = mysqlnd_ms_config_json_string_from_section(section, SECT_DB_NAME, sizeof(SECT_DB_NAME) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
	if (entry_exists && json_value) {
		if (entry_is_list) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_DB_NAME, SECT_XA_NAME);
		} else {
			json_value_len = strlen(json_value);
			store_data->db = mnd_pestrndup(json_value, json_value_len, persistent);
			store_data->db_len = json_value_len;
		}
		mnd_efree(json_value);
	}

	json_int = mysqlnd_ms_config_json_int_from_section(section, SECT_CONNECT_FLAGS_NAME, sizeof(SECT_CONNECT_FLAGS_NAME) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
	if (entry_exists) {
		if (entry_is_list) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a number", SECT_CONNECT_FLAGS_NAME, SECT_XA_NAME);
		} else {
			if (json_int < 0) {
				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
								MYSQLND_MS_ERROR_PREFIX " Invalid value for '"SECT_CONNECT_FLAGS_NAME"' '%i' in section '%s'", json_int, SECT_XA_NAME);
			} else {
				store_data->flags = (unsigned int)json_int;
			}
		}
	}

	json_value = mysqlnd_ms_config_json_string_from_section(section, SECT_XA_STORE_GLOBAL_TRX_TABLE, sizeof(SECT_XA_STORE_GLOBAL_TRX_TABLE) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
	if (entry_exists && json_value) {
		if (entry_is_list) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string. Using default", SECT_XA_STORE_GLOBAL_TRX_TABLE, SECT_XA_NAME);
			/* Fallback to default to avoid further checks for empty string */
			store_data->global_table = mnd_pestrndup(MYSQL_STORE_DEFAULT_TRX_TABLE, strlen(MYSQL_STORE_DEFAULT_TRX_TABLE), persistent);
		} else {
			json_value_len = strlen(json_value);
			if (0 == json_value_len) {
				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " Empty string not allowed for '%s' from '%s'. Using default", SECT_XA_STORE_GLOBAL_TRX_TABLE, SECT_XA_NAME);
				/* Fallback to default to avoid further checks for empty string */
				store_data->global_table = mnd_pestrndup(MYSQL_STORE_DEFAULT_TRX_TABLE, strlen(MYSQL_STORE_DEFAULT_TRX_TABLE), persistent);
			} else {
				store_data->global_table = mnd_pestrndup(json_value, json_value_len, persistent);
			}
		}
		mnd_efree(json_value);
	} else {
		store_data->global_table = mnd_pestrndup(MYSQL_STORE_DEFAULT_TRX_TABLE, strlen(MYSQL_STORE_DEFAULT_TRX_TABLE), persistent);
	}

	json_value = mysqlnd_ms_config_json_string_from_section(section, SECT_XA_STORE_PARTICIPANT_TABLE, sizeof(SECT_XA_STORE_PARTICIPANT_TABLE) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
	if (entry_exists && json_value) {
		if (entry_is_list) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string. Using default", SECT_XA_STORE_PARTICIPANT_TABLE, SECT_XA_NAME);
			/* Fallback to default to avoid further checks for empty string */
			store_data->participant_table = mnd_pestrndup(MYSQL_STORE_DEFAULT_PARTICIPANTS_TABLE, strlen(MYSQL_STORE_DEFAULT_PARTICIPANTS_TABLE), persistent);
		} else {
			json_value_len = strlen(json_value);
			if (0 == json_value_len) {
				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " Empty string not allowed for '%s' from '%s'. Using default", SECT_XA_STORE_PARTICIPANT_TABLE, SECT_XA_NAME);
				/* Fallback to default to avoid further checks for empty string */
				store_data->participant_table = mnd_pestrndup(MYSQL_STORE_DEFAULT_PARTICIPANTS_TABLE, strlen(MYSQL_STORE_DEFAULT_PARTICIPANTS_TABLE), persistent);
			} else {
				store_data->participant_table = mnd_pestrndup(json_value, json_value_len, persistent);
			}
		}
		mnd_efree(json_value);
	} else {
		store_data->participant_table = mnd_pestrndup(MYSQL_STORE_DEFAULT_PARTICIPANTS_TABLE, strlen(MYSQL_STORE_DEFAULT_PARTICIPANTS_TABLE), persistent);
	}

	json_value = mysqlnd_ms_config_json_string_from_section(section, SECT_XA_STORE_GC_TABLE, sizeof(SECT_XA_STORE_GC_TABLE) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
	if (entry_exists && json_value) {
		if (entry_is_list) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string. Using default", SECT_XA_STORE_GC_TABLE, SECT_XA_NAME);
			/* Fallback to default to avoid further checks for empty string */
			store_data->gc_table = mnd_pestrndup(MYSQL_STORE_DEFAULT_GC_TABLE, strlen(MYSQL_STORE_DEFAULT_GC_TABLE), persistent);
		} else {
			json_value_len = strlen(json_value);
			if (0 == json_value_len) {
				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " Empty string not allowed for '%s' from '%s'. Using default", SECT_XA_STORE_GC_TABLE, SECT_XA_NAME);
				/* Fallback to default to avoid further checks for empty string */
				store_data->gc_table = mnd_pestrndup(MYSQL_STORE_DEFAULT_GC_TABLE, strlen(MYSQL_STORE_DEFAULT_GC_TABLE), persistent);
			} else {
				store_data->gc_table = mnd_pestrndup(json_value, json_value_len, persistent);
			}
		}
		mnd_efree(json_value);
	} else {
		store_data->gc_table = mnd_pestrndup(MYSQL_STORE_DEFAULT_GC_TABLE, strlen(MYSQL_STORE_DEFAULT_GC_TABLE), persistent);
	}

	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_xa_store_mysql_begin */
static enum_func_status
mysqlnd_ms_xa_store_mysql_begin(void * data, MYSQLND_ERROR_INFO * error_info,
								MYSQLND_MS_XA_ID * xa_id,
								unsigned int timeout TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	char * sql;
	int sql_len;

	MYSQLND_MS_XA_STATE_STORE_MYSQL * store_data = (MYSQLND_MS_XA_STATE_STORE_MYSQL *)data;

	DBG_ENTER("mysqlnd_ms_xa_store_mysql_begin");
	if (!store_data) {
		DBG_RETURN(ret);
	}

	/* lazy connection */
	if (!store_data->conn && (PASS != mysqlnd_ms_xa_store_mysql_connect(data, error_info TSRMLS_CC))) {
		DBG_INF("Connect failed");
		DBG_RETURN(ret);
	}

	/* plain autocommit is fine */
	if (timeout == 0) {
		/* timeout = 0 -> endless */
		sql_len = spprintf(&sql, 0,
					   "INSERT INTO %s(gtrid, format_id, state, intend, modified, started, timeout) "
					   "VALUES (%d, %d, 'XA_NON_EXISTING', 'XA_NON_EXISTING', NOW(), NOW(), NULL))",
						store_data->global_table,
						xa_id->gtrid,
						xa_id->format_id);
	} else {
		sql_len = spprintf(&sql, 0,
					   "INSERT INTO %s(gtrid, format_id, state, intend, modified, started, timeout) "
					   "VALUES (%d, %d, 'XA_NON_EXISTING', 'XA_NON_EXISTING', NOW(), NOW(), DATE_ADD(NOW(), INTERVAL %u SECOND))",
						store_data->global_table,
						xa_id->gtrid,
						xa_id->format_id,
						timeout);
	}
	ret = mysqlnd_query(store_data->conn, sql, sql_len);
	efree(sql);
	if (PASS != ret) {
		xa_id->store_id = NULL;
		COPY_SQL_ERROR(store_data->conn, error_info);
	} else {
		/* Don't set if the call fails! We won't get an opportunity to free later on */
		spprintf(&(xa_id->store_id), 0, MYSQLND_LLU_SPEC, mysqlnd_insert_id(store_data->conn));
		if (1 != mysqlnd_affected_rows(store_data->conn)) {
			SET_CLIENT_ERROR((_ms_p_ei error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, MYSQLND_MS_ERROR_PREFIX " Failed to add an entry to the state store transaction table, affected_rows != 1.");
			ret = FAIL;
		}
	}

	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_xa_store_mysql_monitor_change_state */
static enum_func_status
mysqlnd_ms_xa_store_mysql_monitor_change_state(void * data,
											 MYSQLND_ERROR_INFO * error_info,
											 MYSQLND_MS_XA_ID * xa_id,
											 enum mysqlnd_ms_xa_state to,
											 enum mysqlnd_ms_xa_state intend TSRMLS_DC)
{
	enum_func_status ret = PASS;
	char * sql;
	int sql_len;
	_ms_smart_type state_to = {0, 0, 0};
	_ms_smart_type state_intend = {0, 0, 0};
	MYSQLND_MS_XA_STATE_STORE_MYSQL * store_data = (MYSQLND_MS_XA_STATE_STORE_MYSQL *)data;

	DBG_ENTER("mysqlnd_ms_xa_store_mysql_monitor_change_state");
	if (!store_data) {
		DBG_RETURN(ret);
	}
	assert(store_data->conn);

	if (XA_NON_EXISTING == to) {
		/* Skip it - no participant, no trx record */
		DBG_RETURN(ret);
	}

	mysqlnd_ms_xa_state_to_string(to, &state_to);
	mysqlnd_ms_xa_state_to_string(intend, &state_intend);

	/* plain autocommit is fine */
	sql_len = spprintf(&sql, 0,
					   "UPDATE %s SET state = '%s', intend = '%s' WHERE "
							"store_trx_id = %s",
						store_data->global_table, state_to.c, state_intend.c,
						xa_id->store_id);
	ret = mysqlnd_query(store_data->conn, sql, sql_len);
	efree(sql);
	_ms_smart_method(free, &state_to);
	_ms_smart_method(free, &state_intend);

	if (PASS != ret) {
		COPY_SQL_ERROR(store_data->conn, error_info);
	} else if (1 != mysqlnd_affected_rows(store_data->conn)) {
		SET_CLIENT_ERROR((_ms_p_ei error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, MYSQLND_MS_ERROR_PREFIX " Failed to update the state store transaction table, affected_rows != 1. Did you delete any records?");
		ret = FAIL;
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_xa_store_mysql_add_participant */
static enum_func_status
mysqlnd_ms_xa_store_mysql_add_participant(void * data, MYSQLND_ERROR_INFO * error_info,
										  MYSQLND_MS_XA_ID * xa_id,
										  const MYSQLND_MS_XA_PARTICIPANT_LIST_DATA * const participant,
										  zend_bool record_cred, const char * localhost_ip TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	char * sql;
	int sql_len;
	char * host = NULL;
	_ms_smart_type state = {0, 0, 0};
	MYSQLND_MS_XA_STATE_STORE_MYSQL * store_data = (MYSQLND_MS_XA_STATE_STORE_MYSQL *)data;

	DBG_ENTER("mysqlnd_ms_xa_store_mysql_add_participant");
	if (!store_data) {
		DBG_RETURN(ret);
	}
	assert(store_data->conn);

	mysqlnd_ms_xa_state_to_string(participant->state, &state);

	if (MYSQLND_MS_CONN_STRING(MYSQLND_MS_CONN_HOST(participant->conn))) {
		host = MYSQLND_MS_CONN_STRING(MYSQLND_MS_CONN_HOST(participant->conn));
	} else {
		if (localhost_ip)
			host = (char *)localhost_ip;
	}

	if (record_cred) {
		/* If the user has configured user/pass on a per server basis, we need to remember.
		 Otherwise we can use the global mysqlnd ms user/pass when doing GC */
		sql_len = spprintf(&sql, 0,
						"INSERT INTO %s"
								"(fk_store_trx_id, "
								"bqual, "
								"server_uuid, scheme, host, port, socket, "
								"state, health, connection_id, "
								"user, password) "
							"VALUES "
								"(%s, '%d', 'TODO XA uuid', '%s', '%s', %d, '%s', '%s', 'OK',"MYSQLND_LLU_SPEC", "
								"'%s', '%s' )",
							store_data->participant_table,
							xa_id->store_id,
							participant->id,
							(MYSQLND_MS_CONN_STRING(participant->conn->scheme)) ? MYSQLND_MS_CONN_STRING(participant->conn->scheme) : "",
							(host) ? host : "",
							(participant->conn->port),
							(MYSQLND_MS_CONN_STRING(participant->conn->unix_socket)) ? MYSQLND_MS_CONN_STRING(participant->conn->unix_socket) : "",
							state.c,
							participant->conn->thread_id,
							(MYSQLND_MS_CONN_STRING(MYSQLND_MS_CONN_USER(participant->conn))) ? MYSQLND_MS_CONN_STRING(MYSQLND_MS_CONN_USER(participant->conn)) : "",
							(MYSQLND_MS_CONN_STRING(MYSQLND_MS_CONN_PASS(participant->conn))) ? MYSQLND_MS_CONN_STRING(MYSQLND_MS_CONN_PASS(participant->conn)) : "");
	} else {
		sql_len = spprintf(&sql, 0,
						"INSERT INTO %s"
								"(fk_store_trx_id, "
								"bqual, "
								"server_uuid, scheme, host, port, socket, "
								"state, health, connection_id) "
							"VALUES "
								"(%s, '%d','TODO XA uuid', '%s', '%s', %d, '%s', '%s', 'OK',"MYSQLND_LLU_SPEC")",
						    store_data->participant_table,
							xa_id->store_id,
							participant->id,
							(MYSQLND_MS_CONN_STRING(participant->conn->scheme)) ? MYSQLND_MS_CONN_STRING(participant->conn->scheme) : "",
							(host) ? host : "",
							(participant->conn->port),
							(MYSQLND_MS_CONN_STRING(participant->conn->unix_socket)) ? MYSQLND_MS_CONN_STRING(participant->conn->unix_socket) : "",
							state.c,
							participant->conn->thread_id);
	}
	/* autocommit is fine */
	ret = mysqlnd_query(store_data->conn, sql, sql_len);
	efree(sql);
	_ms_smart_method(free, &state);

	if (PASS != ret) {
		COPY_SQL_ERROR(store_data->conn, error_info);
	} else if (1 != mysqlnd_affected_rows(store_data->conn)) {
		SET_CLIENT_ERROR((_ms_p_ei error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, MYSQLND_MS_ERROR_PREFIX " Failed to create participant table entry, affected_rows != 1.");
		ret = FAIL;
	}

	DBG_RETURN(ret);
}
/* }}} */

//* {{{ mysqlnd_ms_xa_store_mysql_participant_change_state */
static enum_func_status
mysqlnd_ms_xa_store_mysql_participant_change_state(void * data, MYSQLND_ERROR_INFO * error_info,
												   MYSQLND_MS_XA_ID * xa_id,
												   const MYSQLND_MS_XA_PARTICIPANT_LIST_DATA * const participant,
												   enum mysqlnd_ms_xa_state from,
												   enum mysqlnd_ms_xa_state to TSRMLS_DC) {
	enum_func_status ret = FAIL;
	char * sql;
	int sql_len;
	_ms_smart_type state_from = {0, 0, 0}, state_to = {0, 0, 0};
	MYSQLND_MS_XA_STATE_STORE_MYSQL * store_data = (MYSQLND_MS_XA_STATE_STORE_MYSQL *)data;

	DBG_ENTER("mysqlnd_ms_xa_store_mysql_participant_change_state");
	if (!store_data) {
		DBG_RETURN(ret);
	}
	assert(store_data->conn);

	mysqlnd_ms_xa_state_to_string(from, &state_from);
	mysqlnd_ms_xa_state_to_string(to, &state_to);

	sql_len = spprintf(&sql, 0, "UPDATE %s SET state = '%s' WHERE "
								"fk_store_trx_id = %s "
								"AND bqual = '%d'",
								store_data->participant_table,
								state_to.c,
								xa_id->store_id,
								participant->id);
	/* autocommit */
	ret = mysqlnd_query(store_data->conn, sql, sql_len);
	efree(sql);
	_ms_smart_method(free, &state_from);
	_ms_smart_method(free, &state_to);

	if (PASS != ret) {
		COPY_SQL_ERROR(store_data->conn, error_info);
	} else if (1 != mysqlnd_affected_rows(store_data->conn)) {
		if (error_info)
			SET_CLIENT_ERROR((_ms_p_ei error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, MYSQLND_MS_ERROR_PREFIX " Failed to update the state store participant table. Did you delete records of the table");
		ret = FAIL;
	}
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_xa_store_mysql_participant_failure */
static enum_func_status
mysqlnd_ms_xa_store_mysql_participant_failure(void * data,  MYSQLND_ERROR_INFO *error_info,
											  MYSQLND_MS_XA_ID * xa_id,
											  const MYSQLND_MS_XA_PARTICIPANT_LIST_DATA * const participant,
											  const MYSQLND_ERROR_INFO * const participant_error_info TSRMLS_DC)
{
	char * sql;
	int sql_len;
	enum_func_status ret = FAIL;
	MYSQLND_MS_XA_STATE_STORE_MYSQL * store_data = (MYSQLND_MS_XA_STATE_STORE_MYSQL *)data;

	DBG_ENTER("mysqlnd_ms_xa_store_mysql_participant_failure");
	if (!store_data) {
		DBG_RETURN(ret);
	}
	assert(store_data->conn);

	sql_len = spprintf(&sql, 0, "UPDATE %s SET health = 'CLIENT ERROR', client_errno = %d, client_error = '%s' WHERE "
								"fk_store_trx_id = %s "
								"AND bqual = '%d'",
								store_data->participant_table,
								participant_error_info->error_no,
								participant_error_info->error,
								xa_id->store_id,
								participant->id);

	ret = mysqlnd_query(store_data->conn, sql, sql_len);
	efree(sql);
	if (PASS != ret) {
		COPY_SQL_ERROR(store_data->conn, error_info);
	} else if (1 != mysqlnd_affected_rows(store_data->conn)) {
		if (error_info)
			SET_CLIENT_ERROR((_ms_p_ei error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, MYSQLND_MS_ERROR_PREFIX " Failed to update the state store participant table. Did you delete records of the table");
		ret = FAIL;
	}
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_xa_store_mysql_monitor_failure */
static enum_func_status
mysqlnd_ms_xa_store_mysql_monitor_failure(void * data, MYSQLND_ERROR_INFO * error_info,
										  MYSQLND_MS_XA_ID * xa_id, enum mysqlnd_ms_xa_state intend TSRMLS_DC) {
	char * sql;
	int sql_len;
	enum_func_status ret = FAIL;
	_ms_smart_type state = {0, 0, 0};
	MYSQLND_MS_XA_STATE_STORE_MYSQL * store_data = (MYSQLND_MS_XA_STATE_STORE_MYSQL *)data;

	DBG_ENTER("mysqlnd_ms_xa_store_mysql_monitor_failure");
	if (!store_data) {
		DBG_RETURN(ret);
	}
	assert(store_data->conn);

	mysqlnd_ms_xa_state_to_string(intend, &state);

	sql_len = spprintf(&sql, 0, "UPDATE %s SET intend = '%s' WHERE "
								"store_trx_id = %s ",
								store_data->global_table,
								state.c,
								xa_id->store_id);
	/* autocommit */
	ret = mysqlnd_query(store_data->conn, sql, sql_len);
	efree(sql);
	_ms_smart_method(free, &state);

	if (PASS != ret) {
		COPY_SQL_ERROR(store_data->conn, error_info);
	} else if (1 != mysqlnd_affected_rows(store_data->conn)) {
		if (error_info)
			SET_CLIENT_ERROR((_ms_p_ei error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, MYSQLND_MS_ERROR_PREFIX " Failed to update the state store transaction table. Did you delete records of the table");
		ret = FAIL;
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_xa_store_mysql_monitor_finish */
static enum_func_status
mysqlnd_ms_xa_store_mysql_monitor_finish(void * data, MYSQLND_ERROR_INFO * error_info,
										  MYSQLND_MS_XA_ID * xa_id, zend_bool failure TSRMLS_DC) {
	char * sql;
	int sql_len;
	enum_func_status ret = FAIL;
	MYSQLND_MS_XA_STATE_STORE_MYSQL * store_data = (MYSQLND_MS_XA_STATE_STORE_MYSQL *)data;

	DBG_ENTER("mysqlnd_ms_xa_store_mysql_monitor_finish");
	if (!store_data) {
		DBG_RETURN(ret);
	}
	assert(store_data->conn);

	if (!xa_id->store_id) {
		/* no record in the DB, nothing to do */
		ret = PASS;
		DBG_RETURN(ret);
	}

	sql_len = spprintf(&sql, 0, "UPDATE %s SET finished = '%s' WHERE "
								"store_trx_id = %s",
								store_data->global_table,
								(failure) ? "FAILURE" : "SUCCESS",
								xa_id->store_id);
	/* autocommit */
	ret = mysqlnd_query(store_data->conn, sql, sql_len);
	efree(sql);
	efree(xa_id->store_id);
	if (PASS != ret) {
		COPY_SQL_ERROR(store_data->conn, error_info);
	} else if (1 != mysqlnd_affected_rows(store_data->conn)) {
		if (error_info)
			SET_CLIENT_ERROR((_ms_p_ei error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, MYSQLND_MS_ERROR_PREFIX " Failed to update the state store transaction table. Did you delete records of the table");
		ret = FAIL;
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_xa_store_mysql_gc_participants */

static enum_func_status
mysqlnd_ms_xa_store_gc_participants(MYSQLND_MS_XA_STATE_STORE_MYSQL * store_data,
									MYSQLND_ERROR_INFO *error_info,
									MYSQLND_MS_XA_ID * xa_id,
									zval _ms_p_zval * store_trx_id,
									zval * failed_participant_list,
									zend_bool must_commit TSRMLS_DC)
{
	enum_func_status ret = FAIL, ok;
	/* SELECT bqual, scheme, host, port, socket, user, password, state, health */
	zval _ms_p_zval *bqual = NULL, _ms_p_zval *scheme, _ms_p_zval *host, _ms_p_zval *port, _ms_p_zval *socket, _ms_p_zval *user, _ms_p_zval *password, _ms_p_zval *state, _ms_p_zval *health;
	zval _ms_p_zval xa_recover_list, _ms_p_zval *pprow;
	zval _ms_p_zval *format_id, _ms_p_zval *bqual_length, _ms_p_zval *gtrid_length, _ms_p_zval *data;
	char * sql, *tmp;
	int sql_len, tmp_len;
	uint64_t num_rows;
	MYSQLND_RES * res = NULL;

	DBG_ENTER("mysqlnd_ms_xa_store_gc_participants");

	for (zend_hash_internal_pointer_reset(Z_ARRVAL_P(failed_participant_list));
			zend_hash_has_more_elements(Z_ARRVAL_P(failed_participant_list)) == SUCCESS;
			zend_hash_move_forward(Z_ARRVAL_P(failed_participant_list))) {

		if ((_MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data, Z_ARRVAL_P(failed_participant_list), pprow) != SUCCESS) ||
			(Z_TYPE_P(_ms_p_zval pprow) != IS_ARRAY)) {
			continue;
		}
		zend_hash_internal_pointer_reset(Z_ARRVAL_P(_ms_p_zval pprow));
		GET_ZVAL_LONG_FROM_HASH(pprow, bqual);
		GET_ZVAL_STRING_FROM_HASH(pprow, scheme);
		GET_ZVAL_STRING_FROM_HASH(pprow, host);
		GET_ZVAL_LONG_FROM_HASH(pprow, port);
		GET_ZVAL_STRING_FROM_HASH(pprow, socket);
		GET_ZVAL_STRING_FROM_HASH(pprow, user);
		GET_ZVAL_STRING_FROM_HASH(pprow, password);
		GET_ZVAL_STRING_FROM_HASH(pprow, state);
		GET_ZVAL_STRING_FROM_HASH(pprow, health);
		DBG_INF_FMT("Checking store_trx_id=%d, bqual=%d, state=%s, health=%s", Z_LVAL_P(_ms_p_zval store_trx_id), Z_LVAL_P(_ms_p_zval bqual), Z_STRVAL_P(_ms_p_zval state), Z_STRVAL_P(_ms_p_zval health));

		if (!strncasecmp(Z_STRVAL_P(_ms_p_zval state), "XA_NON_EXISTING", Z_STRLEN_P(_ms_p_zval state))) {
		  DBG_INF("No action has been carried out on the participant");
		  continue;
		}

		if (strncasecmp(Z_STRVAL_P(_ms_p_zval health), "OK", Z_STRLEN_P(_ms_p_zval health)) &&
			strncasecmp(Z_STRVAL_P(_ms_p_zval state), "XA_PREPARED", Z_STRLEN_P(_ms_p_zval state))) {
			/*
			 * Failed client or server and a XA trx for which is either:
			 *
			 *   - state < XA_PREPARED (= has been forgotten by the server)
			 *   - state > XA_PREPARED (= has been comitted or been rolled back)
			 */
			DBG_INF("No GC required.");
			sql_len = spprintf(&sql, 0, "UPDATE %s SET health = 'GC_DONE' WHERE fk_store_trx_id = %d AND bqual = %d",
							store_data->participant_table,
							(int)Z_LVAL_P(_ms_p_zval store_trx_id),
							(int)Z_LVAL_P(_ms_p_zval bqual));
			ok = mysqlnd_query(store_data->conn, sql, sql_len);
			efree(sql);
			if (PASS != ok) {
				COPY_SQL_ERROR(store_data->conn, error_info);
				goto gc_participants_exit;
			}
			continue;
		}

		DBG_INF("GC possibly required.");
		/*
		 * Failed client that has left servers behind in XA_PREPARED state OR
		 * timed out client. In case of a timeout there are two cases to
		 * consider:
		 *
		 *   1) client has disconnected (= it failed, treat like a failed one)
		 *   2) client is still connected
		 *
		 * If the client is still connected, we cannot do anything but
		 * wait for it to die.
		 */

		if (((Z_STRLEN_P(_ms_p_zval scheme) > sizeof("tcp://")) && !memcmp(Z_STRVAL_P(_ms_p_zval scheme), "tcp://", sizeof("tcp://") - 1)) ||
			((Z_STRLEN_P(_ms_p_zval scheme) > sizeof("unix://")) && !memcmp(Z_STRVAL_P(_ms_p_zval scheme), "unix://", sizeof("unix://") - 1))) {

#if PHP_VERSION_ID < 50600
			MYSQLND * conn = mysqlnd_init(FALSE);
			if (mysqlnd_connect(conn,
							Z_STRVAL_P(_ms_p_zval host),
							Z_STRLEN_P(_ms_p_zval user) ? Z_STRVAL_P(_ms_p_zval user) : store_data->user,
							Z_STRLEN_P(_ms_p_zval password) ? Z_STRVAL_P(_ms_p_zval password) : store_data->password,
							Z_STRLEN_P(_ms_p_zval password) ? Z_STRLEN_P(_ms_p_zval password) : store_data->password_len,
							store_data->db, store_data->db_len,
							(unsigned int)Z_LVAL_P(_ms_p_zval port),
							NULL /* socket */, 0 /* flags */ TSRMLS_CC) == NULL) {
#else
			MYSQLND * conn = mysqlnd_init(MYSQLND_CLIENT_NO_FLAG, FALSE);
			if (mysqlnd_connect(conn,
							Z_STRVAL_P(_ms_p_zval host),
							Z_STRLEN_P(_ms_p_zval user) ? Z_STRVAL_P(_ms_p_zval user) : store_data->user,
							Z_STRLEN_P(_ms_p_zval password) ? Z_STRVAL_P(_ms_p_zval password) : store_data->password,
							Z_STRLEN_P(_ms_p_zval password) ? Z_STRLEN_P(_ms_p_zval password) : store_data->password_len,
							store_data->db, store_data->db_len,
							(unsigned int)Z_LVAL_P(_ms_p_zval port),
							NULL /* socket */, 0 /* flags */,
							MYSQLND_CLIENT_NO_FLAG TSRMLS_CC) == NULL) {
#endif
				COPY_SQL_ERROR(conn, error_info);
				mysqlnd_close(conn, MYSQLND_CLOSE_DISCONNECTED);
				goto gc_participants_exit;
			} else {
				ok = mysqlnd_autocommit(conn, 1);
				if (PASS != ok) {
					COPY_SQL_ERROR(conn, error_info);
					mysqlnd_close(conn, MYSQLND_CLOSE_EXPLICIT);
					goto gc_participants_exit;
				}
			}

			/*
			 Check the list of open xa trx. Most of the time we should not find
			 our ID. If the client has disconnected (in PREPARE state)
			 before finishing the XA trx, the server will rollback and forget.
			 Should the server crash in PREPARE state, it may recover the
			 trx but it SHOULD not be comitted to avoid problems with the
			 binary log. Then, we SHOULD roll it back. However, if there is a single
			 participant in XA_COMMIT state already, we MUST commit.

			 The worst case scenario is that we have one fresh, active
			 XA trx while the GC is run and tries to wrap up a failed
			 XA trx of the same ID (gtrid + bqual). The GC may then interrupt
			 the ongoing (faultfree) XA trx, or the ongoing XA trx may interrupt
			 the GC and the GC considers itself failed.
			*/
			sql_len= spprintf(&sql, 0, "XA RECOVER");
			ok = mysqlnd_query(conn, sql, sql_len);
			efree(sql);
			if (PASS != ok) {
				COPY_SQL_ERROR(conn, error_info);
				mysqlnd_close(conn, MYSQLND_CLOSE_DISCONNECTED);
				goto gc_participants_exit;
			}
			if (!(res = mysqlnd_store_result(conn))) {
				COPY_SQL_ERROR(store_data->conn, error_info);
				mysqlnd_close(conn, MYSQLND_CLOSE_DISCONNECTED);
				goto gc_participants_exit;
			}
			num_rows = mysqlnd_num_rows(res);

			if (0 == num_rows) {
				mysqlnd_free_result(res, TRUE);
				/*
				 * Nothing to do...? This may be a timed out XA trx that is still ongoing with
				 * some clients still connected to the participants and state < XA_PREPARED.
				 * We should be able to find out running XA START.
				 */
				ok = PASS;
				if (!strncasecmp(Z_STRVAL_P(_ms_p_zval health), "OK", Z_STRLEN_P(_ms_p_zval health))) {
					sql_len = spprintf(&sql, 0, "XA START '%d'", xa_id->gtrid);
					ok = mysqlnd_query(conn, sql, sql_len);
					efree(sql);
					if (PASS != ok) {
						if (1440 == mysqlnd_errno(conn)) {
							/* ERROR 1440 (XAE08): XAER_DUPID: The XID already exists */
							DBG_INF("This timed out XA trx is still active with state < XA_PREPARED.");
						} else {
							COPY_SQL_ERROR(conn, error_info);
							mysqlnd_close(conn, MYSQLND_CLOSE_DISCONNECTED);
							goto gc_participants_exit;
						}
					}
					/* As we will disconnect immediately, a possibly started XA trx can be ignored. */
				}

				if (ok == PASS) {
					DBG_INF("XA trx should not be active any more.");
					sql_len = spprintf(&sql, 0, "UPDATE %s SET health = 'GC_DONE' WHERE fk_store_trx_id = %d AND bqual = %d",
									store_data->participant_table,
									(int)Z_LVAL_P(_ms_p_zval store_trx_id),
									(int)Z_LVAL_P(_ms_p_zval bqual));
					ok = mysqlnd_query(store_data->conn, sql, sql_len);
					efree(sql);
					if (PASS != ok) {
						COPY_SQL_ERROR(store_data->conn, error_info);
						mysqlnd_close(conn, MYSQLND_CLOSE_DISCONNECTED);
						goto gc_participants_exit;
					}
				}
			} else {

				MAKE_STD_ZVAL(xa_recover_list);
				mysqlnd_fetch_all(res, MYSQLND_FETCH_NUM, _ms_a_zval xa_recover_list);
				if (Z_TYPE(_ms_p_zval xa_recover_list) != IS_ARRAY) {
					mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
									  E_WARNING TSRMLS_CC,
									  MYSQLND_MS_ERROR_PREFIX " MySQL XA state store error: Garbage collection failed. "
									  "Failed to fetch results for XA RECOVER");
					_ms_zval_dtor(xa_recover_list);
					mysqlnd_close(conn, MYSQLND_CLOSE_DISCONNECTED);
					goto gc_participants_exit;
				}

				/* The initial version should hardly ever get here.
			Server must have been shut down between prepare and commit during mysqlnd_ms_xa_commit(). */
				for (zend_hash_internal_pointer_reset(Z_ARRVAL(_ms_p_zval xa_recover_list));
					zend_hash_has_more_elements(Z_ARRVAL(_ms_p_zval xa_recover_list)) == SUCCESS;
					zend_hash_move_forward(Z_ARRVAL(_ms_p_zval xa_recover_list))) {

					if ((_MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data, Z_ARRVAL(_ms_p_zval xa_recover_list), pprow) != SUCCESS) ||
						(Z_TYPE_P(_ms_p_zval pprow) != IS_ARRAY)) {
					continue;
				}
					zend_hash_internal_pointer_reset(Z_ARRVAL_P(_ms_p_zval pprow));

					/*
				mysql> XA RECOVER;
				+----------+--------------+--------------+------+
				| formatID | gtrid_length | bqual_length | data |
				+----------+--------------+--------------+------+
				|        1 |            1 |            0 | 1    |
				+----------+--------------+--------------+------+
				*/
					/* we can ignore this - its always 0 with the initial version */
					GET_ZVAL_STRING_FROM_HASH(pprow, format_id);
					/* we can ignore details - data is always gtrid only with the initial version */
					GET_ZVAL_STRING_FROM_HASH(pprow, gtrid_length);
					GET_ZVAL_STRING_FROM_HASH(pprow, bqual_length);
					GET_ZVAL_STRING_FROM_HASH(pprow, data);

					/* compare data = gtrid with unsigned int xa_id.gtrid */
					if ((tmp_len = spprintf(&tmp, 0, "%d", xa_id->gtrid)) != 0) {
						if (!strncasecmp(Z_STRVAL_P(_ms_p_zval data), tmp, tmp_len - 1)) {
						efree(tmp);
						/* this is ours... */
						if (must_commit) {
							DBG_INF("There is at least one participant in XA_COMMIT state, we MUST commit");
							sql_len = spprintf(&sql, 0, "XA COMMIT '%d'", xa_id->gtrid);
						} else {
							DBG_INF("No participant reached XA_COMMIT before, we can and SHOULD rollback");
							sql_len = spprintf(&sql, 0, "XA ROLLBACK '%d'", xa_id->gtrid);
						}
						ok = mysqlnd_query(store_data->conn, sql, sql_len);
						efree(sql);
						if (PASS != ok) {
							if ((!strncasecmp(Z_STRVAL_P(_ms_p_zval health), "OK", Z_STRLEN_P(_ms_p_zval health))) &&
								(1399 == mysqlnd_errno(conn))) {
								/* Possibly a timed out client which is connected to the server still
								ERROR 1399 (XAE07): XAER_RMFAIL: The command cannot be executed when global transaction is in the  NON-EXISTING state */
								DBG_INF("This timed out XA trx is still active with state > XA_PREPARED.");
							} else {
								COPY_SQL_ERROR(store_data->conn, error_info);
								_ms_zval_dtor(xa_recover_list);
								mysqlnd_close(conn, MYSQLND_CLOSE_DISCONNECTED);
								goto gc_participants_exit;
							}
						} else {
							/* GC done... */
							sql_len = spprintf(&sql, 0, "UPDATE %s SET health = 'GC_DONE' WHERE fk_store_trx_id = %d AND bqual = %d",
								store_data->participant_table,
								(int)Z_LVAL_P(_ms_p_zval store_trx_id),
								(int)Z_LVAL_P(_ms_p_zval bqual));
							ok = mysqlnd_query(store_data->conn, sql, sql_len);
							efree(sql);
							if (PASS != ok) {
								COPY_SQL_ERROR(store_data->conn, error_info);
								_ms_zval_dtor(xa_recover_list);
								mysqlnd_close(conn, MYSQLND_CLOSE_DISCONNECTED);
								goto gc_participants_exit;
							}
						}
					} else {
							efree(tmp);
						}
					}
				}
				_ms_zval_dtor(xa_recover_list);
			}
			mysqlnd_close(conn, MYSQLND_CLOSE_DISCONNECTED);

		} else if (Z_STRLEN_P(_ms_p_zval scheme) > sizeof("pipe://") && !memcmp(Z_STRVAL_P(_ms_p_zval scheme), "pipe://", sizeof("pipe://") - 1)) {
			/* XA TODO */
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
									  E_WARNING TSRMLS_CC,
									  MYSQLND_MS_ERROR_PREFIX " MySQL XA state store error: Garbage collection failed. "
									  "Windows pipe scheme '%s' is not supported yet", Z_STRVAL_P(_ms_p_zval scheme));
			goto gc_participants_exit;
		} else {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
									  E_WARNING TSRMLS_CC,
									  MYSQLND_MS_ERROR_PREFIX " MySQL XA state store error: Garbage collection failed. "
									  "Unknown scheme '%s'", Z_STRVAL_P(_ms_p_zval scheme));
			goto gc_participants_exit;
		}
	}

	if (bqual) {
		/* All participants happy? */
		sql_len = spprintf(&sql, 0, "SELECT bqual FROM %s WHERE health != 'GC_DONE' AND fk_store_trx_id = %d AND bqual = %d LIMIT 1",
						store_data->participant_table,
						(int)Z_LVAL_P(_ms_p_zval store_trx_id),
						(int)Z_LVAL_P(_ms_p_zval bqual));
		ret = mysqlnd_query(store_data->conn, sql, sql_len);
		efree(sql);
		if (PASS != ret) {
			COPY_SQL_ERROR(store_data->conn, error_info);
			goto gc_participants_exit;
		}

		if (!(res = mysqlnd_store_result(store_data->conn))) {
			COPY_SQL_ERROR(store_data->conn, error_info);
			goto gc_participants_exit;
		}
		num_rows = mysqlnd_num_rows(res);
		mysqlnd_free_result(res, TRUE);
		if (0 != num_rows) {
			ret = FAIL;
		}
	}

gc_participants_exit:
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_xa_store_mysql_do_gc_one */
static enum_func_status
mysqlnd_ms_xa_store_mysql_do_gc_one(void * data,
								MYSQLND_ERROR_INFO *error_info,
								MYSQLND_MS_XA_ID * xa_id,
								unsigned int gc_max_retries TSRMLS_DC)
{
	enum_func_status ret = FAIL, ok;
	char * sql;
	int sql_len, gc_id = 0, attempts = 0;
	uint64_t num_rows;
	MYSQLND_RES * res = NULL;
	zval _ms_p_zval row, _ms_p_zval *entry, _ms_p_zval store_trx_id_list, _ms_p_zval *pprow;
	MYSQLND_MS_XA_STATE_STORE_MYSQL * store_data = (MYSQLND_MS_XA_STATE_STORE_MYSQL *)data;

	DBG_ENTER("mysqlnd_ms_xa_store_mysql_do_gc_one");
	if (!store_data) {
		DBG_RETURN(ret);
	}

	if (!store_data->conn && (PASS != mysqlnd_ms_xa_store_mysql_connect(data, error_info TSRMLS_CC))) {
		DBG_INF("Connect failed");
		DBG_RETURN(ret);
	}

	if (PASS != mysqlnd_begin_transaction(store_data->conn, TRANS_START_NO_OPT, NULL)) {
		COPY_SQL_ERROR(store_data->conn, error_info);
		DBG_RETURN(ret);
	}

	/* There shall be only one GC run  at a time, lock the table */
	/* SELECT * FROM store_data->gc_table WHERE fk_gtrid = ... FOR UPDATE */
	if (xa_id->store_id) {
		/* We know exectly which store_id to rollback */
		sql_len = spprintf(&sql, 0, "SELECT gc_id, attempts FROM %s WHERE "
							"gtrid = '%d' AND format_id = '%d' AND fk_store_trx_id = %s FOR UPDATE",
							store_data->gc_table,
							xa_id->gtrid,
							xa_id->format_id,
							xa_id->store_id
  					);
	} else {
		/* there may be more than one record in the global trx records table with the gtrid we are given */
		sql_len = spprintf(&sql, 0, "SELECT gc_id, attempts FROM %s WHERE "
							"gtrid = '%d' AND format_id = '%d' FOR UPDATE",
							store_data->gc_table,
							xa_id->gtrid,
							xa_id->format_id
  					);
	}

	ok = mysqlnd_query(store_data->conn, sql, sql_len);
	efree(sql);

	if (PASS != ok) {
		COPY_SQL_ERROR(store_data->conn, error_info);
		goto gc_one_exit;
	}

	if (!(res = mysqlnd_store_result(store_data->conn))) {
		COPY_SQL_ERROR(store_data->conn, error_info);
		goto gc_one_exit;
	}

	MAKE_STD_ZVAL(row);
#ifdef MYSQLND_STORE_NO_COPY
	mysqlnd_fetch_into(res, MYSQLND_FETCH_NUM, _ms_a_zval row, MYSQLND_MYSQLI);
#else
	mysqlnd_fetch_into(res, MYSQLND_FETCH_NUM, _ms_a_zval row);
#endif
	if (Z_TYPE(_ms_p_zval row) == IS_ARRAY) {

		zend_hash_internal_pointer_reset(Z_ARRVAL(_ms_p_zval row));

		if (zend_hash_has_more_elements(Z_ARRVAL(_ms_p_zval row))) {
			_MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data, Z_ARRVAL(_ms_p_zval row), entry);
			convert_to_long_ex(entry);
			gc_id = Z_LVAL_P(_ms_p_zval entry);
			DBG_INF_FMT("gc_id=%d", attempts);
			zend_hash_move_forward(Z_ARRVAL(_ms_p_zval (row)));
		}
		if (zend_hash_has_more_elements(Z_ARRVAL(_ms_p_zval row))) {
			_MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data, Z_ARRVAL(_ms_p_zval row), entry);
			convert_to_long_ex(entry);
			attempts = Z_LVAL_P(_ms_p_zval entry);
			DBG_INF_FMT("attempts=%d", attempts);
		}
	}
	_ms_zval_dtor(row);

	num_rows = mysqlnd_num_rows(res);
	mysqlnd_free_result(res, TRUE);

	if (!num_rows) {
		sql_len = spprintf(&sql, 0, "INSERT INTO %s(gtrid, format_id, fk_store_trx_id, attempts) "
							"VALUES (%d, '%d', %s, 1)",
							store_data->gc_table,
							xa_id->gtrid,
							xa_id->format_id,
							(xa_id->store_id) ? xa_id->store_id : "NULL"
						  );
		ok = mysqlnd_query(store_data->conn, sql, sql_len);
		efree(sql);
		if (PASS != ok) {
			COPY_SQL_ERROR(store_data->conn, error_info);
			goto gc_one_exit;
		}
		gc_id = (int)mysqlnd_insert_id(store_data->conn);

	} else {
		attempts++;
		if (((0 != gc_max_retries) && (attempts > gc_max_retries)) || (attempts >= 65535)) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
									  E_WARNING TSRMLS_CC,
									  MYSQLND_MS_ERROR_PREFIX " MySQL XA state store error: Garbage collection failed. "
									  "Maximum number of GC attempts reached: %d retries done, maximum number of %d retries allowed. "
									  "Please, handle manually", attempts, gc_max_retries);
			goto gc_one_exit;
		}
		sql_len = spprintf(&sql, 0, "UPDATE %s SET attempts = %d WHERE "
									"gtrid = %d AND format_id = '%d' AND fk_store_trx_id = %s",
							store_data->gc_table,
							(int)attempts,
							xa_id->gtrid,
							xa_id->format_id,
							(xa_id->store_id) ? xa_id->store_id : "NULL"
						  );
		ok = mysqlnd_query(store_data->conn, sql, sql_len);
		efree(sql);
		if (PASS != ok) {
			COPY_SQL_ERROR(store_data->conn, error_info);
			goto gc_one_exit;
		}
	}
	/* fetch store trx ids */
	if (xa_id->store_id) {
		sql_len = spprintf(&sql, 0, "SELECT store_trx_id, state, intend, finished, started, timeout FROM %s WHERE store_trx_id = %s AND "
							"((finished != 'NO') OR ((timeout IS NOT NULL) AND (timeout < NOW())))",
							store_data->global_table,
							(xa_id->store_id) ? xa_id->store_id : "NULL");
	} else {
		/* OR ((timeout IS NOT NULL) AND (timeout < NOW())) */
		sql_len = spprintf(&sql, 0, "SELECT store_trx_id, state, intend, finished, started, timeout FROM %s WHERE "
							"gtrid = '%d' AND format_id = '%d' AND "
							"((finished != 'NO') OR ((timeout IS NOT NULL) AND (timeout < NOW())))",
							store_data->global_table,
							xa_id->gtrid,
							xa_id->format_id);
	}
	ok = mysqlnd_query(store_data->conn, sql, sql_len);
	efree(sql);
	if (PASS != ok) {
		COPY_SQL_ERROR(store_data->conn, error_info);
		goto gc_one_exit;
	}
	if (!(res = mysqlnd_store_result(store_data->conn))) {
		COPY_SQL_ERROR(store_data->conn, error_info);
		goto gc_one_exit;
	}
	MAKE_STD_ZVAL(store_trx_id_list);
	mysqlnd_fetch_all(res, MYSQLND_FETCH_NUM, _ms_a_zval store_trx_id_list);
	if (Z_TYPE(_ms_p_zval store_trx_id_list) != IS_ARRAY) {
		mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
							  E_WARNING TSRMLS_CC,
							  MYSQLND_MS_ERROR_PREFIX " MySQL XA state store error: Garbage collection failed. "
							  "Failed to fetch list of store transaction ids.");
		goto gc_one_exit;
	}
	mysqlnd_free_result(res, TRUE);

	for (zend_hash_internal_pointer_reset(Z_ARRVAL(_ms_p_zval store_trx_id_list));
			zend_hash_has_more_elements(Z_ARRVAL(_ms_p_zval store_trx_id_list)) == SUCCESS;
			zend_hash_move_forward(Z_ARRVAL(_ms_p_zval store_trx_id_list))) {
		zval _ms_p_zval *store_trx_id, _ms_p_zval *state, _ms_p_zval *intend, _ms_p_zval *finished, _ms_p_zval *started, _ms_p_zval *timeout;

		if ((_MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data, Z_ARRVAL(_ms_p_zval store_trx_id_list), pprow) != SUCCESS) ||
			(Z_TYPE_P(_ms_p_zval pprow) != IS_ARRAY)) {
			continue;
		}
		zend_hash_internal_pointer_reset(Z_ARRVAL_P(_ms_p_zval pprow));

		if ((zend_hash_has_more_elements(Z_ARRVAL_P(_ms_p_zval pprow)) != SUCCESS) ||
			(_MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data, Z_ARRVAL_P(_ms_p_zval pprow), store_trx_id) != SUCCESS)) {
			continue;
		}
		/* NOTE: the hash gets modified */
		convert_to_long_ex(store_trx_id);
		zend_hash_move_forward(Z_ARRVAL_P(_ms_p_zval pprow));
		GET_ZVAL_STRING_FROM_HASH(pprow, state);
		GET_ZVAL_STRING_FROM_HASH(pprow, intend);
		GET_ZVAL_STRING_FROM_HASH(pprow, finished);
		GET_ZVAL_STRING_FROM_HASH(pprow, started);
		GET_ZVAL_STRING_FROM_HASH(pprow, timeout);
		DBG_INF_FMT("store_trx_id=%d, finished=%s", Z_LVAL_P(_ms_p_zval store_trx_id), Z_STRVAL_P(_ms_p_zval finished));

		if (!strncasecmp(Z_STRVAL_P(_ms_p_zval finished), "SUCCESS", Z_STRLEN_P(_ms_p_zval finished))) {
			/* nothing to do: erase */

			sql_len = spprintf(&sql, 0, "DELETE FROM %s WHERE store_trx_id = %d",
							store_data->global_table,
							(int)Z_LVAL_P(_ms_p_zval store_trx_id));
			ok = mysqlnd_query(store_data->conn, sql, sql_len);
			efree(sql);
			if (PASS != ok) {
				COPY_SQL_ERROR(store_data->conn, error_info);
				goto gc_one_exit;
			}
			continue;
		} else if (!strncasecmp(Z_STRVAL_P(_ms_p_zval finished), "FAILURE", Z_STRLEN_P(_ms_p_zval finished)) ||
			!strncasecmp(Z_STRVAL_P(_ms_p_zval finished), "NO", Z_STRLEN_P(_ms_p_zval finished))) {
		  	/* Note: transaction could be still ongoing but we have reached the timeout */
			zval _ms_p_zval failed_participant_list;
			sql_len = spprintf(&sql, 0, "SELECT bqual, scheme, host, port, socket, user, password, state, health "
										"FROM %s WHERE fk_store_trx_id = %d AND health != 'GC_DONE'",
								store_data->participant_table,
								(int)Z_LVAL_P(_ms_p_zval store_trx_id));
			ok = mysqlnd_query(store_data->conn, sql, sql_len);
			efree(sql);
			if (PASS != ok) {
				COPY_SQL_ERROR(store_data->conn, error_info);
				goto gc_one_exit;
			}

			if (!(res = mysqlnd_store_result(store_data->conn))) {
				COPY_SQL_ERROR(store_data->conn, error_info);
				goto gc_one_exit;
			}
			num_rows = mysqlnd_num_rows(res);
			if (0 == num_rows) {
				ok = PASS;
				mysqlnd_free_result(res, TRUE);
				/* TODO - no failed participant - weird? */
				DBG_INF_FMT("%s shows a failure for trx %d but lists no failed participants.",
							store_data->participant_table,
							Z_LVAL_P(_ms_p_zval store_trx_id));
			} else {

				MAKE_STD_ZVAL(failed_participant_list);
				mysqlnd_fetch_all(res, MYSQLND_FETCH_NUM, _ms_a_zval failed_participant_list);
				if (Z_TYPE(_ms_p_zval failed_participant_list) != IS_ARRAY) {
					_ms_zval_dtor(failed_participant_list);
					mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
									E_WARNING TSRMLS_CC,
									MYSQLND_MS_ERROR_PREFIX " MySQL XA state store error: Garbage collection failed. "
									"Failed to fetch list of failed participants");
					goto gc_one_exit;
				}
				mysqlnd_free_result(res, TRUE);

				/* Is there any participant in COMMIT state? If so, we must try to commit the others! */
				sql_len = spprintf(&sql, 0, "SELECT state "
									"FROM %s WHERE fk_store_trx_id = %d AND health != 'GC_DONE' AND state = 'XA_COMMIT' ",
							store_data->participant_table,
							(int)Z_LVAL_P(_ms_p_zval store_trx_id));
				ok = mysqlnd_query(store_data->conn, sql, sql_len);
				efree(sql);
				if (PASS != ok) {
					COPY_SQL_ERROR(store_data->conn, error_info);
					goto gc_one_exit;
				}

				if (!(res = mysqlnd_store_result(store_data->conn))) {
					COPY_SQL_ERROR(store_data->conn, error_info);
					goto gc_one_exit;
				}
				num_rows = mysqlnd_num_rows(res);
				mysqlnd_free_result(res, TRUE);

				ok = mysqlnd_ms_xa_store_gc_participants(store_data, error_info, xa_id, store_trx_id, _ms_a_zval failed_participant_list, (num_rows > 0) ? TRUE : FALSE TSRMLS_CC);
				zval_ptr_dtor(&failed_participant_list);
			}
			if (ok == PASS) {
				sql_len = spprintf(&sql, 0, "DELETE FROM %s WHERE store_trx_id = %d",
								store_data->global_table,
								(int)Z_LVAL_P(_ms_p_zval store_trx_id));
				ok = mysqlnd_query(store_data->conn, sql, sql_len);
				efree(sql);
				if (PASS != ok) {
					COPY_SQL_ERROR(store_data->conn, error_info);
					goto gc_one_exit;
				}
			}
		} else {
			/* bark: unknown finished state */
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
								E_WARNING TSRMLS_CC,
								MYSQLND_MS_ERROR_PREFIX " MySQL XA state store error: Garbage collection failed. "
								"Unknown finished state '%s' found for store transaction id %d",
								Z_STRVAL_P(_ms_p_zval finished),
								Z_LVAL_P(_ms_p_zval store_trx_id)
 							);
			goto gc_one_exit;
		}

	}

	/* GC success, remove GC record */
	sql_len = spprintf(&sql, 0, "DELETE FROM %s WHERE gc_id = %d",
						store_data->gc_table,
						gc_id);
	ret = mysqlnd_query(store_data->conn, sql, sql_len);
	efree(sql);
	if (PASS != ret) {
		COPY_SQL_ERROR(store_data->conn, error_info);
	}

gc_one_exit:
	_ms_zval_dtor(store_trx_id_list);
	if (PASS != (ok = mysqlnd_commit(store_data->conn, TRANS_COR_NO_OPT, NULL))) {
		COPY_SQL_ERROR(store_data->conn, error_info);
		ret = FAIL;
	}

	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_xa_store_mysql_gc_one */
static enum_func_status
mysqlnd_ms_xa_store_mysql_gc_one(void * data,
								MYSQLND_ERROR_INFO *error_info,
								MYSQLND_MS_XA_ID * xa_id,
								unsigned int gc_max_retries TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	DBG_ENTER("mysqlnd_ms_xa_store_mysql_gc_one");
	DBG_INF_FMT("gtrid %d", xa_id->gtrid);
	ret = mysqlnd_ms_xa_store_mysql_do_gc_one(data, error_info, xa_id, gc_max_retries TSRMLS_CC);
	DBG_RETURN(ret);
}

/* {{{ mysqlnd_ms_xa_store_mysql_gc_all */
static enum_func_status
mysqlnd_ms_xa_store_mysql_gc_all(void * data,
								MYSQLND_ERROR_INFO *error_info,
								unsigned int gc_max_retries,
								unsigned int gc_max_trx_per_run TSRMLS_DC)
{
	enum_func_status ret = FAIL, ok;
	char * sql;
	int sql_len;
	MYSQLND_RES * res = NULL;
	zval _ms_p_zval store_trx_id_list, _ms_p_zval *pprow;
	MYSQLND_MS_XA_ID id;

	MYSQLND_MS_XA_STATE_STORE_MYSQL * store_data = (MYSQLND_MS_XA_STATE_STORE_MYSQL *)data;
	DBG_ENTER("mysqlnd_ms_xa_store_mysql_gc_all");
	if (!store_data) {
		DBG_RETURN(ret);
	}

	if (!store_data->conn && (PASS != mysqlnd_ms_xa_store_mysql_connect(data, error_info TSRMLS_CC))) {
		DBG_INF("Connect failed");
		DBG_RETURN(ret);
	}

	sql_len = spprintf(&sql, 0, "SELECT store_trx_id, gtrid, format_id FROM %s WHERE "
		"((finished = 'SUCCESS') OR (finished = 'FAILURE') OR ((timeout IS NOT NULL) AND (timeout < NOW()))) LIMIT %d",	store_data->global_table, gc_max_trx_per_run);
	ok = mysqlnd_query(store_data->conn, sql, sql_len);
	efree(sql);

	if ((PASS != ok) || (!(res = mysqlnd_store_result(store_data->conn)))) {
		COPY_SQL_ERROR(store_data->conn, error_info);
		DBG_RETURN(ret);
	}
	MAKE_STD_ZVAL(store_trx_id_list);
	ZVAL_NULL(_ms_a_zval store_trx_id_list);
	mysqlnd_fetch_all(res, MYSQLND_FETCH_NUM, _ms_a_zval store_trx_id_list);
	if (Z_TYPE(_ms_p_zval store_trx_id_list) != IS_ARRAY) {
		mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
							  E_WARNING TSRMLS_CC,
							  MYSQLND_MS_ERROR_PREFIX " MySQL XA state store error: Garbage collection failed. "
							  "Failed to fetch list of store transaction ids.");
		mysqlnd_free_result(res, TRUE);
		_ms_zval_dtor(store_trx_id_list);
		DBG_RETURN(ret);
	}
	mysqlnd_free_result(res, TRUE);

	for (zend_hash_internal_pointer_reset(Z_ARRVAL(_ms_p_zval store_trx_id_list));
			zend_hash_has_more_elements(Z_ARRVAL(_ms_p_zval store_trx_id_list)) == SUCCESS;
			zend_hash_move_forward(Z_ARRVAL(_ms_p_zval store_trx_id_list))) {
		zval _ms_p_zval *store_trx_id, _ms_p_zval *gtrid, _ms_p_zval *format_id;

		if ((_MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data, Z_ARRVAL(_ms_p_zval store_trx_id_list), pprow) != SUCCESS) ||
			(Z_TYPE_P(_ms_p_zval pprow) != IS_ARRAY)) {
			continue;
		}
		zend_hash_internal_pointer_reset(Z_ARRVAL_P(_ms_p_zval pprow));
		if ((zend_hash_has_more_elements(Z_ARRVAL_P(_ms_p_zval pprow)) != SUCCESS) ||
			(_MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data, Z_ARRVAL_P(_ms_p_zval pprow), store_trx_id) != SUCCESS)) {
			continue;
		}
		/* NOTE: the hash gets modified */
		GET_ZVAL_STRING_FROM_HASH(pprow, store_trx_id);
		GET_ZVAL_LONG_FROM_HASH(pprow, gtrid);
		GET_ZVAL_LONG_FROM_HASH(pprow, format_id);
		DBG_INF_FMT("store_trx_id=%s, gtrid=%d, format_id=%d", Z_STRVAL_P(_ms_p_zval store_trx_id), Z_LVAL_P(_ms_p_zval gtrid), Z_LVAL_P(_ms_p_zval format_id));
		MYSQLND_MS_XA_ID_RESET(id);
	//	id.store_id = Z_STRVAL_P(_ms_p_zval store_trx_id);
		id.gtrid = Z_LVAL_P(_ms_p_zval gtrid);
		id.format_id = Z_LVAL_P(_ms_p_zval format_id);

		ok = mysqlnd_ms_xa_store_mysql_do_gc_one(data, error_info, &id, gc_max_retries TSRMLS_CC);
	}
	_ms_zval_dtor(store_trx_id_list);

	ret = PASS;
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_xa_store_mysql_dtor */
static void
mysqlnd_ms_xa_store_mysql_dtor(void ** data, zend_bool persistent TSRMLS_DC)
{
	MYSQLND_MS_XA_STATE_STORE_MYSQL * store_data;
	DBG_ENTER("mysqlnd_ms_xa_store_mysql_dtor");

	if (data && *data) {
		store_data = (MYSQLND_MS_XA_STATE_STORE_MYSQL *)*data;
	}
	if (store_data) {
		if (store_data->host) {
			mnd_pefree(store_data->host, persistent);
		}
		if (store_data->socket) {
			mnd_pefree(store_data->socket, persistent);
		}
		if (store_data->user) {
			mnd_pefree(store_data->user, persistent);
		}
		if (store_data->password) {
			mnd_pefree(store_data->password, persistent);
			store_data->password_len = 0;
		}
		if (store_data->db) {
			mnd_pefree(store_data->db, persistent);
			store_data->db_len = 0;
		}
		if (store_data->conn) {
			mysqlnd_close(store_data->conn, MYSQLND_CLOSE_DISCONNECTED);
			store_data->conn = NULL;
		}

		/* those table names should be set always - anyway... */
		if (store_data->global_table) {
			mnd_pefree(store_data->global_table, persistent);
		}
		if (store_data->participant_table) {
			mnd_pefree(store_data->participant_table, persistent);
		}
		if (store_data->gc_table) {
			mnd_pefree(store_data->gc_table, persistent);
		}
		mnd_pefree(store_data, persistent);
		store_data = NULL;
	}

	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_xa_store_mysql_dtor_conn_close */
static void
mysqlnd_ms_xa_store_mysql_dtor_conn_close(void ** data, zend_bool persistent TSRMLS_DC)
{
	MYSQLND_MS_XA_STATE_STORE_MYSQL * store_data;
	DBG_ENTER("mysqlnd_ms_xa_store_mysql_dtor_conn_close");

	if (data && *data) {
		 store_data = (MYSQLND_MS_XA_STATE_STORE_MYSQL *)*data;
	}

	if (store_data && store_data->conn) {
		mysqlnd_close(store_data->conn, MYSQLND_CLOSE_DISCONNECTED);
		store_data->conn = NULL;
	}
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_xa_store_mysql_ctor */
enum_func_status
mysqlnd_ms_xa_store_mysql_ctor(MYSQLND_MS_XA_STATE_STORE * store, zend_bool persistent TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	DBG_ENTER("mysqlnd_ms_xa_store_mysql_ctor");

	store->data = mnd_pecalloc(1, sizeof(MYSQLND_MS_XA_STATE_STORE_MYSQL), persistent);
	if (!store->data) {
		DBG_RETURN(ret);
	}

	store->name = SECT_XA_STORE_MYSQL;
	store->load_config = mysqlnd_ms_xa_store_mysql_load_config;
	store->begin = mysqlnd_ms_xa_store_mysql_begin;
	store->monitor_change_state = mysqlnd_ms_xa_store_mysql_monitor_change_state;
	store->monitor_failure = mysqlnd_ms_xa_store_mysql_monitor_failure;
	store->monitor_finish = mysqlnd_ms_xa_store_mysql_monitor_finish;
	store->add_participant = mysqlnd_ms_xa_store_mysql_add_participant;
	store->participant_change_state = mysqlnd_ms_xa_store_mysql_participant_change_state;
	store->participant_failure = mysqlnd_ms_xa_store_mysql_participant_failure;
	store->garbage_collect_one = mysqlnd_ms_xa_store_mysql_gc_one;
	store->garbage_collect_all = mysqlnd_ms_xa_store_mysql_gc_all;
	store->dtor = mysqlnd_ms_xa_store_mysql_dtor;
	store->dtor_conn_close = mysqlnd_ms_xa_store_mysql_dtor_conn_close;

	ret = PASS;
	DBG_RETURN(ret);
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
