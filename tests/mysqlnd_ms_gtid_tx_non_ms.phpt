--TEST--
GTID - transactions in non MS connections
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

include_once("util.inc");
$sql = mst_get_gtid_sql($db);

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
			'on_commit'	 				=> $sql['update'],
		),

		'lazy_connections' => 1,
		'trx_stickiness' => 'disabled',
		'filters' => array(
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_gtid_tx_non_ms.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_tx_non_ms.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function get_autocommit_setting($offset, $link, $hint = NULL) {
		$res = mst_mysqli_query($offset, $link, "SELECT @@autocommit AS auto_commit", $hint);
		$row = $res->fetch_assoc();
		return $row['auto_commit'];
	}

	$link = mst_mysqli_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* make sure we run through the tx code with non ms connections */
	if (!get_autocommit_setting(3, $link))
		printf("[004] Autocommit should be on by default\n");

	if (!$link->autocommit(false))
		printf("[005] Failed to deactivate autocommit\n");

	if (get_autocommit_setting(6, $link))
		printf("[007] Autocommit should be off\n");

	if (!$link->autocommit(true))
		printf("[008] Failed to activate autocommit\n");

	if (!get_autocommit_setting(9, $link))
		printf("[010] Autocommit should be on\n");

	 if (!$link->autocommit(false))
		printf("[011] Failed to disable autocommit\n");

	if (get_autocommit_setting(12, $link))
		printf("[013] Autocommit should be off\n");

	if (!$link->query("DROP TABLE IF EXISTS test") ||
		!$link->query("CREATE TABLE test(id INT) ENGINE=InnoDB") ||
		!$link->query("INSERT INTO test(id) VALUES (1)") ||
		!$link->commit())
		printf("[014] [%d] %s\n", $link->errno, $link->error);

	$res = $link->query("SELECT * FROM test");
	var_dump($res->num_rows);

	$link->query("INSERT INTO test(id) VALUES (2)");

	$res = $link->query("SELECT * FROM test");
	var_dump($res->num_rows);

	$link->rollback();

	$res = $link->query("SELECT * FROM test");
	var_dump($res->num_rows);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_tx_non_ms.ini"))
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_tx_non_ms.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
		printf("[clean] %s\n");
?>
--EXPECTF--
int(1)
int(2)
int(1)
done!