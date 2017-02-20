--TEST--
pick = user, callback = not given
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
		'pick' 	=> array('user' => array('andrey' => 'ulf')),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_user_no_callback.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_user_no_callback.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
	set_error_handler('mst_error_handler');

	if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
		printf("[001] Cannot connect to the server using host=%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
			$host, $user, $db, $port, $socket);

	if ($res = mst_mysqli_query(2, $link, "SELECT 1 FROM DUAL"))
		var_dump($res->fetch_assoc());

	mst_mysqli_query(3, $link, "SET @my_role='master'");

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_user_no_callback.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_user_no_callback.ini'.\n");
?>
--EXPECTF--
Fatal error: mysqli_real_connect(): (mysqlnd_ms) Error by creating filter 'user', can't find section 'callback' . Stopping. in %s on line %d
