--TEST--
Charsets, invalid using PDO
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
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_charsets_pdo_fail.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

function test_for_charset($host, $user, $passwd, $db, $port, $socket) {
	if (!$link = mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket))
		die(sprintf("skip Cannot connect, [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

	if (!($res = mysqli_query($link, 'SELECT version() AS server_version')) ||
			!($tmp = mysqli_fetch_assoc($res))) {
		mysqli_close($link);
		die(sprintf("skip Cannot check server version, [%d] %s\n",
		mysqli_errno($link), mysqli_error($link)));
	}
	mysqli_free_result($res);
	$version = explode('.', $tmp['server_version']);
	if (empty($version)) {
		mysqli_close($link);
		die(sprintf("skip Cannot check server version, based on '%s'",
			$tmp['server_version']));
	}

	if ($version[0] <= 4 && $version[1] < 1) {
		mysqli_close($link);
		die(sprintf("skip Requires MySQL Server 4.1+\n"));
	}

	if (($res = mysqli_query($link, 'SHOW CHARACTER SET LIKE "pleasenot"', MYSQLI_STORE_RESULT)) && (mysqli_num_rows($res) == 1)) {
		die(sprintf("skip WOW, server has charset 'pleasenot'!\n"));
	}

	if (!$res = mysqli_query($link, 'SELECT @@character_set_connection AS charset'))
		die(sprintf("skip Cannot select current charset, [%d] %s\n", $link->errno, $link->error));

	if (!$row = mysqli_fetch_assoc($res))
		die(sprintf("skip Cannot detect current charset, [%d] %s\n", $link->errno, $link->error));

	return $row['charset'];
}

test_for_charset($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
test_for_charset($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_charsets_pdo_fail.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

    $pdo = new PDO("mysql:host=myapp;dbname=" . $db . ";charset=pleasenot", $user, $passwd);
var_dump($pdo->quote("a"));
	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_charsets_pdo_fail.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_charsets_pdo_fail.ini'.\n");
?>
--EXPECTF--
Fatal error: Uncaught exception 'PDOException' with message 'SQLSTATE[HY000] [2019] Unknown character set' in %s:%d
Stack trace:
#0 %s(%d): PDO->__construct('mysql:host=%s', '%s', '%A')
#1 {main}
  thrown in %s on line 5