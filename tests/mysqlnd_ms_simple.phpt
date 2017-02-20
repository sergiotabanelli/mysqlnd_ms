--TEST--
Simple
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.force_config_usage=0
mysqlnd_ms.config_file=test_mysqlnd_ms_simple.ini
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"phpBB" => array(
		'master' => array($master_host),
		'slave' => array($slave_host, $slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_simple.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--FILE--
<?php
	require_once("connect.inc");

	$link = mst_mysqli_connect("phpBB", $user, $passwd, "", $port, $socket);
	
	var_dump($res=$link->query("/*ms=slave*/SELECT user from mysql.user"));
	var_dump($res2=$link->query("/*ms=master*/SELECT user from mysql.user"));
	var_dump($res3=$link->query("/*ms=master*/SELECT user from mysql.user"));


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_simple.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_simple.ini'.\n");
?>
--EXPECTF--
object(mysqli_result)#%d (5) {
  ["current_field"]=>
  int(0)
  ["field_count"]=>
  int(1)
  ["lengths"]=>
  NULL
  ["num_rows"]=>
  int(%d)
  ["type"]=>
  int(0)
}
object(mysqli_result)#%d (5) {
  ["current_field"]=>
  int(0)
  ["field_count"]=>
  int(1)
  ["lengths"]=>
  NULL
  ["num_rows"]=>
  int(%d)
  ["type"]=>
  int(0)
}
object(mysqli_result)#%d (5) {
  ["current_field"]=>
  int(0)
  ["field_count"]=>
  int(1)
  ["lengths"]=>
  NULL
  ["num_rows"]=>
  int(%d)
  ["type"]=>
  int(0)
}
done!