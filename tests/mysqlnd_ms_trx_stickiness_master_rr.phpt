--TEST--
trx_stickiness=master (PHP 5.3.99+), pick = roundrobin
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP 5.3.99 or newer, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host, $slave_host),
		'trx_stickiness' => 'master',
		'pick' => array("roundrobin"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_trx_stickiness_master_round_robin.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_trx_stickiness_master_round_robin.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	echo "----\n";
	mst_compare_stats();
	echo "----\n";
	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(3, $link, "SET @myrole='slave 1'", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(4, $link, "SET @myrole='slave 2'", MYSQLND_MS_SLAVE_SWITCH);
	echo "----\n";
	mst_compare_stats();
	echo "----\n";

	/* explicitly disabling autocommit via API */
	$link->autocommit(FALSE);
	echo "----\n";
	mst_compare_stats();
	echo "----\n";
	/* this can be the start of a transaction, thus it shall be run on the master */
	mst_mysqli_fech_role(mst_mysqli_query(5, $link, "SELECT @myrole AS _role"));
	echo "----\n";
	mst_compare_stats();
	echo "----\n";

	/* back to the slave for the next SELECT because autocommit  is on */
	$link->autocommit(TRUE);
	mst_mysqli_fech_role(mst_mysqli_query(6, $link, "SELECT @myrole AS _role"));
	mst_mysqli_fech_role(mst_mysqli_query(7, $link, "SELECT @myrole AS _role"));
	mst_mysqli_fech_role(mst_mysqli_query(8, $link, "SELECT @myrole AS _role"));

	/* explicitly disabling autocommit via API */
	$link->autocommit(FALSE);
	/* SQL hint wins */
	mst_mysqli_fech_role(mst_mysqli_query(9, $link, "SELECT @myrole AS _role", MYSQLND_MS_SLAVE_SWITCH));
	mst_mysqli_fech_role(mst_mysqli_query(10, $link, "SELECT @myrole AS _role", MYSQLND_MS_LAST_USED_SWITCH));
	mst_mysqli_fech_role(mst_mysqli_query(10, $link, "SELECT @myrole AS _role", MYSQLND_MS_MASTER_SWITCH));

	mst_mysqli_fech_role(mst_mysqli_query(11, $link, "SELECT @myrole AS _role"));

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_trx_stickiness_master_round_robin.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_trx_stickiness_master_round_robin.ini'.\n");
?>
--EXPECTF--
----
----
----
Stats use_slave: 2
Stats use_master: 1
Stats use_slave_sql_hint: 2
Stats use_master_sql_hint: 1
Stats lazy_connections_slave_success: 2
Stats lazy_connections_master_success: 1
----
----
Stats trx_autocommit_off: 1
----
This is 'master' speaking
----
Stats use_master: 2
Stats use_slave_guess: 1
Stats trx_master_forced: 1
----
This is 'slave 1' speaking
This is 'slave 2' speaking
This is 'slave 1' speaking
This is 'master' speaking
This is 'master' speaking
This is 'master' speaking
This is 'master' speaking
done!