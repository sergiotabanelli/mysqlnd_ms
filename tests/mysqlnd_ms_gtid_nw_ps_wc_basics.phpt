--TEST--
PS, autocommit, GTID nowait Write Consistency
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
		 	'type'						=> 5,
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

if ($error = mst_create_config("test_mysqlnd_ms_gtid_nw_ps_wc_basics.ini", $settings))
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
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_nw_ps_wc_basics.ini
mysqlnd_ms.collect_statistics=1
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
	require_once("connect.inc");
	require_once("util.inc");
 	$sql = mst_get_gtid_memcached($db);
    $rwhere = "m.id = '" . $sql['global_key'] . "'";
   	$wwhere = "m.id = '" . $sql['global_wkey'] . "'";
	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[".(string)2/*offset*/."] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	if (mysqli_connect_errno()) {
		printf("[".(string)3/*offset*/."] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	/* we need an extra non-MS link for checking memcached GTID. */
	$memc_link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
	$master1_link = mst_mysqli_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
	$master2_link = mst_mysqli_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
 //   $link = $master1_link;
	if (!($stmt = $link->prepare("SET @myrole = ?"))) // Prepared on master1
		printf("[".(string)4/*offset*/."] [%d] %s\n", $link->errno, $link->error);
	
	if (!($stmt2 = $link->prepare("/*".MYSQLND_MS_LAST_USED_SWITCH."*/SELECT @myrole AS _role FROM DUAL"))) // Prepared on master1
		printf("[".(string)5/*offset*/."] [%d] %s\n", $link->errno, $link->error);

	if (!($stmt3 = $link->prepare("INSERT INTO gtid_test(id) VALUES(CONCAT(@@server_uuid,'-',@myrole))"))) // Prepared on master2
		printf("[".(string)6/*offset*/."] [%d] %s\n", $link->errno, $link->error);

	if (!($stmt4 = $link->prepare("SELECT CONCAT(@myrole,'-',id) AS _ext_id FROM gtid_test"))) // Prepared on master3
		printf("[".(string)7/*offset*/."] [%d] %s\n", $link->errno, $link->error);

	$role = NULL;
	if (!$stmt2->bind_result($role)) 
		printf("[".(string)8/*offset*/."] [%d] %s\n", $link->errno, $link->error);

	$server_uid = NULL;
	if (!$stmt4->bind_result($server_uid)) 
		printf("[".(string)9/*offset*/."] [%d] %s\n", $link->errno, $link->error);

	if (!$stmt->bind_param('s',$master)) 
		printf("[".(string)10/*offset*/."] [%d] %s\n", $link->errno, $link->error);
	$master = 'Master1';
	if (!$stmt->execute()) // Execute on master1
		printf("[".(string)11/*offset*/."] [%d] %s\n", $link->errno, $link->error);
	if (!$stmt2->execute()) // Execute on master1
		printf("[".(string)12/*offset*/."] [%d] %s\n", $link->errno, $link->error);
	while ($stmt2->fetch()) 
		printf("Role %s\n", $role);
	
	$master = 'Master2';
	if (!$stmt->execute()) // Execute on master2
		printf("[".(string)13/*offset*/."] [%d] %s\n", $link->errno, $link->error);
	if (!$stmt2->execute()) // Execute on master2
		printf("[".(string)14/*offset*/."] [%d] %s\n", $link->errno, $link->error);
	while ($stmt2->fetch()) 
		printf("Role %s\n", $role);
		
	$master = 'Master3';
	if (!$stmt->execute()) // Execute on master3
		printf("[".(string)15/*offset*/."] [%d] %s\n", $link->errno, $link->error);
	if (!$stmt2->execute()) // Execute on master3
		printf("[".(string)16/*offset*/."] [%d] %s\n", $link->errno, $link->error);
	while ($stmt2->fetch()) 
		printf("Role %s\n", $role);

	$res = mst_mysqli_query(17/*offset*/, $link, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(18/*offset*/, $link, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(19/*offset*/, $link, "SELECT @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());
	
	if (!$stmt3->execute()) // Execute on master1
		printf("[".(string)20/*offset*/."] [%d] %s\n", $link->errno, $link->error);
	printf("Num_rows %d\n",  $stmt3->affected_rows);
	$gtid = mysqlnd_ms_get_last_gtid($link);
	if (!$gtid)
		printf("[".(string)21/*offset*/."] Expecting gtid got empty, [%d] %s\n", $link->errno, $link->error);	
	$rgtid = mst_mysqli_fetch_gtid_memcached(22/*offset*/, $memc_link, $db, $rwhere);
	$wgtid = mst_mysqli_fetch_wgtid_memcached(23/*offset*/, $memc_link, $db, $wwhere);
	if ($rgtid != $gtid || $wgtid[1] != $gtid)
		printf("[".(string)24/*offset*/."] Expecting gtid %s on memcached got %s %s\n", $gtid, $rgtid, $wgtid[1]);	
	if (!mst_mysqli_wait_gtid_memcached(25/*offset*/, $master2_link, $db, $gtid))
		printf("[".(string)26/*offset*/."] Timeout or gtid not replicated for %s, [%d] %s\n", $gtid, $master2_link->errno, $master2_link->error);	
		
	if (!$stmt4->execute()) // Execute on master1, The list of masters has now changed so the roundrobin filter will reset context and we are again on first position 
		printf("[".(string)27/*offset*/."] [%d] %s\n", $link->errno, $link->error);
	while ($stmt4->fetch()) {
        printf("Server uid %s\n", $server_uid);
    }
	
	if (!$stmt3->execute()) // Execute on master2
		printf("[".(string)28/*offset*/."] [%d] %s\n", $link->errno, $link->error);
	printf("Num_rows %d\n", $stmt3->affected_rows);
	$gtid = mysqlnd_ms_get_last_gtid($link);
	if (!$gtid)
		printf("[".(string)29/*offset*/."] Expecting gtid got empty, [%d] %s\n", $link->errno, $link->error);	
	$rgtid = mst_mysqli_fetch_gtid_memcached(30/*offset*/, $memc_link, $db, $rwhere);
	$wgtid = mst_mysqli_fetch_wgtid_memcached(31/*offset*/, $memc_link, $db, $wwhere);
	if ($rgtid != $gtid || $wgtid[1] != $gtid)
		printf("[".(string)32/*offset*/."] Expecting gtid %s on memcached got %s %s\n", $gtid, $rgtid, $wgtid[1]);	
	if (!mst_mysqli_wait_gtid_memcached(33/*offset*/, $master1_link, $db, $gtid))
		printf("[".(string)34/*offset*/."] Timeout or gtid not replicated for %s, [%d] %s\n", $gtid, $master1_link->errno, $master1_link->error);	
	
	if (!$stmt4->execute()) // Execute on master1
		printf("[".(string)35/*offset*/."] [%d] %s\n", $link->errno, $link->error);
	while ($stmt4->fetch()) {
        printf("Server uid %s\n", $server_uid);
    }

	$res = mst_mysqli_query(36/*offset*/, $memc_link, "SELECT id FROM gtid_test");
	var_dump($res->fetch_assoc());
	
	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_ps_autocommit_report_error.ini"))
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_nw_ps_wc_basics.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
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
Role Master1
Role Master2
Role Master3
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
Num_rows 1
Server uid Master1-%s-Master1
Num_rows 1
Server uid Master1-%s-Master1
Server uid Master1-%s-Master2
NULL
done!