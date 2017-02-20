--TEST--
SQL hints padded with whitespace
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
if ($error = mst_create_config("test_mysqlnd_ms_sql_hints_space.ini", $settings))
  die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_sql_hints_space.ini
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
	/* for simplicty lets assume those work, there's a dedicated test to check they do */
	my_mysqli_query(10, $link, sprintf("/*%s*/DROP TABLE IF EXISTS test", MYSQLND_MS_MASTER_SWITCH), $expected);
	$emulated_master_thread_id = $link->thread_id;
	$emulated_master = mst_mysqli_get_emulated_id(11, $link);
	my_mysqli_query(20, $link, sprintf("/*%s*/DROP TABLE IF EXISTS test", MYSQLND_MS_SLAVE_SWITCH), $expected);
	$emulated_slave_thread_id = $link->thread_id;
	$emulated_slave = mst_mysqli_get_emulated_id(21, $link);

	my_mysqli_query(30, $link, sprintf("/* %s */CREATE TABLE test(id INT)", MYSQLND_MS_MASTER_SWITCH), $expected);
	if ($emulated_master != ($tmp = mst_mysqli_get_emulated_id(31, $link))) {
		printf("[032] Expecting master got %s\n", $tmp);
	}

	my_mysqli_query(40, $link, sprintf("/* %s*/CREATE TABLE test(id INT)", MYSQLND_MS_SLAVE_SWITCH), $expected);
	if ($emulated_slave != ($tmp = mst_mysqli_get_emulated_id(41, $link))) {
		printf("[042] Expecting slave got %s\n", $tmp);
	}

	my_mysqli_query(50, $link, sprintf("/*%s */INSERT INTO test(id) VALUES (CONNECTION_ID())", MYSQLND_MS_SLAVE_SWITCH), $expected);
	if ($emulated_slave != ($tmp = mst_mysqli_get_emulated_id(51, $link))) {
		printf("[052] Expecting slave got %s\n", $tmp);
	}

	my_mysqli_query(60, $link, sprintf("/**/INSERT INTO test(id) VALUES (CONNECTION_ID())"), $expected);
	if ($emulated_master != ($tmp = mst_mysqli_get_emulated_id(61, $link))) {
		printf("[062] Expecting master got %s\n", $tmp);
	}


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_sql_hints_space.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_sql_hints_space.ini'.\n");
?>
--EXPECTF--
done!