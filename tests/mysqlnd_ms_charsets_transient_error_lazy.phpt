--TEST--
Charsets and transient errors, lazy
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'lazy_connections' =>  1,
		'transient_error' => array('mysql_error_codes' => array(2019), "max_retries" => 1, "usleep_retry" => 11),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_charsets_transient_error_lazy.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

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

	if (($res = mysqli_query($link, 'SHOW CHARACTER SET LIKE "letmebeinvalid"', MYSQLI_STORE_RESULT)) &&
			(mysqli_num_rows($res) == 1)) {
		die(sprintf("skip Bogus charset 'letmebeinvalid' found on server\n"));
	}
	if (!$res = mysqli_query($link, 'SELECT @@character_set_connection AS charset'))
		die(sprintf("skip Cannot select current charset, [%d] %s\n", $link->errno, $link->error));

	if (!$row = mysqli_fetch_assoc($res))
		die(sprintf("skip Cannot detect current charset, [%d] %s\n", $link->errno, $link->error));

	return $row['charset'];
}

$emulated_master_charset = test_for_charset($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
$emulated_slave_charset = test_for_charset($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

if ($emulated_master_charset != $emulated_slave_charset) {
	die(sprintf("skip Master (%s) and slave (%s) must use the same default charset.", $emulated_master_charset, $emulated_slave_charset));
}
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_charsets_transient_error_lazy.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	/* shall be run on *all* configured machines - all masters, all slaves */
	if (!$link->set_charset('letmebeinvalid'))
		printf("[004] [%d] %s\n", $link->errno, $link->error);

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_charsets_transient_error_lazy.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_charsets_transient_error_lazy.ini'.\n");
?>
--EXPECTF--
Transient error retries: 0
[004] [2019] %s
Transient error retries: 2
done!