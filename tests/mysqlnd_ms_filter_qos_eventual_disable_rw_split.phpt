--TEST--
Filter QOS, eventual, no rw split
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $emulated_master_host_only,
				'port' 		=> (int)$emulated_master_port,
				'socket' 	=> $emulated_master_socket,
			),
		),
		'slave' => array(
			"slave1" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket,
			),
		 ),

		'lazy_connections' => 0,
		'filters' => array(
			"quality_of_service" => array(
				"eventual_consistency" => 1
			),
			'user' => array('callback' => 'pick_server')
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos_eventual_disable_rw_split.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

include_once("util.inc");
msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos_eventual_disable_rw_split.ini
mysqlnd_ms.disable_rw_split=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function pick_server($connected_host, $query, $master, $slaves, $last_used_connection, $in_transaction) {
		printf("pick_server('%s', '%s', %d, %d)\n", $connected_host, $query, count($master), count($slaves));
		return ($last_used_connection) ? $last_used_connection : $master[0];
	}

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	if (!$link->query("DROP TABLE IF EXISTS test") ||
		!$link->query("CREATE TABLE test(id INT)") ||
		!$link->query("INSERT INTO test(id) VALUES (1)"))
		printf("[002] [%d] %s\n", $link->errno, $link->error);

	$master_id = mst_mysqli_get_emulated_id(3, $link);


	if ($res = mst_mysqli_query(4, $link, "SELECT id FROM test"))
		var_dump($res->fetch_all());

	$server_id = mst_mysqli_get_emulated_id(5, $link);
	if ($server_id != $master_id)
		printf("[006] Query should have been executed on master because rw split is disabled\n");

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_qos_eventual_disable_rw_split.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos_eventual_disable_rw_split.ini'.\n");
?>
--EXPECTF--
pick_server('myapp', 'DROP TABLE IF EXISTS test', 1, 1)
pick_server('myapp', 'CREATE TABLE test(id INT)', 1, 1)
pick_server('myapp', 'INSERT INTO test(id) VALUES (1)', 1, 1)
pick_server('myapp', '/*ms=last_used*//*3*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles', 1, 1)
pick_server('myapp', '/*4*/SELECT id FROM test', 1, 1)
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
}
pick_server('myapp', '/*ms=last_used*//*5*//*util.inc*/SELECT role FROM _mysqlnd_ms_roles', 1, 1)
done!