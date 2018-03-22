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

/* $Id: mysqlnd_ms.h 311091 2011-05-16 15:42:48Z andrey $ */
#ifndef MYSQLND_MS_ENUM_N_DEF_H
#define MYSQLND_MS_ENUM_N_DEF_H

#ifdef PHP_WIN32
#include "win32/time.h"
#else
#include "sys/time.h"
#include <stddef.h>
#include <libmemcached/memcached.h>
#endif
#if PHP_MAJOR_VERSION < 7
#include "ext/standard/php_smart_str.h"
#else
#include "ext/standard/php_smart_string.h"
#endif

//#include "mysqlnd_ms.h"

#include "fabric/mysqlnd_fabric.h"

//BEGIN HACK
/*
#define zend_bool unsigned char
#define ulong unsigned long
#define uint unsigned int
#define uint64_t unsigned __int64
#define SUCCESS  0
#define FAILURE  -1
#define NULL ((void *)0)
#define SIZE_MAX ((size_t)-1)
*/
#if PHP_MAJOR_VERSION < 7
#define _MS_SEND_QUERY_A_EXT
#define _MS_SEND_QUERY_D_EXT
#define _MS_SEND_QUERY_AD_EXT

#define _MS_REAP_QUERY_A_EXT
#define _MS_REAP_QUERY_D_EXT
#define _MS_REAP_QUERY_AD_EXT

#define _ms_zval_dtor(zv) zval_ptr_dtor(&(zv))
#define _ms_zval_ptr_dtor(zv) zval_ptr_dtor(&(zv))
#define _ms_p_zval *
#define _ms_a_zval
#define _ms_hash_zval_type void
#define _ms_smart_type smart_str
#define _ms_smart_method(meth, ...) smart_str_##meth(__VA_ARGS__)
#define _ms_add_next_index_stringl(h, s, l) add_next_index_stringl(h, s, l, 1)

#define _MS_STR_D_P(str) char **str, unsigned int *str##_len
#define _MS_STR_A(str) str, str##_len
#define _MS_STR_DECL(str) char *str = NULL; unsigned int str##_len = 0
#define _MS_STR_L_FUNC_PS_1(func, arg1, str, ...) str##_len = func((arg1), &(str), __VA_ARGS__)
#define _MS_STR_L(str) str##_len
#define _MS_STR_S(str) str
#define _MS_STR(str) str
#define _MS_STR_PEFREE(str, persistent) pefree(str, persistent)
#define _MS_HASHSTR_GET_ZR_FUNC_PTR(func, ht, str, str_len, ptr) func((ht), (str), (str_len) + 1, (void **)&(ptr))
#define _MS_HASH_GET_ZR_FUNC_PTR_1(func, ht, arg1, ptr) func((ht), (arg1), (void **)&(ptr))
#define _MS_HASH_GET_ZR_FUNC_PTR(func, ht, ptr) func((ht), (void **)&(ptr))
#define _MS_HASH_GET_ZR_FUNC_PTR_VA(func, ht, ptr, ...) func((ht), (void **)&(ptr), __VA_ARGS__)
#define _MS_HASHSTR_SET_ZR_FUNC_PTR(func, ht, str, str_len, ptr) func((ht), (str), (str_len) + 1, &(ptr), sizeof(ptr), NULL)
#define _MS_HASH_SET_ZR_FUNC_PTR_1(func, ht, arg1, ptr) func((ht), (arg1), &(ptr), sizeof(ptr), NULL)

#define _MS_ZVAL_STRINGL(zv, str, str_len) ZVAL_STRINGL(zv, str, str_len, 1)
#define _MS_ZVAL_STRING(zv, str) ZVAL_STRING(zv, str, 1)

#define _MS_RETURN_ZSTR(str, persistent) return (str && str##_len ? zend_string_init(str, str##_len, persistent) : NULL)

#define _MS_HASH_Z_PTR_P(pDest) pDest

#define _MS_HASH_PERSISTENT(ht) (ht->persistent)

#define ZVAL_STR(z, s) ZVAL_STRINGL(z, s->val, s->len, 0)

#define _MS_RETURN_STRING(s) RETURN_STRING(s, 1)

#define _ms_size_type unsigned int

static zend_always_inline int _ms_hash_str_get_current_key(const HashTable *ht, char **key, uint *key_len, ulong *num_index, HashPosition *pos)
{
	int ret = zend_hash_get_current_key_ex(ht, key, (uint *) key_len, num_index, 0/*dup*/, pos);
	if (ret == HASH_KEY_IS_STRING) {
		(*key_len)--;
	}
	return ret;
}

#define zend_hash_str_find_ptr  zend_hash_find
#define zend_hash_index_find_ptr zend_hash_index_find
#define zend_hash_str_add_ptr zend_hash_add
#define zend_hash_str_update_ptr zend_hash_update
#define zend_hash_index_update_ptr zend_hash_index_update
#define zend_hash_next_index_insert_ptr zend_hash_next_index_insert
#define zend_hash_get_current_data_ptr_ex zend_hash_get_current_data_ex
#define zend_hash_get_current_data_ptr zend_hash_get_current_data
#define zend_hash_str_del zend_hash_del
#define zend_hash_str_find zend_hash_find
#define zend_hash_sort_ex zend_hash_sort
#define zend_hash_str_exists zend_hash_exists

#define zend_string_release zend_string_free

struct _zend_string {
	char *val;
	size_t len;
	int persistent;
};

typedef struct _zend_string zend_string;

static zend_always_inline zend_string *zend_string_alloc(int persistent)
{
	/* single alloc, so free the buf, will also free the struct */
	zend_string *str = pemalloc(sizeof(zend_string), persistent);

	str->val = NULL;
	str->len = 0;
	return str;
}

static zend_always_inline zend_string *zend_string_init(char *str, size_t len, int persistent)
{
	/* single alloc, so free the buf, will also free the struct */
	zend_string *zstr = zend_string_alloc(persistent);

	zstr->val = str;
	zstr->len = len;
	return zstr;
}

static zend_always_inline void zend_string_free(zend_string *s)
{
	if (s->val)
		pefree(s->val, s->persistent);
	pefree(s, s->persistent);
}
#else
extern zval *php_get_session_var(zend_string *name);

#define _MS_SEND_QUERY_A_EXT , type, read_cb, err_cb
#define _MS_SEND_QUERY_D_EXT , enum_mysqlnd_send_query_type type, zval *read_cb, zval *err_cb
#define _MS_SEND_QUERY_AD_EXT , MYSQLND_SEND_QUERY_IMPLICIT, NULL, NULL

#define _MS_REAP_QUERY_A_EXT , type
#define _MS_REAP_QUERY_D_EXT , enum_mysqlnd_reap_result_type type
#define _MS_REAP_QUERY_AD_EXT , MYSQLND_REAP_RESULT_IMPLICIT

#define _ms_zval_dtor(zv) zval_dtor(&(zv))
#define _ms_zval_ptr_dtor(zv) zval_ptr_dtor((zv))
#define _ms_p_zval
#define _ms_a_zval &
#define _ms_hash_zval_type zval
#define _ms_smart_type smart_string
#define _ms_smart_method(meth, ...) smart_string_##meth(__VA_ARGS__)
#define _ms_add_next_index_stringl add_next_index_stringl

