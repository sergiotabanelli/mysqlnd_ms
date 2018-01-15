--TEST--
Random, user multi, no master
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);

$settings = array(
	"myapp" => array(
		'filters'	=> array(
			'user_multi' => array('callback' => 'pick_servers'),
			"random" => array()
		),
		'master' 	=> array($master_host),
		'slave' 	=> array($slave_host),
		'lazy_connection' => 1,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_random_user_multi_no_slave.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_random_user_multi_no_slave.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function pick_servers($connected_host, $query, $masters, $slaves, $last_used_connection, $in_transaction) {
		printf("pick_server('%s', '%s, '%s')\n", $connected_host, $query, $last_used_connection);
		/* array(master_array(master_idx, master_idx), slave_array(slave_idx, slave_idx)) */
		return array(array(0), array());
	}

	if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
		printf("[001] Cannot connect to the server using host=%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
			$host, $user, $db, $port, $socket);

	$res = mst_mysqli_query(2, $link, "SELECT 1 FROM DUAL");
	var_dump($res);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_random_user_multi_no_slave.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_random_user_multi_no_slave.ini'.\n");
?>
--EXPECTF--
pick_server('myapp', '/*2*/SELECT 1 FROM DUAL, '')

Warning: mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate slave connection. 0 slaves to choose from. Something is wrong in %s on line %d

Warning: mysqli::query(): (mysqlnd_ms) No connection selected by the last filter in %s on line %d
[002] [2000] (mysqlnd_ms) No connection selected by the last filter
bool(false)
done!