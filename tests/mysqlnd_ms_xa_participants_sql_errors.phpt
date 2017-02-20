--TEST--
XA participants SQL errors
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP Emulated master and emulated slave seem to the the same, see tests/README");
}
if (($slave_host == $emulated_slave_host)) {
	die("SKIP Slave and emulated slave seem to the the same, see tests/README");
}
if ($master_host == $emulated_master_host) {
	die("SKIP Master and emulated master seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host, $master_host),
		'slave' => array($emulated_slave_host, $slave_host),
		'filters' => array('roundrobin' => array()),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_xa_participants_sql_error.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_participants_sql_error.ini
mysqlnd_ms.multi_master=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* SQL errors in participants lines must be recognized and preserved properly */
	$xa_id = mt_rand(0, 1000);

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	/* single slave */
	if (true !== mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[002] [%d] %s\n", $link->errno, $link->error);
	}
	mst_mysqli_query(3, $link, "SELECT first_slave + troubles * really");
	if (true !== mysqlnd_ms_xa_rollback($link, $xa_id)) {
		printf("[004] [%d] %s\n", $link->errno, $link->error);
	}

	/* second slave */
	if (true !== mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[005] [%d] %s\n", $link->errno, $link->error);
	}

	var_dump(mst_mysqli_query(6, $link, "SELECT '6 - first slave' AS _msg")->fetch_assoc()['_msg']);
	$first = $link->thread_id;
	mst_mysqli_query(7, $link, "SELECT second_slave + troubles * really");
	var_dump(mst_mysqli_query(8, $link, "SELECT '8 - first slave' AS _msg")->fetch_assoc()['_msg']);
	if ($link->thread_id != $first) {
		printf("[009] Load balancing broken?\n");
	}
	if (true !== mysqlnd_ms_xa_commit($link, $xa_id)) {
		printf("[010] [%d] %s\n", $link->errno, $link->error);
	}

	/* first and second master: if our code worked for slaves, it must work for masters either */
	if (true !== mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[011] [%d] %s\n", $link->errno, $link->error);
	}
	mst_mysqli_query(12, $link, "SET @myrole='master1'");
	$first = $link->thread_id;
	mst_mysqli_query(13, $link, "SET @myrole='master2'");
	mst_mysqli_query(14, $link, "SET no_clue");
	if ($link->thread_id != $first) {
		printf("[015] Load balancing broken?\n");
	}
	var_dump(mst_mysqli_query(16, $link, "SELECT @myrole AS _msg", MYSQLND_MS_LAST_USED_SWITCH)->fetch_assoc()['_msg']);
	var_dump(mst_mysqli_query(17, $link, "SELECT @myrole AS _msg", MYSQLND_MS_MASTER_SWITCH)->fetch_assoc()['_msg']);

	/* just cover all of them... */
	mst_mysqli_query(18, $link, "SELECT first_slave");
	mst_mysqli_query(19, $link, "SELECT second_slave");
	mst_mysqli_query(20, $link, "SELECT first_master", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(21, $link, "SELECT second_master", MYSQLND_MS_MASTER_SWITCH);
	var_dump(mst_mysqli_query(22, $link, "SELECT 'first_slave' AS _msg")->fetch_assoc()['_msg']);
	if (true !== mysqlnd_ms_xa_commit($link, $xa_id)) {
		printf("[023] [%d] %s\n", $link->errno, $link->error);
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_xa_participants_sql_error.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_participants_sql_error.ini'.\n");
?>
--EXPECTF--
[003] [1054] %s
string(15) "6 - first slave"
[007] [1054] %s
string(15) "8 - first slave"
[014] [1193] %s
string(7) "master1"
string(7) "master2"
[018] [1054] %s
[019] [1054] %s
[020] [1054] %s
[021] [1054] %s
string(11) "first_slave"
done!