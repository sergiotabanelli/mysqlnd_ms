--TEST--
GTID nowait consensus
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
				'host' 	=> $emulated_slave_host_only, // will be used as master
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket,
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
			'fetch_last_gtid'			=> "SELECT value AS trx_id FROM gtid_test WHERE id = 'MY_EXECUTED_GTID'",
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

if (($error = mst_create_config("test_mysqlnd_ms_gtid_nw_consensus.ini", $settings)))
	die(sprintf("SKIP %s\n", $error));
if (($error = mst_mysqli_setup_gtid_memcached($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)))
  	die(sprintf("Failed to setup GTID memcached on emulated master, %s\n", $error));
if (($error = mst_mysqli_create_gtid_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket)))
	die(sprintf("Failed to create test table on master %s\n", $error));
if (($error = mst_mysqli_create_gtid_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)))
	die(sprintf("Failed to create test table on emulated master %s\n", $error));
if (($error = mst_mysqli_create_gtid_test_table($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket)))
	die(sprintf("Failed to create test table on emulated slave %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_nw_consensus.ini
mysqlnd_ms.multi_master=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
	if (!$socket)
		$socket = "/dummmy.sock"; // HACK to avoid compare failures
	if (!$master_socket)
		$master_socket = $socket;
	if (!$emulated_slave_socket)
		$emulated_slave_socket = $socket;
	if (!$emulated_master_socket)
		$emulated_master_socket = $socket;
 	$sql = mst_get_gtid_memcached($db);
    $rwhere = "m.id = '" . $sql['global_key'] . "'";
   	$wwhere = "m.id = '" . $sql['global_wkey'] . "'";
   	$wmaster1 = $rmaster1 = $emaster1 = "E|master1|$master_host_only|$user|$master_socket|$master_port|$db|8519680|?";
   	$rmaster1[0] = 'R'; 
    $wmaster1[0] = 'W'; 
   	$wmaster2 = $rmaster2 = $emaster2 = "E|master2|$emulated_slave_host_only|$user|$emulated_slave_socket|$emulated_slave_port|$db|8519680|?";
	$rmaster2[0] = 'R';
    $wmaster2[0] = 'W'; 
   	$wmaster3 = $rmaster3 = $emaster3 = "E|master3|$emulated_master_host_only|$user|$emulated_master_socket|$emulated_master_port|$db|8519680|?";
   	$rmaster3[0] = 'R';
    $wmaster3[0] = 'W'; 
	$master1_link = mst_mysqli_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
	$master2_link = mst_mysqli_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
 	/* we need an extra non-MS link for checking memcached GTID. */
	$master3_link = $memc_link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
	if (mysqli_connect_errno()) {
		printf("[".(string)1/*offset*/."] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}	
	
	echo "TEST no previous keys found\n";	
	mst_mysqli_insert_gtid_memcached(2/*offset*/, $memc_link, $sql['global_wkey'], 80, $db); // Init wtoken counter to 80
 	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
 	mst_mysqli_query(3/*offset*/, $link, "SET @myrole = 'Master1'");
 	$rgtid = mst_mysqli_fetch_gtid_memcached(4/*offset*/, $memc_link, $db, "m.id = '" . $sql['global_wkey'] . ":78'");
 	if ($rgtid) {
		printf("[".(string)5/*offset*/."] Expecting empty memcached got %s\n", $rgtid);	 	
 	}  	
 	$rgtid = mst_mysqli_fetch_gtid_memcached(6/*offset*/, $memc_link, $db, "m.id = '" . $sql['global_wkey'] . ":79'");
 	if (strncmp($rgtid, $wmaster1, strlen($wmaster1)) != 0) {
		printf("[".(string)7/*offset*/."] Expecting memcached %s got %s\n", $wmaster1, $rgtid);	 	
 	}  	
 	$rgtid = mst_mysqli_fetch_gtid_memcached(8/*offset*/, $memc_link, $db, "m.id = '" . $sql['global_wkey'] . ":80'");
 	if (strncmp($rgtid, $emaster1, strlen($emaster1)) != 0) {
		printf("[".(string)9/*offset*/."] Expecting memcached %s got %s\n", $emaster1, $rgtid);	 	
 	}  	
 	mst_mysqli_delete_gtid_memcached(10/*offset*/, $memc_link, $db);
 	
 	
 	echo "TEST no previous keys found with concurrent connection\n";	
	mst_mysqli_insert_gtid_memcached(11/*offset*/, $memc_link, $sql['global_wkey'], 90, $db); 
	// Set 90 to a wait token this way it shoud retry the selection process
	mst_mysqli_insert_gtid_memcached(12/*offset*/, $memc_link, $sql['global_wkey'] . ":90", $wmaster1 .'?', $db); 
	//  round robin should now be on master 2 but retry should move it to master 3
  	mst_mysqli_query(13/*offset*/, $link, "SET @myrole = 'Master3'"); // We should also get a warning here
  	$rgtid = mst_mysqli_fetch_gtid_memcached(14/*offset*/, $memc_link, $db, "m.id = '" . $sql['global_wkey'] . ":88'");
 	if ($rgtid) {
		printf("[".(string)15/*offset*/."] Expecting empty memcached got %s\n", $rgtid);	 	
 	}  	
 	$rgtid = mst_mysqli_fetch_gtid_memcached(16/*offset*/, $memc_link, $db, "m.id = '" . $sql['global_wkey'] . ":89'");
 	if (strncmp($rgtid, $wmaster2, strlen($wmaster2)) != 0) {
		printf("[".(string)17/*offset*/."] Expecting memcached %s got %s\n", $wmaster2, $rgtid);	 	
 	}  	
 	$rgtid = mst_mysqli_fetch_gtid_memcached(18/*offset*/, $memc_link, $db, "m.id = '" . $sql['global_wkey'] . ":91'");
 	if (strncmp($rgtid, $emaster3, strlen($emaster3)) != 0) {
		printf("[".(string)19/*offset*/."] Expecting memcached %s got %s\n", $emaster3, $rgtid);	 	
 	}  	
	mst_mysqli_delete_gtid_memcached(20/*offset*/, $memc_link, $db);
 	
  	echo "TEST concurrency with mixed executed and running\n";	
	mst_mysqli_insert_gtid_memcached(21/*offset*/, $memc_link, $sql['global_wkey'], 100, $db); 
	// Set 96 to running on master2
	mst_mysqli_insert_gtid_memcached(22/*offset*/, $memc_link, $sql['global_wkey'] . ":96", $rmaster2 .'?', $db); 
	// Set 97 to executed on master2 
	mst_mysqli_insert_gtid_memcached(23/*offset*/, $memc_link, $sql['global_wkey'] . ":97", $emaster2 .'?', $db); 
	// Set 98 to executed on master2 
	mst_mysqli_insert_gtid_memcached(24/*offset*/, $memc_link, $sql['global_wkey'] . ":98", $emaster2 .'?', $db); 
  	mst_mysqli_query(25/*offset*/, $link, "SET @myrole = 'Master2'");
  	$rgtid = mst_mysqli_fetch_gtid_memcached(26/*offset*/, $memc_link, $db, "m.id = '" . $sql['global_wkey'] . ":95'");
 	if ($rgtid) {
		printf("[".(string)27/*offset*/."] Expecting empty memcached got %s\n", $rgtid);	 	
 	}  	
 	$rgtid = mst_mysqli_fetch_gtid_memcached(28/*offset*/, $memc_link, $db, "m.id = '" . $sql['global_wkey'] . ":99'");
 	if (strncmp($rgtid, $rmaster2, strlen($rmaster2)) != 0) {
		printf("[".(string)29/*offset*/."] Expecting memcached %s got %s\n", $rmaster2, $rgtid);	 	
 	}  	
 	$rgtid = mst_mysqli_fetch_gtid_memcached(30/*offset*/, $memc_link, $db, "m.id = '" . $sql['global_wkey'] . ":100'");
 	if (strncmp($rgtid, $emaster2, strlen($emaster2)) != 0) {
		printf("[".(string)31/*offset*/."] Expecting memcached %s got %s\n", $emaster2, $rgtid);	 	
 	}  	
	mst_mysqli_delete_gtid_memcached(32/*offset*/, $memc_link, $db);
 	
  	echo "TEST concurrency with non progressive executed gtid\n";	
	mst_mysqli_insert_gtid_memcached(33/*offset*/, $memc_link, $sql['global_wkey'], 110, $db); 
	// Set 105 to executed on master2 whith 500 
	mst_mysqli_insert_gtid_memcached(34/*offset*/, $memc_link, $sql['global_wkey'] . ":107", $emaster2 .'m2:500?', $db); 
	// Set 106 to executed on master1 with 400 this should be the gtid against which shoud be checked 
	mst_mysqli_insert_gtid_memcached(35/*offset*/, $memc_link, $sql['global_wkey'] . ":108", $emaster1 .'m1:400?', $db); 
	// Set 107 to executed on master1 with 200 this should not be chosed
	mst_mysqli_insert_gtid_memcached(36/*offset*/, $memc_link, $sql['global_wkey'] . ":109", $emaster1 .'m1:200?', $db); 
	mst_mysqli_set_my_gtid_executed($master1_link, 'm2:500,m1:200,m1:400');
	mst_mysqli_set_my_gtid_executed($master2_link, 'm2:500');
	mst_mysqli_set_my_gtid_executed($master3_link, 'm1:400'); // Only master1 and master3 has the right gtid	
  	mst_mysqli_query(37/*offset*/, $link, "SET @myrole = 'Master1'");
 	$rgtid = mst_mysqli_fetch_gtid_memcached(38/*offset*/, $memc_link, $db, "m.id = '" . $sql['global_wkey'] . ":110'");
 	if (strncmp($rgtid, $emaster1, strlen($emaster1)) != 0) {
		printf("[".(string)39/*offset*/."] Expecting memcached %s got %s\n", $emaster1, $rgtid);	 	
 	}  		
  	mst_mysqli_query(40/*offset*/, $link, "SET @myrole = 'Master3'");
 	$rgtid = mst_mysqli_fetch_gtid_memcached(41/*offset*/, $memc_link, $db, "m.id = '" . $sql['global_wkey'] . ":111'");
 	if (strncmp($rgtid, $emaster3, strlen($emaster3)) != 0) {
		printf("[".(string)42/*offset*/."] Expecting memcached %s got %s\n", $emaster3, $rgtid);	 	
 	}  		
	mst_mysqli_delete_gtid_memcached(43/*offset*/, $memc_link, $db);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_nw_consensus.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_nw_consensus.ini'.\n");

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
TEST no previous keys found
TEST no previous keys found with concurrent connection

Warning: mysqli::query(): (mysqlnd_ms) Something wrong found wait token %s for key %s but it should not be there in %s on line %d
TEST concurrency with mixed executed and running
TEST concurrency with non progressive executed gtid
done!

