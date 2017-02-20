--TEST--
select_db() and kill()
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

if ($db == 'pleasenot')
	die("SKIP Test database must not be 'pleasenot', use 'test' or the like.");

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_select_db_fail.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

$db = 'pleasenot';
function test_pleasenot_access($host, $user, $passwd, $db, $port, $socket) {

	if ($link = mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket))
		die(sprintf("SKIP Can connect to 'pleasenot', [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

	return;
}
test_pleasenot_access($master_host_only, $user, $passwd, $db, $port, $socket);
test_pleasenot_access($slave_host_only, $user, $passwd, $db, $port, $socket);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_select_db_fail.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(3, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);

	if (!$link->select_db("pleasenot"))
		printf("[005] [%d] %s\n", $link->errno, $link->error);

	$res = mst_mysqli_query(6, $link, "SELECT SELECT @myrole AS _role, DATABASE() as _database", MYSQLND_MS_SLAVE_SWITCH);

	if (!$link->select_db($db))
		printf("[007] [%d] %s\n", $link->errno, $link->error);

	$res = mst_mysqli_query(8, $link, "SELECT @myrole AS _role, DATABASE() as _database", MYSQLND_MS_MASTER_SWITCH);
	$row = $res->fetch_assoc();
	printf("%s - %s\n", $row['_role'], ($row['_database'] == $db) ? 'OK' : sprintf("Wrong DB - %s!", $row['_database']));

	$res = mst_mysqli_query(9, $link, "SELECT @myrole AS _role, DATABASE() as _database");
	$row = $res->fetch_assoc();
	printf("%s - %s\n", $row['_role'], ($row['_database'] == $db) ? 'OK' : sprintf("Wrong DB - %s!", $row['_database']));

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_select_db_fail.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_select_db_fail.ini'.\n");
?>
--EXPECTF--
[005] [%d] %spleasenot%s
[006] [%d] %s
master - OK
slave - OK
done!