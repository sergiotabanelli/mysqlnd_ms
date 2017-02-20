--TEST--
Round robin, weights, more weights than servers
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

include_once("util.inc");

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 	=> $emulated_master_host_only,
				'port'	=> (int)$emulated_master_port,
				'socket' => $emulated_master_socket
			),
		),
		'slave' => array(
			"slave1" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket
			),
			"slave2" =>  array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket
			),
		),
		'pick' => array('random' => array("weights" => array("slave1" => -1, "slave2" => 65536, "master1" => 0))),
		'failover' => array('strategy' => 'loop_before_master', 'max_retries' => 0),

	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_random_weight_range.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_random_weight_range.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
	set_error_handler('mst_error_handler');

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_random_weight_range.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_random_weight_range.ini'.\n");
?>
--EXPECTF--
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) Invalid value '-1' for weight. Stopping in %s on line %d
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) Invalid value '65536' for weight. Stopping in %s on line %d
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) You must specify the load balancing weight for none or all configured servers. There is no default weight yet. Stopping in %s on line %d
done!