#define _MS_STR_D_P(str) zend_string **str
#define _MS_STR_A(str) str
#define _MS_STR_DECL(str) zend_string *str = NULL
#define _MS_STR_L_FUNC_PS_1(func, arg1, str, ...) str = func((arg1), __VA_ARGS__)
#define _MS_STR_L(str) str->len
#define _MS_STR_S(str) str->val
#define _MS_STR(str) str
#define _MS_STR_PEFREE(str, persistent) zend_string_free(str)
#define _MS_GET_ZR_FUNC_PTR_VA(func, ptr, ...) ((ptr = func(__VA_ARGS__)) == NULL ? FAILURE : SUCCESS)
#define _MS_HASHSTR_GET_ZR_FUNC_PTR(func, ht, str, str_len, ptr) _MS_GET_ZR_FUNC_PTR_VA(func, ptr, ht, str, str_len)
#define _MS_HASH_GET_ZR_FUNC_PTR_1(func, ht, arg1, ptr) _MS_GET_ZR_FUNC_PTR_VA(func, ptr, ht, arg1)
#define _MS_HASH_GET_ZR_FUNC_PTR(func, ht, ptr) _MS_GET_ZR_FUNC_PTR_VA(func, ptr, ht)
#define _MS_HASH_GET_ZR_FUNC_PTR_VA(func, ht, ptr, ...) _MS_GET_ZR_FUNC_PTR_VA(func, ptr, ht, __VA_ARGS__)
#define _MS_SET_ZR_FUNC_PTR_VA(func, ...) (func(__VA_ARGS__) == NULL ? FAILURE : SUCCESS)
#define _MS_HASHSTR_SET_ZR_FUNC_PTR(func, ht, str, str_len, ptr) _MS_SET_ZR_FUNC_PTR_VA(func, ht, str, str_len, ptr)
#define _MS_HASH_SET_ZR_FUNC_PTR_1(func, ht, arg1, ptr) _MS_SET_ZR_FUNC_PTR_VA(func, ht, arg1, ptr)
#define _MS_HASH_SET_ZR_FUNC_PTR(func, ht,ptr) _MS_SET_ZR_FUNC_PTR_VA(func, ht, ptr)

#define _MS_ZVAL_STRINGL(zv, str, str_len) ZVAL_STRINGL(zv, str, str_len)
#define _MS_ZVAL_STRING(zv, str) ZVAL_STRING(zv, str)

#define _MS_RETURN_ZSTR(str, persistent) return str

#define _MS_HASH_Z_PTR_P(pDest) Z_PTR_P(pDest)

#define _MS_HASH_PERSISTENT(ht) (ht->u.flags & HASH_FLAG_PERSISTENT)

#define ALLOC_INIT_ZVAL(z)
#define INIT_ZVAL(z);
#define MAKE_STD_ZVAL(z)
#define _MS_RETURN_STRING RETURN_STRING

#define _ms_size_type size_t

#define zend_uint uint32_t

static zend_always_inline int _ms_hash_str_get_current_key(HashTable *ht, char **key, size_t *key_len, ulong *num_index, HashPosition *pos)
{
	zend_string *zs_key = NULL;
	int ret = zend_hash_get_current_key_ex(ht, &zs_key, num_index, pos ? pos : &ht->nInternalPointer);
	if (zs_key && ret == HASH_KEY_IS_STRING) {
		*key = zs_key->val;
		*key_len = zs_key->len;
	}
	return ret;
}

#endif
#if PHP_VERSION_ID < 70100
#define _ms_p_ei *
#define _ms_a_ei

#define _ms_csize_type unsigned int

#define _ms_mysqlnd_error_info_init(error_info)
#define _ms_mysqlnd_error_info_free_contents(error_info)

#define enum_mysqlnd_client_option enum_mysqlnd_option

#define MYSQLND_MS_CONN_STRING(conn_str) (conn_str)
#define MYSQLND_MS_CONN_STRING_LEN(conn_str) conn_str##_len
#define MYSQLND_MS_CONN_D_STRINGL(conn_str) char *conn_str, unsigned int conn_str##_len
#define MYSQLND_MS_CONN_DV_STRINGL(conn_str) char *conn_str; unsigned int conn_str##_len
#define MYSQLND_MS_CONN_A_STRINGL(conn_str) conn_str, conn_str##_len
#define MYSQLND_MS_CONN_D_STRING(conn_str) char *conn_str
#define MYSQLND_MS_CONN_DV_STRING(conn_str) MYSQLND_MS_CONN_D_STRING(conn_str)
#define MYSQLND_MS_CONN_A_STRING(conn_str) conn_str

#define MYSQLND_MS_CONN_D_CSTRINGL(conn_str) const char *conn_str, unsigned int conn_str##_len
#define MYSQLND_MS_CONN_DV_CSTRINGL(conn_str) const char *conn_str; unsigned int conn_str##_len
#define MYSQLND_MS_CONN_A_CSTRINGL MYSQLND_MS_CONN_A_STRINGL
#define MYSQLND_MS_CONN_D_CSTRING(conn_str) const char *conn_str
#define MYSQLND_MS_CONN_DV_CSTRING(conn_str) MYSQLND_MS_CONN_D_CSTRING(conn_str)
#define MYSQLND_MS_CONN_A_CSTRING MYSQLND_MS_CONN_A_STRING

#define MYSQLND_MS_CONN_STRING_DUP(dstr, sstr, persistent) (dstr) = (sstr) ?  mnd_pestrdup((sstr), persistent) : NULL
#define MYSQLND_MS_CONN_STRINGL_DUP(dstr, sstr, persistent) MYSQLND_MS_CONN_STRING_DUP(dstr, sstr, persistent); (dstr##_len) = (sstr##_len)
#define MYSQLND_MS_CONN_STRING_FREE(conn_str, persistent) if ((conn_str)) mnd_pefree((conn_str), persistent)
#define MYSQLND_MS_CONN_STRINGL_FREE(conn_str, persistent) MYSQLND_MS_CONN_STRING_FREE(conn_str, persistent);  (conn_str) = NULL; (conn_str##_len) = 0
#define MYSQLND_MS_S_TO_CONN_STRINGL(conn_str, str, str_len) (conn_str) = str; (conn_str##_len) = str_len
#define MYSQLND_MS_S_TO_CONN_STRING(conn_str, str) (conn_str) = str
#define MYSQLND_MS_CONN_HOST(conn) conn->host
#define MYSQLND_MS_CONN_USER(conn) conn->user
#define MYSQLND_MS_CONN_PASS(conn) conn->passwd
#define _ms_mysqlnd_plugin_get_plugin_connection_data_data mysqlnd_plugin_get_plugin_connection_data_data
#define _ms_mysqlnd_plugin_get_plugin_stmt_data mysqlnd_plugin_get_plugin_stmt_data
#define _ms_mysqlnd_conn_get_methods mysqlnd_conn_get_methods
#define _ms_mysqlnd_conn_data_get_methods mysqlnd_conn_data_get_methods
#define _ms_mysqlnd_stmt_get_methods mysqlnd_stmt_get_methods
#define _ms_mysqlnd_protocol_get_methods mysqlnd_protocol_get_methods
#define _MS_CONN_SET_STATE CONN_SET_STATE
#define _MS_CONN_GET_STATE CONN_GET_STATE
#else
#define _ms_p_ei
#define _ms_a_ei &

#define _ms_csize_type const size_t

#define _ms_mysqlnd_error_info_init(error_info) mysqlnd_error_info_init(error_info, 0)
#define _ms_mysqlnd_error_info_free_contents(error_info) mysqlnd_error_info_free_contents(error_info)

