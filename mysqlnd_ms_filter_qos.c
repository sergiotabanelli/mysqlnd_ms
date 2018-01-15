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
  | Author: Ulf Wendel <uw@php.net>                                      |
  |         Andrey Hristov <andrey@php.net>                              |
  +----------------------------------------------------------------------+
*/

/* $Id: mysqlnd_ms.c 311179 2011-05-18 11:26:22Z andrey $ */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "ext/mysqlnd/mysqlnd.h"
#include "ext/mysqlnd/mysqlnd_debug.h"
#include "ext/mysqlnd/mysqlnd_priv.h"
#ifndef mnd_emalloc
#include "ext/mysqlnd/mysqlnd_alloc.h"
#endif
#include "ext/mysqlnd/mysqlnd_ext_plugin.h"
#if PHP_VERSION_ID >= 70100
#include "ext/mysqlnd/mysqlnd_connection.h"
#endif

#ifdef MYSQLND_MS_HAVE_MYSQLND_QC
#include "ext/mysqlnd_qc/mysqlnd_qc.h"
#endif

#include "mysqlnd_ms.h"
#include "mysqlnd_ms_config_json.h"
#include "mysqlnd_ms_enum_n_def.h"
#include "mysqlnd_ms_switch.h"


// BEGIN HACK

static enum_func_status
mysqlnd_ms_qos_server_has_gtid(MYSQLND_CONN_DATA * conn, MYSQLND_MS_CONN_DATA ** conn_data, char *sql, size_t sql_len,
								unsigned int wait_time,
								MYSQLND_ERROR_INFO * tmp_error_info TSRMLS_DC);

