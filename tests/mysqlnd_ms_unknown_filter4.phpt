--TEST--
Unknown filter, filtername int 1
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $master_host_only,
				'port' 		=> (int)$master_port,
				'socket' 	=> $master_socket,
			),
		),
		'slave' => array(
			"slave1" => array(
				'host' 	=> $slave_host_only,
				'port' 	=> (int)$slave_port,
				'socket' => $slave_socket,
			),
		 ),
		'lazy_connections' => 0,
		'filters' => array(
			"random" => array("sticky" => true),
			1 => array( "is" => "not", "fair" => "!"),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_unknown_filter4.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_unknown_filter4.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	if ($link !== FALSE) {
		echo "not ok\n";
	}
	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_unknown_filter4.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_unknown_filter4.ini'.\n");
?>
--EXPECTF--
Warning: mysqli_real_connect(): (HY000/2000): (mysqlnd_ms) Unknown filter '1' . Stopping in %s on line %d
[001] [2000] (mysqlnd_ms) Unknown filter '1' . Stopping
done!