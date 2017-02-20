--TEST--
INI setting: mysqlnd_ms.force_config_usage
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

	"192.168.14.17" => array(
		'master' => array('forced_master_hostname_ip'),
		'slave' => array('forced_slave_hostname_ip'),
		'lazy_connections' => 0,
	),

	"my_orginal_mysql_server_host" => array(
		'master' => array('forced_master_hostname_orgname'),
		'slave' => array('forced_slave_hostname_orgname'),
		 'lazy_connections' => 0,
	),

	"lazy_default_and_no_error" => array(
		'master' => array('forced_master_hostname_orgname'),
		'slave' => array('forced_slave_hostname_orgname'),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_ini_force_config.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.force_config_usage=1
mysqlnd_ms.config_file=test_mysqlnd_ms_ini_force_config.ini
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
		printf("[001] Is this a valid code? [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	$link = @mst_mysqli_connect("192.168.14.17", $user, $passwd, $db, $port, $socket);
	if (isset($mst_connect_errno_codes[mysqli_connect_errno()])) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	} else {
		printf("[002] Is this a valid code? [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	$link = @mst_mysqli_connect("my_orginal_mysql_server_host", $user, $passwd, $db, $port, $socket);
	if (isset($mst_connect_errno_codes[mysqli_connect_errno()])) {
		printf("[003] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	} else {
		printf("[003] Is this a valid code? [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	$link = mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket);
	if (0 == mysqli_connect_errno()) {
		/* 0 means no error. This is documented behaviour! */
		$res = $link->query("SELECT 'This connection should have been forbidden!'");
		$row = $res->fetch_row();
		printf("[004] %s\n", $row[0]);
	} else if (2000 == mysqli_connect_errno()) {
		/* Error: 2000 (CR_UNKNOWN_ERROR), HYOOO */
		printf("[005] Connection failed. The plugin can't set a specific error code as none exists, we go for unspecific code 2000 (CR_UNKNOWN_ERROR), [%d] %s\n",
			  mysqli_connect_errno(), mysqli_connect_error());
	} else {
		printf("[006] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	$link = @mst_mysqli_connect("lazy_default_and_no_error", $user, $passwd, $db, $port, $socket);
	if (isset($mst_connect_errno_codes[mysqli_connect_errno()])) {
		printf("[007] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	} else {
		printf("[008] No error because no connection yet [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* error is delayed due to lazy */
	@$link->query("DROP TABLE IF EXISTS test");
	if (isset($mst_connect_errno_codes[mysqli_errno($link)])) {
		printf("[009] [%d] %s\n", mysqli_errno($link), mysqli_error($link));
	} else {
		printf("[010] Unexpected error [%d] '%s'\n", mysqli_errno($link), mysqli_error($link));
	}

	/* error */
	$link = @mst_mysqli_connect("i_hope_there_is_no_such_host", $user, $passwd, $db, $port, $socket);
	if (isset($mst_connect_errno_codes[mysqli_connect_errno()])) {
		printf("[011] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	} else {
		printf("[011] Is this a valid code? [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_ini_force_config.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_ini_force_config.ini'.\n");
?>
--EXPECTF--
[001] [%d] %s
[002] [%d] %s
[003] [%d] %s

Warning: mysqli_real_connect(): (mysqlnd_ms) Exclusive usage of configuration enforced but did not find the correct INI file section (%s) in %s on line %d

Warning: mysqli_real_connect(): (HY000/2000): (mysqlnd_ms) Exclusive usage of configuration enforced but did not find the correct INI file section in %s on line %d
[005] Connection failed. The plugin can't set a specific error code as none exists, we go for unspecific code 2000 (CR_UNKNOWN_ERROR), [2000] (mysqlnd_ms) Exclusive usage of configuration enforced but did not find the correct INI file section
[008] No error because no connection yet [%d] ''
[009] [%d] %s
[011] Is this a valid code? [2000] (mysqlnd_ms) Exclusive usage of configuration enforced but did not find the correct INI file section
done!