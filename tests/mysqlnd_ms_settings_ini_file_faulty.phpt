--TEST--
mysqlnd_ms: Faulty ini file
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
$file = "test_mysqlnd_ms_settings_ini_file_faulty.ini";

if (file_exists($file) && !@unlink($file))
	die(sprintf("SKIP Cannot unlink existing file '%s'.", $file));

if (!$fp = @fopen($file, "w"))
	die(sprintf("SKIP Cannot open file '%s' for writing.", $file));

if (!@fwrite($fp, "Hihihuhu"))
	die(sprintf("SKIP Cannot write to file '%s'.", $file));

fclose($fp);
?>

--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_settings_ini_file_faulty.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	var_dump($link);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_settings_ini_file_faulty.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_settings_ini_file_faulty.ini'.\n");
?>
--EXPECTF--
%A
[001] [%d] %s
bool(false)
done!