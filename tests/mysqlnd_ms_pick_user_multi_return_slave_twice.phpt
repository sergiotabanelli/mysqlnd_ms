--TEST--
User multi, RR, return slave twice
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

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
		'slave' 	=> array($emulated_slave_host, $emulated_master_host),
		'lazy_connections' => 0,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_user_multi_return_slave_twice.ini", $settings))
	die(sprintf("SKIP %s\n", $error));


msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[1]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_user_multi_return_slave_twice.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
	set_error_handler('mst_error_handler');

	function pick_servers($connected_host, $query, $masters, $slaves, $last_used_connection, $in_transaction) {
		printf("pick_server('%s', '%s, '%s')\n", $connected_host, $query, $last_used_connection);
		/* array(master_array(master_idx, master_idx), slave_array(slave_idx, slave_idx)) */
		return array(array(0), array(0, 0, 1));
	}

	if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
		printf("[001] Cannot connect to the server using host=%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
			$host, $user, $db, $port, $socket);

	if ($res = mst_mysqli_query(2, $link, "SELECT 2 FROM DUAL")) {
		$last_server_id = mst_mysqli_get_emulated_id(3, $link);
		var_dump($last_server_id);
	}

	if ($res = mst_mysqli_query(4, $link, "SELECT 4 FROM DUAL")) {
		$server_id = mst_mysqli_get_emulated_id(5, $link);
		var_dump($server_id);
		printf("[006] Server changed: %s\n", ($server_id != $last_server_id) ? 'yes' : 'no');
		$last_server_id = $server_id;
	}

	if ($res = mst_mysqli_query(7, $link, "SELECT 7 FROM DUAL")) {
		$server_id = mst_mysqli_get_emulated_id(8, $link);
		var_dump($server_id);
		printf("[009] Server changed: %s\n", ($server_id != $last_server_id) ? 'yes' : 'no');
		$last_server_id = $server_id;
	}

	if ($res = mst_mysqli_query(10, $link, "SELECT 10 FROM DUAL")) {
		$server_id = mst_mysqli_get_emulated_id(11, $link);
		var_dump($server_id);
		printf("[012] Server changed: %s\n", ($server_id != $last_server_id) ? 'yes' : 'no');
		$last_server_id = $server_id;
	}

	if ($res = mst_mysqli_query(13, $link, "DROP TABLE IF EXISTS test"))
		var_dump(mst_mysqli_get_emulated_id(14, $link));

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_user_multi_return_slave_twice.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_user_multi_return_slave_twice.ini'.\n");
?>
--EXPECTF--
pick_server('myapp', '/*2*/SELECT 2 FROM DUAL, '')
pick_server('myapp', '/*ms=last_used*//*3*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles, '%s')
string(%d) "slave[1]-%d"
pick_server('myapp', '/*4*/SELECT 4 FROM DUAL, '%s')
pick_server('myapp', '/*ms=last_used*//*5*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles, '%s')
string(%d) "slave[1]-%d"
[006] Server changed: no
pick_server('myapp', '/*7*/SELECT 7 FROM DUAL, '%s')
pick_server('myapp', '/*ms=last_used*//*8*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles, '%s')
string(%d) "master-%d"
[009] Server changed: yes
pick_server('myapp', '/*10*/SELECT 10 FROM DUAL, '%s')
pick_server('myapp', '/*ms=last_used*//*11*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles, '%s')
string(%d) "slave[1]-%d"
[012] Server changed: yes
pick_server('myapp', '/*13*/DROP TABLE IF EXISTS test, '%s')
pick_server('myapp', '/*ms=last_used*//*14*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles, '%s')
string(%d) "master-%d"
done!
