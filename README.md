# mymysqlnd_ms
This is a fork of the [mysqlnd replication and load balancing plugin pecl extension](mysqlnd_ms http://php.net/manual/en/book.mysqlnd-ms.php). 

>**DOCUMENTATION IS SLIGHTLY DIFFERNET FROM ORIGINAL ONE AND IT IS STILL NOT READY, YOU CAN FIND IT STARTING FROM [DOCS](docs/BOOK.md) DIRECTORY**

# MAJOR CHANGES
Most changes are in [Global transaction IDs](docs/BOOK/QUICKSTART-AND-EXAMPLES/GLOBAL-TRANSACTION-IDS.md) injection implementation and session consistency implementation of the [Quality Of Service filter](docs/BOOK/QUICKSTART-AND-EXAMPLES/SERVICE-LEVEL-AND-CONSISTENCY.md).

* PHP7.x porting
* [New QOS session consinstency](docs/BOOK/QUICKSTART-AND-EXAMPLES/SERVICE-LEVEL-AND-CONSISTENCY.md) and [transaction id injection](docs/BOOK/QUICKSTART-AND-EXAMPLES/GLOBAL-TRANSACTION-IDS.md)
* [Server side read consistency](docs/BOOK/QUICKSTART-AND-EXAMPLES/SERVICE-LEVEL-AND-CONSISTENCY.md#server-side-read-consistency) (mysql >= 5.7.6 with --session-track-gtids=OWN_GTID)
  * Mysql native built-in read consistency 
  * PHP session_id read consistency enforcing
* [Server side write consistency](docs/BOOK/QUICKSTART-AND-EXAMPLES/SERVICE-LEVEL-AND-CONSISTENCY.md#server-side-write-consistency) (mysql >= 5.7.6 with --session-track-gtids=OWN_GTID)
  * Multi master write consistency 
  * Multi master write consistency logical partitions
* [Client side read consistency](docs/BOOK/QUICKSTART-AND-EXAMPLES/SERVICE-LEVEL-AND-CONSISTENCY.md#client-side-read-consistency)
  * Distinct master connection for client side read consistency and gtid injection
  * Client side transaction id injection with memcached mysql plugin
* [Simple client side write consistency](docs/BOOK/QUICKSTART-AND-EXAMPLES/SERVICE-LEVEL-AND-CONSISTENCY.md#simple-client-side-write-consistency)
  * Multi master write consistency 
  * Multi master write consistency logical partitions   
* New `config_dir` ini directive for [connection based json config files](docs/BOOK/QUICKSTART-AND-EXAMPLES/RUNNING-STATEMENTS.md)
* New `master_on` ini directive for [rw splitting] 
* New `inject_on` ini directive for transcation id consistency tracking/injection
* New `mysqlnd_ms_set_trx` and `mysqlnd_ms_unset_trx` php functions for application based transaction tracking

Any suggestions or comments are very welcome.

>**DOCUMENTATION IS SLIGHTLY DIFFERNET FROM ORIGINAL ONE AND IT IS STILL NOT READY, YOU CAN FIND IT STARTING FROM [DOCS](docs/BOOK.md) DIRECTORY**


>**WORK IN PROGRESS**

## PHP7.x porting
The `mymysqlnd_ms` extension has been tested on PHP5.x (5.5, 5.6) and PHP7.x (7.0, 7.1, 7.2) with no ZTS and ONLY ON LINUX (centos 6 but i hope it works on any linux distribution). Requires libxm2, libmemcached and php json extension. 

## INSTALL
* Download or clone from Github.
* Install php-devel and php json package for your distribution and PHP version.
* Install libmemcached and libxml2 development packages for your distribution.
* from your cloned or downloaded directory run:

```
cd myinstalldir/mymysqlnd_ms
phpize
./configure
make
sudo make install
```
If you find any problems open an [issue on Github](https://github.com/sergiotabanelli/mymysqlnd_ms/issues) 

