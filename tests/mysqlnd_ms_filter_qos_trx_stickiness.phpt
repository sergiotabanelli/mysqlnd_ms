--TEST--
Filter QOS, trx_stickiness
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");
require_once("util.inc");

if (version_compare(PHP_VERSION, '5.4.0-dev', '<'))
	die(sprintf("SKIP Requires PHP 5.4.0 or newer, using " . PHP_VERSION));

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
		'trx_stickiness' => 'master',
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos_trx_stickiness.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos_trx_stickiness.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	if ($res = mst_mysqli_query(2, $link, "SELECT 2 FROM DUAL")) {
		printf("Server: %s\n", mst_mysqli_get_emulated_id(3, $link));
	}
	/* its only about in_transaction flag, we don't need to test all combinations */
	if (!$link->autocommit(false)) {
		printf("[003] [%d] %s\n", $link->errno, $link->error);
	}
	if (mst_mysqli_query(4, $link, "DROP TABLE IF EXISTS test")) {
		printf("Server: %s\n", mst_mysqli_get_emulated_id(5, $link));
	}
	if ($res = mst_mysqli_query(6, $link, "SELECT 6 FROM DUAL")) {
		printf("Server: %s\n", mst_mysqli_get_emulated_id(7, $link));
	}

	if (!$link->autocommit(true)) {
		printf("[008] [%d] %s\n", $link->errno, $link->error);
	}
	if ($res = mst_mysqli_query(9, $link, "SELECT 9 FROM DUAL")) {
		printf("Server: %s\n", mst_mysqli_get_emulated_id(10, $link));
	}
	if ($res = mst_mysqli_query(11, $link, "DROP TABLE IF EXISTS test")) {
		printf("Server: %s\n", mst_mysqli_get_emulated_id(12, $link));
	}



	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_qos_trx_stickiness.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos_trx_stickiness.ini'.\n");
?>
--EXPECTF--
Server: slave-%d
Server: master-%d
Server: master-%d
Server: slave-%d
Server: master-%d
done!