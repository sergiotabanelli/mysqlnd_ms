--TEST--
Invalid server_charset
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
			"master1" => array(
				'host' 		=> $master_host_only,
				'port' 		=> (int)$master_port,
				'socket' 	=> $master_socket,
			),
		),

		'slave' => array(
			"slave1" => array(
				'host' 	=> $slave_host_only,
				'port' 	=> (int)$slave_port,
				'socket' => $slave_socket,
			),
		 ),

		'lazy_connections' => 1,
		'filters' => array(
			"random" => array('sticky' => '1'),
		),

		'server_charset' => "\0please_let_me_be_invalid",
	),

);

if ($error = mst_create_config("test_mysqlnd_ms_settings_offline_charset_invalid.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_settings_offline_charset_invalid.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$text = "foobar";
	if (!($text_enc = $link->real_escape_string($text))) {
		printf("[002] [%d/%s] '%s'\n", $link->errno, $link->sqlstate, $link->error);
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_settings_offline_charset_invalid.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_settings_offline_charset_invalid.ini'.\n");
?>
--EXPECTF--

Fatal error: mysqli_real_connect(): (mysqlnd_ms) Erroneous server_charset [] in %s on line %d