--TEST--
Config file with nothing but {}
--SKIPIF--
<?php
require_once('skipif.inc');
if (version_compare(PHP_VERSION, '5.3.99', "<")) {
	die("SKIP Function not available before PHP 5.4.0");
}
_skipif_check_extensions(array("mysqli"));

if (FALSE === file_put_contents("test_mysqlnd_ms_settings_ini_file_json_stub.ini", "{}"))
	die(sprintf("SKIP Failed to create config file\n"));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_settings_ini_file_json_stub.ini
--FILE--
<?php
	require_once("connect.inc");

	if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
		printf("[001] Cannot connect to the server using host=%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
			$host, $user, $db, $port, $socket);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_settings_ini_file_json_stub.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_settings_ini_file_json_stub.ini'.\n");
?>
--EXPECTF--

Warning: mysqli_real_connect(): %A
[001] Cannot connect to the server using host=%s
done!