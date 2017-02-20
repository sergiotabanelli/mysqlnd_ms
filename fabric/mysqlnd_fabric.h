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

#ifndef MYSQLND_FABRIC_H
#define MYSQLND_FABRIC_H

#include "Zend/zend_types.h"

/* Consumers should only use opaque mysqlnd_fabric pointers via accessor functions */
struct struct_mysqlnd_fabric;
typedef struct struct_mysqlnd_fabric mysqlnd_fabric;

enum mysqlnd_fabric_strategy {
	DIRECT,
	DUMP
};

/**
 * Create a new Fabric handle
 * 
 * If DIRECT strategy is used lookups will be mapped directly to Fabric RPC calls
 * (i.e. sharding.lookup_servers). This causes a lot of requests.
 * If DUMP strategy is used an initial dump will be fetched from Fabric. This
 * dump will eventually be cached. All further lookups will use this cache.
 */
mysqlnd_fabric *mysqlnd_fabric_init(enum mysqlnd_fabric_strategy strategy, unsigned int timeout, zend_bool trx_warn_serverlist_changes);
void mysqlnd_fabric_free(mysqlnd_fabric *fabric);
int mysqlnd_fabric_add_rpc_host(mysqlnd_fabric *fabric, char *url);
zend_bool mysqlnd_fabric_get_trx_warn_serverlist_changes(mysqlnd_fabric *fabric);
unsigned int mysqlnd_fabric_get_error_no(mysqlnd_fabric *fabric);
char *mysqlnd_fabric_get_error(mysqlnd_fabric *fabric);

typedef void(*mysqlnd_fabric_apply_func)(const char *url, void *data);

int mysqlnd_fabric_host_list_apply(const mysqlnd_fabric *fabric, mysqlnd_fabric_apply_func cb, void *data);

enum mysqlnd_fabric_server_mode {
	OFFLINE = 0,
	READ_ONLY = 1,
	READ_WRITE = 3
};

enum mysqlnd_fabric_server_role {
	SPARE = 0,
	SCALE = 1,
	SECONDARY = 2,
	PRIMARY = 3
};

typedef struct {
	size_t uuid_len;
	char uuid[41];
	size_t group_len;
	char group[65];
	size_t hostname_len;
	char hostname[65];
	unsigned int port;
	enum mysqlnd_fabric_server_mode mode;
	enum mysqlnd_fabric_server_role role;
	double weight;
} mysqlnd_fabric_server;

enum mysqlnd_fabric_hint {
	LOCAL,
	GLOBAL
};

mysqlnd_fabric_server *mysqlnd_fabric_get_group_servers(mysqlnd_fabric *fabric, const char *group);
mysqlnd_fabric_server *mysqlnd_fabric_get_shard_servers(mysqlnd_fabric *fabric, const char *table, const char *key, enum mysqlnd_fabric_hint hint);
void mysqlnd_fabric_free_server_list(mysqlnd_fabric_server *servers);

#endif	/* MYSQLND_FABRIC_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
