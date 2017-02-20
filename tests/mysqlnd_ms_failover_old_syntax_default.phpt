--TEST--
Failover=default, old syntax
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"myapp" => array(
		'master' => array("unreachable:7033"),
		'slave' => array("unreachable:6033", $emulated_slave_host),
		'lazy_connections' => 1,
		'pick' 	=> array('roundrobin'),
		'failover' => array(),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_failover_old_syntax_default.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_failover_old_syntax_default.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH, true, true, false, version_compare(PHP_VERSION, '5.3.99', ">"));

	/* tries first, fails */
	mst_mysqli_fech_role(mst_mysqli_query(5, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role", NULL, true, true, false, version_compare(PHP_VERSION, '5.3.99', ">")));

	/* picks second */
	mst_mysqli_fech_role(mst_mysqli_query(6, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role"));

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_failover_old_syntax_default.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_failover_old_syntax_default.ini'.\n");
?>
--EXPECTF--
Connect error, [002] %s
Connect error, [005] %s
This is '' speaking
done!