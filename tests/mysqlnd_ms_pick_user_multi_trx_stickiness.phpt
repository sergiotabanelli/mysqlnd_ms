--TEST--
User multi, RR, trx_stickiness
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

include_once("util.inc");
$ret = mst_is_slave_of($emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket, $emulated_master_host_only, $emulated_master_port, $emulated_master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true == $ret)
	die("SKIP Check config.inc notes! Configured emulated master and emulated slave could be part of a replication cluster\n");

$settings = array(
	"myapp" => array(
		'filters'	=> array(
			'user_multi' => array('callback' => 'pick_servers'),
			"roundrobin" => array()
		),
		'master' 	=> array('mymaster' => $emulated_master_host),
		'slave' 	=> array($emulated_slave_host),
		'lazy_connections' => 1,
		'trx_stickiness' => 'master',
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_user_multi_trx_stickiness.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[1]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_user_multi_trx_stickiness.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
	set_error_handler('mst_error_handler');

	function pick_servers($connected_host, $query, $masters, $slaves, $last_used_connection, $in_transaction) {
		printf("pick_server('%s', '%s, %d)\n", $connected_host, $query, $in_transaction);
		/* array(master_array(master_idx, master_idx), slave_array(slave_idx, slave_idx)) */
		return array(array(0), array(0));
	}

	if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
		printf("[001] Cannot connect to the server using host=%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
			$host, $user, $db, $port, $socket);

	if ($res = mst_mysqli_query(2, $link, "SELECT 2 FROM DUAL")) {
		printf("Server: %s\n", mst_mysqli_get_emulated_id(3, $link));
	}
	/* its only about in_transaction flag, we don't need to test all combinations */
	if (!$link->autocommit(false)) {
		printf("[003] [%d] %s\n", $link->errno, $link->error);
	}
	if ($res = mst_mysqli_query(4, $link, "SELECT 4 FROM DUAL")) {
		printf("Server: %s\n", mst_mysqli_get_emulated_id(5, $link));
	}
	if (!$link->commit()) {
		printf("[006] [%d] %s\n", $link->errno, $link->error);
	}
	if ($res = mst_mysqli_query(7, $link, "SELECT 7 FROM DUAL")) {
		printf("Server: %s\n", mst_mysqli_get_emulated_id(5, $link));
	}
	if (!$link->autocommit(true)) {
		printf("[008] [%d] %s\n", $link->errno, $link->error);
	}
	if ($res = mst_mysqli_query(9, $link, "SELECT 9 FROM DUAL")) {
		printf("Server: %s\n", mst_mysqli_get_emulated_id(10, $link));
	}
	if (!$link->rollback()) {
		printf("[011] [%d] %s\n", $link->errno, $link->error);
	}
	if ($res = mst_mysqli_query(12, $link, "SELECT 9 FROM DUAL")) {
		printf("Server: %s\n", mst_mysqli_get_emulated_id(13, $link));
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_user_multi_trx_stickiness.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_user_multi_trx_stickiness.ini'.\n");
?>
--EXPECTF--
pick_server('myapp', '/*2*/SELECT 2 FROM DUAL, 0)
pick_server('myapp', '/*ms=last_used*//*3*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles, 0)
Server: slave[1]-%d
pick_server('myapp', '/*4*/SELECT 4 FROM DUAL, 1)
pick_server('myapp', '/*ms=last_used*//*5*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles, 1)
Server: master-%d
pick_server('myapp', '/*7*/SELECT 7 FROM DUAL, 1)
pick_server('myapp', '/*ms=last_used*//*5*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles, 1)
Server: master-%d
pick_server('myapp', '/*9*/SELECT 9 FROM DUAL, 0)
pick_server('myapp', '/*ms=last_used*//*10*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles, 0)
Server: slave[1]-%d
pick_server('myapp', '/*12*/SELECT 9 FROM DUAL, 0)
pick_server('myapp', '/*ms=last_used*//*13*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles, 0)
Server: slave[1]-%d
done!