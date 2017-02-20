--TEST--
Lazy,loop,ro,multi master
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);

include_once("util.inc");

$settings = array(
	"myapp" => array(
		'master' => array("unreachable:8033", $master_host),
		'slave' => array("unreachable:6033", "unreachable:7033"),
		'pick' 	=> array('random' => array('sticky' => '1')),
		'lazy_connections' => 1,
		'failover' => array('strategy' => 'loop_before_master', 'max_retries' => 0),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_lazy_slave_failure_failover_loop_multi_master_random_once.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_lazy_slave_failure_failover_loop_multi_master_random_once.ini
mysqlnd_ms.collect_statistics=1
mysqlnd_ms.multi_master=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function compare_stats($offset, $stats, $exp_stats, $fields = NULL) {
		if (!$fields) {
			$fields = array_keys($exp_stats);
		}
		foreach ($fields as $k => $field) {
			if (!isset($stats[$field])) {
				printf("[%03d] No such stat '%s'\n",
					$offset, $field);
			} else if ($stats[$field] != $exp_stats[$field]) {
				printf("[%03d] Expecting stat '%s' = %d, got %d\n",
					$offset, $field, $exp_stats[$field], $stats[$field]);
			}
		}
	}

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	/* let's hope we hit both slaves */
	$stats = $exp_stats = mysqlnd_ms_get_stats();
	for ($i = 0; $i < 10; $i++) {
		ob_start();
		$res = $link->query(sprintf("SELECT %d AS _run FROM DUAL", $i));
		$tmp = ob_get_contents();
		ob_end_clean();
		/* NOTE: it is ok to get a warning from the underlying API if connection fails */
		if (!stristr($tmp, "warning")) {
			/* ... we should never get here */
			printf("no warning %d\n", $i);
		} else {
			$exp_stats['lazy_connections_slave_failure']+= 2;
		}
		if ($res) {
			$row = $res->fetch_assoc();
			printf("%d - '%s',", $row['_run'], $link->error);
		} else {
			/* we should never get here */
			printf("no result %d - %s\n", $i, $link->error);
		}
	}
	printf("\n");
	$stats = mysqlnd_ms_get_stats();
	compare_stats(3, $stats, $exp_stats, array("lazy_connections_slave_failure"));

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_lazy_slave_failure_failover_loop_multi_master_random_once.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_lazy_slave_failure_failover_loop_multi_master_random_once.ini'.\n");
?>
--EXPECTF--
0 - '',1 - '',2 - '',3 - '',4 - '',5 - '',6 - '',7 - '',8 - '',9 - '',
done!