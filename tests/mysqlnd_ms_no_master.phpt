--TEST--
No master configured
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"name_of_a_config_section" => array(
		'slave' => array($slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_no_master.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_no_master.ini
--FILE--
<?php
	require_once("connect.inc");

	$link = mst_mysqli_connect("name_of_a_config_section", $user, $passwd, $db, $port, $socket);
	if (0 !== mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	} else {
	  if (!$link->query("DROP TABLE IF EXISTS test")) {
		  printf("[002] [%d] %s\n", $link->errno, $link->error);
	  }
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_no_master.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_no_master.ini'.\n");
?>
--EXPECTF--
Fatal error: mysqli_real_connect(): (mysqlnd_ms) Section [master] doesn't exist for host [name_of_a_config_section] in %s on line %d