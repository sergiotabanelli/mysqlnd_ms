--TEST--
Filter QOS, eventual consistency
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

_skipif_check_extensions(array("mysqli"));

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
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket,
			),
		 ),

		'lazy_connections' => 0,
		'filters' => array(
			"quality_of_service" => array(
				"eventual_consistency" => 1,
			),
			"random" => array(),
		),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos_eventual_consistency.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos_eventual_consistency.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* master */
	mst_mysqli_query(2, $link, "SET @myrole='master'");

	/* slave */
	$res = mst_mysqli_query(4, $link, "SELECT @myrole FROM DUAL");
	var_dump($res->fetch_assoc());

	/* slave */
	$res = mst_mysqli_query(6, $link, "SELECT @myrole FROM DUAL", MYSQLND_MS_SLAVE_SWITCH);
	var_dump($res->fetch_assoc());

	/* master, allowed */
	$res = mst_mysqli_query(8, $link, "SELECT @myrole FROM DUAL", MYSQLND_MS_MASTER_SWITCH);
	var_dump($res->fetch_assoc());

	/* master */
	$res = mst_mysqli_query(10, $link, "SELECT @myrole FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());

	if (false === ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_SESSION)))
		printf("[012] [%d] %s\n", $link->errno, $link->error);

	/* master */
	$res = mst_mysqli_query(14, $link, "SELECT @myrole FROM DUAL");
	var_dump($res->fetch_assoc());

	if (false === ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL)))
		printf("[016] [%d] %s\n", $link->errno, $link->error);

	/* slave */
	$res = mst_mysqli_query(18, $link, "SELECT @myrole FROM DUAL");
	var_dump($res->fetch_assoc());


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_qos_eventual_consistency.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos_eventual_consistency.ini'.\n");
?>
--EXPECTF--
array(1) {
  ["@myrole"]=>
  NULL
}
array(1) {
  ["@myrole"]=>
  NULL
}
array(1) {
  ["@myrole"]=>
  string(6) "master"
}
array(1) {
  ["@myrole"]=>
  string(6) "master"
}
array(1) {
  ["@myrole"]=>
  string(6) "master"
}
array(1) {
  ["@myrole"]=>
  NULL
}
done!