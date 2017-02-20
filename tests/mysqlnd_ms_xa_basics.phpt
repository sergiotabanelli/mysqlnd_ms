--TEST--
XA basics
--XFAIL--
Playground
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
		'xa' => array(
			'rollback_on_close' => 1
		),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_xa_basics.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_basics.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
	mst_stats_diff(15, mysqlnd_ms_get_stats());

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$xa_id = mt_rand(0, 1000);

	mysqlnd_ms_xa_begin($link, $xa_id);
	$link->query("SELECT 1");
	$link->kill($link->thread_id);
	$link->query("SET @my_role='master'");

	if (!mysqlnd_ms_xa_commit($link, $xa_id))
		printf("[002] [%d/%s] '%s'\n", $link->errno, $link->sqlstate, $link->error);

		die(":)");
		mst_stats_diff(15, mysqlnd_ms_get_stats());
	var_dump(mysqlnd_ms_xa_rollback($link, $xa_id));
	$link->query("SELECT 1");
	$link->query("SET @my_role='master'");
	/*
	if (!mysqlnd_ms_xa_commit($link, $xa_id))
		printf("[003] [%d/%s] '%s'\n", $link->errno, $link->sql_state, $link->error);
*/
	unset($link);
	mst_stats_diff(15, mysqlnd_ms_get_stats());
	print "done or gone!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_xa_basics.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_basics.ini'.\n");
?>
--EXPECTF--
done!
