--TEST--
Limits: SQL prepare
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
		'slave' => array($slave_host, $slave_host),
		'pick' => array("roundrobin"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_sql_prepare.ini", $settings))
  die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_sql_prepare.ini
--FILE--
<?php
	require_once("connect.inc");

	/* shall use host = forced_master_hostname_abstract_name from the ini file */
	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	/* one and only master */
	$sql = sprintf("/*%s*/SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	if (!$link->query($sql)) {
		printf("[002] [%d] %s\n", $link->errno, $link->error);
	}

	/* first slave */
	$sql = sprintf("/*%s*/SET @myrole='slave_a'", MYSQLND_MS_SLAVE_SWITCH);
	if (!$link->query($sql)) {
		printf("[003] [%d] %s\n", $link->errno, $link->error);
	}

	/*
	  At this point we are connected to the slave.
	  We do a SQL PREPARE ... SELECT. A PREPARE is not
	  a SELECT. It will be redirected to the master.
	  If you want to run it on the slave to which you are
	  currently connected, for example to avoid overloading the master,
	  you must use the SQL hint MYSQLND_MS_LAST_USED_SWITCH.
	*/
	if (!$link->query("PREPARE mystmt FROM 'SELECT @myrole AS _role'"))
		printf("[004] [%d] %s\n", $link->errno, $link->error);
	if (!($res = $link->query("EXECUTE mystmt")))
		printf("[005] [%d] %s\n", $link->errno, $link->error);
	$row = $res->fetch_assoc();
	if ('master' != $row['_role']) {
		printf("[006] Expecting 'master' got '%s'\n", $row['_role']);
	}
	$res->close();

	/* second slave - round robin ! Also note: two slaves with the same URI */
	if (!($res = $link->query("SELECT @myrole AS _role")))
		printf("[007] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	if ('' != $row['_role'])
		printf("[008] Expecting '' got '%s'\n", $row['_role']);

	/* should go to the master */
	if (!($res = $link->query("EXECUTE mystmt")))
		printf("[009] [%d] %s\n", $link->errno, $link->error);
	$row = $res->fetch_assoc();
	if ('master' != $row['_role']) {
		printf("[010] Expecting 'master' got '%s'\n", $row['_role']);
	}
	$res->close();

	/*
	A more tricky scenatio for which there is no solution but
	to use a user-defined server selection (pick_server() handler).
	PREPARE ... - slave A, forced with MS_SLAVE hint
	EXECUTE ... - slave A, forced with MS_LAST_USED hint
    EXECUTE     - master. no hint
    EXECUTE     - ... no way to get to slave A, MS_SLAVE hint will use slave B because of RR
	*/
	$query = sprintf("/*%s*/PREPARE mystmt FROM 'SELECT @myrole AS _role'", MYSQLND_MS_SLAVE_SWITCH);
	if (!$link->query($query))
		printf("[011] [%d] %s\n", $link->errno, $link->error);

	/* if we don't use a hint this goes to the master because it is no select */
	$query = sprintf("/*%s*/EXECUTE mystmt", MYSQLND_MS_LAST_USED_SWITCH);
	if (!($res = $link->query($query)))
		printf("[012] [%d] %s\n", $link->errno, $link->error);
	$row = $res->fetch_assoc();
	if ('slave_a' != $row['_role']) {
		printf("[013] Expecting 'slave_a' got '%s'\n", $row['_role']);
	}
	$res->close();

	if (!($res = $link->query("EXECUTE mystmt")))
		printf("[014] [%d] %s\n", $link->errno, $link->error);
	$row = $res->fetch_assoc();
	if ('master' != $row['_role']) {
		printf("[015] Expecting 'master' got '%s'\n", $row['_role']);
	}
	$res->close();

	/* round robin will give us the second slave */
	$query = sprintf("/*%s*/EXECUTE mystmt", MYSQLND_MS_SLAVE_SWITCH);
	if (!($res = $link->query($query)))
		printf("[016] [%d] %s\n", $link->errno, $link->error);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_sql_prepare.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_sql_prepare.ini'.\n");
?>
--EXPECTF--
[016] [%d] %s
done!