#define MYSQLND_MS_CONN_STRING(conn_str) (conn_str).s
#define MYSQLND_MS_CONN_STRING_LEN(conn_str) (conn_str).l
#define MYSQLND_MS_CONN_D_STRINGL(conn_str) MYSQLND_STRING conn_str
#define MYSQLND_MS_CONN_DV_STRINGL(conn_str) MYSQLND_MS_CONN_D_STRINGL(conn_str)
#define MYSQLND_MS_CONN_A_STRINGL(conn_str) conn_str
#define MYSQLND_MS_CONN_D_STRING MYSQLND_MS_CONN_D_STRINGL
#define MYSQLND_MS_CONN_DV_STRING MYSQLND_MS_CONN_DV_STRINGL
#define MYSQLND_MS_CONN_A_STRING MYSQLND_MS_CONN_A_STRINGL

#define MYSQLND_MS_CONN_D_CSTRINGL(conn_str) MYSQLND_CSTRING conn_str
#define MYSQLND_MS_CONN_DV_CSTRINGL(conn_str) MYSQLND_MS_CONN_D_CSTRINGL(conn_str)
#define MYSQLND_MS_CONN_A_CSTRINGL(conn_str) mnd_str2c(conn_str)
#define MYSQLND_MS_CONN_D_CSTRING MYSQLND_MS_CONN_D_CSTRINGL
#define MYSQLND_MS_CONN_DV_CSTRING MYSQLND_MS_CONN_DV_CSTRINGL
#define MYSQLND_MS_CONN_A_CSTRING MYSQLND_MS_CONN_A_CSTRINGL

#define MYSQLND_MS_CONN_STRINGL_DUP(dstr, sstr, persistent) (dstr).s = (sstr).s ?  mnd_pestrdup((sstr).s, persistent) : NULL; (dstr).l = (sstr).l
#define MYSQLND_MS_CONN_STRING_DUP MYSQLND_MS_CONN_STRINGL_DUP
#define MYSQLND_MS_CONN_STRINGL_FREE(conn_str, persistent) if ((conn_str).s) mnd_pefree((conn_str).s, persistent);  (conn_str).s = NULL; (conn_str).l = 0
#define MYSQLND_MS_CONN_STRING_FREE MYSQLND_MS_CONN_STRINGL_FREE
#define MYSQLND_MS_S_TO_CONN_STRINGL(conn_str, str, str_len) (conn_str).s = str; (conn_str).l = str_len
#define MYSQLND_MS_S_TO_CONN_STRING(conn_str, str) MYSQLND_MS_S_TO_CONN_STRINGL(conn_str, str, strlen(str))
#define MYSQLND_MS_CONN_HOST(conn) conn->hostname
#define MYSQLND_MS_CONN_USER(conn) conn->username
#define MYSQLND_MS_CONN_PASS(conn) conn->password
#define _ms_mysqlnd_plugin_get_plugin_connection_data_data(conn, plugin_id) ((void *)((char *)conn + sizeof(MYSQLND_CONN_DATA) + plugin_id * sizeof(void *)))
#define _ms_mysqlnd_plugin_get_plugin_stmt_data(stmt, plugin_id) ((void *)((char *)stmt + sizeof(MYSQLND_STMT) + plugin_id * sizeof(void *)))
#define _ms_mysqlnd_conn_get_methods() (&mysqlnd_mysqlnd_conn_methods)
#define _ms_mysqlnd_conn_data_get_methods() (&mysqlnd_mysqlnd_conn_data_methods)
#define _ms_mysqlnd_stmt_get_methods() (&mysqlnd_mysqlnd_stmt_methods)
#define _ms_mysqlnd_protocol_get_methods() (&mysqlnd_mysqlnd_protocol_payload_decoder_factory_methods)
#define _MS_CONN_SET_STATE(conn, s) SET_CONNECTION_STATE(&(conn)->state, s)
#define _MS_CONN_GET_STATE(conn) GET_CONNECTION_STATE(&(conn)->state)
#define SET_STMT_ERROR(stmt, e, s, m) SET_CLIENT_ERROR(stmt->error_info, e, s, m)

#endif
#if PHP_MAJOR_VERSION < 7
#define MYSQLND_MS_ADD_ASSOC_LONG(arg, key, n) add_assoc_long_ex((arg), (key), sizeof((key)), (n))
#define MYSQLND_MS_ADD_ASSOC_STRING(arg, key, str) add_assoc_string_ex((arg), (key), sizeof((key)), (str) ? (str):"", 1)
#else
#define MYSQLND_MS_ADD_ASSOC_LONG(arg, key, n) add_assoc_long_ex((arg), (key), sizeof((key)) - 1, (n))
#define MYSQLND_MS_ADD_ASSOC_STRING(arg, key, str) add_assoc_string_ex((arg), (key), sizeof((key)) - 1, (str) ? (str):"")
#endif
#define MYSQLND_MS_ADD_ASSOC_CONN_STRING(arg, key, str) MYSQLND_MS_ADD_ASSOC_STRING(arg, key, MYSQLND_MS_CONN_STRING((str)))

#define MYSQLND_MS_GTID_CALL_FAIL(meth, ...) ((meth) ? meth(__VA_ARGS__) : FAIL)
#define MYSQLND_MS_GTID_CALL_PASS(meth, ...) ((meth) ? meth(__VA_ARGS__) : PASS)
#define MYSQLND_MS_GTID_CALL(meth, ...) if (meth) meth(__VA_ARGS__)

//END HACK

#define MS_DECLARE_AND_LOAD_CONN_DATA(conn_data, connection) \
	MYSQLND_MS_CONN_DATA ** conn_data = (MYSQLND_MS_CONN_DATA **) _ms_mysqlnd_plugin_get_plugin_connection_data_data((connection), mysqlnd_ms_plugin_id)

#define MS_LOAD_CONN_DATA(conn_data, connection) \
	(conn_data) = (MYSQLND_MS_CONN_DATA **) _ms_mysqlnd_plugin_get_plugin_connection_data_data((connection), mysqlnd_ms_plugin_id)

#define MS_CALL_ORIGINAL_CONN_HANDLE_METHOD(method) ms_orig_mysqlnd_conn_handle_methods->method
#define MS_CALL_ORIGINAL_CONN_DATA_METHOD(method) ms_orig_mysqlnd_conn_methods->method
extern struct st_mysqlnd_conn_data_methods * ms_orig_mysqlnd_conn_methods;
extern struct st_mysqlnd_conn_methods * ms_orig_mysqlnd_conn_handle_methods;
#define MS_LOAD_STMT_DATA(stmt_data, statement) \
	(stmt_data) = (MYSQLND_MS_STMT_DATA **)  _ms_mysqlnd_plugin_get_plugin_stmt_data((statement), mysqlnd_ms_plugin_id)

#define MYSQLND_MS_ERROR_INFO(conn_object) (*((conn_object)->error_info))

#define MYSQLND_MS_UPSERT_STATUS(conn_object) (*((conn_object)->upsert_status))




#define BEGIN_ITERATE_OVER_SERVER_LISTS(el, masters, slaves) \
{ \
	/* need to cast, as masters of slaves could be const. We use external llist_position, so this is safe */ \
	DBG_INF_FMT("master(%p) has %d, slave(%p) has %d", \
		(masters), zend_llist_count((zend_llist *) (masters)), (slaves), zend_llist_count((zend_llist *) (slaves))); \
	{ \
		MYSQLND_MS_LIST_DATA ** el_pp;\
		zend_llist * lists[] = {NULL, (zend_llist * ) (masters), (zend_llist *) (slaves), NULL}; \
		zend_llist ** list = lists; \
		while (*++list) { \
			zend_llist_position	pos; \
			/* search the list of easy handles hanging off the multi-handle */ \
			for (el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(*list, &pos); \
				 el_pp && ((el) = *el_pp) && (el)->conn; \
				 el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_next_ex(*list, &pos)) \
			{ \

#define END_ITERATE_OVER_SERVER_LISTS \
			} \
		} \
	} \
}


