--TEST--
Server GTID nowait Without memcached configuration
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
			"slave2" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket,
			),
		),

		'global_transaction_id_injection' => array(
		 	'type'						=> 5,
			'fetch_last_gtid'			=> $sql['fetch_last_gtid'],
			'report_error'				=> true,
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

if ($error = mst_create_config("test_mysqlnd_ms_gtid_nw_serverside_basics_nomemc.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
if ($error = mst_mysqli_drop_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
	die(sprintf("Failed to drop test table on master %s\n", $error));
if ($error = mst_mysqli_drop_test_table($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket))
	die(sprintf("Failed to drop test table on emulated slave %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_nw_serverside_basics_nomemc.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	$slave_link = mst_mysqli_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
	mst_mysqli_query(4, $link, "SET @myrole = 'Master'");
	$gtid = mysqlnd_ms_get_last_gtid($link);
	if ($gtid)
		printf("[4CHK] Expecting empty gtid got %s, [%d] %s\n", $gtid, $link->errno, $link->error);			

	mst_mysqli_query(5, $link, "SET @myrole = 'Slave1'", MYSQLND_MS_SLAVE_SWITCH);
	$gtid = mysqlnd_ms_get_last_gtid($link);
	if ($gtid)
		printf("[5CHK] Expecting empty gtid got %s, [%d] %s\n", $gtid, $link->errno, $link->error);	
		
	mst_mysqli_query(6, $link, "SET @myrole = 'Slave2'", MYSQLND_MS_SLAVE_SWITCH);
	$gtid = mysqlnd_ms_get_last_gtid($link);
	if ($gtid)
		printf("[6CHK] Expecting empty gtid got %s, [%d] %s\n", $gtid, $link->errno, $link->error);	
		
	$res = mst_mysqli_query(7, $link, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(8, $link, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());

	mst_mysqli_query(9, $link, "DROP TABLE IF EXISTS test"); 
	$gtid = mysqlnd_ms_get_last_gtid($link);
	if (!$gtid)
		printf("[9CHK] Expecting gtid got empty, [%d] %s\n", $link->errno, $link->error);	
	printf("[9CHK] GTID from get_last %s\n", $gtid);

	mst_mysqli_query(10, $link, "CREATE TABLE test(id INT) ENGINE=InnoDB");
	$prev_gtid = $gtid = mysqlnd_ms_get_last_gtid($link);
	if (!$gtid)
		printf("[10CHK] Expecting gtid got empty, [%d] %s\n", $gtid, $link->errno, $link->error);
	printf("[10CHK] GTID from get_last %s\n", $gtid);

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
	printf("[11CHK] GTID from get_last %s\n", $gtid);
	
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

	/* runs on slave1 and slave2 because the on_connect flag is not set*/
	$res = mst_mysqli_query(16, $link2, "SELECT id FROM test");
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(17, $link2, "SELECT id FROM test");
	
	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_nw_serverside_basics_nomemc.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_nw_serverside_basics_nomemc.ini'.\n");

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

Warning: mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong in /home/osboxes/mysqlnd_ms/tests/util.inc on line 124
[005] [2000] (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong

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
[9CHK] GTID from get_last %s
[10CHK] GTID from get_last %s
[11CHK] GTID from get_last %s
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
[017] [1146] Table 'test.test' doesn't exist
done!
