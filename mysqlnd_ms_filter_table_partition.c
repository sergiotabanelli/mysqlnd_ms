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
#include "mysqlnd_qp.h"

static enum_func_status mysqlnd_ms_load_table_filters(HashTable * master_rules, HashTable * slave_rules,
													  struct st_mysqlnd_ms_config_json_entry * section,
													  MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC);

/* {{{ mysqlnd_ms_filter_ht_dtor */
static void
mysqlnd_ms_filter_ht_dtor(_ms_hash_zval_type * data)
{
	HashTable * entry = _ms_p_zval (HashTable _ms_p_zval *) _MS_HASH_Z_PTR_P(data);
	TSRMLS_FETCH();
	if (entry) {
		zend_hash_destroy(entry);
		mnd_free(entry);
	}
}
/* }}} */

/* {{{ table_filter_dtor */
static void
table_filter_dtor(struct st_mysqlnd_ms_filter_data * pDest TSRMLS_DC)
{
	MYSQLND_MS_FILTER_TABLE_DATA * filter = (MYSQLND_MS_FILTER_TABLE_DATA *) pDest;
	DBG_ENTER("table_specific_dtor");

	zend_hash_destroy(&filter->master_rules);
	zend_hash_destroy(&filter->slave_rules);
	mnd_pefree(filter, filter->parent.persistent);

	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ table_filter_conn_pool_replaced */
void table_filter_conn_pool_replaced(struct st_mysqlnd_ms_filter_data * data, zend_llist * master_connections, zend_llist * slave_connections, MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC)
{
	DBG_ENTER("table_filter_conn_pool_replaced");
	/* Must be Fabric: weights? */
	mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE,
								E_WARNING TSRMLS_CC,
								MYSQLND_MS_ERROR_PREFIX " Replacing the connection pool at runtime is not supported by the '%s' filter.", PICK_TABLE);
	DBG_VOID_RETURN;
}

/* {{{ mysqlnd_ms_table_filter_ctor */
MYSQLND_MS_FILTER_DATA *
mysqlnd_ms_table_filter_ctor(struct st_mysqlnd_ms_config_json_entry * section, zend_llist * master_connections, zend_llist * slave_connections, MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC)
{
	MYSQLND_MS_FILTER_TABLE_DATA * ret;
	DBG_ENTER("mysqlnd_ms_table_filter_ctor");
	DBG_INF_FMT("section=%p", section);
	ret = mnd_pecalloc(1, sizeof(MYSQLND_MS_FILTER_TABLE_DATA), persistent);
	if (ret) {
		do {

			ret->parent.filter_dtor = table_filter_dtor;
			ret->parent.filter_conn_pool_replaced = table_filter_conn_pool_replaced;

			zend_hash_init(&ret->master_rules, 4, NULL/*hash*/, mysqlnd_ms_filter_ht_dtor/*dtor*/, persistent);
			zend_hash_init(&ret->slave_rules, 4, NULL/*hash*/, mysqlnd_ms_filter_ht_dtor/*dtor*/, persistent);
			if (FAIL == mysqlnd_ms_load_table_filters(&ret->master_rules, &ret->slave_rules, section, error_info, persistent TSRMLS_CC)) {
				ret->parent.filter_dtor((MYSQLND_MS_FILTER_DATA *)ret TSRMLS_CC);
				ret = NULL;
				break;
			}
		} while (0);
	} else {
		MYSQLND_MS_WARN_OOM();
	}
	DBG_RETURN((MYSQLND_MS_FILTER_DATA *) ret);
}
/* }}} */

/* {{{ mysqlnd_ms_filter_dtor */
static void
mysqlnd_ms_filter_dtor(void * data)
{
	MYSQLND_MS_TABLE_FILTER * entry = (MYSQLND_MS_TABLE_FILTER *) data;
	TSRMLS_FETCH();
	if (entry) {
		zend_bool pers = entry->persistent;
		if (entry->wild) {
			mnd_pefree(entry->wild, pers);
			entry->wild = NULL;
		}
		if (entry->host_id) {
			mnd_pefree(entry->host_id, pers);
			entry->host_id = NULL;
		}
#ifdef WE_NEED_NEXT
		if (entry->next) {
			mysqlnd_ms_filter_dtor(entry->next);
		}
#endif
		mnd_pefree(entry, pers);
		* (MYSQLND_MS_TABLE_FILTER **) data = NULL;
	}
}
/* }}} */

