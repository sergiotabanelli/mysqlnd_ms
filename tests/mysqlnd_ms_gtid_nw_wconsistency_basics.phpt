--TEST--
GTID nowait Write Consistency (TOBE REWRITTEN)
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
			'race_avoid'				=> 3,
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
    $rwhere = "m.id = '" . $sql['global_key'] . ":0'";
   	$wwhere = "m.id = '" . $sql['global_wkey'] . "'";
	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[".(string)1/*offset*/."] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	$link2 = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[".(string)2/*offset*/."] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	$link3 = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[".(string)3/*offset*/."] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	/* we need an extra non-MS link for checking memcached GTID. */
	$memc_link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
	$master1_link = mst_mysqli_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
	$master2_link = mst_mysqli_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
	
	mst_mysqli_query(4/*offset*/, $link, "SET @myrole = 'Master1'"); //Execute on master1

	mst_mysqli_query(5/*offset*/, $link, "SET @myrole = 'Master2'"); //Execute on master2

	mst_mysqli_query(6/*offset*/, $link, "SET @myrole = 'Master3'"); //Execute on master3

	mst_mysqli_query(7/*offset*/, $link, "INSERT INTO gtid_test(id) VALUES(@myrole)"); //Execute on master1
	$master1_gtid = $gtid = mysqlnd_ms_get_last_gtid($link);
	if (!$gtid)
		printf("[".(string)8/*offset*/."] Expecting gtid got empty, [%d] %s\n", $link->errno, $link->error);	
	$rgtid = mst_mysqli_fetch_gtid_memcached(9/*offset*/, $memc_link, $db, $rwhere, true);
	$wgtid = mst_mysqli_fetch_wgtid_memcached(10/*offset*/, $memc_link, $db, $wwhere, true);
	if ($rgtid[1] != $gtid || $wgtid[1] != $gtid)
		printf("[".(string)11/*offset*/."] Expecting gtid %s on memcached got %s %s\n", $gtid, $rgtid, $wgtid[1]);	
	if (!mst_mysqli_wait_gtid_memcached(12/*offset*/, $master2_link, $db, $gtid))
		printf("[".(string)13/*offset*/."] Timeout or gtid not replicated for %s, [%d] %s\n", $gtid, $master2_link->errno, $master2_link->error);	

    print "fetch on master2?";
	$res = mst_mysqli_query(14/*offset*/, $link, "SELECT CONCAT(@myrole,'-',id) AS _ext_id FROM gtid_test ORDER BY _ext_id"); //Execute on master2
	var_dump($res->fetch_all());
	
    print "fetch on master1?";
	$res = mst_mysqli_query(15/*offset*/, $link, "SELECT CONCAT(@myrole,'-',id) AS _ext_id FROM gtid_test ORDER BY _ext_id"); //Execute on master1
	var_dump($res->fetch_all());

    print "fetch on master2?";
	$res = mst_mysqli_query(16/*offset*/, $link, "SELECT CONCAT(@myrole,'-',id) AS _ext_id FROM gtid_test ORDER BY _ext_id"); //Execute on master2
	var_dump($res->fetch_all());

    print "Errors for link1?";
	$res = mst_mysqli_fetch_gtid_memcached_errors(17/*offset*/, $memc_link, $db);
	var_dump($res->fetch_all());

    print "fetch on master1?";
	$res = mst_mysqli_query(18/*offset*/, $link2, "SELECT *  FROM gtid_test"); //Execute on master1
	var_dump($res->fetch_all());
	
    print "fetch on master2?";
	$res = mst_mysqli_query(19/*offset*/, $link2, "SELECT * FROM gtid_test"); //Execute on master2
	var_dump($res->fetch_all());

    print "fetch on master1?";
	$res = mst_mysqli_query(20/*offset*/, $link2, "SELECT * FROM gtid_test"); //Execute on master2
	var_dump($res->fetch_all());

    print "Errors for link2?";
	$res = mst_mysqli_fetch_gtid_memcached_errors(21/*offset*/, $memc_link, $db);
	var_dump($res->fetch_all());

	$res = mst_mysqli_query(22/*offset*/, $master1_link, $sql['fetch_last_gtid']); //get last gtid from valid master
	$gtid = $res->fetch_assoc()['trx_id'];

    $emid = $emulated_master_host_only . ':' . $emulated_master_port . ':' . ($emulated_master_socket ? $emulated_master_socket : '/var/lib/mysql/mysql.sock');
    $slid = $slave_host_only . ':' . $slave_port . ':' . ($slave_socket ? $slave_socket : '/var/lib/mysql/mysql.sock');

	// Check cached
	$res = mst_mysqli_query(24/*offset*/, $memc_link, $sql['select'] . " WHERE id = '$emid' OR id = '$slid'"); //get last gtid from valid master
	if (($c = count($res->fetch_all())) != 2)
		printf("[".(string)13/*offset*/."] Unexpected last_gtid cached count %d\n", $c);	

	// Add cache for master3

	mst_mysqli_set_gtid_memcached(23/*offset*/, $memc_link, $db, $gtid, 'id = \'' . $emulated_master_host_only . ':' . $emulated_master_port . ':' . ($emulated_master_socket ? $emulated_master_socket : '/var/lib/mysql/mysql.sock') . '\'', $gtid, $db);

    print "fetch on master1?";
	$res = mst_mysqli_query(25/*offset*/, $link3, "SELECT *  FROM gtid_test"); //Execute on master1
	var_dump($res->fetch_all());
	
    print "fetch on master2?";
	$res = mst_mysqli_query(26/*offset*/, $link3, "SELECT * FROM gtid_test"); //Execute on master2
	var_dump($res->fetch_all());

    print "fetch on master3?";
	$res = mst_mysqli_query(27/*offset*/, $link3, "SELECT * FROM gtid_test"); //Execute on master3
	var_dump($res->fetch_all());


    print "Errors for link3?";
	$res = mst_mysqli_fetch_gtid_memcached_errors(28/*offset*/, $memc_link, $db);
	var_dump($res->fetch_all());

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
fetch on master2?array(2) {
  [0]=>
  array(1) {
    [0]=>
    string(15) "Master1-Master1"
  }
  [1]=>
  array(1) {
    [0]=>
    string(24) "Master1-MY_EXECUTED_GTID"
  }
}
fetch on master1?array(2) {
  [0]=>
  array(1) {
    [0]=>
    string(15) "Master2-Master1"
  }
  [1]=>
  array(1) {
    [0]=>
    string(24) "Master2-MY_EXECUTED_GTID"
  }
}
fetch on master2?array(2) {
  [0]=>
  array(1) {
    [0]=>
    string(15) "Master1-Master1"
  }
  [1]=>
  array(1) {
    [0]=>
    string(24) "Master1-MY_EXECUTED_GTID"
  }
}
Errors for link1?array(0) {
}
fetch on master1?array(2) {
  [0]=>
  array(2) {
    [0]=>
    string(7) "Master1"
    [1]=>
    NULL
  }
  [1]=>
  array(2) {
    [0]=>
    string(16) "MY_EXECUTED_GTID"
    [1]=>
    string(0) ""
  }
}
fetch on master2?array(2) {
  [0]=>
  array(2) {
    [0]=>
    string(7) "Master1"
    [1]=>
    NULL
  }
  [1]=>
  array(2) {
    [0]=>
    string(16) "MY_EXECUTED_GTID"
    [1]=>
    string(0) ""
  }
}
fetch on master1?array(2) {
  [0]=>
  array(2) {
    [0]=>
    string(7) "Master1"
    [1]=>
    NULL
  }
  [1]=>
  array(2) {
    [0]=>
    string(16) "MY_EXECUTED_GTID"
    [1]=>
    string(0) ""
  }
}
Errors for link2?array(0) {
}
fetch on master1?array(2) {
  [0]=>
  array(2) {
    [0]=>
    string(7) "Master1"
    [1]=>
    NULL
  }
  [1]=>
  array(2) {
    [0]=>
    string(16) "MY_EXECUTED_GTID"
    [1]=>
    string(0) ""
  }
}
fetch on master2?array(2) {
  [0]=>
  array(2) {
    [0]=>
    string(7) "Master1"
    [1]=>
    NULL
  }
  [1]=>
  array(2) {
    [0]=>
    string(16) "MY_EXECUTED_GTID"
    [1]=>
    string(0) ""
  }
}
fetch on master3?array(1) {
  [0]=>
  array(2) {
    [0]=>
    string(16) "MY_EXECUTED_GTID"
    [1]=>
    string(0) ""
  }
}
Errors for link3?array(0) {
}
done!

