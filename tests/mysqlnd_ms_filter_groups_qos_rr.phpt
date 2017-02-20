--TEST--
Filter: node_groups, qos, rr
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $emulated_master_host_only,
				'port' 		=> (int)$emulated_master_port,
				'socket' 	=> $emulated_master_socket,
			),
			"master2" => array(
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
			"slave2" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket,
			),
		 ),

		'lazy_connections' => 0,
		'failover' => array('strategy' => 'loop_before_master'),
		'filters' => array(
			"node_groups" => array(
				"C" => array(
					'master' => array('master1'),
					'slave'	 => array('slave2', 'slave1'),
				),
			),
			"quality_of_service" => array(
				"strong_consistency" => 1,
			),
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_groups_qos_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.multi_master=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_groups_qos_rr.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function run_queries($offset, $link, $sql, $queries) {
		foreach ($queries as $query) {
			$res = mst_mysqli_query($offset, $link, $sql, (isset($query['switch'])) ? $query['switch'] : NULL);
			if ($res && ($row = $res->fetch_assoc()))  {
				if ($row['_role'] != $query['_role']) {
					printf("[%03d + 1] Expecting '%s' got '%s'\n", $offset, $query['_role'], $row['_role']);
				}
			} else {
				printf("[%03d + 2] [%d] %s, no result\n", $offset, $link->errno, $link->error);
			}
		}
	}

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	$masters = array();
	/* setup: mark all connections, masters first, we use round robin */
	mst_mysqli_query(2, $link, "SET @myrole='master1'");
	$masters[$link->thread_id] = "master1";

	mst_mysqli_query(3, $link, "SET @myrole='master2'");
	$masters[$link->thread_id] = "master2";

	/* groups filter not used */
	$sql = "SELECT @myrole AS _role";
	$queries = array(
		array("_role" => "master1", "switch" => MYSQLND_MS_MASTER_SWITCH),
		array("_role" => "master2", "switch" => MYSQLND_MS_MASTER_SWITCH),
		array("_role" => "master1"),
		array("_role" => "master2"),
		array("_role" => "master1"),
	);
	run_queries(10, $link, $sql, $queries);

	/* groups used */
	$sql = "/*C*/SELECT @myrole AS _role";
	$queries = array(
		array("_role" => "master1", "switch" => MYSQLND_MS_MASTER_SWITCH),
		array("_role" => "master1", "switch" => MYSQLND_MS_MASTER_SWITCH),
		array("_role" => "master1"),
		/* provoke error */
		array("_role" => "master1", "switch" => MYSQLND_MS_SLAVE_SWITCH),
	);
	run_queries(20, $link, $sql, $queries);


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_groups_qos_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_groups_qos_rr.ini'.\n");
?>
--EXPECTF--
done!