#define BEGIN_ITERATE_OVER_SERVER_LISTS_NEW(el, masters, slaves) \
{ \
	/* need to cast, as masters of slaves could be const. We use external llist_position, so this is safe */ \
	DBG_INF_FMT("master(%p) has %d, slave(%p) has %d", \
				(masters), zend_llist_count((zend_llist *) (masters)), (slaves), zend_llist_count((zend_llist *) (slaves))); \
	{ \
		MYSQLND_MS_LIST_DATA ** el_pp; \
		zend_llist * internal_master_list = (masters); \
		zend_llist * internal_slave_list = (slaves); \
		zend_llist * internal_list = internal_master_list; \
		zend_llist_position	pos; \
		/* search the list of easy handles hanging off the multi-handle */ \
		for ((el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(internal_list, &pos)) \
			  || ((internal_list = internal_slave_list) \
			      && \
				  (el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(internal_list, &pos))) ; \
			 el_pp && ((el) = *el_pp) && (el)->conn; \
		 	 (el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_next_ex(internal_list, &pos)) \
			 || \
			 ( \
				(internal_list == internal_master_list) \
				&& \
				/* yes, we need an assignment */ \
				(internal_list = internal_slave_list) \
				&& \
				(el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex(internal_slave_list, &pos)) \
			 ) \
			) \
		{ \

#define END_ITERATE_OVER_SERVER_LISTS_NEW \
		} \
	} \
}

#define BEGIN_ITERATE_OVER_SERVER_LIST(el, list) \
{ \
	/* need to cast, as this list could be const. We use external llist_position, so this is safe */ \
	DBG_INF_FMT("list(%p) has %d", (list), zend_llist_count((zend_llist *) (list))); \
	{ \
		MYSQLND_MS_LIST_DATA ** MACRO_el_pp;\
		zend_llist_position	MACRO_pos; \
		/* search the list of easy handles hanging off the multi-handle */ \
		for (((el) = NULL), MACRO_el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_first_ex((zend_llist *)(list), &MACRO_pos); \
			 MACRO_el_pp && ((el) = *MACRO_el_pp) && (el)->conn; \
			 ((el) = NULL), MACRO_el_pp = (MYSQLND_MS_LIST_DATA **) zend_llist_get_next_ex((zend_llist *)(list), &MACRO_pos)) \
		{ \

#define END_ITERATE_OVER_SERVER_LIST \
		} \
	} \
}

#define MS_TIMEVAL_TO_UINT64(tp) (uint64_t)(tp.tv_sec*1000000 + tp.tv_usec)
#define MS_TIME_SET(time_now) \
	{ \
		struct timeval __tp = {0}; \
		struct timezone __tz = {0}; \
		gettimeofday(&__tp, &__tz); \
		(time_now) = MS_TIMEVAL_TO_UINT64(__tp); \
	} \

#define MS_TIME_DIFF(run_time) \
	{ \
		uint64_t __now; \
		MS_TIME_SET(__now); \
		(run_time) = __now - (run_time); \
	} \


#define MS_WARN_AND_RETURN_IF_TRX_FORBIDS_FAILOVER(stgy, retval) \
   if ((TRUE == (stgy)->in_transaction) && (TRUE == (stgy)->trx_stop_switching)) { \
		mysqlnd_ms_client_n_php_error(error_info, CR_UNKNOWN_ERROR, UNKNOWN_SQLSTATE, E_WARNING TSRMLS_CC,  \
					MYSQLND_MS_ERROR_PREFIX " Automatic failover is not permitted in the middle of a transaction"); \
		DBG_INF("In transaction, no switch allowed"); \
		DBG_RETURN((retval)); \
	} \


#define MS_CHECK_CONN_FOR_TRANSIENT_ERROR(connection, conn_data, transient_error_no) \
	if ((connection) && MYSQLND_MS_ERROR_INFO((connection)).error_no) { \
		MS_CHECK_FOR_TRANSIENT_ERROR((MYSQLND_MS_ERROR_INFO((connection)).error_no), (conn_data), (transient_error_no)); \
	} \

#define MS_CHECK_FOR_TRANSIENT_ERROR(error_no, conn_data, transient_error_no) \
    { \
		(transient_error_no) = 0; \
		if ((conn_data) && (*(conn_data)) && \
			(TRANSIENT_ERROR_STRATEGY_ON == (*(conn_data))->stgy.transient_error_strategy)) { \
			zend_llist_position	pos; \
			zend_llist * transient_error_codes = &((*(conn_data))->stgy.transient_error_codes); \
			uint * transient_error_code_p; \
			for (transient_error_code_p = (uint *)zend_llist_get_first_ex(transient_error_codes, &pos); \
				transient_error_code_p; \
				transient_error_code_p = (uint *)zend_llist_get_next_ex(transient_error_codes, &pos)) { \
				if ((error_no) == *transient_error_code_p) { \
					(transient_error_no) = *transient_error_code_p; \
					break; \
				} \
			} \
		} \
	} \

#define MYSQLND_MS_WARN_OOM() \
	php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Failed to allocate memory. Memory exhausted.")


#define MASTER_SWITCH "ms=master"
#define SLAVE_SWITCH "ms=slave"
#define LAST_USED_SWITCH "ms=last_used"
#define ALL_SERVER_SWITCH "ms=all"

//BEGIN HACK

#define STRONG_SWITCH "ms=strong"
#define SESSION_SWITCH "ms=session"
#define EVENTUAL_SWITCH "ms=eventual"
#define NOINJECT_SWITCH "ms=noinject"
#define INJECT_SWITCH "ms=inject"

#define GTID_RUNNING_MARKER 'R'
#define GTID_WAIT_MARKER 'W'
#define GTID_EXECUTED_MARKER 'E'
#define GTID_GTID_MARKER '?'
#define GTID_RUNNING_HACK_COUNTER -1
#define GTID_RUNNING_RANDOMID 1000

#define MEMCACHED_DEBUG_KEY "__debug__"
#define MEMCACHED_ERROR_KEY "__error__"
#define TEST_ACTIVE_QUERY "SELECT @@global.read_only"
#define TEST_ACTIVE_WQUERY "SELECT @@global.read_only FROM DUAL WHERE @@global.read_only = 0"
//END HACK

