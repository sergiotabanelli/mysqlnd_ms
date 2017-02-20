--TEST--
SQL hints to control query redirection
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

include_once("util.inc");
$ret = mst_is_slave_of($emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket, $emulated_master_host_only, $emulated_master_port, $emulated_master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true == $ret)
	die("SKIP Configured emulated master and emulated slave could be part of a replication cluster\n");

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_sql_hints.ini", $settings))
  die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_sql_hints.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function my_mysqli_query($offset, $link, $query, $expected) {
		if (!$res = $link->query($query)) {
			printf("[%03d + 01] [%d] %s\n", $offset, $link->errno, $link->error);
			return false;
		}
		if ($expected) {
			$row = $res->fetch_assoc();
			$res->close();
			if (empty($row)) {
				printf("[%03d + 02] [%d] %s, empty result\n", $offset, $link->errno, $link->error);
				return false;
			}
			if ($row != $expected) {
				printf("[%03d + 03] Unexpected results, dumping data\n", $offset);
				var_dump($row);
				var_dump($expected);
				return false;
			}
		}
		return true;
	}

	if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
		printf("[001] Cannot connect to the server using host=%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
			$host, $user, $db, $port, $socket);

	$expected = array();
	my_mysqli_query(10, $link, sprintf("/*%s*/DROP TABLE IF EXISTS test", MYSQLND_MS_MASTER_SWITCH), $expected);
	$emulated_master_thread_id = $link->thread_id;
	$emulated_master = mst_mysqli_get_emulated_id(11, $link);
	my_mysqli_query(20, $link, sprintf("/*%s*/DROP TABLE IF EXISTS test", MYSQLND_MS_SLAVE_SWITCH), $expected);
	$emulated_slave_thread_id = $link->thread_id;
	$emulated_slave = mst_mysqli_get_emulated_id(21, $link);
	my_mysqli_query(30, $link, sprintf("/*%s*/CREATE TABLE test(id INT)", MYSQLND_MS_MASTER_SWITCH), $expected);
	my_mysqli_query(40, $link, sprintf("/*%s*/CREATE TABLE test(id INT)", MYSQLND_MS_SLAVE_SWITCH), $expected);
	my_mysqli_query(50, $link, sprintf("/*%s*/INSERT INTO test(id) VALUES (CONNECTION_ID())", MYSQLND_MS_SLAVE_SWITCH), $expected);
	my_mysqli_query(60, $link, sprintf("/*%s*/INSERT INTO test(id) VALUES (CONNECTION_ID())", MYSQLND_MS_MASTER_SWITCH), $expected);
	my_mysqli_query(70, $link, sprintf("/*%s*/SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH), $expected);
	my_mysqli_query(80, $link, sprintf("/*%s*/SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH), $expected);

	/* slave, no hint */
	$expected = array('_role' => 'slave');
	my_mysqli_query(90, $link, "SELECT @myrole AS _role", $expected);
	$server_id = mst_mysqli_get_emulated_id(91, $link);
	if ($server_id != $emulated_slave)
		printf("[092] Query should have been run on the slave\n");

	/* master, no hint */
	$expected = array();
	my_mysqli_query(100, $link, "INSERT INTO test(id) VALUES (-2)", $expected);
	$server_id = mst_mysqli_get_emulated_id(101, $link);
	if ($server_id != $emulated_master)
		printf("[102] Query should have been run on the master\n");

	/* ... boring: slave */
	$expected = array("id" => $emulated_slave_thread_id);
	my_mysqli_query(110, $link, sprintf("/*%s*/SELECT id FROM test", MYSQLND_MS_SLAVE_SWITCH), $expected);
	$server_id = mst_mysqli_get_emulated_id(111, $link);
	if ($server_id != $emulated_slave)
		printf("[112] Query should have been run on the slave\n");

	/* master, no hint */
	$expected = array();
	my_mysqli_query(120, $link, "DELETE FROM test WHERE id = -2", $expected);
	$server_id = mst_mysqli_get_emulated_id(121, $link);
	if ($server_id != $emulated_master)
		printf("[122] Query should have been run on the master\n");

	/* master, forced */
	$expected = array("id" => $emulated_master_thread_id);
	my_mysqli_query(130, $link, sprintf("/*%s*/SELECT id FROM test", MYSQLND_MS_MASTER_SWITCH), $expected);
	$server_id = mst_mysqli_get_emulated_id(131, $link);
	if ($server_id != $emulated_master)
		printf("[132] Query should have been run on the master\n");

	/* master, forced */
	$expected = array("_role" => 'master');
	my_mysqli_query(140, $link, sprintf("/*%s*/SELECT @myrole AS _role", MYSQLND_MS_LAST_USED_SWITCH), $expected);
	$server_id = mst_mysqli_get_emulated_id(141, $link);
	if ($server_id != $emulated_master)
		printf("[142] Query should have been run on the master\n");

	/* slave, forced */
	$expected = array();
	my_mysqli_query(150, $link, sprintf("/*%s*/INSERT INTO test(id) VALUES (0)", MYSQLND_MS_SLAVE_SWITCH), $expected);
	$server_id = mst_mysqli_get_emulated_id(151, $link);
	if ($server_id != $emulated_slave)
		printf("[152] Query should have been run on the slave\n");

	$expected = array('_role' => 'slave');
	my_mysqli_query(160, $link, sprintf("/*%s*/SELECT @myrole AS _role", MYSQLND_MS_LAST_USED_SWITCH), $expected);
	$server_id = mst_mysqli_get_emulated_id(161, $link);
	if ($server_id != $emulated_slave)
		printf("[162] Query should have been run on the slave\n");

	$expected = array();
	my_mysqli_query(170, $link, sprintf("/*%s*/DROP TABLE IF EXISTS test", MYSQLND_MS_MASTER_SWITCH), $expected);
	my_mysqli_query(180, $link, sprintf("/*%s*/DROP TABLE IF EXISTS test", MYSQLND_MS_SLAVE_SWITCH), $expected);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_sql_hints.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_sql_hints.ini'.\n");
?>
--EXPECTF--
done!