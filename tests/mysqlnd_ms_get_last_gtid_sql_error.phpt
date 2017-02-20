--TEST--
mysqlnd_ms_get_last_gtid(), sql error
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
			'fetch_last_gtid'			=> 'Hi there!',
			'check_for_gtid'			=> $sql['check_for_gtid'],
			'report_error'				=> true,
		),

		'lazy_connections' => 1,
		'trx_stickiness' => 'disabled',
		'filters' => array(
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_get_last_gtid_sql_error.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_get_last_gtid_sql_error.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* commit, slave */
	if (!$link->autocommit(false))
		printf("[002] [%d] %s\n", $link->errno, $link->error);

	mst_mysqli_query(3, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_SLAVE_SWITCH);

	if (false !== ($ret = mysqlnd_ms_get_last_gtid($link))) {
		printf("[005] Expecting false, got %s\n", var_export($ret, true));
	} else {
		printf("[006] [%d] %s\n", $link->errno, $link->error);
	}

	mst_mysqli_query(7, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_LAST_USED_SWITCH);

	if (!$link->autocommit(false))
		printf("[009] [%d] %s\n", $link->errno, $link->error);

	/* autocommit, master */
	if (!$link->query("DROP TABLE IF EXISTS test"))
		printf("[010] [%d] %s\n", $link->errno, $link->error);

	if (false !== ($ret = mysqlnd_ms_get_last_gtid($link))) {
		printf("[011] Expecting false, got %s\n", var_export($ret, true));
	} else {
		printf("[012] [%d] %s\n", $link->errno, $link->error);
	}
	/* check if error on line */
	if (!$link->query("CREATE TABLE test(id INT)"))
		printf("[013] [%d] %s\n", $link->errno, $link->error);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_get_last_gtid_sql_error.ini"))
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_get_last_gtid_sql_error.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
		printf("[clean] %s\n");
?>
--EXPECTF--
[006] [1064] %s
[012] [1064] %s
done!