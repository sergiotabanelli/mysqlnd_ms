# Setup
The plugin is implemented as a PHP extension. See also the [installation](REF:../INSTALLING-CONFIGURING) instructions to install the `mymysqlnd_ms` extension.

Compile or configure the PHP MySQL extension (API) ([mysqli](http://php.net/manual/en/ref.mysqli.php), [mysql](http://php.net/manual/en/ref.mysql.php), [PDO_MYSQL](http://php.net/manual/en/ref.pdo-mysql.php)) that you plan to use with support for the [mysqlnd](http://php.net/manual/en/book.mysqlnd.php) library. `mymysqlnd_ms` is a plugin for the mysqlnd library. To use the plugin with any of the PHP MySQL extensions, the extension has to use the mysqlnd library.

Then, load the extension into PHP and activate the plugin in the PHP configuration file using the PHP configuration directive named [mysqlnd_ms.enable](REFA:../INSTALLING-CONFIGURING/RUNTIME-CONFIGURATION.md). The following example enable the plugin (php.ini).
###### Example 1
```
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=/path/to/mysqlnd_ms_plugin.ini
```
The plugin uses its own configuration file. Use the PHP configuration directive [mysqlnd_ms.config_file](REFA:../INSTALLING-CONFIGURING/RUNTIME-CONFIGURATION.md) to set the full file path to the plugin-specific configuration file. This file must be readable by PHP (e.g., the web server user).

Create a plugin-specific configuration file. Save the file to the path set by the PHP configuration directive [mysqlnd_ms.config_file](REFA:../INSTALLING-CONFIGURING/RUNTIME-CONFIGURATION.md).

The [plugin configuration file](REF:../) is JSON based. It is divided into one or more cluster sections. Each cluster section has a name, for example, `myapp`. Every cluster section makes its own set of configuration settings.

A section must, at a minimum, list the MySQL replication master server, and set a list of slaves. The plugin supports using only one master server per section. Multi-master MySQL replication setups are not yet fully supported. Use the configuration setting master to set the hostname, and the port or socket of the MySQL master server. MySQL slave servers are configured using the slave keyword.

###### Example 2
```
{
    "myapp": {
        "master": {
            "master_0": {
                "host": "localhost"
            }
        },
        "slave": [

        ]
    }
}
```
Configuring a MySQL slave server list is required, although it may contain an empty list. 
Server lists can use anonymous or non-anonymous syntax. Non-anonymous lists include alias names for the servers, such as master_0 for the master in the above example. The quickstart uses the more verbose non-anonymous syntax.

Instead of using a global configuration file with multiple named cluster sections (e.g. `myapp`, `myapp1`, `myapp2` ecc.) a per section distinct file can be used. Per section configuration files must be named as the cluster choosen name and saved in the directory specified through the [mysqlnd_ms.config_dir](REFA:../INSTALLING-CONFIGURING/RUNTIME-CONFIGURATION.md) ini directive. When the plugin recieve a connection request to a configured cluster name, it first search the global config file for a corresponding cluster section, if no cluster section is found it try to open a corresponding file in the configured [mysqlnd_ms.config_dir](REFA:../INSTALLING-CONFIGURING/RUNTIME-CONFIGURATION.md) directory. The specific cluster config file must not contain a named cluster section but directly the configuration for the cluster, e.g. the config file for cluster named  `myapp1` must be stored in a file named `myapp1` (no extension) and look like in the following example:
###### Example 3
```
{
    "master": {
        "master_0": {
            "host": "master.for.myapp1"
        }
    },
    "slave": {
        "slave_0": {
            "host": "slave.for.myapp1"
        }
    }
}
```

If in a configured cluster there are at least two servers, the plugin can start to load balance and switch connections. Switching connections is not always transparent and can cause issues in certain cases. The reference sections about [connection pooling and switching](REF:../CONCEPTS), [local transaction handling](REF:../CONCEPTS), [failover](REF:../CONCEPTS), [load balancing](REF:../CONCEPTS) and [read-write splitting](REF:../CONCEPTS) all provide more details. And potential pitfalls are described later in this guide.

It is the responsibility of the application to handle potential issues caused by connection switches, by configuring a master with at least one slave server, which allows switching to work therefore related problems can be found.

The MySQL master and MySQL slave servers, which you configure, do not need to be part of MySQL replication setup. For testing purpose you can use single MySQL server and make it known to the plugin as a master and slave server as shown below. This could help you to detect many potential issues with connection switches. However, such a setup will not be prone to the issues caused by replication lag.  In the following example the same server is used as a master and as a slave (testing only!)

###### Example 4
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
The plugin attempts to notify you of invalid configurations. It will throw a warning during PHP startup if the configuration file cannot be read, is empty or parsing the JSON failed. Depending on your PHP settings those errors may appear in some log files only. Further validation is done when a connection is to be established and the configuration file is searched for valid sections. Setting [mysqlnd_ms.force_config_usage](REFA:../INSTALLING-CONFIGURING/RUNTIME-CONFIGURATION.md) may help debugging a faulty setup.