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

/* $Id: mysqlnd_ms.c 311179 2011-05-18 11:26:22Z andrey $ */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "ext/mysqlnd/mysqlnd.h"
#include "ext/mysqlnd/mysqlnd_debug.h"
#include "ext/mysqlnd/mysqlnd_priv.h"

//BEGIN HACK
//#include "ext/session/php_session.h"
//END HACK
#if PHP_VERSION_ID >= 50400
#include "ext/mysqlnd/mysqlnd_ext_plugin.h"
#endif
#if PHP_VERSION_ID >= 70100
#include "ext/mysqlnd/mysqlnd_connection.h"
#endif
#ifndef mnd_emalloc
#include "ext/mysqlnd/mysqlnd_alloc.h"
#endif
#ifndef mnd_sprintf
#define mnd_sprintf spprintf
#endif
#include "mysqlnd_ms.h"
#include "ext/standard/php_rand.h"

#include "mysqlnd_query_parser.h"
#include "mysqlnd_qp.h"

#include "mysqlnd_ms_enum_n_def.h"
#include "mysqlnd_ms_config_json.h"
#include "mysqlnd_ms_filter_user.h"
#include "mysqlnd_ms_filter_random.h"
#include "mysqlnd_ms_filter_round_robin.h"
#include "mysqlnd_ms_filter_table_partition.h"
#include "mysqlnd_ms_filter_qos.h"
#include "mysqlnd_ms_filter_groups.h"

#include "mysqlnd_ms_conn_pool.h"

#include "mysqlnd_ms_switch.h"

typedef MYSQLND_MS_FILTER_DATA * (*func_filter_ctor)(struct st_mysqlnd_ms_config_json_entry * section,
													zend_llist * master_connections, zend_llist * slave_connections,
													MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC);


struct st_specific_ctor_with_name
{
	const char * name;
	size_t name_len;
	func_filter_ctor ctor;
	enum mysqlnd_ms_server_pick_strategy pick_type;
	zend_bool multi_filter;
};


/* TODO: write copy ctors */
static const struct st_specific_ctor_with_name specific_ctors[] =
{
	{PICK_RROBIN,		sizeof(PICK_RROBIN) - 1,		mysqlnd_ms_rr_filter_ctor,		SERVER_PICK_RROBIN,		FALSE},
	{PICK_RANDOM,		sizeof(PICK_RANDOM) - 1,		mysqlnd_ms_random_filter_ctor,	SERVER_PICK_RANDOM,		FALSE},
	{PICK_USER,			sizeof(PICK_USER) - 1,			mysqlnd_ms_user_filter_ctor,	SERVER_PICK_USER,		FALSE},
	{PICK_USER_MULTI,	sizeof(PICK_USER_MULTI) - 1,	mysqlnd_ms_user_filter_ctor,	SERVER_PICK_USER_MULTI,	TRUE},
#ifdef MYSQLND_MS_HAVE_FILTER_TABLE_PARTITION
	{PICK_TABLE,		sizeof(PICK_TABLE) - 1,			mysqlnd_ms_table_filter_ctor,	SERVER_PICK_TABLE,		TRUE},
#endif
	{PICK_QOS,			sizeof(PICK_QOS) - 1,			mysqlnd_ms_qos_filter_ctor,		SERVER_PICK_QOS,		TRUE},
	{PICK_GROUPS,		sizeof(PICK_GROUPS) - 1,		mysqlnd_ms_groups_filter_ctor,		SERVER_PICK_GROUPS,		TRUE},
	{NULL,				0,								NULL, 							SERVER_PICK_LAST_ENUM_ENTRY, FALSE}
};


