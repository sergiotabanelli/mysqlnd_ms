--TEST--
Issue 16: in multi master scenarios, write consistency enforcement could fail if a transaction start with a read query
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
			)
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

if ($error = mst_create_config("test_mysqlnd_ms_issue_16.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
if ($error = mst_mysqli_setup_gtid_memcached($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
  	die(sprintf("Failed to setup GTID memcached on emulated master, %s\n", $error));
if ($error = mst_mysqli_create_gtid_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
	die(sprintf("Failed to create test table on master %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_issue_16.ini
mysqlnd_ms.collect_statistics=1
mysqlnd_ms.multi_master=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
 	$sql = mst_get_gtid_memcached($db);
   	$wwhere = "m.id = '" . $sql['global_wkey'] . "'";
	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("Connection fails [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	/* we need an extra non-MS link for checking memcached GTID. */
	$memc_link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
	$master1_link = mst_mysqli_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
	$master2_link = mst_mysqli_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
	if (!($ret = $link->begin_transaction()))
		printf("Begin transaction fails [%d] %s\n", $link->errno, $link->error);
	if (!($ret = $link->query("SELECT @@server_uuid AS id FROM DUAL")))
		printf("SELECT id fails [%d] %s\n", $link->errno, $link->error);
	$wgtid = mst_mysqli_fetch_wgtid_memcached('', $memc_link, $db, $wwhere, true);
	if (substr($wgtid[0], 0, 1) != 'R') 
		printf("Write consistency not initialized %s\n", $wgtid[0]);
	if (!($ret = $link->query("INSERT INTO gtid_test(id) VALUES(@@server_uuid)")))
		printf("INSERT id fails [%d] %s\n", $link->errno, $link->error);
	$wgtid = mst_mysqli_fetch_wgtid_memcached('', $memc_link, $db, $wwhere, true);
	if (substr($wgtid[0], 0, 1) != 'R') 
		printf("Write consistency closed before commit %s\n", $wgtid[0]);
	if (!($ret = $link->commit()))
		printf("Commit transaction fails [%d] %s\n", $link->errno, $link->error);
	$wgtid = mst_mysqli_fetch_wgtid_memcached('', $memc_link, $db, $wwhere, true);
	if (substr($wgtid[0], 0, 1) != 'E') 
		printf("Write consistency not closed %s\n", $wgtid[0]);

	// check if mecached is cleaned on a close within a transaction
	if (!($ret = $link->begin_transaction()))
		printf("Begin transaction fails [%d] %s\n", $link->errno, $link->error);
	if (!($ret = $link->query("SELECT @@server_uuid AS id FROM DUAL")))
		printf("SELECT id fails [%d] %s\n", $link->errno, $link->error);
	$wgtid = mst_mysqli_fetch_wgtid_memcached('', $memc_link, $db, $wwhere, true);
	if (substr($wgtid[0], 0, 1) != 'R') 
		printf("Second write consistency not initialized %s\n", $wgtid[0]);
	$link->close();
	$wgtid = mst_mysqli_fetch_wgtid_memcached('', $memc_link, $db, $wwhere, true);
	if (substr($wgtid[0], 0, 1) != 'E') 
		printf("Second write consistency not closed %s\n", $wgtid[0]);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_issue_16.ini"))
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_nw_ps_wc_basics.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_gtid_memcached($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n", $error);
	if ($error = mst_mysqli_drop_gtid_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
		printf("[clean] %s\n", $error);
?>
--EXPECTF--
done!