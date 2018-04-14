--TEST--
Server GTID, nowait with not responding memcached
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

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

$sql = mst_get_gtid_memcached($db);

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $master_host_only,
				'port' 		=> (int)$master_port,
				'socket' 	=> $master_socket,
			),
		),
		'slave' => array(
			"slave1" => array(
				'host' 	=> $slave_host_only,
				'port' 	=> (int)$slave_port,
				'socket' => $slave_socket,
			),
		),

		'global_transaction_id_injection' => array(
		 	'type'						=> 5,
			'fetch_last_gtid'			=> $sql['fetch_last_gtid'],
			'report_error'				=> true,
			'memcached_host'			=> 'realyunknown',
			'memcached_port'			=> $emulated_master_port + $memcached_port_add_hack,
			'memcached_key'				=> $sql['global_key'],
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

if ($error = mst_create_config("test_mysqlnd_ms_gtid_nw_serverside_nomemc.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
if ($error = mst_mysqli_drop_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
	die(sprintf("Failed to drop test table on master %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_nw_serverside_nomemc.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	$stats_before = mysqlnd_ms_get_stats();

	mst_mysqli_query(4, $link, "SET @myrole = 'Master'");
	mst_mysqli_query(5, $link, "SET @myrole = 'Slave'", MYSQLND_MS_SLAVE_SWITCH);

	if (mst_mysqli_query(6, $link, "DROP TABLE IF EXISTS test")) {
		printf("[6CHK] Expeting error 2000 got [%d] %s\n", $link->errno, $link->error);
	}
	if (mst_mysqli_query(7, $link, "CREATE TABLE test(id INT) ENGINE=InnoDB")) {
		printf("[7CHK] Expeting error 2014 got [%d] %s\n", $link->errno, $link->error);
	}
	
	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_nw_serverside_nomemc.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_nw_serverside_nomemc.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
		printf("[clean] %s\n");
?>
--EXPECTF--
Warning: mysqli::query(): (mysqlnd_ms) Something wrong could not get owned token gtid in %s on line %d

Warning: mysqli::query(): (mysqlnd_ms) Something wrong no valid selection in %s on line %d

Warning: mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong in %s on line %d
[004] [2000] (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong

Warning: mysqli::query(): (mysqlnd_ms) Something wrong could not get owned token gtid in %s on line %d

Warning: mysqli::query(): (mysqlnd_ms) Something wrong no valid selection in %s on line %d

Warning: mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong in %s on line %d
[005] [2000] (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong

Warning: mysqli::query(): (mysqlnd_ms) Error setting memcached last write %s in %s on line %d

Warning: mysqli::query(): OK packet set GTID failed %s in %s on line %d

Warning: mysqli::query(): Error reading result set's header in %s on line %d
[006] [2000] Error on gtid_set_last_write
[007] [2014] Commands out of sync; you can't run this command now
done!
