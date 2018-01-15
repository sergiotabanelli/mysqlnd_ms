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

/* $Id: $ */
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
#if PHP_VERSION_ID >= 50400
#include "ext/mysqlnd/mysqlnd_ext_plugin.h"
#endif

#include "mysqlnd_ms.h"
#include "mysqlnd_ms_config_json.h"
#include "mysqlnd_ms_enum_n_def.h"
#include "mysqlnd_ms_switch.h"

#include "mysqlnd_query_parser.h"
#include "mysqlnd_qp.h"


/* {{{ groups_filter_dtor */
static void
groups_filter_dtor(struct st_mysqlnd_ms_filter_data * pDest TSRMLS_DC)
{
	MYSQLND_MS_FILTER_GROUPS_DATA * filter = (MYSQLND_MS_FILTER_GROUPS_DATA *) pDest;
	DBG_ENTER("groups_filter_dtor");


	if (&filter->groups) {
		zend_hash_destroy(&filter->groups);
	}
	mnd_pefree(filter, filter->parent.persistent);

	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_filter_groups_ht_dtor */
void
mysqlnd_ms_filter_groups_ht_dtor(_ms_hash_zval_type * pDest)
{
	MYSQLND_MS_FILTER_GROUPS_DATA_GROUP * element = pDest? _ms_p_zval (MYSQLND_MS_FILTER_GROUPS_DATA_GROUP _ms_p_zval *) _MS_HASH_Z_PTR_P(pDest) : NULL;
	TSRMLS_FETCH();
	DBG_ENTER("mysqlnd_ms_filter_groups_ht_dtor");

	zend_hash_destroy(&element->master_context);
	zend_hash_destroy(&element->slave_context);

	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_groups_filter_conn_pool_replaced */
static void
groups_filter_conn_pool_replaced(struct st_mysqlnd_ms_filter_data * data, zend_llist * master_connections, zend_llist * slave_connections, MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_groups_filter_conn_pool_replaced");
	/* Must be Fabric: filter using master and slave context for something...  */
	mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
								E_WARNING TSRMLS_CC,
								MYSQLND_MS_ERROR_PREFIX " Replacing the connection pool at runtime is not supported by the '%s' filter.", PICK_GROUPS);
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ mysqlnd_ms_groups_filter_ctor */
MYSQLND_MS_FILTER_DATA *
mysqlnd_ms_groups_filter_ctor(struct st_mysqlnd_ms_config_json_entry * section, zend_llist * master_connections, zend_llist * slave_connections, MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC)
{
	MYSQLND_MS_FILTER_GROUPS_DATA * ret = NULL;
	MYSQLND_MS_LIST_DATA * entry, **entry_pp;
	zend_llist_position	pos;

	DBG_ENTER("mysqlnd_ms_groups_filter_ctor");
	DBG_INF_FMT("section=%p", section);
	if (section) {
		ret = mnd_pecalloc(1, sizeof(MYSQLND_MS_FILTER_GROUPS_DATA), persistent);
		if (ret) {

			ret->parent.filter_dtor = groups_filter_dtor;
			ret->parent.filter_conn_pool_replaced = groups_filter_conn_pool_replaced;

			zend_hash_init(&ret->groups, 4, NULL/*hash*/, mysqlnd_ms_filter_groups_ht_dtor, persistent);

			if ((TRUE == mysqlnd_ms_config_json_section_is_list(section TSRMLS_CC) &&
				 TRUE == mysqlnd_ms_config_json_section_is_object_list(section TSRMLS_CC)))
			{
				/* node_groups => array(GROUP_NAME_A => ...) */
				struct st_mysqlnd_ms_config_json_entry * subsection = NULL;
				HashTable server_names;

				/* 1) build list of all server names from config - note that names must be globally unique */
				zend_hash_init(&server_names, 4, NULL/*hash*/, NULL/*dtor*/, FALSE);

				for (entry_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(master_connections, &pos);
					entry_pp && (entry = *entry_pp) && (entry->name_from_config) && (entry->conn);
					entry_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_next_ex(master_connections, &pos)) {

					if (SUCCESS != _MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, &server_names, entry->name_from_config, strlen(entry->name_from_config), (*entry_pp))) {
						mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
							E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Failed to setup master server list for '%s' filter. Stopping", PICK_GROUPS);
					}
				}

				for (entry_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(slave_connections, &pos);
					entry_pp && (entry = *entry_pp) && (entry->name_from_config) && (entry->conn);
					entry_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_next_ex(slave_connections, &pos)) {

					if (SUCCESS != _MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, &server_names, entry->name_from_config, strlen(entry->name_from_config), (*entry_pp))) {
						mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
							E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Failed to setup slave server list for '%s' filter. Stopping", PICK_GROUPS);
					}
				}
				DBG_INF_FMT("server name list has %d entries", zend_hash_num_elements(&server_names));

				/* 2. iterate over node_group names */
				do {
					char * current_group_name = NULL;
					size_t current_group_name_len = 0;
					char * server_name = NULL;
					size_t server_name_len = 0;
					struct st_mysqlnd_ms_config_json_entry * serversection = NULL;
					zend_bool section_exists;
					zend_bool is_list;
					MYSQLND_MS_FILTER_GROUPS_DATA_GROUP * node_group = NULL;

					subsection = mysqlnd_ms_config_json_next_sub_section(section,
																	&current_group_name,
																	&current_group_name_len,
																	NULL TSRMLS_CC);
					if (!subsection) {
						break;
					}
					DBG_INF_FMT("checking node group '%s'", current_group_name);

					/* 3. for each node_group parse out master and slave servers */
					node_group = mnd_pecalloc(1, sizeof(MYSQLND_MS_FILTER_GROUPS_DATA_GROUP), persistent);
					if (!node_group) {
						mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
							E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Failed to allocate memory to create node group '%s' for '%s' filter. Stopping", current_group_name, PICK_GROUPS);
						break;
					}

					/* TODO: dtor needed? Seems not to be the case */
					zend_hash_init(&node_group->master_context, 4, NULL/*hash*/, NULL/*dtor*/, persistent);
					zend_hash_init(&node_group->slave_context, 4, NULL/*hash*/, NULL/*dtor*/, persistent);
					if (SUCCESS != _MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, &ret->groups, current_group_name, current_group_name_len, node_group)) {
						mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
							E_RECOVERABLE_ERROR TSRMLS_CC,
							MYSQLND_MS_ERROR_PREFIX " Failed to create node group '%s' for '%s' filter. Stopping", current_group_name, PICK_GROUPS);
						break;
					}

					/* 3.1 masters */
					serversection = mysqlnd_ms_config_json_sub_section(subsection,
																	MASTER_NAME,
																	sizeof(MASTER_NAME) - 1,
																	&section_exists TSRMLS_CC);
					if (section_exists && serversection) {
						ulong nkey = 0;
						server_name = NULL;
						do {
							server_name = mysqlnd_ms_config_json_string_from_section(serversection, NULL, 0, nkey,  &section_exists,  &is_list TSRMLS_CC);
							if (section_exists && server_name) {
								server_name_len = strlen(server_name);

								if (SUCCESS != _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, &server_names, server_name, server_name_len, entry_pp)) {
									mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
									E_RECOVERABLE_ERROR TSRMLS_CC,
									MYSQLND_MS_ERROR_PREFIX " Unknown master '%s' (section '%s') in '%s' filter configuration. Stopping",
									server_name, current_group_name, PICK_GROUPS);
									mnd_efree(server_name);
									continue;
								}
								if (SUCCESS != _MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, &node_group->master_context, server_name, server_name_len, server_name)) {
									mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
										E_RECOVERABLE_ERROR TSRMLS_CC,
										MYSQLND_MS_ERROR_PREFIX " Failed to add master '%s' to node group '%s' for '%s' filter. Stopping", server_name, current_group_name, PICK_GROUPS);
									mnd_efree(server_name);
									continue;
								}
								mnd_efree(server_name);
							}
						} while (section_exists && ++nkey);
						DBG_INF_FMT("added '%d' masters", zend_hash_num_elements(&node_group->master_context));
					}
					if ((zend_llist_count(master_connections) > 0) && (0 ==  zend_hash_num_elements(&node_group->master_context))) {
						/*
							A user may not configure any slaves if using multi-master setup.
							Unfortunately we don't know whether multi-master is on or not.
							Thus, no check and warning in the slave section below.

							However, configuring masters but not naming any in a group
							section stinks.
						*/
						mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
								E_RECOVERABLE_ERROR TSRMLS_CC,
								MYSQLND_MS_ERROR_PREFIX " No masters configured in node group '%s' for '%s' filter. Please, verify the setup", current_group_name, PICK_GROUPS);
					}

					/* 3.1 slaves */
					serversection = mysqlnd_ms_config_json_sub_section(subsection,
																	SLAVE_NAME,
																	sizeof(SLAVE_NAME) - 1,
																	&section_exists TSRMLS_CC);
					if (section_exists && serversection) {
						ulong nkey = 0;
						server_name = NULL;
						do {
							server_name = mysqlnd_ms_config_json_string_from_section(serversection, NULL, 0, nkey,  &section_exists,  &is_list TSRMLS_CC);
							if (section_exists && server_name) {
								server_name_len = strlen(server_name);

								if (SUCCESS != _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, &server_names, server_name, server_name_len, entry_pp)) {
									mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
									E_RECOVERABLE_ERROR TSRMLS_CC,
									MYSQLND_MS_ERROR_PREFIX " Unknown slave '%s' (section '%s') in '%s' filter configuration. Stopping",
									server_name, current_group_name, PICK_GROUPS);
									mnd_efree(server_name);
									continue;
								}
								if (SUCCESS != _MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, &node_group->slave_context, server_name, server_name_len, server_name)) {
									mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
										E_RECOVERABLE_ERROR TSRMLS_CC,
										MYSQLND_MS_ERROR_PREFIX " Failed to add slave '%s' to node group '%s' for '%s' filter. Stopping", server_name, current_group_name, PICK_GROUPS);
									mnd_efree(server_name);
									continue;
								}
								mnd_efree(server_name);
							}
						} while (section_exists && ++nkey);
						DBG_INF_FMT("added '%d' slaves", zend_hash_num_elements(&node_group->slave_context));
					}

				} while (1);

				zend_hash_destroy(&server_names);
			}

		} else {
			MYSQLND_MS_WARN_OOM();
		}
	}

	DBG_RETURN((MYSQLND_MS_FILTER_DATA *) ret);
}
/* }}} */


