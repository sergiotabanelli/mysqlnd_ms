--TEST--
set_server_option() and transient errors, lazy
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
		'lazy_connections' => 1,
		'transient_error' => array('mysql_error_codes' => array(2006), "max_retries" => 3, "usleep_retry" => 2),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_set_server_option_lazy.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_set_server_option_lazy.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");


	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	/* Sort of bogus test - there will be no error on the line. And, we can't kill
	the line to force an error as the line has not been established yet. */
	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	if (!$link->multi_query("DROP TABLE IF EXISTS test; DROP TABLE IF EXISTS test"))
		printf("[003] [%d] %s\n", $link->errno, $link->error);

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	/* Now the line is established and killing checks the same situation as if
	we had a non-lazy connection... however ... */

	$link->kill($link->thread_id);
	sleep(1);

	if (!$link->multi_query("DROP TABLE IF EXISTS test; DROP TABLE IF EXISTS test"))
		printf("[004] [%d] %s\n", $link->errno, $link->error);

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_set_server_option_lazy.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_set_server_option_lazy.ini'.\n");
?>
--EXPECTF--
Transient error retries: 0
[003] [1064] %s
Transient error retries: 0
[004] [2006] %s
Transient error retries: 3
done!