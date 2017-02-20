--TEST--
Lazy,loop,rr,max_retries,master
--SKIPIF--
<?php
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

$settings = array(
	"myapp" => array(
		'master' => array("unreachable:8033", "unreachable:9033", $emulated_master_host),
		'slave' => array("unreachable:6033", "unreachable:7033", $emulated_slave_host),
		'pick' 	=> array('roundrobin'),
		'lazy_connections' => 1,
		'failover' => array('strategy' => 'loop_before_master', 'max_retries' => 1),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_lazy_master_failure_failover_loop_max_retries_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.multi_master=1
mysqlnd_ms.config_file=test_mysqlnd_ms_lazy_master_failure_failover_loop_max_retries_rr.ini
mysqlnd_ms.collect_statistics=1
mysqlnd_ms.disable_rw_split=1
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
	/* one successful to one slave possible */
	$exp_stats['lazy_connections_master_success'] = 1;
	for ($i = 0; $i < 10; $i++) {
		ob_start();
		$res = $link->query(sprintf("SELECT %d AS _run FROM DUAL", $i));
		$tmp = ob_get_contents();
		ob_end_clean();
		/* NOTE: it is ok to get a warning from the underlying API if connection fails */
		if (stristr($tmp, "warning")) {
			$exp_stats['lazy_connections_master_failure']+=2;
		}
		if ($res) {
			$row = $res->fetch_assoc();
			printf("%s %d,", mst_mysqli_get_emulated_id(2, $link), $row['_run']);
		} else {
			printf("no result %d,", $i);
		}
	}
	printf("\n");
	$stats = mysqlnd_ms_get_stats();
	compare_stats(3, $stats, $exp_stats, array(
		"lazy_connections_master_failure", "lazy_connections_master_success",
		));

	$exp_stats = $stats;
	for ($i = 0; $i < 10; $i++) {
		ob_start();
		$res = $link->query(sprintf("/*%s*/SELECT %d AS _run FROM DUAL", MYSQLND_MS_MASTER_SWITCH, $i));
		$tmp = ob_get_contents();
		ob_end_clean();
		/* NOTE: it is ok to get a warning from the underlying API if connection fails */
		if (stristr($tmp, "warning")) {
			$exp_stats['lazy_connections_master_failure']+=2;
		}
		if ($res) {
			$row = $res->fetch_assoc();
			printf("%s %d,", mst_mysqli_get_emulated_id(4, $link), $row['_run']);
		} else {
			printf("no result %d,", $i);
		}
	}
	printf("\n");
	$stats = mysqlnd_ms_get_stats();
	compare_stats(5, $stats, $exp_stats, array(
		"lazy_connections_master_failure", "lazy_connections_master_success",
		"lazy_connections_master_failure", "lazy_connections_master_success",
		));

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_lazy_master_failure_failover_loop_max_retries_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_lazy_master_failure_failover_loop_max_retries_rr.ini'.\n");
?>
--EXPECTF--
no result 0,master-%d 1,no result 2,master-%d 3,no result 4,master-%d 5,no result 6,master-%d 7,no result 8,master-%d 9,
no result 0,master-%d 1,no result 2,master-%d 3,no result 4,master-%d 5,no result 6,master-%d 7,no result 8,master-%d 9,
done!
