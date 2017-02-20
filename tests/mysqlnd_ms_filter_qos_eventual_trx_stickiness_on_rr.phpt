--TEST--
Filter QOS, eventual, trx_stickiness=on, roundrobin
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.4.99-dev', '<'))
	die(sprintf("SKIP Requires PHP 5.5.0 or newer, using " . PHP_VERSION));

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $emulated_master_host_only,
				'port' 		=> (int)$emulated_master_port,
				'socket' 	=> $emulated_master_socket,
			),
			"master2" => array(
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
			"slave2" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket,
			),
		 ),

		'lazy_connections' => 1,
		'trx_stickiness' => 'on',
		'filters' => array(
			"quality_of_service" => array(
				"eventual_consistency" => 1
			),
			'roundrobin' => 1
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos_eventual_trx_stickiness_on_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);


if (!$link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
	die(sprintf("skip Cannot connect, [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

/* BEGIN READ ONLY exists since MySQL 5.6.5 */
if ($link->server_version < 50605) {
	die(sprintf("skip Emulated master: need MySQL 5.6.5+, got %s", $link->server_version));
}

if (!$link = mst_mysqli_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket))
	die(sprintf("skip Cannot connect, [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

/* BEGIN READ ONLY exists since MySQL 5.6.5 */
if ($link->server_version < 50605) {
	die(sprintf("skip Emulated slave: need MySQL 5.6.5+, got %s", $link->server_version));
}
?>
--INI--
mysqlnd_ms.multi_master=1
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos_eventual_trx_stickiness_on_rr.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* master0, RW split */
	if (!$link->query("SET @myrole='master0'"))
		printf("[002] [%d] %s\n", $link->errno, $link->error);

	if (!$link->query("SET @myrole='master1'"))
		printf("[003] [%d] %s\n", $link->errno, $link->error);

	mst_mysqli_query(4, $link, "SET @myrole='slave0'", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(5, $link, "SET @myrole='slave1'", MYSQLND_MS_SLAVE_SWITCH);


	/* slave 0 */
	$link->begin_transaction(MYSQLI_TRANS_START_READ_ONLY);
	if ($res = mst_mysqli_query(4, $link, "SELECT @myrole AS _role"))
		var_dump($res->fetch_all());

	$link->commit();

	/* master 0 */
	$link->begin_transaction(MYSQLI_TRANS_START_READ_WRITE);
	if ($res = mst_mysqli_query(5, $link, "SELECT @myrole AS _role"))
		var_dump($res->fetch_all());

	$link->rollback();

	/* slave 1 */
	if ($res = mst_mysqli_query(6, $link, "SELECT @myrole AS _role"))
		var_dump($res->fetch_all());

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_qos_eventual_trx_stickiness_on_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos_eventual_trx_stickiness_on_rr.ini'.\n");

?>
--EXPECTF--
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(6) "slave0"
  }
}
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(7) "master0"
  }
}
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(6) "slave1"
  }
}
done!