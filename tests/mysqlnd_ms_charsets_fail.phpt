--TEST--
Charsets, choosing invalid
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_charsets_fail.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

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
mysqlnd_ms.config_file=test_mysqlnd_ms_charsets_fail.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(3, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);

	/* slave */
	if (!$res = mst_mysqli_query(4, $link, "SELECT @myrole AS _role, @@character_set_connection AS _charset", MYSQLND_MS_LAST_USED_SWITCH))
		printf("[005] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	if ('slave' != $row['_role'])
		printf("[006] Expecting reply from slave not from '%s'\n", $row['_role']);

	$current_charset = $row['_charset'];
	$new_charset = 'pleasenot';

	/* shall be run on *all* configured machines - all masters, all slaves */
	if (!$link->set_charset($new_charset))
		printf("[007] [%d] %s\n", $link->errno, $link->error);

	/* slave */
	if ($res = mst_mysqli_query(8, $link, "SELECT @myrole AS _role, @@character_set_connection AS _charset", MYSQLND_MS_LAST_USED_SWITCH)) {
		$row = $res->fetch_assoc();

		if ('slave' != $row['_role'])
			printf("[009] Expecting reply from slave not from '%s'\n", $row['_role']);

		if ($row['_charset'] != $current_charset)
			printf("[010] Expecting charset '%s' got '%s'\n", $current_charset, $row['_charset']);
	}

	if ($link->character_set_name() != $current_charset)
		printf("[011] Expecting charset '%s' got '%s'\n", $current_charset, $link->character_set_name());

	if ($res = mst_mysqli_query(12, $link, "SELECT @myrole AS _role, @@character_set_connection AS _charset", MYSQLND_MS_MASTER_SWITCH)) {

		$row = $res->fetch_assoc();

		if ('master' != $row['_role'])
			printf("[013] Expecting reply from master not from '%s'\n", $row['_role']);

		if ($row['_charset'] != $current_charset)
			printf("[014] Expecting charset '%s' got '%s'\n", $current_charset, $row['_charset']);
	 }

	if ($link->character_set_name() != $current_charset)
		printf("[015] Expecting charset '%s' got '%s'\n", $current_charset, $link->character_set_name());

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_charsets_fail.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_charsets_fail.ini'.\n");
?>
--EXPECTF--
[007] [%d] %s
done!