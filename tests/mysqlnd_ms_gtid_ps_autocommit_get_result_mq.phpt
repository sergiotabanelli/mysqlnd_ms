--TEST--
PS, autocommit, GTID, stmt.get_result, mq
--SKIPIF--
<?php
die("SKIP not used anymore");
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

include_once("util.inc");
$sql = mst_get_gtid_sql($db);
if ($error = mst_mysqli_setup_gtid_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
  die(sprintf("SKIP Failed to setup GTID on master, %s\n", $error));
if ($error = mst_mysqli_setup_gtid_table($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket))
  die(sprintf("SKIP Failed to setup GTID on slave, %s\n", $error));

if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		die(sprintf("SKIP Cannot connect, [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));

if (!$link->query("DROP PROCEDURE IF EXISTS p") ||
	!$link->query("CREATE PROCEDURE p(IN ver_in VARCHAR(25)) BEGIN SELECT ver_in AS _ver_out; END;") ||
	!$link->prepare("CALL p(?)")) {
	unlink("test_mysqlnd_ms_gtid_ps_autocommit_get_result_mq.ini");
	die(sprintf("SKIP Not supported, [%d] %s\n", $link->errno, $link->error));
}

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'global_transaction_id_injection' => array(
		 	'type'						=> 1,
			'on_commit'	 				=> $sql['update'],
			'fetch_last_gtid'			=> $sql['fetch_last_gtid'],
			'report_error'				=> true,
		),

		'lazy_connections' => 1,
		'trx_stickiness' => 'disabled',
		'filters' => array(
			"quality_of_service" => array(
				"session_consistency" => 1,
			),
			"roundrobin" => array(),
		),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_gtid_ps_autocommit_get_result_mq.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_ps_autocommit_get_result_mq.ini
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

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$expected = array(
		"gtid_autocommit_injections_success" => 0,
		"gtid_autocommit_injections_failure" => 0,
		"gtid_commit_injections_success" => 0,
		"gtid_commit_injections_failure" => 0,
	);

	if (!$link->query("DROP PROCEDURE IF EXISTS p") ||
		!$link->query("CREATE PROCEDURE p(IN ver_in VARCHAR(25)) BEGIN SELECT ver_in AS _ver_out; SELECT 1; END;"))
		printf("[002] [%d] %s\n", $link->errno, $link->error);

	$expected['gtid_autocommit_injections_success'] += 2;

	$stats = mysqlnd_ms_get_stats();
	compare_stats(3, $stats, $expected);

	if (!($stmt = $link->prepare("CALL p(?)")))
		printf("[004] [%d] %s\n", $link->errno, $link->error);

	$version = '12.3';
	if (!$stmt->bind_param('s', $version))
		printf("[005] [%d] %s\n", $stmt->errno, $stmt->error);

	$stats = mysqlnd_ms_get_stats();
	compare_stats(6, $stats, $expected);

	if (!$stmt->execute())
		printf("[007] [%d] %s\n", $stmt->errno, $stmt->error);

	$expected['gtid_autocommit_injections_success']++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(8, $stats, $expected);

	do {
		if ($res = $stmt->get_result())
			var_dump($res->fetch_assoc());
	} while ($stmt->more_results() && $stmt->next_result());

	$stats = mysqlnd_ms_get_stats();
	compare_stats(9, $stats, $expected);

	/* check if line is useable */
	if (!($res = $link->query(sprintf("/*%s*/SELECT 'Is the line' AS _msg FROM DUAL", MYSQLND_MS_MASTER_SWITCH))))
		printf("[010] [%d] %s\n", $link->errno, $link->error);

	$expected['gtid_autocommit_injections_success']++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(11, $stats, $expected);

	$row = $res->fetch_assoc();
	printf("%s\n", $row['_msg']);

	/* just for fun */
	if (!$link->commit())
		printf("[012] [%d] %s\n", $link->errno, $link->error);

	if (!$stmt->bind_param('s', $version) || !$stmt->execute())
		printf("[013] [%d] %s\n", $stmt->errno, $stmt->error);

	$expected['gtid_autocommit_injections_success']++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(14, $stats, $expected);

	do {
		if ($res = $stmt->get_result())
			var_dump($res->fetch_assoc());
	} while ($stmt->more_results() && $stmt->next_result());

	$stats = mysqlnd_ms_get_stats();
	compare_stats(15, $stats, $expected);

	/* slave, no increment */
	if (!($res = $link->query("SELECT 'still useable?' AS _msg FROM DUAL")))
		printf("[016] [%d] %s\n", $link->errno, $link->error);

	$stats = mysqlnd_ms_get_stats();
	compare_stats(17, $stats, $expected);

	$row = $res->fetch_assoc();
	printf("%s\n", $row['_msg']);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_ps_autocommit_get_result_mq.ini"))
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_ps_autocommit_get_result_mq.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_gtid_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n", $error);
	if ($error = mst_mysqli_drop_gtid_table($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket))
		printf("[clean] %s\n", $error);
?>
--EXPECTF--
array(1) {
  ["_ver_out"]=>
  string(4) "12.3"
}
array(1) {
  [1]=>
  int(1)
}
Is the line
array(1) {
  ["_ver_out"]=>
  string(4) "12.3"
}
array(1) {
  [1]=>
  int(1)
}
still useable?
done!