/* {{{ mysqlnd_ms_ht_filter_dtor */
static void
mysqlnd_ms_ht_filter_dtor(_ms_hash_zval_type * pDest)
{
	mysqlnd_ms_filter_dtor( _ms_p_zval _MS_HASH_Z_PTR_P(pDest) );
}
/* }}} */

#if 0
/* {{{ mysqlnd_ms_filter_compare (based on array_data_compare) */
static int
mysqlnd_ms_filter_comparator(const MYSQLND_MS_TABLE_FILTER * a, const MYSQLND_MS_TABLE_FILTER * b)
{
	if (a && b) {
		return a->priority > b->priority? -1:(a->priority == b->priority? 0: 1);
	}
	return 0;
}
/* }}} */

/* {{{ mysqlnd_ms_filter_compare (based on array_data_compare) */
static int
mysqlnd_ms_filter_compare(const void * a, const void * b TSRMLS_DC)
{
	Bucket * f = *((Bucket **) a);
	Bucket * s = *((Bucket **) b);
	MYSQLND_MS_TABLE_FILTER * first = *((MYSQLND_MS_TABLE_FILTER **) f->pData);
	MYSQLND_MS_TABLE_FILTER * second = *((MYSQLND_MS_TABLE_FILTER **) s->pData);

	return mysqlnd_ms_filter_comparator(first, second);
}
/* }}} */
#endif

/* {{{ mysqlnd_ms_table_add_rule */
static enum_func_status
mysqlnd_ms_table_add_rule(HashTable * rules_ht,
						  const char * const section_name, const size_t section_name_len,
						  const char * const filter_mask, const size_t filter_mask_len,
						  struct st_mysqlnd_ms_config_json_entry * current_filter,
						  MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_TABLE_FILTER * new_filter_entry = NULL;
	DBG_ENTER("mysqlnd_ms_table_add_rule");
	DBG_INF_FMT("filter_mask=%s", filter_mask);
	do {
		zend_bool value_exists = FALSE;
		zend_bool section_exists;
		struct st_mysqlnd_ms_config_json_entry * sub_section =
				mysqlnd_ms_config_json_sub_section(current_filter, section_name, section_name_len, &section_exists TSRMLS_CC);

		zend_bool subsection_is_list = section_exists? mysqlnd_ms_config_json_section_is_list(sub_section TSRMLS_CC) : FALSE;
		/* we don't need objects, we check for this */
		zend_bool subsection_isnt_obj_list =
				subsection_is_list && !mysqlnd_ms_config_json_section_is_object_list(sub_section TSRMLS_CC)? TRUE:FALSE;

		DBG_INF_FMT("getting in? %d", sub_section && subsection_isnt_obj_list);
		if (sub_section && subsection_isnt_obj_list) {
			zend_bool server_list_is_list_value;
			do {
				size_t server_name_len;
				char * server_name =
					mysqlnd_ms_config_json_string_from_section(current_filter, section_name, section_name_len, 0,
															  &value_exists, &server_list_is_list_value TSRMLS_CC);
				if (!value_exists) {
					break;
				}
				server_name_len = strlen(server_name);
				new_filter_entry = mnd_pecalloc(1, sizeof(MYSQLND_MS_TABLE_FILTER), persistent);
				if (!new_filter_entry) {
					ret = FAIL;
					break;
				}
				new_filter_entry->persistent = persistent;
				new_filter_entry->host_id_len = server_name_len;
				new_filter_entry->host_id = mnd_pestrndup(server_name? server_name:"", server_name?server_name_len:0, persistent);
				DBG_INF_FMT("server_name=%s", server_name);
				mnd_efree(server_name);
				server_name = NULL;
				/* now add */
				{
					HashTable _ms_p_zval * existing_filter;
					if (SUCCESS == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, rules_ht, filter_mask, filter_mask_len, existing_filter)) {
						DBG_INF("Filter HT already exists");
						if (!existing_filter ||
							SUCCESS != _MS_HASH_SET_ZR_FUNC_PTR(zend_hash_next_index_insert_ptr, _ms_p_zval existing_filter, new_filter_entry))
						{
							char error_buf[256];
							snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX "Couldn't add new filter and couldn't find the original %*s",
									(int) filter_mask_len, filter_mask);
							error_buf[sizeof(error_buf) - 1] = '\0';
							SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
							php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", error_buf);
							DBG_ERR_FMT("%s", error_buf);
							mysqlnd_ms_filter_dtor(new_filter_entry);
							ret = FAIL;
						} else {
							DBG_INF("Added to the existing HT");
							DBG_INF("re-sorting");
							/* Sort specified array. */
#ifdef PRIORITY_IS_OFF_FOR_NOW
							zend_hash_sort(_ms_p_zval existing_filter, zend_qsort, mysqlnd_ms_filter_compare, 0 /* renumber */ TSRMLS_CC);
#endif
						}
					} else {
						HashTable * ht_for_new_filter = mnd_malloc(sizeof(HashTable));
						DBG_INF("Filter HT doesn't exist, need to create it");
						if (ht_for_new_filter) {
							if (SUCCESS == zend_hash_init(ht_for_new_filter, 2, NULL, mysqlnd_ms_ht_filter_dtor, 1/*pers*/)) {
								if (SUCCESS != _MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, rules_ht, filter_mask, filter_mask_len, ht_for_new_filter)) {
									char error_buf[256];
									snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX "The hashtable %*s did not exist in the slave_rules but couldn't add", (int) filter_mask_len, filter_mask);
									error_buf[sizeof(error_buf) - 1] = '\0';
									SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
									php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", error_buf);
									DBG_ERR_FMT("%s", error_buf);

									zend_hash_destroy(ht_for_new_filter);
									mnd_free(ht_for_new_filter);
									ht_for_new_filter = NULL;
									mysqlnd_ms_filter_dtor(new_filter_entry);
									new_filter_entry = NULL;
								} else if (SUCCESS != _MS_HASH_SET_ZR_FUNC_PTR(zend_hash_next_index_insert_ptr, ht_for_new_filter, new_filter_entry))
								{
									char error_buf[256];
									snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX "Couldn't add new filter and couldn't find the original %*s",
											(int) filter_mask_len, filter_mask);
									error_buf[sizeof(error_buf) - 1] = '\0';
									SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
									php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", error_buf);
									DBG_ERR_FMT("%s", error_buf);
									mysqlnd_ms_filter_dtor(new_filter_entry);
								} else {
									DBG_INF("Created, added to global HT and filter added to local HT");
								}
							}
						}
					}
				}
			} while (server_list_is_list_value == TRUE);
		} /* slaves_section && subsection_is_obj_list */
	} while (0);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_load_table_filters */
