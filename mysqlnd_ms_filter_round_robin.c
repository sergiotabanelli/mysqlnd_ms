/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2012 The PHP Group                                |
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
#include "mysqlnd_ms.h"
#include "mysqlnd_ms_switch.h"
#include "mysqlnd_ms_enum_n_def.h"
#include "mysqlnd_ms_lb_weights.h"
#include "mysqlnd_ms_config_json.h"

/* {{{ mysqlnd_ms_filter_rr_context_dtor */
static void
mysqlnd_ms_filter_rr_context_dtor(_ms_hash_zval_type * data)
{
	MYSQLND_MS_FILTER_RR_CONTEXT * context = data? _ms_p_zval (MYSQLND_MS_FILTER_RR_CONTEXT _ms_p_zval *) _MS_HASH_Z_PTR_P(data) : NULL;
	zend_bool persistent = context->weight_list.persistent;
	zend_llist_clean(&context->weight_list);
	if (context) {
		mnd_pefree(context, persistent);
	}
}
/* }}} */


/* {{{ rr_filter_dtor */
static void
rr_filter_dtor(struct st_mysqlnd_ms_filter_data * pDest TSRMLS_DC)
{
	MYSQLND_MS_FILTER_RR_DATA * filter = (MYSQLND_MS_FILTER_RR_DATA *) pDest;
	DBG_ENTER("rr_filter_dtor");

	zend_hash_destroy(&filter->master_context);
	zend_hash_destroy(&filter->slave_context);
	zend_hash_destroy(&filter->lb_weight);
	mnd_pefree(filter, filter->parent.persistent);

	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_filter_rr_reset_current_weight */
static void
mysqlnd_ms_filter_rr_reset_current_weight(void * data TSRMLS_DC)
{
	MYSQLND_MS_FILTER_LB_WEIGHT_IN_CONTEXT * context = *(MYSQLND_MS_FILTER_LB_WEIGHT_IN_CONTEXT **) data;
	context->lb_weight->current_weight = context->lb_weight->weight;
}
/* }}} */

/* {{{ rr_filter_conn_pool_replaced */
void rr_filter_conn_pool_replaced(struct st_mysqlnd_ms_filter_data * f_data, zend_llist * master_connections, zend_llist * slave_connections, MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC)
{
	MYSQLND_MS_FILTER_RR_DATA * filter = (MYSQLND_MS_FILTER_RR_DATA *) f_data;

	DBG_ENTER("rr_filter_conn_pool_replaced");
	/* Must be Fabric: weights? */
	if (zend_hash_num_elements(&filter->lb_weight)) {
		mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
									E_WARNING TSRMLS_CC,
									MYSQLND_MS_ERROR_PREFIX " Replacing the connection pool at runtime is not supported by the '%s' filter: server weights can't be used.", PICK_RROBIN);
	} else {
		DBG_INF("Resetting context");
		zend_hash_clean(&filter->master_context);
		zend_hash_clean(&filter->slave_context);
	}

	DBG_VOID_RETURN;
}

