--TEST--
Filter: node_groups, invalid group name, empty group
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
			"slave2" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket,
			),
		 ),

		'lazy_connections' => 0,
		'filters' => array(
			"node_groups" => array(
				"\0" => array(
					'master' => array('master1'),
					'slave' => array('slave1'),
				),
				"\n" => array(
					'master' => array(),
					'slave' => array('slave1'),
				),
				"A\n" => array(
					'master' => array('master1'),
					'slave' => array('slave1'),
				),
			),
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_groups_setup_mess.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.multi_master=0
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_groups_setup_mess.ini
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
	mst_mysqli_query(4, $link, "SET @myrole='slave2'", MYSQLND_MS_SLAVE_SWITCH);

	$res = mst_mysqli_query(5, $link, "SELECT 5, @myrole AS _role", MYSQLND_MS_MASTER_SWITCH);
	if (!$res) {
		printf("[006] [%d] %s\n", $link->errno, $link->error);
	} else {
		var_dump($res->fetch_assoc());
	}

	$res = mst_mysqli_query(7, $link, "SELECT 7, @myrole AS _role");
	if (!$res) {
		printf("[008] [%d] %s\n", $link->errno, $link->error);
	} else {
		var_dump($res->fetch_assoc());
	}
	$res = mst_mysqli_query(9, $link, "SELECT 9, @myrole AS _role");
	if (!$res) {
		printf("[010] [%d] %s\n", $link->errno, $link->error);
	} else {
		var_dump($res->fetch_assoc());
	}

	$res = mst_mysqli_query(11, $link, "/*\0*/SELECT 11, @myrole AS _role", MYSQLND_MS_MASTER_SWITCH);
	if (!$res) {
		printf("[012] [%d] %s\n", $link->errno, $link->error);
	} else {
		var_dump($res->fetch_assoc());
	}

	$res = mst_mysqli_query(13, $link, "/*\0*/SELECT 13, @myrole AS _role");
	if (!$res) {
		printf("[014] [%d] %s\n", $link->errno, $link->error);
	} else {
		var_dump($res->fetch_assoc());
	}

	$res = mst_mysqli_query(15, $link, "/*\n*/SELECT 15, @myrole AS _role");
	if (!$res) {
		printf("[016] [%d] %s\n", $link->errno, $link->error);
	} else {
		var_dump($res->fetch_assoc());
	}

	$res = mst_mysqli_query(17, $link, "/*A\n*/SELECT 17, @myrole AS _role");
	if (!$res) {
		printf("[018] [%d] %s\n", $link->errno, $link->error);
	} else {
		var_dump($res->fetch_assoc());
	}


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_groups_setup_mess.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_groups_setup_mess.ini'.\n");
?>
--EXPECTF--
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) No masters configured in node group '
' for 'node_groups' filter. Please, verify the setup in %s on line %d
[001] [2000] (mysqlnd_ms) No masters configured in node group %A
array(2) {
  [5]=>
  string(1) "5"
  ["_role"]=>
  string(7) "master1"
}
array(2) {
  [7]=>
  string(1) "7"
  ["_role"]=>
  string(6) "slave1"
}
array(2) {
  [9]=>
  string(1) "9"
  ["_role"]=>
  string(6) "slave2"
}
array(2) {
  [11]=>
  string(2) "11"
  ["_role"]=>
  string(7) "master1"
}
array(2) {
  [13]=>
  string(2) "13"
  ["_role"]=>
  string(6) "slave1"
}
array(2) {
  [15]=>
  string(2) "15"
  ["_role"]=>
  string(6) "slave1"
}
array(2) {
  [17]=>
  string(2) "17"
  ["_role"]=>
  string(6) "slave1"
}
done!