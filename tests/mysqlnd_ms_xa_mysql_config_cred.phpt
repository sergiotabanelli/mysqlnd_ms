--TEST--
XA state store mysql: participant cred
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
	"cred_type" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),

		'xa' => array(
			'state_store' => array(
				'participant_localhost_ip' => '127.0.0.1',
				'record_participant_credentials' => array(false),
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
			))),
	),
	"cred_true" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			'state_store' => array(
				'participant_localhost_ip' => '127.0.0.1',
				'record_participant_credentials' => true,
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
			))),
	),
	"cred_false" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			'state_store' => array(
				'participant_localhost_ip' => '127.0.0.1',
				'record_participant_credentials' => 0,
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
			))),
	),

	"cred_default" => array(
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
					'global_trx_table' => 'mysqlnd_ms_xa_trx',
					'participant_table' => 'mysqlnd_ms_xa_participants',
			))),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_xa_mysql_config_cred.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_mysql_config_cred.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	set_error_handler('mst_error_handler');

	if (!($link = mst_mysqli_connect("cred_type", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$xa_id = mt_rand(0, 1000);
	var_dump(mysqlnd_ms_xa_begin($link, $xa_id));
	if ($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, 'mysqlnd_ms_xa_trx', 'mysqlnd_ms_xa_participants')) {
		printf("[002] %s\n", $error);
	}

	if (!($link = mst_mysqli_connect("cred_true", $user, $passwd, $db, $port, $socket)))
		printf("[003] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	var_dump(mysqlnd_ms_xa_begin($link, $xa_id));
	$res = mst_mysqli_query(4, $link, "SELECT 'cred_true'");
	var_dump($res->fetch_assoc());
	mst_mysqli_query(5, $link, "SET @myrole='master'");

	if (!($link_check = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
		printf("[006] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	$res = mst_mysqli_query(6, $link_check, sprintf("SELECT user, password FROM mysqlnd_ms_xa_participants"));
	if ($res->num_rows != 2) {
		printf("[007] Expecting two participants, found %d\n", $res->num_ros);
	}
	while ($row = $res->fetch_assoc()) {
		if ($row['user'] != $user) {
			printf("[008] Expecting user = '%s', got '%s'\n", $user, $row['user']);
		}
		if ($row['password'] != $passwd) {
			printf("[009] Expecting password = '%s', got '%s'\n", $passwd, $row['password']);
		}
	}

	if ($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, 'mysqlnd_ms_xa_trx', 'mysqlnd_ms_xa_participants')) {
		printf("[010] %s\n", $error);
	}

	if (!($link = mst_mysqli_connect("cred_false", $user, $passwd, $db, $port, $socket)))
		printf("[011] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	var_dump(mysqlnd_ms_xa_begin($link, $xa_id));
	$res = mst_mysqli_query(12, $link, "SELECT 'cred_false'");
	var_dump($res->fetch_assoc());
	mst_mysqli_query(13, $link, "SET @myrole='master'");

	$res = mst_mysqli_query(14, $link_check, sprintf("SELECT user, password FROM mysqlnd_ms_xa_participants"));
	if ($res->num_rows != 2) {
		printf("[015] Expecting two participants, found %d\n", $res->num_ros);
	}
	while ($row = $res->fetch_assoc()) {
		if ($row['user'] != "") {
			printf("[016] Expecting user = '%s', got '%s'\n", "", $row['user']);
		}
		if ($row['password'] != "") {
			printf("[017] Expecting password = '%s', got '%s'\n", "", $row['password']);
		}
	}
	if ($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, 'mysqlnd_ms_xa_trx', 'mysqlnd_ms_xa_participants')) {
		printf("[018] %s\n", $error);
	}

	if (!($link = mst_mysqli_connect("cred_default", $user, $passwd, $db, $port, $socket)))
		printf("[019] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	var_dump(mysqlnd_ms_xa_begin($link, $xa_id));
	$res = mst_mysqli_query(20, $link, "SELECT 'cred_default'");
	var_dump($res->fetch_assoc());
	mst_mysqli_query(21, $link, "SET @myrole='master'");

	$res = mst_mysqli_query(22, $link_check, sprintf("SELECT user, password FROM mysqlnd_ms_xa_participants "));
	if ($res->num_rows != 2) {
		printf("[023] Expecting two participants, found %d\n", $res->num_ros);
	}
	while ($row = $res->fetch_assoc()) {
		if ($row['user'] != "") {
			printf("[024] Expecting user = '%s', got '%s'\n", "", $row['user']);
		}
		if ($row['password'] != "") {
			printf("[025] Expecting password = '%s', got '%s'\n", "", $row['password']);
		}
	}

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!unlink("test_mysqlnd_ms_xa_mysql_config_cred.ini")) {
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_mysql_config_cred.ini'.\n");
	}

	if (($error = mst_mysqli_drop_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
		printf("[clean] %s\n", $error);
	}
?>
--EXPECTF--
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) 'record_participant_credentials' from 'state_store' must be a string in %s on line %d
bool(true)
bool(true)
array(1) {
  ["cred_true"]=>
  string(9) "cred_true"
}
bool(true)
array(1) {
  ["cred_false"]=>
  string(10) "cred_false"
}
bool(true)
array(1) {
  ["cred_default"]=>
  string(12) "cred_default"
}
done!