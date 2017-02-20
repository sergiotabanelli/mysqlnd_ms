--TEST--
Filter QOS, eventual consistency
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

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
			"slave2" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket,
			),
		 ),

		'lazy_connections' => 0,
		'filters' => array(
			"quality_of_service" => array(
				"eventual_consistency" => 1,
			),
			"roundrobin" => array(),
		),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos_eventual_consistency_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

include_once("util.inc");
msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[1,2]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos_eventual_consistency_rr.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function fetch_role($offset, $link, $switch = NULL) {
		$query = 'SELECT @myrole AS _role';
		if ($switch)
			$query = sprintf("/*%s*/%s", $switch, $query);

		$res = mst_mysqli_query($offset, $link, $query, $switch);
		if (!$res) {
			printf("[%03d +01] [%d] [%s\n", $offset, $link->errno, $link->error);
			return NULL;
		}

		$row = $res->fetch_assoc();
		return $row['_role'];
	}

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* master */
	mst_mysqli_query(2, $link, "SET @myrole='master'");

	/* slave 1 */
	mst_mysqli_query(4, $link, "SET @myrole='slave 1'", MYSQLND_MS_SLAVE_SWITCH);

	/* slave 2 */
	mst_mysqli_query(6, $link, "SET @myrole='slave 2'", MYSQLND_MS_SLAVE_SWITCH);

	$servers = array();

	/* slave 1 */
	$role = fetch_role(8, $link);
	$server_id = mst_mysqli_get_emulated_id(9, $link);
	if (isset($servers[$server_id][$role]))
		$servers[$server_id][$role] = $servers[$server_id][$role] + 1;
	else
		$servers[$server_id] = array($role => 1);

	/* slave 2 */
	$role = fetch_role(10, $link);
	$server_id = mst_mysqli_get_emulated_id(11, $link);
	if (isset($servers[$server_id][$role]))
		$servers[$server_id][$role] = $servers[$server_id][$role] + 1;
	else
		$servers[$server_id] = array($role => 1);

	/* master */
	$role = fetch_role(12, $link, MYSQLND_MS_MASTER_SWITCH);
	$server_id = mst_mysqli_get_emulated_id(13, $link);
	if (isset($servers[$server_id][$role]))
		$servers[$server_id][$role] = $servers[$server_id][$role] + 1;
	else
		$servers[$server_id] = array($role => 1);

	if (false === ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_SESSION)))
		printf("[014] [%d] %s\n", $link->errno, $link->error);

	/* master */
	$role = fetch_role(16, $link);
	$server_id = mst_mysqli_get_emulated_id(17, $link);
	if (isset($servers[$server_id][$role]))
		$servers[$server_id][$role] = $servers[$server_id][$role] + 1;
	else
		$servers[$server_id] = array($role => 1);

	if (false === ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL)))
		printf("[018] [%d] %s\n", $link->errno, $link->error);

	/* slave 1 */
	$role = fetch_role(19, $link);
	$server_id = mst_mysqli_get_emulated_id(20, $link);
	if (isset($servers[$server_id][$role]))
		$servers[$server_id][$role] = $servers[$server_id][$role] + 1;
	else
		$servers[$server_id] = array($role => 1);

	foreach ($servers as $server_id => $roles) {
		foreach ($roles as $role => $num_queries) {
			printf("%s (%s) has run %d queries\n", $role, $server_id, $num_queries);
		}
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_qos_eventual_consistency_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos_eventual_consistency_rr.ini'.\n");
?>
--EXPECTF--
slave 1 (%s) has run 2 queries
slave 2 (%s) has run 1 queries
master (%s) has run 2 queries
done!