/* {{{ get_element_ptr */
static void mysqlnd_ms_get_element_ptr(void * d, void * arg TSRMLS_DC)
{
	MYSQLND_MS_LIST_DATA * data = d? *(MYSQLND_MS_LIST_DATA **) d : NULL ;
	char ptr_buf[SIZEOF_SIZE_T + 1];
	_ms_smart_type * context = (_ms_smart_type *) arg;
	DBG_ENTER("mysqlnd_ms_get_element_ptr");
	DBG_INF_FMT("ptr=%p", data->conn);
	if (data) {
#if SIZEOF_SIZE_T == 8
		int8store(ptr_buf, (size_t) data->conn);
#elif SIZEOF_SIZE_T == 4
		int4store(ptr_buf, (size_t) data->conn);
#else
#error Unknown platform
#endif
		ptr_buf[SIZEOF_SIZE_T] = '\0';
		DBG_INF_FMT("data->conn=%p ptr_buf='%s'", data->conn, ptr_buf);
		_ms_smart_method(appendl, context, ptr_buf, SIZEOF_SIZE_T);
	} else {
		DBG_INF("No data!");
	}
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_get_fingerprint */
void
mysqlnd_ms_get_fingerprint(_ms_smart_type * context, zend_llist * list TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_get_fingerprint");
	zend_llist_apply_with_argument(list, mysqlnd_ms_get_element_ptr, context TSRMLS_CC);
	_ms_smart_method(appendc, context, '\0');
	DBG_INF_FMT("context %s len=%d", context->c,context->len);
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_get_fingerprint_connection */
void
mysqlnd_ms_get_fingerprint_connection(_ms_smart_type * context, MYSQLND_MS_LIST_DATA ** d TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_get_fingerprint_connection");
	mysqlnd_ms_get_element_ptr((void *) d, (void *)context TSRMLS_CC);
	_ms_smart_method(appendc, context, '\0');
	DBG_INF_FMT("context=%s len=%d", context->c, context->len);
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_filter_list_dtor */
static void
mysqlnd_ms_filter_list_dtor(void * pDest)
{
	MYSQLND_MS_FILTER_DATA * filter = *(MYSQLND_MS_FILTER_DATA **) pDest;
	TSRMLS_FETCH();
	DBG_ENTER("mysqlnd_ms_filter_list_dtor");
	DBG_INF_FMT("%p", filter);
	if (filter) {
		zend_bool pers = filter->persistent;
		DBG_INF_FMT("name=%s dtor=%p", filter->name? filter->name:"n/a", filter->filter_dtor);
		if (filter->name) {
			mnd_pefree(filter->name, pers);
		}
		if (filter->filter_dtor) {
			filter->filter_dtor(filter TSRMLS_CC);
		} else {
			mnd_pefree(filter, pers);
		}
	}
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_lb_strategy_setup */
void
mysqlnd_ms_lb_strategy_setup(struct mysqlnd_ms_lb_strategies * strategies,
							 struct st_mysqlnd_ms_config_json_entry * the_section,
							 MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC)
{
	zend_bool value_exists = FALSE, is_list_value = FALSE;

	DBG_ENTER("mysqlnd_ms_lb_strategy_setup");
	{
		struct st_mysqlnd_ms_config_json_entry * failover_section = mysqlnd_ms_config_json_sub_section(the_section, FAILOVER_NAME, sizeof(FAILOVER_NAME) - 1,  &value_exists TSRMLS_CC);

		strategies->failover_strategy 			= DEFAULT_FAILOVER_STRATEGY;
		strategies->failover_max_retries 		= DEFAULT_FAILOVER_MAX_RETRIES;
		strategies->failover_remember_failed 	= DEFAULT_FAILOVER_REMEMBER_FAILED;

		if (value_exists) {
			int64_t failover_max_retries;
			char * remember_failed;
			/* 1.4.0 syntax failover: { strategy: ...} */
			char * failover_strategy =
				mysqlnd_ms_config_json_string_from_section(failover_section, FAILOVER_STRATEGY_NAME, sizeof(FAILOVER_STRATEGY_NAME) - 1, 0, &value_exists, &is_list_value TSRMLS_CC);

			if (!value_exists) {
				/* pre 1.4.0 syntax failover : { disabled|... } */
				failover_strategy = mysqlnd_ms_config_json_string_from_section(the_section, FAILOVER_NAME,
									sizeof(FAILOVER_NAME) - 1, 0,
									&value_exists, &is_list_value TSRMLS_CC);
			}

			if (value_exists && failover_strategy) {
				/* 1.4.0 syntax failover: { strategy: ...} */
				if (!strncasecmp(FAILOVER_STRATEGY_DISABLED, failover_strategy, sizeof(FAILOVER_STRATEGY_DISABLED) - 1)) {
					strategies->failover_strategy = SERVER_FAILOVER_DISABLED;
				} else if (!strncasecmp(FAILOVER_STRATEGY_MASTER, failover_strategy, sizeof(FAILOVER_STRATEGY_MASTER) - 1)) {
					strategies->failover_strategy = SERVER_FAILOVER_MASTER;
				} else if (!strncasecmp(FAILOVER_STRATEGY_LOOP, failover_strategy, sizeof(FAILOVER_STRATEGY_LOOP) - 1)) {
					strategies->failover_strategy = SERVER_FAILOVER_LOOP;
				}
				mnd_efree(failover_strategy);
			}

			failover_max_retries =
				mysqlnd_ms_config_json_int_from_section(failover_section, FAILOVER_MAX_RETRIES, sizeof(FAILOVER_MAX_RETRIES) - 1, 0, &value_exists, &is_list_value TSRMLS_CC);

			if (value_exists) {
				if ((failover_max_retries < 0) || (failover_max_retries > 65535)) {
					mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
					 E_RECOVERABLE_ERROR TSRMLS_CC,
					MYSQLND_MS_ERROR_PREFIX " Invalid value '%i' for " FAILOVER_MAX_RETRIES ". Stopping", failover_max_retries);
				} else {
					strategies->failover_max_retries = (uint)failover_max_retries;
				}
			}

			remember_failed = mysqlnd_ms_config_json_string_from_section(failover_section, FAILOVER_REMEMBER_FAILED, sizeof(FAILOVER_REMEMBER_FAILED) - 1, 0, &value_exists, &is_list_value TSRMLS_CC);
			if (value_exists) {
				strategies->failover_remember_failed = !mysqlnd_ms_config_json_string_is_bool_false(remember_failed);
				if (strategies->failover_remember_failed) {
					/* should we do a lazy init? */
					zend_hash_init(&strategies->failed_hosts, 8, NULL/*hash*/, NULL/*dtor*/, persistent);
				}
				mnd_efree(remember_failed);
			}
		}
	}

	{
		char * master_on_write =
			mysqlnd_ms_config_json_string_from_section(the_section, MASTER_ON_WRITE_NAME, sizeof(MASTER_ON_WRITE_NAME) - 1, 0,
													   &value_exists, &is_list_value TSRMLS_CC);

		strategies->mysqlnd_ms_flag_master_on_write = FALSE;
		strategies->master_used = FALSE;

		if (value_exists && master_on_write) {
			DBG_INF("Master on write active");
			strategies->mysqlnd_ms_flag_master_on_write = !mysqlnd_ms_config_json_string_is_bool_false(master_on_write);
			mnd_efree(master_on_write);
		}
	}

	{
		char * trx_strategy =
			mysqlnd_ms_config_json_string_from_section(the_section, TRX_STICKINESS_NAME, sizeof(TRX_STICKINESS_NAME) - 1, 0,
													   &value_exists, &is_list_value TSRMLS_CC);

		strategies->trx_stickiness_strategy = DEFAULT_TRX_STICKINESS_STRATEGY;
		strategies->trx_stop_switching = FALSE;
		strategies->trx_read_only = FALSE;
		strategies->in_transaction = FALSE;

		if (value_exists && trx_strategy) {

#if PHP_VERSION_ID >= 50399
			if (!strncasecmp(TRX_STICKINESS_MASTER, trx_strategy, sizeof(TRX_STICKINESS_MASTER) - 1)) {
				DBG_INF("trx_stickiness = master");
				strategies->trx_stickiness_strategy = TRX_STICKINESS_STRATEGY_MASTER;
#if PHP_VERSION_ID >= 50499
			} else if (!strncasecmp(TRX_STICKINESS_ON, trx_strategy, sizeof(TRX_STICKINESS_ON) - 1)) {
				DBG_INF("trx_stickiness = on");
				strategies->trx_stickiness_strategy = TRX_STICKINESS_STRATEGY_ON;
#endif
			}

#else
			SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
							 MYSQLND_MS_ERROR_PREFIX " trx_stickiness strategy is not supported before PHP 5.4.0");
			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " trx_stickiness strategy is not supported before PHP 5.4.0");
#endif
			mnd_efree(trx_strategy);
		}
	}


	{
		struct st_mysqlnd_ms_config_json_entry * trans_section = mysqlnd_ms_config_json_sub_section(the_section, TRANSIENT_ERROR_NAME,
																									sizeof(TRANSIENT_ERROR_NAME) - 1,  &value_exists TSRMLS_CC);

		strategies->transient_error_strategy 	= DEFAULT_TRANSIENT_ERROR_STRATEGY;
		strategies->transient_error_max_retries	= DEFAULT_TRANSIENT_ERROR_MAX_RETRIES;
		strategies->transient_error_usleep_before_retry = DEFAULT_TRANSIENT_ERROR_USLEEP_BEFORE_RETRY;

		if (value_exists && trans_section) {
			int64_t trans_max_retries, usleep_before_retry;
			struct st_mysqlnd_ms_config_json_entry * error_codes_section;

			strategies->transient_error_strategy = TRANSIENT_ERROR_STRATEGY_ON;
			zend_llist_init(&strategies->transient_error_codes, sizeof(uint), NULL /*dtor*/, persistent);

			trans_max_retries =
				mysqlnd_ms_config_json_int_from_section(trans_section, TRANSIENT_ERROR_MAX_RETRIES, sizeof(TRANSIENT_ERROR_MAX_RETRIES) - 1, 0, &value_exists, &is_list_value TSRMLS_CC);

			if (value_exists) {
				if ((trans_max_retries < 0) || (trans_max_retries > 65535)) {
					/* TODO: Leak? Will we exit the function immediately? */
					mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
					 E_RECOVERABLE_ERROR TSRMLS_CC,
					MYSQLND_MS_ERROR_PREFIX " Invalid value '%i' for " TRANSIENT_ERROR_MAX_RETRIES ". Stopping", trans_max_retries);
				} else {
					strategies->transient_error_max_retries = (uint)trans_max_retries;
				}
			}

			usleep_before_retry =
				mysqlnd_ms_config_json_int_from_section(trans_section, TRANSIENT_ERROR_USLEEP_RETRY, sizeof(TRANSIENT_ERROR_USLEEP_RETRY) - 1, 0, &value_exists, &is_list_value TSRMLS_CC);

			if (value_exists) {
				if ((usleep_before_retry < 0) || (usleep_before_retry > 30000000)) {
					mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
					 E_RECOVERABLE_ERROR TSRMLS_CC,
					MYSQLND_MS_ERROR_PREFIX " Invalid value '%i' for " TRANSIENT_ERROR_USLEEP_RETRY ". Stopping", usleep_before_retry);
				} else {
					strategies->transient_error_usleep_before_retry = (long)usleep_before_retry;
				}
			}

			error_codes_section = mysqlnd_ms_config_json_sub_section(trans_section, TRANSIENT_ERROR_CODES, sizeof(TRANSIENT_ERROR_CODES) - 1, &value_exists TSRMLS_CC);
			if (value_exists && error_codes_section) {
				if (TRUE == mysqlnd_ms_config_json_section_is_list(error_codes_section TSRMLS_CC)) {
					ulong nkey = 0;
					int64_t error_code;
					uint real_error_code;

					while ((error_code = mysqlnd_ms_config_json_int_from_section(error_codes_section, NULL, 0, nkey, &value_exists, &is_list_value TSRMLS_CC))
						&& value_exists) {
						if ((error_code < 0) || (error_code > 9999)) {
							/* TODO: Leak? Will we exit the function immediately? */
							mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
														  E_RECOVERABLE_ERROR TSRMLS_CC,
									 MYSQLND_MS_ERROR_PREFIX " Invalid value '%i' for entry %lu from " TRANSIENT_ERROR_CODES " list. Stopping", error_code, nkey);
						} else {
							real_error_code = (uint)error_code;
							zend_llist_add_element(&strategies->transient_error_codes, &real_error_code);
						}
						nkey++;
					}
				} else {
					mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
					 E_RECOVERABLE_ERROR TSRMLS_CC,
					MYSQLND_MS_ERROR_PREFIX " Invalid value for " TRANSIENT_ERROR_CODES ". Please, provide a list. Stopping");
				}
			}
		}
	}

	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_section_filters_add_filter */
