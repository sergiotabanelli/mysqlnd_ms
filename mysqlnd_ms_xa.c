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

#include "ext/standard/php_rand.h"

#include "mysqlnd_ms_xa.h"
#include "mysqlnd_ms.h"
#include "mysqlnd_ms_config_json.h"
#include "mysqlnd_ms_xa_store_mysql.h"
#include "mysqlnd_ms_conn_pool.h"

#define BEGIN_ITERATE_OVER_PARTICIPANT_LIST(el, participants) \
{ \
	DBG_INF_FMT("pariticipants(%p) has %d", \
		(participants), zend_llist_count((zend_llist *) (participants))); \
	{ \
		MYSQLND_MS_XA_PARTICIPANT_LIST_DATA ** el_pp;\
		zend_llist_position	pos; \
		for (el_pp = (MYSQLND_MS_XA_PARTICIPANT_LIST_DATA **) zend_llist_get_first_ex((zend_llist *) (participants), &pos); \
			 el_pp && ((el) = *el_pp); \
			 el_pp = (MYSQLND_MS_XA_PARTICIPANT_LIST_DATA **) zend_llist_get_next_ex((zend_llist *) (participants), &pos)) \
		{ \
			if (!(el)->conn) { \
				php_error_docref(NULL TSRMLS_CC, E_ERROR, MYSQLND_MS_ERROR_PREFIX " A participants connection is missing." \
								"Either you are using Fabric or something is very wrong. Please, report a bug"); \
			} else { \


#define END_ITERATE_OVER_PARTICIPANT_LIST \
			} \
		} \
	} \
}

#define MYSQLND_MS_XA_PROXY_ERROR_INFO(proxy_conn, proxy_conn_data) \
  &(MYSQLND_MS_ERROR_INFO((((proxy_conn_data)->stgy.last_used_conn) ? (proxy_conn_data)->stgy.last_used_conn : (proxy_conn))))

#define MYSQLND_MS_XA_GTRID_EQUALS_ID(gtrid, id) \
   ((id).gtrid == (gtrid))

#define MYSQLND_MS_XA_ERR_GLOBAL_LOCAL_TRX " The command cannot be executed when global transaction is in the  ACTIVE state. You must end the global/XA transaction first"
#define MYSQLND_MS_XA_ERR_GLOBAL_LOCAL_TRX_SQLSTATE "XAE07"
#define MYSQLND_MS_XA_ERR_GLOBAL_LOCAL_TRX_ERRNO 1399

#define MYSQLND_MS_XA_ERR_LOCAL_GLOBAL_TRX " Some work is done outside global transaction. You must end the active local transaction first"
#define MYSQLND_MS_XA_ERR_LOCAL_GLOBAL_TRX_SQLSTATE "XAE09"
#define MYSQLND_MS_XA_ERR_LOCAL_GLOBAL_TRX_ERRNO 1400

/* {{{ mysqlnd_ms_xa_state_to_string */
void
mysqlnd_ms_xa_state_to_string(enum mysqlnd_ms_xa_state state, _ms_smart_type * str)
{
	switch (state) {
		case XA_NON_EXISTING:
			_ms_smart_method(appendl, str, "XA_NON_EXISTING" , sizeof("XA_NON_EXISTING") - 1);
			break;
		case XA_ACTIVE:
			_ms_smart_method(appendl, str, "XA_ACTIVE", sizeof("XA_ACTIVE") - 1);
			break;
		case XA_IDLE:
			_ms_smart_method(appendl, str, "XA_IDLE", sizeof("XA_IDLE") - 1);
			break;
		case XA_PREPARED:
			_ms_smart_method(appendl, str, "XA_PREPARED", sizeof("XA_PREPARED") - 1);
			break;
		case XA_COMMIT:
			_ms_smart_method(appendl, str, "XA_COMMIT", sizeof("XA_COMMIT") - 1);
			break;
		case XA_ROLLBACK:
			_ms_smart_method(appendl, str, "XA_ROLLBACK", sizeof("XA_ROLLBACK") - 1);
			break;
		default:
			/* forgotten option? */
			assert(0);
			break;
	}
	_ms_smart_method(0, str);
}
/* }}} */

