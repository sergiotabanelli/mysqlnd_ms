--TEST--
trx_stickiness=master (PHP 5.3.99+), pick = random
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
		'pick' => array("random"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_trx_stickiness_master_random.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_trx_stickiness_master_random.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);

	$slaves = array();
	do {
		mst_mysqli_query(3, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);
		$slaves[$link->thread_id] = true;
	} while (count($slaves) < 2);

	/* explicitly disabling autocommit via API */
	$link->autocommit(FALSE);
	/* this can be the start of a transaction, thus it shall be run on the master */
	mst_mysqli_fech_role(mst_mysqli_query(5, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role"));

	/* back to the slave for the next SELECT because autocommit  is on */
	$link->autocommit(TRUE);
	mst_mysqli_fech_role(mst_mysqli_query(6, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role"));
	mst_mysqli_fech_role(mst_mysqli_query(7, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role"));
	mst_mysqli_fech_role(mst_mysqli_query(8, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role"));

	/* explicitly disabling autocommit via API */
	$link->autocommit(FALSE);
	/* SQL hint does NOT win! */
	mst_mysqli_fech_role(mst_mysqli_query(9, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role", MYSQLND_MS_SLAVE_SWITCH));
	mst_mysqli_fech_role(mst_mysqli_query(10, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role", MYSQLND_MS_LAST_USED_SWITCH));
	mst_mysqli_fech_role(mst_mysqli_query(10, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role", MYSQLND_MS_MASTER_SWITCH));

	mst_mysqli_fech_role(mst_mysqli_query(11, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role"));

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_trx_stickiness_master_random.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_trx_stickiness_master_random.ini'.\n");
?>
--EXPECTF--
This is 'master %d' speaking
This is 'slave %d' speaking
This is 'slave %d' speaking
This is 'slave %d' speaking
This is 'master %d' speaking
This is 'master %d' speaking
This is 'master %d' speaking
This is 'master %d' speaking
done!