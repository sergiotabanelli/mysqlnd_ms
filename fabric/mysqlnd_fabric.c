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

#include "zend.h"
#include "zend_alloc.h"
#include "main/php.h"
#include "main/spprintf.h"
#include "main/php_streams.h"

#include "ext/standard/php_rand.h"

#include "ext/mysqlnd/mysqlnd.h"
#include "ext/mysqlnd/mysqlnd_priv.h"
#include "ext/mysqlnd/mysqlnd_debug.h"
#include "mysqlnd_ms_enum_n_def.h"
#if PHP_VERSION_ID >= 50400
#include "ext/mysqlnd/mysqlnd_ext_plugin.h"
#endif
#include "mysqlnd_ms.h"

#include "mysqlnd_fabric.h"
#include "mysqlnd_fabric_priv.h"

#include "ext/standard/php_rand.h"

extern const myslqnd_fabric_strategy mysqlnd_fabric_strategy_direct;
extern const myslqnd_fabric_strategy mysqlnd_fabric_strategy_dump;

mysqlnd_fabric *mysqlnd_fabric_init(enum mysqlnd_fabric_strategy strategy, unsigned int timeout, zend_bool trx_warn_serverlist_changes)
{
	mysqlnd_fabric *fabric = ecalloc(1, sizeof(mysqlnd_fabric));

	switch (strategy) {
	case DIRECT:
		fabric->strategy = mysqlnd_fabric_strategy_direct;
		break;
	case DUMP:
		fabric->strategy = mysqlnd_fabric_strategy_dump;
		break;
	}

	fabric->error_no = 0;
	fabric->timeout = timeout;
	fabric->trx_warn_serverlist_changes = trx_warn_serverlist_changes;

	if (fabric->strategy.init) {
		fabric->strategy.init(fabric);
	}

	return fabric;
}

void mysqlnd_fabric_free(mysqlnd_fabric *fabric)
{
	int i;
	if (fabric->strategy.deinit) {
		fabric->strategy.deinit(fabric);
	}
	for (i = 0; i < fabric->host_count ; ++i) {
		efree(fabric->hosts[i].url);
	}
	efree(fabric);
}

zend_bool mysqlnd_fabric_get_trx_warn_serverlist_changes(mysqlnd_fabric *fabric)
{
	return fabric->trx_warn_serverlist_changes;
}

unsigned int mysqlnd_fabric_get_error_no(mysqlnd_fabric *fabric)
{
	return fabric->error_no;
}

char *mysqlnd_fabric_get_error(mysqlnd_fabric *fabric)
{
	return fabric->error;
}

int mysqlnd_fabric_add_rpc_host(mysqlnd_fabric *fabric, char *url)
{
	if (fabric->host_count >= 10) {
		TSRMLS_FETCH();
		php_error_docref(NULL TSRMLS_CC, E_ERROR, MYSQLND_MS_ERROR_PREFIX " Please report a bug: no more than 10 Fabric hosts allowed");
		return 1;
	}

	fabric->hosts[fabric->host_count].url = estrdup(url);
	fabric->host_count++;

	return 0;
}

int mysqlnd_fabric_host_list_apply(const mysqlnd_fabric *fabric, mysqlnd_fabric_apply_func cb, void *data)
{
	int i;
	for (i = 0; i < fabric->host_count; ++i) {
		cb(fabric->hosts[i].url, data);
	}
	return i;
}

mysqlnd_fabric_server *mysqlnd_fabric_get_group_servers(mysqlnd_fabric *fabric, const char *group)
{
	if (!fabric->host_count) {
		return NULL;
	}
	return fabric->strategy.get_group_servers(fabric, group);
}

mysqlnd_fabric_server *mysqlnd_fabric_get_shard_servers(mysqlnd_fabric *fabric, const char *table, const char *key, enum mysqlnd_fabric_hint hint)
{
	if (!fabric->host_count) {
		return NULL;
	}
	return fabric->strategy.get_shard_servers(fabric, table, key, hint);
}

void mysqlnd_fabric_free_server_list(mysqlnd_fabric_server *servers)
{
	efree(servers);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
