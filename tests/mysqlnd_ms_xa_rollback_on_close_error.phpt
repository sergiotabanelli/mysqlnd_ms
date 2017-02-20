--TEST--
Implicit rollback, failing
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");
require_once("util.inc");

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP Emulated master and emulated slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

if (($error = mst_mysqli_setup_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket,'mysqlnd_ms_xa_trx', 'mysqlnd_ms_xa_participants')) ||
	($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, 'mysqlnd_ms_xa_trx', 'mysqlnd_ms_xa_participants'))) {
	die(sprintf("SKIP %s\n", $error));
}

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			'rollback_on_close' => 1,
			'state_store' =>
			array(
				'participant_localhost_ip' => '127.0.0.1',
				'mysql' =>
				array(
					'host' => $emulated_master_host_only,
					'user' => $user,
					'password' => $passwd,
					'db'   => $db,
					'port' => $emulated_master_port,
					'socket' => $emulated_master_socket,
					'global_trx_table' => 'mysqlnd_ms_xa_trx',
					'participant_table' => 'mysqlnd_ms_xa_participants',
					'participant_localhost_ip' => 'pseudo_ip_for_localhost'
			))),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_xa_rollback_on_close_error.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_rollback_on_close_error.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$xa_id = mt_rand(0, 1000);
	mst_stats_diff(1, mysqlnd_ms_get_stats());

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[002] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(3, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(4, $link, "CREATE TABLE test(id INT)", MYSQLND_MS_LAST_USED_SWITCH);

	mysqlnd_ms_xa_begin($link, $xa_id);
	mst_mysqli_query(5, $link, "INSERT INTO test(id) VALUES (5)", MYSQLND_MS_LAST_USED_SWITCH);
	mst_stats_diff(7, mysqlnd_ms_get_stats());

	if (($error = mst_mysqli_drop_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
		printf("[008] %s\n", $error);
	}
	mst_stats_diff(9, mysqlnd_ms_get_stats());

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!unlink("test_mysqlnd_ms_xa_rollback_on_close_error.ini")) {
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_rollback_on_close_error.ini'.\n");
	}

	if (($error = mst_mysqli_drop_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
		printf("[clean] %s\n", $error);
	}
?>
--EXPECTF--
[007] use_slave: 0 -> 1
[007] use_slave_sql_hint: 0 -> 1
[007] use_last_used_sql_hint: 0 -> 2
[007] lazy_connections_slave_success: 0 -> 1
[007] xa_begin: 0 -> 1
[007] xa_participants: 0 -> 1
[007] pool_masters_total: 0 -> 1
[007] pool_slaves_total: 0 -> 1
[007] pool_masters_active: 0 -> 1
[007] pool_slaves_active: 0 -> 1
done!
Warning: Unknown: (mysqlnd_ms) MySQL XA state store error: %s in Unknown on line 0

Warning: Unknown: (mysqlnd_ms) MySQL XA state store error: %s in Unknown on line 0

Warning: Unknown: (mysqlnd_ms) MySQL XA state store error: %s in Unknown on line 0
