--TEST--
Invalid connect flag (negative number and array)
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array(
			  array(
				'host' 			=> $master_host_only,
				'port' 			=> (int)$master_port,
				'socket' 		=> $master_socket,
				'db'			=> (string)$db,
				'user'			=> $user,
				'password'		=> $passwd,
				'connect_flags' => -1,
			  ),
		),
		'slave' => array(
			array(
				'host' 			=> $slave_host_only,
				'port' 			=> (double)$slave_port,
				'socket'		=> $slave_socket,
				'db'			=> $db,
				'user'			=> $user,
				'password'		=> $passwd,
				'connect_flags' => array(-1),
			),
		),
		'pick' => 'roundrobin',
		'lazy_connections' => 0,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_settings_connect_flags_invalid.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_settings_connect_flags_invalid.ini
--FILE--
<?php
	require_once("connect.inc");

	/* note that user etc are to be taken from the config! */
	if (!($link = mst_mysqli_connect("myapp", NULL, NULL, NULL, NULL, NULL)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_settings_connect_flags_invalid.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_settings_connect_flags_invalid.ini'.\n");
?>
--EXPECTF--
Catchable fatal error: mysqli_real_connect(): (mysqlnd_ms) Invalid value for connect_flags '-1' . Stopping in %s on line %d