# Read-write splitting
If no [SQL hints](SQL-HINTS.md), filters or requested [Service level and consistency](SERVICE-LEVEL-AND-CONSISTENCY.md) force a different behavior, the plugin executes read-only statements on the configured MySQL slaves, and all other queries on the MySQL master/s. If the query statement does not contain any `mymysqlnd_ms` [SQL hints](SQL-HINTS.md) it is considered read-only if it either start with SELECT or if it does not starts with one of the configured [mysqlnd_ms.master_on](../INSTALLING-CONFIGURING/RUNTIME-CONFIGURATION.md#mysqlnd_ms.master_on) tokens.

Application can ask the plugin if a given query will be evaluated as read-only by invoking the  built-in [mysqlnd_ms_is_select](REF:../MYSQLND_MS-FUNCTIONS/) function.

>BEWARE: The built-in read-write splitter is not aware of multi-statements. Multi-statements are seen as one statement. The splitter will check the beginning of the statement to decide where to run the statement. If, for example, a multi-statement begins with SELECT 1 FROM DUAL; INSERT INTO test(id) VALUES (1); ... the plugin will evaluate it as read-only and possibly run it on a slave although the statement is not read-only.



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

######Example 1 `mysqlnd_ms.master_on` ini directive not set
```
<?php
$mysqli = new mysqli("myapp", "username", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

/* Will run on master */
if (!$mysqli->query("SET @x='this is master'")) {
    printf("[%d] %s\n", $mysqli->errno, $mysqli->error);
}

$mysqli->close();
?>
```

######Example 2 `mysqlnd_ms.master_on` ini directive set
```  
mysqlnd_ms.master_on=INSERT,UPDATE,DELETE,LOAD,REPLACE,CREATE,ALTER,DROP,TRUNCATE,RENAME,LOCK,UNLOCK,CALL
```

```
<?php
$mysqli = new mysqli("myapp", "username", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

/* Will run on slave */
if (!$mysqli->query("SET @x='this is slave'")) {
    printf("[%d] %s\n", $mysqli->errno, $mysqli->error);
}

$mysqli->close();
?>
```

