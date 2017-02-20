--TEST--
mysqlnd_ms_xa_gc() @ mysql store
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

if (($error = mst_mysqli_setup_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)) ||
	($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
	die(sprintf("SKIP %s\n", $error));
}

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
		    'rollback_on_close' => 0,
			'state_store' => array(
				'participant_localhost_ip' => '127.0.0.1',
				'mysql' =>
				array(
					'host' => $emulated_master_host_only,
					'user' => $user,
					'password' => $passwd,
					'db'   => $db,
					'port' => $emulated_master_port,
					'socket' => $emulated_master_socket,
				)
			),
			"garbage_collection" => array(
				"max_retries" => 3,
				"probability" => 0,
				"max_transactions_per_run" => 100
			),
		),
	),
);

if ($error = mst_create_config("test_mysqlnd_ms_xa_mysql_gc_one.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_mysql_gc_one.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link_kill = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[002] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	/*
		Create a trx that needs to be rolled back by killing master
		connection prior to commit. This will leave garbage in the
		state store.
	*/
	$xa_id = mt_rand(0, 1000);

	if (true !== mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[003] [%d] %s\n", $link->errno, $link->error);
	}

	mst_mysqli_query(4, $link, "SELECT 1");
	mst_mysqli_query(5, $link, "SET @myrole='master'");

	mysqlnd_ms_xa_commit($link, $xa_id);
	$xa_id++;

	if (true !== mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[003] [%d] %s\n", $link->errno, $link->error);
	}

	mst_mysqli_query(4, $link, "SELECT 1");
	mst_mysqli_query(5, $link, "SET @myrole='master'");
	$thread_id = $link->thread_id;

	if (!$link_kill->kill($thread_id)) {
		printf("[006] [%d] %s\n", $link_kill->errno, $link_kill->error);
	}

	if (false != mysqlnd_ms_xa_commit($link, $xa_id)) {
		printf("[007] XA commit should have failed\n");
	}

		$xa_id++;

	if (true !== mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[008] [%d] %s\n", $link->errno, $link->error);
	}

	mysqlnd_ms_xa_commit($link, $xa_id);

	$xa_id++;
	if (true !== mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[009] [%d] %s\n", $link->errno, $link->error);
	}
	mysqlnd_ms_xa_rollback($link, $xa_id);

	var_dump(mysqlnd_ms_xa_gc($link, $xa_id));
	var_dump(mysqlnd_ms_xa_gc($link, $xa_id - 1));
	var_dump(mysqlnd_ms_xa_gc($link, $xa_id - 3));
	var_dump(mysqlnd_ms_xa_gc($link, $xa_id - 2));
	var_dump(mysqlnd_ms_xa_gc($link, $xa_id));

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!unlink("test_mysqlnd_ms_xa_mysql_gc_one.ini")) {
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_mysql_gc_one.ini'.\n");
	}

	if (($error = mst_mysqli_drop_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
		printf("[clean] %s\n", $error);
	}
?>
--EXPECTF--
Warning: %A
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
done!