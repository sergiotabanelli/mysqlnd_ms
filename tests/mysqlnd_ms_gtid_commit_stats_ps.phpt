--TEST--
PS, GTID commit mode stats
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

include_once("util.inc");
$ret = mst_is_slave_of($emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket, $emulated_master_host_only, $emulated_master_port, $emulated_master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true == $ret)
	die("SKIP Configured emulated master and emulated slave could be part of a replication cluster\n");

$sql = mst_get_gtid_sql($db);
if ($error = mst_mysqli_setup_gtid_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
  die(sprintf("SKIP Failed to setup GTID on master, %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");

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
		),

		'lazy_connections' => 0,
		'trx_stickiness' => 'disabled',
		'filters' => array(
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_gtid_commit_stats_ps.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_commit_stats_ps.ini
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

	function mst_mysqli_stmt($offset, $link, $sql, $switch = NULL) {
		$query = sprintf("/*%d*/%s", $offset, $sql);
		if ($switch)
			$query = sprintf("/*%s*/%s", $switch, $query);

		if (!($stmt = $link->prepare($query))) {
			printf("[%03d] [%d] %s\n", $offset, $link->errno, $link->error);
			return NULL;
		}

		if (!$stmt->execute()) {
			printf("[%03d] [%d] %s\n", $offset, $stmt->errno, $stmt->error);
			return NULL;
		}
		return $stmt;
	}

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	/* we need an extra non-MS link for checking GTID. If we use MS link, the check itself will change GTID */
	$emulated_master_link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
	if (mysqli_connect_errno()) {
		printf("[003] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	$expected = array(
		"gtid_autocommit_injections_success" => 0,
		"gtid_autocommit_injections_failure" => 0,
		"gtid_commit_injections_success" => 0,
		"gtid_commit_injections_failure" => 0,
		"gtid_implicit_commit_injections_success" => 0,
		"gtid_implicit_commit_injections_failure" => 0,
	);
	$stats = mysqlnd_ms_get_stats();
	compare_stats(4, $stats, $expected);

	/* auto commit on (default) */
	$gtid = mst_mysqli_fetch_gtid(5, $emulated_master_link, $db);
	if (mst_mysqli_stmt(6, $link, "SET @myrole = 'Master'"))
		$expected['gtid_autocommit_injections_success']++;

	$new_gtid = mst_mysqli_fetch_gtid(8, $emulated_master_link, $db);
	if ($new_gtid <= $gtid) {
		printf("[009] GTID has  been incremented on master in prior to commit\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(10, $stats, $expected);

	/* auto commit off for the real stuff... */
	$link->autocommit(false);

	/* master statement but it must not increment */
	$stmt = mst_mysqli_stmt(11, $link, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_MASTER_SWITCH);
	if (!($res = $stmt->get_result()))
		printf("[013] %d %s\n", $stmt->errno, $stmt->error);

	$new_gtid = mst_mysqli_fetch_gtid(14, $emulated_master_link, $db);
	if ($new_gtid != $gtid) {
		printf("[015] GTID has been incremented on master without commit after master query\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(16, $stats, $expected);

	$row = $res->fetch_assoc();
	printf("I am the %s of ", $row['_role']);

	/* slave statement */
	$stmt = mst_mysqli_stmt(17, $link, "SET @myrole = 'Slave'", MYSQLND_MS_SLAVE_SWITCH);

	$new_gtid = mst_mysqli_fetch_gtid(20, $emulated_master_link, $db);
	if ($new_gtid != $gtid) {
		printf("[021] GTID has been incremented on master without commit after slave query\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(22, $stats, $expected);

	$stmt = mst_mysqli_stmt(23, $link, "SELECT @myrole AS _role FROM DUAL");
	if (!($res = $stmt->get_result()))
		printf("[025] %d %s\n", $link->errno, $link->error);

	$new_gtid = mst_mysqli_fetch_gtid(26, $emulated_master_link, $db);
	if ($new_gtid != $gtid) {
		printf("[027] GTID has been incremented on master without commit after slaver query\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(28, $stats, $expected);

	$row = $res->fetch_assoc();
	printf("poor me, the %s.\n", $row['_role']);

	/* do NOT increment - commit goes to the slave! trx stickiness is off! */
	$link->commit();
	$new_gtid = mst_mysqli_fetch_gtid(27, $emulated_master_link, $db);
	if ($new_gtid != $gtid) {
		printf("[028] GTID has been incremented on master after commit on slave\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(29, $stats, $expected);

	/* master statement but it must not increment */
	$stmt = mst_mysqli_stmt(30, $link, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_MASTER_SWITCH);
	if (!($res = $stmt->get_result()))
		printf("[032] %d %s\n", $link->errno, $link->error);

	$new_gtid = mst_mysqli_fetch_gtid(33, $emulated_master_link, $db);
	if ($new_gtid != $gtid) {
		printf("[034] GTID has been incremented on master without commit after master query\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(35, $stats, $expected);

	/* must increment - last used is master, commit goes to master ! */
	if ($link->commit())
	  $expected['gtid_commit_injections_success']++;

	$new_gtid = mst_mysqli_fetch_gtid(36, $emulated_master_link, $db);
	if ($new_gtid <= $gtid) {
		printf("[038] GTID has not been incremented on master after commit on master\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(40, $stats, $expected);

	/* must increment - last used is master, commit goes to master ! */
	if ($link->commit())
	  $expected['gtid_commit_injections_success']++;
	$new_gtid = mst_mysqli_fetch_gtid(41, $emulated_master_link, $db);
	if ($new_gtid <= $gtid) {
		printf("[042] GTID has not been incremented on master after commit on master\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(43, $stats, $expected);

	/* implicit commit */
	$link->autocommit(true);

	$new_gtid = mst_mysqli_fetch_gtid(44, $emulated_master_link, $db);
	if ($new_gtid <= $gtid) {
		printf("[045] GTID not incremented\n");
	}
	$gtid = $new_gtid;
	$expected["gtid_implicit_commit_injections_success"]++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(46, $stats, $expected);

	$link->commit();

	$stats = mysqlnd_ms_get_stats();
	compare_stats(47, $stats, $expected);

	$link->autocommit(false);

	$stats = mysqlnd_ms_get_stats();
	compare_stats(48, $stats, $expected);


	$new_gtid = mst_mysqli_fetch_gtid(49, $emulated_master_link, $db);
	if ($new_gtid != $gtid) {
		printf("[050] GTID incremented $new_gtid $gtid\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(51, $stats, $expected);

	/* failure */
	$link->kill($link->thread_id);
	$link->commit();

	$new_gtid = mst_mysqli_fetch_gtid(52, $emulated_master_link, $db);
	if ($new_gtid != $gtid) {
		printf("[053] GTID has must not change because commit was done on dead connection\n");
	}
	$stats = mysqlnd_ms_get_stats();
	$expected["gtid_commit_injections_failure"]++;
	compare_stats(54, $stats, $expected);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_commit_stats_ps.ini"))
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_commit_stats_ps.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_gtid_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n", $error);
?>
--EXPECTF--
I am the Master of poor me, the Slave.
done!