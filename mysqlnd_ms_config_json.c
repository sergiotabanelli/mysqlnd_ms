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

/* $Id: mysqlnd_ms_config_json.c 333051 2014-03-21 13:11:33Z uw $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/mysqlnd/mysqlnd.h"
#include "ext/mysqlnd/mysqlnd_debug.h"
#include "ext/mysqlnd/mysqlnd_priv.h"

#include "mysqlnd_ms.h"
#include "mysqlnd_ms_config_json.h"

#include "ext/json/php_json.h"

#ifndef mnd_sprintf
#define mnd_sprintf spprintf
#define mnd_sprintf_free efree
#endif

struct st_mysqlnd_ms_config_json_entry
{
	union {
		struct {
			char * c;
			size_t len;
		} str;
		HashTable * ht;
		double dval;
		long long lval;
	} value;
	zend_uchar type;
};

struct st_mysqlnd_ms_json_config {
	struct st_mysqlnd_ms_config_json_entry * main_section;
#ifdef ZTS
	MUTEX_T LOCK_access;
#endif
};


/* {{{ mysqlnd_ms_config_json_init */
PHP_MYSQLND_MS_API struct st_mysqlnd_ms_json_config *
mysqlnd_ms_config_json_init(TSRMLS_D)
{
	struct st_mysqlnd_ms_json_config * ret;
	DBG_ENTER("mysqlnd_ms_config_json_init");
	ret = mnd_calloc(1, sizeof(struct st_mysqlnd_ms_json_config));
	if (ret) {
#ifdef ZTS
		ret->LOCK_access = tsrm_mutex_alloc();
#endif
	}
	DBG_RETURN(ret);
}
/* }}} */

void
mysqlnd_ms_config_json_section_dtor(void * data)
{
	struct st_mysqlnd_ms_config_json_entry * entry = (struct st_mysqlnd_ms_config_json_entry *) data;
	TSRMLS_FETCH();
	if (entry) {
		switch (entry->type) {
			case IS_DOUBLE:
			case IS_LONG:
			case IS_NULL:
				break;
			case IS_STRING:
				mnd_free(entry->value.str.c);
				break;
			case IS_ARRAY:
				zend_hash_destroy(entry->value.ht);
				mnd_free(entry->value.ht);
				break;
			default:
				php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX
						" Unknown entry type %d  in mysqlnd_ms_config_json_section_dtor", entry->type);
		}
		mnd_free(entry);
	}
}
/* }}} */

static void
mysqlnd_ms_config_json_section_hash_dtor(_ms_hash_zval_type * data)
{
	mysqlnd_ms_config_json_section_dtor(_ms_p_zval (void _ms_p_zval *) _MS_HASH_Z_PTR_P(data));
}
/* {{{ mysqlnd_ms_config_json_free */
PHP_MYSQLND_MS_API void
mysqlnd_ms_config_json_free(struct st_mysqlnd_ms_json_config * cfg TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_config_json_free");
	if (cfg) {
		mysqlnd_ms_config_json_section_dtor(cfg->main_section);
#ifdef ZTS
		tsrm_mutex_free(cfg->LOCK_access);
#endif
		mnd_free(cfg);
	}

	DBG_VOID_RETURN;
}
/* }}} */


static struct st_mysqlnd_ms_config_json_entry * mysqlnd_ms_zval_data_to_hashtable(zval * json_data TSRMLS_DC);