/* {{{ mysqlnd_ms_rr_filter_ctor */
MYSQLND_MS_FILTER_DATA *
mysqlnd_ms_rr_filter_ctor(struct st_mysqlnd_ms_config_json_entry * section, zend_llist * master_connections, zend_llist * slave_connections, MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC)
{
	MYSQLND_MS_FILTER_RR_DATA * ret;
	DBG_ENTER("mysqlnd_ms_rr_filter_ctor");
	DBG_INF_FMT("section=%p", section);
	/* section could be NULL! */
	ret = mnd_pecalloc(1, sizeof(MYSQLND_MS_FILTER_RR_DATA), persistent);
	if (ret) {

		ret->parent.filter_dtor = rr_filter_dtor;
		ret->parent.filter_conn_pool_replaced = rr_filter_conn_pool_replaced;

		zend_hash_init(&ret->master_context, 4, NULL/*hash*/, mysqlnd_ms_filter_rr_context_dtor, persistent);
		zend_hash_init(&ret->slave_context, 4, NULL/*hash*/, mysqlnd_ms_filter_rr_context_dtor, persistent);
		zend_hash_init(&ret->lb_weight, 4, NULL/*hash*/, mysqlnd_ms_filter_lb_weigth_dtor, persistent);

		/* roundrobin => array(weights  => array(name => w, ... )) */
		if (section &&
			(TRUE == mysqlnd_ms_config_json_section_is_list(section TSRMLS_CC) &&
			TRUE == mysqlnd_ms_config_json_section_is_object_list(section TSRMLS_CC)))
		{
			struct st_mysqlnd_ms_config_json_entry * subsection = NULL;
			do {
				char * current_subsection_name = NULL;
				size_t current_subsection_name_len = 0;

				subsection = mysqlnd_ms_config_json_next_sub_section(section,
																	&current_subsection_name,
																	&current_subsection_name_len,
																	NULL TSRMLS_CC);
				if (!subsection) {
					break;
				}
				DBG_INF_FMT("subsection(%u)=[%s]", (unsigned int) current_subsection_name_len, current_subsection_name);
				if (!strcmp(current_subsection_name, SECT_LB_WEIGHTS)) {
					mysqlnd_ms_filter_ctor_load_weights_config(&ret->lb_weight, PICK_RROBIN, subsection, master_connections,  slave_connections, error_info, persistent TSRMLS_CC);
					break;
				}
			} while (1);
		}
	} else {
		MYSQLND_MS_WARN_OOM();
	}
	DBG_RETURN((MYSQLND_MS_FILTER_DATA *) ret);
}
/* }}} */


/* {{{ mysqlnd_ms_choose_connection_rr_fetch_context */
static MYSQLND_MS_FILTER_RR_CONTEXT *
mysqlnd_ms_choose_connection_rr_fetch_context(HashTable * rr_contexts, zend_llist * connections, HashTable * lb_weights_list TSRMLS_DC)
{
	MYSQLND_MS_FILTER_RR_CONTEXT _ms_p_zval * ret_context = NULL;
	_ms_smart_type fprint = {0};

	DBG_ENTER("mysqlnd_ms_choose_connection_rr_fetch_context");

	mysqlnd_ms_get_fingerprint(&fprint, connections TSRMLS_CC);
	if (SUCCESS != _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, rr_contexts, fprint.c, fprint.len - 1 /*\0 counted*/, ret_context)) {
		MYSQLND_MS_FILTER_RR_CONTEXT * context = mnd_pemalloc(sizeof(MYSQLND_MS_FILTER_RR_CONTEXT), _MS_HASH_PERSISTENT(rr_contexts));
		int retval;
		DBG_INF("Init the master context");
		memset(context, 0, sizeof(MYSQLND_MS_FILTER_RR_CONTEXT));
		context->pos = 0;
		mysqlnd_ms_weight_list_init(&context->weight_list, _MS_HASH_PERSISTENT(rr_contexts) TSRMLS_CC);

		retval = _MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, rr_contexts, fprint.c, fprint.len - 1 /*\0 counted*/, context);

		if (SUCCESS == retval) {
			/* fetch ptr to the data inside the HT */
			retval = _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, rr_contexts, fprint.c, fprint.len - 1 /*\0 counted*/, ret_context);
		}
		DBG_INF_FMT("Add context retval %d num elements %d", retval, zend_hash_num_elements(rr_contexts));
		_ms_smart_method(free, &fprint);
		if (SUCCESS != retval) {
			DBG_RETURN(NULL);
		}

		if (zend_hash_num_elements(lb_weights_list)) {
			/* sort list for weighted load balancing */
			if (PASS != mysqlnd_ms_populate_weights_sort_list(lb_weights_list, &(_ms_p_zval ret_context)->weight_list, connections TSRMLS_CC)) {
				DBG_RETURN(NULL);
			}
			DBG_INF_FMT("Sort list has %d elements", zend_llist_count(&(_ms_p_zval ret_context)->weight_list));
		}
	} else {
		_ms_smart_method(free, &fprint);
	}
	DBG_INF_FMT("context=%p", _ms_p_zval ret_context);
	DBG_RETURN(_ms_p_zval ret_context);
}
/* }}} */


