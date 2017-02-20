--TEST--
MM, R/W splitting on but no slaves, RR
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);

include_once("util.inc");

$settings = array(
	"myapp" => array(
		'master' => array("unreachable:7033", $master_host),
		'slave' => array(),
		'pick' 	=> array('roundrobin' => array('sticky' => '1')),
		'lazy_connections' => 1,
		'failover' => array('strategy' => 'loop_before_master', 'max_retries' => 0),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_multi_master_failure_failover_loop_master_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_multi_master_failure_failover_loop_master_rr.ini
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

	@mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);

	/* let's hope we hit both slaves */
	$stats = $exp_stats = mysqlnd_ms_get_stats();
	for ($i = 0; $i < 10; $i++) {
		ob_start();
		$res = $link->query(sprintf("SELECT @myrole AS _msg, %d AS _run FROM DUAL", $i));
		$tmp = ob_get_contents();
		ob_end_clean();
		/* NOTE: it is ok to get a warning from the underlying API if connection fails */
		if (!stristr($tmp, "warning")) {
			/* ... we must be connected to the master */
			$exp_stats['lazy_connections_master_success'] = 1;
		}
		if ($res) {
			$row = $res->fetch_assoc();
			printf("%s %d,", $row['_msg'], $row['_run']);
		} else {
			printf("no result %d,", $i);
		}
	}
	printf("\n");
	$stats = mysqlnd_ms_get_stats();
	compare_stats(3, $stats, $exp_stats, array("lazy_connections_master_success"));


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_multi_master_failure_failover_loop_master_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_multi_master_failure_failover_loop_master_rr.ini'.\n");
?>
--EXPECTF--
master 0,master 1,master 2,master 3,master 4,master 5,master 6,master 7,master 8,master 9,
done!
