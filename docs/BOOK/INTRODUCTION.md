# Introduction
The `mymysqlnd_ms` is a fork of the mysqlnd replication and load balancing plugin (`mysqlnd_ms`) it adds easy to use MySQL replication support to all PHP MySQL extensions that use mysqlnd. Most of the `mymysqlnd_ms` changes are in [Global transaction IDs](REF:CONCEPTS) and session consistency implementation of the Quality Of Service [service level and consistency](REF:CONCEPTS). The `mymysqlnd_ms` plugin has been tested on PHP5.x (5.5, 5.6) and PHP7.x (7.0, 7.1, 7.2) with no ZTS and ONLY ON LINUX (centos 6 but i hope it works on any linux distribution). Requires libxm2 and libmemcached.

As of version PHP 5.3.3 the MySQL native driver for PHP (mysqlnd) features an internal plugin C API. C plugins, such as the replication and load balancing plugin, can extend the functionality of mysqlnd.

The MySQL native driver for PHP is a C library that ships together with PHP as of PHP 5.3.0. It serves as a drop-in replacement for the MySQL Client Library (libmysqlclient). Using mysqlnd has several advantages: no extra downloads are required because it's bundled with PHP, it's under the PHP license, there is lower memory consumption in certain cases, and it contains new functionality such as asynchronous queries.

Mysqlnd plugins like `mymysqlnd_ms` operate, for the most part, transparently from a user perspective. The replication and load balancing plugin supports all PHP applications, and all MySQL PHP extensions. It does not change existing APIs. Therefore, it can easily be used with existing PHP applications.

## Key Features
The key features of mymysqlnd\_ms are as follows.

* Transparent and therefore easy to use.
     * Supports all of the PHP MySQL extensions.
     * SSL support.
     * A consistent API.
     * Little to no application changes required, dependent on the required usage scenario.
     * Lazy connections: connections to master and slave servers are not opened before a SQL statement is executed.
     * Read consinstency: Transparently avoid replication lag impact, read your writes across different web requests.
     * Write consistency: Transparent support for multi-master circular replication and multi-master group replication with automatic routing based on customizable non conflicting write contexts.

* Can be used with any MySQL clustering solution.
     * MySQL Replication: Read-write splitting is done by the plugin. Primary focus of the plugin.
     * MySQL multi-master circular replication and multi-master group replication: Customizable write contexts conflict partitioning with consistent routing.  
     * MySQL Cluster: Read-write splitting can be disabled.
     * Third-party solutions: the plugin is optimized for MySQL Replication but can be used with any other kind of MySQL clustering solution.

* Featured read-write split strategies
     * Automatic detection of read and write query.
     * Supports SQL hints to overrule automatism.
     * User-defined.
     * Can be disabled for, for example, when using synchronous clusters such as MySQL Cluster.

* Featured load balancing strategies
     * Round Robin: choose a different slave in round-robin fashion for every slave request.
     * Random: choose a random slave for every slave request.
     * Random once (sticky): choose a random slave once to run all slave requests for the duration of a web request.
     * User-defined. The application can register callbacks with mysqlnd_ms.
     * Transaction aware when using API calls only to control transactions.
     * Weighted load balancing: servers can be assigned different priorities, for example, to direct more requests to a powerful machine than to another less powerful machine. Or, to prefer nearby machines to reduce latency.

* Global transaction ID
     * Support for built-in global transaction identifier and [session track gtids](https://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html#sysvar_session_track_gtids) feature of MySQL 5.7.6 or newer.
     * Global transaction identifier Client-side emulation for MySQL < 5.7.6 with memcached plugin support for MySQL > 5.6.6.
     * Supports using transaction ids to identify up-to-date asynchronous slaves and masters when session consistency is required.
     * Throttling: optionally, the plugin can wait for a slave to become "synchronous" before continuing.

* Service and consistency levels
     * Applications can request eventual, session and strong consistency service levels for connections. Appropriate cluster nodes will be searched automatically.
     * Session consistency can be used to transparently avoid replication lag impacts. 
     * Session consistency can be used to transparently avoid write conflicts in asyncronous multi-master clusters.
     * Eventual consistent MySQL Replication slave accesses can be replaced with fast local cache accesses transparently to reduce server load.

* Partitioning and sharding
     * Servers of a replication cluster can be organized into groups. SQL hints can be used to manually direct queries to a specific group. Specific group can also be selected connecting to symbolic hostnames. Grouping can be used to partition (shard) the data, or to cure the issue of hotspots with updates.
     * Write load can be distributed to distinct masters on a non conflicting write contexts basis.

* MySQL Fabric
     * Experimental support for MySQL Fabric is included.

## Limitations
The built-in read-write-split mechanism is very basic. By default, every query which starts with SELECT is considered a read request to be sent to a MySQL slave server. All other queries (such as SHOW statements) are considered as write requests that are sent to the MySQL master server. The build-in behavior can be overruled using SQL hints, or a user-defined callback function or with the new [mysqlnd_ms.master_on](REFA:INSTALLING-CONFIGURING/RUNTIME-CONFIGURATION.md) ini directive

The read-write splitter is not aware of multi-statements. Multi-statements are considered as one statement. The decision of where to run the statement will be based on the beginning of the statement string. For example, if using `mysqli_multi_query()` to execute the multi-statement `SELECT id FROM test ; INSERT INTO test(id) VALUES (1)`, the statement will be redirected to a slave server because it begins with `SELECT`. The `INSERT` statement, which is also part of the multi-statement, will not be redirected to a master server.

> NOTE: Applications must be aware of the consequences of connection switches that are performed for load balancing purposes. Please check the documentation on [Connection pooling and switching](REF:CONCEPTS), [Local transaction handling](REF:CONCEPTS), [Failover](REF:CONCEPTS), [Load balancing](REF:CONCEPTS) and [Read-write splitting](REF:CONCEPTS).

