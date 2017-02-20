--TEST--
Manual failover, unknown slave host
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
		'master' 	=> array($emulated_master_host),
		'slave' 	=> array($emulated_slave_host, $emulated_slave_host, $emulated_slave_host),
		'pick' 		=> array("roundrobin"),
		'failover' => array('strategy' => 'master'),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_failover_killed.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[1,2,3]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_failover_killed.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function my_mysqli_query($offset, $link, $query) {
		global $mst_connect_errno_codes;

		if (!($res = @$link->query($query))) {
			printf("[%03d + 01] [%d] %s\n", $offset, $link->errno, $link->error);
			return 0;
		}

		if (!is_object($res)) {
			return $link->thread_id;
		}

		if (!($row = @$res->fetch_assoc())) {
			printf("[%03d + 03] [%d] %s\n", $offset, $link->errno, $link->error);
			return 0;
		}
		return @mst_mysqli_get_emulated_id($offset, $link);
	}

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (0 !== mysqli_connect_errno())
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$threads = array(
		'master' 	=> array(),
		'slave 1' 	=> array(),
		'slave 2' 	=> array(),
		'slave 3' 	=> array(),
	);

	$threads['master'][my_mysqli_query(10, $link, "DROP TABLE IF EXISTS test")] = 1;
	$threads['slave 1'][my_mysqli_query(20, $link, "SELECT 'Slave 1' AS msg")] = 1;
	$threads['slave 2'][my_mysqli_query(30, $link, "SELECT 'Slave 2' AS msg")] = 1;
	$thread_id = $link->thread_id;
	$threads['slave 3'][my_mysqli_query(40, $link, "SELECT 'Slave 3' AS msg")] = 1;

	$link->kill($thread_id);

	$threads['slave 1'][my_mysqli_query(50, $link, "SELECT 'Slave 1' AS msg")]++;
	$threads['slave 2'][my_mysqli_query(60, $link, "SELECT 'Slave 2' AS msg")] = 1;
	$threads['slave 3'][my_mysqli_query(70, $link, "SELECT 'Slave 3' AS msg")]++;

	$threads['slave 1'][my_mysqli_query(80, $link, "SELECT 'Slave 1' AS msg")]++;
	$threads['slave 2'][my_mysqli_query(90, $link, "SELECT 'Slave 2' AS msg")]++;
	$threads['slave 3'][my_mysqli_query(100, $link, "SELECT 'Slave 3' AS msg")]++;

	foreach ($threads['slave 2'] as $thread_id => $num_queries) {
		printf("Slave 2, %d\n", $thread_id);
		if (isset($threads['slave 1'][$thread_id])) {
			printf("[201] Slave 2 is Slave 1 ?!\n");
			var_dump($threads);
		}
		if (isset($threads['slave 3'][$thread_id])) {
			printf("[202] Slave 2 is Slave 3 ?!\n");
			var_dump($threads);
		}
		if (isset($threads['master'][$thread_id])) {
			printf("[203] Slave 2 is the Master ?!\n");
			var_dump($threads);
		}

	}
	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_failover_killed.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_failover_killed.ini'.\n");
?>
--EXPECTF--
[060 + 01] [2006] MySQL server has gone away
[090 + 01] [2006] MySQL server has gone away
Slave 2, %d
Slave 2, 0
done!