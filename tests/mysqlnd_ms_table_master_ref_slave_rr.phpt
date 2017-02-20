--TEST--
table filter: master referencing slave (rr)
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
					  "master" => array("slave1"),
					  "slave" => array("slave1"),
					),
				),
			),
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_table_master_ref_slave_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_table_master_ref_slave_rr.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* valid config or not? */
	mst_mysqli_verbose_query(2, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_verbose_query(3, $link, "DROP TABLE IF EXISTS test");
	mst_mysqli_verbose_query(4, $link, "CREATE TABLE test(id INT)");
	mst_mysqli_verbose_query(5, $link, "SELECT * FROM test");
	mst_mysqli_verbose_query(6, $link, "INSERT INTO test(id) VALUES (1)");
	$res = mst_mysqli_verbose_query(7, $link, "SELECT * FROM test");
	if ($res) {
		printf("[008] Who has stored the table ?!");
		var_dump($res->fetch_assoc());
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_table_master_ref_slave_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_table_master_ref_slave_rr.ini'.\n");
?>
--EXPECTF--
[002 + 01] Query 'DROP TABLE IF EXISTS test'
[002 + 02] Thread '%d'
[003 + 01] Query 'DROP TABLE IF EXISTS test'

Warning: mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong in %s on line %d
[003] [2000] (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong
[003 + 02] Thread '%d'
[004 + 01] Query 'CREATE TABLE test(id INT)'

Warning: mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong in %s on line %d
[004] [2000] (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong
[004 + 02] Thread '%d'
[005 + 01] Query 'SELECT * FROM test'
[005 + 02] Thread '%d'
[006 + 01] Query 'INSERT INTO test(id) VALUES (1)'

Warning: mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong in %s on line %d
[006 + 02] Thread '%d'
[007 + 01] Query 'SELECT * FROM test'
[007 + 02] Thread '%d'
done!