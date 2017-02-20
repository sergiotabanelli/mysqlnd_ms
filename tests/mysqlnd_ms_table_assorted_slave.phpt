--TEST--
table filter basics: leak
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");
if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_check_feature(array("table_filter"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

include_once("util.inc");
$ret = mst_is_slave_of($emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket, $emulated_master_host_only, $emulated_master_port, $emulated_master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true == $ret)
	die("SKIP Configured emulated master and emulated slave could be part of a replication cluster\n");

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
				'port' 	=> (double)$emulated_slave_port,
				'socket' 	=> $emulated_slave_socket,
			),
		),
		'lazy_connections' => 0,
 		'filters' => array(
			"table" => array(
				"rules" => array(
					$db . ".test1%" => array(
						"master" => array("master1"),
						"slave" => array("slave1"),
					),
				),
			),
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_table_assorted_slave.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_table_assorted_slave.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	} else {

		mst_mysqli_query(3, $link, "DROP TABLE IF EXISTS test1");
		$emulated_master = mst_mysqli_get_emulated_id(4, $link);

		/* there is no slave to run this query... */
		if ($res = mst_mysqli_query(5, $link, "SELECT 'one' AS _id FROM test1")) {
			var_dump($res->fetch_assoc());
		}
		$server_id = mst_mysqli_get_emulated_id(6, $link);
		if ($server_id == $emulated_master)
			printf("[007] Master has replied to slave query\n");

		if (!is_null($server_id))
		  printf("[008] Connected to some server, but which one?\n");

	  printf("[009] [%s/%d] %s\n", $link->sqlstate, $link->errno, $link->error);

	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_table_assorted_slave.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_table_assorted_slave.ini'.\n");
?>
--EXPECTF--
[009] [HY000/2002] Some meaningful message from mysqlnd_ms, e.g. some connect error
done!