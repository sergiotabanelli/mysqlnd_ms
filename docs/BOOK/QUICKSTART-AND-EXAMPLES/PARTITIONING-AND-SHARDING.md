# Partitioning and Sharding
>NOTE: The `mymysqlnd_ms` fork has more options and tricks to support partitioning and sharding. Documentation for these features should be updated.

Database clustering is done for various reasons. Clusters can improve availability, fault tolerance, and increase performance by applying a divide and conquer approach as work is distributed over many machines. Clustering is sometimes combined with partitioning and sharding to further break up a large complex task into smaller, more manageable units.

The `mymysqlnd_ms` plugin aims to support a wide variety of MySQL database clusters. Some flavors of MySQL database clusters have built-in methods for partitioning and sharding, which could be transparent to use. The plugin supports the two most common approaches: MySQL Replication table filtering, and Sharding (application based partitioning).

MySQL Replication supports partitioning as filters that allow you to create slaves that replicate all or specific databases of the master, or tables. It is then in the responsibility of the application to choose a slave according to the filter rules. You can either use the `mymysqlnd_ms` [node_groups](REFA:../PLUGIN-CONFIGURATION-FILE.md) filter to manually support this, or use the experimental table filter.

Manual partitioning or sharding is supported through the node grouping filter, and [SQL hints](SQL-HINTS.md). The [node_groups](REFA:../PLUGIN-CONFIGURATION-FILE.md) filter lets you assign a symbolic name to a group of master and slave servers. In the example, the master `master_0` and `slave_0` form a group with the name `Partition_A`. It is entirely up to you to decide what makes up a group. For example, you may use node groups for sharding, and use the group names to address shards like `Shard_A_Range_0_100`.

Configuration #1 Cluster node groups

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
                "host": "simulate_slave_failure",
                "port": "0"
            },
            "slave_1": {
                "host": "127.0.0.1",
                "port": 3311
            }
        },
        "filters": {
            "node_groups": {
                "Partition_A" : {
                    "master": ["master_0"],
                    "slave": ["slave_0"]
                }
            },
           "roundrobin": []
        }
    }
}
```

Example #1 Manual partitioning using SQL hints

```
<?php
function select($mysqli, $msg, $hint = '')
{
    /* Note: weak test, two connections to two servers may have the same thread id */
    $sql = sprintf("SELECT CONNECTION_ID() AS _thread, '%s' AS _hint FROM DUAL", $msg);
    if ($hint) {
        $sql = $hint . $sql;
    }
    if (!($res = $mysqli->query($sql))) {
        printf("[%d] %s", $mysqli->errno, $mysqli->error);
        return false;
    }
    $row =  $res->fetch_assoc();
    printf("%d - %s - %s\n", $row['_thread'], $row['_hint'], $sql);
    return true;
}

$mysqli = new mysqli("myapp", "user", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

/* All slaves allowed */
select($mysqli, "slave_0");
select($mysqli, "slave_1");

/* only servers of node group "Partition_A" allowed */
select($mysqli, "slave_1", "/*Partition_A*/");
select($mysqli, "slave_1", "/*Partition_A*/");
?>

6804 - slave_0 - SELECT CONNECTION_ID() AS _thread, 'slave1' AS _hint FROM DUAL
2442 - slave_1 - SELECT CONNECTION_ID() AS _thread, 'slave2' AS _hint FROM DUAL
6804 - slave_0 - /*Partition_A*/SELECT CONNECTION_ID() AS _thread, 'slave1' AS _hint FROM DUAL
6804 - slave_0 - /*Partition_A*/SELECT CONNECTION_ID() AS _thread, 'slave1' AS _hint FROM DUAL
```

By default, the plugin will use all configured master and slave servers for query execution. But if a query begins with a SQL hint like /*node_group*/, the plugin will only consider the servers listed in the `node_group` for query execution. Thus, SELECT queries prefixed with `/*Partition_A*/` will only be executed on slave_0.

