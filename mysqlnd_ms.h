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

/* $Id: mysqlnd_ms.h 334441 2014-07-31 15:39:33Z uw $ */
#ifndef MYSQLND_MS_H
#define MYSQLND_MS_H

#ifdef PHP_WIN32
#define PHP_MYSQLND_MS_API __declspec(dllexport)
#else
# if defined(__GNUC__) && __GNUC__ >= 4
#  define PHP_MYSQLND_MS_API __attribute__ ((visibility("default")))
# else
#  define PHP_MYSQLND_MS_API
# endif
#endif

#ifndef SMART_STR_START_SIZE
#define SMART_STR_START_SIZE 1024
#endif
#ifndef SMART_STR_PREALLOC
#define SMART_STR_PREALLOC 256
#endif
#include "ext/standard/php_smart_str.h"

#include "ext/mysqlnd/mysqlnd.h"
#include "ext/mysqlnd/mysqlnd_statistics.h"
#include "ext/mysqlnd/mysqlnd_debug.h"
#include "ext/mysqlnd/mysqlnd_priv.h"
#include "mysqlnd_ms_enum_n_def.h"

#if MYSQLND_VERSION_ID > 50009
#include "ext/mysqlnd/mysqlnd_reverse_api.h"
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#define MYSQLND_MS_CONFIG_FORMAT "json"

ZEND_BEGIN_MODULE_GLOBALS(mysqlnd_ms)
	zend_bool enable;
	zend_bool force_config_usage;
	const char * config_file;
	zval * user_pick_server;
	zend_bool collect_statistics;
	zend_bool multi_master;
	zend_bool disable_rw_split;
	char * config_startup_error;
	HashTable xa_state_stores;
	// BEGIN HACK
	const char * master_on;
	// END HACK
ZEND_END_MODULE_GLOBALS(mysqlnd_ms)


#ifdef ZTS
#define MYSQLND_MS_G(v) TSRMG(mysqlnd_ms_globals_id, zend_mysqlnd_ms_globals *, v)
#else
#define MYSQLND_MS_G(v) (mysqlnd_ms_globals.v)
#endif

#define PHP_MYSQLND_MS_VERSION "1.6.0-alpha"
#define MYSQLND_MS_VERSION_ID 10600

#define MYSQLND_MS_ERROR_PREFIX "(mysqlnd_ms)"

extern MYSQLND_STATS * mysqlnd_ms_stats;


/*
  ALREADY FIXED:
  Keep it false for now or we will have races in connect,
  where multiple instance can read the slave[] values and so
  move the pointer of each other. Need to find better implementation of
  hotloading.
  Maybe not use `hotloading? FALSE:TRUE` but an expclicit lock around
  the array extraction of master[] and slave[] and pass FALSE to
  mysqlnd_ms_json_config_string(), meaning it should not try to get a lock.
*/
#define MYSLQND_MS_HOTLOADING FALSE

extern unsigned int mysqlnd_ms_plugin_id;
extern struct st_mysqlnd_ms_json_config * mysqlnd_ms_json_config;
ZEND_EXTERN_MODULE_GLOBALS(mysqlnd_ms)


// BEGIN HACK
enum_func_status
mysqlnd_ms_set_tx(MYSQLND_CONN_DATA * conn, zend_bool mode TSRMLS_DC);
enum_func_status
mysqlnd_ms_unset_tx(MYSQLND_CONN_DATA * proxy_conn, zend_bool commit TSRMLS_DC);
// END HACK
void mysqlnd_ms_register_hooks();
void mysqlnd_ms_conn_list_dtor(void * pDest);
PHP_MYSQLND_MS_API zend_bool mysqlnd_ms_match_wild(const char * const str, const char * const wildstr TSRMLS_DC);
struct st_mysqlnd_ms_list_data;
enum_func_status mysqlnd_ms_lazy_connect(struct st_mysqlnd_ms_list_data * element, zend_bool master TSRMLS_DC);

void mysqlnd_ms_client_n_php_error(MYSQLND_ERROR_INFO * error_info,
								   unsigned int client_error_code,
								   const char * const client_error_state,
								   unsigned int php_error_level TSRMLS_DC,
								   const char * const format, ...);

enum_func_status
mysqlnd_ms_connect_to_host_aux(MYSQLND_CONN_DATA * proxy_conn, MYSQLND_CONN_DATA * conn, const char * name_from_config,
							   zend_bool is_master,
							   const char * host, unsigned int port,
							   struct st_mysqlnd_ms_conn_credentials * cred,
							   struct st_mysqlnd_ms_global_trx_injection * global_trx,
							   zend_bool lazy_connections,
							   zend_bool persistent TSRMLS_DC);

struct st_ms_token_and_value
{
	unsigned int token;
	zval value;
};


struct st_mysqlnd_query_scanner
{
	void * scanner;
	zval * token_value;
};

struct st_mysqlnd_ms_table_info
{
	char * db;
	char * table;
	char * org_table;
	zend_bool persistent;
};

struct st_mysqlnd_ms_field_info
{
	char * db;
	char * table;
	char * name;
	char * org_name;
	void * custom_data;
	zend_bool free_custom_data;
	zend_bool persistent;
};

struct st_mysqlnd_parse_info
{
	zend_llist table_list;
	zend_llist select_field_list;
	zend_llist where_field_list;
	zend_llist * active_field_list;
	zend_bool parse_where;
	enum_mysql_statement_type statement;
	zend_bool persistent;
};

struct st_mysqlnd_query_parser
{
	struct st_mysqlnd_query_scanner * scanner;
	struct st_mysqlnd_parse_info parse_info;
};


#endif	/* MYSQLND_MS_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
