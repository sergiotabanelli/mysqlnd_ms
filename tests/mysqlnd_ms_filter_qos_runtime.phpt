--TEST--
Filter QOS
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

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
			"quality_of_service" => array(
				"strong_consistency" => 1,
			),
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos_runtime.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

include_once("util.inc");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos_runtime.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = new mysqli($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	var_dump(mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_SESSION));

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	var_dump(mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL));
	/* master */
	mst_mysqli_query(3, $link, "SET @myrole='master'");
	/* slave */
	$res = mst_mysqli_query(5, $link, "SELECT @myrole FROM DUAL");
	var_dump($res->fetch_assoc());
	var_dump(mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_STRONG));
	/* master */
	$res = mst_mysqli_query(7, $link, "SELECT @myrole FROM DUAL");
	var_dump($res->fetch_assoc());

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_qos_runtime.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos_runtime.ini'.\n");
?>
--EXPECTF--
Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No mysqlnd_ms connection in %s on line %d
bool(false)
bool(true)
array(1) {
  ["@myrole"]=>
  NULL
}
bool(true)
array(1) {
  ["@myrole"]=>
  string(6) "master"
}
done!