--TEST--
mysqlnd_ms_set_qos(), GTID SQL parsing
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

if (($master_host == $slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

include_once("util.inc");
$sql = mst_get_gtid_sql($db);
if ($error = mst_mysqli_setup_gtid_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
  die(sprintf("SKIP Failed to setup GTID on master, %s\n", $error));


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
			"slave2" => array(
				'host' 	=> $slave_host_only,
				'port' 	=> (int)$slave_port,
				'socket' => $slave_socket,
			),
		 ),

		'lazy_connections' => 1,

		'global_transaction_id_injection' => array(
			'on_commit'	 				=> $sql['update'],
			'fetch_last_gtid'			=> $sql['fetch_last_gtid'],
			'check_for_gtid'			=> "\n'#GTID'" . $sql['check_for_gtid'],
			'report_error'				=> true,
		),

	),

);
if ($error = mst_create_config("test_mysqlnd_ms_set_qos_gtid_sql_parsing2.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_set_qos_gtid_sql_parsing2.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	if (!$link->query("DROP TABLE IF EXISTS test") ||
		!$link->query("CREATE TABLE test(id INT)") ||
		!$link->query("INSERT INTO test(id) VALUES (1)"))
		printf("[002] [%d] %s\n", $link->errno, $link->error);

	if (false === ($gtid = mysqlnd_ms_get_last_gtid($link)))
		printf("[003] [%d] %s\n", $link->errno, $link->error);

	/* GTID */
	if (true !== ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_SESSION, MYSQLND_MS_QOS_OPTION_GTID, $gtid))) {
		printf("[004] [%d] %s\n", $link->errno, $link->error);
	}

	if ($res = mst_mysqli_query(6, $link, "SELECT id FROM test"))
		var_dump($res->fetch_all());

	printf("[007] [%d] '%s'\n", $link->errno, $link->error);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_set_qos_gtid_sql_parsing2.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_set_qos_gtid_sql_parsing2.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
		printf("[clean] %s\n", $error);

	if ($error = mst_mysqli_drop_gtid_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
		printf("[clean] %s\n", $error);
?>
--EXPECTF--
Warning: mysqli::query(): (mysqlnd_ms) SQL error while checking slave for GTID: 1064/'%s' in %s on line %d

Warning: mysqli::query(): (mysqlnd_ms) SQL error while checking slave for GTID: 1064/'%s' in %s on line %d
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
}
[007] [0] ''
done!