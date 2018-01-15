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
  | Author: Ulf Wendel <uw@php.net>                                      |
  |         Andrey Hristov <andrey@php.net>                              |
  |         Johannes Schlueter <johannes@php.net>                        |
  +----------------------------------------------------------------------+
*/

/* $Id: mysqlnd_ms.c $ */
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
#include "mysqlnd_ms.h"
#include "mysqlnd_ms_switch.h"
#include "mysqlnd_ms_enum_n_def.h"
#include "mysqlnd_ms_config_json.h"


/* {{{ mysqlnd_ms_filter_lb_weigth_dtor */
void
mysqlnd_ms_filter_lb_weigth_dtor(_ms_hash_zval_type * pDest)
{
	MYSQLND_MS_FILTER_LB_WEIGHT * element = pDest? _ms_p_zval (MYSQLND_MS_FILTER_LB_WEIGHT _ms_p_zval *) _MS_HASH_Z_PTR_P(pDest) : NULL;
	TSRMLS_FETCH();
	DBG_ENTER("mysqlnd_ms_filter_lb_weigth_dtor");
	if (element) {
		mnd_pefree(element, element->persistent);
	}
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_filter_lb_weigth_in_ctx_dtor */
static void
mysqlnd_ms_filter_lb_weigth_in_ctx_dtor(void * pDest)
{
	MYSQLND_MS_FILTER_LB_WEIGHT_IN_CONTEXT * element = pDest? *(MYSQLND_MS_FILTER_LB_WEIGHT_IN_CONTEXT **) pDest : NULL;
	TSRMLS_FETCH();
	DBG_ENTER("mysqlnd_ms_filter_lb_weigth_in_ctx_dtor");
	if (element) {
		mnd_pefree(element, element->lb_weight->persistent);
	}
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_filter_ctor_load_weights_config */
void
mysqlnd_ms_filter_ctor_load_weights_config(HashTable * lb_weights_list, const char * filter_name, struct st_mysqlnd_ms_config_json_entry * section, zend_llist * master_connections, zend_llist * slave_connections, MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC)
{
	zend_bool value_exists = FALSE, is_list_value = FALSE;
	struct st_mysqlnd_ms_config_json_entry * subsection = NULL;
	HashTable server_names;
	MYSQLND_MS_LIST_DATA * entry, **entry_pp;
	zend_llist_position	pos;
	DBG_ENTER("mysqlnd_ms_filter_ctor_load_weights_config");

	/* Build server hash table */
	zend_hash_init(&server_names, 4, NULL/*hash*/, NULL/*dtor*/, FALSE);

	for (entry_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(master_connections, &pos);
		entry_pp && (entry = *entry_pp) && (entry->name_from_config) && (entry->conn);
		entry_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_next_ex(master_connections, &pos)) {

		if (SUCCESS != _MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, &server_names, entry->name_from_config, strlen(entry->name_from_config), *entry_pp)) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
							E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Failed to setup master server list for '%s' filter. Stopping", filter_name);
		}
	}

	for (entry_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(slave_connections, &pos);
		entry_pp && (entry = *entry_pp) && (entry->name_from_config) && (entry->conn);
		entry_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_next_ex(slave_connections, &pos)) {

		if (SUCCESS != _MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, &server_names, entry->name_from_config, strlen(entry->name_from_config), *entry_pp)) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
							E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Failed to setup slave server list for '%s' filter. Stopping", filter_name);
		}
	}

	do {
		char * current_subsection_name = NULL;
		size_t current_subsection_name_len = 0;
		int weight;
		_ms_smart_type fprint_conn = {0};
		MYSQLND_MS_LIST_DATA _ms_p_zval * hentry_pp = NULL;

		subsection = mysqlnd_ms_config_json_next_sub_section(section,
															&current_subsection_name,
															&current_subsection_name_len,
															NULL TSRMLS_CC);
		if (!subsection) {
			break;
		}

		if (SUCCESS != _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, &server_names, current_subsection_name, current_subsection_name_len, hentry_pp)) {
			mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
									E_RECOVERABLE_ERROR TSRMLS_CC,
									MYSQLND_MS_ERROR_PREFIX " Unknown server '%s' in '%s' filter configuration. Stopping",
									current_subsection_name, filter_name);
			continue;
		}
		weight = mysqlnd_ms_config_json_int_from_section(section, current_subsection_name,
														 current_subsection_name_len, 0,
														 &value_exists, &is_list_value TSRMLS_CC);

		if (value_exists) {
			if ((weight < 0) || (weight > 65535)) {
				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
					 E_RECOVERABLE_ERROR TSRMLS_CC,
					MYSQLND_MS_ERROR_PREFIX " Invalid value '%i' for weight. Stopping", weight);
			} else if (NULL == hentry_pp) {
				mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
					 E_RECOVERABLE_ERROR TSRMLS_CC,
					MYSQLND_MS_ERROR_PREFIX " Fingerprint is empty. Did you ignore an error about an unknown server? Stopping");
			} else {
				MYSQLND_MS_FILTER_LB_WEIGHT * weight_entry;
				weight_entry = mnd_pecalloc(1, sizeof(MYSQLND_MS_FILTER_LB_WEIGHT), persistent);
				if (!weight_entry) {
					/* Not sure if we need to stop here */
					MYSQLND_MS_WARN_OOM();
				} else {
					weight_entry->weight = weight_entry->current_weight = weight;
					weight_entry->persistent = persistent;

					mysqlnd_ms_get_fingerprint_connection(&fprint_conn, _ms_a_zval hentry_pp TSRMLS_CC);

					if (SUCCESS != _MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, lb_weights_list, fprint_conn.c, fprint_conn.len - 1, weight_entry)) {
						mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
							E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Failed to create internal weights lookup table for filter '%s'. Stopping", filter_name);
					}

					_ms_smart_method(free, &fprint_conn);
				}
			}
		}
	} while (1);


	if (zend_hash_num_elements(lb_weights_list) &&
		(zend_hash_num_elements(&server_names) != zend_hash_num_elements(lb_weights_list)))
	{
		mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
			E_RECOVERABLE_ERROR TSRMLS_CC,
			MYSQLND_MS_ERROR_PREFIX " You must specify the load balancing weight for none or all configured servers. There is no default weight yet. Stopping");
	}
	DBG_INF_FMT("weights %d", zend_hash_num_elements(lb_weights_list));

	zend_hash_destroy(&server_names);

	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_populate_weights_sort_list */
