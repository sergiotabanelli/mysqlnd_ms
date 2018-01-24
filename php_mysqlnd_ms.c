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

/* $Id: php_mysqlnd_ms.c 334535 2014-08-08 13:47:00Z uw $ */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "ext/mysqlnd/mysqlnd.h"
#include "ext/mysqlnd/mysqlnd_debug.h"
#include "ext/mysqlnd/mysqlnd_priv.h"
#include "ext/mysqlnd/mysqlnd_ext_plugin.h"
#if PHP_VERSION_ID >= 70100
#include "ext/mysqlnd/mysqlnd_connection.h"
#endif
#include "mysqlnd_ms.h"
#include "mysqlnd_ms_config_json.h"
#include "ext/standard/php_rand.h"
#include "mysqlnd_ms_filter_qos.h"
#include "mysqlnd_ms_switch.h"
#include "fabric/mysqlnd_fabric.h"
#include "mysqlnd_ms_xa.h"
#include "mysqlnd_ms_conn_pool.h"

#ifndef mnd_sprintf
#define mnd_sprintf spprintf
#define mnd_sprintf_free efree
#endif


#define STR_W_LEN(str)  str, (sizeof(str) - 1)
const MYSQLND_STRING mysqlnd_ms_stats_values_names[MS_STAT_LAST] =
{
	{ STR_W_LEN("use_slave") },
	{ STR_W_LEN("use_master") },
	{ STR_W_LEN("use_slave_guess") },
	{ STR_W_LEN("use_master_guess") },
	{ STR_W_LEN("use_slave_sql_hint") },
	{ STR_W_LEN("use_master_sql_hint") },
	{ STR_W_LEN("use_last_used_sql_hint") },
	{ STR_W_LEN("use_slave_callback") },
	{ STR_W_LEN("use_master_callback") },
	{ STR_W_LEN("non_lazy_connections_slave_success") },
	{ STR_W_LEN("non_lazy_connections_slave_failure") },
	{ STR_W_LEN("non_lazy_connections_master_success") },
	{ STR_W_LEN("non_lazy_connections_master_failure") },
	{ STR_W_LEN("lazy_connections_slave_success") },
	{ STR_W_LEN("lazy_connections_slave_failure") },
	{ STR_W_LEN("lazy_connections_master_success") },
	{ STR_W_LEN("lazy_connections_master_failure") },
	{ STR_W_LEN("trx_autocommit_on") },
	{ STR_W_LEN("trx_autocommit_off") },
	{ STR_W_LEN("trx_master_forced") },
#ifndef MYSQLND_HAS_INJECTION_FEATURE
	/* TODO: document */
	{ STR_W_LEN("gtid_autocommit_injections_success") },
	{ STR_W_LEN("gtid_autocommit_injections_failure") },
	{ STR_W_LEN("gtid_commit_injections_success") },
	{ STR_W_LEN("gtid_commit_injections_failure") },
	{ STR_W_LEN("gtid_implicit_commit_injections_success") },
	{ STR_W_LEN("gtid_implicit_commit_injections_failure") },
#endif
	{ STR_W_LEN("transient_error_retries") },
	{ STR_W_LEN("fabric_sharding_lookup_servers_success") },
	{ STR_W_LEN("fabric_sharding_lookup_servers_failure") },
	{ STR_W_LEN("fabric_sharding_lookup_servers_time_total") },
	{ STR_W_LEN("fabric_sharding_lookup_servers_bytes_total") },
	{ STR_W_LEN("fabric_sharding_lookup_servers_xml_failure") },
	{ STR_W_LEN("xa_begin") },
	{ STR_W_LEN("xa_commit_success") },
	{ STR_W_LEN("xa_commit_failure") },
	{ STR_W_LEN("xa_rollback_success") },
	{ STR_W_LEN("xa_rollback_failure") },
	{ STR_W_LEN("xa_participants") },
	{ STR_W_LEN("xa_rollback_on_close") },
	{ STR_W_LEN("pool_masters_total") },
	{ STR_W_LEN("pool_slaves_total") },
	{ STR_W_LEN("pool_masters_active") },
	{ STR_W_LEN("pool_slaves_active") },
	{ STR_W_LEN("pool_updates") },
	{ STR_W_LEN("pool_master_reactivated") },
	{ STR_W_LEN("pool_slave_reactivated") }
};
/* }}} */


ZEND_DECLARE_MODULE_GLOBALS(mysqlnd_ms)

unsigned int mysqlnd_ms_plugin_id;

// BEGIN HACK
//static zend_bool mysqlnd_ms_global_config_loaded = FALSE;
static time_t mysqlnd_ms_global_config_loaded = 0;
// END HACK
struct st_mysqlnd_ms_json_config * mysqlnd_ms_json_config = NULL;

// BEGIN HACK
static void mysqlnd_ms_check_config(TSRMLS_D) /* {{{ */
{
	char * json_file_name = INI_STR("mysqlnd_ms.config_file");
	if (json_file_name) {
	    struct stat file_stat;
        int err = stat(json_file_name, &file_stat);
		if (file_stat.st_mtime > mysqlnd_ms_global_config_loaded) {
		/*	FILE *fp;
			fp=fopen("/tmp/mymysqlnd.log", "a");
					fprintf(fp, "Read conf file\n");
			fclose(fp); */
			mysqlnd_ms_config_json_free(mysqlnd_ms_json_config TSRMLS_CC);
			mysqlnd_ms_json_config = mysqlnd_ms_config_json_init(TSRMLS_C);
			mysqlnd_ms_config_json_load_configuration(mysqlnd_ms_json_config TSRMLS_CC);
			mysqlnd_ms_global_config_loaded = file_stat.st_mtime;
		}
	}
}
/* }}} */
// END HACK

/* {{{ php_mysqlnd_ms_config_globals_ctor */
static void
mysqlnd_ms_globals_ctor(zend_mysqlnd_ms_globals * mysqlnd_ms_globals TSRMLS_DC)
{
	mysqlnd_ms_globals->enable = FALSE;
	mysqlnd_ms_globals->force_config_usage = FALSE;
	mysqlnd_ms_globals->config_file = NULL;
	mysqlnd_ms_globals->collect_statistics = FALSE;
	mysqlnd_ms_globals->multi_master = FALSE;
	mysqlnd_ms_globals->disable_rw_split = FALSE;
	mysqlnd_ms_globals->config_startup_error = NULL;
	// BEGIN HACK
	mysqlnd_ms_globals->master_on = NULL;
	mysqlnd_ms_globals->inject_on = NULL;
	mysqlnd_ms_globals->config_dir = NULL;
	// END HACK
}
/* }}} */

