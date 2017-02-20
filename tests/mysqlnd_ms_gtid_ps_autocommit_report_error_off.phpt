--TEST--
PS, autocommit, GTID, error
--SKIPIF--
<?php
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

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'filters' => array(
			"roundrobin" => array(),
		),
		'global_transaction_id_injection' => array(
			'on_commit'	 				=> $sql['update'],
			'report_error'				=> false,
		),
		'trx_stickiness' => 'disabled',
		'lazy_connections' => 1
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_gtid_ps_autocommit_report_error_off.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_ps_autocommit_report_error_off.ini
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

	if (!$link->query("DROP TABLE IF EXISTS test") ||
		!$link->query("CREATE TABLE test(id INT) ENGINE=InnoDB"))
		printf("[002] [%d] %s\n", $link->errno, $link->error);

	$expected['gtid_autocommit_injections_success'] += 2;

	/* statement created in autocommit mode on master connection */
	if (!($stmt = $link->prepare(sprintf("/*%s*/SELECT COUNT(*) AS _num_rows FROM test", MYSQLND_MS_MASTER_SWITCH))))
		printf("[003] [%d] %s\n", $link->errno, $link->error);

	$stats = mysqlnd_ms_get_stats();
	compare_stats(4, $stats, $expected);

	if (!$stmt->execute())
		printf("[005] [%d] %s\n", $stmt->errno, $stmt->error);

	$expected['gtid_autocommit_injections_success']++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(6, $stats, $expected);

	$num_rows = NULL;
	if (!$stmt->bind_result($num_rows))
		printf("[007] [%d] %s\n", $stmt->errno, $stmt->error);

	if (!($res = $stmt->fetch()))
		printf("[008] [%d] %s\n", $stmt->errno, $stmt->error);

	printf("Rows %d\n", $num_rows);

	while ($stmt->fetch())
		printf("[009] Clean line...\n");

	if ($err = mst_mysqli_drop_gtid_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[010] %s\n", $err);

	if (!$stmt->execute()) {
		printf("[011] [%d/%s] %s\n", $stmt->errno, $stmt->sqlstate, $stmt->error);
		printf("[012] [%d/%s] %s\n", $link->errno, $link->sqlstate, $link->error);
	}

	$expected['gtid_autocommit_injections_failure']++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(13, $stats, $expected);

	if (!($res = $stmt->get_result()))
		printf("[014] [%d] %s\n", $stmt->errno, $stmt->error);

	$row = $res->fetch_assoc();
	printf("Rows %d\n", $row['_num_rows']);

	if (!$link->query("INSERT INTO test(id) VALUES (1)"))
		printf("[015] [%d] %s\n", $link->errno, $link->error);

	$expected['gtid_autocommit_injections_failure']++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(16, $stats, $expected);

	/* execute shall be done although injection gives error */
	if (!$stmt->execute()) {
		printf("[017] [%d/%s] %s\n", $stmt->errno, $stmt->sqlstate, $stmt->error);
		printf("[018] [%d/%s] %s\n", $link->errno, $link->sqlstate, $link->error);
	}

	$expected['gtid_autocommit_injections_failure']++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(19, $stats, $expected);

	$num_rows = NULL;
	if (!$stmt->bind_result($num_rows))
		printf("[020] [%d] %s\n", $stmt->errno, $stmt->error);

	if (!($res = $stmt->fetch()))
		printf("[021] [%d] %s\n", $stmt->errno, $stmt->error);

	printf("Rows %d\n", $num_rows);

	while ($stmt->fetch());
		;

	if (!$link->query("DROP TABLE IF EXISTS test"))
		printf("[023] [%d] %s\n", $link->errno, $link->error);

	$expected['gtid_autocommit_injections_failure']++;

	if (!$stmt->execute()) {
		printf("[024] [%d/%s] %s\n", $stmt->errno, $stmt->sqlstate, $stmt->error);
		printf("[025] [%d/%s] %s\n", $link->errno, $link->sqlstate, $link->error);
	}

	$expected['gtid_autocommit_injections_failure']++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(26, $stats, $expected);

	if ($error = mst_mysqli_setup_gtid_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[027] %s\n", $error);

	if (!$stmt->execute()) {
		printf("[028] [%d/%s] %s\n", $stmt->errno, $stmt->sqlstate, $stmt->error);
		printf("[029] [%d/%s] %s\n", $link->errno, $link->sqlstate, $link->error);
	}

	$expected['gtid_autocommit_injections_success']++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(30, $stats, $expected);

	$num_rows = NULL;
	if (!$stmt->bind_result($num_rows))
		printf("[031] [%d] %s\n", $stmt->errno, $stmt->error);

	if (!($res = $stmt->fetch()))
		printf("[033] [%d] %s\n", $stmt->errno, $stmt->error);

	printf("Rows %d\n", $num_rows);

	if (!$link->query("CREATE TABLE test(id INT) ENGINE=InnoDB"))
		printf("[034] [%d] %s\n", $link->errno, $link->error);

	$expected['gtid_autocommit_injections_success']++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(35, $stats, $expected);

	if (!$link->query("INSERT INTO test(id) VALUES (1)"))
		printf("[036] [%d] %s\n", $link->errno, $link->error);

	$expected['gtid_autocommit_injections_success']++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(37, $stats, $expected);

	if (!$stmt->execute()) {
		printf("[038] [%d/%s] %s\n", $stmt->errno, $stmt->sqlstate, $stmt->error);
		printf("[039] [%d/%s] %s\n", $link->errno, $link->sqlstate, $link->error);
	}

	$expected['gtid_autocommit_injections_success']++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(40, $stats, $expected);

	$num_rows = NULL;
	if (!$stmt->bind_result($num_rows))
		printf("[041] [%d] %s\n", $stmt->errno, $stmt->error);

	if (!($res = $stmt->fetch()))
		printf("[042] [%d] %s\n", $stmt->errno, $stmt->error);

	printf("Rows %d\n", $num_rows);


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_ps_autocommit_report_error_off.ini"))
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_ps_autocommit_report_error_off.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n", $error);

	if ($error = mst_mysqli_drop_gtid_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n", $error);
?>
--EXPECTF--
Rows 0
Rows 0
Rows 1
[024] [1146/42S02] %s
[025] [1146/42S02] %s
[028] [1146/42S02] %s
[029] [1146/42S02] %s
[033] [2014] %s
Rows 0
Rows 1
done!