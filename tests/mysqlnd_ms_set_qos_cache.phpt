--TEST--
mysqlnd_ms_set_qos(), max age/lag w. TTL cache
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
_skipif_check_extensions(array("mysqlnd_qc"));
if (!defined("MYSQLND_MS_HAVE_CACHE_SUPPORT")) {
	die("SKIP Cache support not compiled in");
}

_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

include_once("util.inc");

$ret = mst_is_slave_of($slave_host_only, $slave_port, $slave_socket, $master_host_only, $master_port, $master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (false == $ret)
	die("SKIP Configured master and slave might not be part of a replication cluster\n");

$lag = mst_mysqli_get_slave_lag($slave_host_only, $user, $passwd, $db, (int)$slave_port, $slave_socket);
if (is_string($lag)) {
	die(sprintf("SKIP %s\n", $lag));
}

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

		'lazy_connections' => 1,
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_set_qos_cache.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_set_qos_cache.ini
apc.use_request_time=0
mysqlnd_qc.use_request_time=0
mysqlnd_qc.collect_statistics=1
mysqlnd_qc.ignore_sql_comments=1
--FILE--
<?php
	/* Caution: any test setting on replication is prone to false positive. Replication may be down! */

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

	/* Test relies on replication, try to reduce false-positives */
	do {
		$lag = mst_mysqli_get_slave_lag($slave_host_only, $user, $passwd, $db, (int)$slave_port, $slave_socket);
		if (is_string($lag)) {
			printf("[003] Caution, false positive - %s\n", $lag);
			$lag = 0;
		}
	} while ($lag > 0);
	/* slave may still be outdated, still possible to get false-positive */

	$attempts = 0;
	while ($attempts < 10) {
		if ($res = mst_mysqli_query(4, $link, "SELECT id FROM test")) {
			if ($res->num_rows == 0) {
				continue;
			}
			break;
		}
		$attempts++;
		sleep(1);
	}
	var_dump($res->fetch_all());
	if (!$res || ($res->num_rows == 0))
		printf("[005] Caution, false positive possible, slave may not be up to date\n");

	if (true !== ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL, MYSQLND_MS_QOS_OPTION_CACHE, 4))) {
		printf("[006] [%d] %s\n", $link->errno, $link->error);
	}

	dump_put_hit();

	/* Should be served from cache */
	if ($res = mst_mysqli_query(7, $link, "SELECT id FROM test")) {
		var_dump($res->fetch_all());
	}

	dump_put_hit();

	/*
		Note: Its not save to rely on the DELETE being executed by the slave
		before we do the next read. We must rely on the stats. This is
		just an extra little thingie on top which may cause a fail if stats
		are wrong.
     */
	mst_mysqli_query(8, $link, "DELETE FROM test");
	usleep(100000);

	/* Should be served from cache */
	if ($res = mst_mysqli_query(9, $link, "SELECT id FROM test")) {
		var_dump($res->fetch_all());
	}

	dump_put_hit();

	printf("[010] [%d] '%s'\n", $link->errno, $link->error);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_set_qos_cache.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_set_qos_cache.ini'.\n");

	require_once("connect.inc");
	require_once("util.inc");
	if ($error = mst_mysqli_drop_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
		printf("[clean] %s\n", $error);

	if ($error = mst_mysqli_drop_gtid_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
		printf("[clean] %s\n", $error);
?>
--EXPECTF--
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
}
cache_put 0
cache_hit 0
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
}
cache_put 1
cache_hit 0
array(1) {
  [0]=>
  array(1) {
    [0]=>
    string(1) "1"
  }
}
cache_put 1
cache_hit 1
[010] [0] ''
done!