--TEST--
User mutli filter
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
		'master' 	=> array('mymaster' => $master_host),
		'slave' 	=> array($slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_user_multi.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_user_multi.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function pick_servers($connected_host, $query, $masters, $slaves, $last_used_connection, $in_transaction) {
		printf("pick_server('%s', '%s, '%s')\n", $connected_host, $query, $last_used_connection);
		/* array(master_array(master_idx, master_idx), slave_array(slave_idx, slave_idx)) */
		return array(array(0), array(0));
	}

	if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
		printf("[001] Cannot connect to the server using host=%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
			$host, $user, $db, $port, $socket);

	$res = mst_mysqli_query(2, $link, "SELECT 1 FROM DUAL");
	var_dump($res->fetch_assoc());

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_user_multi.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_user_multi.ini'.\n");
?>
--EXPECTF--
pick_server('myapp', '/*2*/SELECT 1 FROM DUAL, '')
array(1) {
  [1]=>
  string(1) "1"
}
done!