--TEST--
Wrong port (9999), slave
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if ($emulated_slave_socket || ('localhost' == $emulated_slave_host_only) || (9999 == $emulated_slave_port))
	die("SKIP Slave is using socket connection, can't test for port, port will not be used");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_can_connect($emulated_slave_host_only, $user, $passwd, $db, 9999, $emulated_slave_socket, "No port connection");


$settings = array(
	"myapp" => array(
		'master' => array(
			  array(
				'host' 		=> $emulated_master_host_only,
				'port' 		=> $emulated_master_port,
				'socket' 	=> $emulated_master_socket,
				'db'		=> $db,
				'user'		=> $user,
				'password'	=> $passwd,
			  ),
		),
		'slave' => array(
			array(
			  'host' 	=> $emulated_slave_host_only,
			  'port' 	=> 9999,
			  'socket' 	=> $emulated_slave_socket,
			  'db'		=> $db,
			  'user'	=> $user,
			  'password'=> $passwd,
			),
		),
		'pick' => 'roundrobin',
		'lazy_connections' => 0,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_settings_port_wrong_slave.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_settings_port_wrong_slave.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* note that user etc are to be taken from the config! */
	if (!($link = mysqli_connect("myapp")))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$res = mst_mysqli_query(2, $link, "SELECT 1 AS _one FROM DUAL");
	var_dump($res->fetch_assoc());

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_settings_port_wrong_slave.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_settings_port_wrong_slave.ini'.\n");
?>
--EXPECTF--
%Aarning: mysqli_connect(): (mysqlnd_ms) Cannot connect to %s in %s on line %d

Warning: mysqli_connect(): (mysqlnd_ms) Error while connecting to the slaves in %s on line %d
%A