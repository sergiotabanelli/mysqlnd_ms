--TEST--
Fabric: select shard + state
--XFAIL--
No state alignment unlike promised by the manual - Emulator does not support dump commands
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");
require_once("util.inc");

if (!getenv("MYSQL_TEST_FABRIC")) {
	die(sprintf("SKIP Fabric - set MYSQL_TEST_FABRIC=1 (config.inc) to enable\n"));
}

_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

$tmp = mst_mysqli_test_for_charset($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
if ($tmp['error'] != '')
	die(sprintf("SKIP %s\n", $tmp['error']));

$emulated_master_charset = $tmp['charset'];

$tmp = mst_mysqli_test_for_charset($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);
if ($tmp['error'] != '')
	die(sprintf("SKIP %s\n", $tmp['error']));

$emulated_slave_charset = $tmp['charset'];

if ($emulated_master_charset != $emulated_slave_charset) {
	die(sprintf("SKIP Emulated master (%s) and emulated slave (%s) must use the same default charset.", $emulated_master_charset, $emulated_slave_charset));
}

if (($emulated_master_host_only == $emulated_slave_host_only) &&
	($emulated_master_port == $emulated_slave_port) &&
	($emulated_master_socket == $emulated_slave_socket)) {
	die("SKIP Emulated master and slave seem to be identical");
}

$process = ms_fork_emulated_fabric_server();
if (is_string($process)) {
	die(sprintf("SKIP %s\n", $process));
}
ms_emulated_fabric_server_shutdown($process);

$settings = array(
	"myapp" => array(
		'fabric' => array(
			'hosts' => array(
				array('host' => getenv("MYSQL_TEST_FABRIC_EMULATOR_HOST"), 'port' => getenv("MYSQL_TEST_FABRIC_EMULATOR_PORT"))
			),
			'timeout' => 2
		)
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_fabric_select_shard_state.ini", $settings))
  die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_fabric_select_shard_state.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");


	$process = ms_fork_emulated_fabric_server();
	if (is_string($process)) {
		printf("[001] %s\n", $process);
	}
	/* Give the system some breath time */
	sleep(1);

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (0 !== mysqli_connect_errno())
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (true !== ($ret = mysqlnd_ms_fabric_select_shard($link,  'fabric_sharding.test', 1))) {
		printf("[003] Expecting true, got %s\n", var_export($ret, true));
	}

	mst_mysqli_query(4, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(5, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);

	/* slave */
	if (!$res = mst_mysqli_query(4, $link, "SELECT @myrole AS _role, @@character_set_connection AS _charset", MYSQLND_MS_SLAVE_SWITCH))
		printf("[006] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	if ('slave' != $row['_role'])
		printf("[007] Expecting reply from slave not from '%s'\n", $row['_role']);

	$current_charset = $row['_charset'];
	$new_charset = ('latin1' == $current_charset) ? 'latin2' : 'latin1';

	/* shall be run on *all* configured machines - all masters, all slaves */
	if (!$link->set_charset($new_charset))
		printf("[008] [%d] %s\n", $link->errno, $link->error);

	/* master */
	if (!$res = mst_mysqli_query(9, $link, "SELECT @myrole AS _role, @@character_set_connection AS _charset", MYSQLND_MS_MASTER_SWITCH))
		printf("[010] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	if ('master' != $row['_role'])
		printf("[011] Expecting reply from master not from '%s'\n", $row['_role']);

	$current_charset = $row['_charset'];
	if ($current_charset != $new_charset)
		printf("[012] Expecting charset '%s' got '%s'\n", $new_charset, $current_charset);

	/* Swap out connections */
	if (true !== ($ret = mysqlnd_ms_fabric_select_shard($link,  'fabric_sharding.test', 1))) {
		printf("[013] Expecting true, got %s\n", var_export($ret, true));
	}

	if (!$res = mst_mysqli_query(14, $link, "SELECT @myrole AS _role, @@character_set_connection AS _charset", MYSQLND_MS_SLAVE_SWITCH))
		printf("[015] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();

	if ('' != $row['_role'])
		printf("[016] Expecting empty session variable, got'%s'\n", $row['_role']);

	$current_charset = $row['_charset'];
	if ($current_charset != $new_charset)
		printf("[017] No state alignment! Expecting charset '%s' got '%s'\n", $new_charset, $current_charset);


	if (!$res = mst_mysqli_query(18, $link, "SELECT @myrole AS _role, @@character_set_connection AS _charset", MYSQLND_MS_MASTER_SWITCH))
		printf("[019] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();

	if ('' != $row['_role'])
		printf("[020] Expecting empty session variable, got'%s'\n", $row['_role']);

	$current_charset = $row['_charset'];
	if ($current_charset != $new_charset)
		printf("[021] No state alignment! Expecting charset '%s' got '%s'\n", $new_charset, $current_charset);

	ms_emulated_fabric_server_shutdown($process);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_fabric_select_shard_state.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_fabric_select_shard_state.ini'.\n");
?>
--EXPECTF--
done!