# XA Distributed Transactions
>
NOTE: As [stated](http://php.net/manual/en/mysqlnd-ms.quickstart.xa_transactions.php) by original Authors:
This feature  is still in a alpha development state. There may be issues and/or feature limitations. Do not use in production environments, although early lab tests indicate reasonable quality.
Please, contact the development team if you are interested in this feature.

XA transactions are a standardized method for executing transactions across multiple resources. Those resources can be databases or other transactional systems. The MySQL server supports XA SQL statements which allows users to carry out a distributed SQL transaction that spawns multiple database servers or any kind as long as they support the SQL statements too. In such a scenario it is in the responsibility of the user to coordinate the participating servers.

`mymysqlnd_ms` can act as a transaction coordinator for a global (distributed, XA) transaction carried out on MySQL servers only. As a transaction coordinator, the plugin tracks all servers involved in a global transaction and transparently issues appropriate SQL statements on the participants. The global transactions are controlled with [mysqlnd_ms_xa_begin](REF:../MYSQLND_MS-FUNCTIONS/), [mysqlnd_ms_xa_commit](REF:../MYSQLND_MS-FUNCTIONS/) and [mysqlnd_ms_xa_rollback](REF:../MYSQLND_MS-FUNCTIONS/). SQL details are mostly hidden from the application as is the need to track and coordinate participants. The following eample is a general pattern for XA transactions
###### Example 1
```
<?php
$mysqli = new mysqli("myapp", "username", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

/* start a global transaction */
$gtrid_id = "12345";
if (!mysqlnd_ms_xa_begin($mysqli, $gtrid_id)) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* run queries as usual: XA BEGIN will be injected upon running a query */
if (!$mysqli->query("INSERT INTO orders(order_id, item) VALUES (1, 'christmas tree, 1.8m')")) {
    /* Either INSERT failed or the injected XA BEGIN failed */
    if ('XA' == substr($mysqli->sqlstate, 0, 2)) {
        printf("Global transaction/XA related failure, [%d] %s\n", $mysqli->errno, $mysqli->error);
    } else {
        printf("INSERT failed, [%d] %s\n", $mysqli->errno, $mysqli->error);
    }
    /* rollback global transaction */
    mysqlnd_ms_xa_rollback($mysqli, $xid);
    die("Stopping.");
}

/* continue carrying out queries on other servers, e.g. other shards */

/* commit the global transaction */
if (!mysqlnd_ms_xa_commit($mysqli, $xa_id)) {
    printf("[%d] %s\n", $mysqli->errno, $mysqli->error);
}
?>
```

Unlike with local transactions, which are carried out on a single server, XA transactions have an identifier (xid) associated with them. The XA transaction identifier is composed of a global transaction identifier (gtrid), a branch qualifier (bqual) a format identifier (formatID). Only the global transaction identifier can and must be given when calling any of the plugins XA functions.

Once a global transaction has been started, the plugin begins tracking servers until the global transaction ends. When a server is picked for query execution, the plugin injects the SQL statement `XA BEGIN` prior to executing the actual SQL statement on the server. `XA BEGIN` makes the server participate in the global transaction. If the injected SQL statement fails, the plugin will report the issue in reply to the query execution function that was used. In the above example, `$mysqli->query("INSERT INTO orders(order_id, item) VALUES (1, 'christmas tree, 1.8m')")` would indicate such an error. You may want to check the errors SQL state code to determine whether the actual query (here: `INSERT`) has failed or the error is related to the global transaction. It is up to you to ignore the failure to start the global transaction on a server and continue execution without having the server participate in the global transaction. Local and global transactions are mutually exclusive
###### Example 2 
```
<?php
$mysqli = new mysqli("myapp", "username", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

/* start a local transaction */
if (!$mysqli->begin_transaction()) {
    die(sprintf("[%d/%s] %s\n", $mysqli->errno, $mysqli->sqlstate, $mysqli->error));
}

/* cannot start global transaction now - must end local transaction first */
$gtrid_id = "12345";
if (!mysqlnd_ms_xa_begin($mysqli, $gtrid_id)) {
    die(sprintf("[%d/%s] %s\n", $mysqli->errno, $mysqli->sqlstate, $mysqli->error));
}
?>
```
The above example will output:

```
Warning: mysqlnd_ms_xa_begin(): (mysqlnd_ms) Some work is done outside global transaction. You must end the active local transaction first in ... on line ...
[1400/XAE09] (mysqlnd_ms) Some work is done outside global transaction. You must end the active local transaction first
```
A global transaction cannot be started when a local transaction is active. The plugin tries to detect this situation as early as possible, that is when [mysqlnd_ms_xa_begin](REF:../MYSQLND_MS-FUNCTIONS/) is called. If using API calls only to control transactions, the plugin will know that a local transaction is open and return an error for [mysqlnd_ms_xa_begin](REF:../MYSQLND_MS-FUNCTIONS/). However, note the plugins limitations on detecting transaction boundaries for [local transactions](REF:../CONCEPTS/). In the worst case, if using direct SQL for local transactions (`BEGIN`, `COMMIT`, ...), it may happen that an error is delayed until some SQL is executed on a server.

To end a global transaction invoke [mysqlnd_ms_xa_commit](REF:../MYSQLND_MS-FUNCTIONS/) or [mysqlnd_ms_xa_rollback](REF:../MYSQLND_MS-FUNCTIONS/). When a global transaction is ended all participants must be informed of the end. Therefore, `mymysqlnd_ms` transparently issues appropriate XA related SQL statements on some or all of them. Any failure during this phase will cause an implicit rollback. The XA related API is intentionally kept simple here. A more complex API that gave more control would bare few, if any, advantages over a user implementation that issues all lower level XA SQL statements itself.

XA transactions use the two-phase commit protocol. The two-phase commit protocol is a blocking protocol. There are cases when no progress can be made, not even when using timeouts. Transaction coordinators should survive their own failure, be able to detect blockades and break ties. `mymysqlnd_ms` takes the role of a transaction coordinator and can be configured to survive its own crash to avoid issues with blocked MySQL servers. Therefore, the plugin can and should be configured to use a persistent and crash-safe state to allow garbage collection of unfinished, aborted global transactions. A global transaction can be aborted in an open state if either the plugin fails (crashes) or a connection from the plugin to a global transaction participant fails. The following is an example of configuration of the persistent crash-safe state store.
###### Configuration 1 
```
{
    "myapp": {
        "xa": {
            "state_store": {
                "participant_localhost_ip": "192.168.2.12",
                "mysql": {
                    "host": "192.168.2.13",
                    "user": "root",
                    "password": "",
                    "db": "test",
                    "port": "3312",
                    "socket": null
                }
            }
        },
        "master": {
            "master_0": {
                "host": "localhost",
                "socket": "\/tmp\/mysql.sock"
            }
        },
        "slave": {
            "slave_0": {
                "host": "192.168.2.14",
                "port": "3306"
            }
        }
    }
}
```
Currently, `mymysqlnd_ms` supports only using MySQL database tables as a state store. The SQL definitions of the tables are given in the [xa](REFA:../PLUGIN-CONFIGURATION-FILE.md) plugin configuration section. Please, make sure to use a transactional and crash-safe storage engine for the tables, such as InnoDB. InnoDB is the default table engine in recent versions of the MySQL server. Make also sure the database server itself is highly available.

