# Quickstart and Examples
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

The mysqlnd replication load balancing plugin is easy to use. This quickstart will demo typical use-cases, and provide practical advice on getting started.

It is strongly recommended to read the reference sections in addition to the quickstart. The quickstart tries to avoid discussing theoretical concepts and limitations. Instead, it will link to the reference sections. It is safe to begin with the quickstart. However, before using the plugin in mission critical environments we urge you to read additionally the background information from the reference sections.

The focus is on using PECL mymysqlnd_ms for work with an asynchronous MySQL cluster, namely MySQL replication. Generally speaking an asynchronous cluster is more difficult to use than a synchronous one. Thus, users of, for example, MySQL Cluster will find more information than needed.