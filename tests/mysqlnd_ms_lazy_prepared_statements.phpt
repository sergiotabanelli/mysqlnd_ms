--TEST--
Prepared Statements + lazy
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
		'lazy_connections' => 1,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_prepared_statements.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_prepared_statements.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(3, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(4, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);

	if (!$stmt = $link->prepare("SELECT @myrole AS _role"))
		printf("[005] [%d] %s\n", $link->errno, $link->error);

	if (!$stmt->execute())
		printf("[006] [%d] %s\n", $stmt->errno, $stmt->error);

	$role = NULL;
	if (!$stmt->bind_result($role))
		printf("[007] [%d] %s\n", $stmt->errno, $stmt->error);

	while ($stmt->fetch())
		printf("Role = '%s'\n", $role);

	if (!$stmt = $link->prepare("/*".MYSQLND_MS_MASTER_SWITCH."*/ SELECT @myrole AS _role"))
		printf("[008] [%d] %s\n", $link->errno, $link->error);

	if (!$stmt->execute())
		printf("[009] [%d] %s\n", $stmt->errno, $stmt->error);

	$role = NULL;
	if (!$stmt->bind_result($role))
		printf("[010] [%d] %s\n", $stmt->errno, $stmt->error);

	while ($stmt->fetch())
		printf("Role = '%s'\n", $role);

	$stmt->close();

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_prepared_statements.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_prepared_statements.ini'.\n");
?>
--EXPECTF--
Role = 'slave'
Role = 'master'
done!