enum_func_status
mysqlnd_ms_populate_weights_sort_list(HashTable * lb_weights_list,
									  zend_llist * lb_sort_list,
									  const zend_llist * const server_list TSRMLS_DC)
{
	int retval = FAILURE;
	MYSQLND_MS_FILTER_LB_WEIGHT _ms_p_zval * weight_entry;
	_ms_smart_type fprint_conn = {0};
	MYSQLND_MS_FILTER_LB_WEIGHT_IN_CONTEXT * lb_weight_context;
	MYSQLND_MS_LIST_DATA * element = NULL;

	DBG_ENTER("mysqlnd_ms_populate_weights_sort_list");

	DBG_INF("Building sort list");
	BEGIN_ITERATE_OVER_SERVER_LIST(element, server_list);
		mysqlnd_ms_get_fingerprint_connection(&fprint_conn, &element TSRMLS_CC);
		retval = _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, lb_weights_list, fprint_conn.c, fprint_conn.len - 1/*\0 counted*/, weight_entry);
		if (SUCCESS == retval) {
			/* persistent needed in weight entry - could take from element/conn */
			DBG_INF_FMT("Weight entry %d current %d", (_ms_p_zval weight_entry)->weight, (_ms_p_zval weight_entry)->current_weight);

			lb_weight_context = mnd_pecalloc(1, sizeof(MYSQLND_MS_FILTER_LB_WEIGHT_IN_CONTEXT), (_ms_p_zval weight_entry)->persistent);
			if (lb_weight_context) {
				/* TODO: are we getting a pointer to the main list ? */
				lb_weight_context->lb_weight = (_ms_p_zval weight_entry);
				lb_weight_context->element = element;
				zend_llist_add_element(lb_sort_list, &lb_weight_context);
			} else {
				MYSQLND_MS_WARN_OOM();
				retval = FAIL;
			}
		}
		if (SUCCESS != retval) {
			DBG_INF_FMT("Failed to create sort list, fingerprint -%s- %d", fprint_conn.c, fprint_conn.len);
			_ms_smart_method(free, &fprint_conn);
			break;
		}
		_ms_smart_method(free, &fprint_conn);
	END_ITERATE_OVER_SERVER_LIST;

	DBG_RETURN(retval == SUCCESS? PASS:FAIL);
}
/* }}} */


/* {{{ mysqlnd_ms_weights_comparator */
static int
mysqlnd_ms_weights_comparator(const zend_llist_element ** el1, const zend_llist_element ** el2 TSRMLS_DC)
{
	MYSQLND_MS_FILTER_LB_WEIGHT_IN_CONTEXT * w1 = (el1 && *el1 && (*el1)->data) ? *(MYSQLND_MS_FILTER_LB_WEIGHT_IN_CONTEXT **)((*el1)->data) : NULL;
	MYSQLND_MS_FILTER_LB_WEIGHT_IN_CONTEXT * w2 = (el2 && *el2 && (*el2)->data) ? *(MYSQLND_MS_FILTER_LB_WEIGHT_IN_CONTEXT **)((*el2)->data) : NULL;
	int ret = 0;

	DBG_ENTER("mysqlnd_ms_weights_comparator");

	if ((w1) && (w1->lb_weight) && (w2) && (w2->lb_weight)) {
		DBG_INF_FMT("w1 %d w1c %d w2 %d w2c %d", w1->lb_weight->weight, w1->lb_weight->current_weight, w2->lb_weight->weight, w2->lb_weight->current_weight);
		if (((MYSQLND_MS_FILTER_LB_WEIGHT *)w1->lb_weight)->current_weight < ((MYSQLND_MS_FILTER_LB_WEIGHT *)w2->lb_weight)->current_weight) {
			ret = 1;
		} else if (((MYSQLND_MS_FILTER_LB_WEIGHT *)w1->lb_weight)->current_weight > ((MYSQLND_MS_FILTER_LB_WEIGHT *)w2->lb_weight)->current_weight) {
			ret = -1;
		}
	}
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_weight_list_copy */
void
mysqlnd_ms_weight_list_copy(zend_llist * dst, zend_llist * src TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_weight_list_copy");
	zend_llist_copy(dst, src);
	dst->dtor = NULL;
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_sort_weights_context_list */
void
mysqlnd_ms_weight_list_init(zend_llist * wl, zend_bool persistent TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_weight_list_init");
	zend_llist_init(wl, sizeof(MYSQLND_MS_FILTER_LB_WEIGHT_IN_CONTEXT *), mysqlnd_ms_filter_lb_weigth_in_ctx_dtor, persistent);
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_weight_list_sort */
void
mysqlnd_ms_weight_list_sort(zend_llist * wl TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_weight_list_sort");
	zend_llist_sort(wl, mysqlnd_ms_weights_comparator TSRMLS_CC);
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_weight_list_deinit */
void
mysqlnd_ms_weight_list_deinit(zend_llist * wl TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_weight_list_deinit");
	zend_llist_clean(wl);
	DBG_VOID_RETURN;
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