static MYSQLND_MS_FILTER_DATA *
mysqlnd_ms_section_filters_add_filter(zend_llist * filters,
									  struct st_mysqlnd_ms_config_json_entry * filter_config,
									  const char * const filter_name, const size_t filter_name_len,
									  zend_llist * master_connections, zend_llist * slave_connections,
									  const zend_bool persistent,
									  MYSQLND_ERROR_INFO * error_info TSRMLS_DC)
{
	MYSQLND_MS_FILTER_DATA * new_filter_entry = NULL;

	DBG_ENTER("mysqlnd_ms_section_filters_add_filter");
	if (filter_name && filter_name_len) {
		unsigned int i = 0;
		/* find specific ctor, if available */
		while (specific_ctors[i].name) {
			DBG_INF_FMT("current_ctor->name=%s", specific_ctors[i].name);
			if (!strcasecmp(specific_ctors[i].name, filter_name)) {
				if (zend_llist_count(filters)) {
					zend_llist_position llist_pos;
					MYSQLND_MS_FILTER_DATA * prev = *(MYSQLND_MS_FILTER_DATA **)zend_llist_get_last_ex(filters, &llist_pos);
					if (FALSE == prev->multi_filter) {
						mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, 0 TSRMLS_CC,
													MYSQLND_MS_ERROR_PREFIX " Error while creating filter '%s' . "
								 					"Non-multi filter '%s' already created. Stopping", filter_name, prev->name);
						DBG_RETURN(NULL);
					}
				}
				if (specific_ctors[i].ctor) {
					new_filter_entry = specific_ctors[i].ctor(filter_config, master_connections, slave_connections, error_info, persistent TSRMLS_CC);
					if (!new_filter_entry) {
						mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, 0 TSRMLS_CC,
								 			MYSQLND_MS_ERROR_PREFIX " Error while creating filter '%s' . Stopping", filter_name);
						DBG_RETURN(NULL);
					}
				} else {
					new_filter_entry = mnd_pecalloc(1, sizeof(MYSQLND_MS_FILTER_DATA), persistent);
					if (!new_filter_entry) {
						MYSQLND_MS_WARN_OOM();
					}
				}
				if (new_filter_entry) {
					new_filter_entry->persistent = persistent;
					new_filter_entry->name = mnd_pestrndup(filter_name, filter_name_len, persistent);
					new_filter_entry->name_len = filter_name_len;
					new_filter_entry->pick_type = specific_ctors[i].pick_type;
					new_filter_entry->multi_filter = specific_ctors[i].multi_filter;
					zend_llist_add_element(filters, &new_filter_entry);
				}
				break;
			}
			++i;
		}
	}
	if (!new_filter_entry) {
		mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, 0 TSRMLS_CC,
									  MYSQLND_MS_ERROR_PREFIX" Unknown filter '%s' . Stopping", filter_name);
	}
	DBG_RETURN(new_filter_entry);
}
/* }}} */