/*
Round 0, sort list
slave 1, current_weight 3 -->  pick, current_weight--
slave 2, current_weight 2
slave 3, current_weight 1

Round 1, sort list
slave 1, current_weight 2 --> pick, current_weight--
slave 2, current_weight 2
slave 3, current_weight 1

Round 2, sort list
slave 2, current_weight 2 --> pick, current_weight--
slave 1, current_weight 1
slave 3, current_weight 1
  NOTE: slave 1, slave 3 ordering is undefined/implementation dependent!

Round 3, sort list
slave 2, current_weight 1 --> pick, current_weight--, reset
slave 1, current_weight 1
slave 3, current_weight 1

Round 4, sort list
slave 1, current_weight 1 --> pick, current_weight--
slave 3, current_weight 1
slave 2, current_weight 0

Round 5, sort list
slave 3, current_weight 1 --> pick, current_weight--
slave 2, current_weight 0
slave 1, current_weight 0

Round 6, sort list
slave 3, current_weight 0 --> RESET -> 1 --> sort again
slave 2, current_weight 0           -> 2
slave 1, current_weight 0           -> 3
*/
/* {{{ mysqlnd_ms_rr_weight_list_get_next */
static MYSQLND_MS_LIST_DATA *
mysqlnd_ms_rr_weight_list_get_next(zend_llist * wl TSRMLS_DC)
{
	MYSQLND_MS_LIST_DATA * element = NULL;

	DBG_ENTER("mysqlnd_ms_rr_weight_list_get_next");
	DBG_INF("Sorting");

	do {
		MYSQLND_MS_FILTER_LB_WEIGHT_IN_CONTEXT * lb_weight_context, ** lb_weight_context_pp;
		zend_llist_position	tmp_pos;

		mysqlnd_ms_weight_list_sort(wl TSRMLS_CC);
		lb_weight_context_pp = (MYSQLND_MS_FILTER_LB_WEIGHT_IN_CONTEXT **)zend_llist_get_first_ex(wl, &tmp_pos);

		if (lb_weight_context_pp && (lb_weight_context = *lb_weight_context_pp)) {
			element = lb_weight_context->element;
			DBG_INF_FMT("element %p current_weight %d", element, lb_weight_context->lb_weight->current_weight);
			if (0 == lb_weight_context->lb_weight->current_weight) {
				/* RESET */
				zend_llist_apply(wl, mysqlnd_ms_filter_rr_reset_current_weight TSRMLS_CC);
				continue;
			}
			lb_weight_context->lb_weight->current_weight--;
		} else {
			DBG_INF("Sorting failed");
		}
	} while (0);
	DBG_RETURN(element);
}
/* }}} */


