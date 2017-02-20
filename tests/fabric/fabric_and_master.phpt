--TEST--
MySQL Fabric: Fabric and fixed master
--SKIPIF--
<?php
require_once(__DIR__.'/../skipif.inc');
_skipif_check_extensions(array("mysqli"));

file_put_contents("fabric_and_master.json", <<<EOT
{
	"testfabric" : {
		"fabric":{
			"hosts": [ {"url":"http://example.com"} ]
		},
		"master": {
		}
	}
}
EOT
);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=fabric_and_master.json
--FILE--
<?php
$c = new mysqli("testfabric", "root", "");
?>
===END===
--CLEAN--
<?php
unlink("fabric_and_master.json");
?>
--EXPECTF--
Warning: mysqli::mysqli(): (mysqlnd_ms) Section [master] exists. Ignored for MySQL Fabric based configuration in %s on line %d
===END===
