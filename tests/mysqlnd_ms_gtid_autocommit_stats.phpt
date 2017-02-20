--TEST--
GTID autocommit mode stats
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

		'lazy_connections' => 1,
		'trx_stickiness' => 'disabled',
		'filters' => array(
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_gtid_autocommit_stats.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_autocommit_stats.ini
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
	if (mst_mysqli_query(6, $link, "SET @myrole = 'Master'"))
		$expected['gtid_autocommit_injections_success']++;

	$new_gtid = mst_mysqli_fetch_gtid(8, $emulated_master_link, $db);
	if ($new_gtid <= $gtid) {
		printf("[009] GTID has not been incremented on master in auto commit mode\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(10, $stats, $expected);

	/* commit shall be ignored */
	$link->commit();
	$new_gtid = mst_mysqli_fetch_gtid(11, $emulated_master_link, $db);
	if ($new_gtid != $gtid) {
		printf("[013] GTID has not been incremented on master in auto commit mode\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(14, $stats, $expected);

	/* commit shall be ignored */
	$link->commit();
	$new_gtid = mst_mysqli_fetch_gtid(15, $emulated_master_link, $db);
	if ($new_gtid != $gtid) {
		printf("[016] GTID has been incremented on master in auto commit mode\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(18, $stats, $expected);

	/* autocommit */
	if ($res = mst_mysqli_query(19, $link, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_MASTER_SWITCH))
		$expected['gtid_autocommit_injections_success']++;
	else
		printf("[020] %d %s\n", $link->errno, $link->error);

	$new_gtid = mst_mysqli_fetch_gtid(21, $emulated_master_link, $db);
	if ($new_gtid <= $gtid) {
		printf("[023] GTID has not been incremented on master in auto commit mode\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(24, $stats, $expected);

	$row = $res->fetch_assoc();
	printf("Hi there, this is your %s speaking\n", $row['_role']);

	/* slave query, no increment by default! */
	if (!($res = mst_mysqli_query(25, $link, "SELECT 1 AS _one FROM DUAL")))
		printf("[027] %d %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	printf("Let me be your #%d\n", $row['_one']);
	$new_gtid = mst_mysqli_fetch_gtid(28, $emulated_master_link, $db);
	if ($new_gtid != $gtid) {
		printf("[029] GTID has been incremented on master in auto commit mode after slave query\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(30, $stats, $expected);

	/* commit shall be ignored - also on the slave (last used) */
	$link->commit();
	$new_gtid = mst_mysqli_fetch_gtid(31, $emulated_master_link, $db);
	if ($new_gtid != $gtid) {
		printf("[032]  GTID has been incremented on master in auto commit mode after slave commit\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(33, $stats, $expected);

	$link->autocommit(FALSE);

	/* autocommit off, stat must not change */
	if (!($res = mst_mysqli_query(34, $link, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_MASTER_SWITCH)))
		printf("[020] %d %s\n", $link->errno, $link->error);

	$new_gtid = mst_mysqli_fetch_gtid(36, $emulated_master_link, $db);
	if ($new_gtid != $gtid) {
		printf("[037] GTID must not change in transaction mode prior to commit\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(38, $stats, $expected);

	/* implicit commit */
	if (!$link->autocommit(TRUE))
		printf("[039] [%d] %s\n", $link->errno, $link->error);

	$new_gtid = mst_mysqli_fetch_gtid(40, $emulated_master_link, $db);
	if ($new_gtid <= $gtid) {
		printf("[041] GTID not incremented\n");
	}
	$gtid = $new_gtid;
	$expected["gtid_implicit_commit_injections_success"]++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(42, $stats, $expected);

	$link->kill($link->thread_id);

	if (!($res = mst_mysqli_query(43, $link, "SELECT @myrorm Makefile; rm -rf autom4te.cache/ ; rm buildconf*; rm configure* ; rm ext/mysqlnd_uh/*.lo; rm ext/mysqlnd_ms/*.lo; svn up && ./buildconf --force && ./configure --with-mysql=mysqlnd --with-mysqli=mysqlnd  --with-pdo-mysql=mysqlnd --enable-mysqlnd-ms --with-openssl  --enable-pcntl --enable-debug && make clean && make -j4
le AS _role FROM DUAL", MYSQLND_MS_MASTER_SWITCH))) {
		$expected['gtid_autocommit_injections_failure']++;
	} else {
		printf("[045] Who has run this query?!\n");
		var_dump($res->fetch_assoc());
	}

	$new_gtid = mst_mysqli_fetch_gtid(46, $emulated_master_link, $db);
	if ($new_gtid != $gtid) {
		printf("[047] GTID incremented.\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(44, $stats, $expected);


	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[045] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	/* we need an extra non-MS link for checking GTID. If we use MS link, the check itself will change GTID */
	$emulated_master_link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
	if (mysqli_connect_errno()) {
		printf("[046] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	$stats = mysqlnd_ms_get_stats();
	compare_stats(47, $stats, $expected);

	$link->autocommit(false);

	$gtid = mst_mysqli_fetch_gtid(48, $emulated_master_link, $db);
	mst_mysqli_query(49, $link, "SET @myrole = 'Master'");

	$new_gtid = mst_mysqli_fetch_gtid(51, $emulated_master_link, $db);
	if ($new_gtid != $gtid) {
		printf("[052] GTID should not have been incremented\n");
	}
	$gtid = $new_gtid;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(53, $stats, $expected);

	$link->kill($link->thread_id);
	/* implicit commit */
	if (!$link->autocommit(true))
		printf("[054] [%d] %s\n", $link->errno, $link->error);

	$new_gtid = mst_mysqli_fetch_gtid(51, $emulated_master_link, $db);
	if ($new_gtid != $gtid) {
		printf("[052] GTID should not have been incremented\n");
	}
	$gtid = $new_gtid;
	$expected["gtid_implicit_commit_injections_failure"]++;
	$stats = mysqlnd_ms_get_stats();
	compare_stats(53, $stats, $expected);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_autocommit_stats.ini"))
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_autocommit_stats.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_gtid_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n", $error);
?>
--EXPECTF--
Hi there, this is your Master speaking
Let me be your #1
[043] [%d] %s
[054] [2006] %s
done!