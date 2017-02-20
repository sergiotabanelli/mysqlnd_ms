--TEST--
Unknwon filter
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'pick'		=> array('unknown'),
		'master' 	=> array($master_host),
		'slave' 	=> array($slave_host, $slave_host, $slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_unknown_policy.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_unknown_policy.ini
--FILE--
<?php
	require_once("connect.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	if ($link !== FALSE) {
		echo "not ok\n";
	}
	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_unknown_policy.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_unknown_policy.ini'.\n");
?>
--EXPECTF--
Warning: mysqli_real_connect(): (HY000/2000): (mysqlnd_ms) Unknown filter 'unknown' . Stopping in %s on line %d
[001] [2000] (mysqlnd_ms) Unknown filter 'unknown' . Stopping
done!