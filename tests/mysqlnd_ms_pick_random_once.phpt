--TEST--
Load Balancing: random_once (slaves)
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

$settings = array(
	"myapp" => array(
		'pick'		=> array('random' => array('sticky' => '1')),
		'master' 	=> array($emulated_master_host),
		'slave' 	=> array($emulated_slave_host, $emulated_slave_host, $emulated_slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_random_once.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

include_once("util.inc");
msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[1,2,3]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_random_once.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function fetch_role($offset, $link, $switch = NULL) {
		$query = 'SELECT @myrole AS _role';
		if ($switch)
			$query = sprintf("/*%s*/%s", $switch, $query);

		$res = mst_mysqli_query($offset, $link, $query, $switch);
		if (!$res) {
			printf("[%03d +01] [%d] [%s\n", $offset, $link->errno, $link->error);
			return NULL;
		}

		$row = $res->fetch_assoc();
		return $row['_role'];
	}

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
		if ($id == $emulated_master)
			printf("[006] Master and slave use the same connection!\n");

		if (mt_rand(0, 10) > 9) {
			/* switch to master to check if next read goes to same slave */
			mst_mysqli_query(7, $link, "DROP TABLE IF EXISTS test");

		}
	}

	foreach ($emulated_slaves as $thread => $details) {
		printf("%s (%s) has run %d queries.\n", $details['role'], $thread, $details['queries']);
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_random_once.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_random_once.ini'.\n");
?>
--EXPECTF--
Slave 1 (%s) has run 100 queries.
done!