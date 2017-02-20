--TEST--
Lazy connect, slave failure and existing slave, user
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

/* Emulated ID does not work with replication */
include_once("util.inc");
$ret = mst_is_slave_of($emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket, $emulated_master_host_only, $emulated_master_port, $emulated_master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true == $ret)
	die("SKIP Configured emulated master and emulated slave could be part of a replication cluster\n");

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array("unreachable:6033", $emulated_slave_host, "unreachable2:6033"),
		'pick' 	=> array('user' => array("callback" => "pick_server")),
		'lazy_connections' => 1
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_lazy_slave_failure2_user.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[2]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_lazy_slave_failure2_user.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$mst_ignore_errors = array(
		/* depends on test machine network configuration */
		'[E_WARNING] mysqli::query(): php_network_getaddresses: getaddrinfo failed: Name or service not known',
	);
	set_error_handler('mst_error_handler');

	function pick_server($connected_host, $query, $emulated_master, $emulated_slaves, $last_used_connection, $in_transaction) {
		static $emulated_slave_idx = 0;

		$where = mysqlnd_ms_query_is_select($query);
		$server = '';
		switch ($where) {
			case MYSQLND_MS_QUERY_USE_LAST_USED:
			  $ret = $last_used_connection;
			  $server = 'last used';
			  break;
			case MYSQLND_MS_QUERY_USE_MASTER:
			  $ret = $emulated_master[0];
			  $server = 'master';
			  break;
			case MYSQLND_MS_QUERY_USE_SLAVE:
			  if ($emulated_slave_idx > 2)
				$emulated_slave_idx = 0;
			  $server = 'slave';
 			  $ret = $emulated_slaves[$emulated_slave_idx++];
			  break;
			default:
			  printf("Unknown return value from mysqlnd_ms_query_is_select, where = %s .\n", $where);
			  $ret = $emulated_master[0];
			  $server = 'unknown';
			  break;
		}
		if (false === stristr($query, "util.inc"))
			printf("pick_server('%s', '%s') => %s\n", $connected_host, $query, $server);
		return $ret;
	}

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$connections = array();

	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	$connections[mst_mysqli_get_emulated_id(3, $link)] = array('master');

	mst_mysqli_query(4, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH, true, false, true, version_compare(PHP_VERSION, '5.3.99', ">"));
	$connections[$link->thread_id][] = 'slave (no fallback)';

	mst_mysqli_fech_role(mst_mysqli_query(5, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role"));
	$connections[mst_mysqli_get_emulated_id(6, $link)] = array('slave');

	mst_mysqli_fech_role(mst_mysqli_query(7, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role", true, false, true, version_compare(PHP_VERSION, '5.3.99', ">")));
	$connections[$link->thread_id][] = 'slave (no fallback)';

	mst_mysqli_fech_role(mst_mysqli_query(8, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role", true, false, true, version_compare(PHP_VERSION, '5.3.99', ">")));
	$connections[$link->thread_id][] = 'slave (no fallback)';

	foreach ($connections as $thread_id => $details) {
		printf("Connection %s -\n", $thread_id);
		foreach ($details as $msg)
		  printf("... %s\n", $msg);
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_lazy_slave_failure2_user.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_lazy_slave_failure2_user.ini'.\n");
?>
--EXPECTF--
pick_server('myapp', '/*ms=master*//*2*/SET @myrole='master'') => master
pick_server('myapp', '/*5*/SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role') => slave
This is '' speaking
pick_server('myapp', '/*1*//*7*/SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role') => slave
%AE_WARNING] mysqli::query(): (mysqlnd_ms) Callback chose tcp://unreachable2:6033 but connection failed in %s on line %A
pick_server('myapp', '/*1*//*8*/SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role') => slave
%AE_WARNING] mysqli::query(): (mysqlnd_ms) Callback chose tcp://unreachable:6033 but connection failed in %s on line %A
Connection master-%d -
... master
Connection 0 -
... slave (no fallback)
... slave (no fallback)
... slave (no fallback)
Connection slave[2]-%d -
... slave
done!