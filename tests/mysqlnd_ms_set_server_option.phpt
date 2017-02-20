--TEST--
set_server_option() and transient errors
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
		'transient_error' => array('mysql_error_codes' => array(2006), "max_retries" => 3, "usleep_retry" => 2),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_set_server_option.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_set_server_option.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");


	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	$link->kill($link->thread_id);
	sleep(1);

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	if (!$link->multi_query("DROP TABLE IF EXISTS test; DROP TABLE IF EXISTS test"))
		printf("[003] [%d] %s\n", $link->errno, $link->error);

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_set_server_option.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_set_server_option.ini'.\n");
?>
--EXPECTF--
Transient error retries: 0
Transient error retries: 0
[003] [2006] %s
Transient error retries: 3
done!