static enum_func_status
mysqlnd_ms_load_table_filters(HashTable * master_rules, HashTable * slave_rules,
							  struct st_mysqlnd_ms_config_json_entry * section,
							  MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC)
{
	enum_func_status ret = PASS;
	unsigned int filter_count = 0;
	DBG_ENTER("mysqlnd_ms_load_table_filters");

	if (section && master_rules && slave_rules) {
		zend_bool section_exists;
		struct st_mysqlnd_ms_config_json_entry * filters_section =
				mysqlnd_ms_config_json_sub_section(section, TABLE_RULES, sizeof(TABLE_RULES) - 1, &section_exists TSRMLS_CC);
		zend_bool subsection_is_list = section_exists? mysqlnd_ms_config_json_section_is_list(filters_section TSRMLS_CC) : FALSE;
		zend_bool subsection_is_obj_list =
				subsection_is_list && mysqlnd_ms_config_json_section_is_object_list(filters_section TSRMLS_CC)? TRUE:FALSE;

		if (filters_section && subsection_is_obj_list) {
			do {
				char * filter_mask = NULL;
				size_t filter_mask_len = 0;
				struct st_mysqlnd_ms_config_json_entry * current_filter =
						mysqlnd_ms_config_json_next_sub_section(filters_section, &filter_mask, &filter_mask_len, NULL TSRMLS_CC);

				if (!current_filter || !filter_mask || !filter_mask_len) {
					if (NULL != filter_mask) {
						char error_buf[128];
						snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " A table filter must be given a name. You must not use an empty string");
						error_buf[sizeof(error_buf) - 1] = '\0';
						SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
						DBG_ERR_FMT("%s", error_buf);
						php_error_docref(NULL TSRMLS_CC, E_RECOVERABLE_ERROR, "%s", error_buf);
					}
					DBG_INF("no next sub-section");
					break;
				}
				DBG_INF_FMT("---------- Loading MASTER rules for [%s] ----------------", filter_mask);
				if (PASS == mysqlnd_ms_table_add_rule(master_rules, MASTER_NAME, sizeof(MASTER_NAME) - 1,
													  filter_mask, filter_mask_len,
													  current_filter, error_info, persistent TSRMLS_CC))
				{
					DBG_INF_FMT("---------- Loading SLAVE rules for [%s] ----------------", filter_mask);
					if (PASS == mysqlnd_ms_table_add_rule(slave_rules, SLAVE_NAME, sizeof(SLAVE_NAME) - 1,
														  filter_mask, filter_mask_len,
														  current_filter, error_info, persistent TSRMLS_CC))
					{
						filter_count++;
					}
				}
			} while (1);
		}
	}
	DBG_INF_FMT("filter_count=%u", filter_count);

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_table_filter_match */
static enum_func_status
mysqlnd_ms_table_filter_match(const char * const db_table_buf, HashTable * rules,
							  zend_llist * in_list, zend_llist * out_list TSRMLS_DC)
{
	enum_func_status ret = PASS;
	zend_bool match = FALSE;
	HashPosition pos_rules;
	HashTable _ms_p_zval * filter_ht;
	DBG_ENTER("mysqlnd_ms_table_filter_match");

	zend_hash_internal_pointer_reset_ex(rules, &pos_rules);
	while ((FALSE == match) && (SUCCESS == _MS_HASH_GET_ZR_FUNC_PTR_VA(zend_hash_get_current_data_ptr_ex, rules, filter_ht, &pos_rules) && filter_ht)) {
		char * filter_mask;
		uint fm_len;
		ulong n_key;

		if (HASH_KEY_IS_STRING == _ms_hash_str_get_current_key(rules, &filter_mask, &fm_len, &n_key, &pos_rules)) {
			DBG_INF_FMT("Comparing [%s] with [%s]", db_table_buf, filter_mask);
			if (TRUE == (match = mysqlnd_ms_match_wild(db_table_buf, filter_mask TSRMLS_CC))) {
				MYSQLND_MS_TABLE_FILTER _ms_p_zval * entry_filter_pp;
				HashPosition pos_servers;
				/* found a match*/
				DBG_INF("Found a match");
				zend_hash_internal_pointer_reset_ex(_ms_p_zval filter_ht, &pos_servers);
				while (SUCCESS == _MS_HASH_GET_ZR_FUNC_PTR_VA(zend_hash_get_current_data_ptr_ex,_ms_p_zval filter_ht, entry_filter_pp,
																&pos_servers) && entry_filter_pp)
				{
					MYSQLND_MS_TABLE_FILTER * entry_filter = _ms_p_zval entry_filter_pp;
					/* compare entry_filter->host_id with MYSQLND_MS_LIST_DATA::name_from_config */
					MYSQLND_MS_LIST_DATA * el;

					BEGIN_ITERATE_OVER_SERVER_LIST(el, in_list);
					if (!strncmp(entry_filter->host_id, el->name_from_config, entry_filter->host_id_len)) {
						DBG_INF_FMT("Matched [%s] with a server, adding to the list", entry_filter->host_id);
						if (el->conn) {
							DBG_INF_FMT("Matched conn_id: "MYSQLND_LLU_SPEC, el->conn->thread_id);
						}

						zend_llist_add_element(out_list, &el);

						goto skip1; /* we can't use continue as BEGIN_ITERATE uses a loop */
						/* XXX: maybe we want directly to go to skip2 ??? */
					}
					END_ITERATE_OVER_SERVER_LIST;
skip1:
					zend_hash_move_forward_ex(_ms_p_zval filter_ht, &pos_servers);
				}
			}
		}
		zend_hash_move_forward_ex(rules, &pos_rules);
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_choose_connection_table_filter */
enum_func_status
mysqlnd_ms_choose_connection_table_filter(void * f_data, const char * query, size_t query_len,
									 const char * const connect_or_select_db,
									 zend_llist * master_list, zend_llist * slave_list,
									 zend_llist * selected_masters, zend_llist * selected_slaves,
									 struct mysqlnd_ms_lb_strategies * stgy_not_used,
									 MYSQLND_ERROR_INFO * error_info
									 TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	MYSQLND_MS_FILTER_TABLE_DATA * filter = (MYSQLND_MS_FILTER_TABLE_DATA *) f_data;
	struct st_mysqlnd_query_parser * parser;
	DBG_ENTER("mysqlnd_ms_choose_connection_table_filter");
	if (filter) {
		int err;
		parser = mysqlnd_qp_create_parser(TSRMLS_C);
		if (parser && !(err = mysqlnd_qp_start_parser(parser, query, query_len TSRMLS_CC))) {
			zend_llist_position tinfo_list_pos;
			struct st_mysqlnd_ms_table_info * tinfo;
			zend_llist master_in_stack, * master_in = &master_in_stack;
			zend_llist master_out_stack, * master_out = &master_out_stack;
			zend_llist slave_in_stack, * slave_in = &slave_in_stack;
			zend_llist slave_out_stack, * slave_out = &slave_out_stack;
			zend_llist_init(master_in, sizeof(MYSQLND_MS_LIST_DATA *), NULL /*dtor*/, FALSE);
			zend_llist_init(master_out, sizeof(MYSQLND_MS_LIST_DATA *), NULL /*dtor*/, FALSE);
			zend_llist_init(slave_in, sizeof(MYSQLND_MS_LIST_DATA *), NULL /*dtor*/, FALSE);
			zend_llist_init(slave_out, sizeof(MYSQLND_MS_LIST_DATA *), NULL /*dtor*/, FALSE);

			mysqlnd_ms_select_servers_all(master_list, slave_list, master_in, slave_in TSRMLS_CC);

			for (tinfo = zend_llist_get_first_ex(&parser->parse_info.table_list, &tinfo_list_pos);
				 tinfo;
				 tinfo = zend_llist_get_next_ex(&parser->parse_info.table_list, &tinfo_list_pos))
			{
				/* 80 char db + '.' + 80 char table + \0 : should be 64 but prepared for the future */
				char db_table_buf[4*80 + 1 + 4*80 + 1];
				DBG_INF_FMT("current db=%s table=%s org_table=%s statement_type=%d",
						tinfo->db? tinfo->db:"n/a",
						tinfo->table? tinfo->table:"n/a",
						tinfo->org_table? tinfo->org_table:"n/a",
						parser->parse_info.statement
					);
				if (tinfo->db) {
					snprintf(db_table_buf, sizeof(db_table_buf), "%s.%s", tinfo->db, tinfo->table);
					DBG_INF_FMT("qualified table=%s", db_table_buf);
				} else if (tinfo->table && connect_or_select_db) {
					snprintf(db_table_buf, sizeof(db_table_buf), "%s.%s", connect_or_select_db, tinfo->table);
					DBG_INF_FMT("qualified table=%s (using connect_or_select_db=%s)", db_table_buf, connect_or_select_db);
				} else {
					char error_buf[256];
					snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " Failed to parse table name");
					error_buf[sizeof(error_buf) - 1] = '\0';
					SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
					DBG_ERR_FMT("%s", error_buf);
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", error_buf);
					break;
				}
				zend_llist_clean(master_out);
				zend_llist_clean(slave_out);
				ret = mysqlnd_ms_table_filter_match(db_table_buf, &filter->master_rules, master_in, master_out TSRMLS_CC);

				if (PASS == ret && parser->parse_info.statement == STATEMENT_SELECT) {
					/* non-SELECTs don't go the the slaves */
					ret = mysqlnd_ms_table_filter_match(db_table_buf, &filter->slave_rules, slave_in, slave_out TSRMLS_CC);
				}

				if (ret != PASS || (!zend_llist_count(master_out) && !zend_llist_count(slave_out))) {
					break;
				}
				zend_llist_clean(master_in);
				zend_llist_clean(slave_in);
				mysqlnd_ms_select_servers_all(master_out, slave_out, master_in, slave_in TSRMLS_CC);
				DBG_INF_FMT("selected_masters=%d selected_slaves=%d", zend_llist_count(master_out), zend_llist_count(slave_out));
			}
			mysqlnd_ms_select_servers_all(master_out, slave_out, selected_masters, selected_slaves TSRMLS_CC);
			zend_llist_clean(master_in);
			zend_llist_clean(slave_in);
			zend_llist_clean(master_out);
			zend_llist_clean(slave_out);

		}
		if (parser) {
#if 0
			if (err) {
				char error_buf[256];
				snprintf(error_buf, sizeof(error_buf), MYSQLND_MS_ERROR_PREFIX " Please, check the SQL syntax. If correct, report a bug. Parser error %d. Failed to parse statement '%*s'",
									err, (int) query_len, query);
				error_buf[sizeof(error_buf) - 1] = '\0';
				SET_CLIENT_ERROR((*error_info), CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, error_buf);
				DBG_ERR_FMT("%s", error_buf);
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", error_buf);
				DBG_INF_FMT("parser start error %d", err);
			}
#endif
			mysqlnd_qp_free_parser(parser TSRMLS_CC);
			ret = PASS;
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
