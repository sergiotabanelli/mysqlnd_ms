--TEST--
Prepare and switch
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

if (($master_host == $slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
		'filters' => array(
			"roundrobin" => array(),
		),
		'lazy_connections' => 1
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_prepare_switch.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_prepare_switch.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!$link->query("DROP TABLE IF EXISTS test") ||
		!$link->query("CREATE TABLE test(id INT)") ||
		!$link->query("INSERT INTO test(id) VALUES (1)"))
		printf("[002] [%d] %s\n", $link->errno, $link->error);

	if (!($stmt_master = $link->prepare(sprintf("/*%s*/SELECT id as _one FROM test", MYSQLND_MS_MASTER_SWITCH)))) {
		printf("[003] [%d] %s\n", $link->errno, $link->error);
	}

	if (!($stmt_slave = $link->prepare("SELECT 2 AS _two FROM DUAL"))) {
		printf("[004] [%d] %s\n", $link->errno, $link->error);
	}

	if (!$stmt_master->execute())
		printf("[005] [%d] %s\n", $stmt_master->errno, $stmt_master->error);

	if (!$stmt_slave->execute())
		printf("[006] [%d] %s\n", $stmt_slave->errno, $stmt_slave->error);

	if (!($res_master = $stmt_master->get_result()))
		printf("[007] [%d] %s\n", $stmt_master->errno, $stmt_master->error);

	if (!($res_slave = $stmt_slave->get_result()))
		printf("[008] [%d] %s\n", $stmt_slave->errno, $stmt_slave->error);

	var_dump($res_slave->fetch_all());
	var_dump($res_master->fetch_all());

	/* resource reusage without close etc */
	if (!($stmt_master = $link->prepare(sprintf("/*%s*/SELECT id as _one FROM test", MYSQLND_MS_MASTER_SWITCH)))) {
		printf("[009] [%d] %s\n", $link->errno, $link->error);
	}

	if (!($stmt_slave = $link->prepare("SELECT 1 AS _one FROM DUAL"))) {
		printf("[010] [%d] %s\n", $link->errno, $link->error);
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_prepare_switch.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_prepare_switch.ini'.\n");
?>
--EXPECTF--
array(1) {
  [0]=>
  array(1) {
    [0]=>
    int(2)
  }
}
array(1) {
  [0]=>
  array(1) {
    [0]=>
    int(1)
  }
}
done!