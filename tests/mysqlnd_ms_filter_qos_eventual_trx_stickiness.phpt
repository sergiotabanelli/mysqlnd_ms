--TEST--
Filter QOS, eventual, trx_stickiness=master
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP 5.3.99 or newer, using " . PHP_VERSION));

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $emulated_master_host_only,
				'port' 		=> (int)$emulated_master_port,
				'socket' 	=> $emulated_master_socket,
			),
		),
		'slave' => array(
			"slave1" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket,
			),
		 ),

		'lazy_connections' => 0,
		'trx_stickiness' => 'master',
		'filters' => array(
			"quality_of_service" => array(
				"eventual_consistency" => 1
			),
			'roundrobin' => 1
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos_eventual_trx_stickiness.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

include_once("util.inc");
msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos_eventual_trx_stickiness.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");



	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* master, RW split */
	if (!$link->query("DROP TABLE IF EXISTS test") ||
		!$link->query("CREATE TABLE test(id INT)") ||
		!$link->query("INSERT INTO test(id) VALUES (1)"))
		printf("[002] [%d] %s\n", $link->errno, $link->error);

	$master_id = mst_mysqli_get_emulated_id(3, $link);

	$link->autocommit(false);
	if ($res = mst_mysqli_query(4, $link, "SELECT id FROM test"))
		var_dump($res->fetch_all());

	$server_id = mst_mysqli_get_emulated_id(5, $link);
	if ($server_id != $master_id)
		printf("[006] Query should have been executed on master because of trx stickiness\n");

	$link->autocommit(true);

	if ($res = mst_mysqli_query(6, $link, "SELECT 1 FROM DUAL"))
		var_dump($res->fetch_all());

	$server_id = mst_mysqli_get_emulated_id(7, $link);
	if ($server_id == $master_id)
		printf("[008] Query should have been executed on slave because of autocommit mode\n");


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_qos_eventual_trx_stickiness.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos_eventual_trx_stickiness.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %s\n");
?>
--EXPECTF--
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
}
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
}
done!