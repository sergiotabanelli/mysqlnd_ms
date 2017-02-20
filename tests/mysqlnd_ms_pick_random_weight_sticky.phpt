--TEST--
Round robin, weights (sticky)
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
			"master2" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket
			),
		),
		'slave' => array(
			"slave1" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket
			),
			"slave2" => array(
				'host' 	=> $emulated_master_host_only,
				'port'	=> (int)$emulated_master_port,
				'socket' => $emulated_master_socket
			),
		),
		'pick' => array(
			'random' => array(
				"weights" => array(
					"slave1" => 2,
					"slave2" => 1,
					"master1" => 2,
					"master2" => 1
					),
				'sticky' => '1'
				)
			),
		'failover' => array('strategy' => 'loop_before_master', 'max_retries' => 0),

	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_random_weight_sticky.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master as second slave");

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_random_weight_sticky.ini
mysqlnd_ms.multi_master=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
	msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master as second slave");

	$slaves = array();
	for ($i = 0; $i < 100; $i++) {
		if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
			printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

		$link->query("SELECT 1");
		$id = mst_mysqli_get_emulated_id(2, $link, false);
		if (!isset($slaves[$id])) {
			$slaves[$id] = 1;
		} else {
			$slaves[$id]++;
		}

		$link->close();
	}

	arsort($slaves);

	$last_used = 0;
	foreach ($slaves as $conn_id => $used) {
		printf("Slave connection %s - %d\n", $conn_id, $used);
		if ($last_used && (($used * 1.2) > $last_used)) {
			printf("[003] Validate manually, could be false positive as it is random - last_used %d used %d\n", $last_used, $used);
		}
		$last_used = $used;
	}


	msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave as master");
	msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");

	$masters = array();
	for ($i = 0; $i < 100; $i++) {
		if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
			printf("[004] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

		$link->query("DROP TABLE IF EXISTS test");
		$id = mst_mysqli_get_emulated_id(5, $link, false);
		if (!isset($masters[$id])) {
			$masters[$id] = 1;
		} else {
			$masters[$id]++;
		}

		$link->close();
	}

	arsort($masters);

	$last_used = 0;
	foreach ($masters as $conn_id => $used) {
		printf("Master connection %s - %d\n", $conn_id, $used);
		if ($last_used && (($used * 1.2) > $last_used)) {
			printf("[006] Validate manually, could be false positive as it is random - last_used %d used %d\n", $last_used, $used);
		}
		$last_used = $used;
	}


	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_random_weight_sticky.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_random_weight_sticky.ini'.\n");
?>
--EXPECTF--
Slave connection slave - %d
Slave connection master as second slave - %d
Master connection master - %d
Master connection slave as master - %d
done!