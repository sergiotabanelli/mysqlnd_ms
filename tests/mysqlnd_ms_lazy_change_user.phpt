--TEST--
lazy + change_user()
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

if ($db == 'mysql')
	die("SKIP Default test database must not be 'mysql', use 'test' or the like");

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
		'lazy_connections' => 1,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_lazy_change_user.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

function test_mysql_access($host, $user, $passwd, $db, $port, $socket) {

	if (!$link = mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket))
		die(sprintf("skip Cannot connect to %s (port '%d', socket '%s') [%d] %s", $host, $port, $socket, mysqli_connect_errno(), mysqli_connect_error()));

	return $link->select_db("mysql");
}

if (!test_mysql_access($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
	die("skip Master server account cannot access mysql database");

if (!test_mysql_access($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket))
	die("skip Slave server account cannot access mysql database");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_lazy_change_user.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	$master_thread = $link->thread_id;

	$link->change_user($user, $passwd, 'mysql');

	mst_mysqli_query(3, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);
	$slave_thread = $link->thread_id;

	$res = mst_mysqli_query(5, $link, "SELECT @myrole AS _role, DATABASE() as _database", MYSQLND_MS_SLAVE_SWITCH);
	if (!$row = $res->fetch_assoc())
		printf("[006] [%d] %s\n", $link->errno, $link->error);

	if ($row['_database'] != 'mysql')
		printf("[007] Expecting database 'mysql' got '%s'\n", $row['_database']);

	if ($row['_role'] != 'slave')
		printf("[008] Expecting role 'slave' got '%s'\n", $row['_role']);

	if ($link->thread_id != $slave_thread)
		printf("[009] Expecting slave connection thread id %d got %d\n", $slave_thread, $link->thread_id);

	$res = mst_mysqli_query(10, $link, "SELECT @myrole AS _role, DATABASE() as _database", MYSQLND_MS_MASTER_SWITCH);
	if (!$row = $res->fetch_assoc())
		printf("[011] [%d] %s\n", $link->errno, $link->error);

	if ($row['_database'] != 'mysql')
		printf("[012] Expecting database 'mysql' got '%s'\n", $row['_database']);

	if ($row['_role'] != '')
		printf("[013] Expecting role '' got '%s'\n", $row['_role']);

	if ($link->thread_id != $master_thread)
		printf("[014] Expecting master connection thread id %d got %d\n", $master_thread, $link->thread_id);

	$link->change_user($user, $passwd, $db);

	$res = mst_mysqli_query(15, $link, "SELECT @myrole AS _role, DATABASE() as _database");
	if (!$row = $res->fetch_assoc())
		printf("[016] [%d] %s\n", $link->errno, $link->error);

	if ($row['_database'] != $db)
		printf("[017] Expecting database '%s' got '%s'\n", $db, $row['_database']);

	if ($row['_role'] != '')
		printf("[018] Expecting role 'slave' got '%s'\n", $row['_role']);

	if ($link->thread_id != $slave_thread)
		printf("[019] Expecting slave connection thread id %d got %d\n", $slave_thread, $link->thread_id);

	$res = mst_mysqli_query(20, $link, "SELECT @myrole AS _role, DATABASE() as _database", MYSQLND_MS_MASTER_SWITCH);
	if (!$row = $res->fetch_assoc())
		printf("[021] [%d] %s\n", $link->errno, $link->error);

	if ($row['_database'] != $db)
		printf("[022] Expecting database '%s' got '%s'\n", $db, $row['_database']);

	if ($row['_role'] != '')
		printf("[023] Expecting role '' got '%s'\n", $row['_role']);

	if ($link->thread_id != $master_thread)
		printf("[024] Expecting master connection thread id %d got %d\n", $master_thread, $link->thread_id);


	print "done!";
?>
--CLEAN--
<?php
if (!unlink("test_mysqlnd_ms_lazy_change_user.ini"))
  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_lazy_change_user.ini'.\n");
?>
--EXPECTF--
done!