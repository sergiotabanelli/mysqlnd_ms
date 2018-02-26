# Runtime Configuration
The behaviour of these functions is affected by settings in php.ini.

### Mymysqlnd_ms Configure Options

Name | Default | Changeable
--- | --- | --- |
`mysqlnd_ms.enable` | 0 | `PHP_INI_SYSTEM`
	
mysqlnd_ms.force_config_usage	0	PHP_INI_SYSTEM	
mysqlnd_ms.config_file	""	PHP_INI_SYSTEM	
mysqlnd_ms.collect_statistics	0	PHP_INI_SYSTEM	
mysqlnd_ms.multi_master	0	PHP_INI_SYSTEM	
mysqlnd_ms.disable_rw_split	0	PHP_INI_SYSTEM	

Here's a short explanation of the configuration directives.

mysqlnd_ms.enable integer
Enables or disables the plugin. If disabled, the extension will not plug into mysqlnd to proxy internal mysqlnd C API calls.

mysqlnd_ms.force_config_usage integer
If enabled, the plugin checks if the host (server) parameters value of any MySQL connection attempt, matches a section name from the plugin configuration file. If not, the connection attempt is blocked.

This setting is not only useful to restrict PHP to certain servers but also to debug configuration file problems. The configuration file validity is checked at two different stages. The first check is performed when PHP begins to handle a web request. At this point the plugin reads and decodes the configuration file. Errors thrown at this early stage in an extensions life cycle may not be shown properly to the user. Thus, the plugin buffers the errors, if any, and additionally displays them when establishing a connection to MySQL. By default a buffered startup error will emit an error of type E_WARNING. If force_config_usage is set, the error type used is E_RECOVERABLE_ERROR.

Please, see also configuration file debugging notes.

mysqlnd_ms.config_file string
Plugin specific configuration file.

mysqlnd_ms.collect_statistics integer
Enables or disables the collection of statistics. The collection of statistics is disabled by default for performance reasons. Statistics are returned by the function mysqlnd_ms_get_stats().

mysqlnd_ms.multi_master integer
Enables or disables support of MySQL multi master replication setups. Please, see also supported clusters.

mysqlnd_ms.disable_rw_split integer
Enables or disables built-in read write splitting.

Controls whether load balancing and lazy connection functionality can be used independently of read write splitting. If read write splitting is disabled, only servers from the master list will be used for statement execution. All configured slave servers will be ignored.

The SQL hint MYSQLND_MS_USE_SLAVE will not be recognized. If found, the statement will be redirected to a master.

Disabling read write splitting impacts the return value of mysqlnd_ms_query_is_select(). The function will no longer propose query execution on slave servers.

Note: Multiple master servers
Setting mysqlnd_ms.multi_master=1 allows the plugin to use multiple master servers, instead of only the first master server of the master list.

Please, see also supported clusters.