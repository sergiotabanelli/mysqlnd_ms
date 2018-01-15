--TEST--
Filter QOS, session consistency, NO Slave
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

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");

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
/*			"slave1" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket,
			),*/
		 ),

		'lazy_connections' => 0,

		'filters' => array(
			"quality_of_service" => array(
				"session_consistency" => 1,
			),
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos_session_consistency_no_slaves.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos_session_consistency_no_slaves.ini
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
	/* slave 1 */
	//mst_mysqli_query(4, $link, "SET @myrole='master 2'", MYSQLND_MS_SLAVE_SWITCH);

	$servers = array();

	/* master 1 */
	$role = fetch_role(6, $link, MYSQLND_MS_MASTER_SWITCH);
	$server_id = mst_mysqli_get_emulated_id(7, $link);
	if (isset($servers[$server_id][$role]))
		$servers[$server_id][$role] = $servers[$server_id][$role] + 1;
	else
		$servers[$server_id] = array($role => 1);

	/* master 1 */
	$role = fetch_role(8, $link, MYSQLND_MS_SLAVE_SWITCH);
	$server_id = mst_mysqli_get_emulated_id(9, $link);
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
	if (!unlink("test_mysqlnd_ms_filter_qos_session_consistency_no_slaves.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos_session_consistency_no_slaves.ini'.\n");
?>
--EXPECTF--
master 1 (master-%d) has run 2 queries
done!