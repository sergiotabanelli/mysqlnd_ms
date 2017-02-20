--TEST--
GTID, config, invalid fetch_last_gtid
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

require_once('skipif.inc');
  require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

include_once("util.inc");
$sql = mst_get_gtid_sql($db);

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $emulated_master_host_only,
				'port' 		=> (int)$emulated_master_port,
				'socket' 	=> $emulated_master_socket,
			),
		),
		'slave' => array(
			"slave1" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket,
			),
		),

		'global_transaction_id_injection' => array(
			'fetch_last_gtid'	=> array($sql['update']),
		),

		'lazy_connections' => 1,
		'trx_stickiness' => 'disabled',
		'filters' => array(
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_gtid_invalid_fetch_last_gtid.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_invalid_fetch_last_gtid.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	/* we need an extra non-MS link for checking GTID. If we use MS link, the check itself will change GTID */
	$emulated_master_link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);

	/* auto commit on (default) */
	$gtid = mst_mysqli_fetch_gtid(3, $emulated_master_link, $db);
	mst_mysqli_query(4, $link, "SET @myrole = 'Master'");
	$new_gtid = mst_mysqli_fetch_gtid(6, $emulated_master_link, $db);
	if ($new_gtid <= $gtid) {
		printf("[007] GTID has not been incremented on master in auto commit mode\n");
	}
	$gtid = $new_gtid;

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_invalid_fetch_last_gtid.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_invalid_fetch_last_gtid.ini'.\n");
?>
--EXPECTF--
Catchable fatal error: mysqli_real_connect(): (mysqlnd_ms) 'fetch_last_gtid' from 'global_transaction_id_injection' must be a string in %s on line %d