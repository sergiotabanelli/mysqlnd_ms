--TEST--
trx_stickiness=on, pick = random
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.4.99-dev', '<'))
	die(sprintf("SKIP Requires PHP 5.5.0 or newer, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($slave_host, $slave_host),
		'trx_stickiness' => 'on',
		'pick' => array("random"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_trx_stickiness_on_random.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_trx_stickiness_on_random.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);

	$slaves = array();
	$i = 0;
	do {
		mst_mysqli_query(3, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH);
		$slaves[$link->thread_id] = true;
		$i++;
	} while (($i <= 100) && (count($slaves) < 2));
	if (100 == $i) {
		die("[004] Two connections happen to have the same thread id, ignore and run again!");
	}

	$link->autocommit(FALSE);
	/* this can be the start of a transaction, thus it shall be run on the master */
	$last = NULL;
	for ($i = 0; $i < 100; $i++) {
		if (!($res = mst_mysqli_query(5, $link, "SELECT CONCAT(@myrole, ' ', CONNECTION_ID()) AS _role"))) {
			printf("[006] [%d] %s\n", $link->errno, $link->error);
		}
		$row = $res->fetch_assoc();
		if (is_null($last)) {
			$last = $row['_role'];
		} else {
		    if ($last != $row['_role']) {
				printf("[007] Connection switch during transaction must not happen!\n");
				break;
		    }
		}

	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_trx_stickiness_on_random.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_trx_stickiness_on_random.ini'.\n");
?>
--EXPECTF--
done!