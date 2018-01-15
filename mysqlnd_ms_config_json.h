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

/* $Id: mysqlnd_ms_config_json.h 331418 2013-09-17 11:57:46Z ab $ */
#ifndef MYSQLND_MS_CONFIG_JSON_H
#define MYSQLND_MS_CONFIG_JSON_H

struct st_mysqlnd_ms_json_config;
struct st_mysqlnd_ms_config_json_entry;

struct knv
{
	char * key;
	union {
		union {
			char * s;
			size_t len;
		} str;
		long long lval;
		double dval;
	} value;
};

PHP_MYSQLND_MS_API struct st_mysqlnd_ms_json_config * mysqlnd_ms_config_json_init(TSRMLS_D);
PHP_MYSQLND_MS_API void mysqlnd_ms_config_json_free(struct st_mysqlnd_ms_json_config * cfg TSRMLS_DC);
PHP_MYSQLND_MS_API enum_func_status mysqlnd_ms_config_json_load_configuration(struct st_mysqlnd_ms_json_config * cfg TSRMLS_DC);

// BEGIN HACK
PHP_MYSQLND_MS_API enum_func_status mysqlnd_ms_config_json_load_configuration_aux(char * json_file_name, struct st_mysqlnd_ms_config_json_entry **section, zend_bool cfg TSRMLS_DC);
PHP_MYSQLND_MS_API struct st_mysqlnd_ms_config_json_entry * mysqlnd_ms_config_json_load_host_configuration(const char * host TSRMLS_DC);
void mysqlnd_ms_config_json_section_dtor(void * data);
// END HACK

PHP_MYSQLND_MS_API zend_bool mysqlnd_ms_config_json_section_exists(struct st_mysqlnd_ms_json_config * cfg, const char * section, size_t section_len, ulong nkey, zend_bool use_lock TSRMLS_DC);
PHP_MYSQLND_MS_API zend_bool mysqlnd_ms_config_json_sub_section_exists(struct st_mysqlnd_ms_config_json_entry * main_section, const char * section, size_t section_len, ulong nkey TSRMLS_DC);

#ifdef U0
/* TODO: never called */
PHP_MYSQLND_MS_API char * mysqlnd_ms_config_json_string(struct st_mysqlnd_ms_json_config * cfg, const char * section, size_t section_len, const char * name, size_t name_len, zend_bool * exists, zend_bool * is_list_value, zend_bool use_lock TSRMLS_DC);
#endif
PHP_MYSQLND_MS_API long long mysqlnd_ms_config_json_int(struct st_mysqlnd_ms_json_config * cfg, const char * section, size_t section_len, const char * name, size_t name_len, zend_bool * exists, zend_bool * is_list_value, zend_bool use_lock TSRMLS_DC);
PHP_MYSQLND_MS_API double mysqlnd_ms_config_json_double(struct st_mysqlnd_ms_json_config * cfg, const char * section, size_t section_len, const char * name, size_t name_len, zend_bool * exists, zend_bool * is_list_value, zend_bool use_lock TSRMLS_DC);

PHP_MYSQLND_MS_API void mysqlnd_ms_config_json_reset_section(struct st_mysqlnd_ms_config_json_entry * section, zend_bool recursive TSRMLS_DC);

PHP_MYSQLND_MS_API struct st_mysqlnd_ms_config_json_entry * mysqlnd_ms_config_json_section(struct st_mysqlnd_ms_json_config * cfg, const char * section, size_t section_len, zend_bool * exists TSRMLS_DC);
PHP_MYSQLND_MS_API struct st_mysqlnd_ms_config_json_entry * mysqlnd_ms_config_json_sub_section(struct st_mysqlnd_ms_config_json_entry *, const char * section, size_t section_len, zend_bool * exists TSRMLS_DC);
PHP_MYSQLND_MS_API zend_bool mysqlnd_ms_config_json_section_is_list(struct st_mysqlnd_ms_config_json_entry * TSRMLS_DC);
PHP_MYSQLND_MS_API zend_bool mysqlnd_ms_config_json_section_is_object_list(struct st_mysqlnd_ms_config_json_entry * section TSRMLS_DC);
PHP_MYSQLND_MS_API struct st_mysqlnd_ms_config_json_entry * mysqlnd_ms_config_json_next_sub_section(struct st_mysqlnd_ms_config_json_entry * main_section, char ** section_name, size_t * section_name_len, ulong * nkey TSRMLS_DC);

PHP_MYSQLND_MS_API char * mysqlnd_ms_config_json_string_from_section(struct st_mysqlnd_ms_config_json_entry * section, const char * name, size_t name_len, ulong nkey, zend_bool * exists, zend_bool * is_list_value TSRMLS_DC);
PHP_MYSQLND_MS_API int64_t mysqlnd_ms_config_json_int_from_section(struct st_mysqlnd_ms_config_json_entry * section, const char * name, size_t name_len, ulong nkey, zend_bool * exists, zend_bool * is_list_value TSRMLS_DC);

zend_bool mysqlnd_ms_config_json_string_is_bool_false(const char * value);

#ifdef ZTS
PHP_MYSQLND_MS_API void mysqlnd_ms_config_json_lock(struct st_mysqlnd_ms_json_config * cfg, const char * const file, unsigned int line TSRMLS_DC);
PHP_MYSQLND_MS_API void mysqlnd_ms_config_json_unlock(struct st_mysqlnd_ms_json_config * cfg, const char * const file, unsigned int line TSRMLS_DC);
#define MYSQLND_MS_CONFIG_JSON_LOCK(cfg) mysqlnd_ms_config_json_lock((cfg), __FILE__, __LINE__ TSRMLS_CC)
#define MYSQLND_MS_CONFIG_JSON_UNLOCK(cfg) mysqlnd_ms_config_json_unlock((cfg), __FILE__, __LINE__ TSRMLS_CC)
#else
#define MYSQLND_MS_CONFIG_JSON_LOCK(cfg)
#define MYSQLND_MS_CONFIG_JSON_UNLOCK(cfg)
#endif

#endif	/* MYSQLND_MS_CONFIG_JSON_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
