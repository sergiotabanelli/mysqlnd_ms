# Service level and consistency
>NOTE: Together with strictly related [global transaction IDs](REF:../CONCEPTS), the [service level and consistency](REF:../QUICKSTART-AND-EXAMPLES) feature is one of the most changed areas of the `mymysqlnd_ms` fork. Functionalities like [server side read consistency](REFA:) and [server side write consistency](REFA:) allow transparent migration to MySQL clusters in almost all use cases with no or at most extremely small effort and application changes.

Different types of MySQL cluster solutions offer different service and data consistency levels to their users. Any asynchronous MySQL replication cluster offers eventual consistency by default. A read executed on an asynchronous slave may return current, stale or no data at all, depending on whether the slave has replayed all changesets from master or not.

Applications using a MySQL replication cluster need to be designed to work correctly with eventual consistent data. In most cases, however, stale data is not acceptable. In those cases only certain slaves or even only master accesses are allowed to achieve the required quality of service from the cluster.

New MySQL functionalities available in more recent versions, like [multi source replication](https://dev.mysql.com/doc/refman/5.7/en/replication-multi-source.html) or [group replication](https://dev.mysql.com/doc/refman/5.7/en/group-replication.html), allow multi-master clusters and need application strategies to avoid write conflicts and enforce write consistency for distinct write context partitions.

The plugin can transparently choose MySQL replication nodes consistent with the read and write requested consistency. Context partitioning means that clients can share with each others the same configured isolated consistency context. With read consistency, a consistency context participant will always read all writes made by other participants, itself included. With write consistency in multi-master clusters, writes from all consistency context participants will always do not conflicts each others.  

In read and write consistency `mymysqlnd_ms` takes the role of context coordinator. Therefore the plugin can and should be configured to use a persistent shared state store. Currently, `mymysqlnd_ms` supports only compatible memcached protocol state store. Read and write session consistency implementation is stricly related to [global transaction IDs](REF:)(from now on GTIDs) which must always be configured together with the [quality_of_service](REFA:../PLUGIN-CONFIGURATION-FILE.md) filter with session consistency. 

A read context partition is a set of application reads made by a context participant that must always at least run against previous writes made by all other context participants. The `mymysqlnd_ms` plugin can transparently enforce two types of read consistency, [server side read consistency](REFA:) and [client side read consistency](REFA:), both with following rules: 
* Reads belonging to a context partition can safely run only on cluster nodes that has already replicated all previous same context partition writes. 
* Reads belonging to a context partition can safely run on cluster nodes that still has not replicated writes from all other contexts.

New MySQL functionalities available in more recent versions, like [multi source replication](https://dev.mysql.com/doc/refman/5.7/en/replication-multi-source.html) or [group replication](https://dev.mysql.com/doc/refman/5.7/en/group-replication.html) allow multi-master clusters and need application strategies to avoid write conflicts and enforce write consistency for distinct write context partitions. A write context partition is a set of application writes that, if run on distinct masters, can potentialy conflict each others but that does not conflicts with write sets from all other defined partitions. The `mymysqlnd_ms` plugin can transparently enforce two types of write consistency, one is [server side write consistency](REFA:) with the following rules: 
* Writes belonging to distinct context partitions can safely run concurrently on distinct MySQL masters without any data conflicts and replication issues.
* Writes belonging to the same context partition can safely run concurrently only on the same master. 
* Writes belonging to the same context partition can safely run **NON** concurrently, there are no still pending same context writes, on any masters that has already replicated all previous same context writes.

The other write consistency type is [simple client side write consistency](REFA:) with the following rules: 
* Writes belonging to distinct context partitions can safely run concurrently on distinct MySQL masters without any data conflicts and replication issues.
* Writes belonging to the same context partition will run only on the same master. 
* Writes belonging to the same context partition can safely change master only when the time to live of the context has expired (no writes during the time to live interval).

### Context partitioning

### Server side read consistency
**[Requires MySQL >= 5.7.6 with --session-track-gtids=OWN_GTID](https://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html#sysvar_session_track_gtids)**.

##### SSRC Performance considerations

### Server side write consistency
**[Requires MySQL >= 5.7.6 with --session-track-gtids=OWN_GTID](https://dev.mysql.com/doc/refman/5.7/en/server-system-variables.html#sysvar_session_track_gtids)**.

##### SSWC Performance considerations

### Client side read consistency

##### CSRC Performance considerations

### Simple client side write consistency

##### SCSWC Performance considerations

### Requesting configured session consistency


