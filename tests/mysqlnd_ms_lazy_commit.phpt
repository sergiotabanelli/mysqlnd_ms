--TEST--
lazy connections and commit
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

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

		'lazy_connections' => 1,
		'filters' => array(
			"random" => array('sticky' => '1'),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_lazy_commit.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_lazy_commit.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* intentionally NOT ms - checking what happens w/o MS */
	$link = mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* commit prior to any command... */
	if (!($wo_ms = $link->commit()))
		printf("[003] [%d] %s\n", $link->errno, $link->error);

	$link->close();

	/* Now MS */
	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[004] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	if (!($w_ms = $link->commit()))
		printf("[005] [%d] %s\n", $link->errno, $link->error);

	$link->close();

	if ($wo_ms != $w_ms)
		printf("[006] Different behaviour with and without MS!\n");

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");

	if (!unlink("test_mysqlnd_ms_lazy_commit.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_lazy_commit.ini'.\n");
?>
--EXPECTF--
done!