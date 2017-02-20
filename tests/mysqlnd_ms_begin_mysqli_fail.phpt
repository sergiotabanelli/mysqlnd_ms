--TEST--
mysqli_begin failure (mysqlnd tx_begin)
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.4.99-dev', '<'))
	die(sprintf("SKIP Requires PHP 5.5.0 or newer, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host, $emulated_slave_host),
		'trx_stickiness' => 'on',
		'pick' => array("random"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_begin_mysqli_fail.ini", $settings))
	die(sprintf("SKIP %s\n", $error));


_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

if (!$link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
	die(sprintf("skip Cannot connect, [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

/* BEGIN READ ONLY exists since MySQL 5.6.5 */
if ($link->server_version < 50605) {
	die(sprintf("skip Emulated master: need MySQL 5.6.5+, got %s", $link->server_version));
}

if (!$link = mst_mysqli_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket))
	die(sprintf("skip Cannot connect, [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

/* BEGIN READ ONLY exists since MySQL 5.6.5 */
if ($link->server_version < 50605) {
	die(sprintf("skip Emulated slave: need MySQL 5.6.5+, got %s", $link->server_version));
}
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_begin_mysqli_fail.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());


	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(3, $link, "DROP TABLE IF EXISTS test");
	mst_mysqli_query(4, $link, "CREATE TABLE test(id INT) ENGINE=InnoDB");

	$slaves = array();
	$i = 0;
	do {
		mst_mysqli_query(6, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);
		mst_mysqli_query(7, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_LAST_USED_SWITCH);
		$slaves[$link->thread_id] = true;
		$i++;
	} while (($i < 10) && (count($slaves) < 2));
	if ((10 == $i) && (count($slaves) < 2)) {
		die("[007] Two connections happen to have the same thread id, ignore and run again!");
	}

	printf("... plain trx commit, begin fails\n");
	$ret = $link->begin_transaction(-1);
	printf("[008] %s '%s'\n", gettype($ret), var_export($ret, true));

	mst_mysqli_fech_role(mst_mysqli_query(9, $link, "SELECT @myrole AS _role"));
	mst_mysqli_fech_role(mst_mysqli_query(10, $link, "SELECT @myrole AS _role"));

	printf("... plain trx commit, begin success, rollback\n");
	$ret = $link->begin_transaction();
	printf("[011] %s '%s'\n", gettype($ret), var_export($ret, true));

	mst_mysqli_fech_role(mst_mysqli_query(12, $link, "SELECT @myrole AS _role"));
	if (isset($slaves[$link->thread_id])) {
		printf("[013] SELECT on slave during transaction\n");
	}
	mst_mysqli_query(14, $link, "INSERT INTO test(id) VALUES (1)");
	$link->rollback();
	$res = mst_mysqli_query(15, $link, "SELECT MAX(id) FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	mst_mysqli_fech_role(mst_mysqli_query(16, $link, "SELECT @myrole AS _role"));

	printf("... plain trx commit, begin fails\n");
	$ret = $link->begin_transaction(-1);
	printf("[017] %s '%s'\n", gettype($ret), var_export($ret, true));

	mst_mysqli_fech_role(mst_mysqli_query(18, $link, "SELECT @myrole AS _role"));
	mst_mysqli_fech_role(mst_mysqli_query(19, $link, "SELECT @myrole AS _role"));

	printf("... plain trx commit, begin success, commit\n");
	$ret = $link->begin_transaction();
	printf("[020] %s '%s'\n", gettype($ret), var_export($ret, true));

	mst_mysqli_fech_role(mst_mysqli_query(21, $link, "SELECT @myrole AS _role"));
	if (isset($slaves[$link->thread_id])) {
		printf("[022] SELECT on slave during transaction\n");
	}
	mst_mysqli_query(23, $link, "INSERT INTO test(id) VALUES (2)");
	$link->commit();
	$res = mst_mysqli_query(24, $link, "SELECT MAX(id) FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	mst_mysqli_fech_role(mst_mysqli_query(25, $link, "SELECT @myrole AS _role"));


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_begin_mysqli_fail.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_begin_mysqli_fail.ini'.\n");

	if ($error = mst_mysqli_drop_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %d\n", $error);
?>
--EXPECTF--
... plain trx commit, begin fails

Warning: mysqli::begin_transaction(): Invalid value for parameter flags (-1) in %s on line %d
[008] boolean 'false'
This is 'slave' speaking
This is 'slave' speaking
... plain trx commit, begin success, rollback
[011] boolean 'true'
This is 'master' speaking
array(1) {
  ["MAX(id)"]=>
  NULL
}
This is 'slave' speaking
... plain trx commit, begin fails

Warning: mysqli::begin_transaction(): Invalid value for parameter flags (-1) in %s on line %d
[017] boolean 'false'
This is 'slave' speaking
This is 'slave' speaking
... plain trx commit, begin success, commit
[020] boolean 'true'
This is 'master' speaking
array(1) {
  ["MAX(id)"]=>
  string(1) "2"
}
This is 'slave' speaking
done!
