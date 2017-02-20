--TEST--
Limits: autocommit - NOT handled by plugin before PHP 5.3.99
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

if (version_compare(PHP_VERSION, '5.3.99') < 0)
	die(sprintf("SKIP Requires PHP > 5.4.0, using " . PHP_VERSION));

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_limits_autocommit.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_limits_autocommit.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/*
	Note: link->autocommit is not handled by the plugin! Don't rely on it!
	*/

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!mysqli_autocommit($link, false))
		printf("[002] Failed to change autocommit setting\n");

	mst_mysqli_query(3, $link, "SET autocommit=0", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(4, $link, "SET autocommit=0", MYSQLND_MS_SLAVE_SWITCH);

	/* applied to master connection because master is the first one contacted by plugin and autocommit is not dispatched */
	if (!mysqli_autocommit($link, true))
		printf("[005] Failed to change autocommit setting\n");

	/* slave because SELECT */
	$res = mst_mysqli_query(6, $link, "SELECT @@autocommit AS auto_commit");
	$row = $res->fetch_assoc();
	if (1 != $row['auto_commit'])
		printf("[007] Autocommit should be on, got '%s'\n", $row['auto_commit']);

	/* master because of hint */
	$res = mst_mysqli_query(8, $link, "SELECT @@autocommit AS auto_commit", MYSQLND_MS_MASTER_SWITCH);
	$row = $res->fetch_assoc();
	if (1 != $row['auto_commit'])
		printf("[009] Autocommit should be on, got '%s'\n", $row['auto_commit']);

	$link->close();

	/* no plugin magic */

	if (!($link = mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket)))
		printf("[010] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!mysqli_autocommit($link, false))
		printf("[011] Failed to change autocommit setting\n");

	mst_mysqli_query(12, $link, "SET autocommit=0");

	if (!mysqli_autocommit($link, false))
		printf("[013] Failed to change autocommit setting\n");

	$res = mst_mysqli_query(14, $link, "SELECT @@autocommit AS auto_commit");
	$row = $res->fetch_assoc();
	if (0 != $row['auto_commit'])
		printf("[015] Autocommit should be off, got '%s'\n", $row['auto_commit']);

	if (!mysqli_autocommit($link, true))
		printf("[016] Failed to change autocommit setting\n");

	$res = mst_mysqli_query(17, $link, "SELECT @@autocommit AS auto_commit");
	$row = $res->fetch_assoc();
	if (1 != $row['auto_commit'])
		printf("[018] Autocommit should be on, got '%s'\n", $row['auto_commit']);

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_limits_autocommit.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_limits_autocommit.ini'.\n");
?>
--EXPECTF--
done!