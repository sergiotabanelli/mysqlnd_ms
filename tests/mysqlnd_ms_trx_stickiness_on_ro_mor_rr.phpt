--TEST--
trx_stickiness=on, RO trx, master on write pick = RR
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
		'slave' => array($slave_host, $slave_host),
		'trx_stickiness' => 'on',
		'master_on_write' => 1,
		'pick' => array("roundrobin"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_trx_stickiness_on_ro_mor_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

if (!$link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
	die(sprintf("skip Cannot connect, [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

/* BEGIN READ ONLY exists since MySQL 5.6.5 */
if ($link->server_version < 50605) {
	die(sprintf("skip Emulated master: need MySQL 5.6.5+, got %s", $link->server_version));
}

?>
--INI--
mysqlnd_ms.multi_master=1
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_trx_stickiness_on_ro_mor_rr.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='master0'", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(3, $link, "SET @myrole='master1'", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(4, $link, "SET @myrole='slave0'", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(5, $link, "SET @myrole='slave1'", MYSQLND_MS_SLAVE_SWITCH);


	$link->begin_transaction(MYSQLI_TRANS_START_READ_ONLY);
	/* ro trx but master on write set, must use master! */
	$last = NULL;
	for ($i = 0; $i < 3; $i++) {
		if (!($res = mst_mysqli_query(5, $link, "SELECT @myrole AS _role"))) {
			printf("[006] [%d] %s\n", $link->errno, $link->error);
		}
		$row = $res->fetch_assoc();
		if (is_null($last)) {
			$last = $row['_role'];
		} else {
		    if ($last != $row['_role']) {
				printf("[007] Connection switch during transaction must not happen!\n");
				break;
		    }
		}
	}
	$link->commit();

	/* must use master 1  */
	if (!($res = mst_mysqli_query(8, $link, "SELECT @myrole AS _role"))) {
		printf("[009] [%d] %s\n", $link->errno, $link->error);
	} else {
		$row = $res->fetch_assoc();
		printf("%s\n", $row['_role']);
	}

	/* no select, no trx... master 0, please! */
	if (!($res = mst_mysqli_query(10, $link, "DROP TABLE IF EXISTS test"))) {
			printf("[011] [%d] %s\n", $link->errno, $link->error);
	}

	if (!($res = mst_mysqli_query(12, $link, "SELECT @myrole AS _role", MYSQLND_MS_LAST_USED_SWITCH))) {
		printf("[013] [%d] %s\n", $link->errno, $link->error);
	} else {
		$row = $res->fetch_assoc();
		printf("%s\n", $row['_role']);
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_trx_stickiness_on_ro_mor_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_trx_stickiness_on_ro_mor_rr.ini'.\n");
?>
--EXPECTF--
master1
master0
done!