--TEST--
mysqlnd_ms_xa_commit()
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP Emulated master and emulated slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_xa_commit.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_commit.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* Parameter mess */

	if (NULL !== ($ret = @mysqlnd_ms_xa_commit())) {
		printf("[001] Expecting NULL, got %s\n", var_export($ret, true));
	}

	$xa_id = mt_rand(0, 1000);
	if (NULL !== ($ret = @mysqlnd_ms_xa_commit($xa_id))) {
		printf("[002] Expecting NULL, got %s\n", var_export($ret, true));
	}

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[003] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (NULL !== ($ret = @mysqlnd_ms_xa_commit($link, $link))) {
		printf("[004] Expecting NULL, got %s\n", var_export($ret, true));
	}

	if (NULL !== ($ret = @mysqlnd_ms_xa_commit($link, $xa_id, "too_many"))) {
		printf("[005] Expecting NULL, got %s\n", var_export($ret, true));
	}

	$link->close();
	if (false !== ($ret = @mysqlnd_ms_xa_begin($link, $xa_id))) {
		printf("[006] Expecting false, got %s\n", var_export($ret, true));
	}
	/* Basics */

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[007] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	/* No server, matching xid */
	if (!mysqlnd_ms_xa_begin($link, $xa_id) || !mysqlnd_ms_xa_commit($link, $xa_id)) {
		printf("[008] [%d] '%s'\n", $link->errno, $link->error);
	}

	/* No server, xid mismatch */
	if (!mysqlnd_ms_xa_begin($link, $xa_id) || !mysqlnd_ms_xa_commit($link, $xa_id + 1)) {
		printf("[009] [%d] '%s'\n", $link->errno, $link->error);
	}

	/* Does xa_commit clear errors? */
	if (!mysqlnd_ms_xa_commit($link, $xa_id)) {
		printf("[010] Commit should not have failed\n");
	}
	printf("[011] Error shall be cleared... [%d] '%s'\n", $link->errno, $link->error);

	/* No begin */
	if (false !== ($ret = mysqlnd_ms_xa_commit($link, $xa_id))) {
		printf("[012] Expecting false, got %s\n", var_export($ret, true));
	}
	printf("[013] [%d] '%s'\n", $link->errno, $link->error);

	/* Commit after rollback */
	if (!mysqlnd_ms_xa_begin($link, $xa_id) || !mysqlnd_ms_xa_rollback($link, $xa_id)) {
		printf("[014] [%d] '%s'\n", $link->errno, $link->error);
	}
	if (false !== ($ret = mysqlnd_ms_xa_commit($link, $xa_id))) {
		printf("[015] Expecting false, got %s\n", var_export($ret, true));
	}
	printf("[016] [%d] '%s'\n", $link->errno, $link->error);

	/* Does it really do a commit? */

	mysqlnd_ms_xa_begin($link, $xa_id);
	/* Error proves that we are in the middle of a global trx */
	mst_mysqli_query(17, $link, "BEGIN", MYSQLND_MS_MASTER_SWITCH);
	if (!mysqlnd_ms_xa_commit($link, $xa_id)) {
		printf("[018] [%d] '%s'\n", $link->errno, $link->error);
	}
	/* Global trx is over, we may start local trx */
	mst_mysqli_query(19, $link, "BEGIN", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(20, $link, "ROLLBACK", MYSQLND_MS_MASTER_SWITCH);

	/* No stressing, plain begin + commit */
	if (!mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[021] [%d] '%s'\n", $link->errno, $link->error);
	}
	mst_mysqli_query(22, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(23, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);
	if (true !== ($ret = mysqlnd_ms_xa_commit($link, $xa_id))) {
		printf("[024] [%d] '%s'\n", $link->errno, $link->error);
	}

	/* Does the server acutally commit anything?! */

	mst_mysqli_query(25, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(26, $link, "CREATE TABLE test(id INT) ENGINE=InnoDB", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(27, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(28, $link, "CREATE TABLE test(id INT) ENGINE=InnoDB", MYSQLND_MS_LAST_USED_SWITCH);

	if (!mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[029] [%d] '%s'\n", $link->errno, $link->error);
	}
	mst_mysqli_query(30, $link, "INSERT INTO test(id) VALUES (1)", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(31, $link, "INSERT INTO test(id) VALUES (2)", MYSQLND_MS_SLAVE_SWITCH);
	if (true !== ($ret = mysqlnd_ms_xa_commit($link, $xa_id))) {
		printf("[032] [%d] '%s'\n", $link->errno, $link->error);
	}

	$res = mst_mysqli_query(33, $link, "SELECT id AS _from_master FROM test", MYSQLND_MS_MASTER_SWITCH);
	var_dump($res->fetch_all(MYSQLI_ASSOC));

	$res = mst_mysqli_query(34, $link, "SELECT id AS _from_slave FROM test");
	var_dump($res->fetch_all(MYSQLI_ASSOC));

	/* Break one of the connections and see what happens */
	if (!mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[035] [%d] '%s'\n", $link->errno, $link->error);
	}
	mst_mysqli_query(36, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	@$link->kill($link->thread_id);
	mst_mysqli_query(37, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);
	if (true !== ($ret = mysqlnd_ms_xa_commit($link, $xa_id))) {
		printf("[038] [%d] '%s'\n", $link->errno, $link->error);
	}
	/* ... ;-) */
	if (true !== ($ret = mysqlnd_ms_xa_commit($link, $xa_id))) {
		printf("[039] [%d] '%s'\n", $link->errno, $link->error);
	}
	if (true !== ($ret = mysqlnd_ms_xa_rollback($link, $xa_id))) {
		printf("[040] [%d] '%s'\n", $link->errno, $link->error);
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_xa_commit.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_commit.ini'.\n");
?>
--EXPECTF--

Warning: mysqlnd_ms_xa_commit(): (mysqlnd_ms) The XA transaction id does not match the one of from XA begin in %s on line %d
[009] [2000] '(mysqlnd_ms) The XA transaction id does not match the one of from XA begin'
[011] Error shall be cleared... [0] ''

Warning: mysqlnd_ms_xa_commit(): (mysqlnd_ms) There is no active XA transaction to commit in %s on line %d
[013] [2000] '(mysqlnd_ms) There is no active XA transaction to commit'

Warning: mysqlnd_ms_xa_commit(): (mysqlnd_ms) There is no active XA transaction to commit in %s on line %d
[016] [2000] '(mysqlnd_ms) There is no active XA transaction to commit'
[017] [1399] XAER_RMFAIL: The command cannot be executed when global transaction is in the  ACTIVE state
array(1) {
  [0]=>
  array(1) {
    ["_from_master"]=>
    string(1) "1"
  }
}
array(1) {
  [0]=>
  array(1) {
    ["_from_slave"]=>
    string(1) "2"
  }
}

Warning: mysqlnd_ms_xa_commit(): (mysqlnd_ms) Failed to switch participant to XA_IDLE state: %s in %s on line %d
[038] [2006] '(mysqlnd_ms) Failed to switch participant to XA_IDLE state: %s'

Warning: mysqlnd_ms_xa_commit(): (mysqlnd_ms) There is no active XA transaction to commit in %s on line %d
[039] [2000] '(mysqlnd_ms) There is no active XA transaction to commit'

Warning: mysqlnd_ms_xa_rollback(): (mysqlnd_ms) There is no active XA transaction to rollback in %s on line %d
[040] [2000] '(mysqlnd_ms) There is no active XA transaction to rollback'
done!