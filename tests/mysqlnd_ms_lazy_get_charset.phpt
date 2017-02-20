--TEST--
lazy connections and get charset before running a statement
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
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_lazy_get_charset.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_lazy_get_charset.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	if (!($tmp = $link->get_charset()))
		printf("[003] [%d] '%s'\n", $link->errno, $link->error);
	else
		printf("[003] Charset is '%s'\n");

	if ($res = mst_mysqli_query(4, $link, "SELECT 1 FROM DUAL"))
		var_dump($res->fetch_assoc());


	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");

	if (!unlink("test_mysqlnd_ms_lazy_get_charset.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_lazy_get_charset.ini'.\n");
?>
--EXPECTF--
Warning: mysqli::get_charset(): The connection has no charset associated in %s on line %d
[003] [%d] ''
array(1) {
  [1]=>
  string(1) "1"
}
done!