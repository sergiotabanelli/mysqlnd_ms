# MySQL Fabric
>NOTE: Consider the support to be of pre-alpha quality. The manual may not list all features or feature limitations. Sharding is the only use case supported by the plugin to date.

>NOTE: Please, check the MySQL reference manual for more information about MySQL Fabric and how to set it up. This manual assumes that you are familiar with the basic concepts and ideas of MySQL Fabric.

>NOTE: New `mymysqlnd_ms` session consistency features does not work with MySQL Fabric. 

MySQL Fabric is a system for managing farms of MySQL servers to achive High Availability and optionally support sharding. Technically, it is a middleware to manage and monitor MySQL servers.

Clients query MySQL Fabric to obtain lists of MySQL servers, their state and their roles. For example, clients can request a list of slaves for a MySQL Replication group and whether they are ready to handle SQL requests. Another example is a cluster of sharded MySQL servers where the client seeks to know which shard to query for a given table and shard key. If configured to use Fabric, the plugin uses XML RCP over HTTP to obtain the list at runtime from a MySQL Fabric host. The XML remote procedure call itself is done in the background and transparent from a developers point of view.

Instead of listing MySQL servers directly in the plugins configuration file it contains a list of one or more MySQL Fabric hosts

Configuration #1 Fabric hosts instead of MySQL servers

```
{
    "myapp": {
        "fabric": {
            "hosts": [
                {
                    "host" : "127.0.0.1",
                    "port" : 8080
                }
            ]
        }
    }
}
```
Users utilize the functions [mysqlnd_ms_fabric_select_shard](REF:../MYSQLND_MS-FUNCTIONS) and [mysqlnd_ms_fabric_select_global](REF:../MYSQLND_MS-FUNCTIONS) to switch to the set of servers responsible for a given shard key. Then, the plugin picks an appropriate server for running queries on. When doing so, the plugin takes care of additional load balancing rules set.

The below example assumes that MySQL Fabric has been setup to shard the table test.fabrictest using the id column of the table as a shard key.

Example #1 Manual partitioning using SQL hints

```
<?php
$mysqli = new mysqli("myapp", "user", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

/* Create a global table - a table available on all shards */
mysqlnd_ms_fabric_select_global($mysqli, "test.fabrictest");
if (!$mysqli->query("CREATE TABLE test.fabrictest(id INT NOT NULL PRIMARY KEY)")) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* Switch connection to appropriate shard and insert record */
mysqlnd_ms_fabric_select_shard($mysqli, "test.fabrictest", 10);
if (!($res = $mysqli->query("INSERT INTO fabrictest(id) VALUES (10)"))) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}

/* Try to read newly inserted record */
mysqlnd_ms_fabric_select_shard($mysqli, "test.fabrictest", 10);
if (!($res = $mysqli->query("SELECT id FROM test WHERE id = 10"))) {
    die(sprintf("[%d] %s\n", $mysqli->errno, $mysqli->error));
}
?>
```
The example creates the sharded table, inserts a record and reads the record thereafter. All SQL data definition language (DDL) operations on a sharded table must be applied to the so called global server group. Prior to creating or altering a sharded table, [mysqlnd_ms_fabric_select_global](REF:../MYSQLND_MS-FUNCTIONS) is called to switch the given connection to the corresponding servers of the global group. Data manipulation (DML) SQL statements must be sent to the shards directly. The [mysqlnd_ms_fabric_select_shard](REF:../MYSQLND_MS-FUNCTIONS) switches a connection to shards handling a certain shard key.
