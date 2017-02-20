--TEST--
Limits: SQL transactions
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if ($emulated_master_host == $emulated_slave_host) {
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
if ($error = mst_create_config("test_mysqlnd_ms_sql_transactions.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_sql_transactions.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/*
		The tricky part about transactions is that the plugin ignores
		them altogether. Autocommit on or off, the plugin ignores it
		and does query redirection regardless of transaction settings.
	*/

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());


	/* Note: unlike with C/J the autocommit setting does not impact the redirection */
	mst_mysqli_query(2, $link, "SET autocommit = 0", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(3, $link, "SET autocommit = 0", MYSQLND_MS_SLAVE_SWITCH);
	$link->autocommit = false;

	/* slave */
	mst_mysqli_query(100, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(4, $link, "CREATE TABLE test(id VARCHAR(20)) ENGINE=InnoDB", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(5, $link, "START TRANSACTION", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(6, $link, "INSERT INTO test(id) VALUES ('slave')", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(7, $link, "COMMIT", MYSQLND_MS_LAST_USED_SWITCH);

	/* master */
	mst_mysqli_query(101, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(8, $link, "CREATE TABLE test(id VARCHAR(20)) ENGINE=InnoDB", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(9, $link, "START TRANSACTION", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(10, $link, "INSERT INTO test(id) VALUES ('master')", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(11, $link, "COMMIT", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(12, $link, "START TRANSACTION", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(13, $link, "INSERT INTO test(id) VALUES ('master')", MYSQLND_MS_LAST_USED_SWITCH);

	/* leaving master with open transaction */
	mst_mysqli_query(14, $link, "START TRANSACTION", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(15, $link, "INSERT INTO test(id) VALUES ('slave')", MYSQLND_MS_LAST_USED_SWITCH);
	mst_mysqli_query(16, $link, "COMMIT", MYSQLND_MS_LAST_USED_SWITCH);

	/* back to the master for a rollback */
	mst_mysqli_query(17, $link, "ROLLBACK", MYSQLND_MS_MASTER_SWITCH);
	$res = mst_mysqli_query(18, $link, "SELECT id FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	while ($row = $res->fetch_assoc())
		printf("Master, id = '%s'\n", $row['id']);

	/* back to the slave for a select */
	$res = mst_mysqli_query(19, $link, "SELECT id FROM test");
	while ($row = $res->fetch_assoc())
		printf("Slave, id = '%s'\n", $row['id']);

	/*
	  No hint. Goes to the master. Open transaction, no autocommit. Slave will not see it.
	  For test running we do not require $emulated_master_host and $emulated_slave_host to
	  identify an actual master and slave in a MySQL replication setup.
	  However, you get the point when looking at the test...
	*/
	$res = mst_mysqli_query(20, $link, "INSERT INTO test(id) VALUES ('master')");
	/* back to the slave for a select */
	$res = mst_mysqli_query(21, $link, "SELECT id FROM test");
	while ($row = $res->fetch_assoc())
		printf("Slave, id = '%s'\n", $row['id']);

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_sql_transactions.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_sql_transactions.ini'.\n");


?>
--EXPECTF--
Master, id = 'master'
Slave, id = 'slave'
Slave, id = 'slave'
Slave, id = 'slave'
Slave, id = 'slave'
done!