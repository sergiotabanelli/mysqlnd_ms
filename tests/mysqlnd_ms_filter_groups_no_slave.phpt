--TEST--
Filter: node_groups, no slave
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

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
		'filters' => array(
			"node_groups" => array(
				"A" => array(
					'master' => array('master1'),
				),
			),
			"random" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_groups_no_slave.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.multi_master=0
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_groups_no_slave.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	set_error_handler('mst_error_handler');

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* mark all connections, masters first, we use round robin */
	mst_mysqli_query(2, $link, "SET @myrole='master1'");
	mst_mysqli_query(3, $link, "SET @myrole='slave1'", MYSQLND_MS_SLAVE_SWITCH);

	$res = mst_mysqli_query(4, $link, "SELECT @myrole AS _role", MYSQLND_MS_MASTER_SWITCH);
	if (!$res) {
		printf("[005] [%d] %s\n", $link->errno, $link->error);
	} else {
		var_dump($res->fetch_assoc());
	}

	$res = mst_mysqli_query(6, $link, "SELECT @myrole AS _role");
	if (!$res) {
		printf("[007] [%d] %s\n", $link->errno, $link->error);
	} else {
		var_dump($res->fetch_assoc());
	}

	$res = mst_mysqli_query(8, $link, "/*A*/SELECT @myrole AS _role", MYSQLND_MS_SLAVE_SWITCH);
	if (!$res) {
		printf("[009] [%d] %s\n", $link->errno, $link->error);
	} else {
		var_dump($res->fetch_assoc());
	}

	$res = mst_mysqli_query(10, $link, "/*A*/SELECT @myrole AS _role");
	if (!$res) {
		printf("[011] [%d] %s\n", $link->errno, $link->error);
	} else {
		var_dump($res->fetch_assoc());
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_groups_no_slave.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_groups_no_slave.ini'.\n");
?>
--EXPECTF--
array(1) {
  ["_role"]=>
  string(7) "master1"
}
array(1) {
  ["_role"]=>
  string(6) "slave1"
}
[E_WARNING] mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate slave connection. 0 slaves to choose from. Something is wrong in %s on line %d
[E_WARNING] mysqli::query(): (mysqlnd_ms) No connection selected by the last filter in %s on line %d
[008] [2000] (mysqlnd_ms) No connection selected by the last filter
[009] [2000] (mysqlnd_ms) No connection selected by the last filter
[E_WARNING] mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate slave connection. 0 slaves to choose from. Something is wrong in %s on line %d
[E_WARNING] mysqli::query(): (mysqlnd_ms) No connection selected by the last filter in %s on line %d
[010] [2000] (mysqlnd_ms) No connection selected by the last filter
[011] [2000] (mysqlnd_ms) No connection selected by the last filter
done!