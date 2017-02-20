--TEST--
parser: SELECT 'a' AS _id FROM test
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_check_feature(array("parser"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array(
			 "master1" => array(
				'host' 		=> $emulated_master_host_only,
				'port' 		=> (int)$emulated_master_port,
				'socket' 	=> $emulated_master_socket,
			),
		),
		'slave' => array(
			 "slave1" => array(
				'host' 		=> $emulated_slave_host_only,
				'port' 		=> (int)$emulated_slave_port,
				'socket' 	=> $emulated_slave_socket,
			),
		),
		'lazy_connections' => 0,
		'filters' => array(
		),
	),
);

if (_skipif_have_feature("table_filter")) {
	$settings['myapp']['filters']['table'] = array(
		"rules" => array(
			$db . ".%" => array(
				"master" => array("master1"),
				"slave" => array("slave1"),
			),
		),
	);
	$settings['myapp']['filters']['roundrobin'] = array(
	);
}

if ($error = mst_create_config("test_mysqlnd_ms_table_parser1.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_table_parser1.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno())
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());


	mst_mysqli_create_test_table($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);
	mst_mysqli_query(2, $link, "SELECT 1 FROM test", MYSQLND_MS_SLAVE_SWITCH);
	$emulated_slave_id = mst_mysqli_get_emulated_id(3, $link);
	mst_mysqli_fetch_id(5, mst_mysqli_query(4, $link, "SELECT 'a' AS _id FROM test"));

	$server_id = mst_mysqli_get_emulated_id(6, $link);
	if ($emulated_slave_id != $server_id)
		printf("[007] Statement has not been executed on the slave\n");

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_table_parser1.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_table_parser1.ini'.\n");
?>
--EXPECTF--
[005] _id = 'a'
done!