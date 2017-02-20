--TEST--
Fabric: mysqlnd_ms_dump_fabric_rpc_hosts()
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");
require_once("util.inc");

if (!getenv("MYSQL_TEST_FABRIC")) {
	die(sprintf("SKIP Fabric - set MYSQL_TEST_FABRIC=1 (config.inc) to enable\n"));
}

$process = ms_fork_emulated_fabric_server();
if (is_string($process)) {
	die(sprintf("SKIP %s\n", $process));
}
ms_emulated_fabric_server_shutdown($process);

$settings = array(
	"myapp" => array(
		'fabric' => array(
			'hosts' => array(
				array('host' => getenv("MYSQL_TEST_FABRIC_EMULATOR_HOST"), 'port' => getenv("MYSQL_TEST_FABRIC_EMULATOR_PORT"))
			),
			'timeout' => 2
		)
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_fabric_dump_fabric_hosts.ini", $settings))
  die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_fabric_dump_fabric_hosts.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (NULL !== ($ret = @mysqlnd_ms_dump_fabric_rpc_hosts())) {
		printf("[001] Expecting NULL, got %s\n", var_export($ret, true));
	}
	$link = array();
	if (false !== ($ret = @mysqlnd_ms_dump_fabric_rpc_hosts($link))) {
		printf("[002] Expecting NULL, got %s\n", var_export($ret, true));
	}
	if (!($link = mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
		printf("[003] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}
	if (NULL !== ($ret = @mysqlnd_ms_dump_fabric_rpc_hosts($link, $link))) {
		printf("[004] Expecting NULL, got %s\n", var_export($ret, true));
	}

	$process = ms_fork_emulated_fabric_server();
	if (is_string($process)) {
		printf("[005] PANIC - is there a phantom server? %s\n", $process);
	}
	/* Give the system some breath time */
	sleep(1);

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (0 !== mysqli_connect_errno())
		printf("[006] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$hosts = mysqlnd_ms_dump_fabric_rpc_hosts($link);
	if (!is_array($hosts) || count($hosts) > 1) {
		printf("[007] Host list is suspicious, dumping\n");
		var_dump($hosts);
	}
	$host = $hosts[0];
	if ($host['hostname'] != getenv("MYSQL_TEST_FABRIC_EMULATOR_HOST")) {
		printf("[008] Expecting hostname = '%s' got '%s'\n",
			getenv("MYSQL_TEST_FABRIC_EMULATOR_HOST"),
			$host['hostname']);
		var_dump($hosts);
	}
	unset($hosts[0]['hostname']);

	if ($host['port'] != getenv("MYSQL_TEST_FABRIC_EMULATOR_PORT")) {
		printf("[009] Expecting port = '%s' got '%s'\n",
			getenv("MYSQL_TEST_FABRIC_EMULATOR_PORT"),
			$host['hostname']);
		var_dump($hosts);
	}
	unset($hosts[0]['port']);
	var_dump($hosts);

	ms_emulated_fabric_server_shutdown($process);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_fabric_dump_fabric_hosts.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_fabric_dump_fabric_hosts.ini'.\n");
?>
--EXPECTF--
array(1) {
  [0]=>
  array(0) {
  }
}
done!