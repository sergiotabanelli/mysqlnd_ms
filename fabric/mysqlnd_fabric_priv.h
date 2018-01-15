/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2013 The PHP Group                                |
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

#ifndef MYSQLND_FABRIC_PRIV_H
#define MYSQLND_FABRIC_PRIV_H
#include "mysqlnd_ms.h"

/* Staying close to mysqlnd here for now, may change later */
#define SET_EMPTY_FABRIC_ERROR(fabric) \
{ \
	(fabric).error_no = 0; \
	(fabric).error[0] = '\0'; \
}

#define SET_FABRIC_ERROR(fabric, a_error_no, b_sqlstate, c_error) \
{\
	if (0 == (a_error_no)) { \
		SET_EMPTY_FABRIC_ERROR(fabric); \
	} else { \
		(fabric).error_no = a_error_no; \
		strlcpy((fabric).sqlstate, b_sqlstate, sizeof((fabric).sqlstate)); \
		strlcpy((fabric).error, c_error, sizeof((fabric).error)); \
	} \
}

enum mysqlnd_fabric_state {
	DISABLED,
	ENABLED
};

/*static const char *mysqlnd_fabric_state_values[] = {
	"DISABLED",
	"ENABLED"
};*/

enum mysqlnd_fabric_map_type_name {
	RANGE,
	HASH
};

/*static const char *mysqlnd_fabric_map_type_name_values[] = {
	"RANGE",
	"HASH"
};*/

typedef struct {
	int shard_mapping_id;
	char schema_name[65];
	char table_name[65];
	char column_name[65];
} mysqlnd_fabric_shard_table;

typedef struct {
	int shard_mapping_id;
	enum mysqlnd_fabric_map_type_name type_name;
	char global_group[65];
}   mysqlnd_fabric_shard_mapping;

typedef struct {
	int shard_mapping_id;
	int lower_bound; /* FIXME - RANGE sharding only */
	int shard_id;
	char group[65];
} mysqlnd_fabric_shard_index;


typedef struct {
	void (*init)(mysqlnd_fabric *fabric);
	void (*deinit)(mysqlnd_fabric *fabric);
	mysqlnd_fabric_server *(*get_group_servers)(mysqlnd_fabric *fabric, const char *group);
	mysqlnd_fabric_server *(*get_shard_servers)(mysqlnd_fabric *fabric, const char *table, const char *key, enum mysqlnd_fabric_hint hint);
} myslqnd_fabric_strategy;

typedef struct {
	char *url;
} mysqlnd_fabric_rpc_host;

#define MYSQLND_MS_ERRMSG_SIZE 1024
#define MYSQLND_MS_SQLSTATE_LENGTH 5
struct struct_mysqlnd_fabric {
	int host_count;
	mysqlnd_fabric_rpc_host hosts[10];
	myslqnd_fabric_strategy strategy;
	void *strategy_data;

	/* timeout connect + read, see PHP stream wrapper */
	unsigned int timeout;

	/* warn about switching to other servers in the middle of a transaction */
	zend_bool trx_warn_serverlist_changes;

	/* error information to be bubbled up to the SQL level - use MYSQLND_ERROR_INFO? */
	char error[MYSQLND_MS_ERRMSG_SIZE+1];
	char sqlstate[MYSQLND_MS_SQLSTATE_LENGTH + 1];
	unsigned int error_no;
};


zend_string *mysqlnd_fabric_http(mysqlnd_fabric *fabric, char *url, char *request_body, size_t request_body_len);
mysqlnd_fabric_server *mysqlnd_fabric_parse_xml(mysqlnd_fabric *fabric, const char *xmlstr, int xmlstr_len);

#endif	/* MYSQLND_FABRIC_PRIV_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
