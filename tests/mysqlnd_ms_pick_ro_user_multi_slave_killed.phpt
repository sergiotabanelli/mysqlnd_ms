--TEST--
RO, user multi, slave killed
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);

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

include_once("util.inc");
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
		global $connections, $link;


		printf("pick_server('%s', '%s, '%s', %d, %d)\n", $connected_host, $query, $last_used_connection, count($masters), count($slaves));
		printf("-> call %d\n", ++$calls);
		$last_used = $last_used_connection;

		$servers = array(array(), array());
		foreach ($masters as $k => $master)
			$servers[0][] = $k;

		foreach ($slaves as $k => $slave)
			$servers[1][] = $k;

		foreach ($connections as $server_id => $thread_id) {
			if (!$link->kill($thread_id)) {
				printf("[002] [%d] %s\n", $link->errno, $link->error);
			}
		}

		printf("<- %d master, %d slaves\n", count($servers[0]), count($servers[1]));

		return $servers;
	}

	 $connections = array();

	if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
		printf("[001][%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if ($res = mst_mysqli_query(3, $link, "SELECT 3 FROM DUAL"))
		var_dump($res->fetch_assoc());


	 $server_id = mst_mysqli_get_emulated_id(4, $link);
	 $connections[$server_id] = $link->thread_id;


	if ($res = mst_mysqli_query(5, $link, "SELECT 5 FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH))
		var_dump($res->fetch_assoc());

	 $connections = array();
	 $server_id = mst_mysqli_get_emulated_id(6, $link);
	 $connections[$server_id] = $link->thread_id;

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
pick_server('myapp', '/*ms=last_used*//*4*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles, '%s', 1, 2)
-> call 2
<- 1 master, 2 slaves
pick_server('myapp', '/*ms=last_used*//*5*/SELECT 5 FROM DUAL, '%s', 1, 2)
-> call 3
<- 1 master, 2 slaves
[005] [2006] %s
pick_server('myapp', '/*ms=last_used*//*6*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles, '%s', 1, 2)
-> call 4
<- 1 master, 2 slaves
[006] [2006] %s
done!