/* {{{ mysqlnd_ms_choose_connection_rr_use_slave */
static MYSQLND_CONN_DATA *
mysqlnd_ms_choose_connection_rr_use_slave(zend_llist * master_connections,
										  zend_llist * slave_connections,
										  MYSQLND_MS_FILTER_RR_DATA * filter,
										  struct mysqlnd_ms_lb_strategies * stgy,
										  enum enum_which_server * which_server,
										  MYSQLND_ERROR_INFO * error_info TSRMLS_DC)
{
	unsigned int * pos;
	MYSQLND_CONN_DATA * connection = NULL;
	zend_llist * l = slave_connections;
	MYSQLND_MS_FILTER_RR_CONTEXT * context = NULL;
	unsigned int retry_count = 0;

	DBG_ENTER("mysqlnd_ms_choose_connection_rr_use_slave");
	*which_server = USE_SLAVE;

	if (0 == zend_llist_count(l)) {
		DBG_INF("Slave list is empty");
		MS_WARN_AND_RETURN_IF_TRX_FORBIDS_FAILOVER(stgy, NULL);
		if ((SERVER_FAILOVER_MASTER == stgy->failover_strategy) || (SERVER_FAILOVER_LOOP == stgy->failover_strategy)) {
			*which_server = USE_MASTER;
			DBG_RETURN(connection);
		}
		/* failover must be disabled */
		mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX
										  " Couldn't find the appropriate slave connection. %d slaves to choose from. "
										  "Something is wrong", zend_llist_count(l));
		DBG_RETURN(connection);
	}

	context = mysqlnd_ms_choose_connection_rr_fetch_context(&filter->slave_context, l, &filter->lb_weight TSRMLS_CC);
	if (context) {
		pos = &(context->pos);
	} else {
		mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
									  MYSQLND_MS_ERROR_PREFIX " Couldn't create or fetch context. Something is quite wrong");
		DBG_RETURN(NULL);
	}
	DBG_INF_FMT("look under pos %u", *pos);
	do {
		MYSQLND_MS_LIST_DATA * element = NULL;

		retry_count++;

		if (zend_llist_count(&context->weight_list)) {
			element = mysqlnd_ms_rr_weight_list_get_next(&context->weight_list TSRMLS_CC);
		} else	{
			unsigned int i = 0;
			BEGIN_ITERATE_OVER_SERVER_LIST(element, l);
				if (i++ == *pos) {
					break;
				}
			END_ITERATE_OVER_SERVER_LIST;
			DBG_INF_FMT("i=%u pos=%u", i, *pos);
		}
		if (!element) {
			/* there is no such safe guard in the random filter. Random tests for connection */
			if ((SERVER_FAILOVER_LOOP == stgy->failover_strategy) &&
				((0 == stgy->failover_max_retries) || (retry_count <= stgy->failover_max_retries)))
			{
				MS_WARN_AND_RETURN_IF_TRX_FORBIDS_FAILOVER(stgy, NULL);
				/* time to increment the position */
				*pos = ((*pos) + 1) % zend_llist_count(l);
				DBG_INF("Trying next slave, if any");
				DBG_INF_FMT("pos is now %u", *pos);
				continue;
			}
			/* unlikely */
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX
										  " Couldn't find the appropriate slave connection. %d slaves to choose from. "
										  "Something is wrong", zend_llist_count(l));
			DBG_RETURN(NULL);
		}
		/* time to increment the position */
		*pos = ((*pos) + 1) % zend_llist_count(l);
		DBG_INF_FMT("pos is now %u", *pos);
		if (element->conn) {
			connection = element->conn;
		}
		if (connection) {
			_ms_smart_type fprint_conn = {0};
			DBG_INF_FMT("Using slave connection "MYSQLND_LLU_SPEC"", connection->thread_id);

			/* Check if this connection has already been marked failed */
			if (stgy->failover_remember_failed) {
				zend_bool _ms_p_zval * failed;
				mysqlnd_ms_get_fingerprint_connection(&fprint_conn, &element TSRMLS_CC);
				if (SUCCESS == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, &stgy->failed_hosts, fprint_conn.c, fprint_conn.len - 1 /*\0 counted*/, failed)) {
					_ms_smart_method(free, &fprint_conn);
					DBG_INF("Skipping previously failed connection");
					continue;
				}
			}

			if (_MS_CONN_GET_STATE(connection) > CONN_ALLOCED || PASS == mysqlnd_ms_lazy_connect(element, FALSE TSRMLS_CC)) {
				MYSQLND_MS_INC_STATISTIC(MS_STAT_USE_SLAVE);
				SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(connection));
				if (fprint_conn.c) {
					_ms_smart_method(free, &fprint_conn);
				}
				/* Real Success !! */
				DBG_RETURN(connection);
			}

			/* Not nice, bad connection, mark it if the user wants it */
			if (stgy->failover_remember_failed) {
				zend_bool *failed = &stgy->failover_remember_failed;
				if (SUCCESS != _MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, &stgy->failed_hosts, fprint_conn.c, fprint_conn.len - 1/*\0 counted*/, failed)) {
					DBG_INF_FMT("Failed to remember failing connection %s", fprint_conn.c);
				}
			}
			if (fprint_conn.c) {
				_ms_smart_method(free, &fprint_conn);
			}
		}
		/* if we are here, we had some kind of a problem, either !connection or establishment failed */
		MS_WARN_AND_RETURN_IF_TRX_FORBIDS_FAILOVER(stgy, NULL);
		if ((SERVER_FAILOVER_LOOP == stgy->failover_strategy) &&
			((0 == stgy->failover_max_retries) || (retry_count <= stgy->failover_max_retries))) {
			DBG_INF("Trying next slave, if any");
			continue;
		} else if (SERVER_FAILOVER_DISABLED == stgy->failover_strategy) {
			DBG_INF("Failover disabled");
			DBG_RETURN(connection);
		}
		DBG_INF("Falling back to the master");
		break;
	} while (retry_count < zend_llist_count(l));

	/*
	   We should never get here if trx disallows switching.
	   If no slaves, we have a test prior to the loop. If no connection, we have tests in the loop.
	*/
	MS_WARN_AND_RETURN_IF_TRX_FORBIDS_FAILOVER(stgy, NULL);

	if ((SERVER_FAILOVER_LOOP == stgy->failover_strategy) && (0 == zend_llist_count(master_connections))) {
		/* must not fall through as we'll loose the connection error */
		DBG_INF("No masters to continue search");
		DBG_RETURN(connection);
	}
	if (SERVER_FAILOVER_DISABLED == stgy->failover_strategy) {
		/*
		We may get here with remember_failed but no failover strategy set.
		TODO: Is this a valid configuration at all?
		*/
		DBG_INF("Failover disabled");
		DBG_RETURN(connection);
	}
	*which_server = USE_MASTER;
	DBG_RETURN(NULL);
}
/* }}} */


