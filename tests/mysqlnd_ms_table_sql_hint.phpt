--TEST--
table filter basics: SQL hint
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_check_feature(array("table_filter"));
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
		'lazy_connections' => 0,
		'filters' => array(
			"table" => array(
				"rules" => array(
					$db . ".test1%" => array(
						"master" => array("master1"),
						"slave" => array("slave1"),
					),
				),
			),
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_table_sql_hint.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_table_sql_hint.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* shall use host = forced_master_hostname_abstract_name from the ini file */
	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	mst_mysqli_query(2, $link, "DROP TABLE IF EXISTS test1");
	$master = $link->thread_id;

	mst_mysqli_query(3, $link, "CREATE TABLE test1(id BIGINT)", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(4, $link, "INSERT INTO test1(id) VALUES (CONNECTION_ID())", MYSQLND_MS_LAST_USED_SWITCH);
	$res = mst_mysqli_query(5, $link, "SELECT id FROM test1", MYSQLND_MS_LAST_USED_SWITCH);
	$row = $res->fetch_assoc();

	if ($master != $row['id'])
		printf("[006] Master thread id differs from INSERT CONNECTION_ID(), expecting %d got %d\n", $master, $row['id']);

	if ($master != $link->thread_id)
		printf("[007] Master thread id differs from SELECT CONNECTION_ID(), expecting %d got %d\n", $master, $link->thread_id);

	mst_mysqli_query(8, $link, "DROP TABLE IF EXISTS test1", MYSQLND_MS_SLAVE_SWITCH);
	$slave = $link->thread_id;
	mst_mysqli_query(9, $link, "CREATE TABLE test1(id BIGINT)", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(10, $link, "INSERT INTO test1(id) VALUES (CONNECTION_ID())", MYSQLND_MS_LAST_USED_SWITCH);

	$res = mst_mysqli_query(11, $link, "SELECT id FROM test1", MYSQLND_MS_LAST_USED_SWITCH);
	$row = $res->fetch_assoc();

	if ($slave != $row['id'])
		printf("[012] Slave thread id differs from INSERT CONNECTION_ID(), expecting %d got %d\n", $slave, $row['id']);

	if ($slave != $link->thread_id)
		printf("[013] Master thread id differs from SELECT CONNECTION_ID(), expecting %d got %d\n", $slave, $link->thread_id);

	if ($slave == $master)
		printf("[014] Master and slave are using the same connection\n");

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_table_sql_hint.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_table_sql_hint.ini'.\n");
?>
--EXPECTF--
done!