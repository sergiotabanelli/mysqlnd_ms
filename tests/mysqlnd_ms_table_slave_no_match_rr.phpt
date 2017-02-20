--TEST--
table filter: no slave for SELECT, use master - pick = roundrobin
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

		'lazy_connections' => 1,
		'filters' => array(
			"table" => array(
				"rules" => array(
					$db . ".test" => array(
						"slave" => array("slave1"),
						"master" => array("master1"),
					),
				),
			),

			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_table_slave_no_match_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_table_slave_no_match_rr.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* shall use host = forced_master_hostname_abstract_name from the ini file */
	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* TODO: what is master and slave server use same thread id? */

	$threads = array();
	mst_mysqli_query(3, $link, "DROP TABLE IF EXISTS test");
	$threads[$link->thread_id] = array("master");

	mst_mysqli_query(4, $link, "CREATE TABLE test(id INT)");
	$threads[$link->thread_id][] = 'master';
	mst_mysqli_query(5, $link, "INSERT INTO test(id) VALUES (1)");
	$threads[$link->thread_id][] = 'master';

	/* db.test -> slave 1 */
	mst_mysqli_create_test_table($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
	$res = mst_mysqli_verbose_query(6, $link, "SELECT id FROM test");
	$threads[$link->thread_id] = array("slave");

	/* db.DUAL -> master, slave 1 has db.test only! */
	$res = mst_mysqli_verbose_query(7, $link, "SELECT 1 FROM DUAL");
	var_dump($res->fetch_assoc());
	$threads[$link->thread_id][] = 'master';

	foreach ($threads as $thread_id => $roles) {
		printf("%d: ", $thread_id);
		foreach ($roles as $k => $role)
		  printf("%s,", $role);
		printf("\n");
	}

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");

	if (!unlink("test_mysqlnd_ms_table_slave_no_match_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_table_slave_no_match_rr.ini'.\n");
?>
--EXPECTF--
[006 + 01] Query 'SELECT id FROM test'
[006 + 02] Thread '%d'
[007 + 01] Query 'SELECT 1 FROM DUAL'

Fatal error: mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate slave connection. 0 slaves to choose from. Something is wrong in %s on line %d