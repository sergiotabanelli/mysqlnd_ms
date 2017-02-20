--TEST--
Use master on write (pick = random_once)
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
		'slave' => array($slave_host, $slave_host),
		'master_on_write' => 1,
		'pick' => array("random" => array("sticky" => "1")),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_master_on_write.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_master_on_write.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	mst_compare_stats();
	if (!($link = mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='Slave 1'", MYSQLND_MS_SLAVE_SWITCH);
	echo "----\n";
	mst_compare_stats();
	echo "----\n";

	$res = mst_mysqli_query(3, $link, "SELECT @myrole AS _role");
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);
	echo "----\n";
	mst_compare_stats();
	echo "----\n";
	/* not a select -> master query */
	mst_mysqli_query(4, $link, "SET @myrole='Master 1'");
	echo "----\n";
	mst_compare_stats();
	echo "----\n";

	/* master on write is active, master should reply */
	$res = mst_mysqli_query(5, $link, "SELECT @myrole AS _role");
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);
	echo "----\n";
	mst_compare_stats();
	echo "----\n";

	/* SQL hint wins */
	$res = mst_mysqli_query(6, $link, "SELECT @myrole AS _role",  MYSQLND_MS_SLAVE_SWITCH);
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);
	echo "----\n";
	mst_compare_stats();
	echo "----\n";

	/* master on write is active, master should reply */
	$res = mst_mysqli_query(7, $link, "SELECT @myrole AS _role");
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);

	/* SQL hint wins */
	$res = mst_mysqli_query(8, $link, "SELECT @myrole AS _role",  MYSQLND_MS_SLAVE_SWITCH);
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);

	/* SQL hint wins */
	$res = mst_mysqli_query(8, $link, "SELECT @myrole AS _role",  MYSQLND_MS_LAST_USED_SWITCH);
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);

	/* master on write... */
	$res = mst_mysqli_query(9, $link, "SELECT @myrole AS _role", MYSQLND_MS_MASTER_SWITCH);
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);

	/* master on write... */
	$res = mst_mysqli_query(10, $link, "SELECT @myrole AS _role", MYSQLND_MS_LAST_USED_SWITCH);
	$row = $res->fetch_assoc();
	$res->close();
	printf("This is '%s' speaking\n", $row['_role']);

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_master_on_write.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_master_on_write.ini'.\n");
?>
--EXPECTF--
----
Stats use_slave: 1
Stats use_slave_sql_hint: 1
Stats lazy_connections_slave_success: 1
Stats pool_masters_total: 1
Stats pool_slaves_total: 3
Stats pool_masters_active: 1
Stats pool_slaves_active: 3
----
This is 'Slave 1' speaking
----
Stats use_slave: 2
Stats use_slave_guess: 1
----
----
Stats use_master: 1
Stats use_master_guess: 1
Stats lazy_connections_master_success: 1
----
This is 'Master 1' speaking
----
Stats use_master: 2
Stats use_slave_guess: 2
----
This is 'Slave 1' speaking
----
Stats use_slave: 3
Stats use_slave_sql_hint: 2
----
This is 'Master 1' speaking
This is 'Slave 1' speaking
This is 'Slave 1' speaking
This is 'Master 1' speaking
This is 'Master 1' speaking
done!