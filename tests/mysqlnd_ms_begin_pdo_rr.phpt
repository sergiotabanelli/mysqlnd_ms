--TEST--
beginTransaction, trx_stickiness=on (UNSUPPORTED!)
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.4.99-dev', '<'))
	die(sprintf("SKIP Requires PHP 5.5.0 or newer, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("pdo_mysql"));

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host, $emulated_master_host),
		'slave' => array($emulated_slave_host, $emulated_slave_host),
		'trx_stickiness' => 'on',
		'pick' => array("roundrobin"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_begin_pdo_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));


_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.multi_master=1
mysqlnd_ms.config_file=test_mysqlnd_ms_begin_pdo_rr.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function test_pdo($host, $user, $passwd, $db, $port, $socket, $options) {

		try {

			$pdo = my_pdo_connect($host, $user, $passwd, $db, $port, $socket, $options);
			$server = array();

			for ($i = 0; $i < 2; $i++) {
				$pdo->query(sprintf("/*%s*/SET @myrole='master%d'", MYSQLND_MS_MASTER_SWITCH, $i));
				$pdo->query(sprintf("/*%s*/DROP TABLE IF EXISTS test", MYSQLND_MS_LAST_USED_SWITCH));
				$pdo->query(sprintf("/*%s*/CREATE TABLE test(id INT) ENGINE=InnoDB", MYSQLND_MS_LAST_USED_SWITCH));
			}
			/* after this, next master is master0 (= wrap) */


			for ($i = 0; $i< 2; $i++) {
				$pdo->query(sprintf("/*%s*/SET @myrole='slave%d'", MYSQLND_MS_SLAVE_SWITCH, $i));
				$pdo->query(sprintf("/*%s*/DROP TABLE IF EXISTS test", MYSQLND_MS_LAST_USED_SWITCH));
				$pdo->query(sprintf("/*%s*/CREATE TABLE test(id INT) ENGINE=InnoDB", MYSQLND_MS_LAST_USED_SWITCH));
			}
			/* next slave is slave0 (= wrap) */

			/* master 0, slave 0 */
			$pdo->beginTransaction();
			var_dump($pdo->query("SELECT @myrole AS _role")->fetch(PDO::FETCH_ASSOC)['_role']);
			var_dump($pdo->query("SELECT @myrole AS _role")->fetch(PDO::FETCH_ASSOC)['_role']);
			$pdo->query("INSERT INTO test(id) VALUES(1)");
			$pdo->commit();
			var_dump($pdo->query(sprintf("/*%s*/SELECT MAX(id) FROM test", MYSQLND_MS_LAST_USED_SWITCH))->fetch(PDO::FETCH_ASSOC));
			var_dump($pdo->query("SELECT @myrole AS _role")->fetch(PDO::FETCH_ASSOC)['_role']);


			/* master 1, slave 1, slave 1 */
			$pdo->beginTransaction();
			var_dump($pdo->query("SELECT @myrole AS _role")->fetch(PDO::FETCH_ASSOC)['_role']);
			var_dump($pdo->query("SELECT @myrole AS _role")->fetch(PDO::FETCH_ASSOC)['_role']);
			$pdo->query("INSERT INTO test(id) VALUES(1)");
			$pdo->commit();
			var_dump($pdo->query(sprintf("/*%s*/SELECT MAX(id) FROM test", MYSQLND_MS_LAST_USED_SWITCH))->fetch(PDO::FETCH_ASSOC));
			var_dump($pdo->query("SELECT @myrole AS _role")->fetch(PDO::FETCH_ASSOC)['_role']);


		} catch (Exception $e) {
			printf("[001] %s\n", $e->__toString());
		}

	}

	$options = array(PDO::ATTR_EMULATE_PREPARES => true);
	test_pdo("myapp", $user, $passwd, $db, $port, $socket, $options);
	$options = array(PDO::ATTR_EMULATE_PREPARES => false);
	test_pdo("myapp", $user, $passwd, $db, $port, $socket, $options);



	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_begin_pdo_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_begin_pdo_rr.ini'.\n");

	if ($error = mst_mysqli_drop_test_table($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))
		printf("[clean] %d\n", $error);
?>
--XFAIL--
Unsupported - PDO has not yet been modified to use appropriate mysqlnd calls
--EXPECTF--
Unsupported feature
string(6) "slave0"
string(6) "slave1"
array(1) {
  ["MAX(id)"]=>
  string(1) "1"
}
string(6) "slave0"
string(6) "slave1"
string(6) "slave0"
array(1) {
  ["MAX(id)"]=>
  string(1) "1"
}
string(6) "slave1"
string(6) "slave0"
string(6) "slave1"
array(1) {
  ["MAX(id)"]=>
  int(1)
}
string(6) "slave0"
string(6) "slave1"
string(6) "slave0"
array(1) {
  ["MAX(id)"]=>
  int(1)
}
string(6) "slave1"
done!
