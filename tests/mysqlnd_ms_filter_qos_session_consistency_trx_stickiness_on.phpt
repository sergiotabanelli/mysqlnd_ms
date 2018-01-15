--TEST--
Filter QOS, session consistency, trx_stickiness=on
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.4.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.5.0, using " . PHP_VERSION));

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

include_once("util.inc");
$ret = mst_is_slave_of($emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket, $emulated_master_host_only, $emulated_master_port, $emulated_master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true == $ret)
	die("SKIP Configured emulated master and emulated slave could be part of a replication cluster\n");

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave1");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master1");


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
		'trx_stickiness' => 'on',

		'filters' => array(
			"quality_of_service" => array(
				"session_consistency" => 1,
			),
			"random" => array("sticky" => 1),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos_session_consistency_trx_stickiness_on.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos_session_consistency_trx_stickiness_on.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	mst_mysqli_query(2, $link, "SET @myrole='master'");
	$emulated_master_id = mst_mysqli_get_emulated_id(3, $link);

	$link->begin_transaction(MYSQLI_TRANS_START_READ_ONLY);

	mst_mysqli_query(4, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);
	$server_id = mst_mysqli_get_emulated_id(5, $link);
	if ($server_id == $emulated_master_id) {
		printf("[006] Expecting slave use, found %s used\n", $server_id);
	}

	if ($res = mst_mysqli_query(7, $link, "SELECT @myrole AS _msg")) {
		$row = $res->fetch_assoc();
		printf("Greetings from '%s'\n", $row['_msg']);
	} else {
		printf("[%d] %s\n", $link->errno, $link->error);
	}

	$server_id = mst_mysqli_get_emulated_id(8, $link);
	if ($server_id == $emulated_master_id) {
		printf("[009] Expecting slave use, found %s used\n", $server_id);
	}


	if (false === ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL)))
		printf("[010] [%d] '%s'\n", $link->errno, $link->error);

	mst_mysqli_query(12, $link, "SET @myrole='slave'");
	$server_id = mst_mysqli_get_emulated_id(15, $link);

	if ($server_id == $emulated_master_id) {
		printf("[016] Expecting slave use, found %s used\n", $server_id);
	}

	/* slave */
	if ($res = mst_mysqli_query(17, $link, "SELECT @myrole AS _msg")) {
		$row = $res->fetch_assoc();
		printf("Greetings from '%s'\n", $row['_msg']);
	} else {
		printf("[%d] %s\n", $link->errno, $link->error);
	}

	/* slave */
	mst_mysqli_query(19, $link, "SET @myrole='master'");

	if (false === ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_SESSION)))
		printf("[021] [%d] '%s'\n", $link->errno, $link->error);

	/* slave */
	if ($res = mst_mysqli_query(22, $link, "SELECT @myrole AS _msg")) {
		$row = $res->fetch_assoc();
		printf("Greetings from '%s'\n", $row['_msg']);
	} else {
		printf("[%d] %s\n", $link->errno, $link->error);
	}


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_qos_session_consistency_trx_stickiness_on.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos_session_consistency_trx_stickiness_on.ini'.\n");
?>
--EXPECTF--
Greetings from 'slave'

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[010] [0] ''
Greetings from 'slave'

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[021] [0] ''
Greetings from 'master'
done!