PHP_GINIT_FUNCTION(mysqlnd_ms) {
	mysqlnd_ms_globals_ctor(mysqlnd_ms_globals TSRMLS_CC);
}

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(mysqlnd_ms)
{
	if (MYSQLND_MS_G(enable)) {

		zend_hash_init(&MYSQLND_MS_G(xa_state_stores), 0, NULL, mysqlnd_ms_xa_gc_hash_dtor, 1);
		MYSQLND_MS_CONFIG_JSON_LOCK(mysqlnd_ms_json_config);
		// BEGIN HACK
		//if (FALSE == mysqlnd_ms_global_config_loaded) {

		//	mysqlnd_ms_config_json_load_configuration(mysqlnd_ms_json_config TSRMLS_CC);
		//	mysqlnd_ms_global_config_loaded = TRUE;

		//}
		mysqlnd_ms_check_config(TSRMLS_C);
		// END HACK
		MYSQLND_MS_CONFIG_JSON_UNLOCK(mysqlnd_ms_json_config);
	}
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION */
PHP_RSHUTDOWN_FUNCTION(mysqlnd_ms)
{
	DBG_ENTER("mysqlnd_ms_shutdown");
	if (MYSQLND_MS_G(enable)) {

		zend_hash_destroy(&MYSQLND_MS_G(xa_state_stores));

		if (MYSQLND_MS_G(config_startup_error)) {
			mnd_sprintf_free(MYSQLND_MS_G(config_startup_error));
		}
	}

	DBG_RETURN(SUCCESS);
}
/* }}} */


/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
	STD_PHP_INI_BOOLEAN("mysqlnd_ms.enable", "0", PHP_INI_SYSTEM, OnUpdateBool, enable, zend_mysqlnd_ms_globals, mysqlnd_ms_globals)
	STD_PHP_INI_ENTRY("mysqlnd_ms.force_config_usage", "0", PHP_INI_SYSTEM, OnUpdateBool, force_config_usage, zend_mysqlnd_ms_globals, mysqlnd_ms_globals)
	STD_PHP_INI_ENTRY("mysqlnd_ms.config_file", NULL, PHP_INI_SYSTEM, OnUpdateString, config_file, zend_mysqlnd_ms_globals, mysqlnd_ms_globals)
	STD_PHP_INI_ENTRY("mysqlnd_ms.collect_statistics", "0", PHP_INI_SYSTEM, OnUpdateBool, collect_statistics, zend_mysqlnd_ms_globals, mysqlnd_ms_globals)
	STD_PHP_INI_ENTRY("mysqlnd_ms.multi_master", "0", PHP_INI_SYSTEM, OnUpdateBool, multi_master, zend_mysqlnd_ms_globals, mysqlnd_ms_globals)
	STD_PHP_INI_ENTRY("mysqlnd_ms.disable_rw_split", "0", PHP_INI_SYSTEM, OnUpdateBool, disable_rw_split, zend_mysqlnd_ms_globals, mysqlnd_ms_globals)
	// BEGIN HACK
	STD_PHP_INI_ENTRY("mysqlnd_ms.master_on", NULL, PHP_INI_SYSTEM, OnUpdateString, master_on, zend_mysqlnd_ms_globals, mysqlnd_ms_globals)
	STD_PHP_INI_ENTRY("mysqlnd_ms.inject_on", NULL, PHP_INI_SYSTEM, OnUpdateString, inject_on, zend_mysqlnd_ms_globals, mysqlnd_ms_globals)
	STD_PHP_INI_ENTRY("mysqlnd_ms.config_dir", NULL, PHP_INI_SYSTEM, OnUpdateString, config_dir, zend_mysqlnd_ms_globals, mysqlnd_ms_globals)
	// END HACK
PHP_INI_END()
/* }}} */


/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(mysqlnd_ms)
{

	ZEND_INIT_MODULE_GLOBALS(mysqlnd_ms, mysqlnd_ms_globals_ctor, NULL);
	REGISTER_INI_ENTRIES();

	if (MYSQLND_MS_G(enable)) {
		mysqlnd_ms_plugin_id = mysqlnd_plugin_register();
		mysqlnd_ms_register_hooks();
#if PHP_MAJOR_VERSION < 7
		mysqlnd_stats_init(&mysqlnd_ms_stats, MS_STAT_LAST);
#else
		mysqlnd_stats_init(&mysqlnd_ms_stats, MS_STAT_LAST, 1);
#endif
		mysqlnd_ms_json_config = mysqlnd_ms_config_json_init(TSRMLS_C);
	}

	REGISTER_STRING_CONSTANT("MYSQLND_MS_VERSION", PHP_MYSQLND_MS_VERSION, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MYSQLND_MS_VERSION_ID", MYSQLND_MS_VERSION_ID, CONST_CS | CONST_PERSISTENT);

	REGISTER_STRING_CONSTANT("MYSQLND_MS_MASTER_SWITCH", MASTER_SWITCH, CONST_CS | CONST_PERSISTENT);
	REGISTER_STRING_CONSTANT("MYSQLND_MS_SLAVE_SWITCH", SLAVE_SWITCH, CONST_CS | CONST_PERSISTENT);
	REGISTER_STRING_CONSTANT("MYSQLND_MS_LAST_USED_SWITCH", LAST_USED_SWITCH, CONST_CS | CONST_PERSISTENT);
// BEGIN HACK
	REGISTER_STRING_CONSTANT("MYSQLND_MS_STRONG_SWITCH", STRONG_SWITCH, CONST_CS | CONST_PERSISTENT);
	REGISTER_STRING_CONSTANT("MYSQLND_MS_SESSION_SWITCH", SESSION_SWITCH, CONST_CS | CONST_PERSISTENT);
	REGISTER_STRING_CONSTANT("MYSQLND_MS_EVENTUAL_SWITCH", EVENTUAL_SWITCH, CONST_CS | CONST_PERSISTENT);

	REGISTER_STRING_CONSTANT("MYSQLND_MS_NOINJECT_SWITCH", NOINJECT_SWITCH, CONST_CS | CONST_PERSISTENT);
	REGISTER_STRING_CONSTANT("MYSQLND_MS_INJECT_SWITCH", INJECT_SWITCH, CONST_CS | CONST_PERSISTENT);
// END HACK
#ifdef ALL_SERVER_DISPATCH
	REGISTER_STRING_CONSTANT("MYSQLND_MS_ALL_SERVER_SWITCH", ALL_SERVER_SWITCH, CONST_CS | CONST_PERSISTENT);
#endif
	REGISTER_LONG_CONSTANT("MYSQLND_MS_QUERY_USE_MASTER", USE_MASTER, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MYSQLND_MS_QUERY_USE_SLAVE", USE_SLAVE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MYSQLND_MS_QUERY_USE_LAST_USED", USE_LAST_USED, CONST_CS | CONST_PERSISTENT);

#ifdef MYSQLND_MS_HAVE_FILTER_TABLE_PARTITION
	REGISTER_LONG_CONSTANT("MYSQLND_MS_HAVE_FILTER_TABLE_PARTITION", 1, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef MYSQLND_MS_HAVE_MYSQLND_QC
	REGISTER_LONG_CONSTANT("MYSQLND_MS_HAVE_CACHE_SUPPORT", 1, CONST_CS | CONST_PERSISTENT);
#endif

#if PHP_VERSION_ID >= 50399
	REGISTER_LONG_CONSTANT("MYSQLND_MS_QOS_CONSISTENCY_STRONG", CONSISTENCY_STRONG, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MYSQLND_MS_QOS_CONSISTENCY_SESSION", CONSISTENCY_SESSION, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL", CONSISTENCY_EVENTUAL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MYSQLND_MS_QOS_OPTION_GTID", QOS_OPTION_GTID, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MYSQLND_MS_QOS_OPTION_AGE", QOS_OPTION_AGE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MYSQLND_MS_QOS_OPTION_CACHE", QOS_OPTION_CACHE, CONST_CS | CONST_PERSISTENT);
#endif

	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(mysqlnd_ms)
{
	UNREGISTER_INI_ENTRIES();
	if (MYSQLND_MS_G(enable)) {
#if PHP_MAJOR_VERSION < 7
		mysqlnd_stats_end(mysqlnd_ms_stats);
#else
		mysqlnd_stats_end(mysqlnd_ms_stats, 1);
#endif

		mysqlnd_ms_config_json_free(mysqlnd_ms_json_config TSRMLS_CC);
		mysqlnd_ms_json_config = NULL;
	}
	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(mysqlnd_ms)
{
	char buf[64];

	php_info_print_table_start();
	php_info_print_table_header(2, "mysqlnd_ms support", "enabled");
	snprintf(buf, sizeof(buf), "%s (%d)", PHP_MYSQLND_MS_VERSION, MYSQLND_MS_VERSION_ID);
	php_info_print_table_row(2, "Mysqlnd master/slave plugin version", buf);
	php_info_print_table_row(2, "Plugin active", MYSQLND_MS_G(enable) ? "yes" : "no");
#if PHP_VERSION_ID >= 50399
	php_info_print_table_row(2, "Transaction mode trx_stickiness supported", "yes");
	php_info_print_table_row(2, "mysqlnd_ms_get_last_used_connection() supported", "yes");
	php_info_print_table_row(2, "mysqlnd_ms_set_qos() supported", "yes");
// BEGIN HACK
	php_info_print_table_row(2, "mysqlnd_ms_set_trx() supported", "yes");
	php_info_print_table_row(2, "mysqlnd_ms_unset_trx() supported", "yes");
// END HACK
#else
	php_info_print_table_row(2, "Transaction mode trx_stickiness supported", "no");
	php_info_print_table_row(2, "mysqlnd_ms_get_last_used_connection() supported", "no");
	php_info_print_table_row(2, "mysqlnd_ms_set_qos() supported", "no");
// BEGIN HACK
	php_info_print_table_row(2, "mysqlnd_ms_set_trx() supported", "no");
	php_info_print_table_row(2, "mysqlnd_ms_unset_trx() supported", "no");
// END HACK
#endif
	php_info_print_table_row(2, "Table partitioning filter supported",
#ifdef MYSQLND_MS_HAVE_FILTER_TABLE_PARTITION
		"yes"
#else
		"no"
#endif
	);
	php_info_print_table_row(2, "Query caching through mysqlnd_qc supported",
#ifdef MYSQLND_MS_HAVE_MYSQLND_QC
		"yes"
#else
		"no"
#endif
	);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlnd_ms_match_wild, 0, 0, 2)
  ZEND_ARG_INFO(0, haystack)
  ZEND_ARG_INFO(0, wild)
ZEND_END_ARG_INFO()

/* {{{ proto long mysqlnd_ms_match_wild(string haystack, string wild)
   */
static PHP_FUNCTION(mysqlnd_ms_match_wild)
{
	char * str;
	char * wild;
	_ms_size_type tmp;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &str, &tmp, &wild, &tmp) == FAILURE) {
		return;
	}

	RETURN_BOOL(mysqlnd_ms_match_wild(str, wild TSRMLS_CC));
}
/* }}} */

#if PHP_VERSION_ID > 50399

#if PHP_VERSION_ID >= 50600
static MYSQLND *zval_to_mysqlnd_inherited(zval *zv TSRMLS_DC) /* {{{ */
{
	unsigned int client_api_capabilities, tmp;
	MYSQLND * conn = zval_to_mysqlnd(zv, 0, &client_api_capabilities TSRMLS_CC);
	if (conn) {
		conn = zval_to_mysqlnd(zv, client_api_capabilities, &tmp TSRMLS_CC);
	}
	return conn;
}
/* }}} */
#elif PHP_VERSION_ID > 50500
static MYSQLND *zval_to_mysqlnd_inherited(zval *zv TSRMLS_DC) /* {{{ */
{
	return zval_to_mysqlnd(zv TSRMLS_CC);
}
/* }}} */
#elif PHP_VERSION_ID <= 50500
static MYSQLND *zval_to_mysqlnd_inherited(zval *zv TSRMLS_DC) /* {{{ */
{
	zend_class_entry *root_ce;

	if (Z_TYPE_P(zv) != IS_OBJECT || !Z_OBJCE_P(zv)->parent) {
		/* This is not an object or it is a non-inherited object, we can use the default implementation without hacks */
		return zval_to_mysqlnd(zv TSRMLS_CC);
	}

	root_ce = Z_OBJCE_P(zv)->parent;
	while (root_ce->parent) {
		root_ce = root_ce->parent;
	}

	if (root_ce->type != ZEND_INTERNAL_CLASS) {
		/* This can neither be mysqli nor pdo */
		return NULL;
	}

	if (!strcmp("mysqli", root_ce->name) || !strcmp("PDO", root_ce->name)) {
		MYSQLND *retval;
		zend_class_entry *orig_ce = Z_OBJCE_P(zv);
		zend_object *object = zend_object_store_get_object(zv TSRMLS_CC);
		object->ce = root_ce;
		retval = zval_to_mysqlnd(zv TSRMLS_CC);
		object->ce = orig_ce;
		return retval;
	} else {
		return zval_to_mysqlnd(zv TSRMLS_CC);
	}
}
/* }}} */
#endif

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlnd_ms_get_last_used_connection, 0, 0, 1)
	ZEND_ARG_INFO(0, object)
ZEND_END_ARG_INFO()


/* {{{ proto array mysqlnd_ms_get_last_used_connection(object handle)
   */
static PHP_FUNCTION(mysqlnd_ms_get_last_used_connection)
{
	zval * handle;
	MYSQLND * proxy_conn;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &handle) == FAILURE) {
		return;
	}
	if (!(proxy_conn = zval_to_mysqlnd_inherited(handle TSRMLS_CC))) {
		RETURN_FALSE;
	}
	{
		MYSQLND_MS_CONN_DATA ** conn_data = (MYSQLND_MS_CONN_DATA **) _ms_mysqlnd_plugin_get_plugin_connection_data_data(proxy_conn->data, mysqlnd_ms_plugin_id);
		const MYSQLND_CONN_DATA * conn = (conn_data && (*conn_data) && (*conn_data)->stgy.last_used_conn)? (*conn_data)->stgy.last_used_conn:proxy_conn->data;

		array_init(return_value);
		MYSQLND_MS_ADD_ASSOC_CONN_STRING(return_value, "scheme", conn->scheme);
		MYSQLND_MS_ADD_ASSOC_STRING(return_value, "host_info", conn->host_info);
#if PHP_VERSION_ID < 70100
		MYSQLND_MS_ADD_ASSOC_CONN_STRING(return_value, "host", conn->host);
#else
		MYSQLND_MS_ADD_ASSOC_CONN_STRING(return_value, "host", conn->hostname);
#endif
		MYSQLND_MS_ADD_ASSOC_LONG(return_value, "port", conn->port);
		MYSQLND_MS_ADD_ASSOC_CONN_STRING(return_value, "socket_or_pipe", conn->unix_socket);
		MYSQLND_MS_ADD_ASSOC_LONG(return_value, "thread_id", conn->thread_id);
		MYSQLND_MS_ADD_ASSOC_CONN_STRING(return_value, "last_message", conn->last_message);
		MYSQLND_MS_ADD_ASSOC_LONG(return_value, "errno", MYSQLND_MS_ERROR_INFO(conn).error_no);
		MYSQLND_MS_ADD_ASSOC_STRING(return_value, "error", (char *) MYSQLND_MS_ERROR_INFO(conn).error);
		MYSQLND_MS_ADD_ASSOC_STRING(return_value, "sqlstate", (char *) MYSQLND_MS_ERROR_INFO(conn).sqlstate);
	}
}
/* }}} */


ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlnd_ms_get_last_gtid, 0, 0, 1)
	ZEND_ARG_INFO(0, object)
