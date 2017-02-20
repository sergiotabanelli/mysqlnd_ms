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

		'lazy_connections' => 0,
		'trx_stickiness' => 'on',

		'filters' => array(
			"quality_of_service" => array(
				"session_consistency" => 1,
			),
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos_session_consistency_trx_stickiness_on_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.multi_master=1
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos_session_consistency_trx_stickiness_on_rr.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	mst_mysqli_query(2, $link, "SET @myrole='master0'");
	mst_mysqli_query(3, $link, "SET @myrole='master1'");

	/* QoS rules out SQL hint, thus we have to relax QoS before we can set rol on slaves */
	if (false === ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL)))
		printf("[004] [%d] '%s'\n", $link->errno, $link->error);

	mst_mysqli_query(5, $link, "SET @myrole='slave0'", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(6, $link, "SET @myrole='slave1'", MYSQLND_MS_SLAVE_SWITCH);

	if (false === ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_SESSION)))
		printf("[007] [%d] '%s'\n", $link->errno, $link->error);


	$link->begin_transaction(MYSQLI_TRANS_START_READ_ONLY);
	/* master 0 */
	mst_mysqli_fech_role(mst_mysqli_query(8, $link, "SELECT @myrole AS _role"));

	/* reject change */
	if (false === ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL)))
		printf("[009] [%d] '%s'\n", $link->errno, $link->error);

    /* master 0, in transaction! */
	mst_mysqli_fech_role(mst_mysqli_query(10, $link, "SELECT @myrole AS _role"));
	$link->commit();

	/* relaxing should be allowed */
	if (false === ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL)))
		printf("[011] [%d] '%s'\n", $link->errno, $link->error);

	/* slave 0, slave 1, slave 0 */
	mst_mysqli_fech_role(mst_mysqli_query(12, $link, "SELECT @myrole AS _role"));
	mst_mysqli_fech_role(mst_mysqli_query(13, $link, "SELECT @myrole AS _role"));
	mst_mysqli_fech_role(mst_mysqli_query(14, $link, "SELECT @myrole AS _role"));

	if (false === ($ret = mysqlnd_ms_set_qos($link, MYSQLND_MS_QOS_CONSISTENCY_SESSION)))
		printf("[015] [%d] '%s'\n", $link->errno, $link->error);

	/* master 1 */
	mst_mysqli_fech_role(mst_mysqli_query(16, $link, "SELECT @myrole AS _role"));


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_qos_session_consistency_trx_stickiness_on_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos_session_consistency_trx_stickiness_on_rr.ini'.\n");
?>
--EXPECTF--
This is 'master0' speaking

Warning: mysqlnd_ms_set_qos(): (mysqlnd_ms) No change allowed in the middle of a transaction in %s on line %d
[009] [0] ''
This is 'master0' speaking
This is 'slave0' speaking
This is 'slave1' speaking
This is 'slave0' speaking
This is 'master1' speaking
done!