/* {{{ mysqlnd_ms_add_zval_to_hash */
static void
mysqlnd_ms_add_zval_to_hash(zval * zv, HashTable * ht, const char * skey, size_t skey_len, ulong nkey, int key_type TSRMLS_DC)
{
	struct st_mysqlnd_ms_config_json_entry * new_entry = NULL;
	zend_bool bval = FALSE;
	DBG_ENTER("mysqlnd_ms_add_zval_to_hash");

	switch (Z_TYPE_P(zv)) {
		case IS_ARRAY:
			new_entry = mysqlnd_ms_zval_data_to_hashtable(zv TSRMLS_CC);
			break;
		case IS_STRING:
			new_entry = mnd_calloc(1, sizeof(struct st_mysqlnd_ms_config_json_entry));
			if (new_entry) {
				new_entry->type = IS_STRING;
				new_entry->value.str.c = mnd_pestrndup(Z_STRVAL_P(zv), Z_STRLEN_P(zv), 1);
				new_entry->value.str.len = Z_STRLEN_P(zv);
				DBG_INF_FMT("str=%s", new_entry->value.str.c);
			} else {
				MYSQLND_MS_WARN_OOM();
			}
			break;
		case IS_DOUBLE:
			new_entry = mnd_calloc(1, sizeof(struct st_mysqlnd_ms_config_json_entry));
			if (new_entry) {
				new_entry->type = IS_DOUBLE;
				new_entry->value.dval = Z_DVAL_P(zv);
				DBG_INF_FMT("dval=%f", new_entry->value.dval);
			} else {
				MYSQLND_MS_WARN_OOM();
			}
			break;
#if PHP_MAJOR_VERSION < 7
		case IS_BOOL:
			DBG_INF("boolean");
#else
		case IS_TRUE:
			bval = TRUE;
		case IS_FALSE:
			DBG_INF("boolean");
			new_entry = mnd_calloc(1, sizeof(struct st_mysqlnd_ms_config_json_entry));
			if (new_entry) {
				new_entry->type = IS_LONG;
				new_entry->value.lval = bval;
				DBG_INF_FMT("lval="MYSQLND_LL_SPEC, (long long) new_entry->value.lval);
			} else {
				MYSQLND_MS_WARN_OOM();
			}
			break;
#endif
		case IS_LONG:
			new_entry = mnd_calloc(1, sizeof(struct st_mysqlnd_ms_config_json_entry));
			if (new_entry) {
				new_entry->type = IS_LONG;
				new_entry->value.lval = Z_LVAL_P(zv);
				DBG_INF_FMT("lval="MYSQLND_LL_SPEC, (long long) new_entry->value.lval);
			} else {
				MYSQLND_MS_WARN_OOM();
			}
			break;
		case IS_NULL:
			new_entry = mnd_calloc(1, sizeof(struct st_mysqlnd_ms_config_json_entry));
			if (new_entry) {
				new_entry->type = IS_NULL;
				DBG_INF("null value");
			} else {
				MYSQLND_MS_WARN_OOM();
			}
			break;
		default:
			DBG_INF("unknown type");
			break;
	}
	if (new_entry) {
		switch (key_type) {
			case HASH_KEY_IS_STRING:
				_MS_HASHSTR_SET_ZR_FUNC_PTR(zend_hash_str_add_ptr, ht, skey, skey_len, new_entry);
				DBG_INF_FMT("New HASH_KEY_IS_STRING entry [%s] %p", skey, new_entry);
				break;
			default:
				_MS_HASH_SET_ZR_FUNC_PTR_1(zend_hash_index_update_ptr, ht, nkey, new_entry);
				DBG_INF_FMT("New HASH_KEY_IS_LONG entry [%u] %p", nkey, new_entry);
				break;
		}
	}
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_zval_data_to_hashtable */
static struct st_mysqlnd_ms_config_json_entry *
mysqlnd_ms_zval_data_to_hashtable(zval * json_data TSRMLS_DC)
{
	struct st_mysqlnd_ms_config_json_entry * ret = NULL;

	DBG_ENTER("mysqlnd_ms_zval_data_to_hashtable");
	if (json_data && (ret = mnd_calloc(1, sizeof(struct st_mysqlnd_ms_config_json_entry)))) {

		ret->type = IS_ARRAY;
		ret->value.ht = mnd_calloc(1, sizeof(HashTable));
		if (!(ret->value.ht)) {
			MYSQLND_MS_WARN_OOM();
			mnd_free(ret);
			ret = NULL;
			DBG_RETURN(ret);
		}
		zend_hash_init(ret->value.ht, Z_TYPE_P(json_data) == IS_ARRAY? zend_hash_num_elements(Z_ARRVAL_P(json_data)) : 1,
						NULL /* hash_func */, mysqlnd_ms_config_json_section_hash_dtor /*dtor*/, 1 /* persistent */);

		if (Z_TYPE_P(json_data) == IS_ARRAY) {
			HashPosition pos;
			zval _ms_p_zval * entry_zval;
			zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(json_data), &pos);
			while (_MS_HASH_GET_ZR_FUNC_PTR_VA(zend_hash_get_current_data_ex, Z_ARRVAL_P(json_data), entry_zval, &pos) == SUCCESS) {
				char * skey = NULL;
				_ms_size_type skey_len = 0;
				_ms_ulong nkey = 0;
				int key_type = _ms_hash_str_get_current_key(Z_ARRVAL_P(json_data), &skey, &skey_len, &nkey, &pos);
				mysqlnd_ms_add_zval_to_hash(_ms_p_zval entry_zval, ret->value.ht, skey, skey_len, nkey, key_type TSRMLS_CC);

				zend_hash_move_forward_ex(Z_ARRVAL_P(json_data), &pos);
			} /* while */
		} else {

		}
	}
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_config_json_load_configuration_aux */
PHP_MYSQLND_MS_API enum_func_status
mysqlnd_ms_config_json_load_configuration_aux(char * json_file_name, struct st_mysqlnd_ms_config_json_entry **section, zend_bool cfg TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	DBG_ENTER("mysqlnd_ms_config_json_load_configuration_aux");
	DBG_INF_FMT("json_file=%s", json_file_name);
	do {
		php_stream * stream;
		_MS_STR_DECL(str_data);
		zval json_data;
		stream = php_stream_open_wrapper_ex(json_file_name, "rb", REPORT_ERRORS, NULL, NULL);

		if (!stream) {
			if (cfg)
				mnd_sprintf(&(MYSQLND_MS_G(config_startup_error)), 0, MYSQLND_MS_ERROR_PREFIX
								" Failed to open server list config file [%s]", json_file_name);
			/* The only one to bark in RINIT as otherwise no specific warning/error appears */
			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX
					" Failed to open server list config file [%s]", json_file_name);
			break;
		}
		_MS_STR_L_FUNC_PS_1(php_stream_copy_to_mem, stream, str_data, PHP_STREAM_COPY_ALL, 0);
		php_stream_close(stream);
		if (!str_data || _MS_STR_L(str_data) <= 0) {
			if (cfg)
				mnd_sprintf(&(MYSQLND_MS_G(config_startup_error)), 0, MYSQLND_MS_ERROR_PREFIX
						" Config file [%s] is empty. If this is not by mistake, please add some minimal JSON to it to prevent this warning. For example, use '{}' ", json_file_name);
			else
				php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX
						" Config file [%s] is empty. If this is not by mistake, please add some minimal JSON to it to prevent this warning. For example, use '{}' ", json_file_name);
			break;
		}
#if PHP_VERSION_ID >= 50399
		php_json_decode_ex(&json_data, _MS_STR_S(str_data), _MS_STR_L(str_data), PHP_JSON_OBJECT_AS_ARRAY, 512 /* default depth */ TSRMLS_CC);
#else
		php_json_decode(&json_data, _MS_STR_S(str_data), _MS_STR_L(str_data), 1 /* assoc */, 512 /* default depth */ TSRMLS_CC);
#endif
		_MS_STR_PEFREE(str_data, 0);

		if (Z_TYPE(json_data) == IS_NULL) {
			if (cfg)
				mnd_sprintf(&(MYSQLND_MS_G(config_startup_error)), 0, MYSQLND_MS_ERROR_PREFIX
						" Failed to parse config file [%s]. Please, verify the JSON", json_file_name);
			else
				php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX
						" Failed to parse config file [%s]. Please, verify the JSON", json_file_name);
			zval_dtor(&json_data);
			break;
		}

		*section = mysqlnd_ms_zval_data_to_hashtable(&json_data TSRMLS_CC);
		zval_dtor(&json_data);
		if (!*section) {
			if (cfg)
				mnd_sprintf(&(MYSQLND_MS_G(config_startup_error)), 0, MYSQLND_MS_ERROR_PREFIX
							" Failed to find a main section in the config file [%s]. Please, verify the JSON", json_file_name);
			else
				php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX
						" Failed to find a main section in the config file [%s]. Please, verify the JSON", json_file_name);
			break;
		}

		ret = PASS;
	} while (0);
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_config_json_load_host_configuration */
PHP_MYSQLND_MS_API struct st_mysqlnd_ms_config_json_entry *
mysqlnd_ms_config_json_load_host_configuration(const char * host TSRMLS_DC)
{
	struct st_mysqlnd_ms_config_json_entry *ret = NULL;
	char * json_dir = INI_STR("mysqlnd_ms.config_dir");
	DBG_ENTER("mysqlnd_ms_config_json_load_host_configuration");
	DBG_INF_FMT("config_dir=%s", json_dir? json_dir:"n/a");
	if (json_dir) {
		size_t lh = strlen(host);
		size_t ld = strlen(json_dir);
	    struct stat file_stat;
		char * fn = emalloc(lh + ld + 1);
		int err;
		memcpy(fn, json_dir, ld);
		memcpy(fn + ld, host, lh + 1);
        err = stat(fn, &file_stat);
    	DBG_INF_FMT("host_config_file=%s", fn);
        if (!err) {
        	mysqlnd_ms_config_json_load_configuration_aux(fn, &ret, FALSE);
        }
        efree(fn);
	}
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_config_json_load_configuration */
PHP_MYSQLND_MS_API enum_func_status
mysqlnd_ms_config_json_load_configuration(struct st_mysqlnd_ms_json_config * cfg TSRMLS_DC)
{
	enum_func_status ret = FAIL;
	char * json_file_name = INI_STR("mysqlnd_ms.config_file");
	DBG_ENTER("mysqlnd_ms_config_json_load_configuration");
	DBG_INF_FMT("json_file=%s", json_file_name? json_file_name:"n/a");

	if (MYSQLND_MS_G(config_startup_error)) {
		mnd_sprintf_free(MYSQLND_MS_G(config_startup_error));
		MYSQLND_MS_G(config_startup_error) = NULL;
	}

	if (!json_file_name) {
		ret = PASS;
	} else if (json_file_name && cfg) {
/*		do {
			php_stream * stream;
			int str_data_len;
			char * str_data;
			zval json_data;
			stream = php_stream_open_wrapper_ex(json_file_name, "rb", REPORT_ERRORS, NULL, NULL);

			if (!stream) {
				mnd_sprintf(&(MYSQLND_MS_G(config_startup_error)), 0, MYSQLND_MS_ERROR_PREFIX
								" Failed to open server list config file [%s]", json_file_name);
				// The only one to bark in RINIT as otherwise no specific warning/error appears
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", MYSQLND_MS_G(config_startup_error));
				break;
			}
			str_data_len = php_stream_copy_to_mem(stream, &str_data, PHP_STREAM_COPY_ALL, 0);
			php_stream_close(stream);
			if (str_data_len <= 0) {
				mnd_sprintf(&(MYSQLND_MS_G(config_startup_error)), 0, MYSQLND_MS_ERROR_PREFIX
								" Config file [%s] is empty. If this is not by mistake, please add some minimal JSON to it to prevent this warning. For example, use '{}' ", json_file_name);
				break;
			}
#if PHP_VERSION_ID >= 50399
			php_json_decode_ex(&json_data, str_data, str_data_len, PHP_JSON_OBJECT_AS_ARRAY, 512  TSRMLS_CC);
#else
			php_json_decode(&json_data, str_data, str_data_len, 1 , 512  TSRMLS_CC);
#endif
			efree(str_data);

			if (Z_TYPE(json_data) == IS_NULL) {
				mnd_sprintf(&(MYSQLND_MS_G(config_startup_error)), 0, MYSQLND_MS_ERROR_PREFIX
								" Failed to parse config file [%s]. Please, verify the JSON", json_file_name);
				zval_dtor(&json_data);
				break;
			}

			cfg->main_section = mysqlnd_ms_zval_data_to_hashtable(&json_data TSRMLS_CC);
			zval_dtor(&json_data);
			if (!cfg->main_section) {
				mnd_sprintf(&(MYSQLND_MS_G(config_startup_error)), 0, MYSQLND_MS_ERROR_PREFIX
								" Failed to find a main section in the config file [%s]. Please, verify the JSON", json_file_name);
				break;
			}

			ret = PASS;
		} while (0);
		*/
		ret = mysqlnd_ms_config_json_load_configuration_aux(json_file_name, &cfg->main_section, TRUE);
	}
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_config_json_section_exists */
PHP_MYSQLND_MS_API zend_bool
mysqlnd_ms_config_json_section_exists(struct st_mysqlnd_ms_json_config * cfg, const char * section, size_t section_len, ulong nkey,
									  zend_bool use_lock TSRMLS_DC)
{
	zend_bool ret = FALSE;
	DBG_ENTER("mysqlnd_ms_config_json_section_exists");
	DBG_INF_FMT("section=[%s] len=[%d]", section? section:"n/a", section_len);

	if (cfg) {
		if (use_lock) {
			MYSQLND_MS_CONFIG_JSON_LOCK(cfg);
		}
		ret = mysqlnd_ms_config_json_sub_section_exists(cfg->main_section, section, section_len, nkey TSRMLS_CC);
		if (use_lock) {
			MYSQLND_MS_CONFIG_JSON_UNLOCK(cfg);
		}
	}

	DBG_INF_FMT("ret=%d", ret);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_config_json_sub_section_exists */
PHP_MYSQLND_MS_API zend_bool
mysqlnd_ms_config_json_sub_section_exists(struct st_mysqlnd_ms_config_json_entry * main_section,
										  const char * section, size_t section_len, ulong nkey TSRMLS_DC)
{
	zend_bool ret = FALSE;
	DBG_ENTER("mysqlnd_ms_config_json_sub_section_exists");
	DBG_INF_FMT("section=[%s] len=[%d]", section? section:"n/a", section_len);

	if (main_section && main_section->type == IS_ARRAY && main_section->value.ht){
		void _ms_p_zval * ini_entry;
		if (section && section_len) {
			ret = (SUCCESS == _MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, main_section->value.ht, section, section_len, ini_entry))? TRUE:FALSE;
		} else {
			ret = (SUCCESS == _MS_HASH_GET_ZR_FUNC_PTR_1(zend_hash_index_find_ptr, main_section->value.ht, nkey, ini_entry))? TRUE:FALSE;
		}
		if (ret)
			DBG_INF_FMT("ini_entry=%p", _ms_p_zval ini_entry);

	}
	DBG_INF_FMT("ret=%d", ret);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_config_json_section */
PHP_MYSQLND_MS_API struct st_mysqlnd_ms_config_json_entry *
mysqlnd_ms_config_json_section(struct st_mysqlnd_ms_json_config * cfg, const char * section, size_t section_len,
							   zend_bool * exists TSRMLS_DC)
{
	struct st_mysqlnd_ms_config_json_entry * ret = NULL;
	DBG_ENTER("mysqlnd_ms_config_json_section");

	if (cfg) {
		ret = mysqlnd_ms_config_json_sub_section(cfg->main_section, section, section_len, exists TSRMLS_CC);
	}
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_config_json_section */
PHP_MYSQLND_MS_API struct st_mysqlnd_ms_config_json_entry *
mysqlnd_ms_config_json_sub_section(struct st_mysqlnd_ms_config_json_entry * main_section,
								   const char * section, size_t section_len, zend_bool * exists TSRMLS_DC)
{
	zend_bool tmp_bool;
	struct st_mysqlnd_ms_config_json_entry * ret = NULL;

	DBG_ENTER("mysqlnd_ms_config_json_sub_section");
	DBG_INF_FMT("section=%s", section);

	if (exists) {
		*exists = 0;
	} else {
		exists = &tmp_bool;
	}

	if (main_section && main_section->type == IS_ARRAY && main_section->value.ht) {
		struct st_mysqlnd_ms_config_json_entry _ms_p_zval * ini_section;
		if (_MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, main_section->value.ht, section, section_len, ini_section) == SUCCESS) {
			if (ini_section && IS_ARRAY == (_ms_p_zval ini_section)->type) {
				*exists = 1;
				ret = _ms_p_zval ini_section;
			}
			DBG_INF_FMT("Section %s %p", section, ret);
		}

	}

	DBG_INF_FMT("ret=%p", ret);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_config_json_section_is_list */
PHP_MYSQLND_MS_API zend_bool
mysqlnd_ms_config_json_section_is_list(struct st_mysqlnd_ms_config_json_entry * section TSRMLS_DC)
{
	zend_bool ret;
	DBG_ENTER("mysqlnd_ms_config_json_section_is_list");
	ret = (section && section->type == IS_ARRAY && section->value.ht)? TRUE:FALSE;
	DBG_INF_FMT("ret=%d", ret);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_config_json_section_is_object_list */
PHP_MYSQLND_MS_API zend_bool
mysqlnd_ms_config_json_section_is_object_list(struct st_mysqlnd_ms_config_json_entry * section TSRMLS_DC)
{
	zend_bool ret = TRUE;
	DBG_ENTER("mysqlnd_ms_config_json_section_is_object_list");
	if (section && section->type == IS_ARRAY && section->value.ht) {
		HashPosition pos;
		struct st_mysqlnd_ms_config_json_entry _ms_p_zval * entry;

		zend_hash_internal_pointer_reset_ex(section->value.ht, &pos);
		while (SUCCESS == _MS_HASH_GET_ZR_FUNC_PTR_VA(zend_hash_get_current_data_ptr_ex, section->value.ht, entry, &pos)) {
			if (!((_ms_p_zval entry) && (_ms_p_zval entry)->type == IS_ARRAY && (_ms_p_zval entry)->value.ht)) {
				ret = FALSE;
				break;
			}
			zend_hash_move_forward_ex(section->value.ht, &pos);
		}
	} else {
		ret = FALSE;
	}
	DBG_INF_FMT("ret=%d", ret);
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_config_json_next_sub_section */
PHP_MYSQLND_MS_API struct st_mysqlnd_ms_config_json_entry *
mysqlnd_ms_config_json_next_sub_section(struct st_mysqlnd_ms_config_json_entry * main_section,
										char ** section_name, size_t * section_name_len, _ms_ulong * nkey TSRMLS_DC)
{
	struct st_mysqlnd_ms_config_json_entry * ret = NULL;
	struct st_mysqlnd_ms_config_json_entry _ms_p_zval * entry;
	DBG_ENTER("mysqlnd_ms_config_json_next_sub_section");

	if (SUCCESS == _MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data_ptr, main_section->value.ht, entry)) {
		char * tmp_skey = NULL;
		_ms_size_type tmp_skey_len = 0;
		_ms_ulong tmp_nkey = 0;
		int key_type;
		if (!section_name) {
			section_name = &tmp_skey;
		}
		if (!nkey) {
			nkey = &tmp_nkey;
		}
		key_type = _ms_hash_str_get_current_key(main_section->value.ht, section_name, &tmp_skey_len, nkey, NULL);

		if (HASH_KEY_IS_STRING == key_type) {
			if (section_name_len) {
				*section_name_len = tmp_skey_len;
				DBG_INF_FMT("section(len)=%s(%d)", *section_name, *section_name_len);
			}
		}

		ret = _ms_p_zval entry;

		zend_hash_move_forward(main_section->value.ht);
	}
	DBG_INF_FMT("ret=%p", ret);
	DBG_RETURN(ret);
}
/* }}} */

/* {{{ mysqlnd_ms_config_json_string_aux_inner */
static char *
mysqlnd_ms_config_json_string_aux_inner(struct st_mysqlnd_ms_config_json_entry * ini_section_entry,
										zend_bool * exists, zend_bool * is_list_value TSRMLS_DC)
{
	char * ret = NULL;
	DBG_ENTER("mysqlnd_ms_config_json_string_aux_inner");

	if (ini_section_entry) {
		switch (ini_section_entry->type) {
			case IS_LONG:
				DBG_INF_FMT("long2string:%lld", ini_section_entry->value.lval);
				{
					char * tmp_buf;
					int tmp_buf_len = spprintf(&tmp_buf, 0, "%lld", ini_section_entry->value.lval);
					ret = mnd_pestrndup(tmp_buf, tmp_buf_len, 0);
					DBG_INF_FMT("result=%s", tmp_buf);
					efree(tmp_buf);
				}
				*exists = 1;
				break;
			case IS_DOUBLE:
				DBG_INF_FMT("double2string:%f", ini_section_entry->value.dval);
				{
					char * tmp_buf;
					int tmp_buf_len = spprintf(&tmp_buf, 0, "%f", ini_section_entry->value.dval);
					ret = mnd_pestrndup(tmp_buf, tmp_buf_len, 0);
					DBG_INF_FMT("result=%s", tmp_buf);
					efree(tmp_buf);
				}
				*exists = 1;
				break;
			case IS_STRING:
				DBG_INF("IS_STRING");
				ret = mnd_pestrndup(ini_section_entry->value.str.c, ini_section_entry->value.str.len, 0);
				*exists = 1;
				break;
			case IS_NULL:
				DBG_INF("IS_NULL");
				*exists = 1;
				break;
			case IS_ARRAY:
			{
				struct st_mysqlnd_ms_config_json_entry _ms_p_zval * value;
				DBG_INF("IS_ARRAY");
				*is_list_value = 1;
				DBG_INF_FMT("the list has %d entries", zend_hash_num_elements(ini_section_entry->value.ht));
				if (SUCCESS == _MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data_ptr, ini_section_entry->value.ht, value)) {
					switch ((_ms_p_zval value)->type) {
						case IS_STRING:
							ret = mnd_pestrndup((_ms_p_zval value)->value.str.c, (_ms_p_zval value)->value.str.len, 0);
							*exists = 1;
							break;
						case IS_LONG:
							{
								char * tmp_buf;
								int tmp_buf_len = spprintf(&tmp_buf, 0, "%lld", (_ms_p_zval value)->value.lval);
								ret = mnd_pestrndup(tmp_buf, tmp_buf_len, 0);
								DBG_INF_FMT("result=%s", tmp_buf);
								efree(tmp_buf);
							}
							*exists = 1;
							break;
						case IS_DOUBLE:
							{
								char * tmp_buf;
								int tmp_buf_len = spprintf(&tmp_buf, 0, "%f", (_ms_p_zval value)->value.dval);
								ret = mnd_pestrndup(tmp_buf, tmp_buf_len, 0);
								DBG_INF_FMT("result=%s", tmp_buf);
								efree(tmp_buf);
							}
							*exists = 1;
							break;
						case IS_ARRAY:
							DBG_ERR("still unsupported type");
							/* to do */
							break;
					}
					zend_hash_move_forward(ini_section_entry->value.ht);
				}
				break;
			}
			default:
				DBG_ERR("Unknown type");
				php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX
							" Unknown entry type %d in mysqlnd_ms_config_json_string", ini_section_entry->type);
				break;
		}
	}
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_config_json_string_from_section */
PHP_MYSQLND_MS_API char *
mysqlnd_ms_config_json_string_from_section(struct st_mysqlnd_ms_config_json_entry * section,
										   const char * name, size_t name_len, ulong nkey,
										   zend_bool * exists, zend_bool * is_list_value TSRMLS_DC)
{
	zend_bool tmp_bool;
	char * ret = NULL;

	DBG_ENTER("mysqlnd_ms_config_json_string_from_section");
	DBG_INF_FMT("name=%s", name);

	if (exists) {
		*exists = 0;
	} else {
		exists = &tmp_bool;
	}
	if (is_list_value) {
		*is_list_value = 0;
	} else {
		is_list_value = &tmp_bool;
	}

	if (section && section->type == IS_ARRAY && section->value.ht) {
		struct st_mysqlnd_ms_config_json_entry _ms_p_zval * ini_section_entry;
		if (name) {
			if (_MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, section->value.ht, name, name_len, ini_section_entry) == SUCCESS) {
				ret = mysqlnd_ms_config_json_string_aux_inner(_ms_p_zval ini_section_entry, exists, is_list_value TSRMLS_CC);
			}
		} else {
			if (_MS_HASH_GET_ZR_FUNC_PTR_1(zend_hash_index_find_ptr, section->value.ht, nkey, ini_section_entry) == SUCCESS) {
				ret = mysqlnd_ms_config_json_string_aux_inner(_ms_p_zval ini_section_entry, exists, is_list_value TSRMLS_CC);
			}
		}
	}
	DBG_INF_FMT("ret=%s", ret);
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_config_json_int_aux_inner */
static int64_t
mysqlnd_ms_config_json_int_aux_inner(struct st_mysqlnd_ms_config_json_entry * ini_section_entry,
										zend_bool * exists, zend_bool * is_list_value TSRMLS_DC)
{
	int64_t ret = 0;
	DBG_ENTER("mysqlnd_ms_config_json_string_aux_inner");

	if (ini_section_entry) {
		switch (ini_section_entry->type) {
			case IS_LONG:
				DBG_INF_FMT("long2string:%lld", ini_section_entry->value.lval);
				ret = ini_section_entry->value.lval;
				*exists = 1;
				break;
			case IS_DOUBLE:
				DBG_INF_FMT("double2string:%f", ini_section_entry->value.dval);
				ret = (int64_t) ini_section_entry->value.dval;
				*exists = 1;
				break;
			case IS_STRING:
				DBG_INF("IS_STRING");
				ret = atoll(ini_section_entry->value.str.c);
				*exists = 1;
				break;
			case IS_NULL:
				DBG_INF("IS_NULL");
				ret = 0;
				*exists = 1;
				break;
			case IS_ARRAY:
			{
				struct st_mysqlnd_ms_config_json_entry _ms_p_zval * value;
				DBG_INF("IS_ARRAY");
				*is_list_value = 1;
				DBG_INF_FMT("the list has %d entries", zend_hash_num_elements(ini_section_entry->value.ht));
				if (SUCCESS == _MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data_ptr, ini_section_entry->value.ht, value)) {
					switch ((_ms_p_zval value)->type) {
						case IS_STRING:
							ret = atoll((_ms_p_zval value)->value.str.c);
							*exists = 1;
							break;
						case IS_LONG:
							ret = (_ms_p_zval value)->value.lval;
							*exists = 1;
							break;
						case IS_DOUBLE:
							ret = (int64_t) (_ms_p_zval value)->value.dval;
							*exists = 1;
							break;
						case IS_ARRAY:
							DBG_ERR("still unsupported type");
							/* to do */
							break;
					}
					zend_hash_move_forward(ini_section_entry->value.ht);
				}
				break;
			}
			default:
				DBG_ERR("Unknown type");
				php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX
							" Unknown entry type %d in mysqlnd_ms_config_json_string", ini_section_entry->type);
				break;
		}
	}
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ mysqlnd_ms_config_json_int_from_section */
PHP_MYSQLND_MS_API int64_t
mysqlnd_ms_config_json_int_from_section(struct st_mysqlnd_ms_config_json_entry * section,
										   const char * name, size_t name_len, ulong nkey,
										   zend_bool * exists, zend_bool * is_list_value TSRMLS_DC)
{
	zend_bool tmp_bool;
	int64_t ret = 0;

	DBG_ENTER("mysqlnd_ms_config_json_int_from_section");
	DBG_INF_FMT("name=%s", name);

	if (exists) {
		*exists = 0;
	} else {
		exists = &tmp_bool;
	}
	if (is_list_value) {
		*is_list_value = 0;
	} else {
		is_list_value = &tmp_bool;
	}

	if (section && section->type == IS_ARRAY && section->value.ht) {
		struct st_mysqlnd_ms_config_json_entry _ms_p_zval * ini_section_entry;
		if (name) {
			if (_MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, section->value.ht, name, name_len, ini_section_entry) == SUCCESS) {
				ret = mysqlnd_ms_config_json_int_aux_inner(_ms_p_zval ini_section_entry, exists, is_list_value TSRMLS_CC);
			}
		} else {
			if (_MS_HASH_GET_ZR_FUNC_PTR_1(zend_hash_index_find_ptr, section->value.ht, nkey, ini_section_entry) == SUCCESS) {
				ret = mysqlnd_ms_config_json_int_aux_inner(_ms_p_zval ini_section_entry, exists, is_list_value TSRMLS_CC);
			}
		}
	}
	DBG_INF_FMT("ret="MYSQLND_LL_SPEC, ret);
	DBG_RETURN(ret);
}
/* }}} */


#ifdef U0
/* TODO: never called */
/* {{{ mysqlnd_ms_config_json_string */
PHP_MYSQLND_MS_API char *
mysqlnd_ms_config_json_string(struct st_mysqlnd_ms_json_config * cfg, const char * section, size_t section_len,
							  const char * name, size_t name_len,
							  zend_bool * exists, zend_bool * is_list_value, zend_bool use_lock TSRMLS_DC)
{
	char * ret = NULL;

	DBG_ENTER("mysqlnd_ms_config_json_string");
	DBG_INF_FMT("name=%s", name);

	if (!cfg) {
		DBG_RETURN(ret);
	}

	if (use_lock) {
		MYSQLND_MS_CONFIG_JSON_LOCK(cfg);
	}
	ret = mysqlnd_ms_config_json_string_aux(cfg->main_section->value.ht, section, section_len, name, name_len, exists, is_list_value TSRMLS_CC);
	if (use_lock) {
		MYSQLND_MS_CONFIG_JSON_UNLOCK(cfg);
	}

	DBG_INF_FMT("ret=%s", ret? ret:"n/a");

	DBG_RETURN(ret);
}
/* }}} */
#endif

#ifdef U0
/* TODO: never called */

/* {{{ mysqlnd_ms_str_to_long_long */
static long long
mysqlnd_ms_str_to_long_long(const char * const s, zend_bool * valid)
{
	long long ret;
	char * end_ptr;
	errno = 0;
	ret = strtoll(s, &end_ptr, 10);
	if (
			(
				(errno == ERANGE && (ret == LLONG_MAX || ret == LLONG_MIN))
				||
				(errno != 0 && ret == 0)
			)
			||
			end_ptr == s
		)
	{
		*valid = 0;
		return 0;
	}
	*valid = 1;
	return ret;
}
/* }}} */

/* {{{ mysqlnd_ms_config_json_int */
PHP_MYSQLND_MS_API long long
mysqlnd_ms_config_json_int(struct st_mysqlnd_ms_json_config * cfg, const char * section, size_t section_len,
						   const char * name, size_t name_len,
						   zend_bool * exists, zend_bool * is_list_value, zend_bool use_lock TSRMLS_DC)
{
	zend_bool tmp_bool;
	long long ret = 0;

	DBG_ENTER("mysqlnd_ms_config_json_int");
	DBG_INF_FMT("name=%s", name);

	if (exists) {
		*exists = 0;
	} else {
		exists = &tmp_bool;
	}
	if (is_list_value) {
		*is_list_value = 0;
	} else {
		is_list_value = &tmp_bool;
	}

	if (!cfg) {
		DBG_RETURN(ret);
	}

	if (use_lock) {
		MYSQLND_MS_CONFIG_JSON_LOCK(cfg);
	}
	if (cfg->main_section) {
		struct st_mysqlnd_ms_config_json_entry _ms_p_zval * ini_section;
		if (_MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, cfg->main_section->value.ht, section, section_len, ini_section) == SUCCESS) {
			struct st_mysqlnd_ms_config_json_entry * ini_section_entry = NULL;

			switch ((_ms_p_zval ini_section)->type) {
				case IS_LONG:
				case IS_DOUBLE:
				case IS_STRING:
					ini_section_entry = _ms_p_zval ini_section;
					break;
				case IS_ARRAY: {
					struct st_mysqlnd_ms_config_json_entry _ms_p_zval * ini_section_entry_pp;
					if (_MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, (_ms_p_zval ini_section)->value.ht, name, name_len, ini_section_entry_pp) == SUCCESS) {
						ini_section_entry = _ms_p_zval ini_section_entry_pp;
					}
					break;
				}
			}
			if (ini_section_entry) {
				switch (ini_section_entry->type) {
					case IS_LONG:
						ret = ini_section_entry->value.lval;
						*exists = 1;
						break;
					case IS_DOUBLE:
						ret = (long long) ini_section_entry->value.dval;
						*exists = 1;
						break;
					case IS_STRING:
						ret = mysqlnd_ms_str_to_long_long(ini_section_entry->value.str.c, exists);
						break;
					case IS_ARRAY:
					{
						struct st_mysqlnd_ms_config_json_entry _ms_p_zval * value;
						*is_list_value = 1;
						DBG_INF_FMT("the list has %d entries", zend_hash_num_elements(ini_section_entry->value.ht));
						if (SUCCESS == _MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data_ptr, ini_section_entry->value.ht, value)) {
							switch ((_ms_p_zval value)->type) {
								case IS_STRING:
									ret = mysqlnd_ms_str_to_long_long((_ms_p_zval value)->value.str.c, exists);
									break;
								case IS_LONG:
									ret = (_ms_p_zval value)->value.lval;
									break;
								case IS_DOUBLE:
									ret = (long long) (_ms_p_zval value)->value.dval;
									*exists = 1;
									break;
								case IS_ARRAY:
									DBG_ERR("still unsupported type");
									break;
									/* to do */
							}
							zend_hash_move_forward(ini_section_entry->value.ht);
						}
						break;
					}
					default:
						php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX
									" Unknown entry type %d in mysqlnd_ms_config_json_int", ini_section_entry->type);
						break;
				}
			}
		} /* if (zend_hash... */
	} /* if (cfg->config) */
	if (use_lock) {
		MYSQLND_MS_CONFIG_JSON_UNLOCK(cfg);
	}

	DBG_INF_FMT("ret="MYSQLND_LL_SPEC, (long long) ret);

	DBG_RETURN(ret);
}
/* }}} */
#endif

