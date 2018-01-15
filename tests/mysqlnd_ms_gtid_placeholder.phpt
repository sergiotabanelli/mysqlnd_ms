--TEST--
Global Transaction ID Injection
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

require_once('skipif.inc');
  require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);

include_once("util.inc");
$ret = mst_mysqli_server_supports_memcached_plugin($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
if (is_string($ret))
	die(sprintf("SKIP Failed to check if server support MEMCACHED plugin, %s\n", $ret));

if (true != $ret)
	die(sprintf("SKIP Server has no MEMCACHED plugin support (want MySQL 5.6.0+ and active daemon_memcached plugin)"));

if ($error = mst_mysqli_setup_gtid_memcached($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
  die(sprintf("SKIP Failed to setup GTID memcached on emulated master, %s\n", $error));

$sql = mst_get_gtid_memcached($db);

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $emulated_master_host_only,
				'port' 		=> (int)$emulated_master_port,
				'socket' 	=> $emulated_master_socket,
			),
		),
		'slave' => array(),

		'global_transaction_id_injection' => array(
		 	'type'						=> 1,
            "memcached_key" => "#SID#SKEY#SWKEY#DB#USER",
            "memcached_port_add_hack" => (int)$memcached_port_add_hack,
			'report_error' => true,
		),

		'lazy_connections' => 1,
		'trx_stickiness' => 'disabled',
		'filters' => array(
			"quality_of_service" => array(
				"session_consistency" => 1,
			),
			"roundrobin" => array(),
		),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_gtid_placeholder.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_placeholder.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
    session_id("memcsession");
    session_start();
    $_SESSION['mysqlnd_ms_gtid_skey'] = "gtid_skey";
    $_SESSION['mysqlnd_ms_gtid_swkey'] = "gtid_swkey";
	$memcid = "memcsessiongtid_skeygtid_swkey$db$user";
	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[".(string)1/*offset*/."] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	/* we need an extra non-MS link for checking memcached GTID. */
	$memc_link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);

	mst_mysqli_query(2/*offset*/, $link, "SET @myrole = 'Master'");
	$gtid = mysqlnd_ms_get_last_gtid($link);
	if ($gtid != 1) {
		printf("[".(string)3/*offset*/."] GTID has not been incremented on master in auto commit mode %s\n", $gtid);
	}

	$mgtid = mst_mysqli_fetch_gtid_memcached(4/*offset*/, $memc_link, $db, "id = '$memcid'");
	if ($gtid != $mgtid) {
		printf("[".(string)5/*offset*/."] Last GTID %s differs from memcached retrived gtid %s for id %s\n", $gtid, $mgtid, $memcid);
	}
	printf("GTID '%s'\n", $mgtid);	
	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_placeholder.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_basics.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_gtid_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n", $error);
?>
--EXPECTF--
GTID '1'
done!