/* {{{ mysqlnd_ms_choose_connection_groups */
enum_func_status
mysqlnd_ms_choose_connection_groups(MYSQLND_CONN_DATA * conn, void * f_data, const char * connect_host,
								 char ** query, size_t * query_len,
								 zend_llist * master_list, zend_llist * slave_list,
								 zend_llist * selected_masters, zend_llist * selected_slaves,
								 struct mysqlnd_ms_lb_strategies * stgy, MYSQLND_ERROR_INFO * error_info TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_FILTER_GROUPS_DATA * filter_data = (MYSQLND_MS_FILTER_GROUPS_DATA *) f_data;
	MYSQLND_MS_LIST_DATA * element;

	DBG_ENTER("mysqlnd_ms_choose_connection_groups");

	if (filter_data && (&filter_data->groups) && (query_len > 0)) {
		MYSQLND_MS_FILTER_GROUPS_DATA_GROUP *node_group, _ms_p_zval *node_group_pp;
		struct st_ms_token_and_value token = {0};
		struct st_mysqlnd_query_scanner * scanner;
		zend_bool found = FALSE;
		char ** server_name_pp; // Not used

		scanner = mysqlnd_qp_create_scanner(TSRMLS_C);
		mysqlnd_qp_set_string(scanner, *query, *query_len TSRMLS_CC);
		token = mysqlnd_qp_get_token(scanner TSRMLS_CC);
		while (token.token == QC_TOKEN_COMMENT) {
			DBG_INF_FMT("token=COMMENT? = %s(%d)", Z_STRVAL(token.value), Z_STRLEN(token.value));
			if (SUCCESS == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, &filter_data->groups, Z_STRVAL(token.value), Z_STRLEN(token.value) - 1, node_group_pp)) {
				DBG_INF_FMT("node_group=%s", Z_STRVAL(token.value));
				found = TRUE;
				break;
			}
			zval_dtor(&token.value);
			token = mysqlnd_qp_get_token(scanner TSRMLS_CC);
		}
		zval_dtor(&token.value);
		mysqlnd_qp_free_scanner(scanner TSRMLS_CC);

		if (FALSE == found) {
			goto use_all;
		}

		/* TODO: check if HTs exist? */
		node_group = _ms_p_zval node_group_pp;
		DBG_INF_FMT("%d masters, %d slaves in group",
					zend_hash_num_elements(&node_group->master_context),
					zend_hash_num_elements(&node_group->slave_context));

		/*
		 Note: the way we write the loop probably determines the rr selection order.
		 We shall use input sort order. No re-ordering!
		*/
		BEGIN_ITERATE_OVER_SERVER_LIST(element, master_list)
			if (element && element->name_from_config) {
				if ((SUCCESS == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, &node_group->master_context, element->name_from_config, strlen(element->name_from_config), server_name_pp))) {
					zend_llist_add_element(selected_masters, &element);
				}
			}
		END_ITERATE_OVER_SERVER_LIST;

		BEGIN_ITERATE_OVER_SERVER_LIST(element, slave_list)
			if (element && element->name_from_config) {
				if ((SUCCESS == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, &node_group->slave_context, element->name_from_config, strlen(element->name_from_config), server_name_pp))) {
					zend_llist_add_element(selected_slaves, &element);
				}
			}
		END_ITERATE_OVER_SERVER_LIST;

		DBG_RETURN(ret);
	}

use_all:
	DBG_INF("Using all servers");
	BEGIN_ITERATE_OVER_SERVER_LIST(element, master_list)
		zend_llist_add_element(selected_masters, &element);
	END_ITERATE_OVER_SERVER_LIST;
	BEGIN_ITERATE_OVER_SERVER_LIST(element, slave_list)
		zend_llist_add_element(selected_slaves, &element);
	END_ITERATE_OVER_SERVER_LIST;

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
