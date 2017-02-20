--TEST--
insert id, affected rows
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host, $emulated_slave_host),
		'pick' => array("roundrobin"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_insert_affected.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_insert_affected.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function run_insert($offset, $link, $num_rows, $switch = NULL) {
		if (!$ret = mst_mysqli_query($offset, $link, "DROP TABLE IF EXISTS test", $switch))
			return $ret;

		if (!$ret = mst_mysqli_query($offset + 1, $link, "CREATE TABLE test(id INT AUTO_INCREMENT PRIMARY KEY, label CHAR(1))", MYSQLND_MS_LAST_USED_SWITCH))
			return $ret;

		for ($i = 0; $i < $num_rows; $i++) {
			if (!$ret = mst_mysqli_query($offset + 2, $link, "INSERT INTO test(label) VALUES ('a')", MYSQLND_MS_LAST_USED_SWITCH))
				return $ret;
		}

		return $ret;
	}

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	/* master, automatically */
	run_insert(10, $link, 1);
	if (1 !== $link->insert_id)
		printf("[17] Master insert id should be 1 got %d\n", $link->insert_id);
	mst_mysqli_query(18, $link, "UPDATE test SET label = 'b'", MYSQLND_MS_LAST_USED_SWITCH);
	if (1 !== $link->affected_rows)
		printf("[19] Master affected should be 1 got %d\n", $link->affected_rows);

	/* slave 1 */
	run_insert(20, $link, 5, MYSQLND_MS_SLAVE_SWITCH);
	if (5 !== $link->insert_id)
		printf("[27] Slave 1 insert id should be 5 got %d\n", $link->insert_id);
	mst_mysqli_query(28, $link, "UPDATE test SET label = 'b'", MYSQLND_MS_LAST_USED_SWITCH);
	if (5 !== $link->affected_rows)
		printf("[29] Slave 1 affected should be 5 got %d\n", $link->affected_rows);

	/* slave 2 */
	run_insert(30, $link, 10, MYSQLND_MS_SLAVE_SWITCH);
	if (10 !== $link->insert_id)
		printf("[37] Slave 2 insert id should be 10 got %d\n", $link->insert_id);
	mst_mysqli_query(38, $link, "UPDATE test SET label = 'b'", MYSQLND_MS_LAST_USED_SWITCH);
	if (10 !== $link->affected_rows)
		printf("[39] Slave 2 affected should be 10 got %d\n", $link->affected_rows);

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_insert_affected.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_insert_affected.ini'.\n");
?>
--EXPECTF--
done!