--TEST--
Thread id
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");
if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

include_once("util.inc");
$ret = mst_is_slave_of($emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket, $emulated_master_host_only, $emulated_master_port, $emulated_master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true == $ret)
	die("SKIP Configured emulated master and emulated slave could be part of a replication cluster\n");

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host, $emulated_slave_host),
		'pick' => array("roundrobin"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_warning_count.ini", $settings))
  die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[1,2]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_warning_count.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
	$threads = array();

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (0 !== mysqli_connect_errno())
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!mysqli_query($link, "DROP TABLE IF EXISTS this_table_does_not_exist"))
		printf("[002] [%d] %s\n", mysqli_errno($link), mysqli_error($link));

	if (1 !== ($tmp = mysqli_warning_count($link)))
		printf("[003] Expecting warning count = 1, got %d\n", $tmp);

	$threads[mst_mysqli_get_emulated_id(4, $link)] = 'master (1)';

	if (!mysqli_query($link, "SELECT 1 FROM DUAL"))
		printf("[005] [%d] %s\n", mysqli_errno($link), mysqli_error($link));

	if (0 !== ($tmp = mysqli_warning_count($link)))
		printf("[006] Expecting warning count = 0, got %d\n", $tmp);

	 $threads[mst_mysqli_get_emulated_id(7, $link)] = 'slave 1';

	 if (!mysqli_query($link, "SELECT 1 FROM DUAL"))
		printf("[008] [%d] %s\n", mysqli_errno($link), mysqli_error($link));

	if (0 !== ($tmp = mysqli_warning_count($link)))
		printf("[009] Expecting warning count = 0, got %d\n", $tmp);

	$threads[mst_mysqli_get_emulated_id(10, $link)] = 'slave 2';

	if (!mysqli_query($link, "/*" . MYSQLND_MS_MASTER_SWITCH . "*/SELECT 1 FROM DUAL"))
		printf("[011] [%d] %s\n", mysqli_errno($link), mysqli_error($link));

	if (0 !== ($tmp = mysqli_warning_count($link)))
		printf("[012] Expecting warning count = 0, got %d\n", $tmp);

 	$threads[mst_mysqli_get_emulated_id(13, $link)] = 'master (2)';

	if (!mysqli_query($link, "/*" . MYSQLND_MS_SLAVE_SWITCH . "*/DROP TABLE IF EXISTS this_table_does_not_exist"))
		printf("[014] [%d] %s\n", mysqli_errno($link), mysqli_error($link));

	if (1 !== ($tmp = mysqli_warning_count($link)))
		printf("[015] Expecting warning count = 1, got %d\n", $tmp);

 	$threads[mst_mysqli_get_emulated_id(16, $link)] = 'slave 1 (2)';

	if (!mysqli_query($link, "/*" . MYSQLND_MS_SLAVE_SWITCH . "*/DROP TABLE IF EXISTS this_table_does_not_exist"))
		printf("[017] [%d] %s\n", mysqli_errno($link), mysqli_error($link));

	if (1 !== ($tmp = mysqli_warning_count($link)))
		printf("[018] Expecting warning count = 1, got %d\n", $tmp);

 	$threads[mst_mysqli_get_emulated_id(19, $link)] = 'slave 2 (2)';


	foreach ($threads as $server_id => $label)
		printf("%d - %s\n", $server_id, $label);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_warning_count.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_warning_count.ini'.\n");
?>
--EXPECTF--
%s - master (2)
%s - slave 1 (2)
%s - slave 2 (2)
done!