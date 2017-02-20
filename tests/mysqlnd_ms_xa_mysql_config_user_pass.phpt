--TEST--
XA state store mysql: user/passwd
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");
require_once("util.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_pass);

_skipif_can_connect($emulated_master_host_only, "letmebeinvalid", $passwd, $db, $emulated_master_port, $emulated_master_socket, "Testing invalid user");
_skipif_can_connect($emulated_master_host_only, $user, "letmebeinvalid", $db, $emulated_master_port, $emulated_master_socket, "Testing invalid passwd");

if (($error = mst_mysqli_setup_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)) ||
	($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
	die(sprintf("SKIP %s\n", $error));
}

$settings = array(
	"user_wrong" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			'state_store' => array(
				'participant_localhost_ip' => '127.0.0.1',
				'mysql' =>
				array(
					'host' => $emulated_master_host_only,
					'user' => "letmebeinvalid",
					'password' => $passwd,
					'db'   => $db,
					'port' => $emulated_master_port,
					'socket' => $emulated_master_socket,
			))),
	),
	"user_type" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			'state_store' => array(
				'participant_localhost_ip' => '127.0.0.1',
				'mysql' =>
				array(
					'host' => $emulated_master_host_only,
					'user' => array($user),
					'password' => $passwd,
					'db'   => $db,
					'port' => $emulated_master_port,
					'socket' => $emulated_master_socket,
			))),
	),

	"pass_wrong" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			'state_store' => array(
				'participant_localhost_ip' => '127.0.0.1',
				'mysql' =>
				array(
					'host' => $emulated_master_host_only,
					'user' => $user,
					'password' => "letmebeinvalid",
					'db'   => $db,
					'port' => $emulated_master_port,
					'socket' => $emulated_master_socket,
			))),
	),
	"pass_type" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			'state_store' => array(
				'participant_localhost_ip' => '127.0.0.1',
				'mysql' =>
				array(
					'host' => $emulated_master_host_only,
					'user' => $user,
					'password' => array($passwd),
					'db'   => $db,
					'port' => $emulated_master_port,
					'socket' => $emulated_master_socket,
			))),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_xa_mysql_config_user_pass.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_mysql_config_user_pass.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	set_error_handler('mst_error_handler');

	if (!($link = mst_mysqli_connect("user_wrong", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$xa_id = mt_rand(0, 1000);
	var_dump(mysqlnd_ms_xa_begin($link, $xa_id));
	if ($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)) {
		printf("[002] %s\n", $error);
	}

	if (!($link = mst_mysqli_connect("pass_wrong", $user, $passwd, $db, $port, $socket)))
		printf("[003] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	var_dump(mysqlnd_ms_xa_begin($link, $xa_id));
	if ($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)) {
		printf("[004] %s\n", $error);
	}

	if (!($link = mst_mysqli_connect("pass_type", $user, $passwd, $db, $port, $socket)))
		printf("[005] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	var_dump(mysqlnd_ms_xa_begin($link, $xa_id));
	if ($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)) {
		printf("[006] %s\n", $error);
	}

	if (!($link = mst_mysqli_connect("user_type", $user, $passwd, $db, $port, $socket)))
		printf("[007] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	var_dump(mysqlnd_ms_xa_begin($link, $xa_id));
	if ($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)) {
		printf("[008] %s\n", $error);
	}

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!unlink("test_mysqlnd_ms_xa_mysql_config_user_pass.ini")) {
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_mysql_config_user_pass.ini'.\n");
	}

	if (($error = mst_mysqli_drop_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
		printf("[clean] %s\n", $error);
	}
?>
--EXPECTF--
[E_WARNING] mysqlnd_ms_xa_begin(): (mysqlnd_ms) MySQL XA state store error: %s in %s on line %d
bool(false)
[E_WARNING] mysqlnd_ms_xa_begin(): (mysqlnd_ms) MySQL XA state store error: %s in %s on line %d
bool(false)
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) 'password' from 'xa' must be a string in %s on line %d
[E_WARNING] mysqlnd_ms_xa_begin(): (mysqlnd_ms) MySQL XA state store error: %s in %s on line %d
bool(false)
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) 'user' from 'xa' must be a string in %s on line %d
[E_WARNING] mysqlnd_ms_xa_begin(): (mysqlnd_ms) MySQL XA state store error:%s in %s on line %d
bool(false)
done!