#define MASTER_NAME							"master"
#define SLAVE_NAME							"slave"
#define PICK_RANDOM							"random"
#define PICK_ONCE							"sticky"
#define PICK_RROBIN							"roundrobin"
#define PICK_USER							"user"
#define PICK_USER_MULTI						"user_multi"
#define PICK_TABLE							"table"
#define PICK_QOS							"quality_of_service"
#define PICK_GROUPS							"node_groups"
#define LAZY_NAME							"lazy_connections"
#define FAILOVER_NAME						"failover"
#define FAILOVER_STRATEGY_NAME				"strategy"
#define FAILOVER_STRATEGY_DISABLED		 	"disabled"
#define FAILOVER_STRATEGY_MASTER			"master"
#define FAILOVER_STRATEGY_LOOP				"loop_before_master"
#define FAILOVER_MAX_RETRIES        		"max_retries"
#define FAILOVER_REMEMBER_FAILED    		"remember_failed"
#define MASTER_ON_WRITE_NAME				"master_on_write"
#define TRX_STICKINESS_NAME					"trx_stickiness"
#define TRX_STICKINESS_MASTER				"master"
#define TRX_STICKINESS_ON					"on"
#define TABLE_RULES							"rules"
#define SECT_SERVER_CHARSET_NAME			"server_charset"
#define SECT_HOST_NAME						"host"
#define SECT_PORT_NAME						"port"
#define SECT_SOCKET_NAME					"socket"
#define SECT_USER_NAME						"user"
#define SECT_PASS_NAME						"password"
#define SECT_DB_NAME						"db"
#define SECT_CONNECT_FLAGS_NAME				"connect_flags"
#define SECT_FILTER_PRIORITY_NAME 			"priority"
#define SECT_FILTER_NAME					"filters"
#define SECT_USER_CALLBACK					"callback"
#define SECT_QOS_STRONG						"strong_consistency"
#define SECT_QOS_SESSION					"session_consistency"
#define SECT_QOS_EVENTUAL					"eventual_consistency"
#define SECT_QOS_AGE						"age"
#define SECT_QOS_CACHE						"cache"
#define SECT_G_TRX_NAME						"global_transaction_id_injection"
#define SECT_G_TRX_ON_COMMIT				"on_commit"
#define SECT_G_TRX_REPORT_ERROR 			"report_error"
#define SECT_G_TRX_FETCH_LAST_GTID 			"fetch_last_gtid"
#define SECT_G_TRX_CHECK_FOR_GTID 			"check_for_gtid"
#define SECT_G_TRX_WAIT_FOR_GTID_TIMEOUT 	"wait_for_gtid_timeout"
//BEGIN HACK
//BEGIN NOWAIT
#define SECT_G_TRX_MEMCACHED				"memcached"
#define SECT_G_TRX_MODULE 					"module"
#define SECT_G_TRX_RUNNING_DEPTH 			"running_depth"
#define SECT_G_TRX_RUNNING_WDEPTH 			"running_wdepth"
//END NOWAIT
#define HOSTS_GROUP							"hosts_group"
#define SECT_G_TRX_WAIT_FOR_WGTID_TIMEOUT 	"wait_for_wgtid_timeout"
#define SECT_G_TRX_THROTTLE_WGTID_TIMEOUT 	"throttle_wgtid_timeout"
#define SECT_G_TRX_GTID_BLOCK_SIZE			"gtid_block_size"
#define SECT_G_TRX_RACE_AVOID			 	"race_avoid"
#define SECT_G_TRX_RUNNING_TTL				"running_ttl"
#define SECT_G_TRX_MEMCACHED_DEBUG_TTL		"memcached_debug_ttl"
#define SECT_G_TRX_ON_CONNECT				"on_connect"
#define SECT_G_TRX_TYPE						"type"
#define SECT_G_TRX_MEMCACHED_HOST			"memcached_host"
#define SECT_G_TRX_MEMCACHED_KEY			"memcached_key"
#define SECT_G_TRX_MEMCACHED_WKEY			"memcached_wkey"
#define SECT_G_TRX_WC_UUID					"wc_uuid"
#define SECT_G_TRX_MEMCACHED_PORT			"memcached_port"
#define SECT_G_TRX_MEMCACHED_PORT_ADD_HACK	"memcached_port_add_hack"
#define SECT_G_TRX_AUTO_CLEAN				"auto_clean"
//END HACK
#define SECT_LB_WEIGHTS						"weights"
#define SECT_FABRIC_NAME					"fabric"
#define SECT_FABRIC_HOSTS					"hosts"
#define SECT_FABRIC_TIMEOUT					"timeout"
#define SECT_FABRIC_TRX_BOUNDARY_WARNING    "trx_warn_serverlist_changes"
#define SECT_XA_NAME						"xa"
#define SECT_XA_ROLLBACK_ON_CLOSE			"rollback_on_close"
#define SECT_XA_STATE_STORE					"state_store"
#define SECT_XA_STORE_MYSQL					"mysql"
#define SECT_XA_STORE_PARTICIPANT_CRED 		"record_participant_credentials"
#define SECT_XA_STORE_GLOBAL_TRX_TABLE		"global_trx_table"
#define SECT_XA_STORE_PARTICIPANT_TABLE		"participant_table"
#define SECT_XA_STORE_GC_TABLE				"garbage_collection_table"
#define SECT_XA_STORE_PARTICIPANT_LOCALHOST "participant_localhost_ip"
#define SECT_XA_GC_NAME 					"garbage_collection"
#define SECT_XA_GC_MAX_RETRIES				"max_retries"
#define SECT_XA_GC_PROBABILITY				"probability"
#define SECT_XA_GC_MAX_TRX_PER_RUN			"max_transactions_per_run"
#define TRANSIENT_ERROR_NAME				"transient_error"
#define TRANSIENT_ERROR_MAX_RETRIES			"max_retries"
#define TRANSIENT_ERROR_USLEEP_RETRY		"usleep_retry"
#define TRANSIENT_ERROR_CODES				"mysql_error_codes"

typedef enum
{
	STATEMENT_SELECT,
	STATEMENT_INSERT,
	STATEMENT_UPDATE,
	STATEMENT_DELETE,
	STATEMENT_TRUNCATE,
	STATEMENT_REPLACE,
	STATEMENT_RENAME,
	STATEMENT_ALTER,
	STATEMENT_DROP,
	STATEMENT_CREATE
} enum_mysql_statement_type;


enum enum_which_server
{
	USE_MASTER,
	USE_SLAVE,
	USE_LAST_USED,
	USE_ALL
};


enum mysqlnd_ms_server_pick_strategy
{
	SERVER_PICK_RROBIN,
	SERVER_PICK_RANDOM,
	SERVER_PICK_USER,
	SERVER_PICK_USER_MULTI,
	SERVER_PICK_TABLE,
	SERVER_PICK_QOS,
	SERVER_PICK_GROUPS,
	SERVER_PICK_LAST_ENUM_ENTRY
};

/* it should work also without any params, json config to the ctor will be NULL */
#define DEFAULT_PICK_STRATEGY SERVER_PICK_RANDOM

enum mysqlnd_ms_server_failover_strategy
{
	SERVER_FAILOVER_DISABLED,
	SERVER_FAILOVER_MASTER,
	SERVER_FAILOVER_LOOP
};

#define DEFAULT_FAILOVER_STRATEGY SERVER_FAILOVER_DISABLED
#define DEFAULT_FAILOVER_MAX_RETRIES 1
#define DEFAULT_FAILOVER_REMEMBER_FAILED 0

enum mysqlnd_ms_trx_stickiness_strategy
{
	TRX_STICKINESS_STRATEGY_DISABLED,
	TRX_STICKINESS_STRATEGY_MASTER,
	TRX_STICKINESS_STRATEGY_ON
};
#define DEFAULT_TRX_STICKINESS_STRATEGY TRX_STICKINESS_STRATEGY_DISABLED

enum mysqlnd_ms_transient_error_strategy
{
	TRANSIENT_ERROR_STRATEGY_DISABLED,
	TRANSIENT_ERROR_STRATEGY_ON
};
#define DEFAULT_TRANSIENT_ERROR_STRATEGY TRANSIENT_ERROR_STRATEGY_DISABLED
#define DEFAULT_TRANSIENT_ERROR_MAX_RETRIES 1
#define DEFAULT_TRANSIENT_ERROR_USLEEP_BEFORE_RETRY 100

