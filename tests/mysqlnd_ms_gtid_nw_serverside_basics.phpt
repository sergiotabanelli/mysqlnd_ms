--TEST--
Server GTID nowait read consistency
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

if ($error = mst_mysqli_setup_gtid_memcached($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
  die(sprintf("SKIP Failed to setup GTID memcached on emulated master, %s\n", $error));

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
			"slave2" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket,
			),
		),

		'global_transaction_id_injection' => array(
		 	'type'						=> 2,
			'fetch_last_gtid'			=> $sql['fetch_last_gtid'],
			'report_error'				=> true,
			'memcached_host'			=> $emulated_master_host_only,
			'memcached_port'			=> $emulated_master_port + $memcached_port_add_hack,
			'memcached_key'				=> $sql['global_key'],
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

if ($error = mst_create_config("test_mysqlnd_ms_gtid_nw_serverside_basics.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
if ($error = mst_mysqli_drop_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
	die(sprintf("SKIP Failed to drop test table on master %s\n", $error));
if ($error = mst_mysqli_drop_test_table($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket))
	die(sprintf("SKIP Failed to drop test table on emulated slave %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_nw_serverside_basics.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	/* we need an extra non-MS link for checking memcached GTID. */
	$memc_link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
	$slave_link = mst_mysqli_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
	mst_mysqli_query(4, $link, "SET @myrole = 'Master'");
	$gtid = mysqlnd_ms_get_last_gtid($link);
	if ($gtid)
		printf("[4CHK] Expecting empty gtid got %s, [%d] %s\n", $gtid, $link->errno, $link->error);			
	$mgtid = mst_mysqli_fetch_gtid_memcached(0, $memc_link, $db, NULL, true);
	if ($mgtid)
		printf("[4CHK] Expecting empty gtid on memcached got %s\n", $mgtid[1]);	

	mst_mysqli_query(5, $link, "SET @myrole = 'Slave1'", MYSQLND_MS_SLAVE_SWITCH);
	$gtid = mysqlnd_ms_get_last_gtid($link);
	if ($gtid)
		printf("[5CHK] Expecting empty gtid got %s, [%d] %s\n", $gtid, $link->errno, $link->error);	
	$mgtid = mst_mysqli_fetch_gtid_memcached(0, $memc_link, $db, NULL, true);
	if ($mgtid)
		printf("[5CHK] Expecting empty gtid on memcached got %s\n", $mgtid[1]);	
		
	mst_mysqli_query(6, $link, "SET @myrole = 'Slave2'", MYSQLND_MS_SLAVE_SWITCH);
	$gtid = mysqlnd_ms_get_last_gtid($link);
	if ($gtid)
		printf("[6CHK] Expecting empty gtid got %s, [%d] %s\n", $gtid, $link->errno, $link->error);	
	$mgtid = mst_mysqli_fetch_gtid_memcached(0, $memc_link, $db, NULL, true);
	if ($mgtid)
		printf("[6CHK] Expecting empty gtid on memcached got %s\n", $mgtid[1]);	
		
	$res = mst_mysqli_query(7, $link, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(8, $link, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());

	mst_mysqli_query(9, $link, "DROP TABLE IF EXISTS test"); 
	$gtid = mysqlnd_ms_get_last_gtid($link);
	if (!$gtid)
		printf("[9CHK] Expecting gtid got empty, [%d] %s\n", $link->errno, $link->error);	
	$mgtid = mst_mysqli_fetch_gtid_memcached(0, $memc_link, $db, NULL, true);
	if (!$mgtid[1] || $mgtid[1] != $gtid)
		printf("[9CHK] Expecting gtid %s on memcached got %s\n", $gtid, $mgtid[1]);	
	printf("[9CHK] GTID from get_last %s GTID from memcached %s\n", $gtid, $mgtid[1]);

	mst_mysqli_query(10, $link, "CREATE TABLE test(id INT) ENGINE=InnoDB");
	$prev_gtid = $gtid = mysqlnd_ms_get_last_gtid($link);
	if (!$gtid)
		printf("[10CHK] Expecting gtid got empty, [%d] %s\n", $gtid, $link->errno, $link->error);
	$mgtid = mst_mysqli_fetch_gtid_memcached(0, $memc_link, $db, NULL, true);
	if (!$mgtid[1] || $mgtid[1] != $gtid)
		printf("[10CHK] Expecting gtid %s on memcached got %s\n", $gtid, $mgtid[1]);	
	printf("[10CHK] GTID from get_last %s GTID from memcached %s\n", $gtid, $mgtid[1]);

	if (!mst_mysqli_wait_gtid_memcached(0, $slave_link, $db, $gtid))
		printf("[0CHK] Timeout or gtid not replicated for %s, [%d] %s\n", $gtid, $slave_link->errno, $slave_link->error);	
	/* Add an executed gtid with different uuid to the slave to check executed gtid parsing code */ 
	$res = mst_mysqli_query(0, $slave_link, "INSERT INTO test(id) VALUES(2)");
	if (!$res)
		printf("[0CHK] Got error in insert on slave [%d] %s\n", $slave_link->errno, $slave_link->error);
		
	mst_mysqli_query(11, $link, "INSERT INTO test(id) VALUES(1)");
	$gtid = mysqlnd_ms_get_last_gtid($link);
	if (!$gtid || $gtid == $prev_gtid)
		printf("[11CHK] Expecting new gtid got the same or empty %s %s, [%d] %s\n", $prev_gtid, $gtid, $link->errno, $link->error);
	$mgtid = mst_mysqli_fetch_gtid_memcached(0, $memc_link, $db, NULL, true);
	if (!$mgtid[1] || $mgtid[1] != $gtid)
		printf("[11CHK] Expecting gtid %s on memcached got %s\n", $gtid, $mgtid);	
	printf("[11CHK] GTID from get_last %s GTID from memcached %s\n", $gtid, $mgtid[1]);
	
	if (!mst_mysqli_wait_gtid_memcached(12, $slave_link, $db, $gtid))
		printf("[12CHK] Timeout or gtid not replicated for %s, [%d] %s\n", $gtid, $slave_link->errno, $slave_link->error);	

	/* run on slave1 which has replicated the INSERT */
	$res = mst_mysqli_query(13, $link, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(14, $link, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());

	$link2 = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[000] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	$mgtid = mst_mysqli_fetch_gtid_memcached(15, $memc_link, $db, NULL, true);
	if (!$mgtid[1] || $mgtid[1] != $gtid)
		printf("[15CHK] Expecting gtid %s on memcached got %s\n", $gtid, $mgtid[1]);	

	/* runs on slave1 because with nowait the on_connect flag is implicit*/
	$res = mst_mysqli_query(16, $link2, "SELECT id FROM test");
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(17, $link2, "SELECT id FROM test");

	$memc_link->query("UNINSTALL PLUGIN daemon_memcached");
	
	if (mst_mysqli_query(18, $link, "DROP TABLE IF EXISTS test")) {
		printf("[18CHK] Expeting error 2000 got [%d] %s\n", $link->errno, $link->error);
	}
	
	$memc_link->query("INSTALL PLUGIN daemon_memcached soname 'libmemcached.so'");
	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_nw_serverside_basics.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_nw_serverside_basics.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_gtid_memcached($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n", $error);
	if ($error = mst_mysqli_drop_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
		printf("[clean] %s\n");
?>
--EXPECTF--
Warning: mysqlnd_ms_get_last_gtid(): (mysqlnd_ms) Fail or no ID has been injected yet in %s on line %d

Warning: mysqlnd_ms_get_last_gtid(): (mysqlnd_ms) Fail or no ID has been injected yet in %s on line %d

Warning: mysqlnd_ms_get_last_gtid(): (mysqlnd_ms) Fail or no ID has been injected yet in %s on line %d
array(1) {
  ["_role"]=>
  string(6) "Slave1"
}
array(1) {
  ["_role"]=>
  string(6) "Slave2"
}
[9CHK] GTID from get_last %s GTID from memcached %s
[10CHK] GTID from get_last %s GTID from memcached %s
[11CHK] GTID from get_last %s GTID from memcached %s
array(1) {
  ["_role"]=>
  string(6) "Slave1"
}
array(1) {
  ["_role"]=>
  string(6) "Slave1"
}
array(1) {
  ["id"]=>
  string(1) "2"
}

Warning: mysqli::query(): (mysqlnd_ms) Error setting memcached last write %s in %s on line %d

Warning: mysqli::query(): OK packet set GTID failed %s in %s on line %d

Warning: mysqli::query(): Error reading result set's header in %s on line %d
[018] [2000] Error on gtid_set_last_write
done!
