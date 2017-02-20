--TEST--
Use master on write (pick = random)
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	$host => array(
		'master' => array($master_host),
		'slave' => array($slave_host, $slave_host, $slave_host, $slave_host),
		'master_on_write' => 1,
		'pick' => array('random'),
		'lazy_connection' => 0
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_master_on_write_random.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_master_on_write_random.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$slaves = array();
	do {
		mst_mysqli_query(2, $link, "SET @myrole='Slave'", MYSQLND_MS_SLAVE_SWITCH);
		$slaves[$link->thread_id] = true;
	} while (count($slaves) < 4);

	$res = mst_mysqli_query(3, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role");

	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);

	/* not a select -> master query */
	mst_mysqli_query(4, $link, "SET @myrole='Master'");


	/* master on write is active, master should reply */
	$res = mst_mysqli_query(5, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role");
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);

	/* SQL hint wins */
	$res = mst_mysqli_query(6, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role",  MYSQLND_MS_SLAVE_SWITCH);
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);

	/* master on write is active, master should reply */
	$res = mst_mysqli_query(7, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role");
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);

	/* SQL hint wins */
	$res = mst_mysqli_query(8, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role",  MYSQLND_MS_SLAVE_SWITCH);
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);

	/* SQL hint wins */
	$res = mst_mysqli_query(9, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role",  MYSQLND_MS_LAST_USED_SWITCH);
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);

	/* master on write is active, master should reply */
	$res = mst_mysqli_query(10, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role");
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);

	/* SQL hint wins */
	$res = mst_mysqli_query(11, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role",  MYSQLND_MS_SLAVE_SWITCH);
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);

	/* master on write... */
	$res = mst_mysqli_query(12, $link, "SELECT @myrole AS _role", MYSQLND_MS_MASTER_SWITCH);
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);

	/* master on write... */
	$res = mst_mysqli_query(13, $link, "SELECT @myrole AS _role", MYSQLND_MS_LAST_USED_SWITCH);
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);


	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_master_on_write_random.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_master_on_write_random.ini'.\n");
?>
--EXPECTF--
This is 'Slave %d' speaking
This is 'Master %d' speaking
This is 'Slave %d' speaking
This is 'Master %d' speaking
This is 'Slave %d' speaking
This is 'Slave %d' speaking
This is 'Master %d' speaking
This is 'Slave %d' speaking
This is 'Master' speaking
This is 'Master' speaking
done!