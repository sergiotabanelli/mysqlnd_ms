--TEST--
PDO Basics
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("pdo_mysql"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pdo_basics.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pdo_basics.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function test_pdo($host, $user, $passwd, $db, $port, $socket, $options) {

		try {

			/* PDO::query() */
			$pdo = $pdo = my_pdo_connect($host, $user, $passwd, $db, $port, $socket, $options);
			$stmt = $pdo->query("SELECT 1 AS _one");
			$result_slave = $stmt->fetchAll(PDO::FETCH_ASSOC);
			var_dump($result_slave);

			$stmt = $pdo->query(sprintf("/*%s*/SELECT 1 AS _one", MYSQLND_MS_MASTER_SWITCH));
			$result_master = $stmt->fetchAll(PDO::FETCH_ASSOC);
			if ($result_master != $result_slave) {
				printf("[001] Master and slave data differ, dumping\n");
				var_dump($result_master);
				var_dump($result_slave);
			}

			/* PDO::exec() */
			$pdo = $pdo = my_pdo_connect($host, $user, $passwd, $db, $port, $socket, $options);
			$pdo->exec(sprintf("/*%s*/SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH));
			$pdo->exec(sprintf("/*%s*/SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH));

			/* PDO::prepare() */
			$stmt = $pdo->prepare("SELECT @myrole AS _role, ?");
			$stmt->execute(array("poor guy"));
			$result = $stmt->fetch(PDO::FETCH_ASSOC);
			var_dump($result);

			$stmt = $pdo->prepare(sprintf("/*%s*/SELECT @myrole AS _role, ?", MYSQLND_MS_MASTER_SWITCH));
			$stmt->execute(array("not so poor guy"));
			$result = $stmt->fetch(PDO::FETCH_ASSOC);
			var_dump($result);

			$stmt = $pdo->prepare(sprintf("/*%s*/SELECT @myrole AS _role, ?", MYSQLND_MS_LAST_USED_SWITCH));
			$stmt->execute(array("rich guy"));
			$result = $stmt->fetch(PDO::FETCH_ASSOC);
			var_dump($result);

			$pdo->exec(sprintf("DROP TABLE IF EXISTS test"));
			$pdo->exec(sprintf("CREATE TABLE test(id INT)"));
			var_dump($pdo->exec(sprintf("INSERT INTO test(id) VALUES (1)")));
			var_dump($pdo->query(sprintf("/*%s*/SELECT * FROM test", MYSQLND_MS_MASTER_SWITCH))->fetch(PDO::FETCH_ASSOC));
			var_dump($pdo->exec(sprintf("DROP TABLE IF EXISTS test")));

		} catch (Exception $e) {
			printf("[001] %s\n", $e->__toString());
		}

	}

	/* $options = array(PDO::MYSQL_ATTR_INIT_COMMAND => "SET NAMES utf8", PDO::ATTR_EMULATE_PREPARES => true); */

	$options = array(PDO::ATTR_EMULATE_PREPARES => true);
	test_pdo("myapp", $user, $passwd, $db, $port, $socket, $options);
	$options = array(PDO::ATTR_EMULATE_PREPARES => false);
	test_pdo("myapp", $user, $passwd, $db, $port, $socket, $options);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pdo_basics.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pdo_basics.ini'.\n");
?>
--EXPECTF--
array(1) {
  [0]=>
  array(1) {
    ["_one"]=>
    string(1) "1"
  }
}
array(2) {
  ["_role"]=>
  string(5) "slave"
  ["poor guy"]=>
  string(8) "poor guy"
}
array(2) {
  ["_role"]=>
  string(6) "master"
  ["not so poor guy"]=>
  string(15) "not so poor guy"
}
array(2) {
  ["_role"]=>
  string(6) "master"
  ["rich guy"]=>
  string(8) "rich guy"
}
int(1)
array(1) {
  ["id"]=>
  string(1) "1"
}
int(0)
array(1) {
  [0]=>
  array(1) {
    ["_one"]=>
    %s1%s
  }
}
array(2) {
  ["_role"]=>
  string(5) "slave"
  ["?"]=>
  string(8) "poor guy"
}
array(2) {
  ["_role"]=>
  string(6) "master"
  ["?"]=>
  string(15) "not so poor guy"
}
array(2) {
  ["_role"]=>
  string(6) "master"
  ["?"]=>
  string(8) "rich guy"
}
int(1)
array(1) {
  ["id"]=>
  int(1)
}
int(0)
done!