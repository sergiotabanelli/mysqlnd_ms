--TEST--
ping
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
if ($error = mst_create_config("test_mysqlnd_ms_ping.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_ping.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='Slave 1'", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(3, $link, "SET @myrole='Slave 2'", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(4, $link, "SET @myrole='Master 1'");

	if (!$link->ping())
		printf("[005] [%d] %s\n", $link->errno, $link-error);

	/* although we kill the master connection, the slaves are still reachable */
	if (!$link->kill($link->thread_id))
		printf("[006] [%d] %s\n", $link->errno, $link->error);

	usleep(2000);
	if ($link->ping())
		printf("[007] Master connection is still alive\n");
	else
		printf("[007] [%d] %s\n", $link->errno, $link->error);

	if (mst_mysqli_query(8, $link, "SET @myrole='Master 1'"))
		printf("[008] Master connection can still run queries\n");

	$res = mst_mysqli_query(9, $link, "SELECT @myrole AS _role");

	if (!$res || !($row = $res->fetch_assoc()))
		printf("[010] Slave connections should be still usable, [%d] %s\n",
			$link->errno, $link->error);

	if ($row['_role'] != "Slave 1")
		printf("[011] Expecting 'Slave 1' got '%s'\n", $row['_role']);

	if (!$link->ping())
		printf("[012] [%d] %s\n", $link->errno, $link->error);

	if (!$link->kill($link->thread_id))
		printf("[013] [%d] %s\n", $link->errno, $link->error);

	usleep(2000);

	if ($link->ping())
		printf("[014] Slave connection is still alive\n");
	else
		printf("[014] [%d] %s\n", $link->errno, $link->error);

	$res = mst_mysqli_query(15, $link, "SELECT @myrole AS _role");

	if (!$res || !($row = $res->fetch_assoc()))
		printf("[016] Slave connections should be still usable, [%d] %s\n",
		  $link->errno, $link->error);

	if ($row['_role'] != "Slave 2")
		printf("[017] Expecting 'Slave 2' got '%s'\n", $row['_role']);

	if (!$link->ping())
		printf("[018] [%d] %s\n", $link->errno, $link->error);

	if (!$link->kill($link->thread_id))
		printf("[019] [%d] %s\n", $link->errno, $link->error);

	usleep(2000);

	if ($link->ping())
		printf("[020] Slave connection is still alive\n");
	else
		printf("[020] [%d] %s\n", $link->errno, $link->error);

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_ping.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_ping.ini'.\n");
?>
--EXPECTF--
[007] [%d] %s
[008] [%d] %s
[014] [%d] %s
[020] [%d] %s
done!