--TEST--
RR, user multi, no master, non-lazy
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
			"roundrobin" => array()
		),
		'master' 	=> array('mymaster' => $master_host),
		'slave' 	=> array($slave_host),
		'lazy_connections' => 0,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_rr_user_multi_no_master_non_lazy.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_rr_user_multi_no_master_non_lazy.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
	set_error_handler('mst_error_handler');

	function pick_servers($connected_host, $query, $masters, $slaves, $last_used_connection, $in_transaction) {
		printf("pick_server('%s', '%s, '%s')\n", $connected_host, $query, $last_used_connection);
		/* array(master_array(master_idx, master_idx), slave_array(slave_idx, slave_idx)) */
		return array(NULL, array(0));
	}

	if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
		printf("[001] Cannot connect to the server using host=%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
			$host, $user, $db, $port, $socket);

	$res = mst_mysqli_query(2, $link, "SELECT 1 FROM DUAL", MYSQLND_MS_MASTER_SWITCH);
	var_dump($res);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_rr_user_multi_no_master_non_lazy.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_rr_user_multi_no_master_non_lazy.ini'.\n");
?>
--EXPECTF--
pick_server('myapp', '/*ms=master*//*2*/SELECT 1 FROM DUAL, '')
[E_RECOVERABLE_ERROR] mysqli::query(): (mysqlnd_ms) User multi filter callback has returned an invalid list of servers to use. The callback must return an array in %s on line %d
[E_WARNING] mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong in %s on line %d
[002] [2000] (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong
bool(false)
done!