/* {{{ mysqlnd_ms_section_filters_set_gtid_qos */
enum_func_status
mysqlnd_ms_section_filters_set_gtid_qos(MYSQLND_CONN_DATA * conn, char * gtid, size_t gtid_len TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	enum_func_status ret = FAIL;
	DBG_ENTER("mysqlnd_ms_section_filters_set_gtid_qos");
	DBG_INF_FMT("set gtid %s", gtid);
	if (conn_data && *conn_data) {
		struct mysqlnd_ms_lb_strategies * stgy = &(*conn_data)->stgy;
		zend_llist * filters = stgy->filters;
		MYSQLND_MS_FILTER_QOS_DATA * qos_filter = NULL;
		MYSQLND_MS_FILTER_DATA * filter, ** filter_pp;
		zend_llist_position	pos;
		if (conn != (*conn_data)->proxy_conn) {
			MS_LOAD_CONN_DATA(conn_data, (*conn_data)->proxy_conn);
			stgy = &(*conn_data)->stgy;
			filters = stgy->filters;
		}
		for (filter_pp = (MYSQLND_MS_FILTER_DATA **) zend_llist_get_first_ex(filters, &pos);
			 filter_pp && (filter = *filter_pp) && (!qos_filter);
			  (filter_pp = (MYSQLND_MS_FILTER_DATA **) zend_llist_get_next_ex(filters, &pos)))
		{
			if (filter->pick_type == SERVER_PICK_QOS) {
				qos_filter = (MYSQLND_MS_FILTER_QOS_DATA *) filter;
			}
		}
		if (qos_filter && qos_filter->consistency == CONSISTENCY_SESSION) {
			if (gtid && gtid_len) {
				if (qos_filter->option_data.gtid_len) {
					efree(qos_filter->option_data.gtid);
					qos_filter->option_data.gtid_len = 0;
					qos_filter->option_data.gtid = NULL;
				}
				qos_filter->option_data.gtid_len = gtid_len;
				qos_filter->option_data.gtid = estrndup(gtid, gtid_len);
				qos_filter->option = QOS_OPTION_GTID;
				DBG_INF_FMT("set qos gtid %s", qos_filter->option_data.gtid);
			}
			ret = PASS;
		}
	}
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_section_filters_switch_qos */
static void
mysqlnd_ms_section_filters_switch_qos(const char * query, size_t query_len, MYSQLND_MS_FILTER_QOS_DATA * filter_data TSRMLS_DC)
{
	zend_bool forced = FALSE;
	enum mysqlnd_ms_filter_qos_consistency which_qos = mysqlnd_ms_query_which_qos(query, query_len, &forced TSRMLS_CC);
	DBG_ENTER("mysqlnd_ms_section_filters_switch_qos");
	if (!forced) {
		DBG_VOID_RETURN;
	}
	if (filter_data) {
		filter_data->consistency = which_qos;
		DBG_INF_FMT("set qos consistency %d", which_qos);
	}
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_section_filters_is_gtid_qos */
enum_func_status
mysqlnd_ms_section_filters_is_gtid_qos(MYSQLND_CONN_DATA * conn TSRMLS_DC)
{
  	MYSQLND_MS_CONN_DATA ** conn_data;
	enum_func_status ret = FAIL;
	DBG_ENTER("mysqlnd_ms_section_filters_is_gtid_qos");
	conn_data = (MYSQLND_MS_CONN_DATA **) _ms_mysqlnd_plugin_get_plugin_connection_data_data(conn, mysqlnd_ms_plugin_id);
	if (conn_data && *conn_data) {
		struct mysqlnd_ms_lb_strategies * stgy = &(*conn_data)->stgy;
		zend_llist * filters = stgy->filters;
		MYSQLND_MS_FILTER_QOS_DATA * qos_filter = NULL;
		MYSQLND_MS_FILTER_DATA * filter, ** filter_pp;
		zend_llist_position	pos;

		for (filter_pp = (MYSQLND_MS_FILTER_DATA **) zend_llist_get_first_ex(filters, &pos);
			 filter_pp && (filter = *filter_pp) && (!qos_filter);
			  (filter_pp = (MYSQLND_MS_FILTER_DATA **) zend_llist_get_next_ex(filters, &pos)))
		{
			if (filter->pick_type == SERVER_PICK_QOS) {
				qos_filter = (MYSQLND_MS_FILTER_QOS_DATA *) filter;
			}
		}
		if (qos_filter && qos_filter->consistency == CONSISTENCY_SESSION) {
			ret = PASS;
		}
	}
	DBG_RETURN(ret);
}
/* }}} */

// END HACK

/* {{{ qos_filter_dtor */
static void
qos_filter_dtor(struct st_mysqlnd_ms_filter_data * pDest TSRMLS_DC)
{
	MYSQLND_MS_FILTER_QOS_DATA * filter = (MYSQLND_MS_FILTER_QOS_DATA *) pDest;
	DBG_ENTER("qos_filter_dtor");

	if (filter->option_data.gtid_len) {
		efree(filter->option_data.gtid);
		filter->option_data.gtid_len = 0;
		filter->option_data.gtid = NULL;
	}
	mnd_pefree(filter, filter->parent.persistent);

	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ qos_filter_conn_pool_replaced */
static void
qos_filter_conn_pool_replaced(struct st_mysqlnd_ms_filter_data * data, zend_llist * master_connections, zend_llist * slave_connections, MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC)
{
	DBG_ENTER("qos_filter_conn_pool_replaced");
	DBG_VOID_RETURN;
}

/* {{{ mysqlnd_ms_qos_filter_ctor */
MYSQLND_MS_FILTER_DATA *
mysqlnd_ms_qos_filter_ctor(struct st_mysqlnd_ms_config_json_entry * section, zend_llist * master_connections, zend_llist * slave_connections, MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC)
{
	MYSQLND_MS_FILTER_QOS_DATA * ret = NULL;
	DBG_ENTER("mysqlnd_ms_qos_filter_ctor");
	DBG_INF_FMT("section=%p", section);
	if (section) {
		ret = mnd_pecalloc(1, sizeof(MYSQLND_MS_FILTER_QOS_DATA), persistent);

		if (ret) {
			zend_bool value_exists = FALSE, is_list_value = FALSE;
			char * service;

			ret->parent.filter_dtor = qos_filter_dtor;
			ret->parent.filter_conn_pool_replaced = qos_filter_conn_pool_replaced;

			ret->consistency = CONSISTENCY_LAST_ENUM_ENTRY;

			service = mysqlnd_ms_config_json_string_from_section(section, SECT_QOS_STRONG, sizeof(SECT_QOS_STRONG) - 1, 0,
																  &value_exists, &is_list_value TSRMLS_CC);
			if (value_exists) {
				DBG_INF("strong consistency");
				mnd_efree(service);
				ret->consistency = CONSISTENCY_STRONG;
			}

			service = mysqlnd_ms_config_json_string_from_section(section, SECT_QOS_SESSION, sizeof(SECT_QOS_SESSION) - 1, 0,
																  &value_exists, &is_list_value TSRMLS_CC);
			if (value_exists) {
				DBG_INF("session consistency");
				mnd_efree(service);
				if (ret->consistency != CONSISTENCY_LAST_ENUM_ENTRY) {
					mnd_pefree(ret, persistent);
					php_error_docref(NULL TSRMLS_CC, E_ERROR,
									 MYSQLND_MS_ERROR_PREFIX " Error by creating filter '%s', '%s' clashes with previous setting. Stopping", PICK_QOS, SECT_QOS_SESSION);
				} else {
					ret->consistency = CONSISTENCY_SESSION;
				}
			}

			service = mysqlnd_ms_config_json_string_from_section(section, SECT_QOS_EVENTUAL, sizeof(SECT_QOS_EVENTUAL) - 1, 0,
															  &value_exists, &is_list_value TSRMLS_CC);
			if (value_exists) {
				DBG_INF("eventual consistency");
				mnd_efree(service);
				if (ret->consistency != CONSISTENCY_LAST_ENUM_ENTRY) {
					mnd_pefree(ret, persistent);
					php_error_docref(NULL TSRMLS_CC, E_ERROR,
									 MYSQLND_MS_ERROR_PREFIX " Error by creating filter '%s', '%s' clashes with previous setting. Stopping", PICK_QOS, SECT_QOS_EVENTUAL);
				} else {
					ret->consistency = CONSISTENCY_EVENTUAL;

					if (TRUE == is_list_value) {
						zend_bool section_exists;
						struct st_mysqlnd_ms_config_json_entry * eventual_section =
							mysqlnd_ms_config_json_sub_section(section, SECT_QOS_EVENTUAL, sizeof(SECT_QOS_EVENTUAL) - 1, &section_exists TSRMLS_CC);

						if (section_exists && eventual_section) {
							char * json_value;

							json_value = mysqlnd_ms_config_json_string_from_section(eventual_section, SECT_QOS_AGE, sizeof(SECT_QOS_AGE) - 1, 0,
																				  &value_exists, &is_list_value TSRMLS_CC);
							if (value_exists && json_value) {
								ret->option = QOS_OPTION_AGE;
								ret->option_data.age = atol(json_value);
								mnd_efree(json_value);
							}

							json_value = mysqlnd_ms_config_json_string_from_section(eventual_section, SECT_QOS_CACHE, sizeof(SECT_QOS_CACHE) - 1, 0,
																				  &value_exists, &is_list_value TSRMLS_CC);
							if (value_exists && json_value) {
								if (QOS_OPTION_AGE == ret->option) {
									mnd_pefree(ret, persistent);
									mnd_efree(json_value);
									php_error_docref(NULL TSRMLS_CC, E_ERROR,
									 MYSQLND_MS_ERROR_PREFIX " Error by creating filter '%s', '%s' has conflicting entries for cache and age. Stopping", PICK_QOS, SECT_QOS_EVENTUAL);
								} else {
									ret->option = QOS_OPTION_CACHE;
									/* TODO - Andrey, do we need range checks? */
									ret->option_data.ttl = (uint)atol(json_value);
									mnd_efree(json_value);
								}
							}
						}
					}
				}
			}
			switch (ret->consistency) {
				case CONSISTENCY_STRONG:
				case CONSISTENCY_SESSION:
				case CONSISTENCY_EVENTUAL:
					break;
				default:
					mnd_pefree(ret, persistent);
					ret = NULL;
					php_error_docref(NULL TSRMLS_CC, E_ERROR, MYSQLND_MS_ERROR_PREFIX
						" Error by creating filter '%s', can't find section '%s', '%s' or '%s' . Stopping",
						PICK_QOS, SECT_QOS_STRONG, SECT_QOS_SESSION, SECT_QOS_EVENTUAL);
			}
		} else {
			MYSQLND_MS_WARN_OOM();
		}
	}

	DBG_RETURN((MYSQLND_MS_FILTER_DATA *) ret);
}
/* }}} */


/* {{{ mysqlnd_ms_qos_server_has_gtid */
static enum_func_status
mysqlnd_ms_qos_server_has_gtid(MYSQLND_CONN_DATA * conn, MYSQLND_MS_CONN_DATA ** conn_data, char *sql, size_t sql_len,
								unsigned int wait_time,
								MYSQLND_ERROR_INFO * tmp_error_info TSRMLS_DC)
{
	MYSQLND_RES * res = NULL;
	enum_func_status ret = FAIL;
	uint64_t total_time = 0, run_time = 0, my_wait_time = wait_time * 1000000;
#if MYSQLND_VERSION_ID >= 50010
	MYSQLND_ERROR_INFO * org_error_info;
#else
	MYSQLND_ERROR_INFO org_error_info;
#endif

	DBG_ENTER("mysqlnd_ms_qos_server_has_gtid");
	DBG_INF_FMT("wait_time=%d", wait_time);

	/* hide errors from user */
	org_error_info = conn->error_info;
#if MYSQLND_VERSION_ID >= 50010
	conn->error_info = tmp_error_info;
#else
	SET_EMPTY_ERROR(conn->error_info);
#endif

	(*conn_data)->skip_ms_calls = TRUE;
	if (wait_time) {
		MS_TIME_SET(run_time);
	}
	do {
		if ((PASS == MS_CALL_ORIGINAL_CONN_DATA_METHOD(send_query)(conn, sql, sql_len _MS_SEND_QUERY_AD_EXT TSRMLS_CC)) &&
			(PASS ==  MS_CALL_ORIGINAL_CONN_DATA_METHOD(reap_query)(conn  _MS_REAP_QUERY_AD_EXT TSRMLS_CC)) &&
#if PHP_VERSION_ID < 50600
			(res = MS_CALL_ORIGINAL_CONN_DATA_METHOD(store_result)(conn TSRMLS_CC))
#else
			(res = MS_CALL_ORIGINAL_CONN_DATA_METHOD(store_result)(conn, MYSQLND_STORE_NO_COPY TSRMLS_CC))
#endif
		)
		{
			ret = (MYSQLND_MS_UPSERT_STATUS(conn).affected_rows) ? PASS : FAIL;
			DBG_INF_FMT("sql = %s -  ret = %d - affected_rows = %d", sql, ret, (MYSQLND_MS_UPSERT_STATUS(conn).affected_rows));
		}
		if (wait_time && (FAIL == ret)) {
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
				if (res) {
					res->m.free_result(res, FALSE TSRMLS_CC);
				}
				continue;
			}
		}
		break;
	} while (1);
	(*conn_data)->skip_ms_calls = FALSE;

#if MYSQLND_VERSION_ID < 50010
	*tmp_error_info = conn->error_info;
#endif
	conn->error_info = org_error_info;

	if (res) {
		res->m.free_result(res, FALSE TSRMLS_CC);
	}

	DBG_RETURN(ret);
}
/* }}} */

#define SHOW_SS_QUERY "SHOW SLAVE STATUS"


/* {{{ mysqlnd_ms_qos_server_get_lag_stage1 */
enum_func_status
mysqlnd_ms_qos_server_get_lag_stage1(MYSQLND_CONN_DATA * conn, MYSQLND_MS_CONN_DATA ** conn_data TSRMLS_DC)
{
	enum_func_status ret;

	DBG_ENTER("mysqlnd_ms_qos_server_get_lag_stage1");

	(*conn_data)->skip_ms_calls = TRUE;

	ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(send_query)(conn, SHOW_SS_QUERY , sizeof(SHOW_SS_QUERY) - 1  _MS_SEND_QUERY_AD_EXT TSRMLS_CC);

	(*conn_data)->skip_ms_calls = FALSE;

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_qos_get_lag_stage2 */
static long
mysqlnd_ms_qos_server_get_lag_stage2(MYSQLND_CONN_DATA * conn, MYSQLND_MS_CONN_DATA ** conn_data TSRMLS_DC)
{
	MYSQLND_RES * res = NULL;
	long lag = -1L;

	DBG_ENTER("mysqlnd_ms_qos_server_get_lag_stage2");

	(*conn_data)->skip_ms_calls = TRUE;

	if ((PASS == MS_CALL_ORIGINAL_CONN_DATA_METHOD(reap_query)(conn  _MS_REAP_QUERY_AD_EXT TSRMLS_CC)) &&
#if PHP_VERSION_ID < 50600
		(res = MS_CALL_ORIGINAL_CONN_DATA_METHOD(store_result)(conn TSRMLS_CC))
#else
		(res = MS_CALL_ORIGINAL_CONN_DATA_METHOD(store_result)(conn, MYSQLND_STORE_NO_COPY TSRMLS_CC))
#endif
	)
	{
		zval _ms_p_zval row;
		zval _ms_p_zval * seconds_behind_master;
		zval _ms_p_zval * io_running;
		zval _ms_p_zval * sql_running;

		MAKE_STD_ZVAL(row);
		mysqlnd_fetch_into(res, MYSQLND_FETCH_ASSOC, _ms_a_zval row, MYSQLND_MYSQL);
		if (Z_TYPE_P(_ms_a_zval row) == IS_ARRAY) {
			/* TODO: make test incasesensitive */
			if (FAILURE == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find, Z_ARRVAL_P(_ms_a_zval row), "Slave_IO_Running", sizeof("Slave_IO_Running") - 1, io_running)) {
				SET_CLIENT_ERROR((_ms_p_ei conn->error_info),  CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Failed to extract Slave_IO_Running");
				goto getlagsqlerror;
			}

			if ((Z_TYPE_P(_ms_p_zval io_running) != IS_STRING) ||
				(0 != strncasecmp(Z_STRVAL_P(_ms_p_zval io_running), "Yes", Z_STRLEN_P(_ms_p_zval io_running))))
			{
				SET_CLIENT_ERROR((_ms_p_ei conn->error_info),  CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Slave_IO_Running is not 'Yes'");
				goto getlagsqlerror;
			}

			if (FAILURE == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find, Z_ARRVAL_P(_ms_a_zval row), "Slave_SQL_Running", sizeof("Slave_SQL_Running") - 1, sql_running))
			{
				SET_CLIENT_ERROR((_ms_p_ei conn->error_info),  CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Failed to extract Slave_SQL_Running");
				goto getlagsqlerror;
			}

			if ((Z_TYPE_P(_ms_p_zval io_running) != IS_STRING) ||
				(0 != strncasecmp(Z_STRVAL_P(_ms_p_zval sql_running), "Yes", Z_STRLEN_P(_ms_p_zval sql_running))))
			{
				SET_CLIENT_ERROR((_ms_p_ei conn->error_info),  CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Slave_SQL_Running is not 'Yes'");
				goto getlagsqlerror;
			}

			if (FAILURE == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find, Z_ARRVAL_P(_ms_a_zval row), "Seconds_Behind_Master", sizeof("Seconds_Behind_Master") - 1, seconds_behind_master))
			{
				SET_CLIENT_ERROR((_ms_p_ei conn->error_info),  CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, "Failed to extract Seconds_Behind_Master");
				goto getlagsqlerror;
			}

			lag = Z_LVAL_P(_ms_p_zval seconds_behind_master);
		}

getlagsqlerror:
		_ms_zval_ptr_dtor(_ms_a_zval row);
	}

	(*conn_data)->skip_ms_calls = FALSE;


	if (res) {
		res->m.free_result(res, FALSE TSRMLS_CC);
	}

	DBG_RETURN(lag);
}
/* }}} */


/* {{{ mysqlnd_ms_qos_which_server */
static enum enum_which_server
mysqlnd_ms_qos_which_server(const char * query, size_t query_len, struct mysqlnd_ms_lb_strategies * stgy TSRMLS_DC)
{
	zend_bool forced;
	enum enum_which_server which_server = mysqlnd_ms_query_is_select(query, query_len, &forced TSRMLS_CC);
	DBG_ENTER("mysqlnd_ms_qos_which_server");

	if ((stgy->trx_stickiness_strategy == TRX_STICKINESS_STRATEGY_MASTER) && stgy->in_transaction && !forced) {
		DBG_INF("Enforcing use of master while in transaction");
		which_server = USE_MASTER;
	} else if (stgy->mysqlnd_ms_flag_master_on_write) {
		if (which_server != USE_MASTER) {
			if (stgy->master_used && !forced) {
				switch (which_server) {
					case USE_MASTER:
					case USE_LAST_USED:
						break;
					case USE_SLAVE:
					default:
						DBG_INF("Enforcing use of master after write");
						which_server = USE_MASTER;
						break;
				}
			}
		} else {
			DBG_INF("Use of master detected");
			stgy->master_used = TRUE;
		}
	}

	switch (which_server) {
		case USE_SLAVE:
		case USE_MASTER:
			break;
		case USE_LAST_USED:
			DBG_INF("Using last used connection");
			if (stgy->last_used_conn) {
				/*	TODO: move is_master flag from global trx struct to CONN_DATA */
			} else {
				/* TODO: handle error at this level? */
			}
			break;
		case USE_ALL:
		default:
			break;
	}
 	DBG_RETURN(which_server);
}
/* }}} */


/* {{{ mysqlnd_ms_choose_connection_qos */
enum_func_status
mysqlnd_ms_choose_connection_qos(MYSQLND_CONN_DATA * conn, void * f_data, const char * connect_host,
								 char ** query, size_t * query_len, zend_bool * free_query,
								 zend_llist * master_list, zend_llist * slave_list,
								 zend_llist * selected_masters, zend_llist * selected_slaves,
								 struct mysqlnd_ms_lb_strategies * stgy, MYSQLND_ERROR_INFO * error_info TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_FILTER_QOS_DATA * filter_data = (MYSQLND_MS_FILTER_QOS_DATA *) f_data;
	MYSQLND_MS_LIST_DATA * element;

	DBG_ENTER("mysqlnd_ms_choose_connection_qos");
	DBG_INF_FMT("query(50bytes)=%*s", MIN(50, *query_len), *query);
// BEGIN HACK
	mysqlnd_ms_section_filters_switch_qos(*query, *query_len, filter_data TSRMLS_CC);
// END HACK

	switch (filter_data->consistency) {
		case CONSISTENCY_SESSION:
//BEGIN HACK
			/*
			  For now...
				 We may be able to use selected slaves which have replicated
				 the last write on the line, e.g. using global transaction ID.

				 We may be able to use slaves which have replicated certain,
				 "tagged" writes. For example, the user could have a relaxed
				 definition of session consistency and require only consistent
				 reads from one table. In that case, we may use master and
				 all slaves which have replicated the latest updates on the
				 table in question.
			*/
/*
			if ((QOS_OPTION_GTID == filter_data->option) && (USE_MASTER != mysqlnd_ms_qos_which_server(*query, *query_len, stgy TSRMLS_CC)))
			{
				smart_str sql = {0, 0, 0};
				zend_bool exit_loop = FALSE;

				BEGIN_ITERATE_OVER_SERVER_LIST(element, slave_list)
					MYSQLND_CONN_DATA * connection = element->conn;
					MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, connection);
					if (!conn_data || !*conn_data) {
						continue;
					}

					if ((*conn_data)->global_trx.check_for_gtid && (CONN_GET_STATE(connection) != CONN_QUIT_SENT) &&
						(
							(CONN_GET_STATE(connection) > CONN_ALLOCED) ||
							(PASS == mysqlnd_ms_lazy_connect(element, TRUE TSRMLS_CC)
						)))
					{
						DBG_INF_FMT("Checking slave connection "MYSQLND_LLU_SPEC"", connection->thread_id);

						if (!sql.c) {
							char * pos = strstr((*conn_data)->global_trx.check_for_gtid, "#GTID");
							if (pos) {
							  	smart_str_appendl(&sql, (*conn_data)->global_trx.check_for_gtid,
												  pos - ((*conn_data)->global_trx.check_for_gtid));
								smart_str_appends(&sql, filter_data->option_data.gtid);
								smart_str_appends(&sql, (*conn_data)->global_trx.check_for_gtid + (pos - ((*conn_data)->global_trx.check_for_gtid)) + sizeof("#GTID") - 1);
								smart_str_appendc(&sql, '\0');
							} else {
								mysqlnd_ms_client_n_php_error(NULL, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
										MYSQLND_MS_ERROR_PREFIX " Failed parse SQL for checking GTID. Cannot find #GTID placeholder");
								exit_loop = TRUE;
							}
						}
						if (sql.c) {
							MYSQLND_ERROR_INFO tmp_error_info;
							memset(&tmp_error_info, 0, sizeof(MYSQLND_ERROR_INFO));
							if (PASS == mysqlnd_ms_qos_server_has_gtid(connection, conn_data, sql.c, sql.len - 1, (*conn_data)->global_trx.wait_for_gtid_timeout,  &tmp_error_info TSRMLS_CC)) {
								zend_llist_add_element(selected_slaves, &element);
							} else if (tmp_error_info.error_no) {
								mysqlnd_ms_client_n_php_error(NULL, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
										MYSQLND_MS_ERROR_PREFIX " SQL error while checking slave for GTID: %d/'%s'",
										tmp_error_info.error_no, tmp_error_info.error);
							}
						}
					}
					if (exit_loop) {
						break;
					}

				END_ITERATE_OVER_SERVER_LIST;

				smart_str_free(&sql);

				BEGIN_ITERATE_OVER_SERVER_LIST(element, master_list)
					zend_llist_add_element(selected_masters, &element);
				END_ITERATE_OVER_SERVER_LIST;
				break;
			}
			DBG_INF("fall-through from session consistency");
*/
			{
				MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
//				if ((*conn_data)->global_trx.type != GTID_NONE && QOS_OPTION_GTID == filter_data->option) {
				if ((*conn_data)->global_trx.type != GTID_NONE && (*conn_data)->global_trx.is_prepare == FALSE) {
					enum enum_which_server which_server = mysqlnd_ms_qos_which_server(*query, *query_len, stgy TSRMLS_CC);
					if (which_server == USE_MASTER || which_server == USE_SLAVE) {
						zend_bool is_write = (USE_MASTER == which_server) ||
								((*conn_data)->stgy.trx_stickiness_strategy != TRX_STICKINESS_STRATEGY_DISABLED && (*conn_data)->stgy.in_transaction && ((*conn_data)->stgy.trx_stickiness_strategy == TRX_STICKINESS_STRATEGY_MASTER || (*conn_data)->stgy.trx_read_only == FALSE));
						(*conn_data)->global_trx.m->gtid_filter(conn, filter_data->option_data.gtid, *query, *query_len, slave_list, master_list, selected_slaves, selected_masters, is_write TSRMLS_CC);
						if ((zend_llist_count(selected_masters) + zend_llist_count(selected_slaves)) <= 0) {
							php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Something wrong no valid selection");
							if ((*conn_data)->global_trx.race_avoid_strategy & GTID_RACE_AVOID_ADD_ERROR) {
								DBG_INF_FMT("Race avoid: add error marker %s", MEMCACHED_ERROR_KEY);
								(*conn_data)->global_trx.m->gtid_trace(conn, MEMCACHED_ERROR_KEY, sizeof(MEMCACHED_ERROR_KEY) - 1, 0, *query, *query_len TSRMLS_CC);
							}
							if ((*conn_data)->global_trx.race_avoid_strategy & GTID_RACE_AVOID_ADD_ACTIVE) {
								DBG_INF("Race avoid: add active servers");
								(*conn_data)->global_trx.m->gtid_race_add_active(conn, master_list, selected_masters, is_write TSRMLS_CC);
								if ((USE_SLAVE == which_server)) {
									(*conn_data)->global_trx.m->gtid_race_add_active(conn, slave_list, selected_slaves, FALSE TSRMLS_CC);
								}
							}
						}
						break;
					} else { //Someone other than me will provide ?
						zend_bool forced;
						(*conn_data)->global_trx.injectable_query = mysqlnd_ms_query_is_injectable_query(*query, *query_len, &forced TSRMLS_CC);
						DBG_INF_FMT("Something forced: no master or slave %u add all slaves", which_server);
						BEGIN_ITERATE_OVER_SERVER_LIST(element, slave_list)
							zend_llist_add_element(selected_slaves, &element);
						END_ITERATE_OVER_SERVER_LIST;
					}
				}
	/*
	 * Fall to strong consistency has no sense,
	 * if yuo need strong consistency when no gtid is set
	 * then set strong consistency by config file and
	 * set gtid session consistency programmatically as usual
	 */
				else if (USE_MASTER != mysqlnd_ms_qos_which_server(*query, *query_len, stgy TSRMLS_CC)) {
					DBG_INF("No gtid writes add all slaves");
					BEGIN_ITERATE_OVER_SERVER_LIST(element, slave_list)
						zend_llist_add_element(selected_slaves, &element);
					END_ITERATE_OVER_SERVER_LIST;
				}
				DBG_INF("fall-through from session consistency");
			}
			// END HACK
		case CONSISTENCY_STRONG:
			/*
			For now and forever...
				... use masters, no slaves.

				This is our master_on_write replacement. All the other filters
				don't need to take care in the future.
			*/
			DBG_INF("using masters only for strong consistency");
			BEGIN_ITERATE_OVER_SERVER_LIST(element, master_list)
				zend_llist_add_element(selected_masters, &element);
			END_ITERATE_OVER_SERVER_LIST;
			break;
		case CONSISTENCY_EVENTUAL:
			/*
			For now...
				Either all masters and slaves or
				slaves filtered by SHOW SLAVE STATUS replication lag
				or slaves plus caching
			*/
			{
#ifdef MYSQLND_MS_HAVE_MYSQLND_QC
				zend_bool search_slaves = FALSE;
				uint ttl = 0;

				if ((QOS_OPTION_CACHE == filter_data->option) &&
					(USE_MASTER != mysqlnd_ms_qos_which_server((const char *)*query, *query_len, stgy TSRMLS_CC)))
				{
					char * server_id = NULL;
					int server_id_len;

					/* TODO: If QC tokenizer would be better we should not use conn->host but conn->host_info */
					server_id_len = spprintf(&server_id, 0, "%s|%d|%d|%s|%s", conn->host, conn->port, (conn->charset) ? conn->charset->nr : 0, conn->user, conn->connect_or_select_db? conn->connect_or_select_db:"");

					ttl = filter_data->option_data.ttl;

					if (FALSE == mysqlnd_qc_query_is_cached(conn, (const char *)*query, *query_len, server_id, server_id_len TSRMLS_CC)) {
						DBG_INF("Query is not cached");
						search_slaves = TRUE;
					} else {
						DBG_INF("Query is in the cache");
						search_slaves = FALSE;
					}
					efree(server_id);
				}

				if ((search_slaves || (QOS_OPTION_AGE == filter_data->option)) &&
					(USE_MASTER != mysqlnd_ms_qos_which_server((const char *)*query, *query_len, stgy TSRMLS_CC)))
#else
				if ((QOS_OPTION_AGE == filter_data->option) &&
					(USE_MASTER != mysqlnd_ms_qos_which_server((const char *)*query, *query_len, stgy TSRMLS_CC)))
#endif
				{
					zend_llist stage1_slaves;
					zend_llist_init(&stage1_slaves, sizeof(MYSQLND_MS_LIST_DATA *), NULL /*dtor*/, 0);

					/* Stage 1 - just fire the queries and forget them for a moment */
					BEGIN_ITERATE_OVER_SERVER_LIST(element, slave_list)
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
							DBG_INF_FMT("Checking slave connection "MYSQLND_LLU_SPEC"", connection->thread_id);

							if (PASS == mysqlnd_ms_qos_server_get_lag_stage1(connection, conn_data TSRMLS_CC)) {
								zend_llist_add_element(&stage1_slaves, &element);
							} else if (connection->error_info->error_no) {
								mysqlnd_ms_client_n_php_error(NULL, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
										MYSQLND_MS_ERROR_PREFIX " SQL error while checking slave for lag: %d/'%s'",
										connection->error_info->error_no, connection->error_info->error);
							}
						}
					END_ITERATE_OVER_SERVER_LIST;
					/* Stage 2 - Now, after all servers have something to do, try to fetch the result, in the same order */
					BEGIN_ITERATE_OVER_SERVER_LIST(element, &stage1_slaves)
						long lag;
						MYSQLND_CONN_DATA * connection = element->conn;
						MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, connection);


						lag = mysqlnd_ms_qos_server_get_lag_stage2(connection, conn_data TSRMLS_CC);
						if (connection->error_info->error_no) {
							mysqlnd_ms_client_n_php_error(NULL, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
												MYSQLND_MS_ERROR_PREFIX " SQL error while checking slave for lag (%d): %d/'%s'",
												lag, connection->error_info->error_no, connection->error_info->error);
							continue;
						}

#ifdef MYSQLND_MS_HAVE_MYSQLND_QC
						if (QOS_OPTION_CACHE == filter_data->option) {
							if ((lag >= 0) && (lag < filter_data->option_data.ttl)) {
								if ((filter_data->option_data.ttl - lag) < ttl) {
									ttl = (filter_data->option_data.ttl - lag);
								}
								zend_llist_add_element(selected_slaves, &element);
							}
							continue;
						}
#endif
						/* Must be QOS_OPTION_AGE */
						if ((lag >= 0) && (lag <= filter_data->option_data.age)) {
							zend_llist_add_element(selected_slaves, &element);
						}
					END_ITERATE_OVER_SERVER_LIST;
					zend_llist_clean(&stage1_slaves);
				} else {
					BEGIN_ITERATE_OVER_SERVER_LIST(element, slave_list)
						zend_llist_add_element(selected_slaves, &element);
					END_ITERATE_OVER_SERVER_LIST;
				}
				BEGIN_ITERATE_OVER_SERVER_LIST(element, master_list)
					zend_llist_add_element(selected_masters, &element);
				END_ITERATE_OVER_SERVER_LIST;

#ifdef MYSQLND_MS_HAVE_MYSQLND_QC
				if (ttl > 0) {
					char * new_query = NULL;
					*query_len = spprintf(&new_query, 0, "/*" ENABLE_SWITCH "*//*" ENABLE_SWITCH_TTL"%u*//*" SERVER_ID_SWITCH "%s|%d|%d|%s|%s*/%s",
						ttl,
						conn->host, conn->port, (conn->charset) ? conn->charset->nr : 0, conn->user, conn->connect_or_select_db? conn->connect_or_select_db:"",
						*query);
					*query = new_query;
					*free_query = TRUE;
					DBG_INF_FMT("Cache option ttl %lu, slave list ttl %lu, %s", filter_data->option_data.ttl, ttl, *query);
				}
#endif
			}
			break;
		default:
			DBG_ERR("Invalid filter data, we should never get here");
			ret = FAIL;
			break;
	}

	DBG_RETURN(ret);
}
/* }}} */



#if PHP_VERSION_ID > 50399
/* {{{ mysqlnd_ms_remove_qos_filter */
static int
mysqlnd_ms_remove_qos_filter(void * element, void * data) {
	MYSQLND_MS_FILTER_DATA * filter = *(MYSQLND_MS_FILTER_DATA **)element;
	return (filter->pick_type == SERVER_PICK_QOS) ? 1 : 0;
}
/* }}} */


/* {{{ mysqlnd_ms_section_filters_prepend_qos */
enum_func_status
mysqlnd_ms_section_filters_prepend_qos(MYSQLND * proxy_conn,
										enum mysqlnd_ms_filter_qos_consistency consistency,
										enum mysqlnd_ms_filter_qos_option option,
										MYSQLND_MS_FILTER_QOS_OPTION_DATA * option_data TSRMLS_DC)
{
  	MYSQLND_MS_CONN_DATA ** conn_data;
	enum_func_status ret = FAIL;
	/* not sure... */
	zend_bool persistent = proxy_conn->persistent;

	DBG_ENTER("mysqlnd_ms_section_filters_prepend_qos");

	conn_data = (MYSQLND_MS_CONN_DATA **) _ms_mysqlnd_plugin_get_plugin_connection_data_data(proxy_conn->data, mysqlnd_ms_plugin_id);
	DBG_INF_FMT("conn_data=%p *conn_data=%p", conn_data, conn_data? *conn_data : NULL);

	if (conn_data && *conn_data) {
		struct mysqlnd_ms_lb_strategies * stgy = &(*conn_data)->stgy;
		zend_llist * filters = stgy->filters;
		MYSQLND_MS_FILTER_DATA * new_filter_entry = NULL;
		MYSQLND_MS_FILTER_QOS_DATA * new_qos_filter = NULL, * old_qos_filter = NULL;
		MYSQLND_MS_FILTER_DATA * filter, ** filter_pp;
		zend_llist_position	pos;

		/* search for old filter - assumptions: there no more than one QOS filter at any time */
		for (filter_pp = (MYSQLND_MS_FILTER_DATA **) zend_llist_get_first_ex(filters, &pos);
			 filter_pp && (filter = *filter_pp) && (!old_qos_filter);
			  (filter_pp = (MYSQLND_MS_FILTER_DATA **) zend_llist_get_next_ex(filters, &pos)))
		{
			if (filter->pick_type == SERVER_PICK_QOS) {
				old_qos_filter = (MYSQLND_MS_FILTER_QOS_DATA *) filter;
			}
		}

		/* new QOS filter */
		new_qos_filter = mnd_pecalloc(1, sizeof(MYSQLND_MS_FILTER_QOS_DATA), persistent);
		if (new_qos_filter) {
			new_qos_filter->parent.filter_dtor = qos_filter_dtor;
			new_qos_filter->consistency = consistency;
			new_qos_filter->option = option;

			/* preserve settings from current filter */
			if (old_qos_filter)
				new_qos_filter->option_data = old_qos_filter->option_data;

			if (QOS_OPTION_AGE == option && CONSISTENCY_EVENTUAL == consistency) {
				new_qos_filter->option_data.age = option_data->age;
			}
			if (QOS_OPTION_CACHE == option && CONSISTENCY_EVENTUAL == consistency) {
				new_qos_filter->option_data.ttl = option_data->ttl;
			}
			if (QOS_OPTION_GTID == option && CONSISTENCY_SESSION == consistency) {
				new_qos_filter->option_data.gtid_len = option_data->gtid_len;
				new_qos_filter->option_data.gtid = estrndup(option_data->gtid, option_data->gtid_len);
				efree(option_data->gtid);
			}


			new_filter_entry = (MYSQLND_MS_FILTER_DATA *)new_qos_filter;
			new_filter_entry->persistent = persistent;
			new_filter_entry->name = mnd_pestrndup(PICK_QOS, sizeof(PICK_QOS) -1, persistent);
			new_filter_entry->name_len = sizeof(PICK_QOS) -1;
			new_filter_entry->pick_type = (enum mysqlnd_ms_server_pick_strategy)SERVER_PICK_QOS;
			new_filter_entry->multi_filter = TRUE;

			/* remove all existing QOS filters */
			zend_llist_del_element(filters, NULL, mysqlnd_ms_remove_qos_filter);

			/* prepend with new filter */
			zend_llist_prepend_element(filters, &new_filter_entry);
		} else {
			MYSQLND_MS_WARN_OOM();
		}
	}

	ret = PASS;
	DBG_RETURN(ret);
}
/* }}} */
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