ZEND_END_ARG_INFO()


/* {{{ proto string mysqlnd_ms_last_gtid(object handle)
   */
static PHP_FUNCTION(mysqlnd_ms_get_last_gtid)
{
	zval * handle;
	MYSQLND * proxy_conn;
	MYSQLND_CONN_DATA * conn = NULL;
	MYSQLND_MS_CONN_DATA ** conn_data = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &handle) == FAILURE) {
		return;
	}
	if (!(proxy_conn = zval_to_mysqlnd_inherited(handle TSRMLS_CC))) {
		RETURN_FALSE;
	}
	// BEGIN HACK
	/*
	{
		MYSQLND_RES * res = NULL;
		zval * row;
		zval ** gtid;

		conn_data = (MYSQLND_MS_CONN_DATA **) mysqlnd_plugin_get_plugin_connection_data_data(proxy_conn->data, mysqlnd_ms_plugin_id);
		if (!conn_data || !(*conn_data)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection or no statement has been run yet");
			RETURN_FALSE;
		}

		if (!(*conn_data)->stgy.last_used_conn) {
  			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection or no ID has been injected yet");
			RETURN_FALSE;
		}
		conn = (*conn_data)->stgy.last_used_conn;
		MS_LOAD_CONN_DATA(conn_data, conn);

		if (!conn_data || !(*conn_data)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to fetch plugin data. Please report a bug");
			RETURN_FALSE;
		}

		if (!(*conn_data)->global_trx.fetch_last_gtid) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "SQL to fetch last global transaction ID is not set");
			RETURN_FALSE;
		}

		(*conn_data)->skip_ms_calls = TRUE;
		if (PASS != MS_CALL_ORIGINAL_CONN_DATA_METHOD(send_query)(conn, (*conn_data)->global_trx.fetch_last_gtid, (*conn_data)->global_trx.fetch_last_gtid_len TSRMLS_CC)) {
			goto getlastidfailure;
		}

		if (PASS !=  MS_CALL_ORIGINAL_CONN_DATA_METHOD(reap_query)(conn TSRMLS_CC)) {
			goto getlastidfailure;
		}
#if PHP_VERSION_ID < 50600
		if (!(res = MS_CALL_ORIGINAL_CONN_DATA_METHOD(store_result)(conn TSRMLS_CC))) {
#else
		if (!(res = MS_CALL_ORIGINAL_CONN_DATA_METHOD(store_result)(conn, MYSQLND_STORE_NO_COPY TSRMLS_CC))) {
#endif
			goto getlastidfailure;
		}

		(*conn_data)->skip_ms_calls = FALSE;

		MAKE_STD_ZVAL(row);
		mysqlnd_fetch_into(res, MYSQLND_FETCH_NUM, row, MYSQLND_MYSQL);
		if (Z_TYPE_P(row) != IS_ARRAY) {
			zval_ptr_dtor(&row);
			res->m.free_result(res, FALSE TSRMLS_CC);
			goto getlastidfailure;
		}

		if (SUCCESS == zend_hash_index_find(Z_ARRVAL_P(row), 0, (void**)&gtid)) {
			RETVAL_ZVAL(*gtid, 1, NULL);
			zval_ptr_dtor(&row);
			res->m.free_result(res, FALSE TSRMLS_CC);
			return;
		} else {
			//no error code set on line, we need to bail explicitly 
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to read GTID from result set. Please report a bug");
		}
	}

getlastidfailure:
	if (conn_data && (*conn_data)) {
		(*conn_data)->skip_ms_calls = FALSE;
	}
	RETURN_FALSE;
	 */
	conn_data = (MYSQLND_MS_CONN_DATA **) _ms_mysqlnd_plugin_get_plugin_connection_data_data(proxy_conn->data, mysqlnd_ms_plugin_id);
	if (!conn_data || !(*conn_data)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection or no statement has been run yet");
		RETURN_FALSE;
	}
	if (!(*conn_data)->stgy.last_used_conn) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection or no ID has been injected yet");
		RETURN_FALSE;
	}
	conn = (*conn_data)->stgy.last_used_conn;
	MS_LOAD_CONN_DATA(conn_data, conn);
	if (!conn_data || !(*conn_data)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to fetch plugin data. Please report a bug");
		RETURN_FALSE;
	}
	{
		char * gtid = NULL;
		if ((*conn_data)->global_trx.type != GTID_NONE && ((*conn_data)->global_trx.last_wgtid)) {
			_MS_RETURN_STRING((*conn_data)->global_trx.last_wgtid);
		} else {
			// no error code set on line, we need to bail explicitly
			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Fail or no ID has been injected yet");
		}
	}
	RETURN_FALSE;
	// END HACK
}
/* }}} */


ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlnd_ms_set_qos, 0, 0, 2)
	ZEND_ARG_INFO(0, object)
	ZEND_ARG_INFO(0, service_level)
	ZEND_ARG_INFO(0, option)
	ZEND_ARG_INFO(0, option_value)
