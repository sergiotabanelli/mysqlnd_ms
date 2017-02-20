--TEST--
Round robin, weights, master w failure
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
		'slave' => array(
			"master1" => array(
				'host' 	=> $emulated_master_host_only,
				'port'	=> (int)$emulated_master_port,
				'socket' => $emulated_master_socket
			),
		),
		'master' => array(
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
		'failover' => array('strategy' => 'loop_before_master', 'max_retries' => 0, ),

	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_random_weight_master_fail_loop.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_random_weight_master_fail_loop.ini
mysqlnd_ms.multi_master=1
mysqlnd_ms.disable_rw_split=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$servers = array();
	for ($i = 0; $i < 100; $i++) {
		$res = mst_mysqli_query($i + 2, $link, "SELECT CONNECTION_ID() AS _id FROM DUAL", NULL, true, false, false, true);
		$row = $res->fetch_assoc();
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
			printf("[002] Validate manually, could be falst positive as its random - last_used %d used %d\n", $last_used, $used);
		}
		$last_used = $used;
	}

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_random_weight_master_fail_loop.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_random_weight_master_fail_loop.ini'.\n");
?>
--EXPECTF--
%d - %d
%d - %d
done!