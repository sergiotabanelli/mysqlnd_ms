--TEST--
Fabric: emulated server
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");
require_once("util.inc");

if (!getenv("MYSQL_TEST_FABRIC")) {
	die(sprintf("SKIP Fabric - set MYSQL_TEST_FABRIC=1 (config.inc) to enable\n"));
}

$process = ms_fork_emulated_fabric_server();
if (is_string($process)) {
	die(sprintf("SKIP %s\n", $process));
}
ms_emulated_fabric_server_shutdown($process);

if ($error = mst_mysqli_create_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, $tablename = "test")) {
  die(sprintf("SKIP Failed to create test table %s\n", $error));
}

$settings = array(
	"myapp" => array(
		'fabric' => array(
			'hosts' => array(
				array('host' => getenv(MYSQL_TEST_FABRIC_EMULATOR_HOST), 'port' => getenv(MYSQL_TEST_FABRIC_EMULATOR_PORT))
			),
			'timeout' => 2
		)
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_fabric_emulated_server.ini", $settings))
  die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_fabric_emulated_server.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");


	$process = ms_fork_emulated_fabric_server();
	if (is_string($process)) {
		die(sprintf("PANIC - there may be already a server running! Details: %s\n", $process));
	}
	/* Give the system some breath time */
	sleep(1);
	/* ms_emulated_fabric_server_defect('sharding.lookup_servers', 'empty'); */
	$stats = mysqlnd_ms_get_stats();
	$start = microtime(true);

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (0 !== mysqli_connect_errno())
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (true !== ($ret = mysqlnd_ms_fabric_select_shard($link, "fabric_sharding.test", 1))) {
		printf("[002] Expecting false, got %s\n", var_export($ret, true));
	}
	if (true !== ($ret = $link->query("UPDATE test SET id = 1 WHERE id = 1"))) {
		printf("[003] Expecting false, got %s, [%d] %s\n", var_export($ret, true),
			$link->errno, $link->error);
	}

	$now = mysqlnd_ms_get_stats();
	foreach ($stats as $k => $v) {
		if ($now[$k] != $v) {
			printf("%s: %s -> %s\n", $k, $v, $now[$k]);
		}
	}
	printf("Runtime %.fs\n", microtime(true) - $start);

	ms_emulated_fabric_server_shutdown($process);

	print "done!";
?>
--CLEAN--
<?php
	require_once("util.inc");
	if ($error = mst_mysqli_drop_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, $tablename = "test"))
		printf("[clean] Cannot remove test table, %s\n", $error);

	if (!unlink("test_mysqlnd_ms_fabric_emulated_server.ini"))
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_fabric_emulated_server.ini'.\n");
?>
--XFAIL--
Emulator does not support dump commands
--EXPECTF--
use_master: 0 -> 1
use_master_guess: 0 -> 1
lazy_connections_master_success: 0 -> 1
fabric_sharding_lookup_servers_success: 0 -> 1
fabric_sharding_lookup_servers_time_total: 0 -> %d
fabric_sharding_lookup_servers_bytes_total: 0 -> %d
Runtime %s
done!