ZEND_END_ARG_INFO()


/* {{{ proto bool mysqlnd_ms_set_qos()
   */
static PHP_FUNCTION(mysqlnd_ms_set_qos)
{
	zval * handle;
	double option;
	zval * option_value = NULL;
	double service_level;
	MYSQLND * proxy_conn;
	MYSQLND_MS_FILTER_QOS_OPTION_DATA option_data;


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zd|dz!", &handle, &service_level, &option, &option_value) == FAILURE) {
		return;
	}

	if (ZEND_NUM_ARGS() > 2) {
		  switch ((int)option) {
			case QOS_OPTION_GTID:
				if (service_level != CONSISTENCY_SESSION) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "GTID option value must be used with MYSQLND_MS_QOS_CONSISTENCY_SESSION only");
					RETURN_FALSE;
				}
				if (!option_value) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Option value required");
					RETURN_FALSE;
				}

				if ((Z_TYPE_P(option_value) != IS_STRING) &&
						(Z_TYPE_P(option_value) != IS_LONG) && (Z_TYPE_P(option_value) != IS_DOUBLE)) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "GTID must be a number or a string");
				}

				convert_to_string(option_value);
				option_data.gtid_len = spprintf(&(option_data.gtid), 0, "%s", Z_STRVAL_P(option_value));
				if (0 == option_data.gtid_len) {
					efree(option_data.gtid);
					option_data.gtid = NULL;
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "GTID is empty");
					RETURN_FALSE;
				}
				break;

			case QOS_OPTION_AGE:
				if (service_level != CONSISTENCY_EVENTUAL) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Maximum age option value must be used with MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL only");
					RETURN_FALSE;
				}
				if (!option_value) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Option value required");
					RETURN_FALSE;
				}
				convert_to_long(option_value);
				option_data.age = Z_LVAL_P(option_value);
				if (option_data.age < 0L) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Maximum age must have a positive value");
					RETURN_FALSE;
				}
				break;

			case QOS_OPTION_CACHE:
#ifdef MYSQLND_MS_HAVE_MYSQLND_QC
				if (service_level != CONSISTENCY_EVENTUAL) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cache TTL option value must be used with MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL only");
					RETURN_FALSE;
				}
				if (!option_value) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Option value required");
					RETURN_FALSE;
				}
				convert_to_long(option_value);
				option_data.ttl = (uint)Z_LVAL_P(option_value);
				if (option_data.ttl < 1) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cache TTL must be at least one");
					RETURN_FALSE;
				}
#else
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cache support is not available with this build");
				RETURN_FALSE;
#endif
				break;

			default:
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid option");
				RETURN_FALSE;
				break;
		  }
	} else {
		option = QOS_OPTION_NONE;
	}

	if (!(proxy_conn = zval_to_mysqlnd_inherited(handle TSRMLS_CC))) {
		RETURN_FALSE;
	}

	{
		MYSQLND_MS_CONN_DATA ** conn_data = NULL;
		conn_data = (MYSQLND_MS_CONN_DATA **) _ms_mysqlnd_plugin_get_plugin_connection_data_data(proxy_conn->data, mysqlnd_ms_plugin_id);
		if (!conn_data || !(*conn_data)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection");
			RETURN_FALSE;
		}

		if ((*conn_data)->stgy.in_transaction && (*conn_data)->stgy.trx_stop_switching) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No change allowed in the middle of a transaction");
			RETURN_FALSE;
		}
	}

	switch ((int)service_level)
	{
		case CONSISTENCY_STRONG:
			if (PASS == mysqlnd_ms_section_filters_prepend_qos(proxy_conn,
					(enum mysqlnd_ms_filter_qos_consistency)service_level,
					(enum mysqlnd_ms_filter_qos_option)option, &option_data TSRMLS_CC))
				RETURN_TRUE;
			break;

		case CONSISTENCY_EVENTUAL:
			/* GTID is free'd by the function called */
			if (PASS == mysqlnd_ms_section_filters_prepend_qos(proxy_conn,
					(enum mysqlnd_ms_filter_qos_consistency)service_level,
					(enum mysqlnd_ms_filter_qos_option)option, &option_data TSRMLS_CC))
				RETURN_TRUE;
			break;

		case CONSISTENCY_SESSION:
			if (PASS == mysqlnd_ms_section_filters_prepend_qos(proxy_conn,
					(enum mysqlnd_ms_filter_qos_consistency)service_level,
					(enum mysqlnd_ms_filter_qos_option)option, &option_data TSRMLS_CC))
				RETURN_TRUE;
			break;

		default:
			/* TODO: decide wheter warning, error or nothing */
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid service level");
			RETURN_FALSE;
			break;
	}
	RETURN_FALSE;
}
/* }}} */

// BEGIN HACK
ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlnd_ms_set_trx, 0, 0, 2)
	ZEND_ARG_INFO(0, connection)
	ZEND_ARG_INFO(0, read_only)
ZEND_END_ARG_INFO()


/* {{{ proto bool mysqlnd_ms_set_trx()
   */
static PHP_FUNCTION(mysqlnd_ms_set_trx)
{
	zval * handle;
	MYSQLND * proxy_conn;
	zend_bool ro = FALSE;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|b", &handle, &ro) == FAILURE) {
		return;
	}
	if (!(proxy_conn = zval_to_mysqlnd_inherited(handle TSRMLS_CC))) {
		RETURN_FALSE;
	}
	if (PASS != mysqlnd_ms_set_tx(proxy_conn->data, ro)){
		RETURN_FALSE;
	}
	RETURN_TRUE;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlnd_ms_unset_trx, 0, 0, 1)
	ZEND_ARG_INFO(0, connection)
	ZEND_ARG_INFO(0, commit)
