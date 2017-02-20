--TEST--
trx_stickiness=on, failover, RR
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.4.99-dev', '<'))
	die(sprintf("SKIP Requires PHP 5.5.0 or newer, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host, "unknown:6033", $emulated_master_host),
		'slave' => array(),
		'trx_stickiness' => 'on',
		'pick' => array("roundrobin"),
		'lazy_connections' => 1,
		'failover' => array('strategy' => 'loop_before_master', 'remember_failed' => 1),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_trx_stickiness_no_slave_failover_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);


if (!$link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
	die(sprintf("skip Cannot connect, [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

/* BEGIN READ ONLY exists since MySQL 5.6.5 */
if ($link->server_version < 50605) {
	die(sprintf("skip Emulated master: need MySQL 5.6.5+, got %s", $link->server_version));
}
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.multi_master=1
mysqlnd_ms.config_file=test_mysqlnd_ms_trx_stickiness_no_slave_failover_rr.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	/* trying to hit the unavailable master */
	$i = 0;
	$servers = array();
	$last = NULL;
	do {

		$link->begin_transaction(MYSQLI_TRANS_START_READ_ONLY);
		/*
         This is the first query in the transaction,
         failover may be done to find an initial connection.
		 Once we have a connection, it must not change, ever!
		*/
		$res = $link->query("SELECT CONNECTION_ID() AS _slave_role");
		if (0 != $link->errno) {
			printf("[003] [%d] '%s'\n", $link->errno, $link->error);
			break;
		}
		$row = $res->fetch_assoc();
		if (!is_null($last)) {
			if ($last == $row['_slave_role']) {
				printf("[007] No switch between transactions or false positive, run again and decide?\n");
			}
		}

		$last = $row['_slave_role'];

		$res = $link->query("SELECT CONNECTION_ID() AS _slave_role");
		if (0 != $link->errno) {
			printf("[004] [%d] '%s'\n", $link->errno, $link->error);
			break;
		}
		$row = $res->fetch_assoc();
		if ($last != $row['_slave_role']) {
			printf("[005] Server switched in the middle of a transaction!\n");
			break;
		}
		$link->commit(TRUE);
		$servers[$last] = true;

	} while ((++$i < 5) && ($res) &&  (0 == $link->errno));
	printf("[006] %d - [%d] '%s'\n", $i, $link->errno, $link->error);
	printf("Number of servers contacted: %d\n", count($servers));

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_trx_stickiness_no_slave_failover_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_trx_stickiness_no_slave_failover_rr.ini'.\n");
?>
--EXPECTF--

Warning: mysqli::query(): php_network_getaddresses: getaddrinfo failed: %s in %s on line %d
[006] 5 - [0] ''
Number of servers contacted: 2
done!