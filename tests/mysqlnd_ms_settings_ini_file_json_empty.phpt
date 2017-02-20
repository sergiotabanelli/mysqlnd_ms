--TEST--
Empty config file
--SKIPIF--
<?php
require_once('skipif.inc');
if (version_compare(PHP_VERSION, '5.3.99', "<")) {
	die("SKIP Function not available before PHP 5.4.0");
}
_skipif_check_extensions(array("mysqli"));

if (FALSE === file_put_contents("test_mysqlnd_ms_settings_ini_file_json_empty.ini", ""))
	die(sprintf("SKIP Failed to create config file\n"));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_settings_ini_file_json_empty.ini
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
	if (!unlink("test_mysqlnd_ms_settings_ini_file_json_empty.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_settings_ini_file_json_empty.ini'.\n");
?>
--EXPECTF--
Warning: mysqli_real_connect(): (mysqlnd_ms) (mysqlnd_ms) Config file [test_mysqlnd_ms_settings_ini_file_json_empty.ini] is empty. If this is not by mistake, please add some minimal JSON to it to prevent this warning. For example, use '{}'  in %s on line %d

Warning: mysqli_real_connect(): php_network_getaddresses: getaddrinfo failed: %s in %s on line %d

Warning: mysqli_real_connect(): (HY000/2002): php_network_getaddresses: getaddrinfo failed: %s in %s on line %d
[001] Cannot connect to the server using %A
done!