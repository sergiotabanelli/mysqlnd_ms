--TEST--
Many masters; syntax exists but unsupported!
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		 /* NOTE: second master will be ignored! */
		'master' => array($master_host, "unreachable"),
		'slave' => array($slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_multi_master.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_multi_master.ini
mysqlnd_ms.multi_master=0
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(3, $link, "SET @myrole='master1'", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(4, $link, "SET @myrole='master2'", MYSQLND_MS_MASTER_SWITCH);

	$master = array();

	$res = mst_mysqli_query(5, $link, "SELECT CONNECTION_ID() AS _conn_id", MYSQLND_MS_MASTER_SWITCH);
	if (!$res)
		printf("[006] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	if ($link->thread_id != $row['_conn_id'])
		printf("[007] Expecting thread_id = %d got %d\n", $link->thread_id, $row['_conn_id']);

	$master[$row['_conn_id']] = (isset($master[$row['_conn_id']])) ? ++$master[$row['_conn_id']] : 1;

	mst_mysqli_query(8, $link, "DROP TABLE IF EXISTS test");
	mst_mysqli_query(9, $link, "CREATE TABLE test(id INT)", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(10, $link, "INSERT INTO test(id) VALUES(1)", MYSQLND_MS_LAST_USED_SWITCH);
	$res = mst_mysqli_query(11, $link, "SELECT CONNECTION_ID() AS _conn_id", MYSQLND_MS_LAST_USED_SWITCH);
	if (!$res)
		printf("[012] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	if ($link->thread_id != $row['_conn_id'])
		printf("[013] Expecting thread_id = %d got %d\n", $link->thread_id, $row['_conn_id']);

	$master[$row['_conn_id']] = (isset($master[$row['_conn_id']])) ? ++$master[$row['_conn_id']] : 1;

	$res = mst_mysqli_query(14, $link, "SELECT CONNECTION_ID() AS _conn_id", MYSQLND_MS_MASTER_SWITCH);
	if (!$res)
		printf("[015] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	if ($link->thread_id != $row['_conn_id'])
		printf("[016] Expecting thread_id = %d got %d\n", $link->thread_id, $row['_conn_id']);

	$master[$row['_conn_id']] = (isset($master[$row['_conn_id']])) ? ++$master[$row['_conn_id']] : 1;

	foreach ($master as $id => $num_queries) {
		printf("Master %d has run %d queries\n", $id, $num_queries);
	}

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_multi_master.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_multi_master.ini'.\n");
?>
--EXPECTF--
Master %d has run %d queries
done!