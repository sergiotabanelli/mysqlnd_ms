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
#include "mysqlnd_ms.h"
#include "mysqlnd_ms_config_json.h"
#include "ext/standard/php_rand.h"

#include "mysqlnd_query_parser.h"
#include "mysqlnd_qp.h"

#include "mysqlnd_ms_enum_n_def.h"

#define MS_STRING(vl, a)				\
{											\
	MAKE_STD_ZVAL((a));						\
	ZVAL_STRING((a), (char *)(vl), 1);	\
}

#define MS_STRINGL(vl, ln, a)				\
{											\
	MAKE_STD_ZVAL((a));						\
	ZVAL_STRINGL((a), (char *)(vl), (ln), 1);	\
}

#define MS_ARRAY(a)		\
{						\
	MAKE_STD_ZVAL((a));	\
	array_init((a));	\
}


/* {{{ user_filter_dtor */
static void
user_filter_dtor(struct st_mysqlnd_ms_filter_data * pDest TSRMLS_DC)
{
	MYSQLND_MS_FILTER_USER_DATA * filter = (MYSQLND_MS_FILTER_USER_DATA *) pDest;
	DBG_ENTER("user_filter_dtor");

	if (filter->user_callback) {
		zval_ptr_dtor(&filter->user_callback);
	}
	mnd_pefree(filter, filter->parent.persistent);

	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ user_filter_conn_pool_replaced */
void user_filter_conn_pool_replaced(struct st_mysqlnd_ms_filter_data * data, zend_llist * master_connections, zend_llist * slave_connections, MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC)
{
	DBG_ENTER("user_filter_conn_pool_replaced");
	DBG_VOID_RETURN;
}

/* {{{ mysqlnd_ms_user_filter_ctor */
MYSQLND_MS_FILTER_DATA *
mysqlnd_ms_user_filter_ctor(struct st_mysqlnd_ms_config_json_entry * section, zend_llist * master_connections, zend_llist * slave_connections, MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC)
{
	MYSQLND_MS_FILTER_USER_DATA * ret = NULL;
	DBG_ENTER("mysqlnd_ms_user_filter_ctor");
	DBG_INF_FMT("section=%p", section);
	if (section) {
		ret = mnd_pecalloc(1, sizeof(MYSQLND_MS_FILTER_USER_DATA), persistent);

		if (ret) {
			zend_bool value_exists = FALSE, is_list_value = FALSE;
			char * callback;

			ret->parent.filter_dtor = user_filter_dtor;
			ret->parent.filter_conn_pool_replaced = user_filter_conn_pool_replaced;

			callback = mysqlnd_ms_config_json_string_from_section(section, SECT_USER_CALLBACK, sizeof(SECT_USER_CALLBACK) - 1, 0,
																  &value_exists, &is_list_value TSRMLS_CC);

			if (value_exists) {
				zval * zv;
				char * c_name;

				MAKE_STD_ZVAL(zv);
				ZVAL_STRING(zv, callback, 1);
				mnd_efree(callback);
				ret->user_callback = zv;
				ret->callback_valid = zend_is_callable(zv, 0, &c_name TSRMLS_CC);
				DBG_INF_FMT("name=%s valid=%d", c_name, ret->callback_valid);
				efree(c_name);
			} else {
				mnd_pefree(ret, persistent);
				php_error_docref(NULL TSRMLS_CC, E_ERROR,
									 MYSQLND_MS_ERROR_PREFIX " Error by creating filter 'user', can't find section '%s' . Stopping.", SECT_USER_CALLBACK);
			}
		} else {
			MYSQLND_MS_WARN_OOM();
		}
	}
	DBG_RETURN((MYSQLND_MS_FILTER_DATA *) ret);
}
/* }}} */


/* {{{ mysqlnd_ms_call_handler */
static zval *
mysqlnd_ms_call_handler(zval *func, int argc, zval **argv, zend_bool destroy_args, MYSQLND_ERROR_INFO * error_info TSRMLS_DC)
{
	int i;
	zval * retval;
	DBG_ENTER("mysqlnd_ms_call_handler");

	MAKE_STD_ZVAL(retval);
	if (call_user_function(EG(function_table), NULL, func, retval, argc, argv TSRMLS_CC) == FAILURE) {
		char error_buf[128];
		snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " Failed to call '%s'", Z_STRVAL_P(func));
		error_buf[sizeof(error_buf) - 1] = '\0';
		SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
		php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "%s", error_buf);
		zval_ptr_dtor(&retval);
		retval = NULL;
	}

	if (destroy_args == TRUE) {
		for (i = 0; i < argc; i++) {
			zval_ptr_dtor(&argv[i]);
		}
	}

	DBG_RETURN(retval);
}
/* }}} */


