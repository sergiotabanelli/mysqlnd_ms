# Running statements
The plugin can be used with any PHP MySQL extension ([mysqli](http://php.net/manual/en/ref.mysqli.php), [mysql](http://php.net/manual/en/ref.mysql.php), [PDO_MYSQL](http://php.net/manual/en/ref.pdo-mysql.php)) that is compiled to use the [mysqlnd](http://php.net/manual/en/book.mysqlnd.php) library. mymysqlnd_ms plugs into the mysqlnd library. It does not change the API or behavior of those extensions.

Whenever a connection to MySQL is being opened, the plugin compares the host parameter value of the connect call, with the cluster section names from the plugin specific configuration file. If, for example, the plugin specific configuration file has a section `myapp` then the section should be referenced by opening a MySQL connection to the host `myapp`

###### Configuration
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
Alternativly a specific cluster configuration file named `myapp` can be placed in the directory specified by the [mysqlnd_ms.config_dir](REFA:../INSTALLING-CONFIGURING/RUNTIME-CONFIGURATION.md) ini directive. In this case the cluster name section must be omitted, previous example for a config file named `myapp` will be:

###### Example 1
```
{
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
```
Now we can open a connection to the configured `myapp` cluster:

###### Example 2
```
<?php
/* Load balanced following "myapp" section rules from the plugins config file */
$mysqli = new mysqli("myapp", "username", "password", "database");
$pdo = new PDO('mysql:host=myapp;dbname=database', 'username', 'password');
$mysql = mysql_connect("myapp", "username", "password");
?>
```
The connection examples above will be load balanced. The plugin will send read-only statements (by default a read-only statement is a `SELECT` statement) to the MySQL slave server with the IP `192.168.2.27` port `3306`. All other statements will be directed to the MySQL master server running on the host `localhost` socket `/tmp/mysql.sock`. This is the defaut behaviour but the plugin [read-write splitting](REF:../CONCEPTS/) logic can also be reversed and instructed to send statements specified in the [mysqlnd_ms.master_on](REFA:../INSTALLING-CONFIGURING/RUNTIME-CONFIGURATION.md) ini directive to the master/s and all other statements to the slave/s. 

The plugin will use the user name `username` and the password `password` to connect to any of the MySQL servers listed in the configured cluster section myapp. Upon connect, the plugin will select `database` as the current schemata.

The username, password and schema name are taken from the connect API calls and used for all servers. In other words: you must use the same username and password for every MySQL server listed in a plugin configuration file section. This is not a general limitation, it is possible to set the username and password for any server in the plugin configuration files, to be used instead of the credentials passed to the API call.

The plugin does not change the API for running statements. [Read-write splitting](REF:../CONCEPTS/) works out of the box. The following example assumes that there is no significant replication lag between the master and the slave.

###### Example 3
```
<?php
/* Load balanced following "myapp" section rules from the plugins config files */
$mysqli = new mysqli("myapp", "username", "password", "database");
if (mysqli_connect_errno()) {
    /* Of course, your error handling is nicer... */
    die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}

/* Statements will be run on the master */
if (!$mysqli->query("DROP TABLE IF EXISTS test")) {
    printf("[%d] %s\n", $mysqli->errno, $mysqli->error);
}
if (!$mysqli->query("CREATE TABLE test(id INT)")) {
    printf("[%d] %s\n", $mysqli->errno, $mysqli->error);
}
if (!$mysqli->query("INSERT INTO test(id) VALUES (1)")) {
    printf("[%d] %s\n", $mysqli->errno, $mysqli->error);
}

/* read-only: statement will be run on a slave */
if (!($res = $mysqli->query("SELECT id FROM test"))) {
    printf("[%d] %s\n", $mysqli->errno, $mysqli->error);
} else {
    $row = $res->fetch_assoc();
    $res->close();
    printf("Slave returns id = '%s'\n", $row['id']);
}
$mysqli->close();
?>
```
The above example will output something similar to:

```
Slave returns id = '1'
add a note add a note
```
