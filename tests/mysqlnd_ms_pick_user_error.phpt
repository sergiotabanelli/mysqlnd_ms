--TEST--
Config settings: pick server = user, handling of user error
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

if (($master_host == $slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
		'pick' 	=> array('user' => array('callback' => 'pick_server')),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_user_error.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_user_error.ini
--FILE--
<?php
	require_once("connect.inc");
	set_error_handler('mst_error_handler');

	function pick_server($connected_host, $query, $master, $slaves, $last_used_connection) {
		global $fail;
		printf("%s\n", $query);
		/* should default to build-in pick logic */
		if ($fail)
		  return -1;
		return $master[0];
	}

	if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
		printf("[001] Cannot connect to the server using host=%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
			$host, $user, $db, $port, $socket);

	/* Catchable fatal error, no server selected */
	$fail = true;
	$query = sprintf("/*%s*/SELECT CONNECTION_ID() as _master FROM DUAL", MYSQLND_MS_MASTER_SWITCH);
	/* random follow-up error message, e.g. 2014 Commands out of sync */
	if (!$res = $link->query($query))
		printf("[002] [%d] %s\n", $link->errno, $link->error);


	/* The connection is still useable. Just rerun the statement and pick a connection from the pool */
	$fail = false;
	$query = sprintf("/*%s*/SELECT CONNECTION_ID() as _master FROM DUAL", MYSQLND_MS_MASTER_SWITCH);
	/* random follow-up error message, e.g. 2014 Commands out of sync */
	if (!$res = $link->query($query))
		printf("[003] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	$res->close();
	printf("Master has thread id %d\n", $row['_master']);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_user_error.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_user_error.ini'.\n");
?>
--EXPECTF--
/*ms=master*/SELECT CONNECTION_ID() as _master FROM DUAL
[E_RECOVERABLE_ERROR] mysqli::query(): (mysqlnd_ms) User filter callback has not returned string with server to use. The callback must return a string in %s on line %d
[E_WARNING] mysqli::query(): (mysqlnd_ms) No connection selected by the last filter in %s on line %d
[002] [2000] (mysqlnd_ms) No connection selected by the last filter
/*ms=master*/SELECT CONNECTION_ID() as _master FROM DUAL
Master has thread id %d
done!