# Runtime Configuration
The behaviour of these functions is affected by settings in php.ini.

### mymysqlnd_ms Configure Options

Name | Default | Changeable
--- | --- | --- |
`mysqlnd_ms.enable` | `0` | `PHP_INI_SYSTEM`
`mysqlnd_ms.force_config_usage` | `0` | `PHP_INI_SYSTEM`
`mysqlnd_ms.config_file` | `""` | `PHP_INI_SYSTEM`
`mysqlnd_ms.config_dir` | `""` | `PHP_INI_SYSTEM`
`mysqlnd_ms.master_on` | `""` | `PHP_INI_SYSTEM`
`mysqlnd_ms.multi_master` | `0` | `PHP_INI_SYSTEM`
`mysqlnd_ms.collect_statistics` | `0` | `PHP_INI_SYSTEM`
`mysqlnd_ms.disable_rw_split` | `0` | `PHP_INI_SYSTEM`

Here's a short explanation of the configuration directives.

### mysqlnd_ms.enable 
type integer

Enables or disables the plugin. If disabled, the extension will not plug into mysqlnd to proxy internal mysqlnd C API calls.

### mysqlnd_ms.force_config_usage 
type integer

If enabled, the plugin checks if the host (server) parameters value of any MySQL connection attempt, matches a section name from the plugin configuration file. If not, the connection attempt is blocked.

This setting is not only useful to restrict PHP to certain servers but also to debug configuration file problems. The configuration file validity is checked at two different stages. The first check is performed when PHP begins to handle a web request. At this point the plugin reads and decodes the configuration file. Errors thrown at this early stage in an extensions life cycle may not be shown properly to the user. Thus, the plugin buffers the errors, if any, and additionally displays them when establishing a connection to MySQL. By default a buffered startup error will emit an error of type `E_WARNING`. If `force_config_usage` is set, the error type used is `E_RECOVERABLE_ERROR`.

Please, see also [Debugging and Tracing](REFA:../PLUGIN-CONFIGURATION-FILE.md).

### mysqlnd_ms.config_file
type string

Specific [plugin configuration file](REFA:../PLUGIN-CONFIGURATION-FILE.md). Whenever a connection to MySQL is being opened, the plugin compares the host parameter value of the connect call, with the cluster section names from the plugin specific configuration file. If, for example, the plugin specific configuration file has a section `myapp` then the section should be referenced by opening a MySQL connection to the host `myapp`

###### Plugin configuration file in php.ini
```
...
mysqlnd_ms.config_file=/path/to/mysqlnd_ms_plugin.json
...
```
###### Plugin json configuration file `/path/to/mysqlnd_ms_plugin.json`
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
### mysqlnd_ms.config_dir
type string

Directory where clusters specific configuration files can be placed, i.e. a specific cluster [plugin configuration file](REFA:../PLUGIN-CONFIGURATION-FILE.md) named `myapp` can be placed in the directory specified by the `config_dir` php.ini directive. In this case the cluster name section must be omitted, previous example for a config file named `myapp` will be:

###### Plugin configuration files directory in php.ini
```
...
mysqlnd_ms.config_file=/path/to/mysqlnd_ms/config_dir
...
```
###### Plugin configuration file `myapp` in `/path/to/mysqlnd_ms/config_dir`
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

### mysqlnd_ms.master_on
type string

By default all query statement that does not start with "SELECT" goes to the master. The `master_on` php.ini directive specifies which query goes to master, i.e.:

```
mysqlnd_ms.master_on=INSERT,UPDATE,DELETE,LOAD,REPLACE,CREATE,ALTER,DROP,TRUNCATE,RENAME,LOCK,UNLOCK,CALL
```
Will route all queries that start with one of the listed tokens to the master/s node/s.

### mysqlnd_ms.collect_statistics
type integer

Enables or disables the collection of statistics. The collection of statistics is disabled by default for performance reasons. Statistics are returned by the function [mysqlnd_ms_get_stats](REF:../MYSQLND_MS-FUNCTIONS/).

### mysqlnd_ms.multi_master
type integer

Enables or disables support of MySQL multi master replication setups. Please, see also [supported clusters](REF:../CONCEPTS/).

### mysqlnd_ms.disable_rw_split
type integer

Enables or disables built-in read write splitting.

Controls whether load balancing and lazy connection functionality can be used independently of read write splitting. If read write splitting is disabled, only servers from the master list will be used for statement execution. All configured slave servers will be ignored.

The SQL hint `MYSQLND_MS_USE_SLAVE` will not be recognized. If found, the statement will be redirected to a master.

Disabling read write splitting impacts the return value of [mysqlnd_ms_query_is_select](REF:../MYSQLND_MS-FUNCTIONS/). The function will no longer propose query execution on slave servers.

>NOTE: Multiple master servers
