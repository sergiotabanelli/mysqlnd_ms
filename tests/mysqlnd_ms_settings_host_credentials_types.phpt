--TEST--
Per host credentials
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$obj = new stdClass();

$settings = array(
	"myapp" => array(
		'master' => array(
			  array(
				'host' 		=> $master_host_only,
				'port' 		=> 123,
				'socket' 	=> PHP_INT_MAX * 2,
				'db'		=> 1.124,
				'user'		=> array($user),
				'password'	=> $obj,
			  ),
		),
		'slave' => array(
			array(
			  'host' 	=> $slave_host_only,
			  'port' 	=> 456.78,
			  'socket' 	=> "an",
			  'db'		=> false,
			  'user'	=> NULL,
			  'password'=> 123,
			),
		),
		'pick' => 'roundrobin',
		'lazy_connections' => 0,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_settings_host_credentials_types.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_settings_host_credentials_types.ini
--FILE--
<?php
	require_once("connect.inc");

	ob_start();
	/* note that user etc are to be taken from the config! */
	if (!($link = mst_mysqli_connect("myapp", NULL, NULL, NULL, NULL, NULL)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	ob_end_clean();

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_settings_host_credentials_types.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_settings_host_credentials_types.ini'.\n");
?>
--EXPECTF--
Catchable fatal error: mysqli_real_connect(): (mysqlnd_ms) Invalid value for user. Cannot be a list/hash' . Stopping in %s on line %d