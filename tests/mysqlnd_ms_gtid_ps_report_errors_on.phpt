--TEST--
GTID, PS and error on stmt
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

include_once("util.inc");
$sql = mst_get_gtid_sql($db);
if ($error = mst_mysqli_setup_gtid_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
  die(sprintf("SKIP Failed to setup GTID on master, %s\n", $error));

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

		'global_transaction_id_injection' => array(
			'on_commit'	 				=> $sql['update'],
			'report_error'				=> true,
		),

		'lazy_connections' => 1,
		'trx_stickiness' => 'disabled',
		'filters' => array(
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_gtid_ps_report_errors_on.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_ps_report_errors_on.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function compare_stats($offset, $stats, $expected) {
		foreach ($stats as $name => $value) {
			if (isset($expected[$name])) {
				if ($value != $expected[$name]) {
					printf("[%03d] Expecting %s = %d got %d\n", $offset, $name, $expected[$name], $value);
				}
				unset($expected[$name]);
			}
		}
		if (!empty($expected)) {
			printf("[%03d] Dumping list of missing stats\n", $offset);
			var_dump($expected);
		}
	}

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	$expected = array(
		"gtid_autocommit_injections_success" => 0,
		"gtid_autocommit_injections_failure" => 0,
		"gtid_commit_injections_success" => 0,
		"gtid_commit_injections_failure" => 0,
	);
	$stats = mysqlnd_ms_get_stats();
	compare_stats(4, $stats, $expected);

	/* master, autocommit */
	if (!($stmt = $link->prepare("DROP TABLE IF EXISTS test")))
		printf("[005] [%d] %s\n", $link->errno, $link->error);

	if (!$stmt->execute()) {
		printf("[006] [%d] %s\n", $stmt->errno, $stmt->error);
		printf("[007] [%d] %s\n", $link->errno, $link->error);
	}

	$expected["gtid_autocommit_injections_success"]++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(8, $stats, $expected);

	/* provoke error */
	if ($error = mst_mysqli_drop_gtid_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[009] %s\n", $error);


	if (!$stmt->execute()) {
		printf("[010] [%d] %s\n", $stmt->errno, $stmt->error);
		printf("[011] [%d] %s\n", $link->errno, $link->error);
	}

	$expected["gtid_autocommit_injections_failure"]++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(12, $stats, $expected);

	/* ... and here comes the trx table back to life  */
	if ($error = mst_mysqli_setup_gtid_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[013] %s\n", $error);


	if (!$stmt->execute()) {
		printf("[014] [%d] %s\n", $stmt->errno, $stmt->error);
		printf("[015] [%d] %s\n", $link->errno, $link->error);
	}

	$expected["gtid_autocommit_injections_success"]++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(16, $stats, $expected);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_ps_report_errors_on.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_ps_report_errors_on.ini'.\n");
?>
--EXPECTF--
[010] [1146] %s
[011] [1146] %s
done!