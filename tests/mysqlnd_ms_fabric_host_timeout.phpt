--TEST--
Fabric: unreachable/timeout host
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (!getenv("MYSQL_TEST_FABRIC")) {
	die(sprintf("SKIP Fabric - set MYSQL_TEST_FABRIC=1 (config.inc) to enable\n"));
}

$settings = array(
	"myapp" => array(
		'fabric' => array(
			'hosts' => array(
				array('host' => 'example.com', 'port' => 8080)
			),
			'timeout' => 2
		)
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_fabric_host_timeout.ini", $settings))
  die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_fabric_host_timeout.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$stats = mysqlnd_ms_get_stats();
	$begin = microtime(true);

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (0 !== mysqli_connect_errno())
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

		/* Would be surprised if anybody set a mapping for table=DUAL... */
	var_dump(mysqlnd_ms_fabric_select_global($link, "test"));
	if ((microtime(true) - $begin) > 5) {
		printf("[002] Operation took longer than expected. Expecting 2s, allowing 5s, took: %.2fs. Verify manually.\n", microtime(true) - $begin);
	}

	$now = mysqlnd_ms_get_stats();

	foreach ($stats as $k => $v) {
		if ($now[$k] != $v) {
			printf("%s: %s -> %s\n", $k, $v, $now[$k]);
		}
	}
	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_fabric_host_timeout.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_fabric_host_timeout.ini'.\n");
?>
--EXPECTF--
Warning: mysqlnd_ms_fabric_select_global(http://example.com:8080/): failed to open stream: Connection timed out in %s on line %d

Warning: mysqlnd_ms_fabric_select_global(): (mysqlnd_ms) Failed to open stream to any configured Fabric host in %s on line %d
bool(false)
fabric_sharding_lookup_servers_failure: 0 -> 1
fabric_sharding_lookup_servers_time_total: 0 -> %d
done!
