# Local transactions
The plugin is not transaction safe by default, because it is not aware of running transactions in all cases. SQL transactions are units of work to be run on a single server. The plugin does not always know when the unit of work starts and when it ends, therefore in these cases, the plugin may decide to switch connections in the middle of a transaction.

No kind of MySQL load balancer can detect transaction boundaries without any kind of hint from the application.

You can use SQL hints to work around this limitation or use the `mymysqlnd_ms` [mysqlnd_ms_set_trx](REF:../MYSQLND_MS-FUNCTIONS) and [mysqlnd_ms_unset_trx](REF:../MYSQLND_MS-FUNCTIONS) functions. Alternatively, you can activate transaction API call monitoring. In the latter case you must use API calls only to control transactions, see below.

###### Configuration 1
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
                "host": "192.168.2.27",
                "port": "3306"
            }
        }
    }
}
```
In the following example the `MYSQLND_MS_LAST_USED_SWITCH` SQL hint are used for transactions
###### Example 1
```
<?php
$mysqli = new mysqli("myapp", "username", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

/* Not a SELECT, will use master */
if (!$mysqli->query("START TRANSACTION")) {
    /* Please use better error handling in your code */
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* Prevent connection switch! */
if (!$mysqli->query(sprintf("/*%s*/INSERT INTO test(id) VALUES (1)", MYSQLND_MS_LAST_USED_SWITCH))) {
    /* Please do proper ROLLBACK in your code, don't just die */
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
if ($res = $mysqli->query(sprintf("/*%s*/SELECT COUNT(*) AS _num FROM test", MYSQLND_MS_LAST_USED_SWITCH))) {
    $row = $res->fetch_assoc();
    $res->close();
    if ($row['_num'] > 1000) {
        if (!$mysqli->query(sprintf("/*%s*/INSERT INTO events(task) VALUES ('cleanup')", MYSQLND_MS_LAST_USED_SWITCH))) {
            die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
        }
    }
} else {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
if (!$mysqli->query(sprintf("/*%s*/UPDATE log SET last_update = NOW()", MYSQLND_MS_LAST_USED_SWITCH))) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
if (!$mysqli->query(sprintf("/*%s*/COMMIT", MYSQLND_MS_LAST_USED_SWITCH))) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

$mysqli->close();
?>
```

The mysqlnd library allows the plugin to monitor the status of the autocommit mode and transaction boundary, if the mode and transaction is set by API calls instead of using SQL statements. This makes it possible for the plugin to become transaction aware. In this case, you do not need to use SQL hints. setting the plugin configuration option [trx_stickiness](REFA:../PLUGIN-CONFIGURATION-FILE.md)=master, the plugin can automatically disable load balancing and connection switches for SQL transactions. In this configuration, the plugin stops load balancing if autocommit as been disabled or a transaction started through API like [mysqli_autocommit](http://php.net/manual/en/mysqli.autocommit.php) or [mysqli_begin_transaction](http://php.net/manual/en/mysqli.begin-transaction.php) and directs all statements to the master. This prevents connection switches in the middle of a transaction. Once autocommit is re-enabled or transaction ends, the plugin starts to load balance statements again. Following is a configuration example for transaction aware load balancing and trx_stickiness setting
###### Configuration 2
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
        "trx_stickiness": "master"
    }
}
```
###### Example 2
```
<?php
$mysqli = new mysqli("myapp", "username", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

/* Disable autocommit, plugin will run all statements on the master */
$mysqli->autocommit(false);

if (!$mysqli->query("INSERT INTO test(id) VALUES (1)")) {
    /* Please do proper ROLLBACK in your code, don't just die */
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
if ($res = $mysqli->query("SELECT COUNT(*) AS _num FROM test")) {
    $row = $res->fetch_assoc();
    $res->close();
    if ($row['_num'] > 1000) {
        if (!$mysqli->query("INSERT INTO events(task) VALUES ('cleanup')")) {
            die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
        }
    }
} else {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
if (!$mysqli->query("UPDATE log SET last_update = NOW()")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
if (!$mysqli->commit()) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* Plugin assumes that the transaction has ended and starts load balancing again */
$mysqli->autocommit(true);
$mysqli->close();
?>
```
To explicitly mark transaction boundaries the `mymysqlnd_ms` [mysqlnd_ms_set_trx](REF:../MYSQLND_MS-FUNCTIONS) and [mysqlnd_ms_unset_trx](REF:../MYSQLND_MS-FUNCTIONS) functions can also be used ([trx_stickiness](REFA:../PLUGIN-CONFIGURATION-FILE.md)=master must also be set).
###### Example 3
```
<?php
$mysqli = new mysqli("myapp", "username", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

/* Stop connection switching */
mysqlnd_ms_set_trx($mysqli);

/* will use master */
if (!$mysqli->query("START TRANSACTION")) {
    /* Please use better error handling in your code */
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

if (!$mysqli->query("INSERT INTO test(id) VALUES (1)")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
/* will stay on master */
if ($res = $mysqli->query("SELECT COUNT(*) AS _num FROM test")) {
    $row = $res->fetch_assoc();
    $res->close();
    if ($row['_num'] > 1000) {
        if (!$mysqli->query("INSERT INTO events(task) VALUES ('cleanup')")) {
            die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
        }
    }
} else {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
if (!$mysqli->query("UPDATE log SET last_update = NOW()")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
if (!$mysqli->query("COMMIT")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
/* restart connection switching from now on*/
mysqlnd_ms_unset_trx($mysqli);


```
>
**NOTE: The [PDO::beginTransaction](http://php.net/manual/en/pdo.begintransaction.php) method does not use [mysqlnd_tx_begin](http://php.net/manual/en/mysqlnd.plugin.api.php) mysqlnd API function, so the** `mymysqlnd_ms` **plugin can not monitor the transaction start. To make it transaction start aware you shuold extend the PDO class and use the [mysqlnd_ms_set_trx](REF:../MYSQLND_MS-FUNCTIONS) function.**
>
###### Example 4
```
class MyPDO extends PDO
{
    public function beginTransaction()
    {
        mysqlnd_ms_set_trx($this);
        return parent::beginTransaction();
    }
}
```

  


