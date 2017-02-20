--TEST--
Transient error basics
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
		'transient_error' => array('mysql_error_codes' => array(1452), "max_retries" => 2, "usleep_retry" => 100),
	),
);
if ($error = mst_create_config("test_mysqlnd_transient_error.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_transient_error.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!mst_mysqli_query(2, $link, "DROP TABLE IF EXISTS c") ||
		!mst_mysqli_query(3, $link, "DROP TABLE IF EXISTS p") ||
		!mst_mysqli_query(4, $link, "CREATE TABLE p(id INT NOT NULL PRIMARY KEY AUTO_INCREMENT)") ||
		!mst_mysqli_query(5, $link, "CREATE TABLE c(id INT NOT NULL PRIMARY KEY AUTO_INCREMENT,
									p_id INT NOT NULL,
									FOREIGN KEY fk_p_id(p_id) REFERENCES p(id))")) {
		printf("[006] [%d] %s\n", $link->errno, $link->error);
	}

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	if (!$link->query("INSERT INTO c(p_id) VALUES(2)")) {
		printf("[007] [%d] %s\n", $link->errno, $link->error);
	}
	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_transient_error.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_transient_error.ini'.\n");
?>
--EXPECTF--
Transient error retries: 0
[007] [1452] %s
Transient error retries: 2
done!