#ifdef U0
/* TODO: never used */
/* {{{ mysqlnd_ms_str_to_double */
static double
mysqlnd_ms_str_to_double(const char * const s, zend_bool * valid)
{
	double ret;
	char * end_ptr;
	errno = 0;
	ret = strtod(s, &end_ptr);
	if (
			(
				(errno == ERANGE && (ret == HUGE_VALF || ret == HUGE_VALL))
				||
				(errno != 0 && ret == 0)
			)
			||
			end_ptr == s
		)
	{
		*valid = 0;
		return 0;
	}
	*valid = 1;
	return ret;
}
/* }}} */

/* {{{ mysqlnd_ms_config_json_double */
PHP_MYSQLND_MS_API double
mysqlnd_ms_config_json_double(struct st_mysqlnd_ms_json_config * cfg, const char * section, size_t section_len,
							  const char * name, size_t name_len,
							  zend_bool * exists, zend_bool * is_list_value, zend_bool use_lock TSRMLS_DC)
{
	zend_bool tmp_bool;
	double ret = 0;

	DBG_ENTER("mysqlnd_ms_config_json_double");
	DBG_INF_FMT("name=%s", name);

	if (!cfg) {
		DBG_RETURN(ret);
	}

	if (exists) {
		*exists = 0;
	} else {
		exists = &tmp_bool;
	}
	if (is_list_value) {
		*is_list_value = 0;
	} else {
		is_list_value = &tmp_bool;
	}

	if (use_lock) {
		MYSQLND_MS_CONFIG_JSON_LOCK(cfg);
	}
	if (cfg->main_section) {
		struct st_mysqlnd_ms_config_json_entry _ms_p_zval * ini_section;
		if (_MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, cfg->main_section->value.ht, section, section_len, ini_section) == SUCCESS) {
			struct st_mysqlnd_ms_config_json_entry * ini_section_entry = NULL;

			switch ((_ms_p_zval ini_section)->type) {
				case IS_LONG:
				case IS_DOUBLE:
				case IS_STRING:
					ini_section_entry = _ms_p_zval ini_section;
					break;
				case IS_ARRAY: {
					struct st_mysqlnd_ms_config_json_entry _ms_p_zval * ini_section_entry_pp;
					if (_MS_HASHSTR_GET_ZR_FUNC_PTR(zend_hash_str_find_ptr, (_ms_p_zval ini_section)->value.ht, name, name_len, ini_section_entry_pp) == SUCCESS) {
						ini_section_entry = _ms_p_zval ini_section_entry_pp;
					}
					break;
				}
			}
			if (ini_section_entry) {
				switch (ini_section_entry->type) {
					case IS_LONG:
						ret = (double) ini_section_entry->value.lval;
						*exists = 1;
						break;
					case IS_DOUBLE:
						ret = ini_section_entry->value.dval;
						*exists = 1;
						break;
					case IS_STRING:
						ret = mysqlnd_ms_str_to_double(ini_section_entry->value.str.c, exists);
						break;
					case IS_ARRAY:
					{
						struct st_mysqlnd_ms_config_json_entry _ms_p_zval * value;
						*is_list_value = 1;
						DBG_INF_FMT("the list has %d entries", zend_hash_num_elements(ini_section_entry->value.ht));
						if (SUCCESS == _MS_HASH_GET_ZR_FUNC_PTR(zend_hash_get_current_data_ptr, ini_section_entry->value.ht, value)) {
							switch ((_ms_p_zval value)->type) {
								case IS_STRING:
									ret = mysqlnd_ms_str_to_double((_ms_p_zval value)->value.str.c, exists);
									break;
								case IS_LONG:
									ret = (double) (_ms_p_zval value)->value.lval;
									*exists = 1;
									break;
								case IS_DOUBLE:
									ret = (_ms_p_zval value)->value.dval;
									*exists = 1;
									break;
								case IS_ARRAY:
									DBG_ERR("still unsupported type");
									break;
									/* to do */
							}
							zend_hash_move_forward(ini_section_entry->value.ht);
						}
						break;
					}
					default:
						php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX
									" Unknown entry type %d in mysqlnd_ms_config_json_double", ini_section_entry->type);
						break;
				}
			}
		} /* if (zend_hash... */
	} /* if (cfg->config) */
	if (use_lock) {
		MYSQLND_MS_CONFIG_JSON_UNLOCK(cfg);
	}

	DBG_INF_FMT("ret=%d", ret);

	DBG_RETURN(ret);
}
/* }}} */
#endif