/* {{{ mysqlnd_ms_choose_connection_rr_use_master */
static MYSQLND_CONN_DATA *
mysqlnd_ms_choose_connection_rr_use_master(zend_llist * master_connections,
										   MYSQLND_MS_FILTER_RR_DATA * filter,
										   struct mysqlnd_ms_lb_strategies * stgy,
										   zend_bool forced_tx_master,
										   MYSQLND_ERROR_INFO * error_info TSRMLS_DC)
{
	zend_llist * l = master_connections;
	unsigned int * pos;
	MYSQLND_MS_LIST_DATA * element = NULL;
	MYSQLND_CONN_DATA * connection = NULL;
	MYSQLND_MS_FILTER_RR_CONTEXT * context;
	unsigned int retry_count = 0;

	DBG_ENTER("mysqlnd_ms_choose_connection_rr_use_master");
	if (0 == zend_llist_count(l)) {
		mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
									  MYSQLND_MS_ERROR_PREFIX
									  " Couldn't find the appropriate master connection. %d masters to choose from. "
									  "Something is wrong", zend_llist_count(l));
		DBG_RETURN(NULL);
	}

	context = mysqlnd_ms_choose_connection_rr_fetch_context(&filter->master_context, l, &filter->lb_weight TSRMLS_CC);
	if (context) {
		pos = &(context->pos);
	} else {
		mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
									  MYSQLND_MS_ERROR_PREFIX " Couldn't create or fetch context. Something is quite wrong");
		DBG_RETURN(NULL);
	}

	while (retry_count++ < zend_llist_count(l)) {
		if (zend_llist_count(&context->weight_list)) {
			element = mysqlnd_ms_rr_weight_list_get_next(&context->weight_list TSRMLS_CC);
		} else {
			unsigned int i = 0;
			BEGIN_ITERATE_OVER_SERVER_LIST(element, l);
				if (i++ == *pos) {
					break; /* stop iterating */
				}
			END_ITERATE_OVER_SERVER_LIST;
			DBG_INF_FMT("USE_MASTER pos=%lu", *pos);
		}

		if (!element) {
			if ((SERVER_FAILOVER_LOOP == stgy->failover_strategy) &&
				((0 == stgy->failover_max_retries) || (retry_count <= stgy->failover_max_retries)))
			{
				MS_WARN_AND_RETURN_IF_TRX_FORBIDS_FAILOVER(stgy, NULL);
				/* we must move to the next position and ignore forced_tx_master */
				*pos = ((*pos) + 1) % zend_llist_count(l);
				DBG_INF("Trying next master, if any");
				continue;
			}
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
										  MYSQLND_MS_ERROR_PREFIX
										  " Couldn't find the appropriate master connection. %d masters to choose from. "
										  "Something is wrong", zend_llist_count(l));
			DBG_RETURN(NULL);
		}
		connection = NULL;
		if (element->conn) {
			connection = element->conn;
		}
		DBG_INF("Using master connection");
		/* time to increment the position */
		*pos = ((*pos) + 1) % zend_llist_count(l);

		if (connection) {
			_ms_smart_type fprint_conn = {0};

			if (stgy->failover_remember_failed) {
				mysqlnd_ms_get_fingerprint_connection(&fprint_conn, &element TSRMLS_CC);
				zend_bool _ms_p_zval * failed;
				mysqlnd_ms_get_fingerprint_connection(&fprint_conn, &element TSRMLS_CC);
				if (SUCCESS == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, &stgy->failed_hosts, fprint_conn.c, fprint_conn.len - 1 /*\0 counted*/, failed)) {
					_ms_smart_method(free, &fprint_conn);
					DBG_INF("Skipping previously failed connection");
					continue;
				}
			}
			if ((_MS_CONN_GET_STATE(connection) > CONN_ALLOCED || PASS == mysqlnd_ms_lazy_connect(element, TRUE TSRMLS_CC))) {
				MYSQLND_MS_INC_STATISTIC(MS_STAT_USE_MASTER);
				SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(connection));
				if (fprint_conn.c) {
					_ms_smart_method(free, &fprint_conn);
				}
				DBG_RETURN(connection);
			}

			if (stgy->failover_remember_failed) {
				zend_bool *failed = &stgy->failover_remember_failed;
				if (SUCCESS != _MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, &stgy->failed_hosts, fprint_conn.c, fprint_conn.len - 1 /*\0 counted*/, failed)) {
					DBG_INF("Failed to remember failing connection");
				}
			}
			if (fprint_conn.c) {
				_ms_smart_method(free, &fprint_conn);
			}
		}
		if ((SERVER_FAILOVER_LOOP == stgy->failover_strategy) &&
			((0 == stgy->failover_max_retries) || (retry_count <= stgy->failover_max_retries))) {
			MS_WARN_AND_RETURN_IF_TRX_FORBIDS_FAILOVER(stgy, NULL);
			DBG_INF("Trying next master, if any");
			continue;
		} else if (SERVER_FAILOVER_DISABLED == stgy->failover_strategy) {
			DBG_INF("Failover disabled");
		}
		break;
	}
	DBG_RETURN(connection);
}
/* }}} */


