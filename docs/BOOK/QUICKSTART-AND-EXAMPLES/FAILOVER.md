# Failover
By default, the plugin does not attempt to fail over if connecting to a host fails. This prevents pitfalls related to [connection state](REF:). It is recommended to manually handle connection errors in a way similar to a failed transaction. You should catch the error, rebuild the connection state and rerun your query as shown below.

If connection state is no issue to you, you can alternatively enable automatic and silent failover. Depending on the configuration, the automatic and silent failover will either attempt to fail over to the master before issuing and error or, try to connect to other slaves, given the query allowes for it, before attempting to connect to a master. Because automatic [failover](REF:../CONCEPTS) is not fool-proof, it is not discussed in the quickstart. Instead, details are given in the concepts section.

Configuration #1 Manual failover, automatic optional

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
       "filters": { "roundrobin": [] }
    }
 }
```
 
Example #1 Manual failover

```
<?php
$mysqli = new mysqli("myapp", "username", "password", "database");
if (!$mysqli) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

$sql = "SELECT 1 FROM DUAL";

/* error handling as it should be done regardless of the plugin */
if (!($res = $link->query($sql))) {
    /* plugin specific: check for connection error */
    switch ($link->errno) {
    case 2002:
    case 2003:
    case 2005:
        printf("Connection error - trying next slave!\n");
        /* load balancer will pick next slave */
        $res = $link->query($sql);
        break;
    default:
        /* no connection error, failover is unlikely to help */
        die(sprintf("SQL error: [%d] %s", $link->errno, $link->error));
        break;
    }
}
if ($res) {
    var_dump($res->fetch_assoc());
}
?>
```
