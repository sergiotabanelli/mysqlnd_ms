# Service level and consistency

>
NOTE: Together with strictly related [global transaction IDs](REF:../CONCEPTS), the [service level and consistency](REF:../CONCEPTS) feature is one of the most changed areas of the `mymysqlnd_ms` fork. Functionlities like [server side read consistency](REFA:) and [server side write consistency](REFA:) allow transparent migration to MySQL clusters in almost all use cases with no or at most extremely small effort and application changes.

>The code should be considered of beta quality. We use it in our restricted intranet production enviroment, but we are the developers so, if we find bugs, we can patch our code almost immediatly. The feature is not required for synchronous clusters, such as MySQL Cluster.

Different types of MySQL cluster solutions offer different service and data consistency levels to their users. Any asynchronous MySQL replication cluster offers eventual consistency by default. A read executed on an asynchronous slave may return current, stale or no data at all, depending on whether the slave has replayed all changesets from master or not.

Applications using a MySQL replication cluster need to be designed to work correctly with eventual consistent data. In most cases, however, stale data is not acceptable. In those cases only certain slaves or even only master accesses are allowed to achieve the required quality of service from the cluster.

New MySQL functionalities available in more recent versions, like [multi source replication](https://dev.mysql.com/doc/refman/5.7/en/replication-multi-source.html) or [group replication](https://dev.mysql.com/doc/refman/5.7/en/group-replication.html), allow multi-master clusters and need application strategies to avoid write conflicts and enforce write consistency for distinct write context partitions.

The plugin is capable of selecting MySQL replication nodes automatically that deliver read and write consistency. Context partitioning means that  clients can share with each others the same configured isolated concistency context. With read consistency, a consistency context partecipant will always read writes by other partecipants, itself included. With write consistency in multi-master clusters, writes from all consistency context partecipants will always do not conflicts each others.  

In read and write consistency `mymysqlnd_ms` takes the role of context coordinator. Therefore the plugin can and should be configured to use a persistent shared state store. Currently, `mymysqlnd_ms` supports only compatible memcached protocol state store. Read and write session consistency implementation is stricly related to [global transaction IDs](REF:)(from now on GTIDs) that there must always be configured together with qos filter with session consistency. 

#### Server side read consistency
**[Requires MySQL >= 5.7.6 with --session-track-gtids=OWN_GTID](https://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html#sysvar_session_track_gtids)**.

A read consistency context is a set of application reads made by a context partecipant that must always at least run against previous writes made by all other context partecipants. The `mymysqlnd_ms` plugin can transparently enforce this type of read consistency: 
* Reads belonging to a context partition can safely run only on cluster nodes that has already replicated all previous same context partition writes. 
* Reads belonging to a context partition can safely run on cluster nodes that still has not replicated writes from all other contexts.

If you simply need to read your writes inside a single MySQL session connection boundaries, you don't need a memcached state store back-end.

Configuration #1: minimal configuration for simple read your writes session read consinstency

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
In previous configuration the `myapp` cluster has 1 master and 2 slaves, always set [trx_stickiness](REFA:../PLUGIN-CONFIGURATION-FILE.md) for read or write consistency. The [global_transaction_id_injection](REFA:../PLUGIN-CONFIGURATION-FILE.md) configure a global transaction id [type](REFA:../PLUGIN-CONFIGURATION-FILE.md)=2, value `2` means [server side read consistency](REFA:). The [fetch_last_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) configured query will be used by the plugin to check cluster hosts for already replicated GTIDs. The [quality_of_service](REFA:../PLUGIN-CONFIGURATION-FILE.md) filter must always be configured with [session_consistency](REFA:../PLUGIN-CONFIGURATION-FILE.md) service level.

##### Connection boundaries escape
In most common use cases, a web application requires more complex read consistency scenario. It needs not only that a client read its writes on a MySQL connection basis, but a much more extended read consistency context, a context that cross MySQL connection boundaries as well same web client requests boundaries, possibly distributed on different php application server front-ends.
   
Configuration #2: minimal configuration for most common server side read consistency use case

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
Last configuration differs from previous one for the use of a memcached state store back-end. This allow the plugin to share read consistency information state across all web application instances. The [memcached_host](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive specify the backend shared state store hostname, you can use any memcached protocol capable server, e.g. [memcached](https://memcached.org/), [couchbase](https://www.couchbase.com/) or also [mysql with the memcached plugin](https://dev.mysql.com/doc/refman/5.6/en/innodb-memcached-setup.html), if not specified a [memcached_port](REFA:../PLUGIN-CONFIGURATION-FILE.md) the memcached default will be used. The [memcached_key](REFA:../PLUGIN-CONFIGURATION-FILE.md) configuration directive use the `#SID` placeholder (one of the available [placeholders](REFA:GLOBAL-TRANSACTION-IDS.md) for consistency context key configuration), it will be replaced by the php [session_id](http://php.net/manual/en/function.session-id.php), the obtained string will be the key that identify the read consistency context partition in the configured memcached state store. The [on_connect](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive instructs the plugin to check the read consistency context partition state at connection start, without this directive the read consistency contexts will not persists across MySQL connection boundaries and more over distinct web requests. With this configuration a user of your web application will always read his writes, also on distinct web requests and also if the web requests will be load balanced on differents php web application front-ends.

### Server side write consistency
**[Requires MySQL >= 5.7.6 with --session-track-gtids=OWN_GTID](https://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html#sysvar_session_track_gtids)**.

New MySQL functionalities available in more recent versions, like [multi source replication](https://dev.mysql.com/doc/refman/5.7/en/replication-multi-source.html) or [group replication](https://dev.mysql.com/doc/refman/5.7/en/group-replication.html) allow multi-master clusters and need application strategies to avoid write conflicts and enforce write consistency for distinct write context partitions. A write context partition is a set of application writes that, if run on distinct masters, can potentialy conflict each others but that does not conflicts with write sets from all other defined partitions. The `mymysqlnd_ms` plugin can transparently enforce this type of write consistency: 
* Writes belonging to distinct context partitions can safely run concurrently on distinct MySQL masters without any data conflicts and replication issues.
* Writes belonging to the same context partition can safely run concurrently only on the same master. 
* Writes belonging to the same context partition can safely run **NON** concurrently, there are no still pending same context writes, on any masters that has already replicated all previous same context writes.

>
BEWARE: distinct write set partitions must not intersect each others. e.g. if a write set include all writes to table A, no other write set partition should include writes to table A.

>
NOTE: server side write consistency always include server side read consistency 

Through the use of [placeholders](REFA:GLOBAL-TRANSACTION-IDS.md) cluster's write context can also be partitioned based on MySQL connection user, schema name, web client php [session_id](http://php.net/manual/en/function.session-id.php) or specific php session variable. 
Write context partitioning can be usefull only for multi-master clusters, so you need to enable the [mysqlnd_ms.multi_master](REFA:../INSTALLING-CONFIGURING/RUNTIME-CONFIGURATION.md) php.ini directive

```  
extension=mysqlnd_ms.so
mysqlnd_ms.enable=1
mysqlnd_ms.multi_master=1
```
A typical use case is write context partitioning on a MySQL user base, e.g a multi-muster cluster has a schema with a table named `mytableA`, 2 MySQL users, `myuserA` with read privilege on all tables schema and write privilege only for  `mytableA` and `myuserB` with read privilege on all the tables schema and write privilege on all tables schema except `mytableA`. 

Configuration #3: Configuration for write context partition on MySQL user basis

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
            "memcached_wkey": "mymy#USER",
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
The `myapp` cluster has now 3 masters and no slaves. The remaining configuration differs from previous one for the [type](REFA:../PLUGIN-CONFIGURATION-FILE.md)=3 configuration directive, value `3` means [server side write consistency](REFA:) it always include also [server side read consistency](REFA:). The [memcached_wkey](REFA:../PLUGIN-CONFIGURATION-FILE.md) configuration directive use the `#USER` placeholder (one of the available [placeholders](REFA:GLOBAL-TRANSACTION-IDS.md) for consistency context key configuration), it will be replaced by the MySQL user used for the connection, the obtained string will be the key that identify the write consistency context partition in the configured memcached state store. For the supposed cluster example, the previous configuration, will establish one context partition for each MySQL user, writes made with a MySQL connection established with the `myuserA` user, will run concurrently only the same master, but will also possibly run concurrently on distinct master then that used for writes made with a MySQL connection established with the `myuserB` user. 

### Client side read consistency
**[Use only with MySQL < 5.7.6]**.

In its most basic form a global transaction ID (GTID) is a counter in a table on the master. The counter is incremented whenever a transaction is committed on the master. Applications can use the a GTID to search for slaves which have already replicated identified writes. 

For MySQL version < 5.7.6 `mymysqlnd_ms` can inject SQL for every committed transaction to increment a GTID counter. The so created GTID is accessible by the application to identify an application write operation. This enables the plugin to deliver session read consistency service level by querying slaves which have already replicated the change.

[Client side read consistency](REFA:) basically is the [old global transacction id injection feature](http://php.net/manual/en/mysqlnd-ms.quickstart.gtid.php) with some optimization and support for the [MySQL memcached plugin](https://dev.mysql.com/doc/refman/5.6/en/innodb-memcached-setup.html). This is different from memcached state store used for server side read consistency. In the former case all MySQL clusters nodes are also memcached capable through the MySQL memcached plugin, the [memcached_host](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive must not be specified. Memcached protocol will be used instead of SQL query to increment and retrive the GTID counter, in which case the [memcached_key](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive must be used instead of [fetch_last_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) and [on_commit](REFA:../PLUGIN-CONFIGURATION-FILE.md) directives. For non memcached configuration follow the [original documentation instructions](http://php.net/manual/en/mysqlnd-ms.quickstart.gtid.php) except for the [check_for_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) that is no more used by the plugin.

You can now use a MySQL memcached plugin managed db table as GTID counter table. The new [memcached_key](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive can be used as a replacement of [on_commit](REFA:../PLUGIN-CONFIGURATION-FILE.md) [fetch_last_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) and [check_for_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) SQL query directives. 

To use this feature login to the master, [install the MySQL memcached plugin](https://dev.mysql.com/doc/refman/5.6/en/innodb-memcached-setup.html) and set the [innodb_api_enable_binlog](https://dev.mysql.com/doc/refman/5.6/en/innodb-parameters.html#sysvar_innodb_api_enable_binlog) option. Create the transaction id table, e.g.:

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
Add the new transaction id table to innodb_memcache.containers: 

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

As for server side read consistency, read context can be partitioned using [placeholders](REFA:GLOBAL-TRANSACTION-IDS.md) in [memcached_key](REFA:../PLUGIN-CONFIGURATION-FILE.md), [on_commit](REFA:../PLUGIN-CONFIGURATION-FILE.md), [fetch_last_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) directives. 

Configuration #4: minimal configuration for most common client side read consistency use case

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
Last configuration differs from previosu configuration #2 used in server side read consistency for the [type](REFA:../PLUGIN-CONFIGURATION-FILE.md)=1 configuration directive which now has value `1` that means [client side read consistency](REFA:). The [memcached_host](REFA:../PLUGIN-CONFIGURATION-FILE.md) and [fetch_last_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) must not be specified because all MySQL nodes will act as GTID state store.

### Session consistency failures and timeouts
Session consistency enforcing force use of nodes consistent with current required consistency level. If no node satisfy required conditions, plugin can wait a limited amount of time that at least one node become consistent. For read consistency you can use [wait_for_gtid_timeout](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive.
 
### Requesting configured session consistency
Service levels can be set in the plugins configuration file and at runtime using [mysqlnd_ms_set_qos](REF:../MYSQLND_MS-FUNCTIONS). If you do not need session consistency for all your application but only for limited code sections, you should not configure the [quality_of_service](REFA:../PLUGIN-CONFIGURATION-FILE.md) filter with enabled [session_consistency](REFA:../PLUGIN-CONFIGURATION-FILE.md), but set it at run time only when needed. 

Example #1 Requesting configured session consistency

```
<?php
$mysqli = new mysqli("myapp", "username", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}
/* Enable the configured consistency type */
if (!mysqlnd_ms_set_qos($mysqli, MYSQLND_MS_QOS_CONSISTENCY_SESSION)) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* Use the configured consistency type */
if (!$mysqli->query("INSERT INTO orders(order_id, item) VALUES (1, 'christmas tree, 1.8m')")) {
    /* Please use better error handling in your code */
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* Use the configured consistency type */
if (!$res = $mysqli->query("SELECT item FROM orders WHERE order_id = 1")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

var_dump($res->fetch_assoc());

/* Back to eventual consistency: stale data allowed */
if (!mysqlnd_ms_set_qos($mysqli, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL)) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* Plugin picks any slave, stale data is allowed */
if (!$res = $mysqli->query("SELECT item, price FROM specials")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
?>
```
In the example the [mysqlnd_ms_set_qos](REF:../MYSQLND_MS-FUNCTIONS) function is used to enforce session consistency for all future statements until further notice, the enforced consistency type depends on the configured [type](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive. The `INSERT` and `SELECT` statement on the orders table run on nodes which ensure the write can be seen by the client. Read-write splitting logic has been adapted to fulfill the service level and configured consistency.

After the application has read its changes from the orders table it returns to the default service level, which is eventual consistency. Eventual consistency puts no restrictions on choosing a node for statement execution. Thus, the SELECT statement on the specials table is executed on any node.

### Eventual consitency
Configuration #5 Maximum age/slave lag

```
{
    "myapp": {
        "master": {
            "master_0": {
                "host": "localhost",
                "socket": "\/tmp\/mysql.sock"
            }
        },
        "slave": {
            "slave_0": {
                "host": "127.0.0.1",
                "port": "3306"
            }
        },
        "failover" : "master"
    }
}
```

Example #2 Limiting slave lag

```
<?php
$mysqli = new mysqli("myapp", "username", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

/* Read from slaves lagging no more than four seconds */
$ret = mysqlnd_ms_set_qos(
    $mysqli,
    MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL,
    MYSQLND_MS_QOS_OPTION_AGE,
    4
);

if (!$ret) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* Plugin picks any slave, which may or may not have the changes */
if (!$res = $mysqli->query("SELECT item, price FROM daytrade")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* Back to default: use of all slaves and masters permitted */
if (!mysqlnd_ms_set_qos($mysqli, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL)) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
?>
```
The eventual consistency service level can be used with an optional parameter to set a maximum slave lag for choosing slaves. If set, the plugin checks `SHOW SLAVE STATUS` for all configured slaves. In case of the example, only slaves for which `Slave_IO_Running=Yes`, `Slave_SQL_Running=Yes` and `Seconds_Behind_Master <= 4` is true are considered for executing the statement `SELECT item, price FROM daytrade`.

Checking `SHOW SLAVE STATUS` is done transparently from an applications perspective. Errors, if any, are reported as warnings. No error will be set on the connection handle. Even if all `SHOW SLAVE STATUS` SQL statements executed by the plugin fail, the execution of the users statement is not stopped, given that master fail over is enabled. Thus, no application changes are required.

Please, note the limitations and properties of `SHOW SLAVE STATUS` as explained in the MySQL reference manual.

To prevent `mymysqlnd_ms` from emitting a warning if no slaves can be found that lag no more than the defined number of seconds behind the master, it is necessary to enable master fail over in the plugins configuration file. If no slaves can be found and fail over is turned on, the plugin picks a master for executing the statement.

If no slave can be found and fail over is turned off, the plugin emits a warning, it does not execute the statement and it sets an error on the connection.

Configuration #5 Fail over not set

```
{
    "myapp": {
        "master": {
            "master_0": {
                "host": "localhost",
                "socket": "\/tmp\/mysql.sock"
            }
        },
        "slave": {
            "slave_0": {
                "host": "127.0.0.1",
                "port": "3306"
            }
        }
    }
}
```
Example #3 No slave within time limit

```
<?php
$mysqli = new mysqli("myapp", "username", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

/* Read from slaves lagging no more than four seconds */
$ret = mysqlnd_ms_set_qos(
    $mysqli,
    MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL,
    MYSQLND_MS_QOS_OPTION_AGE,
    4
);

if (!$ret) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* Plugin picks any slave, which may or may not have the changes */
if (!$res = $mysqli->query("SELECT item, price FROM daytrade")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}


/* Back to default: use of all slaves and masters permitted */
if (!mysqlnd_ms_set_qos($mysqli, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL)) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
?>
```
The above example will output:

```
PHP Warning:  mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate slave connection. 0 slaves to choose from. Something is wrong in %s on line %d
PHP Warning:  mysqli::query(): (mysqlnd_ms) No connection selected by the last filter in %s on line %d
[2000] (mysqlnd_ms) No connection selected by the last filter
```
