--TEST--
Round robin, weights
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
			"slave3" =>  array(
				'host' 	=> "lalala",
				'port' 	=> 0,
				'socket' => "/kein/anschluss/unter/dieser.nummer"
			),
		),
		'pick' => array('random' => array("weights" => array("slave1" => 8, "slave2" => 4, "slave3" => 1, "master1" => 3))),
		'failover' => array('strategy' => 'loop_before_master', 'max_retries' => 0),

	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_random_weight.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_random_weight.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$servers = array();
	for ($i = 0; $i < 100; $i++) {
		$row = @$link->query("SELECT CONNECTION_ID() AS _id FROM DUAL")->fetch_assoc();
		if (!isset($servers[$row['_id']])) {
			$servers[$row['_id']] = 1;
		} else {
			$servers[$row['_id']]++;
		}
	}

	arsort($servers);

	$last_used = 0;
	foreach ($servers as $conn_id => $used) {
		printf("%04d - %d\n", $conn_id, $used);
		if ($last_used && (($used * 1.2) > $last_used)) {
			printf("[002] Validate manually, could be false positive as it is random - last_used %d used %d\n", $last_used, $used);
		}
		$last_used = $used;
	}

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_random_weight.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_random_weight.ini'.\n");
?>
--EXPECTF--
%d - %d
%d - %d
done!