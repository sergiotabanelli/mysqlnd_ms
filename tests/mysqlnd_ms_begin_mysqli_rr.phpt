--TEST--
mysqli_begin (mysqlnd tx_begin), round robin
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.4.99-dev', '<'))
	die(sprintf("SKIP Requires PHP 5.5.0 or newer, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host, $emulated_master_host),
		'slave' => array($emulated_slave_host, $emulated_slave_host),
		'trx_stickiness' => 'on',
		'pick' => array("roundrobin"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_begin_mysqli_rr.ini", $settings))
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
mysqlnd_ms.multi_master=1
mysqlnd_ms.config_file=test_mysqlnd_ms_begin_mysqli_rr.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	for ($i = 0; $i < 2; $i++) {
		mst_mysqli_query(2, $link, sprintf("SET @myrole='master%d'", $i), MYSQLND_MS_MASTER_SWITCH);
		mst_mysqli_query(3, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_LAST_USED_SWITCH);
		mst_mysqli_query(4, $link, "CREATE TABLE test(id INT) ENGINE=InnoDB", MYSQLND_MS_LAST_USED_SWITCH);
		$server['master'][$link->thread_id] = true;
	}
	if (count($server['master']) < 2)  {
		die("[005] Two connections happen to have the same thread id, ignore and run again!");
	}
	/* after this, next master is master0 (= wrap) */


	for ($i = 0; $i< 2; $i++) {
		mst_mysqli_query(6, $link, sprintf("SET @myrole='slave%d'", $i), MYSQLND_MS_SLAVE_SWITCH);
		mst_mysqli_query(6, $link, "DROP TABLE IF EXISTS test", MYSQLND_MS_LAST_USED_SWITCH);
			$server['slave'][$link->thread_id] = true;
	}
	if (count($server['slave']) < 2) {
		die("[007] Two connections happen to have the same thread id, ignore and run again!");
	}
	/* next slave is slave0 (= wrap) */

	/* master 0, slave 0 */
	printf("... plain trx commit\n");
	$link->begin_transaction();
	mst_mysqli_fech_role(mst_mysqli_query(7, $link, "SELECT @myrole AS _role"));
	mst_mysqli_query(8, $link, "INSERT INTO test(id) VALUES (1)");
	$link->commit();
	$res = mst_mysqli_query(9, $link, "SELECT MAX(id) FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	mst_mysqli_fech_role(mst_mysqli_query(9, $link, "SELECT @myrole AS _role"));

	/* master 1, slave 1, slave 1 */
	$link->begin_transaction();
	mst_mysqli_fech_role(mst_mysqli_query(10, $link, "SELECT @myrole AS _role"));
	mst_mysqli_query(11, $link, "INSERT INTO test(id) VALUES (2)");
	$link->commit();
	$res = mst_mysqli_query(12, $link, "SELECT MAX(id) FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	mst_mysqli_fech_role(mst_mysqli_query(13, $link, "SELECT @myrole AS _role"));


	/* maaster 0, master 0, slave 0 */
	printf("... plain trx rollback\n");
	$link->begin_transaction();
	mst_mysqli_fech_role(mst_mysqli_query(14, $link, "SELECT @myrole AS _role"));
	mst_mysqli_query(15, $link, "INSERT INTO test(id) VALUES (3)");
	$link->rollback();
	$res = mst_mysqli_query(16, $link, "SELECT MAX(id) AS _id FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	$row = $res->fetch_assoc();
	if ($row['_id'] > 2) {
		printf("[017] No rollback!");
	}
	mst_mysqli_fech_role(mst_mysqli_query(18, $link, "SELECT @myrole AS _role"));

	/* master 1, master 1, slave 1 */
	printf("... named trx rollback\n");
	$link->begin_transaction(0, "foobar");
	mst_mysqli_fech_role(mst_mysqli_query(19, $link, "SELECT @myrole AS _role"));
	mst_mysqli_query(20, $link, "INSERT INTO test(id) VALUES (4)");
	$link->rollback();
	$res = mst_mysqli_query(21, $link, "SELECT MAX(id) AS _id FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	$row = $res->fetch_assoc();
	if ($row['_id'] > 2) {
		printf("[022] No rollback!");
	}
	mst_mysqli_fech_role(mst_mysqli_query(23, $link, "SELECT @myrole AS _role"));

	/* master 0, master 0, master 0, slave 0 */
	printf("... named trx rollback started many times\n");
	$link->begin_transaction(0, "foobar");
	$link->begin_transaction(0, "foobar20101002928282384gdsgfhdgjhgjh");
	$link->begin_transaction(0, "abc");
	mst_mysqli_fech_role(mst_mysqli_query(19, $link, "SELECT @myrole AS _role"));
	mst_mysqli_query(20, $link, "INSERT INTO test(id) VALUES (5)");
	$link->rollback();
	$res = mst_mysqli_query(21, $link, "SELECT MAX(id) AS _id FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	$row = $res->fetch_assoc();
	if ($row['_id'] > 2) {
		printf("[022] No rollback!");
	}
	mst_mysqli_fech_role(mst_mysqli_query(23, $link, "SELECT @myrole AS _role"));

	/* master 1, master 1, master 1, slave 1 */
	printf("... named trx commit started many times\n");
	$link->begin_transaction(0, "foobar");
	$link->begin_transaction(0, "foobar20101002928282384gdsgfhdgjhgjh");
	$link->begin_transaction(0, "abc");
	mst_mysqli_fech_role(mst_mysqli_query(24, $link, "SELECT @myrole AS _role"));
	mst_mysqli_query(25, $link, "INSERT INTO test(id) VALUES (6)");
	$link->commit();
	$res = mst_mysqli_query(26, $link, "SELECT MAX(id) AS _id FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	mst_mysqli_fech_role(mst_mysqli_query(28, $link, "SELECT @myrole AS _role"));

	/* May use slave */
	/* slave 0 */
	printf("... read only transaction commit\n");
	$link->begin_transaction(MYSQLI_TRANS_START_READ_ONLY);
	mst_mysqli_fech_role(mst_mysqli_query(29, $link, "SELECT @myrole AS _role"));
	$link->commit();

	/* slave 1 */
	printf("... read only transaction rollback\n");
	$link->begin_transaction(MYSQLI_TRANS_START_READ_ONLY);
	mst_mysqli_fech_role(mst_mysqli_query(30, $link, "SELECT @myrole AS _role"));
	$link->commit();

	/* slave 0, master 0, master 0 */
	printf("... named read only transaction commit\n");
	$link->begin_transaction(MYSQLI_TRANS_START_READ_ONLY, "a");
	$link->begin_transaction(MYSQLI_TRANS_START_READ_ONLY, "abcdefghijklmnopqrstuvwxyz");
	mst_mysqli_fech_role(mst_mysqli_query(31, $link, "SELECT @myrole AS _role"));
	$link->commit();

	printf("... autocommit off, begin, commit\n");
	$link->autocommit(false);
	$link->begin_transaction(MYSQLI_TRANS_START_READ_WRITE, "aobc");
	mst_mysqli_fech_role(mst_mysqli_query(32, $link, "SELECT @myrole AS _role"));
	$link->commit();
	/* switch between trx: master 1, last used ignored! */
	$res = mst_mysqli_query(33, $link, "SELECT @@autocommit AS _autocommit FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	mst_mysqli_fech_role(mst_mysqli_query(34, $link, "SELECT @myrole AS _role"));

	/* master 0, master 0, master 1 */
	printf("... autocommit off, begin, rollback\n");
	$link->autocommit(false);
	$link->begin_transaction(MYSQLI_TRANS_START_READ_WRITE, "aobr");
	mst_mysqli_fech_role(mst_mysqli_query(35, $link, "SELECT @myrole AS _role"));
	mst_mysqli_query(36, $link, "INSERT INTO test(id) VALUES (7)");
	$link->rollback();

;
	/* switch between trx?, master 0 */
	$res = mst_mysqli_query(37, $link, "SELECT MAX(id) AS _id FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(38, $link, "SELECT @@autocommit AS _autocommit FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());

	/* master 1, master 1, master 1 */
	printf("... autocommit on, begin, rollback\n");
	$link->autocommit(true);
	$link->begin_transaction(MYSQLI_TRANS_START_READ_WRITE, "aonbr");
	mst_mysqli_fech_role(mst_mysqli_query(38, $link, "SELECT @myrole AS _role"));
	mst_mysqli_query(39, $link, "INSERT INTO test(id) VALUES (8)");
	$link->rollback();
	$res = mst_mysqli_query(40, $link, "SELECT MAX(id) AS _id FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(41, $link, "SELECT @@autocommit AS _autocommit FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());

	printf("... autocommit on, begin, begin (= implicit commit), rollback\n");
	$link->autocommit(true);
	$last = NULL;
	for ($i = 0; $i < 3; $i++) {
		/* master 0, master 1, master 0 */
		$link->begin_transaction(MYSQLI_TRANS_START_READ_WRITE, "aonbbr");
		mst_mysqli_query(43, $link, "INSERT INTO test(id) VALUES (9)");
		/* implicit commit */
		$link->begin_transaction(MYSQLI_TRANS_START_READ_WRITE, "aonbbr");
	}

	/* one of the masters */
	mst_mysqli_fech_role(mst_mysqli_query(44, $link, "SELECT @myrole AS _role"));

	/* master 1, master 1, master 1 */
	$link->begin_transaction(MYSQLI_TRANS_START_READ_WRITE, "aonbbr");
	mst_mysqli_fech_role(mst_mysqli_query(45, $link, "SELECT @myrole AS _role"));
	mst_mysqli_query(46, $link, "INSERT INTO test(id) VALUES (10)");
	$link->rollback();

	$res = mst_mysqli_query(47, $link, "SELECT MAX(id) AS _id FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());
	$res = mst_mysqli_query(48, $link, "SELECT @@autocommit AS _autocommit FROM test", MYSQLND_MS_LAST_USED_SWITCH);
	var_dump($res->fetch_assoc());


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_begin_mysqli_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_begin_mysqli_rr.ini'.\n");

	if ($error = mst_mysqli_drop_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %d\n", $error);
?>
--EXPECTF--
... plain trx commit
This is 'master0' speaking
array(1) {
  ["MAX(id)"]=>
  string(1) "1"
}
This is 'slave0' speaking
This is 'master1' speaking
array(1) {
  ["MAX(id)"]=>
  string(1) "2"
}
This is 'slave1' speaking
... plain trx rollback
This is 'master0' speaking
This is 'slave0' speaking
... named trx rollback
This is 'master1' speaking
This is 'slave1' speaking
... named trx rollback started many times
This is 'master0' speaking
This is 'slave0' speaking
... named trx commit started many times
This is 'master1' speaking
array(1) {
  ["_id"]=>
  string(1) "6"
}
This is 'slave1' speaking
... read only transaction commit
This is 'slave0' speaking
... read only transaction rollback
This is 'slave1' speaking
... named read only transaction commit
This is 'slave0' speaking
... autocommit off, begin, commit
This is 'master0' speaking
array(1) {
  ["_autocommit"]=>
  string(1) "0"
}
This is 'master1' speaking
... autocommit off, begin, rollback
This is 'master0' speaking
array(1) {
  ["_id"]=>
  string(1) "6"
}
array(1) {
  ["_autocommit"]=>
  string(1) "0"
}
... autocommit on, begin, rollback
This is 'master0' speaking
array(1) {
  ["_id"]=>
  string(1) "6"
}
array(1) {
  ["_autocommit"]=>
  string(1) "1"
}
... autocommit on, begin, begin (= implicit commit), rollback
This is 'master0' speaking
This is 'master1' speaking
array(1) {
  ["_id"]=>
  string(1) "9"
}
array(1) {
  ["_autocommit"]=>
  string(1) "1"
}
done!