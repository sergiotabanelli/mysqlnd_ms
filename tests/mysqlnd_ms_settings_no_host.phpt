--TEST--
No hostname
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
					'port' 		=> (int)$master_port,
				'socket' 	=> $master_socket,
				'db'		=> (string)$db,
				'user'		=> $user,
				'password'	=> $passwd,
			  ),
		),
		'slave' => array(
			array(
				  'port' 	=> (double)$slave_port,
			  'socket' 	=> $slave_socket,
			  'db'		=> $db,
			  'user'	=> $user,
			  'password'=> $passwd,
			),
		),
		'pick' => 'roundrobin',
		'lazy_connections' => 0,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_settings_no_host.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_settings_no_host.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	set_error_handler('mst_error_handler');

	/* note that user etc are to be taken from the config! */
	if (!($link = mst_mysqli_connect("myapp", NULL, NULL, NULL, NULL, NULL)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_settings_no_host.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_settings_no_host.ini'.\n");
?>
--EXPECTF--
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) Cannot find [host] in [master] section in config in %s on line %d
[E_WARNING] mysqli_real_connect(): (mysqlnd_ms) Error while connecting to the master(s) in %s on line %d
[E_WARNING] mysqli_real_connect(): (HY000/2000): (mysqlnd_ms) Cannot find [host] in section in config in %s on line %d
[001] [2000] (mysqlnd_ms) Cannot find [host] in section in config
done!