/* {{{ mysqlnd_ms_config_json_reset_section */
PHP_MYSQLND_MS_API void
mysqlnd_ms_config_json_reset_section(struct st_mysqlnd_ms_config_json_entry * section, zend_bool recursive TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_config_json_reset_section");
	DBG_INF_FMT("section=%p", section);

	if (section && section->type == IS_ARRAY && section->value.ht) {
		struct st_mysqlnd_ms_config_json_entry _ms_p_zval * ini_section_entry;
		HashPosition pos;

		zend_hash_internal_pointer_reset_ex(section->value.ht, &pos);
		while (_MS_HASH_GET_ZR_FUNC_PTR_VA(zend_hash_get_current_data_ptr_ex, section->value.ht, ini_section_entry, &pos) == SUCCESS) {
			if (IS_ARRAY == (_ms_p_zval ini_section_entry)->type && recursive) {
				mysqlnd_ms_config_json_reset_section((_ms_p_zval ini_section_entry), recursive TSRMLS_CC);
			}
			zend_hash_move_forward_ex(section->value.ht, &pos);
		}
		zend_hash_internal_pointer_reset(section->value.ht);
	}

	DBG_VOID_RETURN;
}
/* }}} */


#ifdef ZTS
/* {{{ mysqlnd_ms_config_json_lock */
PHP_MYSQLND_MS_API void
mysqlnd_ms_config_json_lock(struct st_mysqlnd_ms_json_config * cfg, const char * const file, unsigned int line TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_config_json_lock");
	DBG_INF_FMT("mutex=%p file=%s line=%u", cfg->LOCK_access, file, line);
	tsrm_mutex_lock(cfg->LOCK_access);
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlnd_ms_config_json_unlock */
PHP_MYSQLND_MS_API void
mysqlnd_ms_config_json_unlock(struct st_mysqlnd_ms_json_config * cfg, const char * const file, unsigned int line TSRMLS_DC)
{
	DBG_ENTER("mysqlnd_ms_config_json_unlock");
	DBG_INF_FMT("mutex=%p file=%s line=%u", cfg->LOCK_access, file, line);
	tsrm_mutex_unlock(cfg->LOCK_access);
	DBG_VOID_RETURN;
}
/* }}} */
#endif


/* {{{ mysqlnd_ms_config_json_string_is_bool_true */
zend_bool
mysqlnd_ms_config_json_string_is_bool_false(const char * value)
{
	if (!value) {
		return TRUE;
	}
	if (!strncmp("0", value, sizeof("0") - 1)) {
		return TRUE;
	}
	if (!strncasecmp("false", value, sizeof("false") - 1)) {
		return TRUE;
	}
	if (!strncasecmp("off", value, sizeof("off") - 1)) {
		return TRUE;
	}
	if (!strncasecmp("aus", value, sizeof("aus") - 1)) {
		return TRUE;
	}
	return FALSE;
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
