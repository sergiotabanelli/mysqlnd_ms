--TEST--
error, errno, sqlstate
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

if (!function_exists("iconv"))
	die("SKIP needs iconv extension\n");

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
		'pick' => array("roundrobin"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_error_message.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_error_message.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	mst_mysqli_create_test_table($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!$link->query("SELECT first_unknown_column FROM test"))
		printf("[002] Expected error, [%d] %s\n", $link->errno, $link->error);

	if (!$link->query("SELECT second_unknown_column FROM test"))
		printf("[003] Expected error, [%d] %s\n", $link->errno, $link->error);

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_error_message.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_error_message.ini'.\n");
?>
--EXPECTF--
[002] Expected error, [%d] %sfirst_unknown_column%s
[003] Expected error, [%d] %ssecond_unknown_column%s
done!