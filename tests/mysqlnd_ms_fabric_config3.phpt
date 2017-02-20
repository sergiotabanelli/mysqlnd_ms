--TEST--
Fabric: config parsing (empty hosts list)
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
if ($error = mst_create_config("test_mysqlnd_ms_fabric_config3.ini", $settings))
  die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_fabric_config3.ini
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
	if (!unlink("test_mysqlnd_ms_fabric_config3.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_fabric_config3.ini'.\n");
?>
--EXPECTF--
Fatal error: mysqli_real_connect(): (mysqlnd_ms) Section [hosts] exists but is empty. This is needed for MySQL Fabric in %s on line %d