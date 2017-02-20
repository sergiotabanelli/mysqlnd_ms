--TEST--
RO, user multi, slave removed
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
			"random" => array('sticky' => '1'),
		),
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host, $emulated_master_host),
		'lazy_connections' => 0,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_ro_user_multi_slave_removed.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[1,2]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_ro_user_multi_slave_removed.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function pick_servers($connected_host, $query, $masters, $slaves, $last_used_connection, $in_transaction) {
		static $calls, $last_used;

		printf("pick_server('%s', '%s, '%s', %d, %d)\n", $connected_host, $query, $last_used_connection, count($masters), count($slaves));
		printf("-> call %d\n", ++$calls);

		if ($last_used && $last_used_connection && ($last_used == $last_used_connection))
			printf("[002] Last used connection has not changed\n");
		$last_used = $last_used_connection;

		$servers = array(array(), array());
		foreach ($masters as $k => $master)
			$servers[0][] = $k;

		foreach ($slaves as $k => $slave)
			if ($slave != $last_used_connection)
				$servers[1][] = $k;

		printf("<- %d master, %d slaves\n", count($servers[0]), count($servers[1]));

		return $servers;
	}


	if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
		printf("[001][%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if ($res = mst_mysqli_query(3, $link, "SELECT 3 FROM DUAL"))
		var_dump($res->fetch_assoc());

	if ($res = mst_mysqli_query(4, $link, "SELECT 4 FROM DUAL"))
		var_dump($res->fetch_assoc());

	if ($res = mst_mysqli_query(5, $link, "SELECT 5 FROM DUAL"))
		var_dump($res->fetch_assoc());

	if ($res = mst_mysqli_query(6, $link, "SELECT 6 FROM DUAL"))
		var_dump($res->fetch_assoc());

	$last_used = mst_mysqli_get_emulated_id(7, $link);
	if ($res = mst_mysqli_query(8, $link, "SELECT 8 FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH))
		var_dump($res->fetch_assoc());

	$server_id = mst_mysqli_get_emulated_id(9, $link);
	if ($server_id != $last_used)
		printf("[010] Server changed from %s to %s\n");

	$last_used = $server_id;

	if ($res = mst_mysqli_query(11, $link, "SELECT 8 FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH))
		var_dump($res->fetch_assoc());

	$server_id = mst_mysqli_get_emulated_id(12, $link);
	if ($server_id != $last_used)
		printf("[013] Server changed from %s to %s\n");


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_ro_user_multi_slave_removed.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_ro_user_multi_slave_removed.ini'.\n");
?>
--EXPECTF--
pick_server('myapp', '/*3*/SELECT 3 FROM DUAL, '', 1, 2)
-> call 1
<- 1 master, 2 slaves
array(1) {
  [3]=>
  string(1) "3"
}
pick_server('myapp', '/*4*/SELECT 4 FROM DUAL, '%s', 1, 2)
-> call 2
<- 1 master, 1 slaves
array(1) {
  [4]=>
  string(1) "4"
}
pick_server('myapp', '/*5*/SELECT 5 FROM DUAL, '%s', 1, 2)
-> call 3
<- 1 master, 1 slaves
array(1) {
  [5]=>
  string(1) "5"
}
pick_server('myapp', '/*6*/SELECT 6 FROM DUAL, '%s', 1, 2)
-> call 4
<- 1 master, 1 slaves
array(1) {
  [6]=>
  string(1) "6"
}
pick_server('myapp', '/*ms=last_used*//*7*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles, '%s', 1, 2)
-> call 5
<- 1 master, 1 slaves
pick_server('myapp', '/*ms=last_used*//*8*/SELECT 8 FROM DUAL, '%s', 1, 2)
-> call 6
[002] Last used connection has not changed
<- 1 master, 1 slaves
array(1) {
  [8]=>
  string(1) "8"
}
pick_server('myapp', '/*ms=last_used*//*9*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles, '%s', 1, 2)
-> call 7
[002] Last used connection has not changed
<- 1 master, 1 slaves
pick_server('myapp', '/*ms=last_used*//*11*/SELECT 8 FROM DUAL, '%s', 1, 2)
-> call 8
[002] Last used connection has not changed
<- 1 master, 1 slaves
array(1) {
  [8]=>
  string(1) "8"
}
pick_server('myapp', '/*ms=last_used*//*12*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles, '%s', 1, 2)
-> call 9
[002] Last used connection has not changed
<- 1 master, 1 slaves
done!