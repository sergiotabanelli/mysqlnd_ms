# mymysqlnd_ms
These are changes made to the mysqlnd_ms pecl php extension http://php.net/manual/en/book.mysqlnd-ms.php

Main changes are about global transaction gtid injection (see: http://php.net/manual/en/mysqlnd-ms.quickstart.gtid.php) and session consistency implementation in the Quality Of Service filter (see: http://php.net/manual/en/mysqlnd-ms.qos-consistency.php).
Below You will find the list of changes, all start with a "BEGIN HACK" comment and stop with a "END HACK" comment, inside the marked block, original code are in comment blocks.

Any suggestions or comments are very welcome.

SESSION CONSISTENCY AND TRANSACTION ID INJECTION
------------------------------------------------
---> SQL transaction injection use distinct master connection:
If the "on_commit" directive is set then transaction injection feature use a distinct dedicated connection to set the transaction ID on the master. Consequentially the transaction injection can now be done after the effective original master query without any results interaction. 
In original source code, to avoid troubles with results, injection was done before the effective write query and a slave will be selected, by qos session consistency filter, also if still does not propagate the corresponding query. 
The mysqlnd_ms_get_last_gtid php function will now use the distinct master connection instead of the last used connection.

---> Transaction injection with memcached mysql plugin: 
You can use a MySQL memcached plugin managed db table as transaction id repository.
The new "memcached_key" directive can be used as a replacement of "on_commit", "fetch_last_gtid" and  "check_for_gtid" SQL query directives. 
To use this feature login to the master, install the MySQL memcached plugin and set the innodb_api_enable_binlog option. Create the transaction id table ... something like:
```
CREATE DATABASE mygtid;
USE mygtid;
CREATE TABLE `memcached` (
  `id` char(32) NOT NULL DEFAULT '',
  `trx_id` varchar(30) NOT NULL,
  `flags` int(11) NOT NULL,
  `cas` bigint(20) NOT NULL,
  `expiry` int(11) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
```
Add the new transaction id table to innodb_memcache.containers: 
```
INSERT INTO `innodb_memcache`.`containers` (`name`, `db_schema`, `db_table`, `key_columns`, `value_columns`, `flags`, `cas_column`, `expire_time_column`,`unique_idx_name_on_key`) VALUES ('default', 'mygtid', 'memcached', 'id', 'trx_id', 'flags','cas','expiry','PRIMARY');
```
Reload the memcached mysql plugin daemon:
```
UNINSTALL PLUGIN daemon_memcached;
INSTALL PLUGIN daemon_memcached soname "libmemcached.so";
```
On all slaves install the MySQL memcached plugin, if slaves can be promoted to master and have binary log enabled set also the innodb_api_enable_binlog option. 

In mysqlnd_ms json configuration file replace the "on_commit", "fetch_last_gtid" and  "check_for_gtid" directives with new "memcached_key" directive...something like:

        ........
        "global_transaction_id_injection": {
            "memcached_key":"myid"
        },
        .......
	
---> memcached_port and memcached_port_add_hack directives:
To preserve mysql_fabric support, the memcached server port COULD NOT be set on a per server node basis, so the memcached mysql plugin daemons should be run on the same port for every node of the cluster. Use the memcached_port directive to specify it if not running on standard 11211 memcached port. 
For test and development environment that runs multiple mysql instances on the same server, there is also the horrible memcached_port_add_hack directive, if set, the resulting memcached port, will be the mysql specified port for the node plus the specified value. The following configuration example will land to memcached ports 11211=3306+7905 for node wtf0, 11212=3307+7905 for node wtf1, 11213=3308+7905 for node wtf2 :

```
{
    "wtf-1": {
        "master": {
            "master_0": {
                "host": "wtf0",
                "port": "3306"
            }
        },
        "slave": {
            "slave_0": {
                "host": "wtf1",
                "port": "3307"
            },
            "slave_1": {
                "host": "wtf2",
                "port": "3308"
            }
        },
        "gtid_on_connect": 1,
        "global_transaction_id_injection": {
            "memcached_key":"#SID",
            "memcached_port_add_hack":7905
        },
        "filters": {
            "quality_of_service": {
                "session_consistency": 1
            },
            "random": {
                "sticky": "1"
            }
        }
   }
}
```

---> Set gtid qos option after injection:
With qos session consistency the gtid qos option is automatically set after every master injection, this way there is no need to set it through the mysqlnd_ms_set_qos function, you can simply set the qos filter in json configuration file without any change to php applications.

---> Check last gtid on slave only if needed:
Every time a slave connection is checked for session consistency the last retrieved gtid is saved and checked before any new effective check on the same slave.

---> #SID placeholder:
In  "memcached_key", "on_commit", "fetch_last_gtid" and  "check_for_gtid" directives the php session id can be used through the "#SID" placeholder. This is particularly usefull in async ajax application contexts, where you need to force session consistency through subsequent http requests. 

--> gtid_on_connect directive:
If set and qos session consistency is configured, then, on every new connection, the last gtid is retrived from the master and automatically set as the new gtid qos filter option value. This is particularly usefull in async ajax application contexts, where you need to mantain session consistency through subsequent http requests. 

MINOR CHANGES
-------------
---> master_on INI directive:
By default all query statement that does not start with "SELECT" goes to the master. With the new master_on php INI directive You can specify which query goes to master, i.e.:

mysqlnd_ms.master_on=INSERT,UPDATE,DELETE,LOAD,REPLACE,CREATE,ALTER,DROP,TRUNCATE,RENAME,LOCK,UNLOCK,CALL

Will route all queries that start with one of the listed tokens to the master node.

---> Configuration reload if json configuration file has been changed: Usefull in fast-cgi/php-fpm contexts.

---> mysqlnd_ms_set_trx() and mysqlnd_ms_unset_trx() php functions:
Use it to mark transactions begin for php modules, like PDO:beginTransaction, that does not use the mysqlnd_tx_begin call, i.e.:

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

---> More SQL Hints:
MYSQLND_MS_STRONG_SWITCH "ms=strong": Switch to qos strong consistency
MYSQLND_MS_SESSION_SWITCH "ms=session": Switch to qos session consistency
MYSQLND_MS_EVENTUAL_SWITCH "ms=eventual": Switch to qos eventual consistency
MYSQLND_MS_NOINJECT_SWITCH "ms=noinject": Stop transaction id injection
MYSQLND_MS_INJECT_SWITCH "ms=inject": Start transaction id injection

POSSIBLE FUTURE CHANGES (ANY SUGGESTIONS OR HINTS???)
-----------------------------------------------------

---> Add fabric master slave group initialization. Actually fabric support is focused on shards, but fabric can be used also to initialize master slave groups on extension startup.

---> Port to php7 and add some phpt test automation. 

---> Take a look at statistics counters.

---> Add a new "slave_on" INI directive as an alternative to "master_on" INI directive.

---> Add a new "all_on" INI directive to dispatch query to all nodes on the cluster.
