--TEST--
Thread id
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

/* Emulated ID does not work with replication */
include_once("util.inc");
$ret = mst_is_slave_of($emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket, $emulated_master_host_only, $emulated_master_port, $emulated_master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true == $ret)
	die("SKIP Configured emulated master and emulated slave could be part of a replication cluster\n");

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host, $emulated_slave_host),
		'pick' => array("roundrobin"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_info.ini", $settings))
	die(sprintf("SKIP %d\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[1,2]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_info.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$threads = array();

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (0 !== mysqli_connect_errno())
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(3, $link, "CREATE TABLE test(id INT)", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(4, $link, "INSERT INTO test(id) VALUES (1), (2), (3)", MYSQLND_MS_LAST_USED_SWITCH);

	$tmp = array('role' => 'master', 'info' => mysqli_info($link));
	$threads[mst_mysqli_get_emulated_id(5, $link)] = $tmp;

	mst_mysqli_query(6, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_LAST_USED_SWITCH);

	mst_mysqli_query(7, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(8, $link, "CREATE TABLE test(id INT)", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(9, $link, "INSERT INTO test(id) VALUES (1), (2), (3)", MYSQLND_MS_LAST_USED_SWITCH);

	$tmp = array('role' => 'slave 1', 'info' => mysqli_info($link));
	$threads[mst_mysqli_get_emulated_id(10, $link)] = $tmp;

	mst_mysqli_query(11, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_LAST_USED_SWITCH);

	mst_mysqli_query(12, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(13, $link, "CREATE TABLE test(id INT)", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(14, $link, "INSERT INTO test(id) VALUES (1), (2), (3)", MYSQLND_MS_LAST_USED_SWITCH);

	$tmp = array('role' => 'slave 2', 'info' => mysqli_info($link));
	$threads[mst_mysqli_get_emulated_id(15, $link)] = $tmp;

	mst_mysqli_query(16, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_LAST_USED_SWITCH);

	foreach ($threads as $thread_id => $info) {
		printf("%s - %s - '%s'\n", $thread_id, $info['role'], $info['info']);
		if ('' == $info['info'])
			printf("info should not be empty. Check manually.\n");
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_info.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_info.ini'.\n");
?>
--EXPECTF--
master-%d - master - '%s'
slave[1,2]-%d - slave 1 - '%s'
slave[1,2]-%d - slave 2 - '%s'
done!