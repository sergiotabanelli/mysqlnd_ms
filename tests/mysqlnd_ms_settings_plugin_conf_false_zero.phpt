--TEST--
JSON '0' == false
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
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host, $emulated_master_host),
		'pick' => array("random" => array('sticky' => '0')),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_settings_plugin_conf_false_zero.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

require_once("util.inc");
msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[1]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_settings_plugin_conf_false_zero.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* shall use host = forced_master_hostname_abstract_name from the ini file */
	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());


	$last_used = NULL;
	for ($i = 0; $i <= 100; $i++) {
		if (!($res = mst_mysqli_query($i, $link, "SELECT 1 FROM DUAL"))) {
		}
		$server = mst_mysqli_get_emulated_id($i, $link);
		if ($last_used && ($server != $last_used))
			break;
		$last_used = $server;
	}

	if ($i >= 100)
		printf("[002] Server has never changed, sticky on?");

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_settings_plugin_conf_false_zero.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_settings_plugin_conf_false_zero.ini'.\n");
?>
--EXPECTF--
done!