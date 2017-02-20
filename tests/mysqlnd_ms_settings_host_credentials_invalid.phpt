--TEST--
Per host credentials, invalid
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
		'master' => array(
			  array(
				'host' 		=> $emulated_master_host,
				'port' 		=> PHP_INT_MAX,
				'socket' 	=> false,
				'db'		=> array("db"),
				'user'		=> NULL,
				'password'	=> -1,
				'connect_flags' => -1,
			  ),
		),
		'slave' => array(
			array(
			  'host' 	=> $emulated_slave_host,
			  'port' 	=> -1,
			  'socket' 	=> array(),
			  'db'		=> NULL,
			  'user'	=> array(),
			  'password'=> array(),
			  'connect_flags' => array(),
			),
		),
		'pick' => 'roundrobin',
		'lazy_connections' => 0,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_settings_host_credentials_invalid.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_settings_host_credentials_invalid.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function my_mysqli_query($offset, $link, $query, $switch = NULL) {
		if ($switch)
			$query = sprintf("/*%s*/%s", $switch, $query);

		if (!$link->real_query($query))
			printf("[%03d + 01] [%d] %s\n", $offset, $link->errno, $link->error);

		if (!($res = $link->use_result()))
			printf("[%03d + 02] [%d] %s\n", $offset, $link->errno, $link->error);

		while ($row = $res->fetch_assoc())
			printf("[%03d + 03] '%s'\n", $offset, $row['_one']);
	}

	/* note that user etc are to be taken from the config! */
	if (!($link = mst_mysqli_connect("myapp", NULL, NULL, NULL, NULL, NULL)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$threads = array();

	/* slave 1 */
	my_mysqli_query(2, $link, "SELECT 1 AS _one FROM DUAL");
	$server_id = mst_mysqli_get_emulated_id(3, $link);
	$threads[$server_id] = array('role' => 'Slave 1', 'stat' => $link->stat());

	/* master */
	my_mysqli_query(4, $link, "SELECT 123 AS _one FROM DUAL", MYSQLND_MS_MASTER_SWITCH);
	$server_id = mst_mysqli_get_emulated_id(5, $link);
	$threads[$server_id] = array('role' => 'Master', 'stat' => $link->stat());

	foreach ($threads as $server_id => $details) {
		printf("%s - %s: '%s'\n", $server_id, $details['role'], $details['stat']);
		if ('' == $details['stat'])
			printf("Server stat must not be empty!\n");
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_settings_host_credentials_invalid.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_settings_host_credentials_invalid.ini'.\n");
?>
--EXPECTF--
Catchable fatal error: mysqli_real_connect(): (mysqlnd_ms) Invalid value for connect_flags '-1' . Stopping in %s on line %d