* [Introduction](REF:)
* [Quickstart and Examples](REF:)
     * [Setup](REF:)
     * [Running statements](REF:)
     * [Connection state](REF:)
     * [SQL Hints](REF:)
     * [Local transactions](REF:)
     * [XA Distributed Transactions](REF:)
     * [Service level and consistency](REF:)
     * [Global transaction IDs](REF:)
     * [Cache integration](REF:)
     * [Failover](REF:)
     * [Partitioning and Sharding](REF:)
     * [MySQL Fabric](REF:)
* [Concepts](REF:)
     * [Architecture](REF:)
     * [Connection pooling and switching](REF:)
     * [Local transaction handling](REF:)
     * [Error handling](REF:)
     * [Transient errors](REF:)
     * [Failover](REF:)
     * [Load balancing](REF:)
     * [Read-write splitting](REF:)
     * [Filter](REF:)
     * [Service level and consistency](REF:)
     * [Global transaction IDs](REF:)
     * [Cache integration](REF:)
     * [Supported clusters](REF:)
     * [XA Distributed transactions](REF:)
* [Installing Configuring](REF:)
     * [Requirements](REF:)
     * [Installation](REF:)
     * [Runtime Configuration](REF:)  
* [Plugin configuration file](REF:)
* [Predefined Constants](REF:)
* [Mysqlnd_ms Functions](REF:)
     * [mysqlnd_ms_dump_servers](REF:) — Returns a list of currently configured servers
     * [mysqlnd_ms_fabric_select_global](REF:) — Switch to global sharding server for a given table
     * [mysqlnd_ms_fabric_select_shard](REF:) — Switch to shard
     * [mysqlnd_ms_get_last_gtid](REF:) — Returns the latest global transaction ID
     * [mysqlnd_ms_get_last_used_connection](REF:) — Returns an array which describes the last used connection
     * [mysqlnd_ms_get_stats](REF:) — Returns query distribution and connection statistics
     * [mysqlnd_ms_match_wild](REF:) — Finds whether a table name matches a wildcard pattern or not
     * [mysqlnd_ms_query_is_select](REF:) — Find whether to send the query to the master, the slave or the last used MySQL server
     * [mysqlnd_ms_set_qos](REF:) — Sets the quality of service needed from the cluster
     * [mysqlnd_ms_set_user_pick_server](REF:) — Sets a callback for user-defined read/write splitting
     * [mysqlnd_ms_xa_begin](REF:) — Starts a distributed/XA transaction among MySQL servers
     * [mysqlnd_ms_xa_commit](REF:) — Commits a distributed/XA transaction among MySQL servers
     * [mysqlnd_ms_xa_gc](REF:) — Garbage collects unfinished XA transactions after severe errors
     * [mysqlnd_ms_xa_rollback](REF:) — Rolls back a distributed/XA transaction among MySQL servers
     * [mysqlnd_ms_set_trx](REF:) — Mark transaction begin for application or extensions that directly use SQL to start transactions    
     * [mysqlnd_ms_unset_trx](REF:) — Mark transaction end for application or extensions that directly use SQL to close transactions
* [Change History](REF:)
