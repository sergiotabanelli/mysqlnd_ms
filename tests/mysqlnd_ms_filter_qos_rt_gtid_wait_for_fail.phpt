--TEST--
Filter QOS, GTID with timeout (fail)
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
	die(sprintf("SKIP Failed to setup GTID table on slave %s\n", $error));

if ($error = mst_mysqli_drop_test_table($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket))
	die(sprintf("Failed to drop test table on slave %s\n", $error));

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

		'global_transaction_id_injection' => array(
			'on_commit'	 				=> $sql['update'],
			'fetch_last_gtid'			=> $sql['fetch_last_gtid'],
			'check_for_gtid'			=> $sql['check_for_gtid_wait'],
			'wait_for_gtid_timeout'		=> $sql['wait_for_gtid_fail'],
			'report_error'				=> true,
		),

	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos_rt_gtid_wait_for_fail.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave1");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master1");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos_rt_gtid_wait_for_fail.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = new mysqli("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	if (!$link->query("DROP TABLE IF EXISTS test") ||
		!$link->query("CREATE TABLE test(id INT) ENGINE=InnoDB") ||
		!$link->query("INSERT INTO test(id) VALUES (1)")) {
		printf("[002] [%d] %s\n", $link->errno, $link->error);
	}

	$emulated_master_id = mst_mysqli_get_emulated_id(3, $link);
	if ($res = mst_mysqli_query(4, $link, "SELECT id FROM test", MYSQLND_MS_MASTER_SWITCH)) {
		var_dump($res->fetch_all());
	}

	if (false === ($gtid = mysqlnd_ms_get_last_gtid($link)))
		printf("[005] [%d] %s\n", $link->errno, $link->error);

	printf("GTID '%s'\n", $gtid);

	/* DO NOT fake a GTID entry on slave - we want to see it fail fetching  */
	if (false == mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_SESSION, MYSQLND_MS_QOS_OPTION_GTID, $gtid)) {
		printf("[006] [%d] %s\n", $link->errno, $link->error);
	}

	/* MS shall try to fetch from slave but run into timeout */
	$run_time = microtime(true);
	if ($res = mst_mysqli_query(7, $link, "SELECT id FROM test")) {
		var_dump($res->fetch_all());
	}

	$run_time = microtime(true) - $run_time;
	$sql = mst_get_gtid_sql($db);
	if ($run_time < $sql['wait_for_gtid_fail']) {
		printf("[008] Runtime %.2fs < %.2fs\n", $run_time, $sql['wait_for_gtid_fail']);
	}

	$server_id = mst_mysqli_get_emulated_id(8, $link);
	/* must be master */
	printf("[009] Run on %s\n", $server_id);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_qos_rt_gtid_wait_for_fail.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos_rt_gtid_wait_for_fail.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");

	if ($error = mst_mysqli_drop_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n", $error);

	if ($error = mst_mysqli_drop_gtid_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n", $error);

	if ($error = mst_mysqli_drop_gtid_table($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket))
		printf("[clean] %s\n", $error);
?>
--EXPECTF--
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
}
GTID '%s'
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
}
[009] Run on master1-%d
done!