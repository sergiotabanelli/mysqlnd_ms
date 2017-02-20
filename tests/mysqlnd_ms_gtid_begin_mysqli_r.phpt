--TEST--
PS, autocommit, GTID, stmt.store_result
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.4.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.5.0, using " . PHP_VERSION));

_skipif_check_extensions(array("mysqli"));

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
			'report_error'				=> true,
		),
		'trx_stickiness' => 'disabled',
		'lazy_connections' => 1
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_gtid_begin_mysqli_r.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

if (!$link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
	die(sprintf("skip Cannot connect, [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

/* BEGIN READ ONLY exists since MySQL 5.6.5 */
if ($link->server_version < 50605) {
	die(sprintf("skip Emulated master: need MySQL 5.6.5+, got %s", $link->server_version));
}

if (!$link = mst_mysqli_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket))
	die(sprintf("skip Cannot connect, [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

/* BEGIN READ ONLY exists since MySQL 5.6.5 */
if ($link->server_version < 50605) {
	die(sprintf("skip Emulated slave: need MySQL 5.6.5+, got %s", $link->server_version));
}

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_begin_mysqli_r.ini
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
		"gtid_implicit_commit_injections_success" => 0,
		"gtid_implicit_commit_injections_failure" => 0,
	);

	if (!$link->query("DROP TABLE IF EXISTS test") ||
		!$link->query("CREATE TABLE test(id INT) ENGINE=InnoDB"))
		printf("[002] [%d] %s\n", $link->errno, $link->error);

	/* we have to record START TRANSACTION as a new trx in our GTID table */
	$link->begin_transaction();
	$expected['gtid_autocommit_injections_success'] += 3;


	/* statement created in autocommit mode on master connection */
	if (!($stmt = $link->prepare(sprintf("/*%s*/SELECT COUNT(*) AS _num_rows FROM test", MYSQLND_MS_MASTER_SWITCH))))
		printf("[003] [%d] %s\n", $link->errno, $link->error);

	$stats = mysqlnd_ms_get_stats();
	compare_stats(4, $stats, $expected);

	if (!$stmt->execute() || !$stmt->store_result())
		printf("[005] [%d] %s\n", $stmt->errno, $stmt->error);

	$stats = mysqlnd_ms_get_stats();
	compare_stats(6, $stats, $expected);

	$link->commit();
	$expected['gtid_commit_injections_success'] += 1;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(7, $stats, $expected);

	/* autocommit on master but no ps */
	if (!$link->query("INSERT INTO test(id) VALUES (1)")) {
		printf("[008] [%d] %s\n", $link->errno, $link->error);
	}
	$expected['gtid_autocommit_injections_success'] += 1;

	/* Note: transaction stickiness is not set! */
	$link->begin_transaction();
	$expected['gtid_autocommit_injections_success'] += 1;

	$stats = mysqlnd_ms_get_stats();
	compare_stats(9, $stats, $expected);

	if (!($res = $link->query(sprintf("/*%s*/SELECT MAX(id) AS _id FROM test", MYSQLND_MS_MASTER_SWITCH)))) {
		printf("[010] [%d] %s\n", $link->errno, $link->error);
	}
	$row = $res->fetch_assoc();
	printf("id = %d\n", $row['_id']);

	if (!$link->query("DELETE FROM test")) {
		printf("[011] [%d] %s\n", $link->errno, $link->error);
	}

	if (!($res = $link->query(sprintf("/*%s*/SELECT MAX(id) AS _id FROM test", MYSQLND_MS_MASTER_SWITCH)))) {
		printf("[012] [%d] %s\n", $link->errno, $link->error);
	}
	$row = $res->fetch_assoc();
	printf("id = %d\n", $row['_id']);

	/* does not matter whether rollback or commit, no trx stickiness anyway */
	$link->rollback();
	/* rollback does not change gtid table */
	$expected['gtid_commit_injections_success'] += 0;

	$stats = mysqlnd_ms_get_stats();
	compare_stats(13, $stats, $expected);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_begin_mysqli_r.ini"))
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_begin_mysqli_r.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n", $error);

	if ($error = mst_mysqli_drop_gtid_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n", $error);
?>
--EXPECTF--
id = 1
id = 0
done!