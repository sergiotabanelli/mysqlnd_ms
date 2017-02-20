--TEST--
PDO::ATTR_AUTOCOMMIT
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP 5.4.0 or newer, using " . PHP_VERSION));

_skipif_check_extensions(array("pdo_mysql"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array(
		"master_0" =>
				array(
					'host' 		=> $emulated_master_host_only,
					'port' 		=> $emulated_master_port,
					'socket' 	=> $emulated_master_socket,
				),
		),
		'slave' => array($emulated_slave_host, $emulated_slave_host),
		'trx_stickiness' => 'master',
		'pick' => array("roundrobin"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pdo_attr_autocommit.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pdo_attr_autocommit.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function test_pdo($host, $user, $passwd, $db, $port, $socket, $options) {

		try {

			/* PDO::query() */
			$pdo = my_pdo_connect($host, $user, $passwd, $db, $port, $socket, $options);
			$pdo->exec(sprintf("SET @myrole='master'"));
			$pdo->exec(sprintf("/*%s*/SET @myrole='slave1'", MYSQLND_MS_SLAVE_SWITCH));
			$pdo->exec(sprintf("/*%s*/SET @myrole='slave2'", MYSQLND_MS_SLAVE_SWITCH));

			/* slave 1 */
			$stmt = $pdo->prepare("SELECT @myrole AS _role");
			$stmt->execute();
			$result = $stmt->fetchAll(PDO::FETCH_ASSOC);
			var_dump($result[0]['_role']);

			$stmt = $pdo->prepare("SELECT @myrole AS _role");
			$stmt->execute();
			$result = $stmt->fetchAll(PDO::FETCH_ASSOC);
			var_dump($result[0]['_role']);

			/* master */
			$pdo->setAttribute(PDO::ATTR_AUTOCOMMIT, false);

			$stmt = $pdo->prepare("SELECT @myrole AS _role");
			$stmt->execute();
			$result = $stmt->fetchAll(PDO::FETCH_ASSOC);
			var_dump($result[0]['_role']);

			/* slave1 */
			$pdo->setAttribute(PDO::ATTR_AUTOCOMMIT, true);
			$stmt = $pdo->prepare("SELECT @myrole AS _role");
			$stmt->execute();
			$result = $stmt->fetchAll(PDO::FETCH_ASSOC);
			var_dump($result[0]['_role']);


		} catch (Exception $e) {
			printf("[001] %s\n", $e->__toString());
		}

	}

	/* $options = array(PDO::MYSQL_ATTR_INIT_COMMAND => "SET NAMES utf8", PDO::ATTR_EMULATE_PREPARES => true); */

	$options = array(PDO::ATTR_EMULATE_PREPARES => false, );
	test_pdo("myapp", $user, $passwd, $db, $port, $socket, $options);
	$options = array(PDO::ATTR_EMULATE_PREPARES => false);
	test_pdo("myapp", $user, $passwd, $db, $port, $socket, $options);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pdo_attr_autocommit.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pdo_attr_autocommit.ini'.\n");
?>
--EXPECTF--
string(6) "slave1"
string(6) "slave2"
string(6) "master"
string(6) "slave1"
string(6) "slave1"
string(6) "slave2"
string(6) "master"
string(6) "slave1"
done!