typedef enum mysqlnd_ms_collected_stats
{
	MS_STAT_USE_SLAVE,
	MS_STAT_USE_MASTER,
	MS_STAT_USE_SLAVE_GUESS,
	MS_STAT_USE_MASTER_GUESS,
	MS_STAT_USE_SLAVE_FORCED,
	MS_STAT_USE_MASTER_FORCED,
	MS_STAT_USE_LAST_USED_FORCED,
	MS_STAT_USE_SLAVE_CALLBACK,
	MS_STAT_USE_MASTER_CALLBACK,
	MS_STAT_NON_LAZY_CONN_SLAVE_SUCCESS,
	MS_STAT_NON_LAZY_CONN_SLAVE_FAILURE,
	MS_STAT_NON_LAZY_CONN_MASTER_SUCCESS,
	MS_STAT_NON_LAZY_CONN_MASTER_FAILURE,
	MS_STAT_LAZY_CONN_SLAVE_SUCCESS,
	MS_STAT_LAZY_CONN_SLAVE_FAILURE,
	MS_STAT_LAZY_CONN_MASTER_SUCCESS,
	MS_STAT_LAZY_CONN_MASTER_FAILURE,
	MS_STAT_TRX_AUTOCOMMIT_ON,
	MS_STAT_TRX_AUTOCOMMIT_OFF,
	MS_STAT_TRX_MASTER_FORCED,
#ifndef MYSQLND_HAS_INJECTION_FEATURE
	MS_STAT_GTID_AUTOCOMMIT_SUCCESS,
	MS_STAT_GTID_AUTOCOMMIT_FAILURE,
	MS_STAT_GTID_COMMIT_SUCCESS,
	MS_STAT_GTID_COMMIT_FAILURE,
	MS_STAT_GTID_IMPLICIT_COMMIT_SUCCESS,
	MS_STAT_GTID_IMPLICIT_COMMIT_FAILURE,
#endif
	MS_STAT_TRANSIENT_ERROR_RETRIES,
	MS_STAT_FABRIC_SHARDING_LOOKUP_SERVERS_SUCCESS,
	MS_STAT_FABRIC_SHARDING_LOOKUP_SERVERS_FAILURE,
	MS_STAT_FABRIC_SHARDING_LOOKUP_SERVERS_TIME_TOTAL,
	MS_STAT_FABRIC_SHARDING_LOOKUP_SERVERS_BYTES_TOTAL,
	MS_STAT_FABRIC_SHARDING_LOOKUP_SERVERS_XML_FAILURE,
	MS_STAT_XA_BEGIN,
	MS_STAT_XA_COMMIT_SUCCESS,
	MS_STAT_XA_COMMIT_FAILURE,
	MS_STAT_XA_ROLLBACK_SUCCESS,
	MS_STAT_XA_ROLLBACK_FAILURE,
	MS_STAT_XA_PARTICIPANTS,
	MS_STAT_XA_ROLLBACK_ON_CLOSE,
	MS_STAT_POOL_MASTERS_TOTAL,
	MS_STAT_POOL_SLAVES_TOTAL,
	MS_STAT_POOL_MASTERS_ACTIVE,
	MS_STAT_POOL_SLAVES_ACTIVE,
	MS_STAT_POOL_UPDATES,
	MS_STAT_POOL_MASTER_REACTIVATED,
	MS_STAT_POOL_SLAVE_REACTIVATED,
	MS_STAT_LAST /* Should be always the last */
} enum_mysqlnd_ms_collected_stats;

#define MYSQLND_MS_INC_STATISTIC(stat) MYSQLND_INC_STATISTIC(MYSQLND_MS_G(collect_statistics), mysqlnd_ms_stats, (stat))
#define MYSQLND_MS_INC_STATISTIC_W_VALUE(stat, value) MYSQLND_INC_STATISTIC_W_VALUE(MYSQLND_MS_G(collect_statistics), mysqlnd_ms_stats, (stat), (value))

#define MYSQLND_MS_TIMEVAL_TO_UINT64(tp) (uint64_t)(tp.tv_sec*1000000 + tp.tv_usec)

#define MYSQLND_MS_STATS_TIME_SET(time_now) \
	if (MYSQLND_MS_G(collect_statistics) == FALSE) { \
		(time_now) = 0; \
	} else { \
		struct timeval __tp = {0}; \
		struct timezone __tz = {0}; \
		gettimeofday(&__tp, &__tz); \
		(time_now) = MYSQLND_MS_TIMEVAL_TO_UINT64(__tp); \
	} \

#define MYSQLND_MS_STATS_TIME_DIFF(run_time) \
	{ \
		uint64_t now; \
		MYSQLND_MS_STATS_TIME_SET(now); \
		(run_time) = now - (run_time); \
	}


typedef struct st_mysqlnd_ms_list_data
{
	/* hash_key should be the only case where we break
	 * encapsulation between core and pool */
	_ms_smart_type pool_hash_key;

	char * name_from_config;
	MYSQLND_CONN_DATA * conn;
	MYSQLND_MS_CONN_DV_STRING(host);
	MYSQLND_MS_CONN_DV_STRING(user);
	MYSQLND_MS_CONN_DV_STRINGL(passwd);
	unsigned int port;
	MYSQLND_MS_CONN_DV_STRING(socket);
	MYSQLND_MS_CONN_DV_STRINGL(db);
	unsigned long connect_flags;
	char * emulated_scheme;
	size_t emulated_scheme_len;
	zend_bool persistent;
} MYSQLND_MS_LIST_DATA;


typedef struct st_mysqlnd_ms_filter_data
{
	void (*filter_dtor)(struct st_mysqlnd_ms_filter_data * TSRMLS_DC);
	void (*filter_conn_pool_replaced)(struct st_mysqlnd_ms_filter_data *, zend_llist * master_connections, zend_llist * slave_connections, MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC);
	char * name;
	size_t name_len;
	enum mysqlnd_ms_server_pick_strategy pick_type;
	zend_bool multi_filter;
	zend_bool persistent;
} MYSQLND_MS_FILTER_DATA;


typedef struct st_mysqlnd_ms_filter_user_data
{
	MYSQLND_MS_FILTER_DATA parent;
	zval user_callback;
	zend_bool callback_valid;
} MYSQLND_MS_FILTER_USER_DATA;


typedef struct st_mysqlnd_ms_filter_table_data
{
	MYSQLND_MS_FILTER_DATA parent;
	HashTable master_rules;
	HashTable slave_rules;
} MYSQLND_MS_FILTER_TABLE_DATA;


typedef struct st_mysqlnd_ms_filter_rr_data
{
	MYSQLND_MS_FILTER_DATA parent;
	HashTable master_context;
	HashTable slave_context;
	HashTable lb_weight;
} MYSQLND_MS_FILTER_RR_DATA;


typedef struct st_mysqlnd_ms_filter_rr_context
{
	unsigned int pos;
	zend_llist weight_list;
} MYSQLND_MS_FILTER_RR_CONTEXT;


typedef struct st_mysqlnd_ms_filter_lb_weight
{
	unsigned int weight;
	unsigned int current_weight;
	zend_bool persistent;
} MYSQLND_MS_FILTER_LB_WEIGHT;


typedef struct st_mysqlnd_ms_filter_lb_weight_in_context
{
	MYSQLND_MS_FILTER_LB_WEIGHT * lb_weight;
	MYSQLND_MS_LIST_DATA * element;
} MYSQLND_MS_FILTER_LB_WEIGHT_IN_CONTEXT;


typedef struct st_mysqlnd_ms_filter_random_lb_context
{
	zend_llist sort_list;
	unsigned int total_weight;
} MYSQLND_MS_FILTER_RANDOM_LB_CONTEXT;


typedef struct st_mysqlnd_ms_filter_random_data
{
	MYSQLND_MS_FILTER_DATA parent;
	struct {
		HashTable master_context;
		HashTable slave_context;
		zend_bool once;
	} sticky;
	HashTable lb_weight;
	struct {
		HashTable master_context;
		HashTable slave_context;
	} weight_context;
} MYSQLND_MS_FILTER_RANDOM_DATA;


enum mysqlnd_ms_filter_qos_consistency
{
	CONSISTENCY_STRONG,
	CONSISTENCY_SESSION,
	CONSISTENCY_EVENTUAL,
	CONSISTENCY_LAST_ENUM_ENTRY
};

