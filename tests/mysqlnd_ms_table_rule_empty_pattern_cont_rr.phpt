--TEST--
table filter: rule with empty pattern (random)
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_check_feature(array("table_filter"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $master_host_only,
				'port' 		=> (int)$master_port,
				'socket' 	=> $master_socket,
			),
			"master2" => array(
				'host'		=> 'Master_Thursday_1540_pm_really',
			),
		),
		'slave' => array(
			"slave1" => array(
				'host' 	=> $slave_host_only,
				'port' 	=> (int)$slave_port,
				'socket' => $slave_socket,
			),
			"master2" => array(
				'host'		=> 'Slave_Thursday_1540_pm_really',
			),
		 ),
		'lazy_connections' => 1,
		'filters' => array(
			"table" => array(
				"rules" => array(
					"" => array(
					  "master" => array("master2"),
					  "slave" => array("slave2"),
					),
					"%" => array(
					  "master" => array("master1"),
					  "slave" => array("slave1"),
					),
				),
			),
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_table_rule_empty_pattern_cont_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_table_rule_empty_pattern_cont_rr.ini
mysqlnd_ms.multi_master=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	set_error_handler('mst_error_handler');

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* valid config or not? */
	mst_mysqli_verbose_query(2, $link, "DROP TABLE IF EXISTS test");
	if (0 == $link->thread_id)
		printf("[003] Not connected to any server.\n");

	mst_mysqli_verbose_query(4, $link, "SELECT 1");
	if (0 == $link->thread_id)
		printf("[005] Not connected to any server.\n");

	$stats = mysqlnd_ms_get_stats();
	foreach ($stats as $k => $v)
		if (0 != $v)
			  printf("[006] Stat '%s' = %d\n", $k, $v);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_table_rule_empty_pattern_cont_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_table_rule_empty_pattern_cont_rr.ini'.\n");
?>
--EXPECTF--
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) A table filter must be given a name. You must not use an empty string in %s on line %d
[001] [2000] (mysqlnd_ms) A table filter must be given a name. You must not use an empty string
[002 + 01] Query 'DROP TABLE IF EXISTS test'
[E_WARNING] mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong in %s on line %d
[002] [2000] (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong
[002 + 02] Thread '0'
[003] Not connected to any server.
[004 + 01] Query 'SELECT 1'
[E_WARNING] mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong in %s on line %d
[004] [2000] (mysqlnd_ms) Couldn't find the appropriate master connection. Something is wrong
[004 + 02] Thread '0'
[005] Not connected to any server.
done!