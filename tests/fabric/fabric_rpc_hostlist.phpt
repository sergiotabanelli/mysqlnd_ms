--TEST--
MySQL Fabric: Fabric RPC host configuration
--SKIPIF--
<?php
require_once(__DIR__.'/../skipif.inc');
_skipif_check_extensions(array("mysqli"));

file_put_contents("fabric_rpc_hostlist.json", <<<EOT
{
	"testfabric" : {
		"fabric":{
			"hosts": [
				{ "host": "fabric1.example.com", "port": 8080 },
				{ "host": "fabric2.example.com", "port": 8080 },
				{ "url": "http://fabric3.example.com:8080/" }
			]
		}
	}
}
EOT
);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=fabric_rpc_hostlist.json
--FILE--
<?php
$c = new mysqli("testfabric", "root", "");
var_dump(mysqlnd_ms_dump_fabric_rpc_hosts($c));
?>
===END===
--CLEAN--
<?php
unlink("fabric_rpc_hostlist.json");
?>
--EXPECT--
array(3) {
  [0]=>
  array(1) {
    ["url"]=>
    string(32) "http://fabric1.example.com:8080/"
  }
  [1]=>
  array(1) {
    ["url"]=>
    string(32) "http://fabric2.example.com:8080/"
  }
  [2]=>
  array(1) {
    ["url"]=>
    string(32) "http://fabric3.example.com:8080/"
  }
}
===END===
