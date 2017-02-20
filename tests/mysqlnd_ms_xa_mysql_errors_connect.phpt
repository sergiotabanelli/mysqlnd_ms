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

_skipif_can_connect("letmebeunknown", $user, "nonono!", $db, $emulated_master_port, $emulated_master_socket);

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			'state_store' => array(
				'participant_localhost_ip' => '127.0.0.1',
				'mysql' =>
				array(
					'host' => "letmebeunknown",
					'user' => $user,
					'password' => "nonono!",
					'db'   => $db,
					'port' => $emulated_master_port,
					'socket' => $emulated_master_socket,
					'global_trx_table' => 'mysqlnd_ms_xa_trx',
					'participant_table' => 'mysqlnd_ms_xa_participants',
					'participant_localhost_ip' => 'pseudo_ip_for_localhost'
			))),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_xa_mysql_errors_connect.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_mysql_errors_connect.ini
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

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!unlink("test_mysqlnd_ms_xa_mysql_errors_connect.ini")) {
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_mysql_errors_connect.ini'.\n");
	}

	if (($error = mst_mysqli_drop_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
		printf("[clean] %s\n", $error);
	}
?>
--EXPECTF--

Warning: mysqlnd_ms_xa_begin(): php_network_getaddresses: getaddrinfo failed: Name or service not known in %s on line %d

Warning: mysqlnd_ms_xa_begin(): (mysqlnd_ms) MySQL XA state store error: php_network_getaddresses: getaddrinfo failed: Name or service not known in %s on line %d
[002] [2002] (mysqlnd_ms) MySQL XA state store error: php_network_getaddresses: getaddrinfo failed: Name or service not known

Warning: mysqlnd_ms_xa_commit(): (mysqlnd_ms) There is no active XA transaction to commit in %s on line %d
[006] [2000] (mysqlnd_ms) There is no active XA transaction to commit
done!