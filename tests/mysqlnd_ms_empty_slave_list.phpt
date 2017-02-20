--TEST--
Empty slave list, no config error, user wants to run multi-master only
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"name_of_a_config_section" => array(
		'master' => array($master_host),
		'slaves' => array(),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_empty_slave_list.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_empty_slave_list.ini
--FILE--
<?php
	require_once("connect.inc");

	$link = mst_mysqli_connect("name_of_a_config_section", $user, $passwd, $db, $port, $socket);
	if (0 !== mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	} else {
	  if (!($res = $link->query("SELECT 'Greetings from the master'"))) {
		  printf("[002] [%d] %s\n", $link->errno, $link->error);
	  } else {
		  $row = $res->fetch_row();
		  printf("[003] %s\n", $row[0]);
	  }
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_empty_slave_list.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_empty_slave_list.ini'.\n");
?>
--EXPECTF--
Fatal error: mysqli_real_connect(): (mysqlnd_ms) Section [slave] doesn't exist for host [name_of_a_config_section] in %s on line %d