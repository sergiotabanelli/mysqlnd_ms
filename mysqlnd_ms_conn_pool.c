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

#include "mysqlnd_ms_conn_pool.h"
#include "mysqlnd_ms.h"

/* {{{ mysqlnd_ms_pool_all_list_dtor */
void
mysqlnd_ms_pool_all_list_dtor(_ms_hash_zval_type * pDest)
{
	MYSQLND_MS_POOL_ENTRY * element = pDest? _ms_p_zval (MYSQLND_MS_POOL_ENTRY _ms_p_zval *) _MS_HASH_Z_PTR_P(pDest) : NULL;
	TSRMLS_FETCH();

	DBG_ENTER("mysqlnd_ms_pool_all_list_dtor");

	if (!element) {
		DBG_VOID_RETURN;
	}
	if (element->ms_list_data_dtor) {
		element->ms_list_data_dtor((void *)&element->data);
	}
	mnd_pefree(element, element->persistent);

	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_pool_cmd_dtor */
void
mysqlnd_ms_pool_cmd_dtor(void * pDest)
{
	MYSQLND_MS_POOL_CMD * element = pDest ? *(MYSQLND_MS_POOL_CMD**)pDest : NULL;
	TSRMLS_FETCH();

	DBG_ENTER("mysqlnd_ms_pool_cmd_dtor");

	if (element) {
		switch (element->cmd) {
			case POOL_CMD_SET_CHARSET:
				DBG_INF_FMT("pool_cmd=%p SET_CHARSET", element->data);
				{
					MYSQLND_MS_POOL_CMD_SET_CHARSET * cmd_data = (MYSQLND_MS_POOL_CMD_SET_CHARSET *)element->data;
					if (cmd_data->csname) {
						mnd_pefree(cmd_data->csname, element->persistent);
						cmd_data->csname = NULL;
					}
					/* XXX: Shouldn't we clean cmd_data / element->data here? */
				}
				break;

			case POOL_CMD_SELECT_DB:
				DBG_INF_FMT("pool_cmd=%p SELECT_DB", element->data);
				{
					MYSQLND_MS_POOL_CMD_SELECT_DB * cmd_data = (MYSQLND_MS_POOL_CMD_SELECT_DB *)element->data;
					if (cmd_data->db) {
						mnd_pefree(cmd_data->db, element->persistent);
					}
					mnd_pefree(cmd_data, element->persistent);
					element->data = NULL;
				}
				break;

			case POOL_CMD_SET_SERVER_OPTION:
				DBG_INF_FMT("pool_cmd=%p SET_SERVER_OPTION", element->data);
				mnd_pefree(element->data, element->persistent);
				element->data = NULL;
				break;

			case POOL_CMD_SET_CLIENT_OPTION:
				DBG_INF_FMT("pool_cmd=%p SET_CLIENT_OPTION", element->data);
				{
					MYSQLND_MS_POOL_CMD_SET_CLIENT_OPTION * cmd_data = (MYSQLND_MS_POOL_CMD_SET_CLIENT_OPTION *)element->data;
					if (cmd_data->value) {
						mnd_pefree(cmd_data->value, element->persistent);
					}
					mnd_pefree(cmd_data, element->persistent);
					element->data = NULL;
				}
				break;

			case POOL_CMD_CHANGE_USER:
				DBG_INF_FMT("pool_cmd=%p CHANGE_USER", element->data);
				{
					MYSQLND_MS_POOL_CMD_CHANGE_USER * cmd_data = (MYSQLND_MS_POOL_CMD_CHANGE_USER *)element->data;
					if (cmd_data->user) {
						mnd_pefree(cmd_data->user, element->persistent);
					}
					if (cmd_data->passwd) {
						mnd_pefree(cmd_data->passwd, element->persistent);
					}
					if (cmd_data->db) {
						mnd_pefree(cmd_data->db, element->persistent);
					}
					mnd_pefree(cmd_data, element->persistent);
					element->data = NULL;
				}
				break;

#if MYSQLND_VERSION_ID >= 50009
			case POOL_CMD_SET_AUTOCOMMIT:
				DBG_INF_FMT("pool_cmd=%p SET_AUTOCOMMIT", element->data);
				{
					MYSQLND_MS_POOL_CMD_SET_AUTOCOMMIT  * cmd_data = (MYSQLND_MS_POOL_CMD_SET_AUTOCOMMIT *)element->data;
					if (cmd_data)
						mnd_pefree(cmd_data, element->persistent);
					element->data = NULL;
				}
				break;
#endif
			case POOL_CMD_SSL_SET:
				DBG_INF_FMT("pool_cmd=%p SSL_SET", element->data);
				{
					MYSQLND_MS_POOL_CMD_SSL_SET * cmd_data = (MYSQLND_MS_POOL_CMD_SSL_SET *)element->data;
					if (cmd_data->key) {
						mnd_pefree(cmd_data->key, element->persistent);
					}
					if (cmd_data->cert) {
						mnd_pefree(cmd_data->cert, element->persistent);
					}
					if (cmd_data->ca) {
						mnd_pefree(cmd_data->ca, element->persistent);
					}
					if (cmd_data->capath) {
						mnd_pefree(cmd_data->capath, element->persistent);
					}
					if (cmd_data->cipher) {
						mnd_pefree(cmd_data->cipher, element->persistent);
					}
					mnd_pefree(cmd_data, element->persistent);
					element->data = NULL;
				}
				break;
			default:
				DBG_INF("unknown command");
				break;
		}
		mnd_pefree(element, element->persistent);
	}
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ pool_flush_active */
static enum_func_status
pool_flush_active(MYSQLND_MS_POOL * pool TSRMLS_DC)
{
	MYSQLND_MS_LIST_DATA * el;
	MYSQLND_MS_POOL_ENTRY _ms_p_zval * pool_element = NULL;
	enum_func_status ret = PASS;

	DBG_ENTER("mysqlnd_ms::pool_flush_active");

	if (!pool) {
		DBG_INF("No pool!");
		DBG_RETURN(FAIL);
	}

	BEGIN_ITERATE_OVER_SERVER_LIST(el, &(pool->data.active_master_list))
	{
		if (SUCCESS == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, &(pool->data.master_list), el->pool_hash_key.c, el->pool_hash_key.len - 1, pool_element)) {
			(_ms_p_zval pool_element)->active = FALSE;
		} else {
			ret = FAIL;
			DBG_INF_FMT("Pool master list is inconsistent, key=%s", el->pool_hash_key.c);
		}
	}
	END_ITERATE_OVER_SERVER_LIST;
	zend_llist_clean(&(pool->data.active_master_list));
	MYSQLND_MS_INC_STATISTIC_W_VALUE(MS_STAT_POOL_MASTERS_ACTIVE, 0);

	BEGIN_ITERATE_OVER_SERVER_LIST(el, &(pool->data.active_slave_list))
	{
		if (SUCCESS == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, &(pool->data.slave_list), el->pool_hash_key.c, el->pool_hash_key.len - 1, pool_element)) {
			(_ms_p_zval pool_element)->active = FALSE;
		} else {
			ret = FAIL;
			DBG_INF_FMT("Pool slave list is inconsistent, key=%s", el->pool_hash_key.c);
		}
	}
	END_ITERATE_OVER_SERVER_LIST;
	zend_llist_clean(&(pool->data.active_slave_list));
	MYSQLND_MS_INC_STATISTIC_W_VALUE(MS_STAT_POOL_SLAVES_ACTIVE, 0);

	DBG_RETURN(ret);
}
/* }}} */


