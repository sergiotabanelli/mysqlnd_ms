--TEST--
MySQL Fabric: Invalid strategy
--SKIPIF--
<?php
require_once(__DIR__.'/../skipif.inc');
_skipif_check_extensions(array("mysqli"));

file_put_contents("fabric_invalid_strategy.json", <<<EOT
{
	"testfabric" : {
		"fabric":{
			"hosts": [],
			"strategy": "invalid fabric strategy"
		}
	}
}
EOT
);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=fabric_invalid_strategy.json
--FILE--
<?php
$c = new mysqli("testfabric", "root", "");
?>
===END===
--CLEAN--
<?php
unlink("fabric_invalid_strategy.json");
?>
--EXPECTF--
Warning: mysqli::mysqli(): (mysqlnd_ms) Unknown MySQL Fabric strategy invalid fabric strategy selected, falling back to default dump in %s on line %d
===END===
