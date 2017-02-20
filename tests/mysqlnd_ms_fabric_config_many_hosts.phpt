--TEST--
Fabric: config parsing (1k Fabric hosts)
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (!getenv("MYSQL_TEST_FABRIC")) {
	die(sprintf("SKIP Fabric - set MYSQL_TEST_FABRIC=1 (config.inc) to enable\n"));
}

$settings = array(
	"myapp" => array(
		'fabric' => array(
			'hosts' => array()
		)
	),
);
for ($i = 0; $i < 1024; $i++) {
	$settings['myapp']['fabric']['hosts'][] = array('host' => '127.0.0.1', 'port' => 123);
}
if ($error = mst_create_config("test_mysqlnd_ms_fabric_config_many_hosts.ini", $settings))
  die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_fabric_config_many_hosts.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (0 !== mysqli_connect_errno())
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_fabric_config_many_hosts.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_fabric_config_many_hosts.ini'.\n");
?>
--EXPECTF--
Fatal error: mysqli_real_connect(): (mysqlnd_ms) Please report a bug: no more than 10 Fabric hosts allowed in %s on line %d