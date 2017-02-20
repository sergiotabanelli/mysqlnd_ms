--TEST--
PECL #22784 - mysql_select_db won't work
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysql"));

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
if ($error = mst_create_config("test_mysqlnd_ms_bug_pecl_22784.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_bug_pecl_22784.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* without MS */
	$link = @my_mysql_connect($host, $user, $passwd, NULL, $port, $socket);
	if (!$link)
		printf("[001] [[%d] %s\n", mysql_errno(), mysql_error());

	$select_ok_wo_ms = mysql_select_db($db, $link);

	if (!$res = mysql_query("SELECT DATABASE() AS _db", $link))
		printf("[003] [%d] %s\n", mysql_errno($link), mysql_error($link));

	if ($row = mysql_fetch_assoc($res)) {
		if ($row['_db'] != $db)
			printf("[005] Expecting DB '%s' got '%s'\n", $db, $row['_db']);
	} else {
		printf("[004] [%d] %s\n", mysql_errno($link), mysql_error($link));
	}
	mysql_close($link);

	/* with MS */
	$link = @my_mysql_connect($host, $user, $passwd, NULL, $port, $socket);
	if (!$link)
		printf("[005] [[%d] %s\n", mysql_errno(), mysql_error());

	$select_ok_w_ms = mysql_select_db($db, $link);

	if (!$res = mysql_query("SELECT DATABASE() AS _db", $link))
		printf("[006] [%d] %s\n", mysql_errno($link), mysql_error($link));

	if ($row = mysql_fetch_assoc($res)) {
		if ($row['_db'] != $db)
			printf("[007] Expecting DB '%s' got '%s'\n", $db, $row['_db']);
	} else {
		printf("[008] [%d] %s\n", mysql_errno($link), mysql_error($link));
	}
	mysql_close($link);

	if ($select_ok_w_ms != $select_ok_wo_ms) {
		printf("[009] select_db w/o MS: %d, select_db w MS: %d\n", $select_ok_wo_ms, $select_ok_w_ms);
	}

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");

	if (!unlink("test_mysqlnd_ms_bug_pecl_22784.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_bug_pecl_22784.ini'.\n");
?>
--EXPECTF--
done!