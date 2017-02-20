--TEST--
select_db() and transient error
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'lazy_connections' => 0,
		'transient_error' => array('mysql_error_codes' => array(1049), "max_retries" => 2, "usleep_retry" => 99),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_select_db_transient_error.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

function test_mysql_access($host, $user, $passwd, $db, $port, $socket) {

	if (!$link = mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket))
		die(sprintf("skip Cannot connect, [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

	return $link->select_db("blubberlutsch");
}

if (test_mysql_access($emulated_master_host_only, $user, $passwd, $db, $port, $socket))
	die("skip Database 'blubblerlutsch' found on emulated master.");

if (test_mysql_access($emulated_slave_host_only, $user, $passwd, $db, $port, $socket))
	die("skip Database 'blubblerlutsch' found on emulated slave.");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_select_db_transient_error.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	if (!$link->select_db("blubblerlutsch"))
		printf("[002] [%d] %s\n", $link->errno, $link->error);

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	if (!$link->select_db($db))
		printf("[003] [%d] %s\n", $link->errno, $link->error);

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_select_db_transient_error.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_select_db_transient_error.ini'.\n");
?>
--EXPECTF--
Transient error retries: 0
[002] [1049] %s
Transient error retries: 4
Transient error retries: 4
done!