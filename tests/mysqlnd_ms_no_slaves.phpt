--TEST--
No slaves given in config -> connect error
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);

$settings = array(
	"name_of_a_config_section" => array(
		'master' => array($master_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_no_slaves.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_no_slaves.ini
--FILE--
<?php
	require_once("connect.inc");

	$link = mst_mysqli_connect("name_of_a_config_section", $user, $passwd, $db, $port, $socket);
	if (0 !== mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	} else {
	  var_dump($link);
	  if (!($res = $link->query("SELECT 'Who runs this?'"))) {
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
	if (!unlink("test_mysqlnd_ms_no_slaves.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_no_slaves.ini'.\n");
?>
--EXPECTF--
Fatal error: mysqli_real_connect(): (mysqlnd_ms) Section [slave] doesn't exist for host [name_of_a_config_section] in %s on line %d