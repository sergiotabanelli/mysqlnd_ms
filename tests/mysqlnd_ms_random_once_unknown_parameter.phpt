--TEST--
LB random once: unknown parameter
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
				'socket' => $emulated_slave_socket,
			),
		 ),
		'lazy_connections' => 1,
		'filters' => array(
			"random" => array(
				"please" => "warn me",
				"sticky" => 1,
				"" => "please",
				"\n" => "SOS"
			),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_random_once_unknown_parameter.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

include_once("util.inc");
msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_random_once_unknown_parameter.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	mst_mysqli_query(2, $link, "DROP TABLE IF EXISTS test");
	$server_id = mst_mysqli_get_emulated_id(3, $link);
	if (is_null($server_id))
		printf("[004] Which server has run this?");

	$last_server_id = NULL;
	for ($i = 0; $i < 10; $i++) {
		mst_mysqli_query(5, $link, "SELECT 1 FROM DUAL");
		$server_id = mst_mysqli_get_emulated_id(6, $link);
		if (!is_null($last_server_id) && ($last_server_id != $server_id)) {
			printf("[007] Connection switch from thread %s to %s\n",
				$last_server_id, $server_id);
		}

		$last_server_id = $server_id;
	}

	if (is_null($server_id))
		printf("[008] Which server has run this?");

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_random_once_unknown_parameter.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_random_once_unknown_parameter.ini'.\n");
?>
--EXPECTF--
done!