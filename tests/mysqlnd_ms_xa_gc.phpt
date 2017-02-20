--TEST--
mysqlnd_ms_xa_gc()
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP Emulated master and emulated slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
	),

	"maxretries_type" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_xa_gc.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_gc.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* Parameter mess */

	if (NULL !== ($ret = @mysqlnd_ms_xa_gc())) {
		printf("[001] Expecting NULL, got %s\n", var_export($ret, true));
	}

	$xa_id = mt_rand(0, 1000);
	if (false !== ($ret = mysqlnd_ms_xa_gc($xa_id))) {
		printf("[002] Expecting false, got %s\n", var_export($ret, true));
	}

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[003] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (NULL !== ($ret = @mysqlnd_ms_xa_gc($link, $link))) {
		printf("[004] Expecting NULL, got %s\n", var_export($ret, true));
	}

	if (NULL !== ($ret = @mysqlnd_ms_xa_gc($link, $xa_id, array()))) {
		printf("[005] Expecting NULL, got %s\n", var_export($ret, true));
	}

	if (NULL !== ($ret = @mysqlnd_ms_xa_gc($link, $xa_id, true, "too_many"))) {
		printf("[006] Expecting NULL, got %s\n", var_export($ret, true));
	}

	$link->close();
	if (false !== ($ret = @mysqlnd_ms_xa_begin($link, $xa_id))) {
		printf("[007] Expecting false, got %s\n", var_export($ret, true));
	}
	/* Basics */

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[008] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	/* Without a state store, nothing happens... */
	var_dump(mysqlnd_ms_xa_gc($link));
	var_dump(mysqlnd_ms_xa_gc($link, $xa_id));
	var_dump(mysqlnd_ms_xa_gc($link, $xa_id, true));

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_xa_gc.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_gc.ini'.\n");
?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
done!