--TEST--
Multi master, no slaves, RR
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

$settings = array(
	"myapp" => array(
		 /* NOTE: second master will be ignored! */
		'master' => array($emulated_master_host, $emulated_master_host),
		'slave' => array(),
		'failover' => array('strategy' => 'master'),
		'pick' => array('roundrobin'),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_multi_master_no_slaves_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

include_once("util.inc");
msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master[1,2]");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_multi_master_no_slaves_rr.ini
mysqlnd_ms.multi_master=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$servers = array();

	/* master 1 */
	mst_mysqli_query(2, $link, "SET @myrole='Master 1'");
	$server_id = mst_mysqli_get_emulated_id(3, $link);
	if (isset($servers[$server_id]))
		$servers[$server_id] = $servers[$server_id] + 1;
	else
		$servers[$server_id] = 1;

	/* master 2 */
	mst_mysqli_query(4, $link, "SET @myrole='Master 2'");
	$server_id = mst_mysqli_get_emulated_id(5, $link);
	if (isset($servers[$server_id]))
		$servers[$server_id] = $servers[$server_id] + 1;
	else
		$servers[$server_id] = 1;

	/* wrap around */
	mst_mysqli_query(6, $link, "SELECT 1 FROM DUAL");
	$server_id = mst_mysqli_get_emulated_id(7, $link);
	if (isset($servers[$server_id]))
		$servers[$server_id] = $servers[$server_id] + 1;
	else
		$servers[$server_id] = 1;

	/* master 2 */
	mst_mysqli_query(8, $link, "SELECT 1 FROM DUAL");
	$server_id = mst_mysqli_get_emulated_id(9, $link);
	if (isset($servers[$server_id]))
		$servers[$server_id] = $servers[$server_id] + 1;
	else
		$servers[$server_id] = 1;

	foreach ($servers as $server_id => $num_queries) {
		printf("%s has run %d queries\n", $server_id, $num_queries);
	}
	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_multi_master_no_slaves_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_multi_master_no_slaves_rr.ini'.\n");
?>
--EXPECTF--
master[1,2]-%d has run 2 queries
master[1,2]-%d has run 2 queries
done!