/* {{{ mysqlnd_ms_load_section_filters */
zend_llist *
mysqlnd_ms_load_section_filters(struct st_mysqlnd_ms_config_json_entry * section, MYSQLND_ERROR_INFO * error_info,
								zend_llist * master_connections, zend_llist * slave_connections,
								zend_bool persistent TSRMLS_DC)
{
	zend_llist * ret = NULL;
	DBG_ENTER("mysqlnd_ms_load_section_filters");

	if (section) {
		ret = mnd_pemalloc(sizeof(zend_llist), persistent);
		if (!ret) {
			MYSQLND_MS_WARN_OOM();
		}
	}
	if (ret) {
		zend_bool section_exists;
		struct st_mysqlnd_ms_config_json_entry * filters_section =
				mysqlnd_ms_config_json_sub_section(section, SECT_FILTER_NAME, sizeof(SECT_FILTER_NAME) - 1, &section_exists TSRMLS_CC);

		zend_llist_init(ret, sizeof(MYSQLND_MS_FILTER_DATA *), (llist_dtor_func_t) mysqlnd_ms_filter_list_dtor /*dtor*/, persistent);
		DBG_INF_FMT("normal filters section =%d", section_exists && filters_section);
		switch (section_exists && filters_section? 1:0) {
			case 1:
				do {
					char * filter_name = NULL;
					size_t filter_name_len = 0;
					_ms_ulong filter_int_name;
					struct st_mysqlnd_ms_config_json_entry * current_filter =
							mysqlnd_ms_config_json_next_sub_section(filters_section, &filter_name, &filter_name_len, &filter_int_name TSRMLS_CC);

					if (!current_filter || !filter_name || !filter_name_len) {
						if (current_filter) {
							char error_buf[128];
							if (filter_name && !filter_name_len) {
								snprintf(error_buf, sizeof(error_buf), "Error loading filters. Filter with empty name found");
							} else if (FALSE == mysqlnd_ms_config_json_section_is_list(current_filter TSRMLS_CC)) { /* filter_int_name */
								filter_name =
									mysqlnd_ms_config_json_string_from_section(filters_section, NULL, 0,
																			   filter_int_name, NULL, NULL TSRMLS_CC);
								filter_name_len = strlen(filter_name);
								DBG_INF_FMT("%d was found as key, the value is %s", filter_int_name, filter_name);
								if (NULL == mysqlnd_ms_section_filters_add_filter(ret, current_filter, filter_name, filter_name_len,
																				master_connections, slave_connections,
																				persistent, error_info TSRMLS_CC)) {
									mnd_pefree(filter_name, 0);
									goto err;
								}
								mnd_pefree(filter_name, 0);
								continue;
							} else {
								snprintf(error_buf, sizeof(error_buf), "Unknown filter '%d' . Stopping", filter_int_name);
							}
							error_buf[sizeof(error_buf) - 1] = '\0';
							mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, 0 TSRMLS_CC,
								 						  MYSQLND_MS_ERROR_PREFIX " %s", error_buf);
							goto err;
						}
						DBG_INF("no next sub-section");
						break;
					}
					if (NULL == mysqlnd_ms_section_filters_add_filter(ret, current_filter, filter_name, filter_name_len,
																	master_connections, slave_connections,
																	persistent, error_info TSRMLS_CC)) {
						goto err;
					}
				} while (1);
				if (zend_llist_count(ret)) {
					zend_llist_position llist_pos;
					MYSQLND_MS_FILTER_DATA * prev = *(MYSQLND_MS_FILTER_DATA **)zend_llist_get_last_ex(ret, &llist_pos);
					if (FALSE != prev->multi_filter) {
						mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
												MYSQLND_MS_ERROR_PREFIX " Error in configuration. Last filter is multi filter. "
												"Needs to be non-multi one. Stopping");
						goto err;
					}
					break;
				}
				/* fall-through */
			case 0:
			{
				/* setup the default */
				unsigned int i = 0;
				DBG_INF("No section, using defaults");
				while (specific_ctors[i].name) {
					if (DEFAULT_PICK_STRATEGY == specific_ctors[i].pick_type) {
						DBG_INF_FMT("Found default pick strategy : %s", specific_ctors[i].name);
						if (NULL == mysqlnd_ms_section_filters_add_filter(ret, NULL, specific_ctors[i].name, specific_ctors[i].name_len,
																		master_connections, slave_connections,
																		persistent, error_info TSRMLS_CC))
						{
							mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
										MYSQLND_MS_ERROR_PREFIX " Can't load default filter '%d' . Stopping", specific_ctors[i].name);
							goto err;
						}
						break;
					}
					++i;
				}
			}
		}
	}
	DBG_RETURN(ret);
err:
	zend_llist_clean(ret);
	mnd_pefree(ret, persistent);
	DBG_RETURN(NULL);
}
/* }}} */

// BEGIN HACK

