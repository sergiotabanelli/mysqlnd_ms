--TEST--
Round robin, weights, loop before master
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
$ret = mst_is_slave_of($emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket, $emulated_master_host_only, $emulated_master_port, $emulated_master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true == $ret)
	die("SKIP Configured emulated master and emulated slave could be part of a replication cluster\n");


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
			"slave3" => array(
				'host' 	=> 'kielerwoche',
				'port'	=> '123',
				'socket'=> 'free_concerts_but_no_free_beer',
			),
		),
		'pick' => array('roundrobin' => array("weights" => array("slave1" => 1, "slave2" => 2, "slave3" => 1, "master1" => 3))),
		'failover' => array('strategy' => 'loop_before_master', "remember_failed" => true, 'max_retries' => 0),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_round_robin_weight_loop.ini", $settings))
	die(sprintf("SKIP %s\n", $error));


msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[1,2]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master[1,2]");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_round_robin_weight_loop.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function monitor_connection_id($link, $hint = NULL) {
		static $calls = 0;
		static $last_id = NULL;
		static $usage = array();

		if ($hint) {
			$sql = sprintf("/*%s*/SELECT CONNECTION_ID() as _id FROM DUAL", $hint);
		} else {
			$sql = "SELECT CONNECTION_ID() AS _id FROM DUAL";
		}
		$row = $link->query($sql)->fetch_assoc();
		if (is_null($last_id)) {
			$last_id = $row['_id'];
		}
		if (!isset($usage[$row['_id']])) {
			$usage[$row['_id']] = 1;
		} else {
			$usage[$row['_id']]++;
		}
		printf("Call %d - %d - %d - ", ++$calls, $row['_id'], $usage[$row['_id']]);

		if ($row['_id'] == $last_id) {
			print " no change\n";
		} else {
			print " change\n";
		}
		$last_id = $row['_id'];
		return $last_id;
	}

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	monitor_connection_id($link);
	monitor_connection_id($link);
	monitor_connection_id($link);
	monitor_connection_id($link);
	monitor_connection_id($link);
	monitor_connection_id($link);
	monitor_connection_id($link);
	monitor_connection_id($link);
	monitor_connection_id($link);
	monitor_connection_id($link);
	monitor_connection_id($link);

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_round_robin_weight_loop.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_round_robin_weight_loop.ini'.\n");
?>
--EXPECTF--
Call 1 - %d - 1 -  no change
Call 2 - %d - 1 -  change
Call 3 - %d - 2 -  change

Warning: mysqli::query(): php_network_getaddresses: getaddrinfo failed: %s in %s on line %A
Call 4 - %d - 3 -  no change
Call 5 - %d - 4 -  no change
Call 6 - %d - 2 -  change
Call 7 - %d - 5 -  change
Call 8 - %d - 6 -  no change
Call 9 - %d - 7 -  no change
Call 10 - %d - 3 -  change
Call 11 - %d - 8 -  change
done!
