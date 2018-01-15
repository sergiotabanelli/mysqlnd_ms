# Global transaction IDs
>
NOTE: Together with strictly related [service level and consistency](REF:../CONCEPTS) the [global transaction IDs](REF:../CONCEPTS) feature is one of the most changed areas of the  `mymysqlnd_ms` fork. Functionlities like [server side read consistency](REFA:GLOBAL-TRANSACTION-IDS.md) and [server side write consistency](REFA:GLOBAL-TRANSACTION-IDS.md) allow transparent migration to MySQL clusters in almost all use cases with no or at most extremely small effort and application changes.

>The code should be considered of beta quality. We use it in our restricted intranet production enviroment, but we are the developers so, if we find bugs, we can patch our code almost immediatly. The feature is not required for synchronous clusters, such as MySQL Cluster.

>BEWARE: Global transaction ID features works only together with the [quality_of_service](REFA:../PLUGIN-CONFIGURATION-FILE.md) filter with [session_consistency](REFA:../PLUGIN-CONFIGURATION-FILE.md) service level.

As of MySQL 5.6.5 the MySQL server features built-in global transaction identifiers (GTID). However the feature set found in MySQL < 5.7.6 was not sufficient to support an effective server side consistency enforcing.

Starting from MySQL 5.7.6 the MySQL server features the [session-track-gtids](https://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html#sysvar_session_track_gtids) system variable, which, if set, will allow a client to be aware of GTID assigned by MySQL to an executed transaction. This will allow the plugin to support effective server side GTIDs consistency scenarios without the need of client side GTID emulation. This is a big advantage in terms of safeness and  write loads. Indeed the **client side emulation** add an SQL write for every explicit transaction and, in autocommit mode, for every query not evalueted as read-only, that is, in default configuration, on every non `SELECT` query. Potentialy there are some tricks to reduce writes due to GTID client side emulation, but, IMHO, use of server side GTID is far more better.    

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

In consistency enforcing `mymysqlnd_ms` takes the role of context coordinator. Therefore the plugin can and should be configured to use a persistent shared state store. Currently, `mymysqlnd_ms` supports only compatible memcached protocol state store. The [memcached_host](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive specify the backend shared state store hostname, you can use any memcached protocol capable server, e.g. [memcached](https://memcached.org/), [couchbase](https://www.couchbase.com/) or also [mysql with the memcached plugin](https://dev.mysql.com/doc/refman/5.6/en/innodb-memcached-setup.html), if not specified a [memcached_port](REFA:../PLUGIN-CONFIGURATION-FILE.md) the memcached default will be used.







* [memcached_host](REFA:../PLUGIN-CONFIGURATION-FILE.md) used for server side consistency, specify the backend shared state store hostname, you can use any memcached protocol capable server, e.g. [memcached](https://memcached.org/), [couchbase](https://www.couchbase.com/) or also [mysql with the memcached plugin](https://dev.mysql.com/doc/refman/5.6/en/innodb-memcached-setup.html).
* [memcached_key](REFA:../PLUGIN-CONFIGURATION-FILE.md) used for server side read consistency and for client side consistency with [MySQL memcached plugin](https://dev.mysql.com/doc/refman/5.6/en/innodb-memcached-setup.html), in server side read consistency specify the backend shared state store key where the last write GTID will be stored. In client side emulation is the key identifying the GTID counter it replace the [on_commit](REFA:../PLUGIN-CONFIGURATION-FILE.md) and [fetch_last_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) directives used if the [MySQL memcached plugin](https://dev.mysql.com/doc/refman/5.6/en/innodb-memcached-setup.html) is not installed on cluster nodes.
* [memcached_wkey](REFA:../PLUGIN-CONFIGURATION-FILE.md) used for server side write consistency, in server side write consistency specify the backend shared state store id where the write context partition state will be stored.
* [fetch_last_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) used for server side read and write consistency and for client side consistency without MySQL memcached plugin, in server side consistency specify the query used to retrive GTID executed set from cluster nodes. In client side emulation is the query used to retrive the last replicated GTID from the table used for GTID counter emulation on cluster nodes.
* [on_commit](REFA:../PLUGIN-CONFIGURATION-FILE.md) used for client side consistency without MySQL memcached plugin, in client side emulation is the query used to increment GTID counter on master.

### Placeholders

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
In previous example the last write stored GTID is relative to the used php session_id.

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

Configuration #1 `gtid_block_size`

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
>
BEWARE: Set `gtid_block_size` only for group replication clusters. If you set it for NON group replication clusters or if you don't set it for group replication clusters, session consitency will not work. 

#### Client side GTIDs


###### MySQL memcached plugin






The quickstart first demonstrates the use of the client-side global transaction ID emulation built-in to PECL/mysqlnd_ms before its show how to use the server-side counterpart. The order ensures that the underlying idea is discussed first.

Idea and client-side emulation

In its most basic form a global transaction ID (GTID) is a counter in a table on the master. The counter is incremented whenever a transaction is committed on the master. Slaves replicate the table. The counter serves two purposes. In case of a master failure, it helps the database administrator to identify the most recent slave for promoting it to the new master. The most recent slave is the one with the highest counter value. Applications can use the global transaction ID to search for slaves which have replicated a certain write (identified by a global transaction ID) already.

PECL/mysqlnd_ms can inject SQL for every committed transaction to increment a GTID counter. The so created GTID is accessible by the application to identify an applications write operation. This enables the plugin to deliver session consistency (read your writes) service level by not only querying masters but also slaves which have replicated the change already. Read load is taken away from the master.

Client-side global transaction ID emulation has some limitations. Please, read the concepts section carefully to fully understand the principles and ideas behind it, before using in production environments. The background knowledge is not required to continue with the quickstart.

First, create a counter table on your master server and insert a record into it. The plugin does not assist creating the table. Database administrators must make sure it exists. Depending on the error reporting mode, the plugin will silently ignore the lack of the table or bail out.

Example #1 Create counter table on master

CREATE TABLE `trx` (
  `trx_id` int(11) DEFAULT NULL,
  `last_update` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=latin1
INSERT INTO `trx`(`trx_id`) VALUES (1);
In the plugins configuration file set the SQL to update the global transaction ID table using on_commit from the global_transaction_id_injection section. Make sure the table name used for the UPDATE statement is fully qualified. In the example, test.trx is used to refer to table trx in the schema (database) test. Use the table that was created in the previous step. It is important to set the fully qualified table name because the connection on which the injection is done may use a different default database. Make sure the user that opens the connection is allowed to execute the UPDATE.

Enable reporting of errors that may occur when mysqlnd_ms does global transaction ID injection.

Example #2 Plugin config: SQL for client-side GTID injection

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
        "global_transaction_id_injection":{
            "on_commit":"UPDATE test.trx SET trx_id = trx_id + 1",
            "report_error":true
        }
    }
}
Example #3 Transparent global transaction ID injection

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

/* auto commit mode, transaction on master, GTID must be incremented */
if (!$mysqli->query("CREATE TABLE test(id INT)")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* auto commit mode, transaction on master, GTID must be incremented */
if (!$mysqli->query("INSERT INTO test(id) VALUES (1)")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* auto commit mode, read on slave, no increment */
if (!($res = $mysqli->query("SELECT id FROM test"))) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

var_dump($res->fetch_assoc());
?>
The above example will output:

array(1) {
  ["id"]=>
  string(1) "1"
}
The example runs three statements in auto commit mode on the master, causing three transactions on the master. For every such statement, the plugin will inject the configured UPDATE transparently before executing the users SQL statement. When the script ends the global transaction ID counter on the master has been incremented by three.

The fourth SQL statement executed in the example, a SELECT, does not trigger an increment. Only transactions (writes) executed on a master shall increment the GTID counter.

Note: SQL for global transaction ID: efficient solution wanted!
The SQL used for the client-side global transaction ID emulation is inefficient. It is optimized for clearity not for performance. Do not use it for production environments. Please, help finding an efficient solution for inclusion in the manual. We appreciate your input.

Example #4 Plugin config: SQL for fetching GTID

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
        "global_transaction_id_injection":{
            "on_commit":"UPDATE test.trx SET trx_id = trx_id + 1",
            "fetch_last_gtid" : "SELECT MAX(trx_id) FROM test.trx",
            "report_error":true
        }
    }
}
Example #5 Obtaining GTID after injection

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
The above example will output:

GTID after transaction 7
GTID after transaction 8
Applications can ask PECL mysqlnd_ms for a global transaction ID which belongs to the last write operation performed by the application. The function mysqlnd_ms_get_last_gtid() returns the GTID obtained when executing the SQL statement from the fetch_last_gtid entry of the global_transaction_id_injection section from the plugins configuration file. The function may be called after the GTID has been incremented.

Applications are adviced not to run the SQL statement themselves as this bares the risk of accidently causing an implicit GTID increment. Also, if the function is used, it is easy to migrate an application from one SQL statement for fetching a transaction ID to another, for example, if any MySQL server ever features built-in global transaction ID support.

The quickstart shows a SQL statement which will return a GTID equal or greater to that created for the previous statement. It is exactly the GTID created for the previous statement if no other clients have incremented the GTID in the time span between the statement execution and the SELECT to fetch the GTID. Otherwise, it is greater.

Example #6 Plugin config: Checking for a certain GTID

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
        "global_transaction_id_injection":{
            "on_commit":"UPDATE test.trx SET trx_id = trx_id + 1",
            "fetch_last_gtid" : "SELECT MAX(trx_id) FROM test.trx",
            "check_for_gtid" : "SELECT trx_id FROM test.trx WHERE trx_id >= #GTID",
            "report_error":true
        }
    }
}
Example #7 Session consistency service level and GTID combined

<?php
$mysqli = new mysqli("myapp", "username", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

/* auto commit mode, transaction on master, GTID must be incremented */
if (   !$mysqli->query("DROP TABLE IF EXISTS test")
    || !$mysqli->query("CREATE TABLE test(id INT)")
    || !$mysqli->query("INSERT INTO test(id) VALUES (1)")
) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* GTID as an identifier for the last write */
$gtid = mysqlnd_ms_get_last_gtid($mysqli);

/* Session consistency (read your writes): try to read from slaves not only master */
if (false == mysqlnd_ms_set_qos($mysqli, MYSQLND_MS_QOS_CONSISTENCY_SESSION, MYSQLND_MS_QOS_OPTION_GTID, $gtid)) {
    die(sprintf("[006] [%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* Either run on master or a slave which has replicated the INSERT */
if (!($res = $mysqli->query("SELECT id FROM test"))) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

var_dump($res->fetch_assoc());
?>
A GTID returned from mysqlnd_ms_get_last_gtid() can be used as an option for the session consistency service level. Session consistency delivers read your writes. Session consistency can be requested by calling mysqlnd_ms_set_qos(). In the example, the plugin will execute the SELECT statement either on the master or on a slave which has replicated the previous INSERT already.

PECL mysqlnd_ms will transparently check every configured slave if it has replicated the INSERT by checking the slaves GTID table. The check is done running the SQL set with the check_for_gtid option from the global_transaction_id_injection section of the plugins configuration file. Please note, that this is a slow and expensive procedure. Applications should try to use it sparsely and only if read load on the master becomes to high otherwise.

Use of the server-side global transaction ID feature

Note: Insufficient server support in MySQL 5.6
The plugin has been developed against a pre-production version of MySQL 5.6. It turns out that all released production versions of MySQL 5.6 do not provide clients with enough information to enforce session consistency based on GTIDs. Please, read the concepts section for details.

Starting with MySQL 5.6.5-m8 the MySQL Replication system features server-side global transaction IDs. Transaction identifiers are automatically generated and maintained by the server. Users do not need to take care of maintaining them. There is no need to setup any tables in advance, or for setting on_commit. A client-side emulation is no longer needed.

Clients can continue to use global transaction identifier to achieve session consistency when reading from MySQL Replication slaves in some cases but not all! The algorithm works as described above. Different SQL statements must be configured for fetch_last_gtid and check_for_gtid. The statements are given below. Please note, MySQL 5.6.5-m8 is a development version. Details of the server implementation may change in the future and require adoption of the SQL statements shown.

Using the following configuration any of the above described functionality can be used together with the server-side global transaction ID feature. mysqlnd_ms_get_last_gtid() and mysqlnd_ms_set_qos() continue to work as described above. The only difference is that the server does not use a simple sequence number but a string containing of a server identifier and a sequence number. Thus, users cannot easily derive an order from GTIDs returned by mysqlnd_ms_get_last_gtid().

Example #8 Plugin config: using MySQL 5.6.5-m8 built-in GTID feature

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
        "global_transaction_id_injection":{
            "fetch_last_gtid" : "SELECT @@GLOBAL.GTID_DONE AS trx_id FROM DUAL",
            "check_for_gtid" : "SELECT GTID_SUBSET('#GTID', @@GLOBAL.GTID_DONE) AS trx_id FROM DUAL",
            "report_error":true
        }
    }
}