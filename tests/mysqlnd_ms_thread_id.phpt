--TEST--
Thread id
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host, $slave_host),
		'pick' => array("roundrobin"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_thread_id.ini", $settings))
  die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_thread_id.ini
--FILE--
<?php
	require_once("connect.inc");

	function check_thread_id($offset, $link, $query) {
	  if (!$link->query($query)) {
		  printf("[%03d + 01] [%d] %s\n", $offset, $link->errno, $link->error);
		  return false;
	  }

	  if (!$res = $link->query(sprintf("/*%s*/SELECT CONNECTION_ID() AS _thread", MYSQLND_MS_LAST_USED_SWITCH))) {
		  printf("[%03d + 02] [%d] %s\n", $offset, $link->errno, $link->error);
		  return false;
	  }

	  $row = $res->fetch_assoc();
	  if ($link->thread_id != $row['_thread']) {
		  printf("[%03d + 03] Expecting thread id %d got %d\n", $offset, $link->thread_id, $row['_thread']);
	  }
	  return $link->thread_id;
	}

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (0 !== mysqli_connect_errno())
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$threads = array();

	$threads[check_thread_id(10, $link, "DROP TABLE IF EXISTS test")]= "master";
	$threads[check_thread_id(20, $link, "SELECT 'Slave1'")] = "slave 1";
	$threads[check_thread_id(30, $link, "SELECT 'Slave2'")] = "slave 2";

	foreach ($threads as $thread_id => $role)
		printf("%d %s\n", $thread_id, $role);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_thread_id.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_thread_id.ini'.\n");
?>
--EXPECTF--
%d master
%d slave 1
%d slave 2
done!