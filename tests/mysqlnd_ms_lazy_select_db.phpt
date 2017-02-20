--TEST--
lazy connections and select_db
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $master_host_only,
				'port' 		=> (int)$master_port,
				'socket' 	=> $master_socket,
			),
		),

		'slave' => array(
			"slave1" => array(
				'host' 	=> $slave_host_only,
				'port' 	=> (int)$slave_port,
				'socket' => $slave_socket,
			),
		 ),

		'lazy_connections' => 1,
		'filters' => array(
			"random" => array('sticky' => '1'),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_lazy_select_db.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_lazy_select_db.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, NULL, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	$res = mst_mysqli_query(3, $link, "SELECT DATABASE() AS _db");
	$row = $res->fetch_assoc();
	if ($row['_db'] != "")
		printf("[004] No DB should be selected, connected to '%s'\n", $row['_db']);

	$res = mst_mysqli_query(5, $link, "SELECT DATABASE() AS _db", MYSQLND_MS_MASTER_SWITCH);
	$row = $res->fetch_assoc();
	if ($row['_db'] != "")
		printf("[006] No DB should be selected, connected to '%s'\n", $row['_db']);

	mysqli_close($link);

	$link = mst_mysqli_connect("myapp", $user, $passwd, NULL, $port, $socket);
	if (mysqli_connect_errno())
		printf("[007] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!mysqli_select_db($link, $db))
		printf("[008] [%d] %s\n", mysqli_errno($link), mysqli_error($link));

	$res = mst_mysqli_query(9, $link, "SELECT DATABASE() AS _db");
	$row = $res->fetch_assoc();
	if ($row['_db'] != $db)
		printf("[010] DB should be '%s', connected to '%s'\n", $db, $row['_db']);

	$res = mst_mysqli_query(11, $link, "SELECT DATABASE() AS _db", MYSQLND_MS_MASTER_SWITCH);
	$row = $res->fetch_assoc();
	if ($row['_db'] != $db)
		printf("[012] DB should be '%s', connected to '%s'\n", $db, $row['_db']);

	mysqli_close($link);

	$link = mst_mysqli_connect("myapp", $user, $passwd, NULL, $port, $socket);
	if (mysqli_connect_errno())
		printf("[013] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	/* free and alloc/copy of internal db field */
	if (!mysqli_select_db($link, $db))
		printf("[014] [%d] %s\n", mysqli_errno($link), mysqli_error($link));

	if (!mysqli_select_db($link, $db))
		printf("[015] [%d] %s\n", mysqli_errno($link), mysqli_error($link));

	$res = mst_mysqli_query(16, $link, "SELECT DATABASE() AS _db");
	$row = $res->fetch_assoc();
	if ($row['_db'] != $db)
		printf("[017] DB should be '%s', connected to '%s'\n", $db, $row['_db']);


	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");

	if (!unlink("test_mysqlnd_ms_lazy_select_db.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_lazy_select_db.ini'.\n");
?>
--EXPECTF--
done!