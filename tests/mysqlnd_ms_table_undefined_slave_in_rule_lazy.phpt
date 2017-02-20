--TEST--
table filter basics: undefined slave, lazy
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
		),
		'slave' => array(
			 "slave1" => array(
				'host' 	=> $slave_host_only,
				'port' 	=> (double)$slave_port,
				'socket' 	=> $slave_socket,
			),
		),
		'lazy_connections' => 1,
		'filters' => array(
			"table" => array(
				"rules" => array(
					$db . ".test1%" => array(
						"master" => array("master1"),
						"slave" => array("slave2"),
					),
				),
			),
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_table_undefined_slave_in_rule_lazy.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_table_undefined_slave_in_rule_lazy.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* shall use host = forced_master_hostname_abstract_name from the ini file */
	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	} else {

		mst_mysqli_query(3, $link, "DROP TABLE IF EXISTS test1");
		$master = $link->thread_id;

		/* there is no slave to run this query... */
		if ($res = mst_mysqli_query(4, $link, "SELECT 'one' AS _id FROM test1")) {
			var_dump($res->fetch_assoc());
	  }
	  if ($link->thread_id == $master)
		  printf("[005] Master has replied to slave query\n");

	  if ($link->thread_id != 0)
		  printf("[006] Connected to some server, but which one?\n");

	  printf("[007] [%s/%d] %s\n", $link->sqlstate, $link->errno, $link->error);

	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_table_undefined_slave_in_rule_lazy.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_table_undefined_slave_in_rule_lazy.ini'.\n");
?>
--EXPECTF--
Fatal error: mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate slave connection. 0 slaves to choose from. Something is wrong in %s on line %d