static int
pool_clear_all_ht_apply_func(_ms_hash_zval_type *pDest, void * argument TSRMLS_DC)
{
	int ret;
	MYSQLND_MS_POOL_ENTRY * pool_element = (MYSQLND_MS_POOL_ENTRY *) _MS_HASH_Z_PTR_P(pDest);

	if (pool_element->ref_counter > 1) {
		(*(unsigned int *)argument)++;
		ret = ZEND_HASH_APPLY_KEEP;
	} else {
		pool_element->active = FALSE;
		ret = ZEND_HASH_APPLY_REMOVE;
	}
	return ret;
}
/* }}} */


/* {{{ pool_clear_all */
static enum_func_status
pool_clear_all(MYSQLND_MS_POOL * pool, unsigned int * referenced TSRMLS_DC)
{
	enum_func_status ret = PASS;

	DBG_ENTER("pool_clear_all");

	if (!pool) {
		DBG_INF("No pool!");
		DBG_RETURN(FAIL);
	}

	*referenced = 0;

	/* Active list will always be flushed */
	zend_llist_clean(&(pool->data.active_master_list));
	zend_llist_clean(&(pool->data.active_slave_list));

	zend_hash_apply_with_argument(&(pool->data.master_list), pool_clear_all_ht_apply_func, (void *)referenced TSRMLS_CC);
	zend_hash_apply_with_argument(&(pool->data.slave_list), pool_clear_all_ht_apply_func, (void *)referenced TSRMLS_CC);

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ pool_get_conn_hash_key */
static void
pool_get_conn_hash_key(_ms_smart_type * hash_key,
						const char * const unique_name_from_config,
						const char * const host,
						const char * const user,
						const char * const passwd, const size_t passwd_len,
						const unsigned int port,
						const char * const socket,
						const char * const db, const size_t db_len,
						const unsigned long connect_flags,
						const zend_bool persistent)
{
	/* TODO: Check whether we could use MYSQLND_MS_LIST_DATA * as a hash key.
	 * This may be quite tricky when we ask a question like "do we already have a
	 * connection to server A open or do we need to create a new MYSQLND_MS_LIST_DATA * object
	 * with a new connection" Hence, to get started, trying this approach without the
	 * pointer address first. It has shortcomings too as it requires a unique name.
	 * We need to get started...
	 */
	// An hack to simplify write consistency
	_ms_smart_method(appendc, hash_key, GTID_RUNNING_MARKER);
	_ms_smart_method(appendc, hash_key, '|');
	if (unique_name_from_config) {
		_ms_smart_method(appendl, hash_key, unique_name_from_config, strlen(unique_name_from_config));
	}
	_ms_smart_method(appendc, hash_key, '|');
	if (host) {
		_ms_smart_method(appendl, hash_key, host, strlen(host));
	}
	_ms_smart_method(appendc, hash_key, '|');
	if (user) {
		_ms_smart_method(appendl, hash_key, user, strlen(user));
	}
	_ms_smart_method(appendc, hash_key, '|');
	if (socket) {
		_ms_smart_method(appendl, hash_key, socket, strlen(socket));
	}
	_ms_smart_method(appendc, hash_key, '|');
	_ms_smart_method(append_unsigned, hash_key, port);
	_ms_smart_method(appendc, hash_key, '|');
	if (db && db_len) {
		_ms_smart_method(appendl, hash_key, db, db_len);
	}
	_ms_smart_method(appendc, hash_key, '|');
	_ms_smart_method(append_unsigned, hash_key, connect_flags);
	_ms_smart_method(appendc, hash_key, '|');
	_ms_smart_method(appendc, hash_key, '\0');
}
/* }}} */


/* {{{ pool_init_pool_hash_key */
static void
pool_init_pool_hash_key(MYSQLND_MS_LIST_DATA * data)
{
	if (!data) {
		return;
	}

	/* Note, accessing "private" MS_LIST_DATA contents */
	if (data->pool_hash_key.len) {
		_ms_smart_method(free, &(data->pool_hash_key));
	}
	pool_get_conn_hash_key(&(data->pool_hash_key),
							data->name_from_config,
							MYSQLND_MS_CONN_STRING(data->host), MYSQLND_MS_CONN_STRING(data->user),	MYSQLND_MS_CONN_STRING(data->passwd), MYSQLND_MS_CONN_STRING_LEN(data->passwd),
							data->port, MYSQLND_MS_CONN_STRING(data->socket), MYSQLND_MS_CONN_STRING(data->db), MYSQLND_MS_CONN_STRING_LEN(data->db),
							data->connect_flags, data->persistent);
}
/* }}} */

/* {{{ pool_add_slave */
static enum_func_status
pool_add_slave(MYSQLND_MS_POOL * pool, _ms_smart_type * hash_key,
			   MYSQLND_MS_LIST_DATA * data, zend_bool persistent TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_POOL_ENTRY * pool_element;

	DBG_ENTER("mysqlnd_ms::pool_add_slave");
	if (!pool) {
		DBG_INF("No pool!");
		DBG_RETURN(FAIL);
	}
	if (!hash_key) {
		DBG_INF("No hashkey!");
		DBG_RETURN(FAIL);	
	}
	if (!data) {
		DBG_INF("No data!");
		DBG_RETURN(FAIL);	
	}

	pool_element = mnd_pecalloc(1, sizeof(MYSQLND_MS_POOL_ENTRY), persistent);
	if (!pool_element) {
		MYSQLND_MS_WARN_OOM();
		ret = FAIL;
	} else {
		DBG_INF_FMT("pool_element=%p data=%p", pool_element, data);
		pool_element->data = data;
		if (pool->data.ms_list_data_dtor) {
			pool_element->ms_list_data_dtor = pool->data.ms_list_data_dtor;
		}
		pool_element->persistent = persistent;
		pool_element->active = TRUE;
		pool_element->activation_counter = 1;
		pool_element->ref_counter = 1;
		pool_element->removed = FALSE;

		if (SUCCESS != _MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, &(pool->data.slave_list), hash_key->c, hash_key->len - 1, pool_element)) {
			DBG_INF_FMT("Failed adding, duplicate hash key, added twice?");
			ret = FAIL;
		}
		zend_llist_add_element(&(pool->data.active_slave_list), &pool_element->data);

		MYSQLND_MS_INC_STATISTIC_W_VALUE(MS_STAT_POOL_SLAVES_TOTAL, zend_hash_num_elements(&(pool->data.slave_list)));
		MYSQLND_MS_INC_STATISTIC_W_VALUE(MS_STAT_POOL_SLAVES_ACTIVE, zend_llist_count(&(pool->data.active_slave_list)));
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ pool_add_master */
static enum_func_status
pool_add_master(MYSQLND_MS_POOL * pool, _ms_smart_type * hash_key,
				MYSQLND_MS_LIST_DATA * data, zend_bool persistent TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_POOL_ENTRY * pool_element;

	DBG_ENTER("mysqlnd_ms::pool_add_master");

	if (!pool) {
		DBG_INF("No pool!");
		DBG_RETURN(FAIL);
	}
	if (!hash_key) {
		DBG_INF("No hashkey!");
		DBG_RETURN(FAIL);	
	}
	if (!data) {
		DBG_INF("No data!");
		DBG_RETURN(FAIL);	
	}

	pool_element = mnd_pecalloc(1, sizeof(MYSQLND_MS_POOL_ENTRY), persistent);
	if (!pool_element) {
		MYSQLND_MS_WARN_OOM();
		ret = FAIL;
	} else {
		DBG_INF_FMT("pool_element=%p data=%p", pool_element, data);

		pool_element->data = data;
		if (pool->data.ms_list_data_dtor) {
			pool_element->ms_list_data_dtor = pool->data.ms_list_data_dtor;
		}
		pool_element->persistent = persistent;
		pool_element->active = TRUE;
		pool_element->activation_counter = 1;
		pool_element->ref_counter = 1;
		pool_element->removed = FALSE;

		if (SUCCESS != _MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, &(pool->data.master_list), hash_key->c, hash_key->len - 1, pool_element)) {
			DBG_INF_FMT("Failed adding, duplicate hash key, added twice?");
			ret = FAIL;
		}
		zend_llist_add_element(&(pool->data.active_master_list), &pool_element->data);

		MYSQLND_MS_INC_STATISTIC_W_VALUE(MS_STAT_POOL_MASTERS_TOTAL, zend_hash_num_elements(&(pool->data.master_list)));
		MYSQLND_MS_INC_STATISTIC_W_VALUE(MS_STAT_POOL_MASTERS_ACTIVE, zend_llist_count(&(pool->data.active_master_list)));
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ pool_connection_exists */
static zend_bool
pool_connection_exists(MYSQLND_MS_POOL * pool, _ms_smart_type * hash_key,
					   MYSQLND_MS_LIST_DATA ** data,
					   zend_bool * is_master, zend_bool * is_active,
					   zend_bool * is_removed TSRMLS_DC)
{
	zend_bool ret = FALSE;
	MYSQLND_MS_POOL_ENTRY _ms_p_zval * pool_element;
//	char first = GTID_RUNNING_MARKER;

	DBG_ENTER("mysqlnd_ms::pool_connection_exists");
	DBG_INF_FMT("pool=%p  hash_key=%p  is_master=%p  is_active=%p  is_removed=%p", pool, hash_key, is_master, is_active, is_removed);

	if (!pool) {
		DBG_INF("No pool!");
		DBG_RETURN(FAIL);
	}
	if (!hash_key) {
		DBG_INF("No hashkey!");
		DBG_RETURN(FAIL);
	}
	if (!is_master || !is_active || !is_removed) {
		DBG_INF("Wrong is_master or is_active or is_removed!");
		DBG_RETURN(FAIL);	
	}
	// TERRIBLE HACK
/*	if (hash_key->c && hash_key->len - 1 && hash_key->c[0] != GTID_RUNNING_MARKER) {
		first = hash_key->c[0];
		hash_key->c[0] = GTID_RUNNING_MARKER;
	}  */
	if (SUCCESS == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, &(pool->data.master_list), hash_key->c, hash_key->len - 1, pool_element)) {
		*is_master = TRUE;
		*is_active = (_ms_p_zval pool_element)->active;
		*is_removed = (_ms_p_zval pool_element)->removed;
		*data = (_ms_p_zval pool_element)->data;
		ret = TRUE;
		DBG_INF_FMT("element=%p list=%p is_master=%d", _ms_p_zval pool_element, &(pool->data.master_list), *is_master);
	} else if (SUCCESS == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, &(pool->data.slave_list), hash_key->c, hash_key->len - 1, pool_element)) {
		*is_master = FALSE;
		*is_active = (_ms_p_zval pool_element)->active;
		*is_removed = (_ms_p_zval pool_element)->removed;
		*data = (_ms_p_zval pool_element)->data;
		DBG_INF_FMT("element=%p list=%p is_master=%d", _ms_p_zval pool_element, &(pool->data.slave_list), *is_master);
		ret = TRUE;
	}
/*	if (first != GTID_RUNNING_MARKER) {
		hash_key->c[0] = first;
	}
*/
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ pool_connection_reactivate */
static enum_func_status
pool_connection_reactivate(MYSQLND_MS_POOL * pool, _ms_smart_type * hash_key, zend_bool is_master TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	MYSQLND_MS_POOL_ENTRY _ms_p_zval * pool_element;
	enum_mysqlnd_ms_collected_stats stat;
	HashTable * ht;
	zend_llist * list;

	DBG_ENTER("mysqlnd_ms::pool_connection_reactivate");
	DBG_INF_FMT("pool=%p", pool);

	if (!pool) {
		DBG_INF("No pool!");
		DBG_RETURN(FAIL);
	}
	if (!hash_key) {
		DBG_INF("No hashkey!");
		DBG_RETURN(FAIL);	
	}

	ht = (is_master) ? &(pool->data.master_list) : &(pool->data.slave_list);
	list = (is_master) ? &(pool->data.active_master_list) : &(pool->data.active_slave_list);
	stat = (is_master) ? MS_STAT_POOL_MASTERS_ACTIVE : MS_STAT_POOL_SLAVES_ACTIVE;

	if (SUCCESS == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, ht, hash_key->c, hash_key->len - 1, pool_element)) {
		DBG_INF_FMT("element=%p is_active=%d is_removed=%d", _ms_p_zval pool_element, (_ms_p_zval pool_element)->active, (_ms_p_zval pool_element)->removed);
		if (!(_ms_p_zval pool_element)->active && !(_ms_p_zval pool_element)->removed) {
			ret = PASS;
			(_ms_p_zval pool_element)->active = TRUE;
			(_ms_p_zval pool_element)->activation_counter++;

			zend_llist_add_element(list, &(_ms_p_zval pool_element)->data);
			DBG_INF_FMT("reactivating list=%p, list_count=%d, is_master=%d", _ms_p_zval pool_element, list, zend_llist_count(list), is_master);
			MYSQLND_MS_INC_STATISTIC_W_VALUE(stat, zend_llist_count(list));
		}
	} else {
		DBG_INF_FMT("not found");
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ pool_register_replace_listener */
static enum_func_status
pool_connection_remove(MYSQLND_MS_POOL * pool, _ms_smart_type * hash_key, zend_bool is_master TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	MYSQLND_MS_POOL_ENTRY _ms_p_zval * pool_element;
	HashTable * ht;

	DBG_ENTER("mysqlnd_ms::pool_connection_remove");
	DBG_INF_FMT("pool=%p", pool);

	if (!pool) {
		DBG_INF("No pool!");
		DBG_RETURN(FAIL);
	}
	/* The compiler will optimize this in release build as logging will be away */
	if (!hash_key) {
		DBG_INF("No hashkey!");
		DBG_RETURN(FAIL);	
	}

	ht = (is_master) ? &(pool->data.master_list) : &(pool->data.slave_list);

	if (SUCCESS == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, ht, hash_key->c, hash_key->len - 1, pool_element)) {
		DBG_INF_FMT("element=%p is_active=%d is_removed=%d ref_counter=%d", _ms_p_zval pool_element, (_ms_p_zval pool_element)->active, (_ms_p_zval pool_element)->removed, (_ms_p_zval pool_element)->ref_counter);
		if (!(_ms_p_zval pool_element)->active && !(_ms_p_zval pool_element)->removed) {
			if (1 == (_ms_p_zval pool_element)->ref_counter) {
				ret = (SUCCESS == zend_hash_str_del(ht, hash_key->c, hash_key->len)) ? PASS : FAIL;
			} else {
				ret = PASS;
				(_ms_p_zval pool_element)->removed = TRUE;
			}
		}
	} else {
		DBG_INF_FMT("not found");
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ pool_register_replace_listener */
static enum_func_status
pool_register_replace_listener(MYSQLND_MS_POOL * pool, func_pool_replace_listener listener, void * data TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_POOL_LISTENER entry;

	DBG_ENTER("mysqlnd_ms::pool_register_replace_listener");

	if (!pool) {
		DBG_INF("No pool!");
		DBG_RETURN(FAIL);
	}
	if (!listener) {
		DBG_INF("NULL listener!");
		DBG_RETURN(FAIL);
	}

	entry.listener = listener;
	entry.data = data;

	zend_llist_add_element(&(pool->data.replace_listener), &entry);

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ pool_notify_replace_listener */
static enum_func_status
pool_notify_replace_listener(MYSQLND_MS_POOL * pool TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_POOL_LISTENER * entry_p;
	zend_llist_position	pos;

	DBG_ENTER("mysqlnd_ms::pool_notify_replace_listener");

	if (!pool) {
		DBG_INF("No pool!");
		DBG_RETURN(FAIL);
	}

	for (
			entry_p = (MYSQLND_MS_POOL_LISTENER *) zend_llist_get_first_ex(&(pool->data.replace_listener), &pos);
			entry_p;
			entry_p = (MYSQLND_MS_POOL_LISTENER *) zend_llist_get_next_ex(&(pool->data.replace_listener), &pos)
		)
	{
		entry_p->listener(pool, entry_p->data TSRMLS_CC);
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ pool_add_reference */
static enum_func_status
pool_add_reference(MYSQLND_MS_POOL * pool, MYSQLND_CONN_DATA * const conn TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	MYSQLND_MS_POOL_ENTRY _ms_p_zval * pool_element;
	HashTable * ht[2];
	unsigned int i;
	DBG_ENTER("mysqlnd_ms::pool_add_reference");

	if (!pool) {
		DBG_INF("No pool given!");
		DBG_RETURN(FAIL);
	}
	if (!conn) {
		DBG_INF("No conn given!");
		DBG_RETURN(FAIL);
	}

	ht[0] = &(pool->data.master_list);
	ht[1] = &(pool->data.slave_list);

	for (i = 0; i < 2; i++) {
		/* XXX: Do we need to use the internal hash pointer instead of external one? */
		for (zend_hash_internal_pointer_reset(ht[i]);
			 	(zend_hash_has_more_elements(ht[i]) == SUCCESS) && (_MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data_ptr, ht[i], pool_element) == SUCCESS);
				zend_hash_move_forward(ht[i])
			)
		{
			if ((_ms_p_zval pool_element)->data->conn == conn) {
				(_ms_p_zval pool_element)->ref_counter++;
				DBG_INF_FMT("%s ref_counter=%d", (0 == i) ? "master" : "slave", (_ms_p_zval pool_element)->ref_counter);
				ret = SUCCESS;
				DBG_RETURN(ret);
			}
		}
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ pool_free_reference */
static enum_func_status
pool_free_reference(MYSQLND_MS_POOL * pool, MYSQLND_CONN_DATA * const conn TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	MYSQLND_MS_POOL_ENTRY _ms_p_zval * pool_element;
	HashTable * ht[2];
	unsigned int i;
	DBG_ENTER("mysqlnd_ms::pool_free_reference");

	if (!pool) {
		DBG_INF("No pool given!");
		DBG_RETURN(FAIL);
	}
	if (!conn) {
		DBG_INF("No conn given!");
		DBG_RETURN(FAIL);
	}

	ht[0] = &(pool->data.master_list);
	ht[1] = &(pool->data.slave_list);

	for (i = 0; i < 2; i++) {
		/* XXX: Do we need to use the internal hash pointer instead of external one? */
		for (
				zend_hash_internal_pointer_reset(ht[i]);
			 	(zend_hash_has_more_elements(ht[i]) == SUCCESS) && (_MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data_ptr, ht[i], pool_element) == SUCCESS);
				zend_hash_move_forward(ht[i])
			)
		{
			ret = PASS;
			if ((_ms_p_zval pool_element)->ref_counter > 1) {
				(_ms_p_zval pool_element)->ref_counter--;
				DBG_INF_FMT("%s ref_counter=%d", (0 == i) ? "master" : "slave", (_ms_p_zval pool_element)->ref_counter);
			} else {
				DBG_INF_FMT("%s double free", (0 == i) ? "master" : "slave");
			}
			DBG_RETURN(ret);
		}
	}
	DBG_RETURN(ret);
}
/* }}} */


/* private */
/* {{{ pool_get_buffered_entry */
static enum_func_status
pool_get_buffered_entry(MYSQLND_MS_POOL * pool,
						enum_mysqlnd_pool_cmd cmd,
						void ** cmd_data, size_t cmd_data_size,
						zend_bool (*comparator)(const MYSQLND_MS_POOL_CMD * pool_cmd, void * arg1, void * arg2 TSRMLS_DC),
						void * cmp_arg1,
						void * cmp_arg2
						TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_POOL_CMD * pool_cmd, ** pool_cmd_pp;
	zend_llist_position pos;
	DBG_ENTER("mysqlnd_ms::pool_get_buffered_entry");

	if (!pool) {
		DBG_INF("No pool given!");
		DBG_RETURN(FAIL);
	}

	*cmd_data = NULL;
	for (
			pool_cmd_pp = (MYSQLND_MS_POOL_CMD **) zend_llist_get_first_ex(&(pool->data.buffered_cmds), &pos);
			pool_cmd_pp && (pool_cmd = *pool_cmd_pp) && pool_cmd->data;
			pool_cmd_pp = (MYSQLND_MS_POOL_CMD **) zend_llist_get_next_ex(&(pool->data.buffered_cmds), &pos)
		)
	{
		if (comparator) {
			if (TRUE == comparator(pool_cmd, cmp_arg1, cmp_arg2 TSRMLS_CC)) {
				cmd_data = (void**)pool_cmd_pp;
				break;
			}
		} else if (pool_cmd->cmd == cmd) {
			cmd_data = (void**)pool_cmd_pp;
			break;
		}
	}

	if (!(*cmd_data)) {

		pool_cmd = mnd_pecalloc(1, sizeof(MYSQLND_MS_POOL_CMD), pool->data.persistent);
		if (!pool_cmd) {
			MYSQLND_MS_WARN_OOM();
			ret = FAIL;
			DBG_RETURN(ret);
		}

		*cmd_data = mnd_pecalloc(1, cmd_data_size, pool->data.persistent);
		if (!*cmd_data) {
			MYSQLND_MS_WARN_OOM();
			ret = FAIL;
			mnd_pefree(pool_cmd, pool->data.persistent);
			DBG_RETURN(ret);
		}

		pool_cmd->cmd = cmd;
		pool_cmd->data = *cmd_data;
		pool_cmd->persistent = pool->data.persistent;
		DBG_INF_FMT("new pool_cmd=%p pool_cmd.data=%p", pool_cmd, pool_cmd->data);

		zend_llist_add_element(&(pool->data.buffered_cmds), &pool_cmd);
		ret = PASS;
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ pool_dispatch_select_db */
static enum_func_status
pool_dispatch_select_db(MYSQLND_MS_POOL * pool, func_mysqlnd_conn_data__select_db cb,
						const char * const db, const size_t db_len TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_POOL_CMD_SELECT_DB  * cmd_data;

	DBG_ENTER("mysqlnd_ms::pool_dispatch_select_db");

	if (!pool) {
		DBG_INF("No pool given!");
		DBG_RETURN(FAIL);
	}
	if (!cb) {
		DBG_INF("No select_db callback!");
		DBG_RETURN(FAIL);
	}

	if (pool->data.in_buffered_replay) {
		DBG_RETURN(ret);
	}

	ret = pool_get_buffered_entry(pool, POOL_CMD_SELECT_DB, (void *)&cmd_data, sizeof(MYSQLND_MS_POOL_CMD_SELECT_DB),
								  NULL /*cmp*/, NULL, NULL TSRMLS_CC);
	if ((PASS == ret) && cmd_data) {
		if (cmd_data->db) {
			DBG_INF_FMT("free db=%s\n", cmd_data->db);
			mnd_pefree(cmd_data->db, pool->data.persistent);
		}
		cmd_data->cb = cb;
		cmd_data->db = (db) ? mnd_pestrndup(db, db_len, pool->data.persistent) : NULL;
		cmd_data->db_len = db_len;
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ pool_dispatch_set_charset */
static enum_func_status
pool_dispatch_set_charset(MYSQLND_MS_POOL * pool, func_mysqlnd_conn_data__set_charset cb,
						  const char * const csname TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_POOL_CMD_SET_CHARSET * cmd_data;

	DBG_ENTER("mysqlnd_ms::pool_dispatch_set_charset");
	if (!pool) {
		DBG_INF("No pool given!");
		DBG_RETURN(FAIL);
	}
	if (!cb) {
		DBG_INF("No set_charset callback!");
		DBG_RETURN(FAIL);
	}

	if (pool->data.in_buffered_replay) {
		DBG_RETURN(ret);
	}

	ret = pool_get_buffered_entry(pool, POOL_CMD_SELECT_DB, (void *)&cmd_data, sizeof(MYSQLND_MS_POOL_CMD_SET_CHARSET),
								  NULL /*cmp*/, NULL, NULL TSRMLS_CC);
	if ((PASS == ret) && cmd_data) {
		if (cmd_data->csname) {
			DBG_INF_FMT("free csname=%s\n", cmd_data->csname);
			mnd_pefree(cmd_data->csname, pool->data.persistent);
		}
		cmd_data->cb = cb;
		cmd_data->csname = (csname) ? mnd_pestrndup(csname, strlen(csname), pool->data.persistent) : NULL;
	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ buffer_search_server_option */
static
zend_bool buffer_search_server_option(const MYSQLND_MS_POOL_CMD * const pool_cmd, void * arg1, void * arg2 TSRMLS_DC)
{
	MYSQLND_MS_POOL_CMD_SET_SERVER_OPTION * cmd_data;

	if (!pool_cmd) {
		return FALSE;
	}

	cmd_data = (MYSQLND_MS_POOL_CMD_SET_SERVER_OPTION *) pool_cmd->data;

	if ((pool_cmd->cmd == POOL_CMD_SET_SERVER_OPTION) && (cmd_data->option == *(enum_mysqlnd_server_option *)arg1)) {
		return TRUE;
	}
	return FALSE;
}
/* }}} */


/* {{{ pool_dispatch_set_server_option */
static enum_func_status
pool_dispatch_set_server_option(MYSQLND_MS_POOL * pool, func_mysqlnd_conn_data__set_server_option cb,
								enum_mysqlnd_server_option option TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_POOL_CMD_SET_SERVER_OPTION * cmd_data;

	DBG_ENTER("mysqlnd_ms::pool_dispatch_set_server_option");

	if (!pool) {
		DBG_INF("No pool given!");
		DBG_RETURN(FAIL);
	}
	if (!cb) {
		DBG_INF("No set_server_option callback!");
		DBG_RETURN(FAIL);
	}

	if (pool->data.in_buffered_replay) {
		DBG_RETURN(ret);
	}

	ret = pool_get_buffered_entry(pool, POOL_CMD_SET_SERVER_OPTION, (void *)&cmd_data,
									sizeof(MYSQLND_MS_POOL_CMD_SET_SERVER_OPTION),
									buffer_search_server_option,
									(void*)&option,
									NULL  TSRMLS_CC);
	if ((PASS == ret) && cmd_data) {
		cmd_data->cb = cb;
		cmd_data->option = option;
	}

	DBG_RETURN(ret);
}
/* }}} */

static
zend_bool pool_client_option_is_char(enum_mysqlnd_client_option option) {

	zend_bool ret = TRUE;

	switch (option) {
		/* WARNING: this list must be kept in sync with mysqlnd */
		case MYSQLND_OPT_NET_CMD_BUFFER_SIZE:
		case MYSQLND_OPT_NET_READ_BUFFER_SIZE:
		case MYSQL_OPT_READ_TIMEOUT:
		case MYSQL_OPT_WRITE_TIMEOUT:
		case MYSQL_OPT_CONNECT_TIMEOUT:
#ifdef MYSQLND_STRING_TO_INT_CONVERSION
		case MYSQLND_OPT_INT_AND_FLOAT_NATIVE:
#endif
		case MYSQL_OPT_LOCAL_INFILE:
		case MYSQL_OPT_PROTOCOL:
		case MYSQLND_OPT_MAX_ALLOWED_PACKET:
		case MYSQL_OPT_CAN_HANDLE_EXPIRED_PASSWORDS:
			ret = FALSE;
			break;
		default:
			break;
	}

	return ret;
}

/* {{{ buffer_search_client_option */
static
zend_bool buffer_search_client_option(const MYSQLND_MS_POOL_CMD * pool_cmd, void * arg1, void * arg2 TSRMLS_DC)
{
	MYSQLND_MS_POOL_CMD_SET_CLIENT_OPTION  * cmd_data;
	DBG_ENTER("buffer_search_client_option");

	if (!pool_cmd) {
		DBG_RETURN(FALSE);
	}

	cmd_data = (MYSQLND_MS_POOL_CMD_SET_CLIENT_OPTION  *)pool_cmd->data;
	if ((pool_cmd->cmd == POOL_CMD_SET_CLIENT_OPTION) &&
		(cmd_data->option == *(enum_mysqlnd_client_option *)arg1)) {

		/*
		* FIXME: We keep the buffered commands in a list.
		* Each set_charset(), set_*_option() call has to decide whether it wants to
		* add an entry to the buffered command list or not.
		* *
		* Assume the user does:
		*   option(INIT_COMMAND, "something");
		*   option(INIT_COMMAND, "else");
		*
		* We certainly want to keep commands and replay both if requested.
		*
		* Assume the user does:
		*   option(LOAD_DATA, 1);
		*   option(LOAD_DATA, 0);
		*
		* Here we do not care about the first one. The below code will cause
		* both LOAD_DATA entries to be stored because 1 != 0. I think, that's
		* wrong. It should not harm as the order of commands should be preserved
		* during replay but it's unnecessary work.
		*/
		if (TRUE == pool_client_option_is_char(cmd_data->option)) {
			if (
				(
					(arg1 && cmd_data->value) && (0 == strcmp(cmd_data->value,(char*)arg2))
				)
				||
				(!arg1 && !cmd_data->value)
			   )
			{
				DBG_INF_FMT("Found data=%p value=%s", cmd_data, (cmd_data->value) ? cmd_data->value : "n/a");
				DBG_RETURN(TRUE);
			}
		} else {
			if (cmd_data->value_int == *(unsigned int*)arg2) {
				DBG_INF_FMT("Found data=%p value_int=%lu", cmd_data, cmd_data->value_int);
				DBG_RETURN(TRUE);
			}
		}
	}
	DBG_RETURN(FALSE);
}
/* }}} */


/* {{{ pool_dispatch_set_client_option */
static enum_func_status
pool_dispatch_set_client_option(MYSQLND_MS_POOL * pool, func_mysqlnd_conn_data__set_client_option cb,
		enum_mysqlnd_client_option option,	const char * const value TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_POOL_CMD_SET_CLIENT_OPTION  * cmd_data;

	DBG_ENTER("mysqlnd_ms::pool_dispatch_set_client_option");

	if (!pool) {
		DBG_INF("No pool given!");
		DBG_RETURN(FAIL);
	}
	if (!cb) {
		DBG_INF("No set_server_option callback!");
		DBG_RETURN(FAIL);
	}

	if (pool->data.in_buffered_replay) {
		DBG_RETURN(ret);
	}

	ret = pool_get_buffered_entry(pool, POOL_CMD_SET_CLIENT_OPTION, (void *)&cmd_data,
									sizeof(MYSQLND_MS_POOL_CMD_SET_CLIENT_OPTION),
									buffer_search_client_option,
									(void *)&option,
									(void *)value
									TSRMLS_CC);

	if ((PASS == ret) && cmd_data) {
		if (cmd_data->value) {
			DBG_INF_FMT("free value=%s\n", cmd_data->value);
			mnd_pefree(cmd_data->value, pool->data.persistent);
		}

		cmd_data->cb = cb;
		cmd_data->option = option;

		if (pool_client_option_is_char(cmd_data->option)) {
			cmd_data->value = (value) ? mnd_pestrndup(value, strlen(value), pool->data.persistent) : NULL;
		} else {
			cmd_data->value = NULL;
			cmd_data->value_int = *(unsigned int*)value;
		}

	}

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ pool_dispatch_change_user */
static enum_func_status
pool_dispatch_change_user(MYSQLND_MS_POOL * pool, func_mysqlnd_conn_data__change_user cb,
						const char * const user,
						const char * const passwd,
						const char * const db,
						const zend_bool silent
#if PHP_VERSION_ID >= 50399
						, const size_t passwd_len
#endif
						TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_POOL_CMD_CHANGE_USER  * cmd_data;

	DBG_ENTER("mysqlnd_ms::pool_dispatch_change_user");

	if (!pool) {
		DBG_INF("No pool given!");
		DBG_RETURN(FAIL);
	}
	if (!cb) {
		DBG_INF("No change_user callback!");
		DBG_RETURN(FAIL);
	}

	if (pool->data.in_buffered_replay) {
		DBG_RETURN(ret);
	}

	ret = pool_get_buffered_entry(pool, POOL_CMD_CHANGE_USER,
								  (void *)&cmd_data, sizeof(MYSQLND_MS_POOL_CMD_CHANGE_USER),
								  NULL, NULL, NULL TSRMLS_CC);
	if ((PASS == ret) && cmd_data) {
		if (cmd_data->user) {
			mnd_pefree(cmd_data->user, pool->data.persistent);
		}
		if (cmd_data->passwd) {
			mnd_pefree(cmd_data->passwd, pool->data.persistent);
		}
		if (cmd_data->db) {
			mnd_pefree(cmd_data->db, pool->data.persistent);
		}
		cmd_data->cb = cb;
		cmd_data->user = (user) ? mnd_pestrndup(user, strlen(user), pool->data.persistent) : NULL;
		cmd_data->db = (db) ? mnd_pestrndup(db, strlen(db), pool->data.persistent) : NULL;
		cmd_data->silent = silent;

#if PHP_VERSION_ID >= 50399
		cmd_data->passwd = (passwd) ? mnd_pestrndup(passwd, passwd_len, pool->data.persistent) : NULL;
		cmd_data->passwd_len = passwd_len;
#else
		cmd_data->passwd = (passwd) ? mnd_pestrndup(passwd, strlen(passwd), pool->data.persistent) : NULL;
#endif
	}


	DBG_RETURN(ret);
}
/* }}} */

#if MYSQLND_VERSION_ID >= 50009
/* {{{ pool_dispatch_set_autocommit */
static enum_func_status
pool_dispatch_set_autocommit(MYSQLND_MS_POOL * pool,
							func_mysqlnd_conn_data__set_autocommit cb,
							unsigned int mode TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_POOL_CMD_SET_AUTOCOMMIT  * cmd_data;

	DBG_ENTER("mysqlnd_ms::pool_dispatch_set_autocommit");
	if (!pool) {
		DBG_INF("No pool given!");
		DBG_RETURN(FAIL);
	}
	if (!cb) {
		DBG_INF("No set_autocommit callback!");
		DBG_RETURN(FAIL);
	}

	if (pool->data.in_buffered_replay) {
		DBG_RETURN(ret);
	}

	ret = pool_get_buffered_entry(pool, POOL_CMD_SET_AUTOCOMMIT,
								  (void *)&cmd_data, sizeof(MYSQLND_MS_POOL_CMD_SET_AUTOCOMMIT),
								  NULL, NULL, NULL TSRMLS_CC);
	if ((PASS == ret) && cmd_data) {
		cmd_data->cb = cb;
		cmd_data->mode = mode;
	}

	DBG_RETURN(ret);
}
/* }}} */
#endif

/* {{{ pool_dispatch_ssl_set */
static enum_func_status
pool_dispatch_ssl_set(MYSQLND_MS_POOL * pool, func_mysqlnd_conn_data__ssl_set cb,
					  const char * const key,
					  const char * const cert,
					  const char * const ca,
					  const char * const capath,
					  const char * const cipher
					  TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_POOL_CMD_SSL_SET  * cmd_data;

	DBG_ENTER("mysqlnd_ms::pool_dispatch_set_ssl");
	if (!pool) {
		DBG_INF("No pool given!");
		DBG_RETURN(FAIL);
	}
	if (!cb) {
		DBG_INF("No ssl_set callback!");
		DBG_RETURN(FAIL);
	}

	if (pool->data.in_buffered_replay) {
		DBG_RETURN(ret);
	}

	ret = pool_get_buffered_entry(pool, POOL_CMD_SSL_SET, (void *)&cmd_data,
								  sizeof(MYSQLND_MS_POOL_CMD_SSL_SET),
								  NULL, NULL, NULL TSRMLS_CC);
	if ((PASS == ret) && cmd_data) {
		if (cmd_data->key) {
			mnd_pefree(cmd_data->key, pool->data.persistent);
		}
		if (cmd_data->cert) {
			mnd_pefree(cmd_data->cert, pool->data.persistent);
		}
		if (cmd_data->ca) {
			mnd_pefree(cmd_data->ca, pool->data.persistent);
		}
		if (cmd_data->capath) {
			mnd_pefree(cmd_data->capath, pool->data.persistent);
		}
		if (cmd_data->cipher) {
			mnd_pefree(cmd_data->cipher, pool->data.persistent);
		}

		cmd_data->cb = cb;
		cmd_data->key = (key) ? mnd_pestrndup(key, strlen(key), pool->data.persistent) : NULL;
		cmd_data->cert = (cert) ? mnd_pestrndup(cert, strlen(cert), pool->data.persistent) : NULL;
		cmd_data->ca = (ca) ? mnd_pestrndup(ca, strlen(ca), pool->data.persistent) : NULL;
		cmd_data->capath = (capath) ? mnd_pestrndup(capath, strlen(capath), pool->data.persistent) : NULL;
		cmd_data->cipher = (cipher) ? mnd_pestrndup(cipher, strlen(cipher), pool->data.persistent) : NULL;
	}

	DBG_RETURN(ret);
}
/* }}} */

/* {{{ pool_replay_cmds */
static enum_func_status
pool_replay_cmds(MYSQLND_MS_POOL * pool, MYSQLND_CONN_DATA * const proxy_conn,
				 func_replay_filter cb TSRMLS_DC)
{
	enum_func_status ret = PASS;
	MYSQLND_MS_POOL_CMD * pool_cmd, ** pool_cmd_pp;
	zend_llist_position pos;

	DBG_ENTER("mysqlnd_ms::pool_replay_cmds");
	DBG_INF_FMT("pool=%p", pool);
	pool->data.in_buffered_replay = TRUE;

	for (pool_cmd_pp = (MYSQLND_MS_POOL_CMD **) zend_llist_get_first_ex(&(pool->data.buffered_cmds), &pos) ;
			pool_cmd_pp && (pool_cmd = *pool_cmd_pp) && pool_cmd->data ;
			pool_cmd_pp = (MYSQLND_MS_POOL_CMD **) zend_llist_get_next_ex(&(pool->data.buffered_cmds), &pos)) {

		DBG_INF_FMT("pool_cmd=%p pool_cmd.data=%p", pool_cmd, pool_cmd->data);

		if (cb && (FALSE == cb(proxy_conn, pool_cmd TSRMLS_CC))) {
			DBG_INF("skipping");
			continue;
		}

		switch (pool_cmd->cmd) {
			case POOL_CMD_SELECT_DB:
				{
					MYSQLND_MS_POOL_CMD_SELECT_DB * args = (MYSQLND_MS_POOL_CMD_SELECT_DB *)pool_cmd->data;
					ret = args->cb(proxy_conn, args->db, args->db_len TSRMLS_CC);
				}
				break;

			case POOL_CMD_SET_CHARSET:
				{
					MYSQLND_MS_POOL_CMD_SET_CHARSET * args = (MYSQLND_MS_POOL_CMD_SET_CHARSET *)pool_cmd->data;
					ret = args->cb(proxy_conn, args->csname TSRMLS_CC);
				}
				break;

			case POOL_CMD_SET_SERVER_OPTION:
				{
					MYSQLND_MS_POOL_CMD_SET_SERVER_OPTION * args = (MYSQLND_MS_POOL_CMD_SET_SERVER_OPTION *)pool_cmd->data;
					ret = args->cb(proxy_conn, args->option TSRMLS_CC);
				}
				break;

			case POOL_CMD_SET_CLIENT_OPTION:
				{
					MYSQLND_MS_POOL_CMD_SET_CLIENT_OPTION * args = (MYSQLND_MS_POOL_CMD_SET_CLIENT_OPTION *)pool_cmd->data;
					if (pool_client_option_is_char(args->option)) {
						ret = args->cb(proxy_conn, args->option, args->value TSRMLS_CC);
					} else {
						ret = args->cb(proxy_conn, args->option, (const char * const)&args->value_int TSRMLS_CC);
					}
				}
				break;

			case POOL_CMD_CHANGE_USER:
				{
					MYSQLND_MS_POOL_CMD_CHANGE_USER * args = (MYSQLND_MS_POOL_CMD_CHANGE_USER *)pool_cmd->data;
					ret = args->cb(proxy_conn, args->user, args->passwd, args->db, args->silent
#if PHP_VERSION_ID >= 50399
								, args->passwd_len
#endif
								TSRMLS_CC);
				}
				break;

#if MYSQLND_VERSION_ID >= 50009
			case POOL_CMD_SET_AUTOCOMMIT:
				{
					MYSQLND_MS_POOL_CMD_SET_AUTOCOMMIT * args = (MYSQLND_MS_POOL_CMD_SET_AUTOCOMMIT *)pool_cmd->data;
					ret = args->cb(proxy_conn, args->mode TSRMLS_CC);
				}
				break;
#endif
			case POOL_CMD_SSL_SET:
				{
					MYSQLND_MS_POOL_CMD_SSL_SET * args = (MYSQLND_MS_POOL_CMD_SSL_SET *)pool_cmd->data;
					ret = args->cb(proxy_conn, args->key, args->cert, args->ca, args->capath, args->cipher TSRMLS_CC);
				}
				break;

			default:
				DBG_INF("Unknown command");
				break;
		}


	}
	pool->data.in_buffered_replay = FALSE;

	DBG_RETURN(ret);
}
/* }}} */


/* {{{ pool_get_active_masters */
static zend_llist *
pool_get_active_masters(MYSQLND_MS_POOL * pool TSRMLS_DC)
{
	zend_llist * ret = &((pool->data).active_master_list);
	return ret;
}
/* }}} */


/* {{{ pool_get_active_slaves */
static zend_llist *
pool_get_active_slaves(MYSQLND_MS_POOL * pool TSRMLS_DC)
{
	zend_llist * ret = &((pool->data).active_slave_list);
	return ret;
}
/* }}} */

static void
pool_dtor(MYSQLND_MS_POOL * pool TSRMLS_DC) {
	DBG_ENTER("mysqlnd_ms::pool_dtor");

	if (pool) {
		zend_llist_clean(&(pool->data.replace_listener));

		zend_llist_clean(&(pool->data.active_master_list));
		zend_llist_clean(&(pool->data.active_slave_list));

		zend_hash_destroy(&(pool->data.master_list));
		zend_hash_destroy(&(pool->data.slave_list));

		zend_llist_clean(&(pool->data.buffered_cmds));

		mnd_pefree(pool, pool->data.persistent);
	}
	DBG_VOID_RETURN;
}


MYSQLND_MS_POOL * mysqlnd_ms_pool_ctor(llist_dtor_func_t ms_list_data_dtor, zend_bool persistent TSRMLS_DC) {
	DBG_ENTER("mysqlnd_ms::pool_ctor");

	MYSQLND_MS_POOL * pool = mnd_pecalloc(1, sizeof(MYSQLND_MS_POOL), persistent);
	if (pool) {

		/* methods */
		pool->flush_active = pool_flush_active;
		pool->clear_all = pool_clear_all;

		pool->init_pool_hash_key = pool_init_pool_hash_key;
		pool->get_conn_hash_key = pool_get_conn_hash_key;
		pool->add_slave = pool_add_slave;
		pool->add_master = pool_add_master;
		pool->connection_exists = pool_connection_exists;
		pool->connection_reactivate = pool_connection_reactivate;
		pool->connection_remove = pool_connection_remove;

		pool->register_replace_listener = pool_register_replace_listener;
		pool->notify_replace_listener = pool_notify_replace_listener;

		pool->get_active_masters = pool_get_active_masters;
		pool->get_active_slaves = pool_get_active_slaves;

		pool->add_reference = pool_add_reference;
		pool->free_reference = pool_free_reference;

		pool->dispatch_select_db = pool_dispatch_select_db;
		pool->dispatch_set_charset = pool_dispatch_set_charset;
		pool->dispatch_set_server_option = pool_dispatch_set_server_option;
		pool->dispatch_set_client_option = pool_dispatch_set_client_option;
		pool->dispatch_change_user = pool_dispatch_change_user;
#if MYSQLND_VERSION_ID >= 50009
		pool->dispatch_set_autocommit = pool_dispatch_set_autocommit;
#endif
		pool->dispatch_ssl_set = pool_dispatch_ssl_set;
		pool->replay_cmds = pool_replay_cmds;

		pool->dtor = pool_dtor;

		/* data */
		pool->data.persistent = persistent;

		zend_llist_init(&(pool->data.replace_listener), sizeof(MYSQLND_MS_POOL_LISTENER), NULL, persistent);

		/* Our own dtor's will call the MS provided dtor on demand */
		pool->data.ms_list_data_dtor = ms_list_data_dtor;

		zend_hash_init(&(pool->data.master_list), 4, NULL, mysqlnd_ms_pool_all_list_dtor, persistent);
		zend_hash_init(&(pool->data.slave_list), 4, NULL, mysqlnd_ms_pool_all_list_dtor, persistent);

		zend_llist_init(&(pool->data.active_master_list), sizeof(MYSQLND_MS_LIST_DATA *), NULL, persistent);
		zend_llist_init(&(pool->data.active_slave_list), sizeof(MYSQLND_MS_LIST_DATA *), NULL, persistent);

		zend_llist_init(&(pool->data.buffered_cmds), sizeof(MYSQLND_MS_POOL_CMD *), mysqlnd_ms_pool_cmd_dtor, persistent);
	}

	DBG_RETURN(pool);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
