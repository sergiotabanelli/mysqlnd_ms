--TEST--
mysqlnd_ms_set_qos(), trx stickiness=master
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.4.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.5.0, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));

include_once("util.inc");

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

		'trx_stickiness' => 'master',
		'lazy_connections' => 1,
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_set_qos_trx_stickiness_master.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

if (!$link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
	die(sprintf("skip Cannot connect, [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

/* BEGIN READ ONLY exists since MySQL 5.6.5 */
if ($link->server_version < 50605) {
	die(sprintf("skip Emulated master: need MySQL 5.6.5+, got %s", $link->server_version));
}

if (!$link = mst_mysqli_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket))
	die(sprintf("skip Cannot connect, [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

/* BEGIN READ ONLY exists since MySQL 5.6.5 */
if ($link->server_version < 50605) {
	die(sprintf("skip Emulated slave: need MySQL 5.6.5+, got %s", $link->server_version));
}
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_set_qos_trx_stickiness_master.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function fetch_change_fetch($offset, $link, $qos) {

		if (!($res = $link->query(sprintf("SELECT %d AS _id", $offset)))) {
			printf("[%03d] BEFORE [%d] '%s'\n", $offset, $link->errno, $link->error);
		} else {
			$row = $res->fetch_assoc();
			printf("[%03d] BEFORE id = %d\n", $offset, $row['_id']);
		}
		if (false == mysqlnd_ms_set_qos($link, $qos)) {
			printf("[%03d] [%d] '%s'\n", $offset, $link->errno, $link->error);
		}
		if (!($res = $link->query(sprintf("SELECT %d AS _id", $offset)))) {
			printf("[%03d] [%d] '%s'\n", $offset, $link->errno, $link->error);
		} else {
			printf("[%03d] AFTER [%d] '%s'\n", $offset, $link->errno, $link->error);
			$row = $res->fetch_assoc();
			printf("[%03d] AFTER id = %d\n", $offset, $row['_id']);
		}

	}

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}


	/* autocommit, qos changes allowed at any time */
	$link->autocommit(true);
	fetch_change_fetch(2, $link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL);

	/* autocommit off, in transaction, qos changes shall not be allowed */
	$link->autocommit(false);
	fetch_change_fetch(3, $link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL);
	fetch_change_fetch(4, $link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL);

	$link->autocommit(true);
	fetch_change_fetch(5, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);

	/* in transaction, forbid changes... */
	$link->begin_transaction();
	fetch_change_fetch(6, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);
	fetch_change_fetch(7, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);

	$link->begin_transaction();
	fetch_change_fetch(8, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);
	$link->commit();
	/* allow... */
	fetch_change_fetch(9, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);

	/* disallow... */
	$link->autocommit(false);
	$link->begin_transaction();
	fetch_change_fetch(10, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);
	$link->commit();
	fetch_change_fetch(11, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);

	/* allow.. */
	$link->autocommit(true);
	fetch_change_fetch(12, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);

	/* forbid... */
	$link->begin_transaction();
	fetch_change_fetch(13, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);
	$link->rollback();
	/* trx has ended - allow... */
	fetch_change_fetch(14, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);

	$link->begin_transaction();
	$link->autocommit(false);
	$link->commit();
	fetch_change_fetch(15, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);

	printf("[016] [%d] '%s'\n", $link->errno, $link->error);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_set_qos_trx_stickiness_master.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_set_qos_trx_stickiness_master.ini'.\n");
?>
--EXPECTF--
[002] BEFORE id = 2
[002] AFTER [0] ''
[002] AFTER id = 2
[003] BEFORE id = 3

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[003] [0] ''
[003] AFTER [0] ''
[003] AFTER id = 3
[004] BEFORE id = 4

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[004] [0] ''
[004] AFTER [0] ''
[004] AFTER id = 4
[005] BEFORE id = 5
[005] AFTER [0] ''
[005] AFTER id = 5
[006] BEFORE id = 6

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[006] [0] ''
[006] AFTER [0] ''
[006] AFTER id = 6
[007] BEFORE id = 7

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[007] [0] ''
[007] AFTER [0] ''
[007] AFTER id = 7
[008] BEFORE id = 8

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[008] [0] ''
[008] AFTER [0] ''
[008] AFTER id = 8
[009] BEFORE id = 9
[009] AFTER [0] ''
[009] AFTER id = 9
[010] BEFORE id = 10

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[010] [0] ''
[010] AFTER [0] ''
[010] AFTER id = 10
[011] BEFORE id = 11

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[011] [0] ''
[011] AFTER [0] ''
[011] AFTER id = 11
[012] BEFORE id = 12
[012] AFTER [0] ''
[012] AFTER id = 12
[013] BEFORE id = 13

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[013] [0] ''
[013] AFTER [0] ''
[013] AFTER id = 13
[014] BEFORE id = 14
[014] AFTER [0] ''
[014] AFTER id = 14
[015] BEFORE id = 15

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[015] [0] ''
[015] AFTER [0] ''
[015] AFTER id = 15
[016] [0] ''
done!