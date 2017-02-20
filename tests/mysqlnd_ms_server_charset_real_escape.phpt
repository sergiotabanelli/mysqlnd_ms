--TEST--
server_charset
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

/* need slave charset to match configured and slave charsets */
if (!($link = mst_mysqli_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket))) {
	die(sprintf("SKIP Can't test slave default charset, [%d] %s\n",
		mysqli_connect_errno(), mysqli_connect_error()));
}

if (!($res = $link->query("SELECT @@character_set_connection AS charset")) ||
	!($row = $res->fetch_assoc())) {
	die(sprintf("SKIP Can't check for slave charset, [%d] %s\n", $link->errno, $link->error));
}

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host, $slave_host),
		'pick' => array("roundrobin"),
		'server_charset' => $row['charset'],
		'lazy_connections' => 1,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_server_charset_real_escape.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_server_charset_real_escape.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	/* check default charset */
	$res = mst_mysqli_query(2, $link, "SELECT @@character_set_connection AS charset");
	$row = $res->fetch_assoc();

	/* From a user perspective MS and non MS-Connection are now in the same state: connected */
	if (!($link_ms = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$string = "андрей\0'\"улф\"\'\13\10\7йоханес\0майескюел мастер слейв";
	$no_ms = $link->real_escape_string($string);
	$ms = $link_ms->real_escape_string($string);

	if ($no_ms !== $ms) {
		printf("[007] Encoded strings differ for charset '%s', MS = '%s', no MS = '%s'\n", $row['charset'], $ms, $no_ms);
		printf("[008] [%d/%s] '%s'\n", $link->errno, $link->sqlstate, $link->error);
		printf("[009] [%d/%s] '%s'\n", $link_ms->errno, $link_ms->sqlstate, $link_ms->error);
	}

	$res = mst_mysqli_query(10, $link_ms, "SELECT @@character_set_connection AS charset");
	$row_ms = $res->fetch_assoc();
	if ($row_ms['charset'] != $row['charset']) {
		printf("[011] MS connection should use charset '%s' but reports '%s'\n", $row['charset'], $row_ms['charset']);
	}

	$link_ms->close();

	print "done!";
?>
--CLEAN--
<?php
if (!unlink("test_mysqlnd_ms_server_charset_real_escape.ini"))
	printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_server_charset_real_escape.ini'.\n");
?>
--EXPECTF--
done!