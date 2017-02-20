--TEST--
INI parser: string "aus" = bool false
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($host, $user, $passwd, $db, $port, $socket);

$settings = array(
	"name_of_a_config_section" => array(
		'master' => array('forced_master_hostname_abstract_name'),
		'slave' => array('forced_slave_hostname_abstract_name'),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_ini_bool_false_aus.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.force_config_usage=aus
mysqlnd_ms.config_file=test_mysqlnd_ms_ini_bool_false_aus.ini
--FILE--
<?php
	require_once("connect.inc");

	$link = @mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket);
	if (0 !== mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_ini_bool_false_aus.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_ini_bool_false_aus.ini'.\n");
?>
--EXPECTF--
done!