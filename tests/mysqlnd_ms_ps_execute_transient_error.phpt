--TEST--
PS::execute() and transient error
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
		'lazy_connections' =>  0,
		'transient_error' => array('mysql_error_codes' => array(1062), "max_retries" => 5, "usleep_retry" => 3),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_ps_execute_transient_error.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_ps_execute_transient_error.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!$link->query("DROP TABLE IF EXISTS test") ||
		!$link->query("CREATE TABLE test(id INT PRIMARY KEY)") ||
		!$link->query("INSERT INTO test(id) VALUES (1)")) {
		printf("[002] [%d] %s\n", $link->errno, $link->error);
	}

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	if (!$stmt = $link->prepare("INSERT INTO test(id) VALUES (2)"))
		printf("[003] [%d] %s\n", $link->errno, $link->error);

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	if (!$stmt->execute())
		printf("[004] [%d] %s\n", $stmt->errno, $stmt->error);

	if (!$stmt = $link->prepare("INSERT INTO test(id) VALUES (2)"))
		printf("[005] [%d] %s\n", $link->errno, $link->error);

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	if (!$stmt->execute())
		printf("[006] [%d] %s\n", $stmt->errno, $stmt->error);

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	if (!$link->query("DROP TABLE IF EXISTS test") )
		printf("[007] [%d] %s\n", $link->errno, $link->error);

	if (!$stmt->execute())
		printf("[008] [%d] %s\n", $stmt->errno, $stmt->error);

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	$stmt->close();

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_ps_execute_transient_error.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_ps_execute_transient_error.ini'.\n");
?>
--EXPECTF--
Transient error retries: 0
Transient error retries: 0
Transient error retries: 0
[006] [1062] %s
Transient error retries: 5
[008] [1146] %s
Transient error retries: 5
done!