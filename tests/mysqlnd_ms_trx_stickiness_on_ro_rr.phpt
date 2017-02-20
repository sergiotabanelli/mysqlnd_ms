--TEST--
trx_stickiness=on, RO trx, pick = random
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.4.99-dev', '<'))
	die(sprintf("SKIP Requires PHP 5.5.0 or newer, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host, $emulated_master_host),
		'slave' => array($emulated_slave_host, $emulated_slave_host),
		'trx_stickiness' => 'on',
		'pick' => array("roundrobin"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_trx_stickiness_on_ro_rr.ini", $settings))
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
mysqlnd_ms.config_file=test_mysqlnd_ms_trx_stickiness_on_ro_rr.ini
mysqlnd_ms.multi_master=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='master0'");
	mst_mysqli_query(3, $link, "SET @myrole='master1'");
	mst_mysqli_query(4, $link, "SET @myrole='slave0'", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(5, $link, "SET @myrole='slave1'", MYSQLND_MS_SLAVE_SWITCH);

	$link->begin_transaction(MYSQLI_TRANS_START_READ_ONLY);
	/* ro transaction can be run on slave */
	for ($i = 0; $i < 5; $i++) {
		if (!($res = mst_mysqli_query(5, $link, "SELECT @myrole AS _role"))) {
			printf("[006] [%d] %s\n", $link->errno, $link->error);
		}
		$row = $res->fetch_assoc();
		printf("Run on %s\n", $row['_role']);

		/* no select... but no master, please! */
		if (!($res = mst_mysqli_query(7, $link, sprintf("SET @msg='_%d'", $i)))) {
			printf("[008] [%d] %s\n", $link->errno, $link->error);
		}
	}
	$link->commit();

	/* slave */
	if (!($res = mst_mysqli_query(9, $link, "SELECT @myrole AS _role"))) {
		printf("[010] [%d] %s\n", $link->errno, $link->error);
	} else {
		$row = $res->fetch_assoc();
		printf("%s\n", $row['_role']);
	}

	/* no select... master, please! */
	if (!($res = mst_mysqli_query(11, $link, sprintf("SET @msg='_%d'", ++$i)))) {
			printf("[012] [%d] %s\n", $link->errno, $link->error);
	}
	mst_mysqli_fech_role(mst_mysqli_query(12, $link, "SELECT @myrole AS _role", MYSQLND_MS_LAST_USED_SWITCH));

	if (!($res = mst_mysqli_query(13, $link, "SELECT @msg AS _msg"))) {
		printf("[014] [%d] %s\n", $link->errno, $link->error);
	} else {
		$row = $res->fetch_assoc();
		printf("%s\n", $row['_msg']);
	}


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_trx_stickiness_on_ro_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_trx_stickiness_on_ro_rr.ini'.\n");
?>
--EXPECTF--
Run on slave0
Run on slave0
Run on slave0
Run on slave0
Run on slave0
slave1
This is 'master0' speaking
_4
done!