ZEND_END_ARG_INFO()


/* {{{ proto bool mysqlnd_ms_unset_trx()
   */
static PHP_FUNCTION(mysqlnd_ms_unset_trx)
{
	zval * handle;
	MYSQLND * proxy_conn;
	zend_bool co = TRUE;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|b", &handle, &co) == FAILURE) {
		return;
	}
	if (!(proxy_conn = zval_to_mysqlnd_inherited(handle TSRMLS_CC))) {
		RETURN_FALSE;
	}
	if (PASS != mysqlnd_ms_unset_tx(proxy_conn->data, co)){
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */
// END HACK
#endif

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlnd_ms_query_is_select, 0, 0, 1)
	ZEND_ARG_INFO(0, query)
ZEND_END_ARG_INFO()


/* {{{ proto long mysqlnd_ms_query_is_select(string query)
   Parse query and propose where to send it */
static PHP_FUNCTION(mysqlnd_ms_query_is_select)
{
	char * query = NULL;
	_ms_size_type query_len;
	zend_bool forced;
	DBG_ENTER("mysqlnd_ms_query_is_select");
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &query, &query_len) == FAILURE) {
		DBG_VOID_RETURN;
	}
	DBG_INF_FMT("Query %s", query);

	RETVAL_LONG(mysqlnd_ms_query_is_select(query, query_len, &forced TSRMLS_CC));
	DBG_VOID_RETURN;
}
/* }}} */


ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlnd_ms_get_stats, 0, 0, 0)
ZEND_END_ARG_INFO()


/* {{{ proto array mysqlnd_ms_get_stats()
    Return statistics on connections and queries */
static PHP_FUNCTION(mysqlnd_ms_get_stats)
{
	DBG_ENTER("mysqlnd_ms_get_stats");
	if (zend_parse_parameters_none() == FAILURE) {
		DBG_VOID_RETURN;
	}

	if (!MYSQLND_MS_G(enable)) {
		DBG_VOID_RETURN;
	}

	mysqlnd_fill_stats_hash(mysqlnd_ms_stats, mysqlnd_ms_stats_values_names, return_value TSRMLS_CC ZEND_FILE_LINE_CC);

	DBG_VOID_RETURN;
}
/* }}} */

static void mysqlnd_ms_fabric_select_servers(zval *return_value, zval *conn_zv, char *table, char *key, enum mysqlnd_fabric_hint hint TSRMLS_DC) /* {{{ */
{
	MYSQLND *proxy_conn;
	MYSQLND_MS_CONN_DATA **conn_data = NULL;
	mysqlnd_fabric_server *servers, *tofree;
	mysqlnd_fabric *fabric;
	_ms_smart_type hash_key = {0};
	unsigned int server_counter = 0;
	zend_bool exists = FALSE, is_master = FALSE, is_active = FALSE, is_removed = FALSE;
	MYSQLND_MS_LIST_DATA * data;

	DBG_ENTER("mysqlnd_ms_fabric_select_servers");

	if (!(proxy_conn = zval_to_mysqlnd_inherited(conn_zv TSRMLS_CC))) {
		RETVAL_FALSE;
		DBG_VOID_RETURN;
	}

	conn_data = (MYSQLND_MS_CONN_DATA **) _ms_mysqlnd_plugin_get_plugin_connection_data_data(proxy_conn->data, mysqlnd_ms_plugin_id);
	if (!conn_data || !(*conn_data)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection");
		RETVAL_FALSE;
		DBG_VOID_RETURN;
	}

	if (!(*conn_data)->fabric) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Connection is not configured to use MySQL Fabric");
		RETVAL_FALSE;
		DBG_VOID_RETURN;
	}
	fabric = (*conn_data)->fabric;

	if (mysqlnd_fabric_get_trx_warn_serverlist_changes(fabric) && ((*conn_data)->stgy.trx_stop_switching))  {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Fabric server exchange in the middle of a transaction");
	}

	/* Can't be any active connection past this point, user has zero connections to choose from */
	if (PASS != ((*conn_data)->pool->flush_active((*conn_data)->pool TSRMLS_CC))) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, MYSQLND_MS_ERROR_PREFIX " Failed to flush connection pool");
	}

	tofree = servers = mysqlnd_fabric_get_shard_servers(fabric, table, key, hint);
	if (mysqlnd_fabric_get_error_no(fabric) > 0) {
		/*
		TODO - should be bubble this up to the connection?
		MYSQLND_ERROR_INFO * error_info = &MYSQLND_MS_ERROR_INFO(proxy_conn->data);
		if (error_info) {
			SET_CLIENT_ERROR((*error_info), fabric->error_no, fabric->sqlstate, fabric->error);
		}
		*/
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s %s", MYSQLND_MS_ERROR_PREFIX, mysqlnd_fabric_get_error(fabric));
		RETVAL_FALSE;
		DBG_VOID_RETURN;
	}
	if (!servers) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " Didn't receive usable servers from MySQL Fabric");
		RETVAL_FALSE;
		DBG_VOID_RETURN;
	}

	for (; servers->hostname && *servers->hostname; servers++, server_counter++) {
#if PHP_VERSION_ID >= 50600
		MYSQLND *conn = mysqlnd_init(proxy_conn->data->m->get_client_api_capabilities(proxy_conn->data TSRMLS_CC), proxy_conn->data->persistent);
#else
		MYSQLND *conn = mysqlnd_init(proxy_conn->data->persistent);
#endif
		char * unique_name_from_config;

		/* FIXME: This assumes we get servers always in the same order... worst case we don't and pool grows */
		mnd_sprintf(&unique_name_from_config, 0, "%s%d", servers->hostname, server_counter);

		/* TODO: Don't know whether Fabric is using cred.db or cred.mysql_flags */

		(*conn_data)->pool->get_conn_hash_key(&hash_key, unique_name_from_config,
											servers->hostname, MYSQLND_MS_CONN_STRING((*conn_data)->cred.user),
											MYSQLND_MS_CONN_STRING((*conn_data)->cred.passwd), MYSQLND_MS_CONN_STRING_LEN((*conn_data)->cred.passwd),
											servers->port, NULL /* socket */,
											NULL /* db */, 0 /* db_len */,
											0 /* flags */,
											proxy_conn->data->persistent);
// MI SEMBRA PROPRIO CHE NON POSSA FUNZIONARE
		exists = (*conn_data)->pool->connection_exists((*conn_data)->pool, &hash_key, &data, &is_master, &is_active, &is_removed TSRMLS_CC);
		exists = (!is_active && !is_removed) ? TRUE : FALSE;
		if (exists) {
			/* Such a server has been added to the pool before */
			if (is_master && (servers->mode == READ_WRITE)) {
				/* ... and, the role has not changed */
				if (PASS == ((*conn_data)->pool->connection_reactivate((*conn_data)->pool, &hash_key, is_master TSRMLS_CC))) {
					/* welcome back server */
					exists = TRUE;
				} else {
					/* unlikely: unexplored territory */
					mnd_sprintf_free(unique_name_from_config);
					php_error_docref(NULL TSRMLS_CC, E_ERROR, MYSQLND_MS_ERROR_PREFIX " Failed to reactivate a server from the pool");
					RETVAL_FALSE;
					DBG_VOID_RETURN;
				}
			} else {
				/* this is unexplored territory: we can't simply reuse, its in the wrong list */
				if (PASS == ((*conn_data)->pool->connection_remove((*conn_data)->pool, &hash_key, is_master TSRMLS_CC))) {
					/* TODO see conn_pool.h - it may or may not actually removed or just marked for removal */
					exists = FALSE;
				}
			}
		}

		if (FALSE == exists) {
			MYSQLND_MS_CONN_DV_STRING(host);
			MYSQLND_MS_S_TO_CONN_STRING(host, servers->hostname);

			if (servers->mode == READ_WRITE) {
				mysqlnd_ms_connect_to_host_aux(proxy_conn->data, conn->data, unique_name_from_config, TRUE,
											host, servers->port, &(*conn_data)->cred, &(*conn_data)->global_trx,
											TRUE, proxy_conn->data->persistent TSRMLS_CC);
			} else {
				mysqlnd_ms_connect_to_host_aux(proxy_conn->data, conn->data, unique_name_from_config, FALSE,
											host, servers->port,  &(*conn_data)->cred, &(*conn_data)->global_trx,
											TRUE, proxy_conn->data->persistent TSRMLS_CC);
			}
		}
		mnd_sprintf_free(unique_name_from_config);

		conn->m->dtor(conn TSRMLS_CC);
	}

	mysqlnd_fabric_free_server_list(tofree);
	_ms_smart_method(free, &hash_key);

	/* FIXME - this will, almost for sure, replay too many commands. Note the filter argument */
	(*conn_data)->pool->replay_cmds((*conn_data)->pool, proxy_conn->data, NULL /* filter */ TSRMLS_CC);
	(*conn_data)->pool->notify_replace_listener((*conn_data)->pool TSRMLS_CC);

	RETVAL_TRUE;
	DBG_VOID_RETURN;
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlnd_ms_fabric_select_shard, 0, 0, 3)
	ZEND_ARG_INFO(0, connection)
    ZEND_ARG_INFO(0, table)
	ZEND_ARG_INFO(0, shard_key)
