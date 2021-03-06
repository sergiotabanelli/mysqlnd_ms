--TEST--
XA state store mysql: commit
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
if ($error = mst_create_config("test_mysqlnd_ms_xa_mysql_commit.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_mysql_commit.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* Plain connect and commit followed by begin */
	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$xa_id = mt_rand(0, 1000);
	if (!mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[002] [%d] %s\n", $link->errno, $link->error);
	}
	mst_mysqli_query(3, $link, "SELECT 1");
	mst_mysqli_query(4, $link, "SET @myrole='master'");
	if (!mysqlnd_ms_xa_commit($link, $xa_id)) {
		printf("[005] [%d] %s\n", $link->errno, $link->error);
	}

	/* start another global trx with the same id - will this fool the store? */
	if (!mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[006] [%d] %s\n", $link->errno, $link->error);
	}
	mst_mysqli_query(7, $link, "SELECT 1");
	if (!mysqlnd_ms_xa_commit($link, $xa_id)) {
		printf("[008] [%d] %s\n", $link->errno, $link->error);
	}

	if (!mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[009] [%d] %s\n", $link->errno, $link->error);
	}

	mst_mysqli_query(10, $link, "SELECT 1");
	$link->kill($link->thread_id);
	if (!mysqlnd_ms_xa_commit($link, $xa_id)) {
		printf("[011] [%d] %s\n", $link->errno, $link->error);
	}

	if (!mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[012] [%d] %s\n", $link->errno, $link->error);
	}

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!unlink("test_mysqlnd_ms_xa_mysql_commit.ini")) {
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_mysql_commit.ini'.\n");
	}

	if (($error = mst_mysqli_drop_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
		printf("[clean] %s\n", $error);
	}
?>
--EXPECTF--
Warning: mysqlnd_ms_xa_commit(): (mysqlnd_ms) Failed to switch participant to XA_IDLE state: %s in %s on line %d
[011] [%d] (mysqlnd_ms) Failed to switch participant to XA_IDLE state: %s
done!