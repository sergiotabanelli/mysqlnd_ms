--TEST--
SQL hints LAST_USED called before any server has been selected, pick = random once
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
		'pick' => array('random' => array('sticky' => '1')),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_last_used_random_once.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_last_used_random_once.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!$link->query(sprintf("/*%s*/SELECT 1", MYSQLND_MS_LAST_USED_SWITCH)))
		printf("[002] [%d][%s] %s\n", $link->errno, $link->sqlstate, $link->error);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_last_used_random_once.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_last_used_random_once.ini'.\n");
?>
--EXPECTF--
Warning: mysqli::query(): (mysqlnd_ms) Last used SQL hint cannot be used because last used connection has not been set yet. Statement will fail in %s on line %d

Warning: mysqli::query(): (mysqlnd_ms) No connection selected by the last filter in %s on line %d
[002] [2000][HY000] (mysqlnd_ms) No connection selected by the last filter
done!