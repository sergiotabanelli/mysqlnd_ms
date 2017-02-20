--TEST--
XA state store mysql: host config
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
			)
		),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_xa_mysql_basics.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_mysql_basics.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$xa_id = mt_rand(0, 1000);
	var_dump(mysqlnd_ms_xa_begin($link, $xa_id));
	var_dump($link->query("SELECT 1")->fetch_assoc());
	printf("[%d/%s] '%s'\n", $link->errno, $link->sqlstate, $link->error);
	var_dump($link->query("SELECT @myrole='master'")->fetch_assoc());
	printf("[%d/%s] '%s'\n", $link->errno, $link->sqlstate, $link->error);
	var_dump(mysqlnd_ms_xa_rollback($link, $xa_id));

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!unlink("test_mysqlnd_ms_xa_mysql_basics.ini")) {
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_mysql_basics.ini'.\n");
	}

	if (($error = mst_mysqli_drop_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
		printf("[clean] %s\n", $error);
	}
?>
--EXPECTF--
bool(true)
array(1) {
  [1]=>
  string(1) "1"
}
[0/00000] ''
array(1) {
  ["@myrole='master'"]=>
  NULL
}
[0/00000] ''
bool(true)
done!