--TEST--
pick = user, callback = non existant
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
		'pick' 	=> array('user' => array('callback' => 'unknown function')),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_user_unknown_function.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_user_unknown_function.ini
--FILE--
<?php
	require_once("connect.inc");
	set_error_handler('mst_error_handler');

	function mst_mysqli_query($offset, $link, $query) {
		$ret = $link->query($query);
		printf("[%03d + 01] [%d] '%s'\n", $offset, $link->errno, $link->error);
		return $ret;
	}

	if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
		printf("[001] Cannot connect to the server using host=%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
			$host, $user, $db, $port, $socket);

	mst_mysqli_query(2, $link, "SELECT 1 FROM DUAL");

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_user_unknown_function.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_user_unknown_function.ini'.\n");
?>
--EXPECTF--
[E_RECOVERABLE_ERROR] mysqli::query(): (mysqlnd_ms) Specified callback (unknown function) is not a valid callback in %s on line %d
[E_WARNING] mysqli::query(): (mysqlnd_ms) No connection selected by the last filter in %s on line %d
[002 + 01] [2000] '(mysqlnd_ms) No connection selected by the last filter'
done!