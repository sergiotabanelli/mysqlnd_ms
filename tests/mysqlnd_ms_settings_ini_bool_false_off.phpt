--TEST--
INI parser: string "off" = bool false
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
if ($error = mst_create_config("test_mysqlnd_ms_ini_bool_false_off.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[1,2,3]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.force_config_usage=off
mysqlnd_ms.config_file=test_mysqlnd_ms_ini_bool_false_off.ini
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
	if (!unlink("test_mysqlnd_ms_ini_bool_false_off.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_ini_bool_false_off.ini'.\n");
?>
--EXPECTF--
done!