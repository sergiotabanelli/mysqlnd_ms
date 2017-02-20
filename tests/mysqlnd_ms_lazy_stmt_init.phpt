--TEST--
lazy connections and stmt init
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
if ($error = mst_create_config("test_mysqlnd_ms_lazy_stmt_init.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_lazy_stmt_init.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	if (!$link->dump_debug_info())
		printf("[003] [%d] %s\n", $link->errno, $link->error);

	if (!($stmt = $link->stmt_init()))
		printf("[004] [%d] %s\n", $link->errno, $link->error);

	if (!($stmt = $link->stmt_init()))
		printf("[005] [%d] %s\n", $link->errno, $link->error);

	/* line useable ?! */
	if (!$link->dump_debug_info())
		printf("[006] [%d] %s\n", $link->errno, $link->error);


	if (is_object($stmt)) {
		$one = NULL;
		if (!$stmt->prepare("SELECT 1 AS _one FROM DUAL") ||
			!$stmt->execute() ||
			!$stmt->bind_result($one) ||
			!$stmt->fetch())
		{
			printf("[007] [%d] '%s'\n", $stmt->errno, $stmt->error);
		} else {
			printf("[008] _one = %s\n", $one);
			if ($stmt->fetch()) {
				printf("[008] More data than expected");
			}
		}
	}
	if (!($stmt = $link->stmt_init()))
		printf("[011] [%d] %s\n", $link->errno, $link->error);

	if (is_object($stmt)) {
		$one = "42";
		if (!$stmt->prepare("SET @a=?") ||
			!$stmt->bind_param("s", $one) ||
			!$stmt->execute())
		{
			printf("[012] [%d] '%s'\n", $stmt->errno, $stmt->error);
		} else {
			if ($res = mst_mysqli_query(13, $link, "SELECT @a", MYSQLND_MS_LAST_USED_SWITCH)) {
				var_dump($res->fetch_assoc());
			}
		}
	}

	if ($res = mst_mysqli_query(14, $link, "SELECT 1 FROM DUAL"))
		var_dump($res->fetch_assoc());

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");

	if (!unlink("test_mysqlnd_ms_lazy_stmt_init.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_lazy_stmt_init.ini'.\n");
?>
--EXPECTF--
[008] _one = 1
array(1) {
  ["@a"]=>
  string(2) "42"
}
array(1) {
  [1]=>
  string(1) "1"
}
done!