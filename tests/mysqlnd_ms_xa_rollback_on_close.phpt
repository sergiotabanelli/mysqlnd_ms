--TEST--
Implicit rollback
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
	"default" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host)
	),
	"off" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array('rollback_on_close' => 0)
	),
	"on" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array('rollback_on_close' => 1)
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_xa_rollback_on_close.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_rollback_on_close.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");


	$xa_id = mt_rand(0, 500);
	mst_stats_diff(1, mysqlnd_ms_get_stats());

	if (!($link = mst_mysqli_connect("default", $user, $passwd, $db, $port, $socket)))
		printf("[002] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(3, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(4, $link, "CREATE TABLE test(id INT)", MYSQLND_MS_LAST_USED_SWITCH);

	mysqlnd_ms_xa_begin($link, $xa_id);
	mst_mysqli_query(5, $link, "INSERT INTO test(id) VALUES (5)", MYSQLND_MS_LAST_USED_SWITCH);
	$link->close();
	mst_stats_diff(6, mysqlnd_ms_get_stats());

	if (!($link = mst_mysqli_connect("on", $user, $passwd, $db, $port, $socket)))
		printf("[007] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(8, $link, "DELETE FROM test", MYSQLND_MS_SLAVE_SWITCH);
	mysqlnd_ms_xa_begin($link, $xa_id);
	mst_mysqli_query(9, $link, "INSERT INTO test(id) VALUES (5)", MYSQLND_MS_LAST_USED_SWITCH);
	$link = 123;
	mst_stats_diff(10, mysqlnd_ms_get_stats());

	if (!($link = mst_mysqli_connect("off", $user, $passwd, $db, $port, $socket)))
		printf("[011] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	/* Ensure the previous INSERT was not committed */
	$res = mst_mysqli_query(12, $link, "SELECT * FROM test");
	var_dump($res->fetch_all(MYSQLI_ASSOC));

	mst_mysqli_query(13, $link, "DELETE FROM test", MYSQLND_MS_SLAVE_SWITCH);
	mysqlnd_ms_xa_begin($link, $xa_id);
	mst_mysqli_query(14, $link, "INSERT INTO test(id) VALUES (5)", MYSQLND_MS_LAST_USED_SWITCH);
	unset($link);
	mst_stats_diff(15, mysqlnd_ms_get_stats());

	$links = array();
	for ($i = 0; $i < 100; $i++) {
		if (!($links[$i] = mst_mysqli_connect("on", $user, $passwd, $db, $port, $socket))) {
			printf("[016] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());
			break;
		}
	}

	foreach ($links as $xa_id => $link) {
		mysqlnd_ms_xa_begin($link, 501 + $xa_id);
		mst_mysqli_query("17-$xa_id", $link, "INSERT INTO test(id) VALUES (5)", MYSQLND_MS_SLAVE_SWITCH);
		$link->close();
	}
	mst_stats_diff(18, mysqlnd_ms_get_stats());

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_xa_rollback_on_close.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_rollback_on_close.ini'.\n");
?>
--EXPECTF--
[006] use_slave: 0 -> 1
[006] use_slave_sql_hint: 0 -> 1
[006] use_last_used_sql_hint: 0 -> 2
[006] lazy_connections_slave_success: 0 -> 1
[006] xa_begin: 0 -> 1
[006] xa_participants: 0 -> 1
[006] pool_masters_total: 0 -> 1
[006] pool_slaves_total: 0 -> 1
[006] pool_masters_active: 0 -> 1
[006] pool_slaves_active: 0 -> 1
[010] use_slave: 1 -> 2
[010] use_slave_sql_hint: 1 -> 2
[010] use_last_used_sql_hint: 2 -> 3
[010] lazy_connections_slave_success: 1 -> 2
[010] xa_begin: 1 -> 2
[010] xa_rollback_success: 0 -> 1
[010] xa_participants: 1 -> 2
[010] xa_rollback_on_close: 0 -> 1
[010] pool_masters_total: 1 -> 2
[010] pool_slaves_total: 1 -> 2
[010] pool_masters_active: 1 -> 2
[010] pool_slaves_active: 1 -> 2
array(0) {
}
[015] use_slave: 2 -> 4
[015] use_slave_guess: 0 -> 1
[015] use_slave_sql_hint: 2 -> 3
[015] use_last_used_sql_hint: 3 -> 4
[015] lazy_connections_slave_success: 2 -> 3
[015] xa_begin: 2 -> 3
[015] xa_participants: 2 -> 3
[015] pool_masters_total: 2 -> 3
[015] pool_slaves_total: 2 -> 3
[015] pool_masters_active: 2 -> 3
[015] pool_slaves_active: 2 -> 3
[018] use_slave: 4 -> 104
[018] use_slave_sql_hint: 3 -> 103
[018] lazy_connections_slave_success: 3 -> 103
[018] xa_begin: 3 -> 103
[018] xa_rollback_success: 1 -> 101
[018] xa_participants: 3 -> 103
[018] xa_rollback_on_close: 1 -> 101
[018] pool_masters_total: 3 -> 103
[018] pool_slaves_total: 3 -> 103
[018] pool_masters_active: 3 -> 103
[018] pool_slaves_active: 3 -> 103
done!