/* {{{ mysqlnd_ms_choose_connection_rr */
MYSQLND_CONN_DATA *
mysqlnd_ms_choose_connection_rr(void * f_data, const char * const query, const size_t query_len,
								struct mysqlnd_ms_lb_strategies * stgy, MYSQLND_ERROR_INFO * error_info,
								zend_llist * master_connections, zend_llist * slave_connections,
								zend_bool allow_master_for_slave TSRMLS_DC)
{
	unsigned int forced = stgy->forced;
	enum enum_which_server which_server = stgy->which_server;
	MYSQLND_MS_FILTER_RR_DATA * filter = (MYSQLND_MS_FILTER_RR_DATA *) f_data;
	zend_bool forced_tx_master = FALSE;
	MYSQLND_CONN_DATA * conn = NULL;
	DBG_ENTER("mysqlnd_ms_choose_connection_rr");

	DBG_INF_FMT("trx_stickiness_strategy=%d in_transaction=%d trx_stop_switching=%d", stgy->trx_stickiness_strategy,  stgy->in_transaction, stgy->trx_stop_switching);


	if (allow_master_for_slave && (USE_SLAVE == which_server) && (0 == zend_llist_count(slave_connections))) {
		/*
		In versions prior to 1.5 the QoS filter could end the filter chain if it had
		sieved out all connections but one. This is no longer allowed to ensure QoS
		cannot overrule trx stickiness.  trx stickiness is mostly handled by
		random/roundrobin filter invoked after QoS. Thus, random/roundrobin
		may be called with an empty slave list to pick a connection for a SELECT.
		If so, we implicitly switch to master list.
		*/
		which_server = USE_MASTER;
	}

	if ((stgy->trx_stickiness_strategy == TRX_STICKINESS_STRATEGY_MASTER) && stgy->in_transaction) {
		DBG_INF("Enforcing use of master while in transaction");
		if (stgy->trx_stop_switching) {
			/* in the middle of a transaction */
			which_server = USE_LAST_USED;
		} else {
			/* first statement run in transaction: disable switch and failover */
			which_server = USE_MASTER;
		}
		forced_tx_master = TRUE;
		MYSQLND_MS_INC_STATISTIC(MS_STAT_TRX_MASTER_FORCED);
	} else if ((stgy->trx_stickiness_strategy == TRX_STICKINESS_STRATEGY_ON) && stgy->in_transaction) {
		if (stgy->trx_stop_switching) {
			DBG_INF("Use last in middle of transaction");
			/* in the middle of a transaction */
			which_server = USE_LAST_USED;
		} else {
			/* first statement run in transaction: disable switch and failover */
			if (FALSE == stgy->trx_read_only) {
				DBG_INF("Enforcing use of master while in transaction");
				forced_tx_master = TRUE;
				which_server = USE_MASTER;
			} else {
				if (0 == zend_llist_count(slave_connections)) {
					DBG_INF("No slaves to run read only transaction, using master");
					forced_tx_master = TRUE;
					which_server = USE_MASTER;
				} else {
					DBG_INF("Considering use of slave while in read only transaction");
					which_server = USE_SLAVE;
				}
			}
		}
	}

	if (stgy->mysqlnd_ms_flag_master_on_write) {
		if (which_server != USE_MASTER) {
			if (stgy->master_used && (forced & TYPE_NODE_SWITCH) == 0) {
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
			conn = mysqlnd_ms_choose_connection_rr_use_slave(master_connections, slave_connections, filter, stgy, &which_server, error_info TSRMLS_CC);
			/*
			conn == NULL && allow_master_for_slave is true if QoS filter has sieved out all available slaves.
			*/
			if ((NULL != conn || USE_MASTER != which_server) &&  !(NULL == conn && TRUE == allow_master_for_slave)) {
				goto return_connection;
			}
			if ((TRUE == stgy->in_transaction) && (TRUE == stgy->trx_stop_switching)) {
				/*
				If our list of slaves may is short and it contains of nothing but previously failed
				slaves, then we should allow failover when searching for an server to run a transaction.
				Thus, we set stgy->trx_stop_switching very late in this function.
				*/
				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
								MYSQLND_MS_ERROR_PREFIX " Automatic failover is not permitted in the middle of a transaction");
				DBG_INF("In transaction, no switch allowed");
				conn = NULL;
				goto return_connection;
			}
			DBG_INF("Fall-through to master");
			/* fall-through */
		case USE_MASTER:
			conn = mysqlnd_ms_choose_connection_rr_use_master(master_connections, filter, stgy, forced_tx_master, error_info TSRMLS_CC);
			break;
		case USE_LAST_USED:
			DBG_INF("Using last used connection");
			if (!stgy->last_used_conn) {
				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
											  MYSQLND_MS_ERROR_PREFIX
											  " Last used SQL hint cannot be used because last used connection has not been set yet. "
											  "Statement will fail");
			} else {
				SET_EMPTY_ERROR(_ms_a_ei MYSQLND_MS_ERROR_INFO(stgy->last_used_conn));
			}
			conn = stgy->last_used_conn;
			break;
		default:
			/* error */
			conn = NULL;
			break;
	}

return_connection:
	if (stgy->in_transaction && (stgy->trx_stickiness_strategy != TRX_STICKINESS_STRATEGY_DISABLED)) {
		// BEGIN HACK
		if (stgy->trx_stop_switching == FALSE) {
			MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);
			if (conn_data && *conn_data && (*conn_data)->global_trx.type != GTID_NONE && (*conn_data)->global_trx.is_master) {
				/* If we were switching and now we stop this should mean autocommit off, begin or implicit commit */
				DBG_INF("Delayed gtid INJECT");
				if (FAIL == MYSQLND_MS_GTID_CALL_PASS((*conn_data)->global_trx.m->gtid_inject_before, conn TSRMLS_CC)) {
					DBG_INF("Failed delayed gtid INJECT");
					mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
															MYSQLND_MS_ERROR_PREFIX " Failed delayed gtid inject after choosing a server");
					MYSQLND_MS_INC_STATISTIC(MS_STAT_GTID_IMPLICIT_COMMIT_FAILURE);
				}
			}
		}
		// END HACK
		/*
		 Initial server for running trx has been identified. No matter
		 whether we found a valid connection or not, we stop switching
		 servers until the transaction has ended. No kind of failover allowed
		 when in a transaction.
		*/
		stgy->trx_stop_switching = TRUE;
	}