enum mysqlnd_ms_filter_qos_option
{
	QOS_OPTION_NONE,
	QOS_OPTION_GTID,
	QOS_OPTION_AGE,
	QOS_OPTION_CACHE,
	QOS_OPTION_LAST_ENUM_ENTRY
};

// BEGIN HACK
enum mysqlnd_ms_gtid_type
{
	GTID_NONE,
	GTID_CLIENT,
	GTID_SERVER,
	GTID_CLIENT_SERVER,
	GTID_XX_CLIENT,
	GTID_EXPERIMENTAL,
	GTID_LAST_ENUM_ENTRY
};

#define GTID_RACE_AVOID_DISABLED				0
#define GTID_RACE_AVOID_ADD_ERROR				1
#define GTID_RACE_AVOID_ADD_ACTIVE					2
#define GTID_RACE_AVOID_MAX_VALUE				3

// END HACK

/* using struct because we will likely add cache ttl later */
typedef struct st_mysqlnd_ms_filter_qos_option_data
{
  char * gtid;
  size_t gtid_len;
  long age;
  uint ttl;
} MYSQLND_MS_FILTER_QOS_OPTION_DATA;

typedef struct st_mysqlnd_ms_filter_qos_data
{
	MYSQLND_MS_FILTER_DATA parent;
	enum mysqlnd_ms_filter_qos_consistency consistency;
	enum mysqlnd_ms_filter_qos_option option;
	MYSQLND_MS_FILTER_QOS_OPTION_DATA option_data;
} MYSQLND_MS_FILTER_QOS_DATA;

typedef struct st_mysqlnd_ms_filter_groups_data
{
	MYSQLND_MS_FILTER_DATA parent;
	HashTable groups;
} MYSQLND_MS_FILTER_GROUPS_DATA;

typedef struct st_mysqlnd_ms_filter_groups_data_group
{
	HashTable master_context;
	HashTable slave_context;
} MYSQLND_MS_FILTER_GROUPS_DATA_GROUP;

/* XA transaction tracking */

enum mysqlnd_ms_xa_state
{
	XA_NON_EXISTING, /* initial state: not started */
	XA_ACTIVE, 		/* XA begin */
	XA_IDLE,		/* XA end */
	XA_PREPARED,	/* XA prepare */
	XA_COMMIT,		/* XA commit */
	XA_ROLLBACK		/* XA rollback */
};

/* Participant in the current XA trx */
typedef struct st_mysqlnd_ms_xa_participant {
	zend_bool persistent;
	MYSQLND_CONN_DATA * conn;
	enum mysqlnd_ms_xa_state state;
	int id;
} MYSQLND_MS_XA_PARTICIPANT_LIST_DATA;

struct st_mysqlnd_ms_config_json_entry;

typedef struct st_mysqlnd_xa_id {
	char * store_id;
	unsigned int gtrid;
	unsigned int format_id;
} MYSQLND_MS_XA_ID;

typedef struct st_mysqlnd_ms_xa_trx_state_store {
	char * name;
	void * data;
							/* Parse JSON config */
	void					(*load_config)(struct st_mysqlnd_ms_config_json_entry * section, void * data,
											MYSQLND_ERROR_INFO * error_info, zend_bool persistent TSRMLS_DC);
	/* mysqlnd_ms_xa_begin() call, expand xa_id by store record id/pk */
	enum_func_status (*begin)(void * data, MYSQLND_ERROR_INFO * error_info, MYSQLND_MS_XA_ID * xa_id,
							  unsigned int timeout TSRMLS_DC);
	/* Switch global/monitor state */
	enum_func_status (*monitor_change_state)(void * data, MYSQLND_ERROR_INFO * error_info, MYSQLND_MS_XA_ID * xa_id,
											 enum mysqlnd_ms_xa_state to, enum mysqlnd_ms_xa_state intend TSRMLS_DC);
	/* Record failure on global level.
	   Called to inform the store of our intend (should be rollback or commit).
	   Any failure is reported, including a failure to switch RMs/servers to XA END in
	   which case - likely - no follow up action is required. Getting notified of
	   a failure does not always mean that GC can be applied.
	 */
	enum_func_status (*monitor_failure)(void * data, MYSQLND_ERROR_INFO * error_info, MYSQLND_MS_XA_ID * xa_id,
						   enum mysqlnd_ms_xa_state intend TSRMLS_DC);
	/* Mark a global transaction for garbage collection: either its finished or it failed (and we gave up) */
	enum_func_status (*monitor_finish)(void * data, MYSQLND_ERROR_INFO * error_info,
											MYSQLND_MS_XA_ID * xa_id, zend_bool failure TSRMLS_DC);
	/* Add participant to previously started XA trx */
	enum_func_status (*add_participant)(void * data, MYSQLND_ERROR_INFO * error_info, MYSQLND_MS_XA_ID * xa_id,
										const MYSQLND_MS_XA_PARTICIPANT_LIST_DATA * const participant,
										zend_bool record_cred, const char * localhost_ip TSRMLS_DC);
	/* Switch participant state */
	enum_func_status (*participant_change_state)(void * data, MYSQLND_ERROR_INFO * error_info, MYSQLND_MS_XA_ID * xa_id,
												const MYSQLND_MS_XA_PARTICIPANT_LIST_DATA * const participant,
												enum mysqlnd_ms_xa_state from, enum mysqlnd_ms_xa_state to TSRMLS_DC);
	/* Record participant failure, called if core experiences an issue with participant */
	enum_func_status (*participant_failure)(void * data, MYSQLND_ERROR_INFO *error_info, MYSQLND_MS_XA_ID * xa_id,
											const MYSQLND_MS_XA_PARTICIPANT_LIST_DATA * const participant,
											const MYSQLND_ERROR_INFO * const participant_error_info TSRMLS_DC);
	/* GC for one specific trx */
	enum_func_status (*garbage_collect_one)(void * data, MYSQLND_ERROR_INFO *error_info, MYSQLND_MS_XA_ID * xa_id,
											unsigned int gc_max_retries TSRMLS_DC);
	/* GC anything you can find... */
	enum_func_status (*garbage_collect_all)(void * data, MYSQLND_ERROR_INFO *error_info, unsigned int gc_max_retries,
											unsigned int gc_max_trx_per_run TSRMLS_DC);
	/* Destructor */
	void (*dtor)(void ** data, zend_bool persistent TSRMLS_DC);
	void (*dtor_conn_close)(void ** data, zend_bool persistent TSRMLS_DC);
} MYSQLND_MS_XA_STATE_STORE;

/* GC details, stored in a global variables hash table */
typedef struct st_mysqlnd_ms_xa_gc {
	unsigned int gc_max_retries;
	unsigned int gc_probability;
	unsigned int gc_max_trx_per_run;
	zend_bool added_to_module_globals;
	MYSQLND_MS_XA_STATE_STORE store;
} MYSQLND_MS_XA_GC;

/* Main XA struct for proxy connection */
typedef struct st_mysqlnd_ms_xa_trx {
	/* Plugin section name */
	char * host;
	size_t host_len;
	zend_bool on;
	zend_bool in_transaction;
	zend_bool rollback_on_close;
	enum mysqlnd_ms_xa_state finish_transaction_intend;
	MYSQLND_MS_XA_GC * gc;
	MYSQLND_MS_XA_ID id;
	unsigned int timeout;
	zend_llist participants;
	char * participant_localhost_ip;
	zend_bool record_participant_cred;
	enum mysqlnd_ms_xa_state state;
} MYSQLND_MS_XA_TRX;