ZEND_END_ARG_INFO()

/* {{{ proto long mysqlnd_ms_fabric_select_shard(mixed connection, string table, string shard_key)
   Pick server configuration for a shard key */
static PHP_FUNCTION(mysqlnd_ms_fabric_select_shard)
{
	zval *conn_zv;
	char *table, *key;
	int table_len, key_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zss", &conn_zv, &table, &table_len, &key, &key_len) == FAILURE) {
		return;
	}

	mysqlnd_ms_fabric_select_servers(return_value, conn_zv, table, key, LOCAL TSRMLS_CC);
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlnd_ms_fabric_select_global, 0, 0, 2)
	ZEND_ARG_INFO(0, connection)
    ZEND_ARG_INFO(0, table)
ZEND_END_ARG_INFO()

/* {{{ proto long mysqlnd_ms_fabric_select_global(mixed connection, string table)
   Pick server configuration for a shard key */
static PHP_FUNCTION(mysqlnd_ms_fabric_select_global)
{
	zval *conn_zv;
	char *table;
	int table_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zs", &conn_zv, &table, &table_len) == FAILURE) {
		return;
	}

	mysqlnd_ms_fabric_select_servers(return_value, conn_zv, table, NULL, GLOBAL TSRMLS_CC);
}
/* }}} */

static void mysqlnd_ms_add_server_to_array(void *data, void *arg TSRMLS_DC) /* {{{ */
{
	zval _ms_p_zval host;
	MYSQLND_MS_LIST_DATA **element = (MYSQLND_MS_LIST_DATA **)data;
	zval *array = (zval *)arg;

	MAKE_STD_ZVAL(host);
	array_init(_ms_a_zval host);
	if ((*element)->name_from_config) {
		MYSQLND_MS_ADD_ASSOC_STRING(_ms_a_zval host, "name_from_config", (*element)->name_from_config);
	} else {
		add_assoc_null(_ms_a_zval host, "name_from_config");
	}

	MYSQLND_MS_ADD_ASSOC_CONN_STRING(_ms_a_zval host, "hostname", (*element)->host);

	if (MYSQLND_MS_CONN_STRING((*element)->user)) {
		MYSQLND_MS_ADD_ASSOC_CONN_STRING(_ms_a_zval host, "user", (*element)->user);
	} else {
		add_assoc_null(_ms_a_zval host, "user");
	}

	add_assoc_long(_ms_a_zval host, "port", (*element)->port);

	if (MYSQLND_MS_CONN_STRING((*element)->socket)) {
		MYSQLND_MS_ADD_ASSOC_CONN_STRING(_ms_a_zval host, "socket", (*element)->socket);
	} else {
		add_assoc_null(_ms_a_zval host, "socket");
	}

	add_next_index_zval(array, _ms_a_zval host);

	if (((*element)->conn) && (_MS_CONN_GET_STATE((*element)->conn) > CONN_ALLOCED)) {
		add_assoc_long(_ms_a_zval host, "thread_id", (*element)->conn->thread_id);
	} else {
		add_assoc_null(_ms_a_zval host, "thread_id");
	}
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlnd_ms_dump_servers, 0, 0, 1)
	ZEND_ARG_INFO(0, connection)
ZEND_END_ARG_INFO()

/* {{{ proto long mysqlnd_ms_dump_servers(mixed connection)
   Dump configured master and slave servers */
static PHP_FUNCTION(mysqlnd_ms_dump_servers)
{
	zval * conn_zv, _ms_p_zval masters, _ms_p_zval slaves;
	MYSQLND *conn;
	MYSQLND_MS_CONN_DATA **conn_data = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &conn_zv) == FAILURE) {
		return;
	}

	if (!(conn = zval_to_mysqlnd_inherited(conn_zv TSRMLS_CC))) {
		RETURN_FALSE;
	}

	conn_data = (MYSQLND_MS_CONN_DATA **) _ms_mysqlnd_plugin_get_plugin_connection_data_data(conn->data, mysqlnd_ms_plugin_id);
	if (!conn_data || !(*conn_data)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection");
		RETURN_FALSE;
	}

	MAKE_STD_ZVAL(masters);
	MAKE_STD_ZVAL(slaves);
	array_init(_ms_a_zval masters);
	array_init(_ms_a_zval slaves);

	zend_llist_apply_with_argument((*conn_data)->pool->get_active_masters((*conn_data)->pool TSRMLS_CC),
								   mysqlnd_ms_add_server_to_array, _ms_a_zval masters TSRMLS_CC);
	zend_llist_apply_with_argument((*conn_data)->pool->get_active_slaves((*conn_data)->pool TSRMLS_CC),
								   mysqlnd_ms_add_server_to_array, _ms_a_zval slaves TSRMLS_CC);

	array_init(return_value);
	add_assoc_zval(return_value, "masters", _ms_a_zval masters);
	add_assoc_zval(return_value, "slaves", _ms_a_zval slaves);
}
/* }}} */

static void mysqlnd_ms_dump_fabric_hosts_cb(const char *url, void *data) /* {{{ */
{
	zval _ms_p_zval item;
	zval *return_value = (zval*)data;
	DBG_ENTER("mysqlnd_ms_dump_fabric_hosts_cb");

	MAKE_STD_ZVAL(item);
	array_init(_ms_a_zval item);
	MYSQLND_MS_ADD_ASSOC_STRING(_ms_a_zval item, "url", (char*)url);
	add_next_index_zval(return_value,_ms_a_zval item);
	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ proto long mysqlnd_ms_dump_fabric_rpc_hosts(mixed connection)
   Dump configured master and slave servers */
static PHP_FUNCTION(mysqlnd_ms_dump_fabric_rpc_hosts)
{
	zval *conn_zv;
	MYSQLND *conn;
	MYSQLND_MS_CONN_DATA **conn_data = NULL;
	DBG_ENTER("mysqlnd_ms_dump_fabric_rpc_hosts");

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &conn_zv) == FAILURE) {
		return;
	}

	if (!(conn = zval_to_mysqlnd_inherited(conn_zv TSRMLS_CC))) {
		RETURN_FALSE;
	}

	conn_data = (MYSQLND_MS_CONN_DATA **) _ms_mysqlnd_plugin_get_plugin_connection_data_data(conn->data, mysqlnd_ms_plugin_id);
	if (!conn_data || !(*conn_data)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection");
		RETURN_FALSE;
	}

	if (!(*conn_data)->fabric) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No MySQL Fabric connection");
		RETURN_FALSE;
	}

	array_init(return_value);
	mysqlnd_fabric_host_list_apply((*conn_data)->fabric, mysqlnd_ms_dump_fabric_hosts_cb, return_value);
	DBG_VOID_RETURN;
}
/* }}} */

