--TEST--
INI parser: anything that is not false is true
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"name_of_a_config_section" => array(
		'master' => array('forced_master_hostname_abstract_name'),
		'slave' => array('forced_slave_hostname_abstract_name'),
		'lazy_connections' => 0,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_ini_bool_true.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.force_config_usage="gurkensalat"
mysqlnd_ms.config_file=test_mysqlnd_ms_ini_bool_true.ini
--FILE--
<?php
	require_once("connect.inc");

	/*
	Error codes indicating connect failure provoked by non-existing host

	Error: 2002 (CR_CONNECTION_ERROR)
	Message: Can't connect to local MySQL server through socket '%s' (%d)
	Error: 2003 (CR_CONN_HOST_ERROR)
	Message: Can't connect to MySQL server on '%s' (%d)
	Error: 2005 (CR_UNKNOWN_HOST)
	Message: Unknown MySQL server host '%s' (%d)
	*/
	$mst_connect_errno_codes = array(
		2002 => true,
		2003 => true,
		2005 => true,
	);

	/* shall use host = forced_master_hostname_abstract_name from the ini file */
	$link = @mst_mysqli_connect("name_of_a_config_section", $user, $passwd, $db, $port, $socket);
	if (isset($mst_connect_errno_codes[mysqli_connect_errno()])) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	} else {
		printf("[001] Looks like the config has not been interpreted correctly [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_ini_bool_true.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_ini_bool_true.ini'.\n");
?>
--EXPECTF--
[001] [%d] %s
done!