// BEGIN HACK
/* Low-level extraction functionality */
typedef struct st_mysqlnd_ms_gtid_trx_methods {
	enum mysqlnd_ms_gtid_type type;
	enum_func_status (*gtid_get_last)(MYSQLND_MS_LIST_DATA * gtid_conn_elm, char ** gtid TSRMLS_DC);
	enum_func_status (*gtid_set_last_write)(MYSQLND_CONN_DATA * connection, char * gtid TSRMLS_DC);
	enum_func_status (*gtid_init)(MYSQLND_CONN_DATA * proxy_conn TSRMLS_DC);
	enum_func_status (*gtid_connect)(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_MS_LIST_DATA * new_element, zend_bool is_master,
	                               zend_bool lazy_connections TSRMLS_DC);
	enum_func_status (*gtid_inject_before)(MYSQLND_CONN_DATA * conn TSRMLS_DC);
	enum_func_status (*gtid_inject_after)(MYSQLND_CONN_DATA * conn, enum_func_status status TSRMLS_DC);
	void (*gtid_filter)(MYSQLND_CONN_DATA * conn, const char * gtid, const char *query, size_t query_len,
									 zend_llist * slave_list, zend_llist * master_list, zend_llist * selected_slaves, zend_llist * selected_masters, zend_bool is_write TSRMLS_DC);
	void (*gtid_reset)(MYSQLND_CONN_DATA * conn, enum_func_status status TSRMLS_DC);
	void (*gtid_trace)(MYSQLND_CONN_DATA * conn, const char * key, size_t key_len, unsigned int ttl, const char * query, size_t query_len TSRMLS_DC);
	void (*gtid_race_add_active)(MYSQLND_CONN_DATA * conn, zend_llist * server_list, zend_llist * selected_servers, zend_bool is_write TSRMLS_DC);
	MYSQLND_CONN_DATA * (*gtid_validate)(MYSQLND_CONN_DATA * conn TSRMLS_DC);
} MYSQLND_MS_GTID_TRX_METHODS;

// END HACK
/*
 NOTE: Some elements are available with every connection, some
 are set for the global/proxy connection only. The global/proxy connection
 is the handle provided by the user. The other connections are the "hidden"
 ones that MS openes to the cluster nodes.
*/
typedef struct st_mysqlnd_ms_conn_data
{
	zend_bool initialized;
	zend_bool skip_ms_calls;
	MYSQLND_CONN_DATA * proxy_conn;
	char * connect_host;
    // BEGIN HACK
	_ms_smart_type *elm_pool_hash_key;
	// END HACK
	struct st_mysqlnd_pool * pool;

	const MYSQLND_CHARSET * server_charset;

	/* Global LB strategy set on proxy conn */
	struct mysqlnd_ms_lb_strategies {
		HashTable table_filters;

		enum mysqlnd_ms_server_failover_strategy failover_strategy;
		uint failover_max_retries;
		zend_bool failover_remember_failed;
		HashTable failed_hosts;

		zend_bool mysqlnd_ms_flag_master_on_write;
		zend_bool master_used;

		/* note: some flags may not be used, however saves us a ton of ifdef to declare them anyway */
		enum mysqlnd_ms_trx_stickiness_strategy trx_stickiness_strategy;
		zend_bool trx_stop_switching;
		zend_bool trx_read_only;
		zend_bool trx_autocommit_off;

		/* buffered tx_begin call */
		zend_bool 		trx_begin_required;
		unsigned int 	trx_begin_mode;
		char *		 	trx_begin_name;

		zend_bool in_transaction;
		zend_bool in_xa_transaction;

		MYSQLND_CONN_DATA * last_used_conn;

		zend_llist * filters;

		enum mysqlnd_ms_transient_error_strategy transient_error_strategy;
		uint transient_error_max_retries;
		long transient_error_usleep_before_retry;
		/* list of uint */
		zend_llist transient_error_codes;

	} stgy;

	struct st_mysqlnd_ms_conn_credentials {
		MYSQLND_MS_CONN_DV_STRING(user);
		MYSQLND_MS_CONN_DV_STRINGL(passwd);
		unsigned int port;
		MYSQLND_MS_CONN_DV_STRING(socket);
		MYSQLND_MS_CONN_DV_STRINGL(db);
		unsigned long mysql_flags;
	} cred;

	/* per connection trx context set on proxy conn and all others */
	struct st_mysqlnd_ms_global_trx_injection {
		char * on_commit;
		size_t on_commit_len;
		char * fetch_last_gtid;
		size_t fetch_last_gtid_len;
		char * check_for_gtid;
		size_t check_for_gtid_len;
		unsigned int wait_for_gtid_timeout;
		/*
		 TODO: This seems to be the only per-connection value.
		 We may want to split up the structure into a global and
		 local part. is_master needs to be local/per-connection.
		 The rest could probably be global, like with stgy and
		 LB weigth.
		*/
		zend_bool report_error;
		zend_bool is_master;

		//BEGIN HACK
		enum mysqlnd_ms_gtid_type type;
		MYSQLND_MS_GTID_TRX_METHODS * m;
		char * memcached_host;
		size_t memcached_host_len;
		unsigned int memcached_port;
		unsigned int memcached_port_add_hack;
		char * memcached_key;
		size_t memcached_key_len;
		char * memcached_wkey;
		size_t memcached_wkey_len;
		char * wc_uuid;
		size_t wc_uuid_len;
		size_t gtid_block_size;
		unsigned int running_ttl;
		unsigned int memcached_debug_ttl;
		unsigned int wait_for_wgtid_timeout;
		unsigned int throttle_wgtid_timeout;
		unsigned int race_avoid_strategy;
		zend_bool gtid_on_connect;
		zend_bool auto_clean;

		MYSQLND_MS_LIST_DATA * gtid_conn_elm;
		memcached_st *memc;
		uint64_t owned_token;
// BEGIN NOWAIT
		char * memcached;
		size_t memcached_len;
		uint64_t module;
		unsigned int running_depth;
		unsigned int running_wdepth;

		uint64_t owned_wtoken;
		uint64_t last_chk_token;
		uint64_t last_chk_wtoken;
		zend_bool first_read;
		zend_bool executed;
		char * last_whost;
// END NOWAIT
		char * last_gtid;
		size_t last_gtid_len;
		char * last_wgtid;
		size_t last_wgtid_len;
		char * last_ckgtid;
		size_t last_ckgtid_len;
		char * last_wckgtid;
		size_t last_wckgtid_len;
		time_t run_time;
		int running;
		int running_id;
		zend_bool injectable_query;
		zend_bool is_prepare;
		//END HACK
	} global_trx;
	mysqlnd_fabric *fabric;

	/* TODO XA: proxy connection only */
	MYSQLND_MS_XA_TRX *xa_trx;

} MYSQLND_MS_CONN_DATA;

// BEGIN HACK
typedef struct st_mysqlnd_ms_stmt_data
{
	char * query;
	size_t query_len;
} MYSQLND_MS_STMT_DATA;
// END HACK

typedef struct st_mysqlnd_ms_table_filter
{
	char * host_id;
	size_t host_id_len;
	char * wild;
	size_t wild_len;
#ifdef WE_NEED_NEXT
	struct st_mysqlnd_ms_table_filter * next;
#endif
	unsigned int priority;
	zend_bool persistent;
} MYSQLND_MS_TABLE_FILTER;


typedef struct st_mysqlnd_ms_command
{
	enum php_mysqlnd_server_command command;
	zend_uchar * payload;
	size_t payload_len;
	enum mysqlnd_packet_type ok_packet;
	zend_bool silent;
	zend_bool ignore_upsert_status;
	zend_bool persistent;
} MYSQLND_MS_COMMAND;

#endif /* MYSQLND_MS_ENUM_N_DEF_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
