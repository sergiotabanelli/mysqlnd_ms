--TEST--
Lazy,loop,remember failed, master only, random
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);

if (($master_host == $slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

$settings = array(
	"myapp" => array(
		'master' => array("unreachable:8033", $master_host),
		'slave' => array("unreachable:6033", "unreachable:7033"),
		'pick' 	=> array('random'),
		'lazy_connections' => 1,
		'failover' => array('strategy' => 'loop_before_master', "remember_failed" => true,	 "max_retries" => 0),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_lazy_failure_failover_remember_master_only_random.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.multi_master=1
mysqlnd_ms.disable_rw_split=1
mysqlnd_ms.config_file=test_mysqlnd_ms_lazy_failure_failover_remember_master_only_random.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$connections = array();

	for ($i = 0; $i < 10; $i++) {
		$res = mst_mysqli_query(5, $link, "SELECT CONNECTION_ID() AS _id");
		$row = $res->fetch_assoc();
		if (!isset($connections[$row['_id']])) {
			$connections[$row['_id']] = 1;
		} else {
			$connections[$row['_id']]++;
		}
	}

	foreach ($connections as $thread_id => $count)
		printf("%d: %d\n", $thread_id, $count);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_lazy_failure_failover_remember_master_only_random.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_lazy_failure_failover_remember_master_only_random.ini'.\n");
?>
--EXPECTF--

Warning: mysqli::query(): php_network_getaddresses: getaddrinfo failed: %s in %s on line %A
%d: 10
done!