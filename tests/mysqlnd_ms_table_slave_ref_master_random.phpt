--TEST--
table filter: slave referencing master
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_check_feature(array("table_filter"));
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
		'lazy_connections' => 0,
		'filters' => array(
			"table" => array(
				"rules" => array(
					$db . ".%" => array(
					  "master" => array("master1"),
					  "slave" => array("master1"),
					),
				),
			),
			"random" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_table_slave_ref_master_random.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_table_slave_ref_master_random.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* valid config or not? */
	mst_mysqli_verbose_query(2, $link, "DROP TABLE IF EXISTS test");
	mst_mysqli_verbose_query(3, $link, "CREATE TABLE test(id INT)");
	mst_mysqli_verbose_query(4, $link, "SELECT * FROM test");

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_table_slave_ref_master_random.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_table_slave_ref_master_random.ini'.\n");
?>
--EXPECTF--
[002 + 01] Query 'DROP TABLE IF EXISTS test'
[002 + 02] Thread '%d'
[003 + 01] Query 'CREATE TABLE test(id INT)'
[003 + 02] Thread '%d'
[004 + 01] Query 'SELECT * FROM test'

Fatal error: mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate slave connection. 0 slaves to choose from. Something is wrong in %s on line %d