/* {{{ mysqlnd_ms_load_xa_config */
void mysqlnd_ms_load_xa_config(struct st_mysqlnd_ms_config_json_entry * main_section,
						   MYSQLND_MS_XA_TRX * xa_trx,
						   MYSQLND_ERROR_INFO * error_info,
						   zend_bool persistent TSRMLS_DC)
{
	zend_bool entry_exists;
	zend_bool entry_is_list;
	char * json_value = NULL;
	int json_int = 0;
	struct st_mysqlnd_ms_config_json_entry * xa_section;

	DBG_ENTER("mysqlnd_ms_load_xa_config");

	xa_section = mysqlnd_ms_config_json_sub_section(main_section, SECT_XA_NAME, sizeof(SECT_XA_NAME)-1, &entry_exists TSRMLS_CC);
	if (entry_exists && xa_section) {
		struct st_mysqlnd_ms_config_json_entry * xa_sub_section;

		xa_sub_section = mysqlnd_ms_config_json_sub_section(xa_section, SECT_XA_STATE_STORE, sizeof(SECT_XA_STATE_STORE)-1, &entry_exists TSRMLS_CC);
		if (entry_exists && xa_sub_section) {
			struct st_mysqlnd_ms_config_json_entry * state_store_section;

			state_store_section = mysqlnd_ms_config_json_sub_section(xa_sub_section, SECT_XA_STORE_MYSQL, sizeof(SECT_XA_STORE_MYSQL)-1, &entry_exists TSRMLS_CC);
			if (entry_exists && state_store_section) {

				/* store is part of the GC struct */
				xa_trx->gc = mnd_pecalloc(1, sizeof(MYSQLND_MS_XA_GC), TRUE /* persistent */);
//				if (FALSE) {
				if (xa_trx->gc) {
					xa_trx->gc->gc_max_retries = 5;
					xa_trx->gc->gc_probability = 5;
					xa_trx->gc->gc_max_trx_per_run = 100;
					xa_trx->gc->added_to_module_globals = FALSE;
				}

				if (!xa_trx->gc) {
						mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " Failed to allocate memory for XA storage backend '" SECT_XA_STORE_MYSQL
										  "'");
				} else {
					enum_func_status ok = mysqlnd_ms_xa_store_mysql_ctor(&(xa_trx->gc->store), TRUE TSRMLS_CC);
					if (FAIL == ok) {
						mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
											MYSQLND_MS_ERROR_PREFIX " Failed to setup XA storage backend '" SECT_XA_STORE_MYSQL
											"'");
						xa_trx->gc->store.dtor(&(xa_trx->gc->store.data), TRUE TSRMLS_CC);
						mnd_pefree(xa_trx->gc, TRUE);
						xa_trx->gc = NULL;
					} else {
						xa_trx->gc->store.load_config(state_store_section, xa_trx->gc->store.data, error_info, TRUE TSRMLS_CC);
					}
				}
//				}
			} else {
				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
											  MYSQLND_MS_ERROR_PREFIX " Currently '" SECT_XA_STORE_MYSQL
											  "' is the only supported state store. Failed to find matching entry in config");
				mnd_pefree(xa_trx->gc, TRUE);
				xa_trx->gc = NULL;
			}
			if (xa_trx->gc) {
				/* register in global list of garbage collection settings, if no GC has been registered for the host (= cluster/config section) already */
				MYSQLND_MS_XA_GC _ms_p_zval * hentry_pp = NULL;
				if (SUCCESS != _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, &MYSQLND_MS_G(xa_state_stores), xa_trx->host, xa_trx->host_len, hentry_pp)) {
//				if (!zend_hash_str_exists(&MYSQLND_MS_G(xa_state_stores), xa_trx->host, xa_trx->host_len + 1)) {
					if (_MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, &MYSQLND_MS_G(xa_state_stores), xa_trx->host, xa_trx->host_len, xa_trx->gc)) {
						mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
												MYSQLND_MS_ERROR_PREFIX " Failed to setup garbage collection %s", xa_trx->host);
							xa_trx->gc->store.dtor(&(xa_trx->gc->store.data), TRUE TSRMLS_CC);
							mnd_pefree(xa_trx->gc, TRUE);
							xa_trx->gc = NULL;
					} else {
						xa_trx->gc->added_to_module_globals = TRUE;
					}
				}
			}

			json_value = mysqlnd_ms_config_json_string_from_section(xa_sub_section, SECT_XA_STORE_PARTICIPANT_LOCALHOST, sizeof(SECT_XA_STORE_PARTICIPANT_LOCALHOST) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
			if (entry_exists && json_value) {
				if (entry_is_list) {
					mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_XA_STORE_PARTICIPANT_LOCALHOST, SECT_XA_STATE_STORE);
				} else {
					xa_trx->participant_localhost_ip = mnd_pestrndup(json_value, strlen(json_value), persistent);
				}
				mnd_efree(json_value);
			} else {
				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
											MYSQLND_MS_ERROR_PREFIX " '%s' must be set in section '%s'. "
											"The garbage collection may run on any of your PHP servers. "
											"To work properly it needs to know the IP of 'localhost'/'.' "
											"entries from the state stores participant list. "
											"If you have not configured any socket/pipe connections, "
											"set %s = '127.0.0.1'. Otherwise use the real IP of 'localhost'/'.'",
											SECT_XA_STORE_PARTICIPANT_LOCALHOST, SECT_XA_STATE_STORE,
											SECT_XA_STORE_PARTICIPANT_LOCALHOST);
			}

			json_value = mysqlnd_ms_config_json_string_from_section(xa_sub_section, SECT_XA_STORE_PARTICIPANT_CRED, sizeof(SECT_XA_STORE_PARTICIPANT_CRED) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
			if (entry_exists && json_value) {
				if (entry_is_list) {
					mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
												MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_XA_STORE_PARTICIPANT_CRED, SECT_XA_STATE_STORE);
				} else {
					xa_trx->record_participant_cred = !mysqlnd_ms_config_json_string_is_bool_false(json_value);
				}
				mnd_efree(json_value);
			} else {
				xa_trx->record_participant_cred = FALSE;
			}
		}

		json_value = mysqlnd_ms_config_json_string_from_section(xa_section, SECT_XA_ROLLBACK_ON_CLOSE, sizeof(SECT_XA_ROLLBACK_ON_CLOSE) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
		if (entry_exists && json_value) {
			if (entry_is_list) {
				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
											MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a string", SECT_XA_ROLLBACK_ON_CLOSE, SECT_XA_NAME);
			} else {
				xa_trx->rollback_on_close = !mysqlnd_ms_config_json_string_is_bool_false(json_value);
			}
			mnd_efree(json_value);
		}

		xa_sub_section = mysqlnd_ms_config_json_sub_section(xa_section, SECT_XA_GC_NAME, sizeof(SECT_XA_GC_NAME)-1, &entry_exists TSRMLS_CC);
		if (entry_exists && xa_sub_section) {

			if (!xa_trx->gc) {
				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
											MYSQLND_MS_ERROR_PREFIX " Garbage collection is unavailable. Either no state store was configured or setting up a state store failed. All settings from '%s' will be ignored", SECT_XA_GC_NAME);
			} else {

				json_int = mysqlnd_ms_config_json_int_from_section(xa_sub_section, SECT_XA_GC_MAX_RETRIES, sizeof(SECT_XA_GC_MAX_RETRIES) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
				if (entry_exists) {
					if (entry_is_list) {
						mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
													MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a number (0...100)", SECT_XA_GC_MAX_RETRIES, SECT_XA_GC_NAME);
					} else {
						if (json_int < 0 || json_int > 100) {
							mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
													MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a number between 0 and 100", 	SECT_XA_GC_MAX_RETRIES, SECT_XA_GC_NAME);
						} else {
							xa_trx->gc->gc_max_retries = json_int;
						}
					}

				}

				json_int = mysqlnd_ms_config_json_int_from_section(xa_sub_section, SECT_XA_GC_MAX_TRX_PER_RUN, sizeof(SECT_XA_GC_MAX_TRX_PER_RUN) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
				if (entry_exists) {
					if (entry_is_list) {
						mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
													MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a number (1...32768)", SECT_XA_GC_MAX_TRX_PER_RUN, SECT_XA_GC_NAME);
					} else {
						if (json_int < 1 || json_int > 32768) {
							mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
													MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a number between 1 and 32768", SECT_XA_GC_MAX_TRX_PER_RUN, SECT_XA_GC_NAME);
						} else {
							xa_trx->gc->gc_max_trx_per_run = json_int;
						}
					}
				}

				json_int = mysqlnd_ms_config_json_int_from_section(xa_sub_section, SECT_XA_GC_PROBABILITY, sizeof(SECT_XA_GC_PROBABILITY) - 1, 0, &entry_exists, &entry_is_list TSRMLS_CC);
				if (entry_exists) {
					if (entry_is_list) {
						mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
													MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a number (0...1000)", SECT_XA_GC_PROBABILITY, SECT_XA_GC_NAME);
					} else {
						if (json_int < 0 || json_int > 1000) {
							mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_RECOVERABLE_ERROR TSRMLS_CC,
													MYSQLND_MS_ERROR_PREFIX " '%s' from '%s' must be a number between 0 and 1000", SECT_XA_GC_PROBABILITY, SECT_XA_GC_NAME);
						} else {
							xa_trx->gc->gc_probability = json_int;
						}
					}
				}
			}
		}

	}

	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_xa_participant_list_dtor */
static void
mysqlnd_ms_xa_participant_list_dtor(void * pDest)
{
	MYSQLND_MS_XA_PARTICIPANT_LIST_DATA * element = pDest? *(MYSQLND_MS_XA_PARTICIPANT_LIST_DATA **) pDest : NULL;
	TSRMLS_FETCH();
	DBG_ENTER("mysqlnd_ms_xa_participant_list_dtor");
	if (!element) {
		DBG_VOID_RETURN;
	}

	mnd_pefree(element, element->persistent);
	DBG_VOID_RETURN;
}
/* }}} */

static void
mysqlnd_ms_xa_participant_list_free_pool_ref(void *pDest, void *arg TSRMLS_DC)
{
	MYSQLND_MS_XA_PARTICIPANT_LIST_DATA * element = pDest? *(MYSQLND_MS_XA_PARTICIPANT_LIST_DATA **) pDest : NULL;
	MYSQLND_MS_POOL * pool = (MYSQLND_MS_POOL *)arg;
	DBG_ENTER("mysqlnd_ms_xa_participant_list_free_pool_ref");
	if (element && element->conn && pool) {
		pool->free_reference(pool, element->conn TSRMLS_CC);
	}
	DBG_VOID_RETURN;
}


