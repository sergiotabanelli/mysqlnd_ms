--TEST--
Transient error: change user
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
		'pick' 	=> array('roundrobin'),
		'lazy_connections' =>  0,
		'transient_error' => array('mysql_error_codes' => array(1045), "max_retries" => 2, "usleep_retry" => 100),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_change_user_transient_error.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_change_user_transient_error.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());


	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	if (!@$link->change_user("letmebe", "unknown", "whatever")) {
		if ($link->errno != 1045 && $link->errno != 2006) {
		  printf("[002] Check the error message and code, it seems uncommon. [002] [%d] %s\n", $link->errno, $link->error);
		} else {
		  printf("[002] [%d] %s\n", $link->errno, $link->error);
		}
	}

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);
	if ($stats['transient_error_retries'] < 3) {
		/* Can be 3 or 4. Until at least MySQL 5.6.10 a failed change user call did not close
		the line. Some later versions close the line. If the line is closed, there will
		be less retries. */
		printf("[003] There should be 3 or 4 retries depending on the server version.");
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_change_user_transient_error.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_change_user_transient_error.ini'.\n");
?>
--EXPECTF--
Transient error retries: 0
[002] [%d] %s
Transient error retries: %d
done!