/* {{{ mysqlnd_ms_user_pick_server */
MYSQLND_CONN_DATA *
mysqlnd_ms_user_pick_server(void * f_data, const char * connect_host, const char * query, size_t query_len,
							zend_llist * master_list, zend_llist * slave_list,
							struct mysqlnd_ms_lb_strategies * stgy, MYSQLND_ERROR_INFO * error_info TSRMLS_DC)
{
	MYSQLND_MS_FILTER_USER_DATA * filter_data = (MYSQLND_MS_FILTER_USER_DATA *) f_data;
	zval * args[7];
	zval * retval;
	MYSQLND_CONN_DATA * ret = NULL;

	DBG_ENTER("mysqlnd_ms_user_pick_server");
	DBG_INF_FMT("query(50bytes)=%*s query_is_select=%p", MIN(50, query_len), query, filter_data? filter_data->user_callback:NULL);

	if (master_list && filter_data && filter_data->user_callback) {
		uint param = 0;
#ifdef ALL_SERVER_DISPATCH
		uint use_all_pos = 0;
#endif
		if (!filter_data->callback_valid) {
			char * cback_name;
			if (!zend_is_callable(filter_data->user_callback, 0, &cback_name TSRMLS_CC)) {
				char error_buf[128];
				snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " Specified callback (%s) is not a valid callback", cback_name);
				error_buf[sizeof(error_buf) - 1] = '\0';
				SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
				php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "%s", error_buf);
			} else {
				filter_data->callback_valid = TRUE;
			}
			efree(cback_name);
			if (!filter_data->callback_valid) {
				DBG_RETURN(ret);
			}
		}

		/* connect host */
		MS_STRING((char *) connect_host, args[param]);

		/* query */
		param++;
		MS_STRINGL((char *) query, query_len, args[param]);
		{
			MYSQLND_MS_LIST_DATA * el, ** el_pp;
			zend_llist_position	pos;
			/* master list */
			param++;
			MS_ARRAY(args[param]);
			for (el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(master_list, &pos); el_pp && (el = *el_pp) && el->conn;
					el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_next_ex(master_list, &pos))
			{
				if (CONN_GET_STATE(el->conn) == CONN_ALLOCED) {
					/* lazy */
					add_next_index_stringl(args[param], el->emulated_scheme, el->emulated_scheme_len, 1);
				} else {
					add_next_index_stringl(args[param], el->conn->scheme, el->conn->scheme_len, 1);
				}
			}

			/* slave list*/
			param++;
			MS_ARRAY(args[param]);
			if (slave_list) {
				for (el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(slave_list, &pos); el_pp && (el = *el_pp) && el->conn;
						el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_next_ex(slave_list, &pos))
				{
					if (CONN_GET_STATE(el->conn) == CONN_ALLOCED) {
						/* lazy */
						add_next_index_stringl(args[param], el->emulated_scheme, el->emulated_scheme_len, 1);
					} else {
						add_next_index_stringl(args[param], el->conn->scheme, el->conn->scheme_len, 1);
					}
				}
			}
			/* last used connection */
			param++;
			MAKE_STD_ZVAL(args[param]);
			if (stgy->last_used_conn && stgy->last_used_conn->scheme) {
				ZVAL_STRING(args[param], stgy->last_used_conn->scheme, 1);
			} else {
				ZVAL_NULL(args[param]);
			}

			/* in transaction */
			param++;
			MAKE_STD_ZVAL(args[param]);
			if (stgy->in_transaction) {
				ZVAL_TRUE(args[param]);
			} else {
				ZVAL_FALSE(args[param]);
			}
#ifdef ALL_SERVER_DISPATCH
			/* use all */
			use_all_pos = ++param;
			MAKE_STD_ZVAL(args[param]);
			Z_ADDREF_P(args[param]);
			ZVAL_FALSE(args[param]);
#endif
		}

		retval = mysqlnd_ms_call_handler(filter_data->user_callback, param + 1, args, FALSE /*we destroy later*/, error_info TSRMLS_CC);
		if (retval) {
			if (Z_TYPE_P(retval) == IS_STRING) {
				do {
					MYSQLND_MS_LIST_DATA * el, ** el_pp;
					zend_llist_position	pos;

					for (el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(master_list, &pos);
						 !ret && el_pp && (el = *el_pp) && el->conn;
						 el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_next_ex(master_list, &pos))
					{
						if (CONN_GET_STATE(el->conn) == CONN_ALLOCED) {
							/* lazy */
							if (!strcasecmp(el->emulated_scheme, Z_STRVAL_P(retval))) {
								MYSQLND_MS_INC_STATISTIC(MS_STAT_USE_MASTER_CALLBACK);
								DBG_INF_FMT("Userfunc chose LAZY master host : [%*s]", el->conn->scheme_len, el->conn->scheme);
								if (PASS == mysqlnd_ms_lazy_connect(el, TRUE TSRMLS_CC)) {
									ret = el->conn;
								} else {
									DBG_ERR("Connect failed, forwarding error to the user");
									ret = el->conn; /* no automatic action: leave it to the user to decide! */
								}
							}
						} else {
							if (!strcasecmp(el->conn->scheme, Z_STRVAL_P(retval))) {
								MYSQLND_MS_INC_STATISTIC(MS_STAT_USE_MASTER_CALLBACK);
								ret = el->conn;
								SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(ret));
								DBG_INF_FMT("Userfunc chose master host : [%*s]", el->conn->scheme_len, el->conn->scheme);
							}
						}
					}
					if (slave_list) {
						for (el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(slave_list, &pos);
							 !ret && el_pp && (el = *el_pp) && el->conn;
							 el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_next_ex(slave_list, &pos))
						{

							if (CONN_GET_STATE(el->conn) == CONN_ALLOCED) {
								/* lazy */
								if (!strcasecmp(el->emulated_scheme, Z_STRVAL_P(retval))) {
									MYSQLND_MS_INC_STATISTIC(MS_STAT_USE_SLAVE_CALLBACK);
									DBG_INF_FMT("Userfunc chose LAZY slave host : [%*s]", el->emulated_scheme_len, el->emulated_scheme);
									if (PASS == mysqlnd_ms_lazy_connect(el, FALSE TSRMLS_CC)) {
										ret = el->conn;
										SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(ret));
									} else {
										char error_buf[128];
										snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " Callback chose %s but connection failed", el->emulated_scheme);
										error_buf[sizeof(error_buf) - 1] = '\0';
										SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
										php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", error_buf);
										DBG_ERR_FMT("%s", error_buf);
										ret = el->conn; /* no automatic action: leave it to the user to decide! */
									}
								}
							} else {
								if (!strcasecmp(el->conn->scheme, Z_STRVAL_P(retval))) {
									MYSQLND_MS_INC_STATISTIC(MS_STAT_USE_SLAVE_CALLBACK);
									ret = el->conn;
									SET_EMPTY_ERROR(MYSQLND_MS_ERROR_INFO(ret));
									DBG_INF_FMT("Userfunc chose slave host : [%*s]", el->conn->scheme_len, el->conn->scheme);
								}
							}
						}
					}
				} while (0);
				if (!ret) {
					char error_buf[256];
					snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " User filter callback has returned an unknown server. The server '%s' can neither be found in the master list nor in the slave list", Z_STRVAL_P(retval));
					error_buf[sizeof(error_buf) - 1] = '\0';
					SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
					DBG_ERR_FMT("%s", error_buf);
					php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "%s", error_buf);
				}
			} else {
				char error_buf[256];
				snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " User filter callback has not returned string with server to use. The callback must return a string");
				error_buf[sizeof(error_buf) - 1] = '\0';
				SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
				DBG_ERR_FMT("%s", error_buf);
				php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "%s", error_buf);
			}
			zval_ptr_dtor(&retval);
		} else {
			/* We should never get here */
			char error_buf[128];
			snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " User filter callback did not return server to use");
			error_buf[sizeof(error_buf) - 1] = '\0';
			SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
			DBG_ERR_FMT("%s", error_buf);
			php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "%s", error_buf);
		}
