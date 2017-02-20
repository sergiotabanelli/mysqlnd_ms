--TEST--
mysqlnd_ms_get_last_gtid() params
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

include_once("util.inc");
$sql = mst_get_gtid_sql($db);

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $master_host_only,
				'port' 		=> (int)$master_port,
				'socket' 	=> $master_socket,
			),
		),
		'slave' => array(
			"slave1" => array(
				'host' 	=> $slave_host_only,
				'port' 	=> (int)$slave_port,
				'socket' => $slave_socket,
			),
		),

		'global_transaction_id_injection' => array(
			'on_commit'	 				=> $sql['update'],
			'fetch_last_gtid'			=> $sql['fetch_last_gtid'],
			'check_for_gtid'			=> $sql['check_for_gtid'],
			'report_error'				=> true,
		),

		'lazy_connections' => 1,
		'trx_stickiness' => 'disabled',
		'filters' => array(
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_get_last_gtid_params.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_get_last_gtid_params.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = NULL;

	if (NULL !== ($ret = @mysqlnd_ms_get_last_gtid()))
		printf("[001] Expecting NULL, got %s\n", var_export($ret, true));

	if (NULL !== ($ret = @mysqlnd_ms_get_last_gtid($link, $link)))
		printf("[002] Expecting NULL, got %s\n", var_export($ret, true));

	if (false !== ($ret = mysqlnd_ms_get_last_gtid($link)))
		printf("[003] Expecting false, got %s\n", var_export($ret, true));


	$link = mysqli_init();
	if (false !== ($ret = mysqlnd_ms_get_last_gtid($link))) {
		printf("[004] Expecting false, got %s\n", var_export($ret, true));
	} else {
		printf("[005] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
		printf("[006] [%d] %s\n", $link->errno, $link->error);
	}

	/* non MS connection */
	$link = mst_mysqli_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
	if (mysqli_connect_errno()) {
		printf("[007] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	if (false !== ($ret = mysqlnd_ms_get_last_gtid($link))) {
		printf("[008] Expecting false, got %s\n", var_export($ret, true));
		printf("[009] [%d] %s\n", $link->errno, $link->error);
	} else {
		printf("[010] [%d] %s\n", $link->errno, $link->error);
	}

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[011] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* no connection selected and lazy */
	if (false !== ($ret = mysqlnd_ms_get_last_gtid($link))) {
		printf("[012] Expecting false, got %s\n", var_export($ret, true));
	} else {
		printf("[013] [%d] %s\n", $link->errno, $link->error);
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_get_last_gtid_params.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_get_last_gtid_params.ini'.\n");
?>
--EXPECTF--

Warning: mysqlnd_ms_get_last_gtid(): (mysqlnd_ms) %s in %s on line %d
[005] [0%A
[006] [0%A

Warning: mysqlnd_ms_get_last_gtid(): (mysqlnd_ms) %s in %s on line %d
[010] [0%A

Warning: mysqlnd_ms_get_last_gtid(): (mysqlnd_ms) %s in %s on line %d
[013] [0%A
done!