/* {{{ mysqlnd_ms_xa_monitor_change_state */
static enum_func_status
mysqlnd_ms_xa_monitor_change_state(MYSQLND_CONN_DATA * proxy_conn, enum mysqlnd_ms_xa_state to, enum mysqlnd_ms_xa_state intend TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);
	MYSQLND_ERROR_INFO * error_info = MYSQLND_MS_XA_PROXY_ERROR_INFO(proxy_conn, *proxy_conn_data);

	DBG_ENTER("mysqlnd_ms_xa_monitor_change_state");
	if ((*proxy_conn_data)->xa_trx->gc) {
		ret = (*proxy_conn_data)->xa_trx->gc->store.monitor_change_state((*proxy_conn_data)->xa_trx->gc->store.data,
																		error_info,
																		&((*proxy_conn_data)->xa_trx->id),
																		to, intend TSRMLS_CC);

		if (PASS != ret) {
			/*
			If this one also fails, it will overwrite the previous error. If so, the previous error
			may be reported as a warning only, assuming the state store throws a warning. Throwing
			a warning shall be considered implementation dependent.
			*/
			(*proxy_conn_data)->xa_trx->gc->store.monitor_failure(
					(*proxy_conn_data)->xa_trx->gc->store.data,
					error_info,
					&((*proxy_conn_data)->xa_trx->id),
					intend TSRMLS_CC);
		}
	}
	if (PASS == ret) {
		(*proxy_conn_data)->xa_trx->state = to;
	}
	DBG_INF_FMT("state=%d", (*proxy_conn_data)->xa_trx->state);
	DBG_RETURN(ret);
}
/* }}} */


static enum_func_status
mysqlnd_ms_xa_participant_change_state(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_MS_XA_PARTICIPANT_LIST_DATA * participant,
									   enum mysqlnd_ms_xa_state to,
									   const char * sql, size_t sql_len,
									   zend_bool report_error TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	MS_DECLARE_AND_LOAD_CONN_DATA(participant_conn_data, participant->conn);
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);
	MYSQLND_ERROR_INFO tmp_error_info = { {'\0'}, {'\0'}, 0, NULL };
	MYSQLND_ERROR_INFO * error_info = MYSQLND_MS_XA_PROXY_ERROR_INFO(proxy_conn, *proxy_conn_data);

	DBG_ENTER("mysqlnd_ms_xa_participant_change_state");
	_ms_mysqlnd_error_info_init(&tmp_error_info);

	if (!report_error) {
		/* preserve current error */
		SET_CLIENT_ERROR(_ms_a_ei tmp_error_info, error_info->error_no, error_info->sqlstate, error_info->error);
	}

	(*participant_conn_data)->skip_ms_calls = TRUE;
	ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(query)(participant->conn, (const char *)sql, sql_len TSRMLS_CC);
	(*participant_conn_data)->skip_ms_calls = FALSE;

	if (PASS == ret) {
		if ((*proxy_conn_data)->xa_trx->gc) {
			ret = (*proxy_conn_data)->xa_trx->gc->store.participant_change_state((*proxy_conn_data)->xa_trx->gc->store.data,
																			(report_error) ? error_info : NULL,
																			&((*proxy_conn_data)->xa_trx->id),
																			participant,
																			participant->state, to TSRMLS_CC);
		}
		if (PASS == ret) {
			participant->state = to;
		}
	} else {
		if ((*proxy_conn_data)->xa_trx->gc) {
			(*proxy_conn_data)->xa_trx->gc->store.participant_failure((*proxy_conn_data)->xa_trx->gc->store.data,
																		(report_error) ? error_info : NULL,
																		&((*proxy_conn_data)->xa_trx->id),
																		participant,
																		&MYSQLND_MS_ERROR_INFO(participant->conn) TSRMLS_CC);
		}
		if (report_error && !tmp_error_info.error_no)
			COPY_CLIENT_ERROR(_ms_p_ei error_info, MYSQLND_MS_ERROR_INFO(participant->conn));

	}

	if (PASS != ret) {
		DBG_INF_FMT("conn="MYSQLND_LLU_SPEC", id=%d (%p) state=%d failed to change to state=%d",
			participant->conn->thread_id, participant->id, participant->conn, participant->state, to);
	}
	if (!report_error) {
		SET_CLIENT_ERROR(_ms_p_ei error_info, tmp_error_info.error_no, tmp_error_info.sqlstate, tmp_error_info.error);
	}
	_ms_mysqlnd_error_info_free_contents(&tmp_error_info);

	DBG_RETURN(ret);
}

static enum_func_status
mysqlnd_ms_xa_participants_change_state(MYSQLND_CONN_DATA * proxy_conn,
										enum mysqlnd_ms_xa_state from, enum mysqlnd_ms_xa_state to,
										const char * sql, size_t sql_len,
										zend_bool report_error TSRMLS_DC)
{
	MYSQLND_MS_CONN_DATA ** participant_conn_data;
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);
	MYSQLND_MS_XA_PARTICIPANT_LIST_DATA * participant = NULL;
	enum_func_status ret = PASS, ok = FAIL;
	MYSQLND_ERROR_INFO tmp_error_info = { {'\0'}, {'\0'}, 0, NULL };
	MYSQLND_ERROR_INFO * error_info = MYSQLND_MS_XA_PROXY_ERROR_INFO(proxy_conn, *proxy_conn_data);

	DBG_ENTER("mysqlnd_ms_xa_participants_change_state");
	_ms_mysqlnd_error_info_init(&tmp_error_info);
	if (!report_error) {
		/* preserve current error */
		SET_CLIENT_ERROR(_ms_a_ei tmp_error_info, error_info->error_no, error_info->sqlstate, error_info->error);
	}

	BEGIN_ITERATE_OVER_PARTICIPANT_LIST(participant, &(*proxy_conn_data)->xa_trx->participants)
		MS_LOAD_CONN_DATA(participant_conn_data, participant->conn);

		if (participant->state == from) {

			(*participant_conn_data)->skip_ms_calls = TRUE;
			ok = MS_CALL_ORIGINAL_CONN_DATA_METHOD(query)(participant->conn, sql, sql_len TSRMLS_CC);
			(*participant_conn_data)->skip_ms_calls = FALSE;

			if (PASS == ok) {
				if ((*proxy_conn_data)->xa_trx->gc) {
					ok = (*proxy_conn_data)->xa_trx->gc->store.participant_change_state((*proxy_conn_data)->xa_trx->gc->store.data,
																					(report_error) ? error_info : NULL,
																					&((*proxy_conn_data)->xa_trx->id),
																					participant,
																					from, to TSRMLS_CC);
				}
				if (ok == PASS) {
					participant->state = to;
				} else {
					ret = ok;
					if (report_error && !tmp_error_info.error_no) {
						SET_CLIENT_ERROR(_ms_a_ei tmp_error_info, error_info->error_no, error_info->sqlstate, error_info->error);
					}
				}

			} else {
				_ms_smart_type state_to = { 0, 0, 0 };
				mysqlnd_ms_xa_state_to_string(to, &state_to);

				if ((*proxy_conn_data)->xa_trx->gc) {
					(*proxy_conn_data)->xa_trx->gc->store.participant_failure((*proxy_conn_data)->xa_trx->gc->store.data,
																			(report_error) ? error_info : NULL,
																			&((*proxy_conn_data)->xa_trx->id),
																			participant,
																			&MYSQLND_MS_ERROR_INFO(participant->conn) TSRMLS_CC);
				}

				DBG_INF_FMT("conn="MYSQLND_LLU_SPEC", id=%d (%p) state=%d failed to change to state=%d",
						participant->conn->thread_id, participant->id, participant->conn, from, to);

				ret = ok;
				if (report_error && !tmp_error_info.error_no) {
					mysqlnd_ms_client_n_php_error(
						&tmp_error_info,
						(MYSQLND_MS_ERROR_INFO(participant->conn).error_no),
						(MYSQLND_MS_ERROR_INFO(participant->conn).sqlstate),
						E_WARNING TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " Failed to switch participant to %s state: %s",
						state_to.c,
						(MYSQLND_MS_ERROR_INFO(participant->conn).error)
					);
				}
				_ms_smart_method(free, &state_to);
			}
		}
	END_ITERATE_OVER_PARTICIPANT_LIST

	if (tmp_error_info.error_no) {
		SET_CLIENT_ERROR(_ms_p_ei error_info, tmp_error_info.error_no, tmp_error_info.sqlstate, tmp_error_info.error);
	}
	_ms_mysqlnd_error_info_free_contents(&tmp_error_info);
	DBG_RETURN(ret);
}

