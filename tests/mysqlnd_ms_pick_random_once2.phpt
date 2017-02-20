--TEST--
Load Balancing: random_once (slaves)
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

include_once("util.inc");
$ret = mst_is_slave_of($emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket, $emulated_master_host_only, $emulated_master_port, $emulated_master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true == $ret)
	die("SKIP Configured emulated master and emulated slave could be part of a replication cluster\n");

$settings = array(
	"myapp" => array(
		'pick'		=> array('random' => array('sticky' => true)),
		'master' 	=> array($emulated_master_host),
		'slave' 	=> array($emulated_slave_host, $emulated_slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_random_once2.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

include_once("util.inc");
msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[1,2]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_random_once2.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());


	/* first master */
	mst_mysqli_query(2, $link, "SET @myrole = 'Master 1'", MYSQLND_MS_MASTER_SWITCH);
	$emulated_master = mst_mysqli_get_emulated_id(3, $link);

	$emulated_slaves = array();
	$num_queries = 100;
	for ($i = 0; $i <= $num_queries; $i++) {
		mst_mysqli_query(4, $link, "SELECT 1");
		$id = mst_mysqli_get_emulated_id(5, $link);
		if (!isset($emulated_slaves[$id])) {
			$emulated_slaves[$id] = array('role' => sprintf("Slave %d", count($emulated_slaves) + 1), 'queries' => 0);
		} else {
			$emulated_slaves[$id]['queries']++;
		}
	}

	foreach ($emulated_slaves as $thread => $details) {
		printf("%s (%s) has run %d queries.\n", $details['role'], $thread, $details['queries']);
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_random_once2.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_random_once2.ini'.\n");
?>
--EXPECTF--
Slave 1 (%s) has run 100 queries.
done!