/* {{{ mysqlnd_ms_query_is_injectable_query */
zend_bool
mysqlnd_ms_query_is_injectable_query(const char * query, size_t query_len, zend_bool *forced TSRMLS_DC)
{
	struct st_ms_token_and_value token = {0};
	struct st_mysqlnd_query_scanner * scanner;
	const char * inject_on = INI_STR("mysqlnd_ms.inject_on") ? INI_STR("mysqlnd_ms.inject_on") : INI_STR("mysqlnd_ms.master_on");
	zend_bool ret = FALSE;
	DBG_ENTER("mysqlnd_ms_query_is_injectable_query");
	*forced = FALSE;
	if (!query) {
		DBG_RETURN(ret);
	}
	scanner = mysqlnd_qp_create_scanner(TSRMLS_C);
	mysqlnd_qp_set_string(scanner, query, query_len TSRMLS_CC);
	token = mysqlnd_qp_get_token(scanner TSRMLS_CC);
	DBG_INF_FMT("token=COMMENT? = %d, %s", token.token == QC_TOKEN_COMMENT, Z_STRVAL(token.value));
	while (token.token == QC_TOKEN_COMMENT) {
		size_t comment_len = Z_STRLEN(token.value);
		char * comment = Z_STRVAL(token.value);

		while (*comment && isspace(*comment)) {
			++comment;
			--comment_len;
		}
		if ((comment_len >= sizeof(NOINJECT_SWITCH)) && (comment[sizeof(NOINJECT_SWITCH)] == '\0' || isspace(comment[sizeof(NOINJECT_SWITCH)])) &&
				!strncasecmp(comment, NOINJECT_SWITCH, sizeof(NOINJECT_SWITCH) - 1))
		{
			DBG_INF("forced noinject");
			ret = FALSE;
			*forced = TRUE;
		} else if ((comment_len >= sizeof(INJECT_SWITCH)) && (comment[sizeof(INJECT_SWITCH)] == '\0' || isspace(comment[sizeof(INJECT_SWITCH)])) &&
						!strncasecmp(comment, INJECT_SWITCH, sizeof(INJECT_SWITCH) - 1))
		{
			DBG_INF("forced inject");
			ret = TRUE;
			*forced = TRUE;
		}
		zval_dtor(&token.value);
		token = mysqlnd_qp_get_token(scanner TSRMLS_CC);
	}
	if (!(*forced)) {
		if (!inject_on || !strlen(inject_on) || !Z_STRVAL(token.value)) {
			ret = (token.token == QC_TOKEN_SELECT) ? FALSE : TRUE;
		} else {
			char * master_tok;
			char * tok;
			DBG_INF_FMT("inject_on %s", inject_on);
			master_tok = strdup(inject_on);
			tok = strtok(master_tok, ",");
			while( tok != NULL && strcasecmp(tok, Z_STRVAL(token.value))) {
				tok = strtok(NULL, ",");
			}
			if (tok) {
				DBG_INF_FMT("injectable query token %s", tok);
				ret = TRUE;
			} else {
				ret = FALSE;
			}
			free(master_tok);
		}
	}
	zval_dtor(&token.value);
	mysqlnd_qp_free_scanner(scanner TSRMLS_CC);
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_query_which_qos */
enum mysqlnd_ms_filter_qos_consistency
mysqlnd_ms_query_which_qos(const char * query, size_t query_len, zend_bool * forced TSRMLS_DC)
{
	enum mysqlnd_ms_filter_qos_consistency ret = CONSISTENCY_EVENTUAL;
	struct st_ms_token_and_value token = {0};
	struct st_mysqlnd_query_scanner * scanner;
	DBG_ENTER("mysqlnd_ms_query_which_qos");

	*forced = FALSE;
	if (!query) {
		DBG_RETURN(ret);
	}

	scanner = mysqlnd_qp_create_scanner(TSRMLS_C);
	mysqlnd_qp_set_string(scanner, query, query_len TSRMLS_CC);
	token = mysqlnd_qp_get_token(scanner TSRMLS_CC);
	DBG_INF_FMT("token=COMMENT? = %d, %s", token.token == QC_TOKEN_COMMENT, Z_STRVAL(token.value));
	while (token.token == QC_TOKEN_COMMENT) {
		size_t comment_len = Z_STRLEN(token.value);
		char * comment = Z_STRVAL(token.value);

		while (*comment && isspace(*comment)) {
			++comment;
			--comment_len;
		}
		if ((comment_len >= sizeof(STRONG_SWITCH)) && (comment[sizeof(STRONG_SWITCH)] == '\0' || isspace(comment[sizeof(STRONG_SWITCH)])) &&
				!strncasecmp(comment, STRONG_SWITCH, sizeof(STRONG_SWITCH) - 1))
		{
			DBG_INF("forced strong");
			ret = CONSISTENCY_STRONG;
			*forced = TRUE;
		} else if ((comment_len >= sizeof(SESSION_SWITCH)) && (comment[sizeof(SESSION_SWITCH)] == '\0' || isspace(comment[sizeof(SESSION_SWITCH)])) &&
						!strncasecmp(comment, SESSION_SWITCH, sizeof(SESSION_SWITCH) - 1))
		{
			DBG_INF("forced session");
			ret = CONSISTENCY_SESSION;
			*forced = TRUE;
		} else if ((comment_len >= sizeof(EVENTUAL_SWITCH)) && (comment[sizeof(EVENTUAL_SWITCH)] == '\0' || isspace(comment[sizeof(EVENTUAL_SWITCH)])) &&
						!strncasecmp(comment, EVENTUAL_SWITCH, sizeof(EVENTUAL_SWITCH) - 1))
		{
			DBG_INF("forced last used");
			ret = CONSISTENCY_EVENTUAL;
			*forced = TRUE;
		}

		zval_dtor(&token.value);
		token = mysqlnd_qp_get_token(scanner TSRMLS_CC);
	}
	zval_dtor(&token.value);
	mysqlnd_qp_free_scanner(scanner TSRMLS_CC);
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_get_php_session */
int
mysqlnd_ms_get_php_session(zval * ret TSRMLS_DC) {
//	zval *params = { to_zval, from_zval, msg_zval };
	_ms_uint param_count = 0;
	zval function_name;
	DBG_ENTER("mysqlnd_ms_get_php_session");
	_MS_ZVAL_STRING(&function_name, "session_id");

	if (call_user_function(
	        CG(function_table), NULL /* no object */, &function_name,
			ret, param_count, NULL TSRMLS_CC
	    ) != SUCCESS || Z_TYPE_P(ret) != IS_STRING) {
		DBG_INF("Failed call to session_id function");
		zval_dtor(&function_name);
//zval_dtor(ret);
		DBG_RETURN(FAILURE);
	}

	/* don't forget to free the zvals */
	zval_dtor(&function_name);
	DBG_RETURN(SUCCESS);
}
/* }}} */

/* {{{ mysqlnd_ms_get_php_svar */
enum_func_status
mysqlnd_ms_get_php_svar(const char * name,  zval _ms_p_zval **val TSRMLS_DC) {
#if PHP_MAJOR_VERSION < 7
	DBG_ENTER("mysqlnd_ms_get_php_svar");
	if (php_get_session_var(name, strlen(name), val TSRMLS_CC) != SUCCESS || Z_TYPE_P(_ms_p_zval *val) != IS_STRING) {
#else
	zend_string * zn = zend_string_init(name, strlen(name), 0);
	DBG_ENTER("mysqlnd_ms_get_php_svar");
	*val = php_get_session_var(zn);
	zend_string_free(zn);
	if ((*val) == NULL || Z_TYPE_P(*val) != IS_STRING) {
#endif

		DBG_RETURN(FAIL);
	}
	DBG_RETURN(PASS);
}
/* }}} */


/* {{{ mysqlnd_ms_str_replace */
char *
mysqlnd_ms_str_replace(const char *orig, const char *rep, const char *with, zend_bool persistent TSRMLS_DC) {
    char * result; // the return string
    const char * ins;    // the next insert point
    char * tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements
	DBG_ENTER("mysqlnd_ms_str_replace");

    // sanity checks and initialization
    if (!orig && !rep)
        DBG_RETURN(NULL);
    len_rep = strlen(rep);
    if (len_rep == 0)
    	DBG_RETURN(NULL); // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = mnd_pemalloc(strlen(orig) + ((len_with - len_rep) * count) + 1, persistent);

    if (!result)
    	DBG_RETURN(NULL);

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    DBG_RETURN(result);
}
/* }}} */

// END HACK

/* {{{ mysqlnd_ms_query_is_select */
PHP_MYSQLND_MS_API enum enum_which_server
mysqlnd_ms_query_is_select(const char * query, size_t query_len, zend_bool * forced TSRMLS_DC)
{
	enum enum_which_server ret = USE_MASTER;
	struct st_ms_token_and_value token = {0};
	struct st_mysqlnd_query_scanner * scanner;
	// BEGIN HACK
	const char * master_on = INI_STR("mysqlnd_ms.master_on");
	// END HACK
	DBG_ENTER("mysqlnd_ms_query_is_select");

	*forced = FALSE;
	if (!query) {
		DBG_RETURN(USE_MASTER);
	}

	DBG_INF_FMT("Query(len)  %s(%d)", query, query_len);
	scanner = mysqlnd_qp_create_scanner(TSRMLS_C);
	mysqlnd_qp_set_string(scanner, query, query_len TSRMLS_CC);
	token = mysqlnd_qp_get_token(scanner TSRMLS_CC);
	DBG_INF_FMT("token=COMMENT? = %d", token.token == QC_TOKEN_COMMENT);
	while (token.token == QC_TOKEN_COMMENT) {
		size_t comment_len = Z_STRLEN(token.value);
		char * comment = Z_STRVAL(token.value);

		while (*comment && isspace(*comment)) {
			++comment;
			--comment_len;
		}
		if ((comment_len >= sizeof(MASTER_SWITCH)) && (comment[sizeof(MASTER_SWITCH)] == '\0' || isspace(comment[sizeof(MASTER_SWITCH)])) &&
				!strncasecmp(comment, MASTER_SWITCH, sizeof(MASTER_SWITCH) - 1))
		{
			DBG_INF("forced master");
			ret = USE_MASTER;
			*forced = TRUE;
			MYSQLND_MS_INC_STATISTIC(MS_STAT_USE_MASTER_FORCED);
		} else if ((comment_len >= sizeof(SLAVE_SWITCH)) && (comment[sizeof(SLAVE_SWITCH)] == '\0' || isspace(comment[sizeof(SLAVE_SWITCH)])) &&
						!strncasecmp(comment, SLAVE_SWITCH, sizeof(SLAVE_SWITCH) - 1))
		{
			DBG_INF("forced slave");
			if (MYSQLND_MS_G(disable_rw_split)) {
				ret = USE_MASTER;
			} else {
				ret = USE_SLAVE;
				MYSQLND_MS_INC_STATISTIC(MS_STAT_USE_SLAVE_FORCED);
			}
			*forced = TRUE;
		} else if ((comment_len >= sizeof(LAST_USED_SWITCH)) && (comment[sizeof(LAST_USED_SWITCH)] == '\0' || isspace(comment[sizeof(LAST_USED_SWITCH)])) &&
						!strncasecmp(comment, LAST_USED_SWITCH, sizeof(LAST_USED_SWITCH) - 1))
		{
			DBG_INF("forced last used");
			ret = USE_LAST_USED;
			*forced = TRUE;
			MYSQLND_MS_INC_STATISTIC(MS_STAT_USE_LAST_USED_FORCED);
#ifdef ALL_SERVER_DISPATCH
		} else if ((comment_len >= sizeof(ALL_SERVER_SWITCH)) && (comment[sizeof(ALL_SERVER_SWITCH)] == '\0' || isspace(comment[sizeof(ALL_SERVER_SWITCH)])) &&
						!strncasecmp(comment, ALL_SERVER_SWITCH, sizeof(ALL_SERVER_SWITCH) - 1))
		{
			DBG_INF("forced all server");
			ret = USE_ALL;
			*forced = TRUE;
#endif
		}

		zval_dtor(&token.value);
		token = mysqlnd_qp_get_token(scanner TSRMLS_CC);
	}
	if (*forced == FALSE) {
	  	if (MYSQLND_MS_G(disable_rw_split)) {
			ret = USE_MASTER;
		} else if (token.token == QC_TOKEN_SELECT) {
			MYSQLND_MS_INC_STATISTIC(MS_STAT_USE_SLAVE_GUESS);
			ret = USE_SLAVE;
#ifdef ALL_SERVER_DISPATCH
		} else if (token.token == QC_TOKEN_SET) {
			ret = USE_ALL;
#endif
// BEGIN HACK
		} else if (master_on && strlen(master_on) && Z_STRVAL(token.value)) {
			char * master_tok;
			char * tok;
			DBG_INF_FMT("master_on %s", master_on);
			master_tok = strdup(master_on);
			tok = strtok(master_tok, ",");
			while( tok != NULL && strcasecmp(tok, Z_STRVAL(token.value))) {
				tok = strtok(NULL, ",");
			}
			if (tok) {
				MYSQLND_MS_INC_STATISTIC(MS_STAT_USE_MASTER_GUESS);
				DBG_INF("master_on master");
				ret = USE_MASTER;
			} else {
				MYSQLND_MS_INC_STATISTIC(MS_STAT_USE_SLAVE_GUESS);
				DBG_INF("master_on slave");
				ret = USE_SLAVE;
			}
			free(master_tok);
// END HACK
		} else {
			MYSQLND_MS_INC_STATISTIC(MS_STAT_USE_MASTER_GUESS);
			ret = USE_MASTER;
		}
	}
	DBG_INF_FMT("Query is %u", ret);
	zval_dtor(&token.value);
	mysqlnd_qp_free_scanner(scanner TSRMLS_CC);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_pick_first_master_or_slave */
MYSQLND_CONN_DATA *
mysqlnd_ms_pick_first_master_or_slave(const MYSQLND_CONN_DATA * const conn TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	zend_llist * master_list = (*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC);
	zend_llist * slave_list = (*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC);
	struct mysqlnd_ms_lb_strategies * stgy = &(*conn_data)->stgy;
	MYSQLND_MS_LIST_DATA * el;
	DBG_ENTER("mysqlnd_ms_pick_first_master_or_slave");

	BEGIN_ITERATE_OVER_SERVER_LIST(el, master_list);
		if (_MS_CONN_GET_STATE(el->conn) == CONN_ALLOCED && PASS == mysqlnd_ms_lazy_connect(el, FALSE TSRMLS_CC)) {
			MYSQLND_MS_INC_STATISTIC(MS_STAT_USE_MASTER);
			SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(el->conn));
			/* Real Success !! */
			DBG_RETURN(stgy->last_used_conn = el->conn);
		}
	END_ITERATE_OVER_SERVER_LIST;
	BEGIN_ITERATE_OVER_SERVER_LIST(el, slave_list);
		if (_MS_CONN_GET_STATE(el->conn) == CONN_ALLOCED && PASS == mysqlnd_ms_lazy_connect(el, FALSE TSRMLS_CC)) {
			MYSQLND_MS_INC_STATISTIC(MS_STAT_USE_SLAVE);
			SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(el->conn));
			/* Real Success !! */
			DBG_RETURN(stgy->last_used_conn = el->conn);
		}
	END_ITERATE_OVER_SERVER_LIST;
	DBG_RETURN(NULL);
}
/* }}} */


/* {{{ mysqlnd_ms_select_servers_all */
enum_func_status
mysqlnd_ms_select_servers_all(zend_llist * master_list, zend_llist * slave_list,
							  zend_llist * selected_masters, zend_llist * selected_slaves TSRMLS_DC)
{
	const MYSQLND_MS_LIST_DATA * el;
	DBG_ENTER("mysqlnd_ms_select_servers_all");

	BEGIN_ITERATE_OVER_SERVER_LIST(el, master_list);
		zend_llist_add_element(selected_masters, &el);
	END_ITERATE_OVER_SERVER_LIST;
	BEGIN_ITERATE_OVER_SERVER_LIST(el, slave_list);
		zend_llist_add_element(selected_slaves, &el);
	END_ITERATE_OVER_SERVER_LIST;

	DBG_RETURN(PASS);
}
/* }}} */

/* {{{ mysqlnd_ms_pick_server_ex */
MYSQLND_CONN_DATA *
mysqlnd_ms_pick_server_ex(MYSQLND_CONN_DATA * conn, char ** query, size_t * query_len, zend_bool * free_query, zend_bool * switched_servers TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
	MYSQLND_CONN_DATA * connection = conn;
	DBG_ENTER("mysqlnd_ms_pick_server_ex");
	DBG_INF_FMT("conn_data=%p *conn_data=%p", conn_data, conn_data? *conn_data : NULL);
	*switched_servers = FALSE;

	if (conn_data && *conn_data) {
		zend_bool allow_master_for_slave = FALSE;
		struct mysqlnd_ms_lb_strategies * stgy = &(*conn_data)->stgy;
		zend_llist * filters = stgy->filters;
		zend_llist * master_list = (*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC);
		zend_llist * slave_list = (*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC);
		zend_llist * selected_masters = NULL, * selected_slaves = NULL;
		zend_llist * output_masters = NULL, * output_slaves = NULL;
		MYSQLND_MS_FILTER_DATA * filter, ** filter_pp;
		zend_bool qos = FALSE;
		zend_llist_position	pos;
		*free_query = FALSE;

		/* order of allocation and initialisation is important !*/
		/* 1. */
		selected_masters = mnd_pemalloc(sizeof(zend_llist), conn->persistent);
		if (!selected_masters) {
			MYSQLND_MS_WARN_OOM();
			goto end;
		}
		zend_llist_init(selected_masters, sizeof(MYSQLND_MS_LIST_DATA *), NULL /*dtor*/, conn->persistent);

		/* 2. */
		selected_slaves = mnd_pemalloc(sizeof(zend_llist), conn->persistent);
		if (!selected_slaves) {
			MYSQLND_MS_WARN_OOM();
			goto end;
		}
		zend_llist_init(selected_slaves, sizeof(MYSQLND_MS_LIST_DATA *), NULL /*dtor*/, conn->persistent);

		/* 3. */
		output_masters = mnd_pemalloc(sizeof(zend_llist), conn->persistent);
		if (!output_masters) {
			MYSQLND_MS_WARN_OOM();
			goto end;
		}
		zend_llist_init(output_masters, sizeof(MYSQLND_MS_LIST_DATA *), NULL /*dtor*/, conn->persistent);

		/* 4. */
		output_slaves = mnd_pemalloc(sizeof(zend_llist), conn->persistent);
		if (!output_slaves) {
			MYSQLND_MS_WARN_OOM();
			goto end;
		}
		zend_llist_init(output_slaves, sizeof(MYSQLND_MS_LIST_DATA *), NULL /*dtor*/, conn->persistent);

		mysqlnd_ms_select_servers_all(master_list, slave_list, selected_masters, selected_slaves TSRMLS_CC);
		connection = NULL;

		for (filter_pp = (MYSQLND_MS_FILTER_DATA **) zend_llist_get_first_ex(filters, &pos);
			 filter_pp && (filter = *filter_pp);
			 filter_pp = (MYSQLND_MS_FILTER_DATA **) zend_llist_get_next_ex(filters, &pos))
		{
			zend_bool trx_continue_filtering = FALSE;
			zend_bool multi_filter = FALSE;
			zend_bool multi_filter_single_conn_continue_search = TRUE;
			if (zend_llist_count(output_masters) || zend_llist_count(output_slaves)) {
				/* swap and clean */
				zend_llist * tmp_sel_masters = selected_masters;
				zend_llist * tmp_sel_slaves = selected_slaves;
				zend_llist_clean(selected_masters);
				zend_llist_clean(selected_slaves);
				selected_masters = output_masters;
				selected_slaves = output_slaves;
				output_masters = tmp_sel_masters;
				output_slaves = tmp_sel_slaves;
			}

			switch (filter->pick_type) {
				case SERVER_PICK_USER:
					connection = mysqlnd_ms_user_pick_server(filter, (*conn_data)->connect_host, (const char * const)*query, *query_len,
															 selected_masters, selected_slaves, stgy,
															 &MYSQLND_MS_ERROR_INFO(conn) TSRMLS_CC);
					break;
				case SERVER_PICK_USER_MULTI:
					multi_filter = TRUE;
					/* TODO:
					 Should we really stop filtering if a user multi filter returns only one server?
					 User might have returned zero masters for upcoming SELECT.
					*/
					multi_filter_single_conn_continue_search = FALSE;
					mysqlnd_ms_user_pick_multiple_server(filter, (*conn_data)->connect_host, (const char * const)*query, *query_len,
														 selected_masters, selected_slaves,
														 output_masters, output_slaves, stgy,
														 &MYSQLND_MS_ERROR_INFO(conn) TSRMLS_CC);
					break;
#ifdef MYSQLND_MS_HAVE_FILTER_TABLE_PARTITION
				case SERVER_PICK_TABLE:
					if (FALSE == stgy->trx_stop_switching) {
						multi_filter = TRUE;
						multi_filter_single_conn_continue_search = TRUE;
						mysqlnd_ms_choose_connection_table_filter(filter, (const char * const)*query, *query_len,
															  _MS_CONN_GET_STATE(conn) > CONN_ALLOCED?
															  	conn->connect_or_select_db:
															  	(*conn_data)->cred.db,
															  selected_masters, selected_slaves, output_masters, output_slaves,
															  stgy, &MYSQLND_MS_ERROR_INFO(conn) TSRMLS_CC);
					} else {
						trx_continue_filtering = TRUE;
					}
					break;
#endif
				case SERVER_PICK_RANDOM:
					connection = mysqlnd_ms_choose_connection_random(filter, (const char * const)*query, *query_len, stgy, &MYSQLND_MS_ERROR_INFO(conn),
																	 selected_masters, selected_slaves, NULL, allow_master_for_slave TSRMLS_CC);
					break;
				case SERVER_PICK_RROBIN:
					connection = mysqlnd_ms_choose_connection_rr(filter, (const char * const)*query, *query_len, stgy, &MYSQLND_MS_ERROR_INFO(conn),
																 selected_masters, selected_slaves, NULL, allow_master_for_slave TSRMLS_CC);
					break;
				case SERVER_PICK_QOS:
					/* TODO: MS must not bail if slave or master list is empty, mostly handled in 1.5 */
					if (FALSE == stgy->trx_stop_switching) {
						multi_filter = TRUE;
						multi_filter_single_conn_continue_search = TRUE;
						allow_master_for_slave = TRUE;
						mysqlnd_ms_choose_connection_qos(conn, filter, (*conn_data)->connect_host, query, query_len,
													 free_query,
													 selected_masters, selected_slaves,
													 output_masters, output_slaves, stgy,
													 &MYSQLND_MS_ERROR_INFO(conn) TSRMLS_CC);
						qos = TRUE;
					} else {
						trx_continue_filtering = TRUE;
					}
					break;
				case SERVER_PICK_GROUPS:
					/* TODO: MS must not bail if slave or master list is empty */
					if (FALSE == stgy->trx_stop_switching) {
						multi_filter = TRUE;
						multi_filter_single_conn_continue_search = TRUE;
						mysqlnd_ms_choose_connection_groups(conn, filter, (*conn_data)->connect_host, query, query_len,
													 selected_masters, selected_slaves,
													 output_masters, output_slaves, stgy,
													 &MYSQLND_MS_ERROR_INFO(conn) TSRMLS_CC);
					}
					break;
				default:
					mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
												  MYSQLND_MS_ERROR_PREFIX " Unknown pick type");
			}
			DBG_INF_FMT("out_masters_count=%d  out_slaves_count=%d", zend_llist_count(output_masters), zend_llist_count(output_slaves));
			if (trx_continue_filtering) {
				continue;
			}
			/* if a multi-connection filter reduces the list to a single connection, then use this connection */
			if (!connection &&
				multi_filter == TRUE &&
				(1 == zend_llist_count(output_masters) + zend_llist_count(output_slaves)) &&
				(FALSE == multi_filter_single_conn_continue_search))
			{
				MYSQLND_MS_LIST_DATA ** el_pp;
				if (zend_llist_count(output_masters)) {
					el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first(output_masters);
				} else {
					el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first(output_slaves);
				}
				if (el_pp && (*el_pp)->conn) {
					MYSQLND_MS_LIST_DATA * element = *el_pp;
					if (_MS_CONN_GET_STATE(element->conn) == CONN_ALLOCED) {
						DBG_INF("Lazy connection, trying to connect...");
						/* lazy connection, connect now */

						if (PASS != mysqlnd_ms_lazy_connect(element, zend_llist_count(output_masters)? TRUE:FALSE TSRMLS_CC)) {
							DBG_INF_FMT("Using master connection "MYSQLND_LLU_SPEC"", element->conn->thread_id);
							connection = element->conn;
						}
					} else {
						connection = element->conn;
					}
				}
			}
			if (connection && (multi_filter == FALSE) && (qos == TRUE) && (*conn_data)->global_trx.m->gtid_validate) {
				connection = (*conn_data)->global_trx.m->gtid_validate(connection);
			}
			if (!connection && (multi_filter == FALSE)) {
				mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
											  MYSQLND_MS_ERROR_PREFIX " No connection selected by the last filter");
				stgy->last_used_conn = conn;
				goto end;
			}
			if (!connection && (0 == zend_llist_count(output_masters) && 0 == zend_llist_count(output_slaves))) {
				/* filtered everything out */
				if ((SERVER_FAILOVER_MASTER == stgy->failover_strategy) || (SERVER_FAILOVER_LOOP == stgy->failover_strategy)) {
					DBG_INF("FAILOVER");
					connection = conn;
				} else {
					mysqlnd_ms_client_n_php_error(&MYSQLND_MS_ERROR_INFO(conn), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
									MYSQLND_MS_ERROR_PREFIX " Couldn't find the appropriate master connection. Something is wrong");
					stgy->last_used_conn = conn;
					goto end;
				}
			}
			if (connection) {
				/* filter till we get one, for now this is just a hack, not the ultimate solution */
				break;
			}
		}
		*switched_servers = (conn == connection) ? FALSE : TRUE;
		stgy->last_used_conn = connection;
end:
		if (selected_masters) {
			zend_llist_clean(selected_masters);
			mnd_pefree(selected_masters, conn->persistent);
		}
		if (selected_slaves) {
			zend_llist_clean(selected_slaves);
			mnd_pefree(selected_slaves, conn->persistent);
		}
		if (output_masters) {
			zend_llist_clean(output_masters);
			mnd_pefree(output_masters, conn->persistent);
		}
		if (output_slaves) {
			zend_llist_clean(output_slaves);
			mnd_pefree(output_slaves, conn->persistent);
		}
#if 0
		if (!connection) {
			connection = conn;
		}
#endif
	}

	DBG_RETURN(connection);
}
/* }}} */


