--TEST--
mysqlnd_ms_xa_rollback() participant error
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
		'lazy_connections' => 0,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_xa_rollback_begin_fail.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_rollback_begin_fail.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$xa_id = mt_rand(0, 1000);


	/* Killing the only participants connections, which is the slave connection */

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	$servers = mysqlnd_ms_dump_servers($link);

	if (!($link_kill = mst_mysqli_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket)))
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!$link_kill->kill($servers['slaves'][0]['thread_id'])) {
		printf("[003] Failed to kill slave connection: [%d] %s\n", $link_kill->errno, $link_kill->error);
	}
	$link_kill->close();

	mysqlnd_ms_xa_begin($link, $xa_id);
	mst_mysqli_query(4, $link, "SELECT 1");
	$ret = mysqlnd_ms_xa_rollback($link, $xa_id);
	printf("[005] rollback returned '%s', [%d/%s] '%s'\n", var_export($ret, true), $link->errno, $link->sqlstate, $link->error);
	$link->close();

	/* Killing the connection of one participants only */

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[006] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	$servers = mysqlnd_ms_dump_servers($link);

	if (!($link_kill = mst_mysqli_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket)))
		printf("[007] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!$link_kill->kill($servers['slaves'][0]['thread_id'])) {
		printf("[008] Failed to kill the slave connection: [%d] %s\n", $link_kill->errno, $link_kill->error);
	}
	$link_kill->close();

	if (!($link_kill = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)))
		printf("[009] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!$link_kill->kill($servers['masters'][0]['thread_id'])) {
		printf("[010] Failed to kill the master connection: [%d] %s\n", $link_kill->errno, $link_kill->error);
	}
	$link_kill->close();

	mysqlnd_ms_xa_begin($link, $xa_id);
	mst_mysqli_query(11, $link, "SELECT 1");
	mst_mysqli_query(12, $link, "SET @my_role='master'");
	$ret = mysqlnd_ms_xa_rollback($link, $xa_id);
	printf("[013] rollback returned '%s', [%d/%s] '%s'\n", var_export($ret, true), $link->errno, $link->sqlstate, $link->error);
	$link->close();

	/* Killing one out of two participants connections */

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[014] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	$servers = mysqlnd_ms_dump_servers($link);

	if (!($link_kill = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)))
		printf("[015] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!$link_kill->kill($servers['masters'][0]['thread_id'])) {
		printf("[016] Failed to kill the master connection: [%d] %s\n", $link_kill->errno, $link_kill->error);
	}
	$link_kill->close();

	if (!($link_kill = mst_mysqli_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket)))
		printf("[017] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!$link_kill->kill($servers['slaves'][0]['thread_id'])) {
		printf("[018] Failed to kill the slave connection: [%d] %s\n", $link_kill->errno, $link_kill->error);
	}
	$link_kill->close();

	mysqlnd_ms_xa_begin($link, $xa_id);
	mst_mysqli_query(19, $link, "SELECT 1");
	mst_mysqli_query(20, $link, "SET @my_role='master'");
	$ret = mysqlnd_ms_xa_rollback($link, $xa_id);
	printf("[021] rollback returned '%s', [%d/%s] '%s'\n", var_export($ret, true), $link->errno, $link->sqlstate, $link->error);
	$link->close();

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_xa_rollback_begin_fail.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_rollback_begin_fail.ini'.\n");
?>
--EXPECTF--
%A
Warning: mysqli::query(): (mysqlnd_ms) Failed to add participant and switch to XA_BEGIN state: %s in %s on line %d
[004] [2006] %s
[005] rollback returned 'true', [0/00000] ''
%A
Warning: mysqli::query(): (mysqlnd_ms) Failed to add participant and switch to XA_BEGIN state: %s in %s on line %d
[011] [2006] %s
%A
Warning: mysqli::query(): (mysqlnd_ms) Failed to add participant and switch to XA_BEGIN state: %s in %s on line %d
[012] [2006] %s
[013] rollback returned 'true', [0/00000] ''
%A
Warning: mysqli::query(): (mysqlnd_ms) Failed to add participant and switch to XA_BEGIN state: %s in %s on line %d
[019] [2006] %s
%A
Warning: mysqli::query(): (mysqlnd_ms) Failed to add participant and switch to XA_BEGIN state: %s in %s on line %d
[020] [2006] %s
[021] rollback returned 'true', [0/00000] ''
done!