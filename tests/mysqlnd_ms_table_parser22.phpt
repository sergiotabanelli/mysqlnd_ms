--TEST--
parser: SELECT X'4D7953514C'
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_check_feature(array("parser"));
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
				'port' 	=> (int)$emulated_slave_port,
				'socket' 	=> $emulated_slave_socket,
			),
		),
		'lazy_connections' => 0,
		'filters' => array(
		),
	),
);

if (_skipif_have_feature("table_filter")) {
	$settings['myapp']['filters']['table'] = array(
		"rules" => array(
			 "%" => array(
				  "master" => array("master1"),
				  "slave" => array("slave1"),
			),
		),
	);
}

if ($error = mst_create_config("test_mysqlnd_ms_table_parser22.ini", $settings))
	die(sprintf("SKIP %s\n", $error));


msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_table_parser22.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");


	$sql = "SELECT X'4D7953514C' AS _id FROM DUAL";
	if (mst_mysqli_server_supports_query(1, $sql, $emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket)) {

		$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
		if (mysqli_connect_errno())
			printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

		mst_mysqli_query(3, $link, "SELECT 1 FROM test", MYSQLND_MS_SLAVE_SWITCH);
		$emulated_slave_id = mst_mysqli_get_emulated_id(4, $link);

		mst_mysqli_fetch_id(6, mst_mysqli_query(5, $link, $sql));
		$server_id = mst_mysqli_get_emulated_id(7, $link);
		if ($emulated_slave_id != $server_id)
			printf("[008] Statement has not been executed on the slave\n");

	} else {
		/* fake result */
		printf("[006] _id = 'MySQL'\n");
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_table_parser22.ini"))
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_table_parser22.ini'.\n");
?>
--EXPECTF--
[001] Testing server support of 'SELECT X'4D7953514C' AS _id FROM DUAL'
[006] _id = 'MySQL'
done!