#if MYSQLND_VERSION_ID >= 50011
	if ((conn) && (stgy->trx_stickiness_strategy != TRX_STICKINESS_STRATEGY_DISABLED) &&
//BEGIN HACK
		//(TRUE == stgy->in_transaction) && (TRUE == stgy->trx_begin_required) && !forced) {
		// Why !forced ???????
		(TRUE == stgy->in_transaction) && (TRUE == stgy->trx_begin_required)) {
//END HACK
		/* See mysqlnd_ms.c tx_begin notes! */
		enum_func_status ret = FAIL;
		MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, conn);

		if (conn_data && *conn_data) {
			/* Send BEGIN now that we have decided on a connection for the transaction */
			DBG_INF_FMT("Delayed BEGIN mode=%d name='%s'", stgy->trx_begin_mode, stgy->trx_begin_name);

			(*conn_data)->skip_ms_calls = TRUE;
			/* TODO: flags */
			ret = MS_CALL_ORIGINAL_CONN_DATA_METHOD(tx_begin)(conn, stgy->trx_begin_mode, stgy->trx_begin_name TSRMLS_CC);
			(*conn_data)->skip_ms_calls = FALSE;

			stgy->trx_begin_required = FALSE;
			stgy->trx_begin_mode = 0;
			if (stgy->trx_begin_name) {
				mnd_pefree(stgy->trx_begin_name, conn->persistent);
				stgy->trx_begin_name = NULL;
			}

			if (FAIL == ret) {
				/* back to the beginning: reset everything */
				stgy->in_transaction = FALSE;
				stgy->trx_stop_switching = FALSE;
				stgy->trx_read_only = FALSE;

				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,
														MYSQLND_MS_ERROR_PREFIX " Failed to start transaction after choosing a server");
			}
		}
	}
#endif

	DBG_RETURN(conn);
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
