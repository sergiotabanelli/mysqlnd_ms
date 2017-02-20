--TEST--
Lazy,loop,random,no master
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

include_once("util.inc");

$settings = array(
	"myapp" => array(
		'master' => array("unreachable:8033"),
		'slave' => array("unreachable:6033", "unreachable:7033"),
		'pick' 	=> array('random'),
		'lazy_connections' => 1,
		'failover' => array('strategy' => 'loop_before_master', 'max_retries' => 0),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_lazy_slave_failure_failover_loop_no_master_random.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_lazy_slave_failure_failover_loop_no_master_random.ini
mysqlnd_ms.collect_statistics=1
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
		$error = "";
		ob_start();
		$res = $link->query(sprintf("SELECT @myrole AS _msg, %d AS _run FROM DUAL", $i));
		$error = $link->error;
		$tmp = ob_get_contents();
		ob_end_clean();
		/* NOTE: it is ok to get a warning from the underlying API if connection fails */
		if (!stristr($tmp, "warning")) {
			/* ... we should never get here */
			printf("no warning %d\n", $i);
		} else {
			$exp_stats['lazy_connections_master_failure']++;
			$exp_stats['lazy_connections_slave_failure']+= 2;
		}
		if ($res) {
			$row = $res->fetch_assoc();
			printf("%s %d\n", $row['_msg'], $row['_run']);
		} else {
			/* note the error message! */
			printf("no result %d - %s\n", $i, $error);
		}
	}
	printf("\n");
	$stats = mysqlnd_ms_get_stats();
	compare_stats(3, $stats, $exp_stats, array("lazy_connections_master_failure", "lazy_connections_slave_failure"));

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_lazy_slave_failure_failover_loop_no_master_random.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_lazy_slave_failure_failover_loop_no_master_random.ini'.\n");
?>
--EXPECTF--
no result 0 - php_network_getaddresses: getaddrinfo failed: %s
no result 1 - php_network_getaddresses: getaddrinfo failed: %s
no result 2 - php_network_getaddresses: getaddrinfo failed: %s
no result 3 - php_network_getaddresses: getaddrinfo failed: %s
no result 4 - php_network_getaddresses: getaddrinfo failed: %s
no result 5 - php_network_getaddresses: getaddrinfo failed: %s
no result 6 - php_network_getaddresses: getaddrinfo failed: %s
no result 7 - php_network_getaddresses: getaddrinfo failed: %s
no result 8 - php_network_getaddresses: getaddrinfo failed: %s
no result 9 - php_network_getaddresses: getaddrinfo failed: %s

done!