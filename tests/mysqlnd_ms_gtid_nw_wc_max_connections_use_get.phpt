--TEST--
GTID nowait check connection load on real memcached server
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

if (!$real_memcached)
	die("SKIP We need a real memcached server\n");

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
		 	'type'						=> 5,
			'fetch_last_gtid'			=> $sql['fetch_last_gtid'],
			'report_error'				=> true,
			'memcached_host'			=> $memcached_host,
			'memcached_port'			=> $memcached_port,
			'memcached_key'				=> $sql['global_key'],
			'memcached_wkey'			=> $sql['global_wkey'],
			'use_get'					=> 1
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

if ($error = mst_create_config("test_mysqlnd_ms_gtid_nw_wc_max_connections.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
if ($error = mst_mysqli_create_gtid_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
	die(sprintf("Failed to create test table on master %s\n", $error));
if ($error = mst_mysqli_drop_gtid_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
	die(sprintf("Failed to drop test table on emulated master %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_nw_wc_max_connections.ini
mysqlnd_ms.multi_master=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
	$max_connections = 2048;
	for ($i=0; $i <= $max_connections; $i++) {
		$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
		$res = mst_mysqli_query($i+1, $link, "SET @myrole = 'Master1'");
		mst_mysqli_query($i+2, $link, "SET @myrole = 'Master2'");
		mst_mysqli_query($i+3, $link, "SET @myrole = 'Master3'");
		$res = mst_mysqli_query($i+4, $link, "SELECT @myrole AS _role FROM DUAL");
		$res = mst_mysqli_query($i+5, $link, "SELECT @myrole AS _role FROM DUAL");
		$res = mst_mysqli_query($i+6, $link, "SELECT @myrole AS _role FROM DUAL");
		usleep(500000);
	}	
	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_nw_wconsistency_concurrency.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_nw_wc_max_connections.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_gtid_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
		printf("[clean] %s\n", $error);
	if ($error = mst_mysqli_drop_gtid_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n", $error);
		
?>
--EXPECTF--
done!

