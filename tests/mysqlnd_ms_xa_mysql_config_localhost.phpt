--TEST--
XA state store: record participant cred
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

if (!($link_check = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
	die(sprintf("[%d] %s\n", mysqli_connect_errno(), mysqli_connect_error()));
}
if (!$link_check->query("DROP TABLE IF EXISTS letmenotexist")) {
	die(sprintf("[%s] %s\n", $link->errno, $link->error));
}

$settings = array(
	"global_wrong" => array(
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
					'global_trx_table' => 'letmenotexist',
			))),
	),
	"global_type" => array(
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
					'global_trx_table' => array('letmenotexist'),
			))),
	),
	"global_empty" => array(
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
					'global_trx_table' => '',
			))),
	),
	"participants_wrong" => array(
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
					'participant_table' => 'letmenotexist',
			))),
	),
	"participants_type" => array(
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
					'participant_table' => array('letmenotexist'),
			))),
	),
	"participants_empty" => array(
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
					'participant_table' => '',
			))),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_xa_mysql_config_localhost.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_mysql_config_localhost.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	set_error_handler('mst_error_handler');

	if (!($link = mst_mysqli_connect("global_wrong", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$xa_id = mt_rand(0, 1000);
	var_dump(mysqlnd_ms_xa_begin($link, $xa_id));
	if ($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)) {
		printf("[002] %s\n", $error);
	}

	if (!($link = mst_mysqli_connect("global_type", $user, $passwd, $db, $port, $socket)))
		printf("[003] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	var_dump(mysqlnd_ms_xa_begin($link, $xa_id));
	if ($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)) {
		printf("[004] %s\n", $error);
	}

	if (!($link = mst_mysqli_connect("global_empty", $user, $passwd, $db, $port, $socket)))
		printf("[005] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	var_dump(mysqlnd_ms_xa_begin($link, $xa_id));
	if ($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)) {
		printf("[006] %s\n", $error);
	}

	if (!($link = mst_mysqli_connect("participants_wrong", $user, $passwd, $db, $port, $socket)))
		printf("[007] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	var_dump(mysqlnd_ms_xa_begin($link, $xa_id));
	mst_mysqli_query(8, $link, "SELECT 'participant_wrong'");
	if ($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)) {
		printf("[009] %s\n", $error);
	}

	if (!($link = mst_mysqli_connect("participants_type", $user, $passwd, $db, $port, $socket)))
		printf("[010] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	var_dump(mysqlnd_ms_xa_begin($link, $xa_id));
	mst_mysqli_query(11, $link, "SELECT 'participant_type'");
	if ($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)) {
		printf("[012] %s\n", $error);
	}

	if (!($link = mst_mysqli_connect("participants_empty", $user, $passwd, $db, $port, $socket)))
		printf("[013] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	var_dump(mysqlnd_ms_xa_begin($link, $xa_id));
	mst_mysqli_query(14, $link, "SELECT 'participant_empty'");
	if ($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)) {
		printf("[015] %s\n", $error);
	}

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!unlink("test_mysqlnd_ms_xa_mysql_config_localhost.ini")) {
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_mysql_config_localhost.ini'.\n");
	}

	if (($error = mst_mysqli_drop_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
		printf("[clean] %s\n", $error);
	}
?>
--EXPECTF--
[E_WARNING] mysqlnd_ms_xa_begin(): (mysqlnd_ms) MySQL XA state store error: %s in %s on line %d
bool(false)
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) 'global_trx_table' from 'xa' must be a string. Using default in %s on line %d
bool(true)
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) Empty string not allowed for 'global_trx_table' from 'xa'. Using default in %s on line %d
bool(true)
bool(true)
[E_WARNING] mysqli::query(): (mysqlnd_ms) MySQL XA state store error: %s in %s on line %d
[008] [1146] (mysqlnd_ms) MySQL XA state store error: %s
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) 'participant_table' from 'xa' must be a string. Using default in %s on line %d
bool(true)
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) Empty string not allowed for 'participant_table' from 'xa'. Using default in %s on line %d
bool(true)
done!