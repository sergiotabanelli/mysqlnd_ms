--TEST--
Filter QOS, trx_stickiness=on, GTID, roundrobin
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

include_once("util.inc");
$ret = mst_is_slave_of($emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket, $emulated_master_host_only, $emulated_master_port, $emulated_master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true == $ret)
	die("SKIP Configured emulated master and emulated slave could be part of a replication cluster\n");

$sql = mst_get_gtid_sql($db);
if ($error = mst_mysqli_setup_gtid_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
  die(sprintf("SKIP Failed to setup GTID on master, %s\n", $error));

if ($error = mst_mysqli_setup_gtid_table($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket))
	die(sprintf("SKIP Failed to drop GTID table on slave %s\n", $error));

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
		'trx_stickiness' => 'on',
		'global_transaction_id_injection' => array(
			'on_commit'	 				=> $sql['update'],
			'fetch_last_gtid'			=> $sql['fetch_last_gtid'],
			'check_for_gtid'			=> $sql['check_for_gtid'],
			'report_error'				=> true,
		),
		'filters' => array(
			"quality_of_service" => array(
				"session_consistency" => 1,
			),
			"roundrobin" => array(),
		),

	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos_tr_gtid_trx_stickiness_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave1");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master1");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos_tr_gtid_trx_stickiness_rr.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	if ($res = mst_mysqli_query(2, $link, "DROP TABLE IF EXISTS test")) {
		printf("Server: %s\n", mst_mysqli_get_emulated_id(3, $link));
	}
	$gtid = mysqlnd_ms_get_last_gtid($link);
	mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_SESSION,  MYSQLND_MS_QOS_OPTION_GTID, $gtid);

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
	if (!unlink("test_mysqlnd_ms_filter_qos_tr_gtid_trx_stickiness_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos_tr_gtid_trx_stickiness_rr.ini'.\n");
?>
--EXPECTF--
Server: master1-%d
Server: master1-%d
Server: master1-%d
Server: master1-%d
Server: master1-%d
done!