--TEST--
mysqlnd_ms_xa_begin()
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
if ($error = mst_create_config("test_mysqlnd_ms_xa_begin.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_begin.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* Parameter mess */

	if (NULL !== ($ret = @mysqlnd_ms_xa_begin())) {
		printf("[001] Expecting NULL, got %s\n", var_export($ret, true));
	}

	$xa_id = mt_rand(0, 1000);
	if (NULL !== ($ret = @mysqlnd_ms_xa_begin($xa_id))) {
		printf("[002] Expecting NULL, got %s\n", var_export($ret, true));
	}

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[003] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (NULL !== ($ret = @mysqlnd_ms_xa_begin($link, $link))) {
		printf("[004] Expecting NULL, got %s\n", var_export($ret, true));
	}

	if (NULL !== ($ret = @mysqlnd_ms_xa_begin($link, $xa_id, "will_i_be_ignored"))) {
		printf("[005] Expecting NULL, got %s\n", var_export($ret, true));
	}

	if (NULL !== ($ret = @mysqlnd_ms_xa_begin($link, $xa_id, 1, "too many"))) {
		printf("[006] Expecting NULL, got %s\n", var_export($ret, true));
	}

	$link->close();
	if (false !== ($ret = @mysqlnd_ms_xa_begin($link, $xa_id))) {
		printf("[007] Expecting false, got %s\n", var_export($ret, true));
	}

	/* Basics */

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[008] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (true !== ($ret = mysqlnd_ms_xa_begin($link, $xa_id))) {
		printf("[009] Expecting true, got %s\n", var_export($ret, true));
	} else {
		printf("[009] [%d] '%s'\n", $link->errno, $link->error);
	}

	if (false !== ($ret = mysqlnd_ms_xa_begin($link, $xa_id))) {
		printf("[010] Expecting false, got %s\n", var_export($ret, true));
	} else {
		/* must end open XA trx first */
		printf("[010] [%d] '%s'\n", $link->errno, $link->error);
	}

	/* No call in between */
	if (!mysqlnd_ms_xa_commit($link, $xa_id)) {
		printf("[011] Commit should not fail\n");
	}

	/* Does xa_begin clear errors? */
	if (@mysqlnd_ms_xa_commit($link, $xa_id)) {
		printf("[012] Failed to provoke error\n");
	}
	printf("[013] Provoking... [%d] '%s'\n", $link->errno, $link->error);

	if (!mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[014] Begin should not have failed\n");
	}
	printf("[015] Error shall be reset... [%d] '%s'\n", $link->errno, $link->error);
	/*
	See whether a XA trx has been started. If it has, then we can expect to see an error
	*/
	mst_mysqli_query(16, $link, "BEGIN", MYSQLND_MS_MASTER_SWITCH);

	if (!mysqlnd_ms_xa_rollback($link, $xa_id)) {
		printf("[017] Rollback should not fail");
	}

	/* No MS connection */
	$link->close();
	if (!($link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)))
		printf("[018] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (false !== ($ret = mysqlnd_ms_xa_begin($link, $xa_id))) {
		printf("[019] Begin should fail, got %s\n", var_export($ret, true));
	}
	printf("[020] [%d] '%s'\n", $link->errno, $link->error);

	$link->close();
	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[021] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());


	/* No stressing, plain begin + commit w query on one server */
	if (true !== ($ret = mysqlnd_ms_xa_begin($link, $xa_id))) {
		printf("[022] [%d] '%s'\n", $link->errno, $link->error);
	}
	mst_mysqli_query(23, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	if (true !== ($ret = mysqlnd_ms_xa_commit($link, $xa_id))) {
		printf("[24] [%d] '%s'\n", $link->errno, $link->error);
	}

	/* No stressing, plain begin + commit w query on one server */
	if (true !== ($ret = mysqlnd_ms_xa_begin($link, $xa_id))) {
		printf("[025] [%d] '%s'\n", $link->errno, $link->error);
	}
	mst_mysqli_query(26, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(27, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);

	if (true !== ($ret = mysqlnd_ms_xa_commit($link, $xa_id))) {
		printf("[028] [%d] '%s'\n", $link->errno, $link->error);
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_xa_begin.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_begin.ini'.\n");
?>
--EXPECTF--
[009] [0] ''

Warning: mysqlnd_ms_xa_begin(): (mysqlnd_ms) The command cannot be executed when global transaction is in the  ACTIVE state. You must end the global/XA transaction first in %s on line %d
[010] [1399] '(mysqlnd_ms) The command cannot be executed when global transaction is in the  ACTIVE state. You must end the global/XA transaction first'
[013] Provoking... [2000] '(mysqlnd_ms) There is no active XA transaction to commit'
[015] Error shall be reset... [0] ''
[016] [1399] XAER_RMFAIL: The command cannot be executed when global transaction is in the  ACTIVE state

Warning: mysqlnd_ms_xa_begin(): (mysqlnd_ms) No mysqlnd_ms connection in %s on line %d
[020] [0] ''
done!