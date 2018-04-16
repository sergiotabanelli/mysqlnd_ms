--TEST--
GTID nowait Write Consistency
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
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
		 	'type'						=> 2,
			'fetch_last_gtid'			=> $sql['fetch_last_gtid'],
			'report_error'				=> true,
			'memcached_host'			=> $emulated_master_host_only,
			'memcached_port'			=> $emulated_master_port + $memcached_port_add_hack,
			'memcached_key'				=> $sql['global_key'],
			'memcached_wkey'			=> $sql['global_wkey'],
			'race_avoid'				=> 1,
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

if ($error = mst_create_config("test_mysqlnd_ms_gtid_nw_wconsistency_basics.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
if ($error = mst_mysqli_setup_gtid_memcached($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
  	die(sprintf("Failed to setup GTID memcached on emulated master, %s\n", $error));
if ($error = mst_mysqli_create_gtid_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
	die(sprintf("Failed to create test table on master %s\n", $error));
if ($error = mst_mysqli_create_gtid_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
	die(sprintf("Failed to create test table on emulated master %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_nw_wconsistency_basics.ini
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
	$link2 = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
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

	mst_mysqli_query(9/*offset*/, $link, "INSERT INTO gtid_test(id) VALUES(@@server_uuid)");
	$master1_gtid = $gtid = mysqlnd_ms_get_last_gtid($link);
	if (!$gtid)
		printf("[".(string)10/*offset*/."] Expecting gtid got empty, [%d] %s\n", $link->errno, $link->error);	
	$rgtid = mst_mysqli_fetch_gtid_memcached(11/*offset*/, $memc_link, $db, $rwhere);
	$wgtid = mst_mysqli_fetch_wgtid_memcached(12/*offset*/, $memc_link, $db, $wwhere);
	if ($rgtid != $gtid || $wgtid[1] != $gtid)
		printf("[".(string)13/*offset*/."] Expecting gtid %s on memcached got %s %s\n", $gtid, $rgtid, $wgtid[1]);	
	$res = mst_mysqli_query(14/*offset*/, $link, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	
	if (!mst_mysqli_wait_gtid_memcached(15/*offset*/, $master2_link, $db, $gtid))
		printf("[".(string)16/*offset*/."] Timeout or gtid not replicated for %s, [%d] %s\n", $gtid, $master2_link->errno, $master2_link->error);	

	// The list of masters has now changed so the roundrobin filter will reset context, so we are again on first position and we get duplicate key error
	if (mst_mysqli_query(17/*offset*/, $link, "INSERT INTO gtid_test(id) VALUES(@@server_uuid)"))
		printf("[".(string)18/*offset*/."] Expecting error got [%d] %s\n", $link->errno, $link->error);		
	$gtid = mysqlnd_ms_get_last_gtid($link);
	if ($gtid != $master1_gtid)
		printf("[".(string)19/*offset*/."] Expecting unmodified gtid got the new %s %s, [%d] %s\n", $master1_gtid, $gtid, $link->errno, $link->error);
	$rgtid = mst_mysqli_fetch_gtid_memcached(20/*offset*/, $memc_link, $db, $rwhere);
	$wgtid = mst_mysqli_fetch_wgtid_memcached(21/*offset*/, $memc_link, $db, $wwhere); // wgtid should be empty
	if ($rgtid != $gtid ||!$wgtid[1])
		printf("[".(string)22/*offset*/."] Expecting gtid %s on memcached got %s %s\n", $gtid, $rgtid, $wgtid[1]);	
	$res = mst_mysqli_query(23/*offset*/, $link, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	

	mst_mysqli_query(24/*offset*/, $link, "INSERT INTO gtid_test(id) VALUES(@@server_uuid)");
	$master2_gtid = $gtid = mysqlnd_ms_get_last_gtid($link);
	if (!$gtid)
		printf("[".(string)25/*offset*/."] Expecting new gtid got empty %s, [%d] %s\n", $gtid, $link->errno, $link->error);
	$rgtid = mst_mysqli_fetch_gtid_memcached(26/*offset*/, $memc_link, $db, $rwhere);
	$wgtid = mst_mysqli_fetch_wgtid_memcached(27/*offset*/, $memc_link, $db, $wwhere);
	if ($rgtid != $gtid || $wgtid[1] != $gtid)
		printf("[".(string)28/*offset*/."] Expecting gtid %s on memcached got %s %s\n", $gtid, $rgtid, $wgtid[1]);	
	$res = mst_mysqli_query(29/*offset*/, $link, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());

	if (!mst_mysqli_wait_gtid_memcached(30/*offset*/, $master1_link, $db, $gtid))
		printf("[".(string)31/*offset*/."] Timeout or gtid not replicated for %s, [%d] %s\n", $gtid, $master1_link->errno, $master1_link->error);	
		
	if (mst_mysqli_query(32/*offset*/, $link, "INSERT INTO gtid_test(id) VALUES(@@server_uuid)"))
		printf("[".(string)33/*offset*/."] Expecting error got [%d] %s\n", $link->errno, $link->error);		
	$gtid = mysqlnd_ms_get_last_gtid($link);
	if ($gtid != $master1_gtid)
		printf("[".(string)34/*offset*/."] Expecting unmodified gtid got the new %s %s, [%d] %s\n", $master1_gtid, $gtid, $link->errno, $link->error);
	$rgtid = mst_mysqli_fetch_gtid_memcached(35/*offset*/, $memc_link, $db, $rwhere);
	$wgtid = mst_mysqli_fetch_wgtid_memcached(36/*offset*/, $memc_link, $db, $wwhere);
	if ($rgtid != $master2_gtid || !$wgtid[1]) // The last effective write was from master2
		printf("[".(string)37/*offset*/."] Expecting gtid %s on memcached got %s %s\n", $master2_gtid, $rgtid, $wgtid[1]);	
	$res = mst_mysqli_query(38/*offset*/, $link, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	
	mst_mysqli_query(39/*offset*/, $link2, "SET @myrole = 'Master1'");

	mst_mysqli_query(40/*offset*/, $link2, "SET @myrole = 'Master2'");

	mst_mysqli_query(41/*offset*/, $link2, "SET @myrole = CONCAT(@myrole,'-1')"); //write consistency exclude master3 and roundrobin restart from master1

	$res = mst_mysqli_query(42/*offset*/, $link2, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(43/*offset*/, $link2, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(44/*offset*/, $link2, "SELECT @myrole AS _role FROM DUAL"); // No read consistency gtid as been set so this will be master 3
	var_dump($res->fetch_assoc());

	print "Test running queue\n";
	mst_mysqli_query(45/*offset*/, $link2, "SET @myrole = CONCAT(@myrole,'-2')");
	$res = mst_mysqli_query(46/*offset*/, $link2, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	mst_mysqli_set_gtid_memcached(47/*offset*/, $memc_link, $db, '1', "id LIKE '%.%'"); //set running counter to 1
	print "Stick to master2\n";
	mst_mysqli_query(48/*offset*/, $link2, "SET @myrole = CONCAT(@myrole,'-2')");
	$res = mst_mysqli_query(49/*offset*/, $link2, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	mst_mysqli_query(50/*offset*/, $link, "SET @myrole = CONCAT(@myrole,'-2')");
	$res = mst_mysqli_query(51/*offset*/, $link, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	mst_mysqli_query(52/*offset*/, $link, "SET @myrole = CONCAT(@myrole,'-2')");
	$res = mst_mysqli_query(53/*offset*/, $link, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	mst_mysqli_set_gtid_memcached(54/*offset*/, $memc_link, $db, '0', "id LIKE '%.%'"); //set running counter to 0
	print "Roundrobin again\n";
	mst_mysqli_query(55/*offset*/, $link2, "SET @myrole = @myrole");
	$res = mst_mysqli_query(56/*offset*/, $link2, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	mst_mysqli_query(57/*offset*/, $link2, "SET @myrole = @myrole");
	$res = mst_mysqli_query(58/*offset*/, $link2, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	mst_mysqli_query(59/*offset*/, $link, "SET @myrole = @myrole");
	$res = mst_mysqli_query(60/*offset*/, $link, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	mst_mysqli_query(61/*offset*/, $link, "SET @myrole = @myrole");
	$res = mst_mysqli_query(62/*offset*/, $link, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	
			
	print "Test avoid race add error token\n";
	mst_mysqli_delete_gtid_memcached(63/*offset*/, $memc_link, $db, "id LIKE '%:%'"); // Delete all running gtids
	if (mst_mysqli_query(64/*offset*/, $link, "INSERT INTO gtid_test(id) VALUES(@@server_uuid)")) // We now should get warnings and error "Something wrong..." 
		printf("[".(string)65/*offset*/."] Expecting error got [%d] %s\n", $link->errno, $link->error);		
	$wgtid = mst_mysqli_fetch_wgtid_memcached(66/*offset*/, $memc_link, $db, $wwhere);
	if ($wgtid[0] != 'X')
		printf("[".(string)67/*offset*/."] Expecting gtid error marker 'X' got %s\n", $wgtid[0]);
	mst_mysqli_query(68/*offset*/, $link2, "SET @myrole = CONCAT(@myrole,'-1')"); //should get a warning and set to master 1
	$res = mst_mysqli_query(69/*offset*/, $link2, "SELECT @myrole AS _role FROM DUAL", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_nw_wconsistency_basics.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_nw_wconsistency_basics.ini'.\n");

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
[%s] [1062] Duplicate entry '%s' for key 'PRIMARY'
array(1) {
  ["_role"]=>
  string(7) "Master1"
}
array(1) {
  ["_role"]=>
  string(7) "Master2"
}
[%s] [1062] Duplicate entry '%s' for key 'PRIMARY'
array(1) {
  ["_role"]=>
  string(7) "Master1"
}
array(1) {
  ["_role"]=>
  string(9) "Master1-1"
}
array(1) {
  ["_role"]=>
  string(7) "Master2"
}
array(1) {
  ["_role"]=>
  NULL
}
Test running queue
array(1) {
  ["_role"]=>
  string(9) "Master2-2"
}
Stick to master2
array(1) {
  ["_role"]=>
  string(11) "Master2-2-2"
}
array(1) {
  ["_role"]=>
  string(9) "Master2-2"
}
array(1) {
  ["_role"]=>
  string(11) "Master2-2-2"
}
Roundrobin again
array(1) {
  ["_role"]=>
  string(9) "Master1-1"
}
array(1) {
  ["_role"]=>
  string(11) "Master2-2-2"
}
array(1) {
  ["_role"]=>
  string(11) "Master2-2-2"
}
array(1) {
  ["_role"]=>
  string(7) "Master1"
}
Test avoid race add error token

Warning: mysqli::query(): (mysqlnd_ms) Something wrong: previous key not found %s. Maybe you need to increase wait_for_wgtid_timeout in %s on line %d

Warning: mysqli::query(): (mysqlnd_ms) Something wrong no valid master or no valid write history for key %s: first (null) last (null) in %s on line %d

Warning: mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong in %s on line %d
[%s] [2000] (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong

Warning: mysqli::query(): (mysqlnd_ms)No valid write history, found error token (null) for key %s: fallback to read consistency rgtid (null) in %s on line %d
array(1) {
  ["_role"]=>
  string(11) "Master1-1-1"
}
done!

