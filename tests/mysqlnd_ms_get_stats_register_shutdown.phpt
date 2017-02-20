--TEST--
mysqlnd_ms_get_stats() + register_shutdown
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
		'lazy_connections' => 0
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_get_stats_register_shutdown.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_get_stats_register_shutdown.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function check_stats() {
		global $exp_stats;
		$stats = mysqlnd_ms_get_stats();
		foreach ($stats as $k => $v) {
			if ($exp_stats[$k] != $v) {
				printf("[005] Expecting %s = '%s', got '%s'\n", $k, $exp_stats[$k], $v);
			}
		}
		if (empty($stats)) {
			printf("[006] Statistics are empty, dumping\n");
			var_dump($stats);
		}
		printf("shutdown completed!\n");
	}
	register_shutdown_function("check_stats");

	/* some stats, not all... we are not much after checking the list */
	$expected = array(
		"use_slave" 							=> true,
		"use_master"							=> true,
		"use_slave_guess" 						=> true,
		"use_master_guess"						=> true,
		"use_slave_sql_hint"					=> true,
		"use_master_sql_hint"					=> true,
		"use_last_used_sql_hint" 				=> true,
		"use_slave_callback"					=> true,
		"use_master_callback"					=> true,
		"non_lazy_connections_slave_success"	=> true,
		"non_lazy_connections_slave_failure"	=> true,
		"non_lazy_connections_master_success"	=> true,
		"non_lazy_connections_master_failure"	=> true,
		"lazy_connections_slave_success"		=> true,
		"lazy_connections_slave_failure"		=> true,
		"lazy_connections_master_success"		=> true,
		"lazy_connections_master_failure"		=> true,
		"trx_autocommit_on"						=> true,
		"trx_autocommit_off"					=> true,
		"trx_master_forced"						=> true,
		"pool_masters_total"					=> true,
		"pool_slaves_total"						=> true,
		"pool_masters_active"					=> true,
		"pool_slaves_active"					=> true,
	);
	$stats = mysqlnd_ms_get_stats();
	$exp_stats = $stats;

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$exp_stats['non_lazy_connections_slave_success']++;
	$exp_stats['non_lazy_connections_master_success']++;
	$exp_stats['pool_masters_total']++;
	$exp_stats['pool_slaves_total']++;
	$exp_stats['pool_masters_active']++;
	$exp_stats['pool_slaves_active']++;

	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	$exp_stats['use_master_sql_hint']++;
	$exp_stats['use_master']++;

	$stats = mysqlnd_ms_get_stats();
	foreach ($stats as $k => $v) {
		if ($exp_stats[$k] != $v) {
			printf("[004] Expecting %s = '%s', got '%s'\n", $k, $exp_stats[$k], $v);
		}
	}

	print "done!\n";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_get_stats_register_shutdown.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_get_stats_register_shutdown.ini'.\n");
?>
--EXPECTF--
done!
shutdown completed!