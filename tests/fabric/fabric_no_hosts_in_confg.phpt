--TEST--
MySQL Fabric: No hosts in config
--SKIPIF--
<?php
require_once(__DIR__.'/../skipif.inc');
_skipif_check_extensions(array("mysqli"));

file_put_contents("fabric_no_hosts_in_confg.json", <<<EOT
{
	"testfabric" : {
		"fabric":{
		}
	}
}
EOT
);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=fabric_no_hosts_in_confg.json
--FILE--
<?php
$c = new mysqli("testfabric", "root", "");
?>
===END===
--CLEAN--
<?php
unlink("fabric_no_hosts_in_confg.json");
?>
--EXPECTF--
Fatal error: mysqli::mysqli(): (mysqlnd_ms) Section [hosts] doesn't exist for host. This is needed for MySQL Fabric in %s on line %d
