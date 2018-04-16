--TEST--
GTID cache integration
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_check_extensions(array("mysqlnd_qc"));
if (!defined("MYSQLND_MS_HAVE_CACHE_SUPPORT")) {
	die("SKIP Cache support not compiled in");
}
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

include_once("util.inc");
$ret = mst_mysqli_server_supports_gtid($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
if (is_string($ret))
	die(sprintf("SKIP Failed to check if server has built-in GTID support, %s\n", $ret));

if (true != $ret)
	die(sprintf("SKIP Server has no built-in GTID support (want MySQL 5.6.16+)"));

$ret = mst_mysqli_server_supports_session_track_gtid($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
if (is_string($ret))
	die(sprintf("SKIP Failed to check if server support SESSION TRACK GTID, %s\n", $ret));

if (true != $ret)
	die(sprintf("SKIP Server has no SESSION TRACK GTID support (want MySQL 5.7.6+ and SESSION_TRACK_GTIDS=OWN_GTID)"));

$ret = mst_mysqli_server_supports_memcached_plugin($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
if (is_string($ret))
	die(sprintf("SKIP Failed to check if server support MEMCACHED plugin, %s\n", $ret));

if (true != $ret)
	die(sprintf("SKIP Server has no MEMCACHED plugin support (want MySQL 5.6.0+ and active daemon_memcached plugin)"));

$ret = mst_is_slave_of($slave_host_only, $slave_port, $slave_socket, $master_host_only, $master_port, $master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true != $ret)
	die("SKIP Configured master and slave seem not to be part of a replication cluster\n");

if ($error = mst_mysqli_setup_gtid_memcached($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
  die(sprintf("SKIP Failed to setup GTID memcached on emulated master, %s\n", $error));

$sql = mst_get_gtid_memcached($db);

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
		 	'type'						=> 2,
			'fetch_last_gtid'			=> $sql['fetch_last_gtid'],
			'report_error'				=> true,
			'memcached_host'			=> $emulated_master_host_only,
			'memcached_port'			=> $emulated_master_port + $memcached_port_add_hack,
			'memcached_key'				=> $sql['global_key'],
			),

		'lazy_connections' => 1,
		'trx_stickiness' => 'on',
		'filters' => array(
			"quality_of_service" => array(
				"session_consistency" => 1,
			),
			"roundrobin" => array(),
		),
	),

);
$settings['myapp1'] = $settings['myapp'];
$settings['myapp1']['global_transaction_id_injection']['qc_ttl'] = 10;
if ($error = mst_create_config("test_mysqlnd_ms_gtid_cache.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
if ($error = mst_mysqli_create_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
	die(sprintf("SKIP Failed to drop test table on master %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_gtid_cache.ini
apc.use_request_time=0
mysqlnd_qc.use_request_time=0
mysqlnd_qc.collect_statistics=1
mysqlnd_qc.ignore_sql_comments=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function dump_put_hit() {
		$stats = mysqlnd_qc_get_core_stats();
		printf("cache_put %d\n", $stats['cache_put']);
		printf("cache_hit %d\n", $stats['cache_hit']);
	}
	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	$link1 = mst_mysqli_connect("myapp1", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}	
	dump_put_hit();
	if ($res = mst_mysqli_query(1/*offset*/, $link, "/*qc=on*//*qc_ttl=10*/SELECT id FROM test")) {
		var_dump($res->fetch_all());
	}
	dump_put_hit();
	if ($res = mst_mysqli_query(2/*offset*/, $link, "/*qc=on*//*qc_ttl=10*/SELECT id FROM test")) {
		var_dump($res->fetch_all());
	}
	dump_put_hit();
	if ($res = mst_mysqli_query(3/*offset*/, $link1, "SELECT id FROM test")) {
		var_dump($res->fetch_all());
	}
	dump_put_hit();
	if ($res = mst_mysqli_query(4/*offset*/, $link1, "SELECT id FROM test")) {
		var_dump($res->fetch_all());
	}
	dump_put_hit();
	
	mst_mysqli_query(5/*offset*/, $link, "DELETE FROM test");
	if ($res = mst_mysqli_query(6/*offset*/, $link, "/*qc=on*//*qc_ttl=10*/SELECT id FROM test")) {
		var_dump($res->fetch_all());
	}
	dump_put_hit();
	if ($res = mst_mysqli_query(7/*offset*/, $link, "/*qc=on*//*qc_ttl=10*/SELECT id FROM test")) {
		var_dump($res->fetch_all());
	}
	dump_put_hit();
	
	mst_mysqli_query(5/*offset*/, $link1, "INSERT INTO test(id) VALUES (1)");
	if ($res = mst_mysqli_query(8/*offset*/, $link1, "SELECT id FROM test")) {
		var_dump($res->fetch_all());
	}
	dump_put_hit();
	if ($res = mst_mysqli_query(9/*offset*/, $link1, "SELECT id FROM test")) {
		var_dump($res->fetch_all());
	}
	dump_put_hit();

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_gtid_cache.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_gtid_cache.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
		printf("[clean] %s\n", $error);

	if ($error = mst_mysqli_drop_gtid_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
		printf("[clean] %s\n", $error);
?>
--EXPECTF--
cache_put 0
cache_hit 0
array(5) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
  [1]=>
  array(1) {
    [0]=>
    string(1) "2"
  }
  [2]=>
  array(1) {
    [0]=>
    string(1) "3"
  }
  [3]=>
  array(1) {
    [0]=>
    string(1) "4"
  }
  [4]=>
  array(1) {
    [0]=>
    string(1) "5"
  }
}
cache_put 1
cache_hit 0
array(5) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
  [1]=>
  array(1) {
    [0]=>
    string(1) "2"
  }
  [2]=>
  array(1) {
    [0]=>
    string(1) "3"
  }
  [3]=>
  array(1) {
    [0]=>
    string(1) "4"
  }
  [4]=>
  array(1) {
    [0]=>
    string(1) "5"
  }
}
cache_put 1
cache_hit 1
array(5) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
  [1]=>
  array(1) {
    [0]=>
    string(1) "2"
  }
  [2]=>
  array(1) {
    [0]=>
    string(1) "3"
  }
  [3]=>
  array(1) {
    [0]=>
    string(1) "4"
  }
  [4]=>
  array(1) {
    [0]=>
    string(1) "5"
  }
}
cache_put 2
cache_hit 1
array(5) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
  [1]=>
  array(1) {
    [0]=>
    string(1) "2"
  }
  [2]=>
  array(1) {
    [0]=>
    string(1) "3"
  }
  [3]=>
  array(1) {
    [0]=>
    string(1) "4"
  }
  [4]=>
  array(1) {
    [0]=>
    string(1) "5"
  }
}
cache_put 2
cache_hit 2
array(0) {
}
cache_put 3
cache_hit 2
array(0) {
}
cache_put 3
cache_hit 3
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
}
cache_put 4
cache_hit 3
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
}
cache_put 4
cache_hit 4
done!