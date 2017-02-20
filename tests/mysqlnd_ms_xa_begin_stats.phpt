--TEST--
mysqlnd_ms_xa_begin() stats
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
if ($error = mst_create_config("test_mysqlnd_ms_xa_begin_stats.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_begin_stats.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	mst_stats_diff(1, mysqlnd_ms_get_stats());

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[002] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	/* plain vanialla */
	$xa_id = mt_rand(0, 1000);
	mysqlnd_ms_xa_begin($link, $xa_id);
	mst_stats_diff(3, mysqlnd_ms_get_stats());
	mysqlnd_ms_xa_commit($link, $xa_id);

	/* what happens to stats in case of an error: when do we increase stats? */
	mysqlnd_ms_xa_begin($link, $xa_id);
	@mysqlnd_ms_xa_begin($link, $xa_id);
	mst_stats_diff(4, mysqlnd_ms_get_stats());
	mysqlnd_ms_xa_commit($link, $xa_id);

	/* what happens to stats when param parsing fails? */
	@mysqlnd_ms_xa_begin($link, $link);
	mst_stats_diff(5, mysqlnd_ms_get_stats());

	/* Related: participants */
	mysqlnd_ms_xa_begin($link, $xa_id);
	mst_mysqli_query(6, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	mst_stats_diff(7, mysqlnd_ms_get_stats());
	mst_mysqli_query(8, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);
	mst_stats_diff(9, mysqlnd_ms_get_stats());
	/* note: participants++ no matter whether we end with commit or rollback */
	mysqlnd_ms_xa_rollback($link, $xa_id);
	mst_stats_diff(10, mysqlnd_ms_get_stats());

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_xa_begin_stats.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_begin_stats.ini'.\n");
?>
--EXPECTF--
[003] xa_begin: 0 -> 1
[003] pool_masters_total: 0 -> 1
[003] pool_slaves_total: 0 -> 1
[003] pool_masters_active: 0 -> 1
[003] pool_slaves_active: 0 -> 1
[004] xa_begin: 1 -> 2
[004] xa_commit_success: 0 -> 1
[005] xa_commit_success: 1 -> 2
[007] use_master: 0 -> 1
[007] use_master_sql_hint: 0 -> 1
[007] lazy_connections_master_success: 0 -> 1
[007] xa_begin: 2 -> 3
[007] xa_participants: 0 -> 1
[009] use_slave: 0 -> 1
[009] use_slave_sql_hint: 0 -> 1
[009] lazy_connections_slave_success: 0 -> 1
[009] xa_participants: 1 -> 2
[010] xa_rollback_success: 0 -> 1
done!