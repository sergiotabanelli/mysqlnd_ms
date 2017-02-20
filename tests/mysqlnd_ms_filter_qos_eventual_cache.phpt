--TEST--
Filter QOS, eventual consistency, cache
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

_skipif_check_extensions(array("mysqli"));
_skipif_check_extensions(array("mysqlnd_qc"));
if (!defined("MYSQLND_MS_HAVE_CACHE_SUPPORT")) {
	die("SKIP Cache support not compiled in");
}

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $emulated_master_host_only,
				'port' 		=> (int)$emulated_master_port,
				'socket' 	=> $emulated_master_socket,
			),
		),
		'slave' => array(
			"slave1" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket,
			),
		 ),

		'lazy_connections' => 0,
		'filters' => array(
			"quality_of_service" => array(
				"eventual_consistency" => array(
					'cache' => 123
				),
			),
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos_eventual_cache.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

include_once("util.inc");
msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos_eventual_cache.ini
apc.use_request_time=0
mysqlnd_qc.use_request_time=0
mysqlnd_qc.collect_statistics=1
mysqlnd_qc.ttl=99
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

	if (!$link->query("DROP TABLE IF EXISTS test") ||
		!$link->query("CREATE TABLE test(id INT)") ||
		!$link->query("INSERT INTO test(id) VALUES (1)"))
		printf("[002] [%d] %s\n", $link->errno, $link->error);

	if (!mysqlnd_qc_set_storage_handler("default")) {
		printf("[003] Failed to switch to default QC handler\n");
	}

	/* TTL JSON vs. mysqlnd_qc.ttl */
	dump_put_hit();
	$now = time();

	/* Ignore repl errors such as slave not running. Result must be the same whatever server is used */
	if ($res = @mst_mysqli_query(4, $link, "SELECT id FROM test"))
		var_dump($res->fetch_all());

	printf("[004] [%d] '%s'\n", $link->errno, $link->error);

	dump_put_hit();


	/* Ignore repl errors such as slave not running. Result must be the same whatever server is used */
	if ($res = @mst_mysqli_query(4, $link, "SELECT id FROM test"))
		var_dump($res->fetch_all());

	printf("[005] [%d] '%s'\n", $link->errno, $link->error);

	$ignore = array();
	$info = mysqlnd_qc_get_cache_info();
	$entries = $info['data'];
	foreach ($entries as $key => $entry) {
		$ttl = $entry['statistics']['valid_until'] - $now;
		/* Allow some fuzzyness and accept TTL=100 (mysqlnd_qc.ttl + 1) although TTL should be 123 most of the time */
		if ($ttl <= 99) {
			printf("[006] Cache entry TTL is %d (expected: >= 123)\n", $ttl);
		}
		$ignore[$key] = true;
	}


	/* TTL API call vs. mysqlnd_qc.ttl */
	if (true !== ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL, MYSQLND_MS_QOS_OPTION_CACHE, 4))) {
		printf("[007] [%d] %s\n", $link->errno, $link->error);
	}
	$now = time();

	/* Ignore repl errors such as slave not running. Result must be the same whatever server is used */
	if ($res = @mst_mysqli_query(8, $link, "SELECT id FROM test WHERE id = 1"))
		var_dump($res->fetch_all());

	printf("[009] [%d] '%s'\n", $link->errno, $link->error);

	dump_put_hit();

	/* Ignore repl errors such as slave not running. Result must be the same whatever server is used */
	if ($res = @mst_mysqli_query(8, $link, "SELECT id FROM test WHERE id = 1"))
		var_dump($res->fetch_all());

	printf("[010] [%d] '%s'\n", $link->errno, $link->error);

	dump_put_hit();

	$info = mysqlnd_qc_get_cache_info();
	$entries = $info['data'];
	foreach ($entries as $key => $entry) {
		if (isset($ignore[$key]))
			continue;
		$ttl = $entry['statistics']['valid_until'] - $now;
		/* Allow some fuzzyness and accept TTL=5 although TTL should be 4 most of the time */
		if ($ttl > 5) {
			printf("[011] Cache entry TTL is %d (expected: <= 5)\n", $ttl);
		}
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_qos_eventual_cache.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos_eventual_cache.ini'.\n");
?>
--EXPECTF--
cache_put 0
cache_hit 0
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
}
[004] [0] ''
cache_put 1
cache_hit 0
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
}
[005] [0] ''
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
}
[009] [0] ''
cache_put 2
cache_hit 1
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
}
[010] [0] ''
cache_put 2
cache_hit 2
done!