--TEST--
XA state store: no connection
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
if (($error = mst_mysqli_drop_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket,
		'my_mysqlnd_ms_xa_trx', 'my_mysqlnd_ms_xa_participants', 'my_mysqlnd_ms_xa_gc'))) {
	die(sprintf("SKIP 1 %s\n", $error));
}

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
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
					'global_trx_table' => 'my_mysqlnd_ms_xa_trx',
					'participant_table' => 'my_mysqlnd_ms_xa_participants',
					'garbage_collection_table' => 'my_mysqlnd_ms_xa_gc',
					'participant_localhost_ip' => 'pseudo_ip_for_localhost'
			))),
	),
);

if ($error = mst_create_config("test_mysqlnd_ms_xa_mysql_errors_tables.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_mysql_errors_tables.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$xa_id = mt_rand(0, 1000);

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (true !== mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[002] [%d] %s\n", $link->errno, $link->error);
	}

	mst_mysqli_query(3, $link, "SELECT 1");
	mst_mysqli_query(4, $link, "SET @myrole='master'");

	if (false !== ($tmp = mysqlnd_ms_xa_commit($link, $xa_id))) {
		printf("[005] Expecting false, got %s\n", var_export($tmp, true));
	}
	printf("[006] [%d] %s\n", $link->errno, $link->error);
	$link->close();

	if (($error = mst_mysqli_setup_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket,
				'my_mysqlnd_ms_xa_trx', 'my_mysqlnd_ms_xa_participants', 'my_mysqlnd_ms_xa_gc')) ||
		($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket,
				'my_mysqlnd_ms_xa_trx', 'my_mysqlnd_ms_xa_participants', 'my_mysqlnd_ms_xa_gc'))) {
		printf("[007] %s\n", $error);
	}

	$xa_id = mt_rand(0, 1000);
	if (!($link_store = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)))
		printf("[008] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[009] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (true !== mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[010] [%d] %s\n", $link->errno, $link->error);
	}

	mst_mysqli_query(11, $link, "SELECT 1");
	/* global table not used if action carried out on participant */
	mst_mysqli_query(12, $link_store, "DROP TABLE IF EXISTS my_mysqlnd_ms_xa_participants");
	mst_mysqli_query(13, $link, "SET @myrole='master'");

	/* participant table and global table used - gc table still references global table */
	mst_mysqli_query(15, $link_store, "DROP TABLE IF EXISTS my_mysqlnd_ms_xa_gc");

	/* now that all the references/children are gone, the global table can be dropped */
	mst_mysqli_query(16, $link_store, "DROP TABLE IF EXISTS my_mysqlnd_ms_xa_trx");

	if (false !== ($tmp = mysqlnd_ms_xa_commit($link, $xa_id))) {
		printf("[017] Expecting false, got %s\n", var_export($tmp, true));
	}

	printf("[018] [%d] %s\n", $link->errno, $link->error);
	$link->close();

	if (($error = mst_mysqli_setup_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket,
			'my_mysqlnd_ms_xa_trx', 'my_mysqlnd_ms_xa_participants', 'my_mysqlnd_ms_xa_gc')) ||
		($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket,
			'my_mysqlnd_ms_xa_trx', 'my_mysqlnd_ms_xa_participants', 'my_mysqlnd_ms_xa_gc'))) {
		printf("[019] %s\n", $error);
	}

	$xa_id = mt_rand(0, 1000);
	if (!($link_store = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)))
		printf("[020] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[021] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (true !== mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[022] [%d] %s\n", $link->errno, $link->error);
	}

	mst_mysqli_query(23, $link, "SELECT 1");
	/* global table not used if action carried out on participant */
	mst_mysqli_query(24, $link, "SET @myrole='master'");

	/* participant table and global table used */
	mst_mysqli_query(25, $link_store, "DROP TABLE IF EXISTS my_mysqlnd_ms_xa_participants");

	if (false !== ($tmp = mysqlnd_ms_xa_rollback($link, $xa_id))) {
		printf("[026] Expecting false, got %s\n", var_export($tmp, true));
	}
	printf("[027] [%d] %s\n", $link->errno, $link->error);
	$link->close();


	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!unlink("test_mysqlnd_ms_xa_mysql_errors_tables.ini")) {
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_store_mysql_errors_tables.ini'.\n");
	}

	if (($error = mst_mysqli_drop_xa_tables($emulated_master_host_only, $user, $passwd, $db,
	$emulated_master_port, $emulated_master_socket,
	"my_mysqlnd_ms_xa_trx", "my_mysqlnd_ms_xa_participants", "my_mysqlnd_ms_xa_gc"))) {
		printf("[clean] %s\n", $error);
	}
?>
--EXPECTF--

Warning: mysqlnd_ms_xa_begin(): (mysqlnd_ms) MySQL XA state store error: %s in %s on line %d
[002] [1146] (mysqlnd_ms) MySQL XA state store error: %s

Warning: mysqlnd_ms_xa_commit(): (mysqlnd_ms) There is no active XA transaction to commit in %s on line %d
[006] [2000] (mysqlnd_ms) There is no active XA transaction to commit

Warning: mysqli::query(): (mysqlnd_ms) MySQL XA state store error: %s in %s on line %d
[013] [1146] (mysqlnd_ms) MySQL XA state store error: %s

Warning: mysqlnd_ms_xa_commit(): (mysqlnd_ms) MySQL XA state store error: %s in %s on line %d

Warning: mysqlnd_ms_xa_commit(): (mysqlnd_ms) MySQL XA state store error: %s in %s on line %d

Warning: mysqlnd_ms_xa_commit(): (mysqlnd_ms) MySQL XA state store error: %s in %s on line %d
[018] [1146] (mysqlnd_ms) MySQL XA state store error: %s

Warning: mysqlnd_ms_xa_rollback(): (mysqlnd_ms) MySQL XA state store error: %s in %s on line %d

Warning: mysqlnd_ms_xa_rollback(): (mysqlnd_ms) MySQL XA state store error: %s in %s on line %d
[027] [1146] (mysqlnd_ms) MySQL XA state store error: %s
done!