# Service level and consistency
>NOTE: Together with strictly related [global transaction IDs](REF:), the [service level and consistency](REF:../QUICKSTART-AND-EXAMPLES) feature is one of the most changed areas of the `mymysqlnd_ms` fork. Functionalities like [server side read consistency](REFA:) and [server side write consistency](REFA:) allow transparent migration to MySQL clusters in almost all use cases with no or at most extremely small effort and application changes.

Different types of MySQL cluster solutions offer different service and data consistency levels to their users. Any asynchronous MySQL replication cluster offers eventual consistency by default. A read executed on an asynchronous slave may return current, stale or no data at all, depending on whether the slave has replayed all changesets from master or not.

Applications using a MySQL replication cluster need to be designed to work correctly with eventual consistent data. In most cases, however, stale data is not acceptable. In those cases only certain slaves or even only master accesses are allowed to achieve the required quality of service from the cluster.

New MySQL functionalities available in more recent versions, like [multi source replication](https://dev.mysql.com/doc/refman/5.7/en/replication-multi-source.html) or [group replication](https://dev.mysql.com/doc/refman/5.7/en/group-replication.html), allow multi-master clusters and need application strategies to avoid write conflicts and enforce write consistency for distinct write context partitions.

The plugin can transparently choose MySQL replication nodes consistent with the read and write requested consistency. 

In read and write consistency `mymysqlnd_ms` takes the role of context coordinator. Therefore the plugin can and should be configured to use a persistent shared state store. Currently, `mymysqlnd_ms` supports only compatible memcached protocol state store. Read and write session consistency implementation is strictly related to [global transaction IDs](REF:)(from now on GTIDs) which must always be configured together with the [quality_of_service](REFA:../PLUGIN-CONFIGURATION-FILE.md) filter with session consistency. 

### Context partitioning
Context partitions are sets of queries made by groups of clients (participants) which need to share with each others the same configured isolated consistency context. With read consistency, a consistency context participant will read all writes made by other participants, itself included. With write consistency, in multi-master clusters, writes from all consistency context participants will always do not conflicts each others. Context partitions dimension can range from single query sent by a single client (eventual consistency) to global unique context partition which include all queries sent by all clients. Eventual consistency can indeed be considered as the smallest context partition where every single query from every single client is a context partition, eventual consistency is the default service level, it does not need any consistency or filter configuration. In a-syncronous or semi-syncronous clusters, smaller context partitions means better load distribution and performance. In `mymysqlnd_ms` context partitions are established through the use of placeholders. [Placeholders](REFA:../QUICKSTART-AND-EXAMPLES/GLOBAL-TRANSACTION-IDS.md) are reserved tokens used in configuration values of the [memcached_key](REFA:../PLUGIN-CONFIGURATION-FILE.md), [memcached_wkey](REFA:../PLUGIN-CONFIGURATION-FILE.md), [fetch_last_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) and [on_commit](REFA:../PLUGIN-CONFIGURATION-FILE.md) directives. The placeholder token will be expanded to the corresponding value at connection init, allowing consistency context establishment on a connection attribute basis (for a complete list see [Placeholders](REFA:../QUICKSTART-AND-EXAMPLES/GLOBAL-TRANSACTION-IDS.md)). 

A read context partition is a set of application reads made by a context participant that must always at least run against previous writes made by all other context participants. The `mymysqlnd_ms` plugin can transparently enforce two types of read consistency, [server side read consistency](REFA:) and [client side read consistency]. 
Starting from MySQL 5.7.6 the MySQL server features the [session-track-gtids](https://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html#sysvar_session_track_gtids) system variable, which, if set, will allow a client to be aware of GTID assigned by MySQL to an executed transaction. This will allow the plugin to support effective server side GTIDs consistency scenarios without the need of client side GTID emulation.

For read consistency the most common scenarios is context partitioning on php user session. This can be achieved through the use of the `#SID` placeholder which is expanded to the  php [session_id](http://php.net/manual/en/function.session-id.php) value. This allow application users to always read them writes also if made in different connections and also if distributed on different php application server front-ends. Especially in async ajax scenarios, where reads and writes are often made on distinct http requests, php user session partitioning is of great value and allow transparent migration to MySQL clusters in almost all use cases with no or at most extremely small effort and application changes.   

New MySQL functionalities like [multi source replication](https://dev.mysql.com/doc/refman/5.7/en/replication-multi-source.html) or [group replication](https://dev.mysql.com/doc/refman/5.7/en/group-replication.html) allow multi-master clusters and need application strategies to avoid write conflicts and enforce write consistency for distinct write context partitions. A write context partition is a set of application writes that, if run on distinct masters, can potentially conflict each others but that does not conflict with write sets from all other defined partitions. The `mymysqlnd_ms` plugin can transparently enforce two types of write consistency [server side write consistency](REFA:) and [simple client side write consistency](REFA:). 

It is widely known that adding masters to MySQL clusters does not scale out and does not increase write performance, that is because all masters replicate the same amount of data, so write load will be repeated on every master. However, given that other masters do not have to do the same amount of processing that the original master had to do when it originally executed the transaction, they apply the changes faster, transactions are replicated in a format that is used to apply row transformations only, without having to re-execute transactions again. There are also much more to take into account for cluster configurations, in practice distinct write queries sent to distinct masters will almost always have better total throughput then the same group of queries sent to a single master (as an example see [an overview of the Group Replication performance](https://mysqlhighavailability.com/an-overview-of-the-group-replication-performance/) multi-master peak with flow-control disabled). So the major obstacles to achieve a certain degree of writes scale-out are write conflicts and replication lag. The idea behind `mymysqlnd_ms` write consistency implementation is to move replication lag and write conflicts management to the php application server, that can be considered a far more easier scale-out resource. To summarize the `mymysqlnd_ms` write consistency implementation tries to put loads on easier scalable front ends with the objective to enhance response time on much harder scalable back ends.

For write consistency common scenarios strictly depends from your application requirements.

TODO: give some use case examples 

### Server side read consistency
**[Requires MySQL >= 5.7.6 with --session-track-gtids=OWN_GTID](https://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html#sysvar_session_track_gtids)**.

Server side read consistency has following rules: 
* Reads belonging to a context partition can safely run only on cluster nodes that has already replicated all previous same context partition writes. 
* Reads belonging to a context partition can safely run on cluster nodes that still has not replicated writes from all other contexts.

Server side read consistency works this way: 
1. On connection init, if a state store is configured and [on_connect](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive is active, the plugin retrieve global stored GTID for the partition and set it as the connection current read consistency GTID. 
2. Every time a read query is executed the plugin will choose and check cluster nodes consistency against the connection current read consistency GTID.
3. Every time a write query is executed the plugin will set corresponding GTID as current for the connection and as last global GTID for the configured consistency partition (only if a state store is configured).

##### Performance considerations
* Step 1 is executed only on connection init and needs a read operation from the configured memcached state store. 
* In step 2, on first read query, the plugin will check configured nodes and retrieve the MySQL executed GTID using the [fetch_last_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) query, the retrieved executed GTID are locally stored. Next read queries will be checked first against the locally stored executed GTID and only on check failure a new effective consistency check is done and a new [fetch_last_gtid](REFA:../PLUGIN-CONFIGURATION-FILE.md) query is sent to the node.
* Step 3 needs a write operation to the configured memcached state store. Step 3 is executed only if the evaluated write query effectively changes the MySQL repository, i.e. depending on your configuration the plugin can evaluate a `SET` query as a write query (see [read-write splitting](REF:)) and send it to the master/s although the statement will not change the repository and MySQL will not generate a corresponding GTID, in this case step 3 will not be executed.   

### Server side write consistency
**[Requires MySQL >= 5.7.6 with --session-track-gtids=OWN_GTID](https://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html#sysvar_session_track_gtids)**.

Server side write consistency has following rules: 
* Writes belonging to distinct context partitions can safely run concurrently on distinct MySQL masters without any data conflicts and replication issues.
* Writes belonging to the same context partition can safely run concurrently only on the same master. 
* Writes belonging to the same context partition can safely run **NON** concurrently, there are no still pending same context writes, on any masters that has already replicated all previous same context writes.

In server side write consistency a context partition state is composed by a running counter that take trace of writes concurrency, a token counter used for progressive queries id assignment and an information query record that holds information about the chosen master and the GTID associated with the query. In autocommit mode and if a query is evaluated as a write query, more or less happens the following: 
1. The token counter and the running counter are atomically incremented and the returned values are respectively assigned as id and running counter for the query.
2. The query state of the previous write query (id - 1) is retrieved.
3. If the running counter is greater then 1 the plugin choose the same master otherwise it will check other masters against the GTID of the previous write query and choose a master according to the configured filters.
4. The the chosen master is written as current state for the query id.
5. The query is sent to the chosen master.
6. The associated GTID is written as current state for the query id and the running counter is atomically decremented.
7. if [auto_clean](REFA:../PLUGIN-CONFIGURATION-FILE.md) directive is active the state of the previous write query (id - 1) is deleted from the state store.

##### Performance considerations

### Client side read consistency

##### Performance considerations

### Simple client side write consistency
The other write consistency type is [simple client side write consistency](REFA:) with following rules: 
* Writes belonging to distinct context partitions can safely run concurrently on distinct MySQL masters without any data conflicts and replication issues.
* Writes belonging to the same context partition will run only on the same master. 
* Writes belonging to the same context partition can safely change master only when the time to live of the context has expired (no writes during the time to live interval).

##### Performance considerations

### Requesting configured session consistency


