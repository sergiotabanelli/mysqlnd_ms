--TEST--
mysqlnd_ms_xa_commit() statis
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
if ($error = mst_create_config("test_mysqlnd_ms_xa_commit_stats.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_commit_stats.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$xa_id = mt_rand(0, 1000);
	mst_stats_diff(1, mysqlnd_ms_get_stats());

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[002] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	/* SUCCESS: no server involved */
	mst_stats_diff(3, mysqlnd_ms_get_stats());
	mysqlnd_ms_xa_begin($link, $xa_id);
	mst_stats_diff(4, mysqlnd_ms_get_stats());
	mysqlnd_ms_xa_commit($link, $xa_id);
	mst_stats_diff(5, mysqlnd_ms_get_stats());


	/* FAILURE stats - logical errors shall be ignored */
	if (!mysqlnd_ms_xa_begin($link, $xa_id) || !mysqlnd_ms_xa_commit($link, $xa_id + 1)) {
		printf("[006] [%d] '%s'\n", $link->errno, $link->error);
	}
	mst_stats_diff(7, mysqlnd_ms_get_stats());

	/* SUCCESS - matches begin */
	mysqlnd_ms_xa_commit($link, $xa_id);
	mst_stats_diff(8, mysqlnd_ms_get_stats());

	/* Failure: no global trx open */
	mysqlnd_ms_xa_commit($link, $xa_id);
	mst_stats_diff(9, mysqlnd_ms_get_stats());

	/* Hard to test any of the cases where a server fails... */
	mysqlnd_ms_xa_begin($link, $xa_id);
	mst_mysqli_query(10, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	@$link->kill($link->thread_id);
	mst_stats_diff(11, mysqlnd_ms_get_stats());
	mst_mysqli_query(12, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);
	mysqlnd_ms_xa_commit($link, $xa_id);
	mst_stats_diff(13, mysqlnd_ms_get_stats());

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_xa_commit_stats.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_commit_stats.ini'.\n");
?>
--EXPECTF--
[003] pool_masters_total: 0 -> 1
[003] pool_slaves_total: 0 -> 1
[003] pool_masters_active: 0 -> 1
[003] pool_slaves_active: 0 -> 1
[004] xa_begin: 0 -> 1
[005] xa_commit_success: 0 -> 1

Warning: mysqlnd_ms_xa_commit(): (mysqlnd_ms) The XA transaction id does not match the one of from XA begin in %s on line %d
[006] [2000] '(mysqlnd_ms) The XA transaction id does not match the one of from XA begin'
[007] xa_begin: 1 -> 2
[008] xa_commit_success: 1 -> 2

Warning: mysqlnd_ms_xa_commit(): (mysqlnd_ms) There is no active XA transaction to commit in %s on line %d
[011] use_master: 0 -> 1
[011] use_master_sql_hint: 0 -> 1
[011] lazy_connections_master_success: 0 -> 1
[011] xa_begin: 2 -> 3
[011] xa_participants: 0 -> 1

Warning: mysqlnd_ms_xa_commit(): (mysqlnd_ms) Failed to switch participant to XA_IDLE state: %s in %s on line %d
[013] use_slave: 0 -> 1
[013] use_slave_sql_hint: 0 -> 1
[013] lazy_connections_slave_success: 0 -> 1
[013] xa_commit_failure: 0 -> 1
[013] xa_participants: 1 -> 2
done!