--TEST--
mysqlnd_ms_set_qos(), trx stickiness, RR
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
			"master2" => array(
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
			"slave2" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket,
			),

		 ),

		'trx_stickiness' => 'on',
		'lazy_connections' => 1,
		'filters' => array(
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_set_qos_trx_stickiness_on_rr.ini", $settings))
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
mysqlnd_ms.config_file=test_mysqlnd_ms_set_qos_trx_stickiness_on_rr.ini
mysqlnd_ms.multi_master=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function change_fetch($offset, $link, $qos) {

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

	function fetch_change_fetch($offset, $link, $qos) {

		if (!($res = $link->query(sprintf("SELECT %d AS _id", $offset)))) {
			printf("[%03d] BEFORE [%d] '%s'\n", $offset, $link->errno, $link->error);
		} else {
			$row = $res->fetch_assoc();
			printf("[%03d] BEFORE id = %d\n", $offset, $row['_id']);
		}
		change_fetch($offset, $link, $qos);
	}

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* autocommit, qos changes allowed at any time */
	$link->autocommit(true);
	fetch_change_fetch(2, $link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL);

	/* fool around with state, allowed... */
	$link->autocommit(false);
	$link->autocommit(true);
	change_fetch(3,  $link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL);

	/* autocommit off, in transaction, qos changes shall not be allowed */
	$link->autocommit(false);
	fetch_change_fetch(4, $link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL);
	fetch_change_fetch(5, $link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL);


	$link->autocommit(true);
	$link->autocommit(false);
	/* autocommit off but no query run before change, allowed (could forbid as well, makes no difference) */
	change_fetch(6, $link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL);

	$link->autocommit(true);
	$link->autocommit(false);
	/* query has been run, server has been picked, not allowed */
	fetch_change_fetch(7, $link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL);

	$link->autocommit(true);
	/* autocommit, allow */
	fetch_change_fetch(8, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);

	/* in transaction, forbid changes... */
	$link->begin_transaction();
	fetch_change_fetch(9, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);
	fetch_change_fetch(10, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);

	$link->begin_transaction();
	/* in trx, forbid */
	fetch_change_fetch(11, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);
	$link->commit();
	/* allow, autocommit is on. */
	fetch_change_fetch(12, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);

	/* disallow... */
	$link->autocommit(false);
	$link->begin_transaction();
	fetch_change_fetch(13, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);
	$link->commit();
	fetch_change_fetch(14, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);

	$link->commit();
	/* allow because no query has been run yet, we are in between transactions */
	change_fetch(15, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);

	/* allow.. */
	$link->autocommit(true);
	fetch_change_fetch(16, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);

	/* forbid... */
	$link->begin_transaction();
	fetch_change_fetch(17, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);
	$link->rollback();
	/* trx has ended - allow... */
	fetch_change_fetch(18, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);

	$link->begin_transaction();
	$link->autocommit(false);
	$link->commit();
	/* forbid, autocommit is off */
	fetch_change_fetch(19, $link, MYSQLND_MS_QOS_CONSISTENCY_SESSION);

	$link->commit();
	/* allow because no query has been run, we are in between transactions */
	change_fetch(20, $link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL);

	printf("[021] [%d] '%s'\n", $link->errno, $link->error);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_set_qos_trx_stickiness_on_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_set_qos_trx_stickiness_on_rr.ini'.\n");
?>
--EXPECTF--

[002] BEFORE id = 2
[002] AFTER [0] ''
[002] AFTER id = 2
[003] AFTER [0] ''
[003] AFTER id = 3
[004] BEFORE id = 4

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[004] [0] ''
[004] AFTER [0] ''
[004] AFTER id = 4
[005] BEFORE id = 5

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[005] [0] ''
[005] AFTER [0] ''
[005] AFTER id = 5
[006] AFTER [0] ''
[006] AFTER id = 6
[007] BEFORE id = 7

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[007] [0] ''
[007] AFTER [0] ''
[007] AFTER id = 7
[008] BEFORE id = 8
[008] AFTER [0] ''
[008] AFTER id = 8
[009] BEFORE id = 9

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[009] [0] ''
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

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[014] [0] ''
[014] AFTER [0] ''
[014] AFTER id = 14
[015] AFTER [0] ''
[015] AFTER id = 15
[016] BEFORE id = 16
[016] AFTER [0] ''
[016] AFTER id = 16
[017] BEFORE id = 17

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[017] [0] ''
[017] AFTER [0] ''
[017] AFTER id = 17
[018] BEFORE id = 18
[018] AFTER [0] ''
[018] AFTER id = 18
[019] BEFORE id = 19

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[019] [0] ''
[019] AFTER [0] ''
[019] AFTER id = 19
[020] AFTER [0] ''
[020] AFTER id = 20
[021] [0] ''
done!