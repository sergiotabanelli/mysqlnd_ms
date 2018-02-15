# Global transaction IDs

>NOTE: Together with strictly related [service level and consistency](REF:../CONCEPTS) the [global transaction IDs](REF:../CONCEPTS) feature is one of the most changed areas of the  `mymysqlnd_ms` fork. Functionlities like [server side read consistency](REFA:GLOBAL-TRANSACTION-IDS.md) and [server side write consistency](REFA:GLOBAL-TRANSACTION-IDS.md) allow transparent migration to MySQL clusters in almost all use cases with no or at most extremely small effort and application changes.

>The code should be considered of beta quality. We use it in our restricted intranet production enviroment, but we are the developers so, if we find bugs, we can patch our code as soon as possible. This feature is not required for synchronous clusters, such as MySQL Cluster.

>BEWARE: Global transaction ID features works only together with the [quality_of_service](REFA:../PLUGIN-CONFIGURATION-FILE.md) filter with [session_consistency](REFA:../PLUGIN-CONFIGURATION-FILE.md) service level.

As of MySQL 5.6.5 the MySQL server features built-in global transaction identifiers (GTID). However the feature set found in MySQL < 5.7.6 was not enough to support an effective server side consistency enforcing.

Starting from MySQL 5.7.6 the MySQL server features the [session-track-gtids](https://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html#sysvar_session_track_gtids) system variable, which, if set, will allow a client to be aware of GTID assigned by MySQL to an executed transaction. This will allow the plugin to support effective server side GTIDs consistency scenarios without the need of client side GTID emulation. This is a big advantage in terms of safeness and  write loads. Indeed the **client side emulation** add an SQL write for every explicit transaction and, in autocommit mode, for every query not evaluated as read-only, that is, in default configuration, on every non `SELECT` query. Potentialy there are some tricks to reduce writes due to GTID client side emulation, but, IMHO, use of server side GTID is far more better.    

The `mymysqlnd_ms` plugin can either use the global transaction ID feature built-in to MySQL >= 5.7.6 or its own global transaction ID emulation. It use GTIDs to enforce three types of session consistency (see also [service level and consistency](REF:../CONCEPTS)).

### Session consistency configuration directives
The [global_transaction_id_injection](REFA:../PLUGIN-CONFIGURATION-FILE.md) section must include all configurations data needed by the QOS filter to enforce session consitency. 

[Global_transaction_id_injection](REFA:../PLUGIN-CONFIGURATION-FILE.md) directives used for session consistency enforcing are better described in [service level and consistency](REF:).

* [type](REFA:../PLUGIN-CONFIGURATION-FILE.md)=1 for [client side read consistency](REFA:SERVICE-LEVEL-AND-CONSISTENCY.md)
* [type](REFA:../PLUGIN-CONFIGURATION-FILE.md)=2 for [server side read consistency](REFA:SERVICE-LEVEL-AND-CONSISTENCY.md)
* [type](REFA:../PLUGIN-CONFIGURATION-FILE.md)=3 for [server side write consistency](REFA:SERVICE-LEVEL-AND-CONSISTENCY.md)
* [wait_for_gtid_timeout](REFA:../PLUGIN-CONFIGURATION-FILE.md) used for read consistency, if no slave is found consistent, specify max seconds to wait for at least one slave to become consistent.
* [wait_for_wgtid_timeout](REFA:../PLUGIN-CONFIGURATION-FILE.md) used in server side write consistency, specify max seconds a write operation can spend choosing a consistent master.
* [running_ttl](REFA:../PLUGIN-CONFIGURATION-FILE.md) used in server side write consistency, specify max running seconds for a write operation.
* [race_avoid](REFA:../PLUGIN-CONFIGURATION-FILE.md) strategy used if session consitency does not find any valid node.
* [on_connect](REFA:../PLUGIN-CONFIGURATION-FILE.md) used for read consistency, specify if the GTID state should be retrived to initialize session consistency on connection init (read consitency escapes connection boundaries).

In consistency enforcing `mymysqlnd_ms` takes the role of context coordinator. Therefore the plugin can and should be configured to use a persistent shared state store. Currently, `mymysqlnd_ms` supports only compatible memcached protocol state store. The [memcached_host](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive specify the backend shared state store hostname. You can use any memcached protocol capable server, e.g. [memcached](https://memcached.org/), [couchbase](https://www.couchbase.com/) or also [mysql with the memcached plugin](https://dev.mysql.com/doc/refman/5.6/en/innodb-memcached-setup.html), if not specified a [memcached_port](REFA:../PLUGIN-CONFIGURATION-FILE.md) the memcached default will be used.

### Placeholders
Placeholders are reserved tokens used in configuration values of the [memcached_key](REFA:../PLUGIN-CONFIGURATION-FILE.md), [memcached_wkey](REFA:../PLUGIN-CONFIGURATION-FILE.md), [fetch_last_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) and [on_commit](REFA:../PLUGIN-CONFIGURATION-FILE.md) directives. The placeholder token will be expanded to the corresponding value at connection init, allowing consistency context establishment on a connection attribute basis. Indeed, placeholders, can be used to establish consistency context partitions. Context partitioning means that clients can share with each others the same configured isolated concistency context. Following the list of currently available placeholders: 

* `#SID` value of the current php [session_id](http://php.net/manual/en/function.session-id.php), value is expanded at connection init
* `#DB` value of the database schema specified on connection init
* `#USER` value of the MySQL user specified on connection init
* `#SKEY` value of the php session veriable `mysqlnd_ms_gtid_skey` (`$_SESSION['mysqlnd_ms_gtid_skey']`), value is expanded at connection init

### Server side GTIDs
**[Requires MySQL >= 5.7.6 with --session-track-gtids=OWN_GTID](https://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html#sysvar_session_track_gtids)**.

To use server side gtids you must set the [type](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive to value 2 or 3 depending on you cluster type and desired [service level and consistency](REF:../CONCEPTS). 

Configuration #1 Server side read consistency

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
            "on_connect": 1,
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

Configuration #2 Server side write consistency

```
{
    "myapp": {
        "master": {
            "master_0": {
                "host": "mymaster0",
             }
            "master_0": {
                "host": "mymaster1",
            },
            "master_1": {
                "host": "mymaster2",
            }
        },
        "slave": [],
        "trx_stickiness": "master",
        "global_transaction_id_injection": {
            "type": 3,
            "fetch_last_gtid": "SELECT @@GLOBAL.GTID_EXECUTED AS trx_id FROM DUAL",
            "memcached_host": "mymemcached",
            "memcached_key": "mymy#SID",
            "on_connect": 1,
            "memcached_wkey": "mymy#USER"
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
The [memcached_key](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive is used in read consistency enforcing, it specify the backend shared state store key where the last write GTID will be stored. The last write stored GTID is relative to the configured read context partition:

```
   ...
	"global_transaction_id_injection": {
	    ...
	    "memcached_key": "mymy#SID",
	    ...
	}
	...
```
In previous example the last write stored GTID is relative to the user php `session_id`.

The [memcached_wkey](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive is used in write consistency enforcing, it specify the id used to group keys that store corresponding write context informations.  

```
   ...
	"global_transaction_id_injection": {
	    ...
	    "memcached_wkey": "mymy#USER",
	    ...
	}
	...
```
In previous example the MySQL connection user is the id of the group of keys that will store write context informations relative to all MySQL same user connections.

The [fetch_last_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) is used for read and write consistency and specify the query used to retrive current GTID executed set from all cluster nodes. Query result will be used to check replication status against the GTID that mark current consistency state.  

```
   ...
	"global_transaction_id_injection": {
	    ...
       "fetch_last_gtid": "SELECT @@GLOBAL.GTID_EXECUTED AS trx_id FROM DUAL",
	    ...
	}
	...
```
In previous example, current replicated GTIDs is retrived through a query that returns the [gtid_executed](https://dev.mysql.com/doc/refman/5.7/en/replication-options-gtids.html#sysvar_gtid_executed) MySQL system varible.

In [MySQL Group replication](https://dev.mysql.com/doc/refman/5.7/en/group-replication.html) each group member use blocks of consecutive GTIDS. For group replication clusters the plugin must be aware of the block size used by MySQL. Use the [gtid_block_size](REFA:../PLUGIN-CONFIGURATION-FILE.md) configuration directive to set the value of the [group_replication_gtid_assignment_block_size](https://dev.mysql.com/doc/refman/5.7/en/group-replication-options.html#sysvar_group_replication_gtid_assignment_block_size) MySQL system variable.

```
   ...
	"global_transaction_id_injection": {
	    ...
       "gtid_block_size": 1000000,
	    ...
	}
	...        
```
In previous example the [gtid_block_size](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive is set to 1000000 which is the the default value used by MySQL for [group_replication_gtid_assignment_block_size](https://dev.mysql.com/doc/refman/5.7/en/group-replication-options.html#sysvar_group_replication_gtid_assignment_block_size) system variable.

>BEWARE: Set `gtid_block_size` only for group replication clusters. If you set it for NON group replication clusters or if you don't set it for group replication clusters, session consitency will not work. 

#### Client side GTIDs
In its most basic form a global transaction ID (GTID) is a counter in a table on the master. The counter is incremented whenever a transaction is committed on the master. Slaves replicate the table. The counter serves two purposes. In case of a master failure, it helps the database administrator to identify the most recent slave for promoting it to the new master. The most recent slave is the one with the highest counter value. Applications can use the global transaction ID to search for slaves which have replicated a certain write (identified by a global transaction ID) already.

`mymysqlnd_ms` can inject SQL for every committed transaction to increment a GTID counter. The so created GTID is accessible by the application to identify an applications write operation. This enables the plugin to deliver session consistency (read your writes) service level by not only querying masters but also slaves which have replicated the change already. Read load is taken away from the master.

Client-side global transaction ID emulation has some limitations. Please, read the concepts section carefully to fully understand the principles and ideas behind it, before using in production environments.

To increment the GTID counter table the plugin can use standard MySQL query specified in the [on_commit](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive or memcached protocol if nodes have the [MySQL memcached plugin](https://dev.mysql.com/doc/refman/5.6/en/innodb-memcached-setup.html) enabled.

>NOTE: In client side emulation GTID table counter is incremented only when the [quality_of_service](REFA:../PLUGIN-CONFIGURATION-FILE.md) filter with [session_consistency](REFA:../PLUGIN-CONFIGURATION-FILE.md) service level is enabled. If session consistency service level is not active the injection will not be done.

###### Client side GTIDs with MySQL memcached plugin
To use this feature all MySQL cluster nodes must be memcached protocol enabled through the MySQL memcached plugin.

First login to the master, [install the MySQL memcached plugin](https://dev.mysql.com/doc/refman/5.6/en/innodb-memcached-setup.html) and set the [innodb_api_enable_binlog](https://dev.mysql.com/doc/refman/5.6/en/innodb-parameters.html#sysvar_innodb_api_enable_binlog) option. Create the transaction id counter table, e.g.:

```
CREATE DATABASE mygtid;
USE mygtid;
CREATE TABLE `memcached` (
  `id` char(32) NOT NULL DEFAULT '',
  `trx_id` varchar(30) NOT NULL,
  `flags` int(11) NOT NULL,
  `cas` bigint(20) NOT NULL,
  `expiry` int(11) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
```
Add the new transaction id counter table to innodb_memcache.containers: 

```
INSERT INTO `innodb_memcache`.`containers` (`name`, `db_schema`, `db_table`, `key_columns`, `value_columns`, `flags`, `cas_column`,
`expire_time_column`,`unique_idx_name_on_key`) VALUES ('default', 'mygtid', 'memcached', 'id', 'trx_id', 'flags','cas','expiry','PRIMARY');
```
Reload the memcached mysql plugin daemon:

```
UNINSTALL PLUGIN daemon_memcached;
INSTALL PLUGIN daemon_memcached soname "libmemcached.so";
```
On all slaves install the MySQL memcached plugin, if slaves can be promoted to master and have binary log enabled set also the [innodb_api_enable_binlog](https://dev.mysql.com/doc/refman/5.6/en/innodb-parameters.html#sysvar_innodb_api_enable_binlog) option. 

As for server side read consistency, read context can be partitioned using [placeholders](REFA:GLOBAL-TRANSACTION-IDS.md) in [memcached_key](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive. 

> BEWARE: For client side read consistency with MySQL memcached plugin the [memcached_host](REFA:../PLUGIN-CONFIGURATION-FILE.md), [on_commit](REFA:../PLUGIN-CONFIGURATION-FILE.md), [fetch_last_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) directives must NOT be specified, the [memcached_key](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive must be specified

Configuration #3 Client side read consistency with MySQL memcached plugin

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
            "type": 1,
            "memcached_key": "mymy#SID",
            "on_connect": 1,
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

###### Client side GTIDs through MySQL queries
First, create athe counter table on your master server, e.g.:

```
CREATE DATABASE mygtid;
USE mygtid;
CREATE TABLE `trx` (
  `id` char(32) NOT NULL DEFAULT '',
  `trx_id` int(11) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
```
In the plugins configuration file set the SQL to update the GTID table using [on_commit](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive and the SQL to fetch the last GTID using [fetch_last_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md). Make sure the table name used for the SQL statements is fully qualified. In the example, `mygtid.trx` is used to refer to table `trx` in the schema (database) `mygtid`. Make sure the user that will open the connection is allowed to execute the UPDATE.

Configuration #4: Client side read consistency with MySQL queries

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
            "type": 1,
            "on_commit": "INSERT INTO mygtid.trx (id, trx_id) VALUES ('#SID', 1) ON DUPLICATE KEY UPDATE trx_id=trx_id+1",
            "fetch_last_gtid" : "SELECT trx_id FROM mygtid.trx WHERE id = '#SID'",
            "on_connect": 1,
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
The [on_commit](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive is used on any detected write query to increment the GTID table counter on master.  

```
   ...
	"global_transaction_id_injection": {
	    ...
       "on_commit": "INSERT INTO mygtid.trx (id, trx_id) VALUES ('#SID', 1) ON DUPLICATE KEY UPDATE trx_id=trx_id+1",
	    ...
	}
	...
```
In previous example, GTID table counter is relative to the user php `session_id`,  `trx_id` counter is inserted with initial value 1 if not present otherwise incremented by 1.

The [fetch_last_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) is used to retrive last replicated GTID from slaves and last executed GTID from master. Query result will be used to check replication status against the GTID that mark current consistency state.  

```
   ...
	"global_transaction_id_injection": {
	    ...
       "fetch_last_gtid": "SELECT trx_id FROM mygtid.trx WHERE id = '#SID'",
	    ...
	}
	...
```
In previous example, GTID table counter is relative to the user php `session_id` and `trx_id` counter is retrieved using a `WHERE` clause.

###### Obtaining GTID after injection
>NOTE: In `mymysqlnd_ms` fork the [mysqlnd_ms_get_last_gtid](REF:../MYSQLND_MS-FUNCTIONS) is of little or no use, the plugin transparently enforce the configured [service level and consistency](REF:) and does not need that applications set the GTID returned by [mysqlnd_ms_get_last_gtid](REF:../MYSQLND_MS-FUNCTIONS) as an option for the session consistency service level.

With the [mysqlnd_ms_get_last_gtid](REF:../MYSQLND_MS-FUNCTIONS) applications can ask mymysqlnd_ms for the GTID which belongs to the last write operation performed by the application

```
<?php
$mysqli = new mysqli("myapp", "username", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

/* auto commit mode, transaction on master, GTID must be incremented */
if (!$mysqli->query("DROP TABLE IF EXISTS test")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

printf("GTID after transaction %s\n", mysqlnd_ms_get_last_gtid($mysqli));

/* auto commit mode, transaction on master, GTID must be incremented */
if (!$mysqli->query("CREATE TABLE test(id INT)")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

printf("GTID after transaction %s\n", mysqlnd_ms_get_last_gtid($mysqli));
?>
```
Previous example prints GTIDs belonging to the `DROP TABLE IF EXISTS test` and `CREATE TABLE test(id INT)` write queries.

###### Error reporting

Enable reporting of errors that may occur when mysqlnd_ms does global transaction ID injection.