#ifdef ALL_SERVER_DISPATCH
/* {{{ mysqlnd_ms_query_all */
static enum_func_status
mysqlnd_ms_query_all(MYSQLND * const proxy_conn, const char * query, unsigned int query_len,
					 zend_llist * master_connections, zend_llist * slave_connections TSRMLS_DC)
{
	MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, proxy_conn);
	enum_func_status ret = PASS;

	DBG_ENTER("mysqlnd_ms_query_all");
	if (!conn_data || !*conn_data) {
		DBG_RETURN(MS_CALL_ORIGINAL_CONN_DATA_METHOD(query)(proxy_conn, query, query_len TSRMLS_CC));
	} else {
		zend_llist * lists[] = {NULL, &(*conn_data)->master_connections, &(*conn_data)->slave_connections, NULL};
		zend_llist ** list = lists;
		while (*++list) {
			zend_llist_position	pos;
			MYSQLND_MS_LIST_DATA * el, ** el_pp;
			/* search the list of easy handles hanging off the multi-handle */
			for (el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(*list, &pos); el_pp && (el = *el_pp) && el->conn;
					el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_next_ex(*list, &pos))
			{
				if (PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(query)(el->conn, query, query_len TSRMLS_CC)) {
					ret = FAIL;
				}
			}
		}
	}

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
