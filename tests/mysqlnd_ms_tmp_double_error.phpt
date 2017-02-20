--TEST--
Temporary
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);

if (!function_exists("iconv"))
	die("SKIP needs iconv extension\n");

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_tmp_double_error.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_tmp_double_error.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$link->query("role=master I_HOPE_THIS_IS_INVALID_SQL");
	printf("[%d] '%s'\n", $link->errno, $link->error);
	$link->query(sprintf("/*%s*/SELECT 1", MYSQLND_MS_LAST_USED_SWITCH));
	printf("[%d] '%s'\n", $link->errno, $link->error);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_tmp_double_error.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_tmp_double_error.ini'.\n");
?>
--EXPECTF--
[1064] '%s'
[0] ''
done!