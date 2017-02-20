--TEST--
MS + QC - failed slave, failover
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

_skipif_check_extensions(array("mysqli"));
_skipif_check_extensions(array("mysqlnd_qc"));
if (!defined("MYSQLND_MS_HAVE_CACHE_SUPPORT")) {
	die("SKIP Cache support not compiled in");
}
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array("unreachable:7003"),
		'filters' => array(
			"quality_of_service" => array(
				"eventual_consistency" => array(
					'cache' => 123
				),
			),
			"roundrobin" => array(),
		),
		'failover' => array("strategy" => "master", "remember_failed" => 1),
	),


);
if ($error = mst_create_config("test_mysqlnd_ms_qc_failed_slave_failover.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

include_once("util.inc");
msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_qc_failed_slave_failover.ini
apc.use_request_time=0
mysqlnd_qc.use_request_time=0
mysqlnd_qc.collect_statistics=1
mysqlnd_qc.ttl=99

--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");


	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}


	mst_mysqli_query(2, $link, "SELECT 1 FROM DUAL");
	if ($res = mst_mysqli_query(3, $link, "SELECT 1 FROM DUAL"))
		var_dump($res->fetch_assoc());

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_qc_failed_slave_failover.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_qc_failed_slave.ini'.\n");
?>
--EXPECTF--
Warning: mysqli::query(): %A
array(1) {
  [1]=>
  string(1) "1"
}
done!