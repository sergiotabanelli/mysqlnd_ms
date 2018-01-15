--TEST--
GTID Write Consistency concurrency
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_check_extensions(array("pcntl"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);


include_once("util.inc");
$ret = mst_mysqli_server_supports_gtid($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
if (is_string($ret))
	die(sprintf("SKIP Failed to check if server has built-in GTID support, %s\n", $ret));

if (true != $ret)
	die(sprintf("SKIP Server has no built-in GTID support (want MySQL 5.6.16+)"));

$ret = mst_mysqli_server_supports_session_track_gtid($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
if (is_string($ret))
	die(sprintf("SKIP Failed to check if server support SESSION TRACK GTID, %s\n", $ret));

if (true != $ret)
	die(sprintf("SKIP Server has no SESSION TRACK GTID support (want MySQL 5.7.6+ and SESSION_TRACK_GTIDS=OWN_GTID)"));

$ret = mst_mysqli_server_supports_memcached_plugin($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
if (is_string($ret))
	die(sprintf("SKIP Failed to check if server support MEMCACHED plugin, %s\n", $ret));

if (true != $ret)
	die(sprintf("SKIP Server has no MEMCACHED plugin support (want MySQL 5.6.0+ and active daemon_memcached plugin)"));

$ret = mst_is_slave_of($slave_host_only, $slave_port, $slave_socket, $master_host_only, $master_port, $master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true != $ret)
	die("SKIP Configured master and slave seem not to be part of a replication cluster\n");

$ret = mst_is_slave_of($master_host_only, $master_port, $master_socket, $slave_host_only, $slave_port, $slave_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured multi master, %s\n", $ret));

if (true != $ret)
	die("SKIP Configured masters seem not to be part of a circular replication cluster\n");

$sql = mst_get_gtid_memcached($db);

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $master_host_only,
				'port' 		=> (int)$master_port,
				'socket' 	=> $master_socket,
			),
			"master2" => array(
				'host' 	=> $slave_host_only, // will be used as master
				'port' 	=> (int)$slave_port,
				'socket' => $slave_socket,
			),			
			"master3" => array(
				'host' 	=> $emulated_master_host_only,
				'port' 	=> (int)$emulated_master_port,
				'socket' => $emulated_master_socket,
			),			
		),
		'slave' => array(),
		'global_transaction_id_injection' => array(
		 	'type'						=> 3,
			'fetch_last_gtid'			=> $sql['fetch_last_gtid'],
			'report_error'				=> true,
			'memcached_host'			=> $emulated_master_host_only,
			'memcached_port'			=> $emulated_master_port + $memcached_port_add_hack,
			'memcached_key'				=> $sql['global_key'],
			'memcached_wkey'			=> $sql['global_wkey'],
			),

		'lazy_connections' => 1,
		'trx_stickiness' => 'on',
		'filters' => array(
			"quality_of_service" => array(
				"session_consistency" => 1,
			),
			"roundrobin" => array(),
		),
	),

);

if ($error = mst_create_config("test_mysqlnd_ms_gtid_wconsistency_concurrency.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
if ($error = mst_mysqli_setup_gtid_memcached($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
  	die(sprintf("Failed to setup GTID memcached on emulated master, %s\n", $error));
if ($error = mst_mysqli_create_gtid_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
	die(sprintf("Failed to create test table on master %s\n", $error));
if ($error = mst_mysqli_drop_gtid_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
	die(sprintf("Failed to drop test table on emulated master %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_wconsistency_concurrency.ini
mysqlnd_ms.multi_master=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
 	$sql = mst_get_gtid_memcached($db);
    $rwhere = "m.id = '" . $sql['global_key'] . "'";
   	$wwhere = "m.id = '" . $sql['global_wkey'] . "'";
	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[".(string)1/*offset*/."] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	$link1 = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[".(string)2/*offset*/."] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	/* we need an extra non-MS link for checking memcached GTID. */
	$memc_link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
	$master1_link = mst_mysqli_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
	$master2_link = mst_mysqli_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

	mst_mysqli_query(3/*offset*/, $link, "SET @myrole = 'Master1'");
	mst_mysqli_query(4/*offset*/, $link, "SET @myrole = 'Master2'");
	mst_mysqli_query(5/*offset*/, $link, "SET @myrole = 'Master3'");

	$res = mst_mysqli_query(6/*offset*/, $link, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(7/*offset*/, $link, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(8/*offset*/, $link, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());

	mst_mysqli_query(9/*offset*/, $link1, "SET @myrole = 'Master1'");
	mst_mysqli_query(10/*offset*/, $link1, "SET @myrole = 'Master2'");
	mst_mysqli_query(11/*offset*/, $link1, "SET @myrole = 'Master3'");

	$res = mst_mysqli_query(12/*offset*/, $link1, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(13/*offset*/, $link1, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(14/*offset*/, $link1, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());
	/* go on to master 2 */
	mst_mysqli_query(15/*offset*/, $link1, "SET @myrole = @myrole");
	mst_mysqli_query(16/*offset*/, $link1, "SET @myrole = @myrole");
	$res = mst_mysqli_query(17/*offset*/, $link1, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	/* go on to master 1 */
	mst_mysqli_query(18/*offset*/, $link, "SET @myrole = @myrole");
	$res = mst_mysqli_query(19/*offset*/, $link, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	$pid = mst_fork_gtid_lock_set(20/*offset*/, $memc_link, $link1);
	var_dump($pid);
	mst_fork_gtid_release(21/*offset*/, $memc_link, $link, $pid);
	
	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_wconsistency_concurrency.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_wconsistency_concurrency.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_gtid_memcached($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n", $error);
	if ($error = mst_mysqli_drop_gtid_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
		printf("[clean] %s\n", $error);
	if ($error = mst_mysqli_drop_gtid_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n", $error);
		
?>
--EXPECTF--
array(1) {
  ["_role"]=>
  string(7) "Master1"
}
array(1) {
  ["_role"]=>
  string(7) "Master2"
}
array(1) {
  ["_role"]=>
  string(7) "Master3"
}
array(1) {
  ["_role"]=>
  string(7) "Master1"
}
array(1) {
  ["_role"]=>
  string(7) "Master2"
}
array(1) {
  ["_role"]=>
  string(7) "Master3"
}
array(1) {
  ["_role"]=>
  string(7) "Master2"
}
array(1) {
  ["_role"]=>
  string(7) "Master1"
}
array(1) {
  ["_role"]=>
  string(12) "norole-lp1:1"
}
int(%d)
array(1) {
  ["_role"]=>
  string(11) "Master3-lp1"
}
array(1) {
  ["_role"]=>
  string(18) "norole-lp1:1-rp1:1"
}
array(1) {
  ["_role"]=>
  string(13) "Master3-lc1:1"
}
done!

