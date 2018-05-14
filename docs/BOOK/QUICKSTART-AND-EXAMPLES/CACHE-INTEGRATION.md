# Cache integration
>Note: Please, find more about version requirements, extension load order dependencies and the current status in the concepts section!

>Note: `mymysqlnd` cache integration has been extended to [server side read consistency](SERVICE-LEVEL-AND-CONSISTENCY.md#server-side-read-consistency) service level. Original implementation has cache integration only for eventual consistency. This documentation is about new cache integration with read consistency service level. For eventual consistency follow the [original documentation instructions](http://php.net/manual/en/mysqlnd-ms.quickstart.cache.php) 

>Note: Requires MySQL >= 5.7.6 with [--session-track-gtids=OWN_GTID](https://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html#sysvar_session_track_gtids)

Databases clusters can deliver different levels of consistency. It is possible to advice the plugin to consider only cluster nodes that can deliver the consistency level requested. For example, if using asynchronous MySQL Replication with its cluster-wide eventual consistency, it is possible to configure or request read session consistency (read your writes) at any time using [mysqlnd_ms_set_qos](REF:../MYSQLND_MS-FUNCTIONS/) (see [service level and consistency](SERVICE-LEVEL-AND-CONSISTENCY.md)).

Replacing a database node read access with a local cache access can improve overall performance and lower the database load. If the cache entry is every reused by other clients than the one creating the cache entry, a database access is saved and thus database load is lowered. Furthermore, system performance can become better if computation and delivery of a database query is slower than a local cache access.

Assuming `mymysqlnd` has been configured with [server side read consistency](SERVICE-LEVEL-AND-CONSISTENCY.md#server-side-read-consistency) it is possible to replace a database node read access with a client-side cache using both time-to-live (TTL) and read context writes as its invalidation strategy. Both the database node and the cache will serve current consistent data for the configured read context. In other words a cached query will continue to be served by the client-side cache until one of these events occurrs:

* Cache TTL expires
* A read context partecipants (see [service level and consistency](SERVICE-LEVEL-AND-CONSISTENCY.md)) writes something new to the cluster.

Configuration #1: Server side read consistency no special entries for caching

```
{
    "myapp": {
        "master": {
            "master_0": {
                "host": "mymaster",
             }
        },
        "slave": {
            "slave_0": {
                "host": "myslave0",
            },
            "slave_1": {
                "host": "myslave1",
            }
        },
        "trx_stickiness": "master",
        "global_transaction_id_injection": {
            "type": 2,
            "fetch_last_gtid": "SELECT @@GLOBAL.GTID_EXECUTED AS trx_id FROM DUAL",
            "memcached_host": "mymemcached",
            "memcached_key": "mymy#SID"
        },
        "filters": {
            "quality_of_service": {
                "session_consistency": 1
            },
            "random": []
        }
    }
}
```

Example #1 Caching a read request

```
<?php
$mysqli = new mysqli("myapp", "username", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

if (   !$mysqli->query("DROP TABLE IF EXISTS test")
    || !$mysqli->query("CREATE TABLE test(id INT)")
    || !$mysqli->query("INSERT INTO test(id) VALUES (1)")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* 
 * No matter if slaves have not yet replicated previuous queries, 
 * server side read consistency enforce a consistent node 
 */
$res = $mysqli->query("/*" . MYSQLND_QC_ENABLE_SWITCH . "*/" . "SELECT id FROM test");
if (!res) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
var_dump($res->fetch_assoc());

/* 
* Result is in the cache, to confirm we check it using mysqlnd_qc plugin function 'mysqlnd_qc_get_core_stats' 
* (requires 'mysqlnd_qc.collect_statistics=1' php ini directive)
*/
$stats = mysqlnd_qc_get_core_stats();
/* cache_put will be 1 */
printf("cache_put %d\n", $stats['cache_put']);
/* cache_hit will be 0 */
printf("cache_hit %d\n", $stats['cache_hit']);

$res = $mysqli->query("/*" . MYSQLND_QC_ENABLE_SWITCH . "*/" . "SELECT id FROM test");
if (!res) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
var_dump($res->fetch_assoc());

/* 
* Query was served from cache, to confirm we check it using mysqlnd_qc plugin function 'mysqlnd_qc_get_core_stats' 
* (requires 'mysqlnd_qc.collect_statistics=1' php ini directive)
*/
$stats = mysqlnd_qc_get_core_stats();
/* cache_put will be 1 */
printf("cache_put %d\n", $stats['cache_put']);
/* cache_hit will be 1 */
printf("cache_hit %d\n", $stats['cache_hit']);


if (!$mysqli->query("INSERT INTO test(id) VALUES (2)")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
$res = $mysqli->query("/*" . MYSQLND_QC_ENABLE_SWITCH . "*/" . "SELECT id FROM test");
if (!res) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
/* 
* Cache was invalidated by the previous insert
*/
var_dump($res->fetch_assoc());

?>
```
The above example use [mysqlnd_qc_get_core_stats()](http://php.net/manual/en/function.mysqlnd-qc-get-core-stats.php) (requires [mysqlnd_qc.collect_statistics=1](http://php.net/manual/en/mysqlnd-qc.configuration.php) php ini directive) only for checking effective cache invalidation strategy, but there is no need to use it in real use cases! The example shows that after a successful write the cache has been validated and query has been run on a consistent cluster node. Note that the used json configuration has no special directives for caching, which queries to cache and with not is left to application developers using [mysqlnd_qc](http://php.net/manual/en/book.mysqlnd-qc.php) available functionalities and SQL HINTS. The same code works also for configuration of multi master [server side write consistency](SERVICE-LEVEL-AND-CONSISTENCY.md#server-side-write-consistency)! 

Furthermore, using the [qc_ttl](REFA:../PLUGIN-CONFIGURATION-FILE.md) configuration directive, the plugin can transparently cache and apply its invalidation strategy on all read queries belonging  to the configured read context. This means that cache integration can be made completely transparent to applications!    

Configuration #1: Server side read consistency with transparent cache integration

```
{
    "myapp": {
        "master": {
            "master_0": {
                "host": "mymaster",
             }
        },
        "slave": {
            "slave_0": {
                "host": "myslave0",
            },
            "slave_1": {
                "host": "myslave1",
            }
        },
        "trx_stickiness": "master",
        "global_transaction_id_injection": {
            "type": 2,
            "fetch_last_gtid": "SELECT @@GLOBAL.GTID_EXECUTED AS trx_id FROM DUAL",
            "memcached_host": "mymemcached",
            "memcached_key": "mymy#SID",
            "qc_ttl": 60
        },
        "filters": {
            "quality_of_service": {
                "session_consistency": 1
            },
            "random": []
        }
    }
}
```
With previous configuration all read queries will transparently be cached through the mysqlnd_qc plugin. 

Example #1 Caching a slave request requires `mysqlnd_qc.collect_statistics=1` php ini directive

```
<?php
$mysqli = new mysqli("myapp", "username", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

if (   !$mysqli->query("DROP TABLE IF EXISTS test")
    || !$mysqli->query("CREATE TABLE test(id INT)")
    || !$mysqli->query("INSERT INTO test(id) VALUES (1)")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* 
 * No matter if slaves have not yet replicated previuous queries, 
 * server side read consistency enforce a consistent node 
 */
$res = $mysqli->query("SELECT id FROM test");
if (!res) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
var_dump($res->fetch_assoc());

/* 
* Result is in the cache, to confirm we check it using mysqlnd_qc plugin function 'mysqlnd_qc_get_core_stats' 
* (requires 'mysqlnd_qc.collect_statistics=1' php ini directive)
*/
$stats = mysqlnd_qc_get_core_stats();
/* cache_put will be 1 */
printf("cache_put %d\n", $stats['cache_put']);
/* cache_hit will be 0 */
printf("cache_hit %d\n", $stats['cache_hit']);

$res = $mysqli->query("SELECT id FROM test");
if (!res) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
var_dump($res->fetch_assoc());

/* 
* Query was served from cache, to confirm we check it using mysqlnd_qc plugin function 'mysqlnd_qc_get_core_stats' 
* (requires 'mysqlnd_qc.collect_statistics=1' php ini directive)
*/
$stats = mysqlnd_qc_get_core_stats();
/* cache_put will be 1 */
printf("cache_put %d\n", $stats['cache_put']);
/* cache_hit will be 1 */
printf("cache_hit %d\n", $stats['cache_hit']);


if (!$mysqli->query("INSERT INTO test(id) VALUES (2)")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
$res = $mysqli->query("SELECT id FROM test");
if (!res) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
/* 
* Cache was invalidated by the previous insert
*/
var_dump($res->fetch_assoc());

?>
```
The above example use [mysqlnd_qc_get_core_stats()](http://php.net/manual/en/function.mysqlnd-qc-get-core-stats.php) (requires [mysqlnd_qc.collect_statistics=1](http://php.net/manual/en/mysqlnd-qc.configuration.php) php ini directive) only for checking effective cache invalidation strategy, but there is no need to use it in real use cases! The eaxample shows that queries are cached without any devoleper specific code and that after a successful write the cache has been validated and query has been run on a consistent cluster node. The same code works also for configuration of multi master [server side write consistency](SERVICE-LEVEL-AND-CONSISTENCY.md#server-side-write-consistency)!

