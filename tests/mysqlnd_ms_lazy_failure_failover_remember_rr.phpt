--TEST--
Lazy,loop,remember failed, rr
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
		'pick' 	=> array('roundrobin'),
		'lazy_connections' => 1,
		'failover' => array('strategy' => 'loop_before_master', "remember_failed" => true, "max_retries" => 0),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_settings_lazy_failure_failover_remember_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.multi_master=1
mysqlnd_ms.config_file=test_mysqlnd_ms_settings_lazy_failure_failover_remember_rr.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$connections = array();
	echo "----\n";
	mst_compare_stats();
	echo "----\n";

	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH, true, true, false, version_compare(PHP_VERSION, '5.3.99', ">"));
	$connections[$link->thread_id] = array('master');
	echo "----\n";
	mst_compare_stats();
	echo "----\n";

	mst_mysqli_query(3, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	$connections[$link->thread_id] = array('master');
	echo "----\n";
	mst_compare_stats();
	echo "----\n";

	/* will fall back to the master */
	mst_mysqli_query(4, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH, true, true, false, version_compare(PHP_VERSION, '5.3.99', ">"));
	$connections[$link->thread_id][] = 'slave (fallback to master)';
	echo "----\n";
	mst_compare_stats();
	echo "----\n";


	/* will again fall back to the master */
	mst_mysqli_query(5, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);
	$connections[$link->thread_id][] = 'slave (fallback to master)';
	echo "----\n";
	mst_compare_stats();
	echo "----\n";

	mst_mysqli_fech_role(mst_mysqli_query(6, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role"));
	$connections[$link->thread_id][] = 'slave (fallback to master)';
	echo "----\n";
	mst_compare_stats();
	echo "----\n";

	mst_mysqli_fech_role(mst_mysqli_query(7, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role"));
	$connections[$link->thread_id][] = 'slave (fallback to master)';
	echo "----\n";
	mst_compare_stats();
	echo "----\n";

	foreach ($connections as $thread_id => $details) {
		printf("Connection %d -\n", $thread_id);
		foreach ($details as $msg)
		  printf("... %s\n", $msg);
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_settings_lazy_failure_failover_remember_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_settings_lazy_failure_failover_remember_rr.ini'.\n");
?>
--EXPECTF--
----
----
----
Stats use_master: 1
Stats use_master_sql_hint: 1
Stats lazy_connections_master_success: 1
Stats lazy_connections_master_failure: 1
----
----
Stats use_master: 2
Stats use_master_sql_hint: 2
----
----
Stats use_master: 3
Stats use_slave_sql_hint: 1
Stats lazy_connections_slave_failure: 2
----
----
Stats use_master: 4
Stats use_slave_sql_hint: 2
----
This is 'slave %d' speaking
----
Stats use_master: 5
Stats use_slave_guess: 1
----
This is 'slave %d' speaking
----
Stats use_master: 6
Stats use_slave_guess: 2
----
Connection %d -
... master
... slave (fallback to master)
... slave (fallback to master)
... slave (fallback to master)
... slave (fallback to master)
done!