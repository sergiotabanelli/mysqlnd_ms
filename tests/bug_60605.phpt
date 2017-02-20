--TEST--
#60605 - mysql_get_server_info() @ PHP 5.3.8
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysql"));

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
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_bug_60605.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
_skipif_connect($host, $user, $passwd, $db, $port, $socket);

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_bug_60605.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* without MS */
	$link = @my_mysql_connect($host, $user, $passwd, $db, $port, $socket);
	if (!$link)
		printf("[001] [[%d] %s\n", mysql_errno(), mysql_error());

	printf("Server info w handle: %s\n", mysql_get_server_info($link));
	printf("Server info wo handle: %s\n", mysql_get_server_info());

	mysql_close($link);

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");

	if (!unlink("test_mysqlnd_ms_bug_60605.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_bug_60605.ini'.\n");
?>
--EXPECTF--
Server info w handle: %s
Server info wo handle: %s
done!