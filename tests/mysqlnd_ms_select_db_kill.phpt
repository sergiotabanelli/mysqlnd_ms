--TEST--
select_db() and kill()
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

if ($db == 'mysql')
	die("SKIP Test database must not be 'mysql', use 'test' or the like.");

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_select_db_kill.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

function test_mysql_access($host, $user, $passwd, $db, $port, $socket) {

	if (!$link = mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket))
		die(sprintf("SKIP Cannot connect, [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

	return $link->select_db("mysql");
}

if (!test_mysql_access($master_host_only, $user, $passwd, $db, $port, $socket))
	die("SKIP Master server account cannot access mysql database");

if (!test_mysql_access($slave_host_only, $user, $passwd, $db, $port, $socket))
	die("SKIP Slave server account cannot access mysql database");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_select_db_kill.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(3, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);
	$slave_thread_id = $link->thread_id;

	if (!$link->kill($link->thread_id))
		printf("[004] [%d] %s\n", $link->errno, $link->error);

	/* will change schema of all connections */
	if (!$link->select_db("mysql"))
		printf("[005] [%d] %s\n", $link->errno, $link->error);

	$res = mst_mysqli_query(6, $link, "SELECT @myrole AS _role, DATABASE() as _database");
	if ($res) {
		printf("[007] Who has run this? Slave thread id is '%d', thread id '%d'\n", $slave_thread_id, $link->thread_id);
		var_dump($res->fetch_assoc());
	}

	$res = mst_mysqli_query(8, $link, "SELECT @myrole AS _role, DATABASE() as _database", MYSQLND_MS_MASTER_SWITCH);
	if (!$row = $res->fetch_assoc())
		printf("[009] [%d] %s\n", $link->errno, $link->error);

	if ($row['_database'] != 'mysql')
		printf("[010] Expecting database 'mysql' got '%s'\n", $row['_database']);

	if ($row['_role'] != 'master')
		printf("[011] Expecting role 'master' got '%s'\n", $row['_role']);

	if (!$link->select_db($db))
		printf("[012] [%d] %s\n", $link->errno, $link->error);

	$res = mst_mysqli_query(13, $link, "SELECT @myrole AS _role, DATABASE() as _database", MYSQLND_MS_MASTER_SWITCH);
	if (!$row = $res->fetch_assoc())
		printf("[014] [%d] %s\n", $link->errno, $link->error);

	if ($row['_database'] != $db)
		printf("[015] Expecting database '%s' got '%s'\n", $db, $row['_database']);

	if ($row['_role'] != 'master')
		printf("[016] Expecting role 'master' got '%s'\n", $row['_role']);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_select_db_kill.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_select_db_kill.ini'.\n");
?>
--EXPECTF--
[006] [2006] %s
done!