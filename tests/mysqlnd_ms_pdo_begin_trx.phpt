--TEST--
PDO::beginTransaction
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

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
					'db'		=> $db,
					'user'		=> $user,
					'password'	=> $passwd,
				),
			"master_1" =>
				array(
					'host' 	=> $emulated_slave_host_only,
					'port' 	=> $emulated_slave_port,
					'socket' 	=> $emulated_slave_socket,
					'db'		=> $db,
					'user'	=> $user,
					'password'=> $passwd,
				),
			),
		'slave' => array(),
		'failover' => array('strategy' => 'master'),
	),
);
if ($error = mst_create_config("test_pdo_begin_trx.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_pdo_begin_trx.ini
mysqlnd_ms.multi_master=1
mysqlnd_ms.disable_rw_split=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function test_pdo($host, $user, $passwd, $db, $port, $socket, $options) {

		try {

			$pdo = my_pdo_connect($host, $user, $passwd, $db, $port, $socket, $options);
			$pdo->beginTransaction();
			$ret = $pdo->query("SELECT @@hostname AS _hostname");
			$row = $ret->fetch(PDO::FETCH_ASSOC);
			var_dump($row['_hostname']);
			$pdo->commit();

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
	if (!unlink("test_pdo_begin_trx.ini"))
	  printf("[clean] Cannot unlink ini file 'test_pdo_begin_trx.ini'.\n");
?>
--EXPECTF--
string(%d) "%s"
string(%d) "%s"
done!
