--TEST--
Config settings: many slaves
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"do_not_overload_mysql" => array(
		'master' => array($host),
		'slave' => array($host),
	),
);
/* in the hope that few Mysql test server are configures to handle 5000 connections */
for ($i = 0; $i < 1; $i++)
	$settings['do_not_overload_mysql']['slave'][] = $host;

if ($error = mst_create_config("test_mysqlnd_ms_settings_force_many_slaves.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_settings_force_many_slaves.ini
--FILE--
<?php
	require_once("connect.inc");

	/* shall use host = forced_master_hostname_abstract_name from the ini file */
	if (!($link = mst_mysqli_connect("do_not_overload_mysql", $user, $passwd, $db, $port, $socket)))
		printf("[001] Cannot connect to the server using host=%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
			$host, $user, $db, $port, $socket);

	if (!$res = $link->query("SELECT 1 AS _one FROM DUAL"))
		printf("[002] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	if ($row['_one'] != 1) {
		printf("[003] [%d] %s - Wrong results, dumping results.\n", $link->errno, $link->error);
		var_dump($rows);
	}
	$res->close();

	if (!$res = $link->query("DROP TABLE IF EXISTS test"))
		printf("[004] [%d] %s\n", $link->errno, $link->error);


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_settings_force_many_slaves.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_settings_force_many_slaves.ini'.\n");
?>
--EXPECTF--
done!