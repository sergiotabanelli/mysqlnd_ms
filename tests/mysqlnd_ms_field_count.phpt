--TEST--
last query field count (without PS)
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_field_count.ini
--SKIPIF--
<?php
require_once("skipif.inc");
require_once("connect.inc");

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));

if (true == $ret)
	die("SKIP Configured emulated master and emulated slave could be part of a replication cluster\n");

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host, $emulated_slave_host),
		'pick' => array("roundrobin"),

	),
);
if ($error = mst_create_config("test_mysqlnd_ms_field_count.ini", $settings))
	die(sprintf("SKIP %s\n", $error));


_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

/* Emulated ID does not work with replication */
include_once("util.inc");
$ret = mst_is_slave_of($emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket, $emulated_master_host_only, $emulated_master_port, $emulated_master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

include_once("util.inc");
msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[1,2]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());


	$threads = array();
	mst_mysqli_query(2, $link, "SET @myrole='Slave 1'", MYSQLND_MS_SLAVE_SWITCH);
	if (0 !== $link->field_count)
		printf("[003] Expecting 0 got field_count = %d\n", $link->field_count);

	$res = mst_mysqli_query(4, $link, "SELECT @myrole AS _role", MYSQLND_MS_LAST_USED_SWITCH);
	$row = $res->fetch_assoc();
	if ($res->field_count != $link->field_count)
		printf("[005] res->field_count = %d, link->field_count = %d\n", $res->field_count, $link->field_count);
	$threads[mst_mysqli_get_emulated_id(6, $link)] = array("role" => $row['_role'], "fields" => $res->field_count);
	$res->close();

	mst_mysqli_query(7, $link, "SET @myrole='Master 1'");
	if (0 !== $link->field_count)
		printf("[008] Expecting 0 got field_count = %d\n", $link->field_count);

	$res = mst_mysqli_query(9, $link, "SELECT @myrole AS _role, 1 as _one", MYSQLND_MS_LAST_USED_SWITCH);
	$row = $res->fetch_assoc();
	if ($res->field_count != $link->field_count)
		printf("[010] res->field_count = %d, link->field_count = %d\n", $res->field_count, $link->field_count);
	$threads[mst_mysqli_get_emulated_id(11, $link)] = array("role" => $row['_role'], "fields" => $res->field_count);
	$res->close();


	mst_mysqli_query(12, $link, "SET @myrole='Slave 2'", MYSQLND_MS_SLAVE_SWITCH);
	if (0 !== $link->field_count)
		printf("[013] Expecting 0 got field_count = %d\n", $link->field_count);

	$res = mst_mysqli_query(14, $link, "SELECT @myrole AS _role, 1 AS _one, 2 as _two", MYSQLND_MS_LAST_USED_SWITCH);
	$row = $res->fetch_assoc();
	if ($res->field_count != $link->field_count)
		printf("[015] res->field_count = %d, link->field_count = %d\n", $res->field_count, $link->field_count);
	$threads[mst_mysqli_get_emulated_id(16, $link)] = array("role" => $row['_role'], "fields" => $res->field_count);
	$res->close();

	mst_mysqli_query(17, $link, "SET @myrole='Slave 1'", MYSQLND_MS_SLAVE_SWITCH);
	if (0 !== $link->field_count)
		printf("[018] Expecting 0 got field_count = %d\n", $link->field_count);

	foreach ($threads as $thread_id => $details)
		printf("%s - %s: %d\n", $thread_id, $details["role"], $details["fields"]);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_field_count.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_field_count.ini'.\n");
?>
--EXPECTF--
slave[1,2]-%d - Slave 1: 1
master-%d - Master 1: 2
slave[1,2]-%d - Slave 2: 3
done!