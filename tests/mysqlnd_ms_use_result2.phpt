--TEST--
mysqli_query + MYSQLI_USE_RESULT
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
if ($error = mst_create_config("test_mysqlnd_use_result2.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_use_result2.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='slave1'", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(3, $link, "SET @myrole='slave2'", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(4, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);

	/* round robin - slave 1  - note how the plugin fails because internally we use real_query */
	$res = mysqli_query($link, "SELECT @myrole AS _role", MYSQLI_USE_RESULT);
	if (!$res) {
		printf("[006] [%d] %s\n", $link->errno, $link->error);
	}

	while ($row = $res->fetch_assoc())
		if ($row['_role'] != 'slave1')
			printf("[007] Expecting 'slave1' got '%s'\n", $row['_role']);

	$res->close();

	mst_mysqli_query(8, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(9, $link, "CREATE TABLE test(id INT, label varchar(20))", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(10, $link, "INSERT INTO test(id, label) VALUES (1, 'a'), (2, 'b'), (3, 'c')", MYSQLND_MS_LAST_USED_SWITCH);
	$ret = mst_mysqli_real_query(11, $link, "SELECT @myrole AS _role, id, label FROM test ORDER BY id ASC", MYSQLND_MS_LAST_USED_SWITCH);
	if (!$ret) {
		printf("[012] [%d] %s\n", $link->errno, $link->error);
	}
	if (!$res = mysqli_use_result($link))
		printf("[013] [%d] %s\n", $link->errno, $link->error);

	while ($row = $res->fetch_assoc())
	  printf("Slave 1, field_count = %d, role = %s, id = %d, label = '%s'\n",
		$res->field_count,
		$row['_role'], $row['id'], $row['label']);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_use_result2.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_ini_force_config.ini'.\n");
?>
--EXPECTF--

Slave 1, field_count = 3, role = slave1, id = 1, label = 'a'
Slave 1, field_count = 3, role = slave1, id = 2, label = 'b'
Slave 1, field_count = 3, role = slave1, id = 3, label = 'c'
done!