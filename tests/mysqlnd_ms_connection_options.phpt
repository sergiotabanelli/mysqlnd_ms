--TEST--
Connection options
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");
require_once("util.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$ret = mst_mysqli_server_supports_ssl($master_host_only, $user, $passwd, $db, $master_port, $master_socket);

if (is_string($ret))
	die(sprintf("SKIP Failed to check if server support SSL, %s\n", $ret));

if (true != $ret)
	die(sprintf("SKIP Server has no SSL support"));

$ret = mst_mysqli_server_supports_ssl($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
if (is_string($ret))
	die(sprintf("SKIP Failed to check if server support SSL, %s\n", $ret));

if (true != $ret)
	die(sprintf("SKIP Server has no SSL support"));

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_connection_options.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_connection_options.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
	$flags = version_compare(PHP_VERSION, '5.6.16') >= 0 ? MYSQLI_CLIENT_SSL_DONT_VERIFY_SERVER_CERT : 0;
	$link = mysqli_init();
	if (!$link->options(MYSQLI_INIT_COMMAND, "SET @myinitcommand = 'something'"))
		printf("[001] Cannot set init command, [%d] %s\n", $link->errno, $link->error);
	if (!$link->ssl_set($client_cert, $client_key, NULL, NULL, NULL))
		printf("[000] Cannot set ssl options, [%d] %s\n", $link->errno, $link->error);

	if (!mst_mysqli_real_connect($link, "myapp", $user, $passwd, $db, $port, $socket, $flags))
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	/* master may have it because MS prototype used to connect to the master first */
	if (!($res = mst_mysqli_query(3, $link, "SELECT @myinitcommand AS _myinit", MYSQLND_MS_MASTER_SWITCH)))
		printf("[004] [%d] %s\n", $link->errno, $link->error);

	if (!$row = $res->fetch_assoc())
		printf("[005] [%d] %s\n", $link->errno, $link->error);

	if ('something' != $row['_myinit'])
		printf("[006] Expecting 'something' got '%s'\n", $row['_myinit']);

	if (!($res = mst_mysqli_query(7, $link, "SELECT @myinitcommand AS _myinit", MYSQLND_MS_SLAVE_SWITCH)))
		printf("[008] [%d] %s\n", $link->errno, $link->error);

	if (!$row = $res->fetch_assoc())
		printf("[009] [%d] %s\n", $link->errno, $link->error);

	if ('something' != $row['_myinit'])
		printf("[010] Expecting 'something' got '%s'\n", $row['_myinit']);

	if (!($res = mst_mysqli_query(11, $link, "SHOW STATUS LIKE 'Ssl_cipher'", MYSQLND_MS_MASTER_SWITCH)))
		printf("[012] [%d] %s\n", $link->errno, $link->error);

	if (!$row = $res->fetch_assoc())
		printf("[013] [%d] %s\n", $link->errno, $link->error);

	if (!$row['Value'] || !strlen($row['Value']))
		printf("[014] Expecting Ssl_cipher got nothing\n");

	if (!($res = mst_mysqli_query(15, $link, "SHOW STATUS LIKE 'Ssl_cipher'", MYSQLND_MS_SLAVE_SWITCH)))
		printf("[016] [%d] %s\n", $link->errno, $link->error);

	if (!$row = $res->fetch_assoc())
		printf("[017] [%d] %s\n", $link->errno, $link->error);

	if (!$row['Value'] || !strlen($row['Value']))
		printf("[018] Expecting Ssl_cipher got nothing\n");

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_connection_options.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_connection_options.ini'.\n");
?>
--EXPECTF--
done!