#ifdef PHP_DEBUG
/* {{{ proto void mysqlnd_ms_debug_set_fabric_raw_dump_data_xml(mixed connection, string shard_table, string shard_mapping_xml, string shard_index, string server)
   Set raw binary dump data for Fabric using XML. This is supposed to be used by testing
   so we don't have to use complex stream wrappers in all tests.
   This function is only available in debug builds so we assume the user knows what he does
   i.e. wedon't check whether dumpstrategy is actually used (-> buffer overflow etc. on misuse) */
static PHP_FUNCTION(mysqlnd_ms_debug_set_fabric_raw_dump_data_xml)
{
	zval *conn_zv;
	MYSQLND *conn;
	MYSQLND_MS_CONN_DATA **conn_data = NULL;
	char *shard_table_xml; size_t shard_table_len;
	char *shard_mapping_xml; size_t shard_mapping_len;
	char *shard_index_xml; size_t shard_index_len;
	char *server_xml; size_t server_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zssss", &conn_zv, &shard_table_xml, &shard_table_len,
			&shard_mapping_xml, &shard_mapping_len, &shard_index_xml, &shard_index_len, &server_xml, &server_len) == FAILURE) {
		return;
	}

	if (!(conn = zval_to_mysqlnd_inherited(conn_zv TSRMLS_CC))) {
		RETURN_FALSE;
	}

	conn_data = (MYSQLND_MS_CONN_DATA **) _ms_mysqlnd_plugin_get_plugin_connection_data_data(conn->data, mysqlnd_ms_plugin_id);
	if (!conn_data || !(*conn_data)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection");
		RETURN_FALSE;
	}

	if (!(*conn_data)->fabric) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No MySQL Fabric connection");
		RETURN_FALSE;
	}

/*	void fabric_set_raw_data_from_xmlstr(mysqlnd_fabric *fabric,
		const char *shard_table_xml, size_t shard_table_len,
		const char *shard_mapping_xml, size_t shard_mapping_len,
		const char *shard_index_xml, size_t shard_index_len,
		const char *server_xml, size_t server_len);*/
/*	fabric_set_raw_data_from_xmlstr((*conn_data)->fabric, shard_table_xml, shard_table_len, shard_mapping_xml,
			shard_mapping_len, shard_index_xml, shard_index_len, server_xml, server_len);
*/
}
/* }}} */


/* {{{ proto long mysqlnd_ms_debug_set_fabric_raw_dump_data_dangerous(mixed connection, string data)
   Set raw binary dump data for Fabric. Be careful data isn't really checked.
   The data has to match the architecture of the system (sizes, endianess, padding, ...)
   This function should not be used by new tests, use the XML version instead, it might be
   used for debugging, but more likely will be removed.
   */
static PHP_FUNCTION(mysqlnd_ms_debug_set_fabric_raw_dump_data_dangerous)
{
	zval *conn_zv;
	MYSQLND *conn;
	MYSQLND_MS_CONN_DATA **conn_data = NULL;
	char *data;
	int data_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zs", &conn_zv, &data, &data_len) == FAILURE) {
		return;
	}

	if (!(conn = zval_to_mysqlnd_inherited(conn_zv TSRMLS_CC))) {
		RETURN_FALSE;
	}

	conn_data = (MYSQLND_MS_CONN_DATA **) _ms_mysqlnd_plugin_get_plugin_connection_data_data(conn->data, mysqlnd_ms_plugin_id);
	if (!conn_data || !(*conn_data)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection");
		RETURN_FALSE;
	}

	if (!(*conn_data)->fabric) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No MySQL Fabric connection");
		RETURN_FALSE;
	}

	void fabric_set_raw_data(mysqlnd_fabric *fabric, char *data, size_t data_len);
	fabric_set_raw_data((*conn_data)->fabric, data, data_len);
}
/* }}} */
#endif

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlnd_ms_xa_begin, 0, 0, 2)
	ZEND_ARG_INFO(0, connection)
    ZEND_ARG_INFO(0, gtrid)
	ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

/* {{{ proto book mysqlnd_ms_xa_begin(mixed connection, int gtrid [, int timeout]) */
static PHP_FUNCTION(mysqlnd_ms_xa_begin)
{
	zval *conn_zv;
	double gtrid;
	/* TODO XA: have the default in the PHP part only? */
	double timeout = 60;
	MYSQLND *conn;
	MYSQLND_MS_CONN_DATA **conn_data = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zd|d", &conn_zv, &gtrid, &timeout) == FAILURE) {
		return;
	}
	if (!(conn = zval_to_mysqlnd_inherited(conn_zv TSRMLS_CC))) {
		RETURN_FALSE;
	}

	MS_LOAD_CONN_DATA(conn_data, conn->data);
	if (!conn_data || !(*conn_data)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection");
		RETURN_FALSE;
	}

	/* TODO XA: Range */
	if (gtrid < 0 || gtrid > 1000) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " gtrid must be in the range of 0 - 1000");
		RETURN_FALSE;
	}
	if (timeout < 0 || timeout > 100) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " timeout must be in the range of 0 - 1000 seconds");
		RETURN_FALSE;
	}

	if (PASS != mysqlnd_ms_xa_monitor_begin(conn->data, *conn_data, (unsigned int)gtrid, (unsigned int)timeout TSRMLS_CC)) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlnd_ms_xa_commit, 0, 0, 2)
	ZEND_ARG_INFO(0, connection)
    ZEND_ARG_INFO(0, gtrid)
ZEND_END_ARG_INFO()

/* {{{ proto book mysqlnd_ms_xa_commit(mixed connection, int gtrid) */
static PHP_FUNCTION(mysqlnd_ms_xa_commit)
{
	zval *conn_zv;
	double gtrid;
	MYSQLND *conn;
	MYSQLND_MS_CONN_DATA **conn_data = NULL;

	/* TODO XA
	 * THe user is not given fine grained control over XA
	 * stages at this point, For now th user will not be able to
	 * go through the individual steps of 2PC at the time of his
	 * liking. This decision is driven a) by simplicity and lazyness and
	 * b) by the belief MS should make using clusters of any kind
	 * simpler. If a user would want to handle 2PC state transitions itself
	 * then he would need to track server changes. This is only of
	 * value if the user aims to send XA END early. This in turn requires
	 * planning of how servers are accessed which implies application
	 * knowledge of the data distribution in the cluster. This is where
	 * an abstraction doing hardly more than wrapping the SQL commands
	 * is useless. To allow power users to do this all we needed to do
	 * is provide a function to track connection switches and to reference
	 * connections opened by the load balancer. Then power users could build
	 * the tiny SQL wrappers on top. Thus, for now, begin -> commit/rollback.
	 * End of the story. Simplistic API comes at the price that we may
	 * keep an XA trx longer in END/PREPARED state than needed.
	 * ... there's also a ton of mysqlnd/mysqlnd_ms refactoring needed. Step by step.
	 */

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zd", &conn_zv, &gtrid) == FAILURE) {
		return;
	}

	if (!(conn = zval_to_mysqlnd_inherited(conn_zv TSRMLS_CC))) {
		RETURN_FALSE;
	}

	MS_LOAD_CONN_DATA(conn_data, conn->data);
	if (!conn_data || !(*conn_data)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection");
		RETURN_FALSE;
	}

	/* TODO XA: Range */
	if (gtrid < 0 || gtrid > 1000) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " gtrid must be in the range of 0 - 1000");
		RETURN_FALSE;
	}

	if (PASS == mysqlnd_ms_xa_monitor_direct_commit(conn->data, *conn_data, (unsigned int)gtrid TSRMLS_CC)) {
		RETVAL_TRUE;
	} else {
		RETVAL_FALSE;
	}
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlnd_ms_xa_rollback, 0, 0, 2)
	ZEND_ARG_INFO(0, connection)
    ZEND_ARG_INFO(0, gtrid)
ZEND_END_ARG_INFO()