/* {{{ mysqlnd_ms_xa_monitor_begin
 Begin XA transaction
 */
enum_func_status
mysqlnd_ms_xa_monitor_begin(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_MS_CONN_DATA * proxy_conn_data,
							unsigned int gtrid, unsigned int timeout TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	MYSQLND_ERROR_INFO * error_info = MYSQLND_MS_XA_PROXY_ERROR_INFO(proxy_conn, proxy_conn_data);
	DBG_ENTER("mysqlnd_ms_xa_monitor_begin");

	SET_EMPTY_ERROR(_ms_p_ei error_info);

	if (!proxy_conn_data->xa_trx) {
		/* Something is very wrong */
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection or connection has not been intitialized");
		DBG_RETURN(ret);
	}

	if (TRUE == proxy_conn_data->xa_trx->on) {
		/* User must commit the current XA trx first */
		mysqlnd_ms_client_n_php_error(
			error_info,
			MYSQLND_MS_XA_ERR_GLOBAL_LOCAL_TRX_ERRNO, MYSQLND_MS_XA_ERR_GLOBAL_LOCAL_TRX_SQLSTATE,
			E_WARNING TSRMLS_CC, MYSQLND_MS_ERROR_PREFIX MYSQLND_MS_XA_ERR_GLOBAL_LOCAL_TRX);
		DBG_RETURN(ret);
	}

	/* TODO XA:
	 * Local and global transactions are mutually exclusive. It may be the case
	 * a user has started a transaction on a local server, then called xa begin.
	 * Our xa begin does not immediately issue any query, we start the global trx
	 * in a lazy fashion as we jump from participant to participant. Now, the
	 * next server we hit might not be the one on which the local trx has been started.
	 * As things go one, we come along the server with the local trx and *bang*...
	 */
	if (TRUE == proxy_conn_data->stgy.in_transaction) {
		/* If the user has used an API call to start a local transaction we may be lucky to detect the issue upfront */
		mysqlnd_ms_client_n_php_error(
			error_info,
			MYSQLND_MS_XA_ERR_LOCAL_GLOBAL_TRX_ERRNO, MYSQLND_MS_XA_ERR_LOCAL_GLOBAL_TRX_SQLSTATE,
			E_WARNING TSRMLS_CC, MYSQLND_MS_ERROR_PREFIX MYSQLND_MS_XA_ERR_LOCAL_GLOBAL_TRX);
		DBG_RETURN(ret);
	}


	/* state monitors needs this, reset in case of failure */
	proxy_conn_data->xa_trx->id.gtrid = gtrid;

	if (proxy_conn_data->xa_trx->gc) {
		ret = proxy_conn_data->xa_trx->gc->store.begin(proxy_conn_data->xa_trx->gc->store.data,
													error_info,	&(proxy_conn_data->xa_trx->id),
													timeout TSRMLS_CC);
		if (PASS != ret) {
			DBG_RETURN(ret);
		}
	}
	/* We don't know yet on which servers we will end up. No SQL to be run. */

	/* Note the _global_ state, participants may have other state as we move on */
	ret = mysqlnd_ms_xa_monitor_change_state(proxy_conn, XA_NON_EXISTING, XA_NON_EXISTING TSRMLS_CC);
	if (PASS == ret) {
		proxy_conn_data->xa_trx->on = TRUE;
		proxy_conn_data->xa_trx->in_transaction = FALSE;
		proxy_conn_data->xa_trx->timeout = timeout;
		assert(0 == zend_llist_count(&proxy_conn_data->xa_trx->participants));
		MYSQLND_MS_INC_STATISTIC(MS_STAT_XA_BEGIN);
	} else {
		if (proxy_conn_data->xa_trx->gc) {
			proxy_conn_data->xa_trx->gc->store.monitor_finish(
				proxy_conn_data->xa_trx->gc->store.data,
				error_info,
				&proxy_conn_data->xa_trx->id,
				FALSE TSRMLS_CC);
		}
		MYSQLND_MS_XA_ID_RESET(proxy_conn_data->xa_trx->id);
	}
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_xa_rollback
 Switch as many RMs/MySQL servers to ROLLBACK as we can
 */
static enum_func_status
mysqlnd_ms_xa_rollback(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_MS_CONN_DATA * proxy_conn_data,
					   MYSQLND_ERROR_INFO * error_info,
					   zend_bool report_error TSRMLS_DC)
{
	enum_func_status ret = PASS, ok = FAIL;
	MYSQLND_MS_XA_PARTICIPANT_LIST_DATA * participant = NULL;
	char * sql;
	int sql_len;
	DBG_ENTER("mysqlnd_ms_xa_rollback");

	BEGIN_ITERATE_OVER_PARTICIPANT_LIST(participant, &proxy_conn_data->xa_trx->participants)
		if ((participant->state >= XA_IDLE) && (participant->state <= XA_COMMIT)) {

			sql_len = spprintf(&sql, 0, "XA ROLLBACK '%d'", proxy_conn_data->xa_trx->id.gtrid);
			ok = mysqlnd_ms_xa_participant_change_state(proxy_conn, participant, XA_ROLLBACK, (const char*)sql, sql_len, report_error TSRMLS_CC);
			efree(sql);

			if (FAIL == ok) {
				if (report_error) {
					mysqlnd_ms_client_n_php_error(
						error_info,
						((participant->conn) ? (MYSQLND_MS_ERROR_INFO(participant->conn).error_no) : CR_UNKNOWN_ERROR),
						((participant->conn) ? (MYSQLND_MS_ERROR_INFO(participant->conn).sqlstate) : UNKNOWN_SQLSTATE),
						E_WARNING TSRMLS_CC,
						MYSQLND_MS_ERROR_PREFIX " Failed to switch participant to XA_ROLLBACK state: %s",
						((participant->conn) ? (MYSQLND_MS_ERROR_INFO(participant->conn).error) : ""));
				}

				if (proxy_conn_data->xa_trx->gc) {
					proxy_conn_data->xa_trx->gc->store.monitor_failure(
						proxy_conn_data->xa_trx->gc->store.data,
						(report_error) ? error_info : NULL,
						&(proxy_conn_data->xa_trx->id),
						XA_ROLLBACK TSRMLS_CC);
				}
				/*
				 Don't stop the loop: switch as many servers to rollback as we can.
				 We may have more RMs/MySQL servers in our list that are in prepared
				 state and should be moved to rollback as early as possible.
				*/
				ret = FAIL;
			}
		}
	END_ITERATE_OVER_PARTICIPANT_LIST

	DBG_RETURN(ret);
}

/* }}} */

/* {{{ mysqlnd_ms_xa_monitor_direct_commit
 Commit XA transaction
 */
enum_func_status
mysqlnd_ms_xa_monitor_direct_commit(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_MS_CONN_DATA * proxy_conn_data, unsigned int gtrid TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	char * sql;
	int sql_len;
	MYSQLND_ERROR_INFO * error_info = MYSQLND_MS_XA_PROXY_ERROR_INFO(proxy_conn, proxy_conn_data);

	DBG_ENTER("mysqlnd_ms_xa_monitor_direct_commit");

	SET_EMPTY_ERROR(_ms_p_ei error_info);

	/* TODO XA: bark */
	if (!proxy_conn_data->xa_trx) {
		/* Something is very wrong */
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection or connection has not been intitialized");
		DBG_RETURN(ret);
	}

	if (FALSE == proxy_conn_data->xa_trx->on) {
		mysqlnd_ms_client_n_php_error(
			error_info,
			CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC, MYSQLND_MS_ERROR_PREFIX " There is no active XA transaction to commit");
		DBG_RETURN(ret);
	}

	if (!MYSQLND_MS_XA_GTRID_EQUALS_ID(gtrid, proxy_conn_data->xa_trx->id)) {
		/* TODO XA: xa should manage gtrids itself and silently, user should not pass gtrid? */
		mysqlnd_ms_client_n_php_error(
			error_info,
			CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC, MYSQLND_MS_ERROR_PREFIX " The XA transaction id does not match the one of from XA begin");
		DBG_RETURN(ret);
	}

	if (XA_NON_EXISTING != proxy_conn_data->xa_trx->state) {
		/* TODO XA: maybe set the actual SQL errno/state of this case? */
		mysqlnd_ms_client_n_php_error(
			error_info,
			CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC, MYSQLND_MS_ERROR_PREFIX " XA transaction is not in its initial XA_NON_EXISTING state");
		DBG_RETURN(ret);
	}

	/* We've gone past the logical errors that shall be ignored by the stats */

	/* Rollback on close logic shall be able to see whether it was preceded by a failed commit/rollback. */
	proxy_conn_data->xa_trx->finish_transaction_intend = XA_COMMIT;

	if (0 != zend_llist_count(&proxy_conn_data->xa_trx->participants)) {
		/*
		* All participants in our list are in ACTIVE = XA BEGIN sent state
		*   Try to move them to IDLE = XA END sent state
		*     Success?
		*        Yes -> move forward
		*        No -> Return FALSE. No RM has comitted anything, but try to
		*              clean up as many RMs as possible and inform the TM state store
		*             about the outcome.
		*/
		sql_len = spprintf(&sql, 0, "XA END '%d'", proxy_conn_data->xa_trx->id.gtrid);
		ret = mysqlnd_ms_xa_participants_change_state(proxy_conn, XA_ACTIVE, XA_IDLE, (const char*)sql, sql_len, TRUE TSRMLS_CC);
		efree(sql);
		if (PASS == ret) {
			/* Global state goes from non_existing to idle */
			ret = mysqlnd_ms_xa_monitor_change_state(proxy_conn, XA_IDLE, XA_COMMIT TSRMLS_CC);
		}

		if (PASS != ret) {
			MYSQLND_MS_INC_STATISTIC(MS_STAT_XA_COMMIT_FAILURE);

			if (proxy_conn_data->xa_trx->gc) {
				proxy_conn_data->xa_trx->gc->store.monitor_failure(
					proxy_conn_data->xa_trx->gc->store.data,
					error_info,
					&(proxy_conn_data->xa_trx->id),
					XA_COMMIT TSRMLS_CC);
			}
			/* rollback as much as we can to release locks */
			sql_len = spprintf(&sql, 0, "XA PREPARE '%d'", proxy_conn_data->xa_trx->id.gtrid);
			mysqlnd_ms_xa_participants_change_state(proxy_conn, XA_IDLE, XA_PREPARED, (const char*)sql, sql_len, FALSE TSRMLS_CC);
			efree(sql);

			mysqlnd_ms_xa_rollback(proxy_conn, proxy_conn_data, error_info, FALSE TSRMLS_CC);
			if (proxy_conn_data->xa_trx->gc) {
				proxy_conn_data->xa_trx->gc->store.monitor_finish(
					proxy_conn_data->xa_trx->gc->store.data,
					error_info,
					&proxy_conn_data->xa_trx->id,
					TRUE TSRMLS_CC);
			}
			goto end_direct_commit;
		}

		/*
		* Are all participants in <predecessor_state> = <last XA something> state?
		*   Yes
		*     Try to move them to <successor_state> = <next XA something> sent state
		*       Success?
		*          Yes -> move forward
		*          No -> Phase 1 error - TM to roll back
		*   No
		*    IMPOSSIBLE, either all or nothing is the outcome of the previous state change
		*/

		sql_len = spprintf(&sql, 0, "XA PREPARE '%d'", proxy_conn_data->xa_trx->id.gtrid);
		ret = mysqlnd_ms_xa_participants_change_state(proxy_conn, XA_IDLE, XA_PREPARED, (const char*)sql, sql_len, TRUE TSRMLS_CC);
		efree(sql);
		if (PASS == ret) {
			ret = mysqlnd_ms_xa_monitor_change_state(proxy_conn, XA_PREPARED, XA_COMMIT TSRMLS_CC);
		}
		if (PASS != ret) {
			MYSQLND_MS_INC_STATISTIC(MS_STAT_XA_COMMIT_FAILURE);
			if (proxy_conn_data->xa_trx->gc) {
				proxy_conn_data->xa_trx->gc->store.monitor_failure(
					proxy_conn_data->xa_trx->gc->store.data,
					error_info,
					&proxy_conn_data->xa_trx->id,
					XA_COMMIT TSRMLS_CC);
			}

			/* phase 1 error, rollback as many of the prepared ones as we can */
			mysqlnd_ms_xa_rollback(proxy_conn, proxy_conn_data, error_info, FALSE TSRMLS_CC);

			if (proxy_conn_data->xa_trx->gc) {
				proxy_conn_data->xa_trx->gc->store.monitor_finish(
					proxy_conn_data->xa_trx->gc->store.data,
					error_info,
					&proxy_conn_data->xa_trx->id,
					/* leave the rest to the GC - a crashed server may come back in XA prepared state and we will roll back thereafter */
					TRUE TSRMLS_CC);
			}

			goto end_direct_commit;
		}

		sql_len = spprintf(&sql, 0, "XA COMMIT '%d'", proxy_conn_data->xa_trx->id.gtrid);
		ret = mysqlnd_ms_xa_participants_change_state(proxy_conn, XA_PREPARED, XA_COMMIT, (const char*)sql, sql_len, TRUE TSRMLS_CC);
		efree(sql);
		if (PASS != ret) {
			MYSQLND_MS_INC_STATISTIC(MS_STAT_XA_COMMIT_FAILURE);
			if (proxy_conn_data->xa_trx->gc) {
				proxy_conn_data->xa_trx->gc->store.monitor_failure(
					proxy_conn_data->xa_trx->gc->store.data,
					error_info,
					&proxy_conn_data->xa_trx->id,
					XA_COMMIT TSRMLS_CC);

				/* leave the rest to the GC */
				proxy_conn_data->xa_trx->gc->store.monitor_finish(
					proxy_conn_data->xa_trx->gc->store.data,
					error_info,
					&proxy_conn_data->xa_trx->id,
					TRUE TSRMLS_CC);
			}
			goto end_direct_commit;
		}
	}

	ret = PASS;
	MYSQLND_MS_INC_STATISTIC(MS_STAT_XA_COMMIT_SUCCESS);

	/* Persist the state change before we erase gtrid and such.
	   It does not really matter whether this call succeed or not: the RMs are
	   in stable committed state. Just be polite and clean up the TM state store.
	 */
	mysqlnd_ms_xa_monitor_change_state(proxy_conn, XA_COMMIT, XA_COMMIT TSRMLS_CC);

	/* the RMs should be happy - either there was no participant or all have comitted */
	if (proxy_conn_data->xa_trx->gc) {
		proxy_conn_data->xa_trx->gc->store.monitor_finish(
					proxy_conn_data->xa_trx->gc->store.data,
					error_info,
					&proxy_conn_data->xa_trx->id,
					FALSE TSRMLS_CC);
	}

end_direct_commit:
	/* if case of any phase 1 or phase 2 RM error, the user cannot try the operation again */
	proxy_conn_data->xa_trx->finish_transaction_intend = XA_NON_EXISTING;
	proxy_conn_data->xa_trx->on = FALSE;
	proxy_conn_data->xa_trx->in_transaction = FALSE;
	MYSQLND_MS_XA_ID_RESET(proxy_conn_data->xa_trx->id);
	proxy_conn_data->xa_trx->timeout = 0;

	zend_llist_apply_with_argument(&proxy_conn_data->xa_trx->participants, mysqlnd_ms_xa_participant_list_free_pool_ref, proxy_conn_data->pool TSRMLS_CC);
	zend_llist_clean(&proxy_conn_data->xa_trx->participants);

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_xa_monitor_direct_rollback
 Commit XA transaction
 */
enum_func_status
mysqlnd_ms_xa_monitor_direct_rollback(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_MS_CONN_DATA * proxy_conn_data, unsigned int gtrid TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	MYSQLND_ERROR_INFO * error_info = MYSQLND_MS_XA_PROXY_ERROR_INFO(proxy_conn, proxy_conn_data);
	char * sql;
	int sql_len;

	DBG_ENTER("mysqlnd_ms_xa_monitor_direct_rollback");

	SET_EMPTY_ERROR(_ms_p_ei error_info);

	/* TODO XA: bark */
	if (!proxy_conn_data->xa_trx) {
		/* Something is very wrong */
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
						 MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection or connection has not been intitialized");
		DBG_RETURN(ret);
	}

	if (FALSE == proxy_conn_data->xa_trx->on) {
		mysqlnd_ms_client_n_php_error(
			error_info,
			CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
			MYSQLND_MS_ERROR_PREFIX " There is no active XA transaction to rollback"
		);
		DBG_RETURN(ret);
	}

	if (!MYSQLND_MS_XA_GTRID_EQUALS_ID(gtrid, proxy_conn_data->xa_trx->id)) {
		/* TODO XA: xa should manage gtrids itself and silently, user should not pass gtrid? */
		mysqlnd_ms_client_n_php_error(
			error_info,
			CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
			MYSQLND_MS_ERROR_PREFIX " The XA transaction id does not match the one of from XA begin"
		);
		DBG_RETURN(ret);
	}

	DBG_INF_FMT("current state=%d", proxy_conn_data->xa_trx->state);

	/* Rollback on close logic shall be able to see whether it was preceded by a failed commit/rollback. */
	proxy_conn_data->xa_trx->finish_transaction_intend = XA_ROLLBACK;

	/* Ensure all are in XA_IDLE state */
	sql_len = spprintf(&sql, 0, "XA END '%d'", proxy_conn_data->xa_trx->id.gtrid);
	ret = mysqlnd_ms_xa_participants_change_state(proxy_conn, XA_ACTIVE, XA_IDLE, (const char*)sql, sql_len, TRUE TSRMLS_CC);
	efree(sql);
	if (PASS == ret) {
		ret = mysqlnd_ms_xa_monitor_change_state(proxy_conn, XA_IDLE, XA_ROLLBACK TSRMLS_CC);
	}
	if (PASS == ret) {
		ret = mysqlnd_ms_xa_rollback(proxy_conn, proxy_conn_data, error_info, TRUE TSRMLS_CC);
	}
	if (PASS != ret) {
		MYSQLND_MS_INC_STATISTIC(MS_STAT_XA_ROLLBACK_FAILURE);

		if (proxy_conn_data->xa_trx->gc) {
			proxy_conn_data->xa_trx->gc->store.monitor_failure(
				proxy_conn_data->xa_trx->gc->store.data,
				error_info,
				&(proxy_conn_data->xa_trx->id),
				XA_ROLLBACK TSRMLS_CC);
		}

		mysqlnd_ms_xa_rollback(proxy_conn, proxy_conn_data, error_info, FALSE TSRMLS_CC);

		if (proxy_conn_data->xa_trx->gc) {
			proxy_conn_data->xa_trx->gc->store.monitor_finish(
				proxy_conn_data->xa_trx->gc->store.data,
				error_info,
				&proxy_conn_data->xa_trx->id,
				TRUE TSRMLS_CC);
		}
		goto end_direct_rollback;
	}

	MYSQLND_MS_INC_STATISTIC(MS_STAT_XA_ROLLBACK_SUCCESS);
	/* don't care much about the monitor, RMs are in safe waters, TM has defined state too */
	mysqlnd_ms_xa_monitor_change_state(proxy_conn, XA_ROLLBACK, XA_ROLLBACK TSRMLS_CC);

	if (proxy_conn_data->xa_trx->gc) {
		proxy_conn_data->xa_trx->gc->store.monitor_finish(
					proxy_conn_data->xa_trx->gc->store.data,
					error_info,
					&proxy_conn_data->xa_trx->id,
					FALSE TSRMLS_CC);
	}

end_direct_rollback:
	proxy_conn_data->xa_trx->finish_transaction_intend = XA_NON_EXISTING;
	proxy_conn_data->xa_trx->on = FALSE;
	proxy_conn_data->xa_trx->in_transaction = FALSE;
	MYSQLND_MS_XA_ID_RESET(proxy_conn_data->xa_trx->id);
	proxy_conn_data->xa_trx->timeout = 0;

	zend_llist_apply_with_argument(&proxy_conn_data->xa_trx->participants, mysqlnd_ms_xa_participant_list_free_pool_ref, proxy_conn_data->pool TSRMLS_CC);
	zend_llist_clean(&proxy_conn_data->xa_trx->participants);

	DBG_RETURN(ret);
}
/* }}} */



/* {{{ mysqlnd_ms_xa_gc_one
 Enforce garbage collection run for a particular trx
 */
enum_func_status
mysqlnd_ms_xa_gc_one(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_MS_CONN_DATA * proxy_conn_data,
					 unsigned int gtrid, zend_bool ignore_max_retries TSRMLS_DC) {
	enum_func_status ret = PASS;
	MYSQLND_ERROR_INFO * error_info = MYSQLND_MS_XA_PROXY_ERROR_INFO(proxy_conn, proxy_conn_data);
	MYSQLND_MS_XA_ID id;
	DBG_ENTER("mysqlnd_ms_xa_gc_one");
	DBG_INF_FMT("ignore_max_retries=%d", ignore_max_retries);

	SET_EMPTY_ERROR(_ms_p_ei error_info);

	if (!proxy_conn_data->xa_trx) {
		/* Something is very wfrong */
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection or connection has not been intitialized");
		DBG_RETURN(ret);
	}

	MYSQLND_MS_XA_ID_RESET(id);
	id.gtrid = gtrid;

	if (proxy_conn_data->xa_trx->gc &&  proxy_conn_data->xa_trx->gc->store.garbage_collect_one) {
		ret = proxy_conn_data->xa_trx->gc->store.garbage_collect_one(
			proxy_conn_data->xa_trx->gc->store.data, error_info,
			&id,
			((ignore_max_retries) ? 0 : proxy_conn_data->xa_trx->gc->gc_max_retries)
			TSRMLS_CC);
	}

	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_xa_gc_all
 Enforce garbage collection run for any XA trx
 */
enum_func_status
mysqlnd_ms_xa_gc_all(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_MS_CONN_DATA * proxy_conn_data,
					 zend_bool ignore_max_retries TSRMLS_DC) {
	enum_func_status ret = PASS;
	MYSQLND_ERROR_INFO * error_info = MYSQLND_MS_XA_PROXY_ERROR_INFO(proxy_conn, proxy_conn_data);
	DBG_ENTER("mysqlnd_ms_xa_gc_all");
	DBG_INF_FMT("ignore_max_retries=%d", ignore_max_retries);

	SET_EMPTY_ERROR(_ms_p_ei error_info);

	if (!proxy_conn_data->xa_trx) {
		/* Something is very wrong */
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection or connection has not been intitialized");
		DBG_RETURN(ret);
	}

	if (proxy_conn_data->xa_trx->gc &&  proxy_conn_data->xa_trx->gc->store.garbage_collect_all) {
		ret = proxy_conn_data->xa_trx->gc->store.garbage_collect_all(
			proxy_conn_data->xa_trx->gc->store.data, error_info,
			(ignore_max_retries) ? 0 : proxy_conn_data->xa_trx->gc->gc_max_retries,
			proxy_conn_data->xa_trx->gc->gc_max_trx_per_run TSRMLS_CC);
	}

	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_xa_proxy_conn_init
 Initialize CONN_DATA struct
 */
MYSQLND_MS_XA_TRX *
mysqlnd_ms_xa_proxy_conn_init(const char * host, size_t host_len, zend_bool persistent TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_xa_proxy_conn_init");
	MYSQLND_MS_XA_TRX *trx = mnd_pecalloc(1, sizeof(MYSQLND_MS_XA_TRX), persistent);

	if (trx) {
		if (!host_len) {
			/* unlikely */
			DBG_INF("No host given, aborting.");
			mnd_pefree(trx, persistent);
			trx = NULL;
		} else {
			/* reundant */
			trx->on = FALSE;
			trx->in_transaction = FALSE;
			trx->rollback_on_close = FALSE;
			trx->finish_transaction_intend = XA_NON_EXISTING;
			trx->gc = NULL;
			trx->participant_localhost_ip = NULL;
			trx->record_participant_cred = FALSE;
			MYSQLND_MS_XA_ID_RESET(trx->id);
			trx->timeout = 0;
			zend_llist_init(&trx->participants, sizeof(MYSQLND_MS_XA_PARTICIPANT_LIST_DATA *), (llist_dtor_func_t) mysqlnd_ms_xa_participant_list_dtor, persistent);
			trx->state = XA_NON_EXISTING;

			trx->host = mnd_pecalloc(host_len + 1, sizeof(char *), persistent);
			strncpy(trx->host, host, host_len);
			trx->host_len = host_len;
		}
	}

	DBG_RETURN(trx);
}
/* }}} */


/* {{{ mysqlnd_ms_xa_free
 Free CONN_DATA struct during mysqlnd free plugin data
 */
void
mysqlnd_ms_xa_proxy_conn_free(MYSQLND_MS_CONN_DATA * proxy_conn_data, zend_bool persistent TSRMLS_DC)
{
	MYSQLND_MS_XA_TRX * trx = proxy_conn_data->xa_trx;
	DBG_ENTER("mysqlnd_ms_xa_free");
	/* TODO XA: we may still have an open XA trx here, need to add cleanup code, likely rollback... */

	zend_llist_apply_with_argument(&trx->participants, mysqlnd_ms_xa_participant_list_free_pool_ref, proxy_conn_data->pool TSRMLS_CC);
	zend_llist_clean(&trx->participants);

	if (trx->participant_localhost_ip) {
		mnd_pefree(trx->participant_localhost_ip, persistent);
	}

	if (trx->host) {
		mnd_pefree(trx->host, persistent);
	}

	if (trx->gc) {
		if (trx->gc->added_to_module_globals) {
			/* must not free the store settings yet, but can close the store conns */
			trx->gc->store.dtor_conn_close(&(trx->gc->store.data), TRUE TSRMLS_CC);
		} else {
			/* don't need the store anymore, no background GC coming */
			trx->gc->store.dtor(&(trx->gc->store.data), TRUE TSRMLS_CC);
		}
	}

	mnd_pefree(trx, persistent);
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_xa_gc_hash_dtor
 Free GC hash list entries
 */
void mysqlnd_ms_xa_gc_hash_dtor(_ms_hash_zval_type * pDest) {
	unsigned long rnd_idx;
	MYSQLND_MS_XA_GC * gc =  pDest ? _ms_p_zval (MYSQLND_MS_XA_GC _ms_p_zval *) _MS_HASH_Z_PTR_P(pDest) : NULL;
	DBG_ENTER("mysqlnd_ms_xa_gc_hash_dtor");

	TSRMLS_FETCH();

	if (gc && gc->store.data) {
		rnd_idx = php_rand(TSRMLS_C);
		RAND_RANGE(rnd_idx, 1, 1000, PHP_RAND_MAX);
		if (gc->gc_probability >= rnd_idx) {
			gc->store.garbage_collect_all(gc->store.data, NULL, gc->gc_max_retries, gc->gc_max_trx_per_run TSRMLS_CC);
		}

		gc->store.dtor(&(gc->store.data), TRUE TSRMLS_CC);
		mnd_pefree(gc, TRUE);
		gc->store.data = NULL;
	}
	DBG_VOID_RETURN;
}

/* {{{ mysqlnd_ms_xa_add_participant */
static enum_func_status
mysqlnd_ms_xa_add_participant(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_CONN_DATA * next_conn TSRMLS_DC)
{
	MYSQLND_MS_XA_PARTICIPANT_LIST_DATA * participant;
	enum_func_status ret = FAIL;
	char * sql;
	int sql_len;
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);
	MS_DECLARE_AND_LOAD_CONN_DATA(participant_conn_data, next_conn);
	MYSQLND_ERROR_INFO * error_info = MYSQLND_MS_XA_PROXY_ERROR_INFO(proxy_conn, *proxy_conn_data);

	DBG_ENTER("mysqlnd_ms_xa_add_participant");

	participant = mnd_pecalloc(1, sizeof(MYSQLND_MS_XA_PARTICIPANT_LIST_DATA), proxy_conn->persistent);
	if (participant) {

		participant->state = XA_NON_EXISTING;
		participant->persistent = proxy_conn->persistent;
		participant->conn = next_conn;

		(*participant_conn_data)->skip_ms_calls = TRUE;
		/* TODO XA and elsewhere: OOM */
		sql_len = spprintf(&sql, 0, "XA BEGIN '%d'", (*proxy_conn_data)->xa_trx->id.gtrid);
		ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(query)(next_conn, (const char *)sql, sql_len TSRMLS_CC);
		efree(sql);
		(*participant_conn_data)->skip_ms_calls = FALSE;

		if (PASS != ret) {
			/* free and bark */
			mnd_pefree(participant, proxy_conn->persistent);
			mysqlnd_ms_client_n_php_error(
					error_info,
					MYSQLND_MS_ERROR_INFO(next_conn).error_no,
					MYSQLND_MS_ERROR_INFO(next_conn).sqlstate,
					E_WARNING TSRMLS_CC,
					MYSQLND_MS_ERROR_PREFIX " Failed to add participant and switch to XA_BEGIN state: %s",
					MYSQLND_MS_ERROR_INFO(next_conn).error);
			DBG_RETURN(ret);
		}

		/* let the state store follow our state settings */
		participant->state = XA_ACTIVE;
		/* e.g. use for bqual */
		participant->id = zend_llist_count(&(*proxy_conn_data)->xa_trx->participants);

		if ((*proxy_conn_data)->xa_trx->gc) {
			ret = (*proxy_conn_data)->xa_trx->gc->store.add_participant((*proxy_conn_data)->xa_trx->gc->store.data,
																	error_info, &((*proxy_conn_data)->xa_trx->id),
																	participant,
																	(*proxy_conn_data)->xa_trx->record_participant_cred,
																	(*proxy_conn_data)->xa_trx->participant_localhost_ip
																	TSRMLS_CC);
		}
		if (PASS == ret) {
			zend_llist_add_element(&(*proxy_conn_data)->xa_trx->participants, &participant);
			MYSQLND_MS_INC_STATISTIC(MS_STAT_XA_PARTICIPANTS);

			/* Make sure this connection does not get closed before the XA trx is done */
			(*proxy_conn_data)->pool->add_reference((*proxy_conn_data)->pool, participant->conn TSRMLS_CC);

			DBG_INF_FMT("id=%d", participant->id);
		} else {
			mnd_pefree(participant, proxy_conn->persistent);
		}
	} else {
		MYSQLND_MS_WARN_OOM();
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_xa_inject_query */
enum_func_status
mysqlnd_ms_xa_inject_query(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_CONN_DATA * next_conn, zend_bool switched_servers TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);
	MYSQLND_ERROR_INFO * error_info = MYSQLND_MS_XA_PROXY_ERROR_INFO(proxy_conn, *proxy_conn_data);

	DBG_ENTER("mysqlnd_ms_xa_inject_query");

	if (!(*proxy_conn_data)->proxy_conn) {
		DBG_INF("TODO XA: Fabric");
		DBG_RETURN(ret);
	}

	/* TODO XA: Correct, isn't it? We must be called from proxy_conn::query() */
	assert(proxy_conn == (*proxy_conn_data)->proxy_conn);
	assert(next_conn);

	if (FALSE == (*proxy_conn_data)->xa_trx->on) {
		/* Nothing to do... */
		DBG_RETURN(ret);
	}

	if (TRUE == (*proxy_conn_data)->stgy.in_transaction) {
		/* User has stated a global transaction followed by a local transaction. Global and local trx are mutually exclusive. */
		mysqlnd_ms_client_n_php_error(
			error_info,
			MYSQLND_MS_XA_ERR_LOCAL_GLOBAL_TRX_ERRNO, MYSQLND_MS_XA_ERR_LOCAL_GLOBAL_TRX_SQLSTATE,
			E_WARNING TSRMLS_CC, MYSQLND_MS_ERROR_PREFIX MYSQLND_MS_XA_ERR_LOCAL_GLOBAL_TRX);
		ret = FAIL;
		DBG_RETURN(ret);
	}

	/* in theory safe to assume conn_data exists ... */
	if (XA_NON_EXISTING == (*proxy_conn_data)->xa_trx->state) {
		/*
		*  XA_NON_EXISTING
		*
		*   Switched servers?
		*     Yes
		*       Success?
		*        Yes
		*          -> Add server to XA server list, if not done yet
		*          -> XA begin, if not done yet
		*        No
		*          IMPOSSIBLE: only called with a valid connection
		*          Panic, if we have XA trx on any server. /User/ must rollback then.
		*     No
		*      -> Add server to XA server list, if not done yet
		*      -> XA begin, if not done yet
		*/
		MYSQLND_MS_XA_PARTICIPANT_LIST_DATA * participant;
		zend_bool found = FALSE;

		BEGIN_ITERATE_OVER_PARTICIPANT_LIST(participant, &(*proxy_conn_data)->xa_trx->participants)
			if (next_conn == participant->conn) {
				found = TRUE;
				break;
			}
		END_ITERATE_OVER_PARTICIPANT_LIST

		if (FALSE == found) {
			/* Add server to XA server list, if not done yet */
			ret = mysqlnd_ms_xa_add_participant(proxy_conn, next_conn TSRMLS_CC);
		}
	}

	DBG_RETURN(ret);
}
/* }}} */

enum_func_status mysqlnd_ms_xa_conn_close(MYSQLND_CONN_DATA * proxy_conn TSRMLS_DC) {

	enum_func_status ret = PASS;
	MS_DECLARE_AND_LOAD_CONN_DATA(proxy_conn_data, proxy_conn);

	DBG_ENTER("mysqlnd_ms_xa_conn_close");
	if (!proxy_conn_data || !(*proxy_conn_data)) {
		DBG_RETURN(ret);
	}

	if (!(*proxy_conn_data)->proxy_conn) {
		DBG_INF("TODO XA: Fabric");
		DBG_RETURN(ret);
	}

	assert(proxy_conn == (*proxy_conn_data)->proxy_conn);
	if ((FALSE == (*proxy_conn_data)->xa_trx->on)) {
		DBG_RETURN(ret);
	}

	if ((*proxy_conn_data)->xa_trx->finish_transaction_intend != XA_NON_EXISTING) {
		/*
			Immediately before the close, the user failed to either commit or rollback the trx.
			Whatever he tried, he failed. THe user has been given feedback on the failure and
			has had the opportunity to fix the situation. Either the situation was ignored
			or the decision was made to leave the rest to the GC. We will not overrule
			the user.
		*/
		DBG_INF("Previous commit/rollback failed, no automatic action");
		DBG_RETURN(ret);
	}

	if (TRUE == (*proxy_conn_data)->xa_trx->rollback_on_close) {
		/* automatic rollback will inform the TM about the end */
		MYSQLND_MS_INC_STATISTIC(MS_STAT_XA_ROLLBACK_ON_CLOSE);
		ret = mysqlnd_ms_xa_monitor_direct_rollback(proxy_conn, *proxy_conn_data, (*proxy_conn_data)->xa_trx->id.gtrid TSRMLS_CC);
	} else {
		/* no automatic rollback, just full engine stop - warn TM */
		if ((*proxy_conn_data)->xa_trx->gc) {
			(*proxy_conn_data)->xa_trx->gc->store.monitor_finish(
					(*proxy_conn_data)->xa_trx->gc->store.data,
					MYSQLND_MS_XA_PROXY_ERROR_INFO(proxy_conn, (*proxy_conn_data)),
					&((*proxy_conn_data)->xa_trx->id),
					TRUE TSRMLS_CC);
		}
	}
	DBG_RETURN(ret);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
