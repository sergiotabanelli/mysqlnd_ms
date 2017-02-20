--TEST--
Empty filters section, lazy = 1
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));

_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

/* Emulated ID does not work with replication */
include_once("util.inc");
$ret = mst_is_slave_of($emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket, $emulated_master_host_only, $emulated_master_port, $emulated_master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true == $ret)
	die("SKIP Configured emulated master and emulated slave could be part of a replication cluster\n");

include_once("util.inc");
msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[1,2]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");

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

		'lazy_connections' => 1,
		'filters' => array(
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_empty_lazy.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_empty_lazy.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* shall use host = forced_master_hostname_abstract_name from the ini file */
	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	$threads = array();

	mst_mysqli_query(2, $link, "DROP TABLE IF EXISTS test");
	$threads[mst_mysqli_get_emulated_id(3, $link)] = array("master");

	$res = mst_mysqli_query(4, $link, "SELECT 1 FROM DUAL");
	$threads[mst_mysqli_get_emulated_id(5, $link)] = array("slave");
	if (!$res)
		printf("[006] [%d] %s\n", $link->errno, $link->error);

	$res = mst_mysqli_query(7, $link, "SELECT 1 FROM DUAL");
	$threads[mst_mysqli_get_emulated_id(8, $link)][] = "slave";
	if (!$res)
		printf("[009] [%d] %s\n", $link->errno, $link->error);


	foreach ($threads as $id => $roles) {
		printf("%s: ", $id);
		foreach ($roles as $role)
		  printf("%s\n", $role);
	}


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_empty_lazy.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_empty_lazy.ini'.\n");
?>
--EXPECTF--
master-%d: master
slave[1,2]-%d: slave
slave
done!