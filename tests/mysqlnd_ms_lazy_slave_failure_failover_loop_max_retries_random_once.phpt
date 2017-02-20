--TEST--
Lazy,loop,random once,max_retries
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
		'master' => array("unreachable:7033", $emulated_master_host),
		'slave' => array("unreachable:6033", $emulated_slave_host),
		'pick' 	=> array('random' => array('sticky' => '1')),
		'lazy_connections' => 1,
		'failover' => array('strategy' => 'loop_before_master', 'max_retries' => 1),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_lazy_slave_failure_failover_loop_max_retries_random_once.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_lazy_slave_failure_failover_loop_max_retries_random_once.ini
mysqlnd_ms.multi_master=1
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	for ($i = 0; $i < 10; $i++) {
		ob_start();
		$res = $link->query(sprintf("SELECT %d AS _run FROM DUAL", $i));
		$tmp = ob_get_contents();
		ob_end_clean();
		/* NOTE: it is ok to get a warning from the underlying API if connection fails */

		if ($res) {
			$row = $res->fetch_assoc();
			printf("%s %d,", mst_mysqli_get_emulated_id(2, $link), $row['_run']);
		} else {
			printf("no result %d,", $i);
		}
	}
	printf("\n");

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_lazy_slave_failure_failover_loop_max_retries_random_once.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_lazy_slave_failure_failover_loop_max_retries_random_once.ini'.\n");
?>
--EXPECTF--
slave-%d 0,slave-%d 1,slave-%d 2,slave-%d 3,slave-%d 4,slave-%d 5,slave-%d 6,slave-%d 7,slave-%d 8,slave-%d 9,
done!