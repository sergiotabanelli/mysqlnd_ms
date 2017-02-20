--TEST--
Filter QOS, strong consistency, MM + R
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

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

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $emulated_master_host_only,
				'port' 		=> (int)$emulated_master_port,
				'socket' 	=> $emulated_master_socket,
			),
			"master2" => array(
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

		'lazy_connections' => 1,
		'failover' => array('strategy' => 'master'),

		'filters' => array(
			"quality_of_service" => array(
				"strong_consistency" => 1,
			),
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos_multi_master_strong_r.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave1");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master[1,2]");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos_multi_master_strong_r.ini
mysqlnd_ms.multi_master=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
	set_error_handler('mst_error_handler');

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	if ($res = mst_mysqli_query(2, $link, "SELECT 1 AS _num FROM DUAL")) {
		$row = $res->fetch_assoc();
		printf("[003] %d - %s\n", $row['_num'], mst_mysqli_get_emulated_id(4, $link));
	} else {
		printf("[005] No result\n");
	}

	if ($res = mst_mysqli_query(6, $link, "SELECT 2 AS _num FROM DUAL")) {
		$row = $res->fetch_assoc();
		printf("[007] %d - %s\n", $row['_num'], mst_mysqli_get_emulated_id(8, $link));
	} else {
		printf("[009] No result\n");
	}

	if ($res = mst_mysqli_query(10, $link, "SELECT 2 AS _num FROM DUAL", MYSQLND_MS_SLAVE_SWITCH)) {
		$row = $res->fetch_assoc();
		printf("[011] %d - %s\n", $row['_num'], mst_mysqli_get_emulated_id(12, $link));
	} else {
		printf("[013] No result\n");
	}


	if ($res = mst_mysqli_query(14, $link, "SELECT 1 AS _num FROM DUAL")) {
		$row = $res->fetch_assoc();
		printf("[015] %d - %s\n", $row['_num'], mst_mysqli_get_emulated_id(16, $link));
	} else {
		printf("[017] No result\n");
	}


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_qos_multi_master_strong_r.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos_multi_master_strong_r.ini'.\n");
?>
--EXPECTF--
[003] 1 - master[1,2]-%d
[007] 2 - master[1,2]-%d
[011] 2 - master[1,2]-%d
[015] 1 - master[1,2]-%d
done!