#ifdef ALL_SERVER_DISPATCH
		convert_to_boolean(args[use_all_pos]);
		Z_DELREF_P(args[use_all_pos]);
#endif
		/* destroy the params */
		{
			unsigned int i;
			for (i = 0; i <= param; i++) {
				zval_ptr_dtor(&args[i]);
			}
		}
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ my_long_compare */
static int
my_long_compare(const void * a, const void * b TSRMLS_DC)
{
	Bucket * f = *((Bucket **) a);
	Bucket * s = *((Bucket **) b);
	zval * first = *((zval **) f->pData);
	zval * second = *((zval **) s->pData);

	if (Z_LVAL_P(first) > Z_LVAL_P(second)) {
		return 1;
	} else if (Z_LVAL_P(first) == Z_LVAL_P(second)) {
		return 0;
	}
	return -1;
}
/* }}} */


/* {{{ mysqlnd_ms_user_pick_multiple_server */
enum_func_status
mysqlnd_ms_user_pick_multiple_server(void * f_data, const char * connect_host, const char * query, size_t query_len,
									 zend_llist * master_list, zend_llist * slave_list,
									 zend_llist * selected_masters, zend_llist * selected_slaves,
									 struct mysqlnd_ms_lb_strategies * stgy, MYSQLND_ERROR_INFO * error_info
									 TSRMLS_DC)
{
	MYSQLND_MS_FILTER_USER_DATA * filter_data = (MYSQLND_MS_FILTER_USER_DATA *) f_data;
	zval * args[7];
	zval * retval;
	enum_func_status ret = FAIL;

	DBG_ENTER("mysqlnd_ms_user_pick_multiple_server");
	DBG_INF_FMT("query(50bytes)=%*s query_is_select=%p", MIN(50, query_len), query, filter_data? filter_data->user_callback:NULL);

	if (master_list && filter_data && filter_data->user_callback) {
		uint param = 0;

		/* connect host */
		MS_STRING((char *) connect_host, args[param]);

		/* query */
		param++;
		MS_STRINGL((char *) query, query_len, args[param]);
		{
			MYSQLND_MS_LIST_DATA * el, ** el_pp;
			zend_llist_position	pos;
			/* master list */
			param++;
			MS_ARRAY(args[param]);
			for (el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(master_list, &pos); el_pp && (el = *el_pp) && el->conn;
					el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_next_ex(master_list, &pos))
			{
				if (CONN_GET_STATE(el->conn) == CONN_ALLOCED) {
					/* lazy */
					add_next_index_stringl(args[param], el->emulated_scheme, el->emulated_scheme_len, 1);
				} else {
					add_next_index_stringl(args[param], el->conn->scheme, el->conn->scheme_len, 1);
				}
			}

			/* slave list*/
			param++;
			MS_ARRAY(args[param]);
			if (slave_list) {
				for (el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(slave_list, &pos); el_pp && (el = *el_pp) && el->conn;
						el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_next_ex(slave_list, &pos))
				{
					if (CONN_GET_STATE(el->conn) == CONN_ALLOCED) {
						/* lazy */
						add_next_index_stringl(args[param], el->emulated_scheme, el->emulated_scheme_len, 1);
					} else {
						add_next_index_stringl(args[param], el->conn->scheme, el->conn->scheme_len, 1);
					}
				}
			}
			/* last used connection */
			param++;
			MAKE_STD_ZVAL(args[param]);
			if (stgy->last_used_conn) {
				ZVAL_STRING(args[param], stgy->last_used_conn->scheme, 1);
			} else {
				ZVAL_NULL(args[param]);
			}

			/* in transaction */
			param++;
			MAKE_STD_ZVAL(args[param]);
			if (stgy->in_transaction) {
				ZVAL_TRUE(args[param]);
			} else {
				ZVAL_FALSE(args[param]);
			}
		}

		retval = mysqlnd_ms_call_handler(filter_data->user_callback, param + 1, args, FALSE /*we destroy later*/, error_info TSRMLS_CC);
		if (retval) {
			if (Z_TYPE_P(retval) != IS_ARRAY) {
				char error_buf[256];
				DBG_ERR("The user returned no array");
				snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " User multi filter callback has not returned a list of servers to use. The callback must return an array");
				error_buf[sizeof(error_buf) - 1] = '\0';
				SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
				DBG_ERR_FMT("%s", error_buf);
				php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "%s", error_buf);
			} else {
				do {
					HashPosition hash_pos;
					zval ** users_masters, ** users_slaves;
					DBG_INF("Checking data validity");
					/* Check data validity */
					{
						zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(retval), &hash_pos);
						if (SUCCESS != zend_hash_get_current_data_ex(Z_ARRVAL_P(retval), (void **)&users_masters, &hash_pos) ||
							Z_TYPE_PP(users_masters) != IS_ARRAY ||
							SUCCESS != zend_hash_move_forward_ex(Z_ARRVAL_P(retval), &hash_pos) ||
							SUCCESS != zend_hash_get_current_data_ex(Z_ARRVAL_P(retval), (void **)&users_slaves, &hash_pos) ||
							Z_TYPE_PP(users_slaves) != IS_ARRAY ||
							(
								0 == zend_hash_num_elements(Z_ARRVAL_PP(users_masters))
								&&
								0 == zend_hash_num_elements(Z_ARRVAL_PP(users_slaves))
							))
						{
							char error_buf[256];
							DBG_ERR("Error in validity");
							snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " User multi filter callback has returned an invalid list of servers to use. The callback must return an array");
							error_buf[sizeof(error_buf) - 1] = '\0';
							SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
							DBG_ERR_FMT("%s", error_buf);
							php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "%s", error_buf);
							break;
						}
					}
					/* convert to long and sort */
					DBG_INF("Converting and sorting");
					{
						zval ** selected_server;
						/* convert to longs and sort */
						zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(users_masters), &hash_pos);
						while (SUCCESS == zend_hash_get_current_data_ex(Z_ARRVAL_PP(users_masters), (void **)&selected_server, &hash_pos)) {
							convert_to_long_ex(selected_server);
							zend_hash_move_forward_ex(Z_ARRVAL_PP(users_masters), &hash_pos);
						}
						if (FAILURE == zend_hash_sort(Z_ARRVAL_PP(users_masters), zend_qsort, my_long_compare, 1 TSRMLS_CC)) {
							char error_buf[256];
							DBG_ERR("Error while sorting the master list");
							snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " User multi filter callback has returned an invalid list of servers to use. Error while sorting the master list");
							error_buf[sizeof(error_buf) - 1] = '\0';
							SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
							DBG_ERR_FMT("%s", error_buf);
							php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "%s", error_buf);
							break;
						}

						/* convert to longs and sort */
						zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(users_slaves), &hash_pos);
						while (SUCCESS == zend_hash_get_current_data_ex(Z_ARRVAL_PP(users_slaves), (void **)&selected_server, &hash_pos)) {
							convert_to_long_ex(selected_server);
							zend_hash_move_forward_ex(Z_ARRVAL_PP(users_slaves), &hash_pos);
						}
						if (FAILURE == zend_hash_sort(Z_ARRVAL_PP(users_slaves), zend_qsort, my_long_compare, 1 TSRMLS_CC)) {
							char error_buf[256];
							DBG_ERR("Error while sorting the slave list");
							snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " User multi filter callback has returned an invalid list of servers to use. Error while sorting the slave list");
							error_buf[sizeof(error_buf) - 1] = '\0';
							SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
							DBG_ERR_FMT("%s", error_buf);
							php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "%s", error_buf);
							break;
						}
					}
					DBG_INF("Extracting into the supplied lists");
					/* extract into llists */
					{
						unsigned int pass;
						zval ** selected_server;

						for (pass = 0; pass < 2; pass++) {
							long i = 0;
							zend_llist_position	list_pos;
							zend_llist * in_list = (pass == 0)? master_list : slave_list;
							zend_llist * out_list = (pass == 0)? selected_masters : selected_slaves;
							MYSQLND_MS_LIST_DATA ** el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(in_list, &list_pos);
							MYSQLND_MS_LIST_DATA * el = el_pp? *el_pp : NULL;
							HashTable * conn_hash = (pass == 0)? Z_ARRVAL_PP(users_masters):Z_ARRVAL_PP(users_slaves);

							DBG_INF_FMT("pass=%u", pass);
							zend_hash_internal_pointer_reset_ex(conn_hash, &hash_pos);
							while (SUCCESS == zend_hash_get_current_data_ex(conn_hash, (void **)&selected_server, &hash_pos)) {
								if (Z_LVAL_PP(selected_server) >= 0) {
									long server_id = Z_LVAL_PP(selected_server);
									DBG_INF_FMT("i=%ld server_id=%ld llist_count=%d", i, server_id, zend_llist_count(in_list));
									if (server_id >= zend_llist_count(in_list)) {
										char error_buf[256];
										snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " User multi filter callback has returned an invalid list of servers to use. Server id is too big");
										error_buf[sizeof(error_buf) - 1] = '\0';
										SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
										DBG_ERR("server_id too big, skipping and breaking");
										DBG_ERR_FMT("%s", error_buf);
										php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "%s", error_buf);
										break; /* skip impossible indices */
									}
									while (i < server_id) {
										el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_next_ex(in_list, &list_pos);
										el = el_pp ? *el_pp : NULL;
										i++;
									}
									if (el && el->conn) {
										DBG_INF_FMT("gotcha. adding server_id=%ld", server_id);
										/*
										  This will copy the whole structure, not the pointer.
										  This is wanted!!
										*/
										zend_llist_add_element(out_list, &el);
									}
								} else {
									/* either negative offset from user or LVAL casting gets us here */
									char error_buf[256];
									snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " User multi filter callback has returned an invalid list of servers to use. Server id is either negative or not a number");
									error_buf[sizeof(error_buf) - 1] = '\0';
									SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
									DBG_ERR("server_id is negative, skipping and breaking");
									DBG_ERR_FMT("%s", error_buf);
									php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "%s", error_buf);
									break;
								}
								zend_hash_move_forward_ex(conn_hash, &hash_pos);
							}
						}
						DBG_INF_FMT("count(master_list)=%d", zend_llist_count(selected_masters));
						DBG_INF_FMT("count(slave_list)=%d", zend_llist_count(selected_slaves));

						ret = PASS;
					}
				} while (0);
			}
			zval_ptr_dtor(&retval);
		} else {
			/* We should never get here */
			char error_buf[256];
			snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " User multi filter callback has not returned a list of servers to use. The callback must return an array");
			error_buf[sizeof(error_buf) - 1] = '\0';
			SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
			DBG_ERR_FMT("%s", error_buf);
			php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "%s", error_buf);
		}


		/* destroy the params */
		{
			unsigned int i;
			for (i = 0; i <= param; i++) {
				zval_ptr_dtor(&args[i]);
			}
		}
	}

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
