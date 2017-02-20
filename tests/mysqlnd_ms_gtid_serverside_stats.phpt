--TEST--
Server GTID and emulation stats
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

include_once("util.inc");
$ret = mst_mysqli_server_supports_gtid($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
if (is_string($ret))
	die(sprintf("SKIP Failed to check if server has built-in GTID support, %s\n", $ret));

if (true != $ret)
	die(sprintf("SKIP Server has no built-in GTID support (want MySQL 5.6.16+)"));

$ret = mst_is_slave_of($slave_host_only, $slave_port, $slave_socket, $master_host_only, $master_port, $master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true != $ret)
	die("SKIP Configured master and slave seem not to be part of a replication cluster\n");

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $master_host_only,
				'port' 		=> (int)$master_port,
				'socket' 	=> $master_socket,
			),
		),
		'slave' => array(
			"slave1" => array(
				'host' 	=> $slave_host_only,
				'port' 	=> (int)$slave_port,
				'socket' => $slave_socket,
			),
		),

		'global_transaction_id_injection' => array(
			'select'					=> "SELECT @@GLOBAL.GTID_EXECUTED",
			"fetch_last_gtid"			=> "SELECT @@GLOBAL.GTID_EXECUTED AS trx_id FROM DUAL",
			"check_for_gtid"			=> "SELECT GTID_SUBSET('#GTID', @@GLOBAL.GTID_EXECUTED) AS trx_id FROM DUAL",
			'report_errors'				=> true,
		),

		'lazy_connections' => 1,
		'trx_stickiness' => 'disabled',
		'filters' => array(
			"random" => array("sticky" => 1),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_gtid_serverside_stats.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_serverside_stats.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	/* we need an extra non-MS link for checking GTID. If we use MS link, the check itself will change GTID */
	$master_link = mst_mysqli_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);

	$stats_before = mysqlnd_ms_get_stats();

	mst_mysqli_query(4, $link, "SET @myrole = 'Master'");
	mst_mysqli_query(5, $link, "SET @myrole = 'Slave'", MYSQLND_MS_SLAVE_SWITCH);

	mst_mysqli_query(6, $link, "DROP TABLE IF EXISTS test");
	mst_mysqli_query(7, $link, "CREATE TABLE test(id INT) ENGINE=InnoDB");
	mst_mysqli_query(8, $link, "INSERT INTO test(id) VALUES(1)");


	$gtid = mysqlnd_ms_get_last_gtid($link);

	printf("GTID from MySQL '%s'\n", preg_replace("@\s@", ".", $gtid));

	/* Session consistency (read your writes): try to read from slaves not only master */
	if (false == mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_SESSION, MYSQLND_MS_QOS_OPTION_GTID, $gtid)) {
		printf("[009] [%d] %s\n", $link->errno, $link->error);
	}

	/* Either run on master or a slave which has replicated the INSERT */
	$res = mst_mysqli_query(10, $link, "SELECT 1 AS id, @myrole AS _role FROM DUAL");
	var_dump($res->fetch_assoc());

	$stats_after = mysqlnd_ms_get_stats();
	foreach ($stats_after as $k => $v) {
		if ((substr($k, 0, 4) == "gtid") && ($v != $stats_before[$k])) {
			printf("[011] GTID emulation stat '%s' has changed from '%s' to '%s'\n",
				$k, $stats_before[$k], $v);
		}
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_serverside_stats.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_serverside_stats.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
		printf("[clean] %s\n");
?>
--EXPECTF--
GTID from MySQL '%s'
array(2) {
  ["id"]=>
  string(1) "1"
  ["_role"]=>
  string(5) "%s"
}
done!
