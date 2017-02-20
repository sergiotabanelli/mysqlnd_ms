--TEST--
Fabric: sharding.lookup_servers command
--XFAIL--
Server names not preserved: localhost instead of emulated_*
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
				array('host' => getenv("MYSQL_TEST_FABRIC_EMULATOR_HOST"), 'port' => getenv("MYSQL_TEST_FABRIC_EMULATOR_PORT"))
			),
			'timeout' => 2
		)
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_fabric_select_shard.ini", $settings))
  die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_fabric_select_shard.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function stats_diff($before) {
		$stats = mysqlnd_ms_get_stats();
		foreach ($before as $k => $v) {
			if ($stats[$k] != $v) {
				printf("%s: %s -> %s\n", $k, $v, $stats[$k]);
			}
		}
		return $stats;
	}

	if (NULL !== ($ret = @mysqlnd_ms_fabric_select_shard())) {
		printf("[001] Expecting NULL, got %s\n", var_export($ret, true));
	}

	if (NULL !== ($ret = @mysqlnd_ms_fabric_select_shard(1, 2, 3, 4))) {
		printf("[002] Expecting NULL, got %s\n", var_export($ret, true));
	}

	if (!($link = mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
		printf("[003] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	if (false !== ($ret = @mysqlnd_ms_fabric_select_shard($link,  "fabric_sharding.test", 1))) {
		printf("[004] Expecting false, got %s\n", var_export($ret, true));
	}

	$process = ms_fork_emulated_fabric_server();
	if (is_string($process)) {
		printf("[005] %s\n", $process);
	}
	/* Give the system some breath time */
	sleep(1);

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (0 !== mysqli_connect_errno())
		printf("[006] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$now = mysqlnd_ms_get_stats();
	if (NULL !== ($ret = @mysqlnd_ms_fabric_select_shard($link,  array(), array()))) {
		printf("[007] Expecting NULL, got %s\n", var_export($ret, true));
	}

	$servers = mysqlnd_ms_dump_servers($link);
	printf("[008] Masters %d, slaves %d\n", count($servers['masters']), count($servers['slaves']));
	$now = stats_diff($now);
	if (false !== ($ret = mysqlnd_ms_fabric_select_shard($link,  'letmebeunknown', 1))) {
		printf("[009] Expecting false, got %s\n", var_export($ret, true));
	}
	$now = stats_diff($now);
	$servers = mysqlnd_ms_dump_servers($link);
	printf("[010] Masters %d, slaves %d\n", count($servers['masters']), count($servers['slaves']));

	$now = stats_diff($now);

	/* Where does this one end up? */
	$link->query("UPDATE test SET id = 1 WHERE id = 1");

	$servers = mysqlnd_ms_dump_servers($link);
	printf("[011] Masters %d, slaves %d\n", count($servers['masters']), count($servers['slaves']));
	$now = stats_diff($now);

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);

	if (true !== ($ret = mysqlnd_ms_fabric_select_shard($link,  'fabric_sharding.test', 1))) {
		printf("[012] Expecting true, got %s\n", var_export($ret, true));
	}
	$now = stats_diff($now);
	$servers = mysqlnd_ms_dump_servers($link);
	printf("[013] Masters %d, slaves %d\n", count($servers['masters']), count($servers['slaves']));

	if ($servers['masters'][0]['name_from_config'] != 'emulated master' ||
		$servers['masters'][0]['hostname'] != $emulated_master_host_only ||
		$servers['masters'][0]['user'] != $user ||
		$servers['masters'][0]['port'] != $emulated_master_port /* TODO ||
		$servers['masters'][0]['socket'] != $emulated_master_socket */) {
		printf("[014] Master seems wrong\n");
		var_dump($servers['masters'][0]);
	}
	unset($servers['masters'][0]);

	if ($servers['slaves'][0]['name_from_config'] != 'emulated slave' ||
		$servers['slaves'][0]['hostname'] != $emulated_slave_host_only ||
		$servers['slaves'][0]['user'] != $user ||
		$servers['slaves'][0]['port'] != $emulated_slave_port /*
		TODO ||
		$servers['slaves'][0]['socket'] != $emulated_slave_socket */) {
		printf("[015] Slave seems wrong\n");
		var_dump($servers['slaves'][0]);
	}
	unset($servers['slaves'][0]);

	var_dump($servers);

	ms_emulated_fabric_server_shutdown($process);

	print "done!";
?>
--CLEAN--
<?php
	require_once("util.inc");
	if ($error = mst_mysqli_drop_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, $tablename = "test"))
		printf("[clean] Cannot remove test table, %s\n", $error);

	if (!unlink("test_mysqlnd_ms_fabric_select_shard.ini"))
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_fabric_select_shard.ini'.\n");
?>
--EXPECTF--
[008] Masters 0, slaves 0

Warning: mysqlnd_ms_fabric_select_shard(): (mysqlnd_ms) Failed to find node set in Fabric XML reply in %s on line %d
fabric_sharding_lookup_servers_success: 0 -> 1
fabric_sharding_lookup_servers_time_total: 0 -> %d
fabric_sharding_lookup_servers_bytes_total: 0 -> %d
fabric_sharding_lookup_servers_xml_failure: 0 -> 1
[010] Masters 0, slaves 0

Warning: mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate master connection. 0 masters to choose from. Something is wrong in %s on line %d

Warning: mysqli::query(): (mysqlnd_ms) No connection selected by the last filter in %s on line %d
[011] Masters 0, slaves 0
use_master_guess: 0 -> 1
fabric_sharding_lookup_servers_success: 1 -> 2
fabric_sharding_lookup_servers_time_total: %d -> %d
fabric_sharding_lookup_servers_bytes_total: %d -> %d
[013] Masters 1, slaves 1
array(2) {
  ["masters"]=>
  array(0) {
  }
  ["slaves"]=>
  array(0) {
  }
}
done!