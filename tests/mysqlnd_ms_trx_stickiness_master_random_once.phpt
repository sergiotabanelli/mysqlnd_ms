--TEST--
trx_stickiness=master
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP 5.3.99 or newer, using " . PHP_VERSION));

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
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'trx_stickiness' => 'master',
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_trx_stickiness_master_random_once.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_trx_stickiness_master_random_once.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	$emulated_master_thread = mst_mysqli_get_emulated_id(3, $link);
	mst_mysqli_query(4, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);
	$emulated_slave_thread = mst_mysqli_get_emulated_id(5, $link);

	/* DDL, implicit commit */
	mst_mysqli_query(6, $link, "DROP TABLE IF EXISTS test");
	mst_mysqli_query(7, $link, "CREATE TABLE test(id INT) ENGINE=InnoDB");
	mst_mysqli_query(8, $link, "INSERT INTO test(id) VALUES(1), (2), (3)");

	/* autocommit is on, not "in transaction", slave shall be used */

	/* NOTE: we do not run SELECT id FROM test! It shall be possible
	to run the test suite without having to setup MySQL replication.
	The configured master and slave server may not be related, thus
	the table test may not be replicated. It is good enough to
	select from DUAL for testing. What needs to be tested is if any
	read-only query would be still run on the slave or on the master
	because we are in a transaction. The thread id tells us if
	the plugin has chosen the master or the salve. */

	$res = mst_mysqli_query(9, $link, "SELECT 1 AS id FROM DUAL");
	$server_id = mst_mysqli_get_emulated_id(10, $link);
	if ($server_id != $emulated_slave_thread) {
		printf("[011] SELECT in autocommit mode should have been run on the slave\n");
	}
	$row = $res->fetch_assoc();
	$res->close();
	if ($row['id'] != 1)
		printf("[012] Expecting id = 1 got id = '%s'\n", $row['id']);

	/* explicitly setting autocommit via API */
	$link->autocommit(TRUE);
	$res = mst_mysqli_query(13, $link, "SELECT 1 AS id FROM DUAL");
	$server_id = mst_mysqli_get_emulated_id(14, $link);
	if ($server_id != $emulated_slave_thread) {
		printf("[015] SELECT in autocommit mode should have been run on the slave\n");
	}
	$row = $res->fetch_assoc();
	$res->close();
	if ($row['id'] != 1)
		printf("[016] Expecting id = 1 got id = '%s'\n", $row['id']);

	/* explicitly disabling autocommit via API */
	$link->autocommit(FALSE);
	/* this can be the start of a transaction, thus it shall be run on the master */
	$res = mst_mysqli_query(17, $link, "SELECT 1 AS id FROM DUAL");
	$server_id = mst_mysqli_get_emulated_id(18, $link);
	if ($server_id != $emulated_master_thread) {
		printf("[019] SELECT not run in autocommit mode should have been run on the master\n");
	}
	$row = $res->fetch_assoc();
	$res->close();
	if ($row['id'] != 1)
		printf("[020] Expecting id = 1 got id = '%s'\n", $row['id']);

	if (!$link->commit())
		printf("[021] [%d] %s\n", $link->errno, $link->error);

	/* autocommit is still off, thus it shall be run on the master */
	$res = mst_mysqli_query(22, $link, "SELECT id FROM test WHERE id = 1");
	$server_id = mst_mysqli_get_emulated_id(23, $link);
	if ($server_id != $emulated_master_thread) {
		printf("[024] SELECT not run in autocommit mode should have been run on the master\n");
	}
	$row = $res->fetch_assoc();
	$res->close();
	if ($row['id'] != 1)
		printf("[025] Expecting id = 1 got id = '%s'\n", $row['id']);

	/* back to the slave for the next SELECT because autocommit  is on */
	$link->autocommit(TRUE);

	$res = mst_mysqli_query(26, $link, "SELECT 1 AS id FROM DUAL");
	$server_id = mst_mysqli_get_emulated_id(27, $link);
	if ($server_id != $emulated_slave_thread) {
		printf("[028] SELECT in autocommit mode should have been run on the slave\n");
	}
	$row = $res->fetch_assoc();
	$res->close();
	if ($row['id'] != 1)
		printf("[029] Expecting id = 1 got id = '%s'\n", $row['id']);

	/* master because update... */
	mst_mysqli_query(30, $link, "UPDATE test SET id = 100 WHERE id = 1");

	/* back to the master because autocommit is off */
	$link->autocommit(FALSE);

	$res = mst_mysqli_query(31, $link, "SELECT id FROM test WHERE id = 100");
	$server_id = mst_mysqli_get_emulated_id(32, $link);
	if ($server_id != $emulated_master_thread) {
		printf("[033] SELECT not run in autocommit mode should have been run on the master\n");
	}
	$row = $res->fetch_assoc();
	$res->close();
	if ($row['id'] != 100)
		printf("[034] Expecting id = 100 got id = '%s'\n", $row['id']);

	mst_mysqli_query(35, $link, "DELETE FROM test WHERE id = 100");
	if (!$link->rollback())
		printf("[036] [%s] %s\n", $link->errno, $link->error);

	$res = mst_mysqli_query(37, $link, "SELECT id FROM test WHERE id = 100");
	$server_id = mst_mysqli_get_emulated_id(38, $link);
	if ($server_id != $emulated_master_thread) {
		printf("[039] SELECT not run in autocommit mode should have been run on the master\n");
	}
	$row = $res->fetch_assoc();
	$res->close();
	if ($row['id'] != 100)
		printf("[040] Expecting id = 100 got id = '%s'\n", $row['id']);

	/* SQL hint must not win: use master albeit SQL hint used */
	$res = mst_mysqli_query(41, $link, "SELECT 1 AS id FROM DUAL", MYSQLND_MS_SLAVE_SWITCH);
	$server_id = mst_mysqli_get_emulated_id(42, $link);
	if ($server_id == $emulated_slave_thread) {
		printf("[043] Forced SELECT in autocommit mode should have been run on the mast\n");
	}
	$row = $res->fetch_assoc();
	$res->close();
	if ($row['id'] != 1)
		printf("[044] Expecting id = 1 got id = '%s'\n", $row['id']);

	$res = mst_mysqli_query(45, $link, "SELECT 1 AS id FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	$server_id = mst_mysqli_get_emulated_id(46, $link);
	if ($server_id == $emulated_slave_thread) {
		printf("[047] Forced SELECT in autocommit mode should have been run on the slave\n");
	}
	$row = $res->fetch_assoc();
	$res->close();
	if ($row['id'] != 1)
		printf("[048] Expecting id = 1 got id = '%s'\n", $row['id']);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_trx_stickiness_master_random_once.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_trx_stickiness_master_random_once.ini'.\n");
?>
--EXPECTF--
done!