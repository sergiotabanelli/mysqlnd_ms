--TEST--
Filter QOS, session consistency, MM + RR
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

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

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave1");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master[1,2]");


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
		 ),

		'lazy_connections' => 0,
		'failover' => array('strategy' => 'master'),

		'filters' => array(
			"quality_of_service" => array(
				"session_consistency" => 1,
			),
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos_session_consistency_mm_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));


?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos_session_consistency_mm_rr.ini
mysqlnd_ms.multi_master=1
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

	/* master 1 */
	mst_mysqli_query(2, $link, "SET @myrole='master 1'");
	/* master 2 */
	mst_mysqli_query(4, $link, "SET @myrole='master 2'");

	$servers = array();

	/* master 1 */
	$role = fetch_role(6, $link, MYSQLND_MS_MASTER_SWITCH);
	$server_id = mst_mysqli_get_emulated_id(7, $link);
	if (isset($servers[$server_id][$role]))
		$servers[$server_id][$role] = $servers[$server_id][$role] + 1;
	else
		$servers[$server_id] = array($role => 1);

	/* master 2 */
	$role = fetch_role(8, $link, MYSQLND_MS_MASTER_SWITCH);
	$server_id = mst_mysqli_get_emulated_id(9, $link);
	if (isset($servers[$server_id][$role]))
		$servers[$server_id][$role] = $servers[$server_id][$role] + 1;
	else
		$servers[$server_id] = array($role => 1);

	/* master 1 */
	$role = fetch_role(10, $link);
	$server_id = mst_mysqli_get_emulated_id(11, $link);
	if (isset($servers[$server_id][$role]))
		$servers[$server_id][$role] = $servers[$server_id][$role] + 1;
	else
		$servers[$server_id] = array($role => 1);

	/* master 2 */
	$role = fetch_role(12, $link);
	$server_id = mst_mysqli_get_emulated_id(13, $link);
	if (isset($servers[$server_id][$role]))
		$servers[$server_id][$role] = $servers[$server_id][$role] + 1;
	else
		$servers[$server_id] = array($role => 1);

	/* master 2 */
	$role = fetch_role(14, $link, MYSQLND_MS_LAST_USED_SWITCH);
	$server_id = mst_mysqli_get_emulated_id(15, $link);
	if (isset($servers[$server_id][$role]))
		$servers[$server_id][$role] = $servers[$server_id][$role] + 1;
	else
		$servers[$server_id] = array($role => 1);

	if (false === ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL)))
		printf("[016] [%d] %s\n", $link->errno, $link->error);

	/* slave 1 */
	$role = fetch_role(16, $link);
	$server_id = mst_mysqli_get_emulated_id(17, $link);
	if (isset($servers[$server_id][$role]))
		$servers[$server_id][$role] = $servers[$server_id][$role] + 1;
	else
		$servers[$server_id] = array($role => 1);

	/* master 1 */
	$role = fetch_role(18, $link, MYSQLND_MS_MASTER_SWITCH);
	$server_id = mst_mysqli_get_emulated_id(19, $link);
	if (isset($servers[$server_id][$role]))
		$servers[$server_id][$role] = $servers[$server_id][$role] + 1;
	else
		$servers[$server_id] = array($role => 1);

	if (false === ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_SESSION)))
		printf("[020] [%d] %s\n", $link->errno, $link->error);

	/* master 2 */
	$role = fetch_role(22, $link, MYSQLND_MS_MASTER_SWITCH);
	$server_id = mst_mysqli_get_emulated_id(23, $link);
	if (isset($servers[$server_id][$role]))
		$servers[$server_id][$role] = $servers[$server_id][$role] + 1;
	else
		$servers[$server_id] = array($role => 1);

	/* master 1 */
	mst_mysqli_query(24, $link, "DROP TABLE IF EXISTS test");
	$role = fetch_role(27, $link, MYSQLND_MS_LAST_USED_SWITCH);

	$server_id = mst_mysqli_get_emulated_id(28, $link);
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
	if (!unlink("test_mysqlnd_ms_filter_qos_session_consistency_mm_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos_session_consistency_mm_rr.ini'.\n");
?>
--EXPECTF--
master 1 (master[1,2]-%d) has run 4 queries
master 2 (master[1,2]-%d) has run 4 queries
 (slave1-%d) has run 1 queries
done!