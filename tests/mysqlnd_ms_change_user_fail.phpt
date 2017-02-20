--TEST--
change_user() - failure
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

if ($db == 'please_do_no_create_such_db')
	die("SKIP Default test database must not be 'please_do_no_create_such_db', use 'test' or the like");

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_change_user_fail.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

function test_unknown_access($host, $user, $passwd, $db, $port, $socket) {

	if (!$link = mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket))
		die(sprintf("skip Cannot connect to %s (port '%d', socket '%s') [%d] %s", $host, $port, $socket, mysqli_connect_errno(), mysqli_connect_error()));

	return $link->select_db("please_do_no_create_such_db");
}

if (test_unknown_access($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
	die("skip Master server account can access 'please_do_no_create_such_db' database");

if (test_unknown_access($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket))
	die("skip Slave server account can access 'please_do_no_create_such_db' database");

require_once("util.inc");
msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_change_user_fail.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	$master_id = mst_mysqli_get_emulated_id(3, $link);
	mst_mysqli_query(4, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);
	$slave_id = mst_mysqli_get_emulated_id(5, $link);

	if (!$link->change_user($user, $passwd, 'please_do_no_create_such_db'))
	  printf("[006] Failed to change user, [%d] %s\n", $link->errno, $link->error);


	if (!@$link->query("SELECT 1") && (2006 == $link->errno)) {
	  /* Server changed its behaviour sometime between 5.6.10 and 5.6.16.
	  A failed change user now shuts down the line. Upto 5.6.10 it just failed but
	  the connection was not killed. Can't do anything about it, ... ignore....
	  */
	  die("done!");
	}

	$res = mst_mysqli_query(7, $link, "SELECT @myrole AS _role, DATABASE() as _database");

	if (!$row = $res->fetch_assoc())
		printf("[008] [%d] %s\n", $link->errno, $link->error);

	if ($row['_database'] != $db)
		printf("[009] Expecting database '%s' got '%s'\n", $db, $row['_database']);

	/* NOTE: we must not check for SQL variables. They may or may not have been reset.
	Do not consider change_user() an atomic call which either works and resets all state
	information or fails and does not reset anything. */

	$server_id =  mst_mysqli_get_emulated_id(10, $link);
	if ($server_id != $slave_id)
		printf("[011] Expecting slave connection found '%s'\n", $server_id);

	$res = mst_mysqli_query(12, $link, "SELECT @myrole AS _role, DATABASE() as _database", MYSQLND_MS_MASTER_SWITCH);
	if (!$row = $res->fetch_assoc())
		printf("[013] [%d] %s\n", $link->errno, $link->error);

	if ($row['_database'] != $db)
		printf("[014] Expecting database '%s' got '%s'\n", $db, $row['_database']);

	$server_id =  mst_mysqli_get_emulated_id(15, $link);
	if ($server_id != $master_id)
		printf("[016] Expecting slave connection found '%s'\n", $server_id);

	print "done!";
?>
--CLEAN--
<?php
if (!unlink("test_mysqlnd_ms_change_user_fail.ini"))
	printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_change_user_fail.ini'.\n");
?>
--EXPECTF--
[006] Failed to change user, [%d] %s
done!