/* {{{ proto book mysqlnd_ms_xa_rollback(mixed connection, int gtrid) */
static PHP_FUNCTION(mysqlnd_ms_xa_rollback)
{
	zval *conn_zv;
	double gtrid;
	MYSQLND *conn;
	MYSQLND_MS_CONN_DATA **conn_data = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zd", &conn_zv, &gtrid) == FAILURE) {
		return;
	}

	if (!(conn = zval_to_mysqlnd_inherited(conn_zv TSRMLS_CC))) {
		RETURN_FALSE;
	}

	MS_LOAD_CONN_DATA(conn_data, conn->data);
	if (!conn_data || !(*conn_data)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection");
		RETURN_FALSE;
	}

	/* TODO XA: Range */
	if (gtrid < 0 || gtrid > 1000) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " gtrid must be in the range of 0 - 1000");
		RETURN_FALSE;
	}

	if (PASS == mysqlnd_ms_xa_monitor_direct_rollback(conn->data, *conn_data, (unsigned int)gtrid TSRMLS_CC)) {
		RETVAL_TRUE;
	} else {
		RETVAL_FALSE;
	}
}


ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlnd_ms_xa_gc, 0, 0, 1)
	ZEND_ARG_INFO(0, connection)
    ZEND_ARG_INFO(0, gtrid)
	ZEND_ARG_INFO(0, ignore_max_retries)
ZEND_END_ARG_INFO()

/* {{{ proto book mysqlnd_ms_xa_gc(mixed connection, [int gtrid, bool ignore_max_retries]) */
static PHP_FUNCTION(mysqlnd_ms_xa_gc)
{
	zval *conn_zv;
	double gtrid = 0;
	zend_bool ignore_max_retries = FALSE;
	MYSQLND *conn;
	MYSQLND_MS_CONN_DATA **conn_data = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|db", &conn_zv, &gtrid, &ignore_max_retries) == FAILURE) {
		return;
	}
	if (!(conn = zval_to_mysqlnd_inherited(conn_zv TSRMLS_CC))) {
		RETURN_FALSE;
	}

	MS_LOAD_CONN_DATA(conn_data, conn->data);
	if (!conn_data || !(*conn_data)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection");
		RETURN_FALSE;
	}

	if (1 == ZEND_NUM_ARGS()) {
		/* TODO XA: gc all */
		if (PASS != mysqlnd_ms_xa_gc_all(conn->data, *conn_data, ignore_max_retries TSRMLS_CC)) {
			RETURN_FALSE;
		}
	} else {
		/* TODO XA: Range */
		if (gtrid < 0 || gtrid > 1000) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " gtrid must be in the range of 0 - 1000");
			RETURN_FALSE;
		}

		if (PASS != mysqlnd_ms_xa_gc_one(conn->data, *conn_data, (unsigned int)gtrid, ignore_max_retries TSRMLS_CC)) {
			RETURN_FALSE;
		}
	}

	RETURN_TRUE;
}
/* }}} */

#ifdef ULF_0
/* {{{ proto book mysqlnd_ms_swim() */
static PHP_FUNCTION(mysqlnd_ms_swim)
{
	zval *conn_zv;
	MYSQLND *proxy_conn;
	MYSQLND_MS_CONN_DATA **conn_data = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &conn_zv) == FAILURE) {
		return;
	}
	if (!(proxy_conn = zval_to_mysqlnd_inherited(conn_zv TSRMLS_CC))) {
		RETURN_FALSE;
	}

	MS_LOAD_CONN_DATA(conn_data, proxy_conn->data);
	if (!conn_data || !(*conn_data)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, MYSQLND_MS_ERROR_PREFIX " No mysqlnd_ms connection");
		RETURN_FALSE;
	}


	{
#if PHP_VERSION_ID >= 50600
		MYSQLND *conn = mysqlnd_init(proxy_conn->data->m->get_client_api_capabilities(proxy_conn->data TSRMLS_CC), proxy_conn->data->persistent);
#else
		MYSQLND *conn = mysqlnd_init(proxy_conn->data->persistent);
#endif

		mysqlnd_ms_connect_to_host_aux(proxy_conn->data, conn->data, "slave_1", FALSE,
											"127.0.0.1", 3312,  &(*conn_data)->cred, &(*conn_data)->global_trx,
											TRUE, proxy_conn->data->persistent TSRMLS_CC);

		(*conn_data)->pool->replay_cmds((*conn_data)->pool, proxy_conn->data, NULL TSRMLS_CC);
		(*conn_data)->pool->notify_replace_listener((*conn_data)->pool TSRMLS_CC);

		conn->m->dtor(conn TSRMLS_CC);
	}

	RETURN_TRUE;
}
/* }}} */
#endif

/* {{{ mysqlnd_ms_deps[] */
static const zend_module_dep mysqlnd_ms_deps[] = {
	ZEND_MOD_REQUIRED("mysqlnd")
	ZEND_MOD_REQUIRED("standard")
	ZEND_MOD_REQUIRED("json")
#ifdef MYSQLND_MS_HAVE_MYSQLND_QC
	ZEND_MOD_REQUIRED("mysqlnd_qc")
#else
	/* ensure proper plugin load order for lazy */
	ZEND_MOD_OPTIONAL("mysqlnd_qc")
#endif
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ mysqlnd_ms_functions */
static const zend_function_entry mysqlnd_ms_functions[] = {
	PHP_FE(mysqlnd_ms_match_wild,	arginfo_mysqlnd_ms_match_wild)
	PHP_FE(mysqlnd_ms_query_is_select,	arginfo_mysqlnd_ms_query_is_select)
	PHP_FE(mysqlnd_ms_get_stats,	arginfo_mysqlnd_ms_get_stats)
#if PHP_VERSION_ID > 50399
	PHP_FE(mysqlnd_ms_get_last_used_connection,	arginfo_mysqlnd_ms_get_last_used_connection)
	PHP_FE(mysqlnd_ms_get_last_gtid,	arginfo_mysqlnd_ms_get_last_gtid)
	PHP_FE(mysqlnd_ms_set_qos,	arginfo_mysqlnd_ms_set_qos)
// BEGIN HACK
	PHP_FE(mysqlnd_ms_set_trx,	arginfo_mysqlnd_ms_set_trx)
	PHP_FE(mysqlnd_ms_unset_trx,	arginfo_mysqlnd_ms_unset_trx)
// END HACK
#endif
	PHP_FE(mysqlnd_ms_fabric_select_shard, arginfo_mysqlnd_ms_fabric_select_shard)
	PHP_FE(mysqlnd_ms_fabric_select_global, arginfo_mysqlnd_ms_fabric_select_global)
	PHP_FE(mysqlnd_ms_dump_servers, arginfo_mysqlnd_ms_dump_servers)
	PHP_FE(mysqlnd_ms_dump_fabric_rpc_hosts, arginfo_mysqlnd_ms_dump_servers)
#ifdef PHP_DEBUG
	PHP_FE(mysqlnd_ms_debug_set_fabric_raw_dump_data_xml, NULL)
	PHP_FE(mysqlnd_ms_debug_set_fabric_raw_dump_data_dangerous, NULL)
#endif
	PHP_FE(mysqlnd_ms_xa_begin, arginfo_mysqlnd_ms_xa_begin)
	PHP_FE(mysqlnd_ms_xa_commit, arginfo_mysqlnd_ms_xa_commit)
	PHP_FE(mysqlnd_ms_xa_rollback, arginfo_mysqlnd_ms_xa_rollback)
	PHP_FE(mysqlnd_ms_xa_gc, arginfo_mysqlnd_ms_xa_gc)
#ifdef ULF_0
	PHP_FE(mysqlnd_ms_swim, NULL)
#endif
	{NULL, NULL, NULL}	/* Must be the last line in mysqlnd_ms_functions[] */
};
/* }}} */


/* {{{ mysqlnd_ms_module_entry */
zend_module_entry mysqlnd_ms_module_entry = {
	STANDARD_MODULE_HEADER_EX,
	NULL,
	mysqlnd_ms_deps,
	"mysqlnd_ms",
	mysqlnd_ms_functions,
	PHP_MINIT(mysqlnd_ms),
	PHP_MSHUTDOWN(mysqlnd_ms),
	PHP_RINIT(mysqlnd_ms),
	PHP_RSHUTDOWN(mysqlnd_ms),
	PHP_MINFO(mysqlnd_ms),
	PHP_MYSQLND_MS_VERSION,
	PHP_MODULE_GLOBALS(mysqlnd_ms),
	PHP_GINIT(mysqlnd_ms),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#ifdef COMPILE_DL_MYSQLND_MS
ZEND_GET_MODULE(mysqlnd_ms)
#endif


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
