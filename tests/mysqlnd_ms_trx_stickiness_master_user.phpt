--TEST--
trx_stickiness=master (PHP 5.3.99+), pick = user (rr)
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP 5.3.99 or newer, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'trx_stickiness' => 'master',
		'pick' 	=> array('user' => array('callback' => 'pick_server')),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_trx_stickiness_master_user.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_trx_stickiness_master_user.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function pick_server($connected_host, $query, $emulated_master, $emulated_slaves, $last_used_connection, $in_transaction) {
		static $pick_server_last_used = "";
		printf("pick_server(%s)\n", $query);
		if ($in_transaction)
			return $emulated_master[0];

		$ret = "";
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
 			  $ret = $emulated_slaves[0];
			  $server = 'slave';
			  break;
			default:
			  printf("[012] Unknown return value from mysqlnd_ms_query_is_select, where = %s .\n", $where);
			  $ret = $emulated_master[0];
			  $server = 'unknown';
			  break;
		}

		$pick_server_last_used = $ret;
		return $ret;
	}

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_compare_stats();
	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(3, $link, "SET @myrole='slave 1'", MYSQLND_MS_SLAVE_SWITCH);
	mst_compare_stats();

	/* explicitly disabling autocommit via API */
	$link->autocommit(FALSE);
	mst_compare_stats();
	/* this can be the start of a transaction, thus it shall be run on the master */
	mst_mysqli_fech_role(mst_mysqli_query(4, $link, "SELECT @myrole AS _role"));
	mst_compare_stats();

	/* back to the slave for the next SELECT because autocommit  is on */
	$link->autocommit(TRUE);
	mst_mysqli_fech_role(mst_mysqli_query(5, $link, "SELECT @myrole AS _role"));
	mst_mysqli_fech_role(mst_mysqli_query(6, $link, "SELECT @myrole AS _role"));
	mst_compare_stats();

	/* explicitly disabling autocommit via API */
	$link->autocommit(FALSE);
	/* SQL hint wins */
	mst_mysqli_fech_role(mst_mysqli_query(7, $link, "SELECT @myrole AS _role", MYSQLND_MS_SLAVE_SWITCH));
	mst_mysqli_fech_role(mst_mysqli_query(8, $link, "SELECT @myrole AS _role", MYSQLND_MS_LAST_USED_SWITCH));
	mst_mysqli_fech_role(mst_mysqli_query(9, $link, "SELECT @myrole AS _role", MYSQLND_MS_MASTER_SWITCH));
	mst_compare_stats();

	mst_mysqli_fech_role(mst_mysqli_query(10, $link, "SELECT @myrole AS _role"));
	mst_compare_stats();

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_trx_stickiness_master_user.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_trx_stickiness_master_user.ini'.\n");
?>
--EXPECTF--
pick_server(/*ms=master*//*2*/SET @myrole='master')
pick_server(/*ms=slave*//*3*/SET @myrole='slave 1')
Stats use_slave_sql_hint: 1
Stats use_master_sql_hint: 1
Stats use_slave_callback: 1
Stats use_master_callback: 1
Stats lazy_connections_slave_success: 1
Stats lazy_connections_master_success: 1
Stats trx_autocommit_off: 1
pick_server(/*4*/SELECT @myrole AS _role)
This is 'master' speaking
Stats use_master_callback: 2
pick_server(/*5*/SELECT @myrole AS _role)
This is 'slave 1' speaking
pick_server(/*6*/SELECT @myrole AS _role)
This is 'slave 1' speaking
Stats use_slave_guess: 2
Stats use_slave_callback: 3
Stats trx_autocommit_on: 1
pick_server(/*ms=slave*//*7*/SELECT @myrole AS _role)
This is 'master' speaking
pick_server(/*ms=last_used*//*8*/SELECT @myrole AS _role)
This is 'master' speaking
pick_server(/*ms=master*//*9*/SELECT @myrole AS _role)
This is 'master' speaking
Stats use_master_callback: 5
Stats trx_autocommit_off: 2
pick_server(/*10*/SELECT @myrole AS _role)
This is 'master' speaking
Stats use_master_callback: 6
done!