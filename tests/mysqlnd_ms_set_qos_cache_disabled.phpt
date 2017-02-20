--TEST--
mysqlnd_ms_set_qos(), cache w. cache disabled
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
if (defined("MYSQLND_MS_HAVE_CACHE_SUPPORT")) {
	die("SKIP Cache support compiled in");
}

include_once("util.inc");
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'pick' => 'roundrobin',
		'lazy_connections' => 1,

	),
);
if ($error = mst_create_config("test_mysqlnd_ms_set_qos_cache_disabled.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_set_qos_cache_disabled.ini
--FILE--
<?php
	/* Caution: any test setting on replication is prone to false positive. Replication may be down! */

	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	if (true !== ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL, MYSQLND_MS_QOS_OPTION_CACHE, 4))) {
		printf("[002] [%d] %s\n", $link->errno, $link->error);
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_set_qos_cache_disabled.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_set_qos_cache_disabled.ini'.\n");
?>
--EXPECTF--
Warning: mysqlnd_ms_set_qos(): Cache support is not available with this build in %s on line %d
[002] [0%s
done!