--TEST--
autocommit() and kill()
--SKIPIF--
<?php
require_once("connect.inc");
require_once('skipif.inc');

_skipif_check_extensions(array("mysqli"));

if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_autocommit_kill.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_autocommit_kill.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$attempts = 0;
	do {
		if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
			printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

		if (!mysqli_autocommit($link, false))
			printf("[002] Failed to change autocommit setting\n");

		mst_mysqli_query(3, $link, "SET autocommit=0", MYSQLND_MS_MASTER_SWITCH);
		$master = $link->thread_id;
		mst_mysqli_query(4, $link, "SET autocommit=0", MYSQLND_MS_SLAVE_SWITCH);
		$slave = $link->thread_id;

		if ($master == $slave) {
			/*
			The test relies on different thread ids, try connecting again.
			try to increase thread id on one of the servers...
			*/
			$tmp[] = @mst_mysqli_connect($master_host, $user, $passwd, $db, $master_port, $master_socket);
			$attempts++;
		}
	} while (($master == $slave) && ($attempts < 5));

	if ($master == $slave) {
		printf("[005] Master and slave connection have the same thread id. Please, try running test again.\n");
		die();
	}

	if (!$link->kill($link->thread_id))
		printf("[006] [%d] %s\n", $link>errno, $link->error);

	printf("[007] Connected to %s\n", ($link->thread_id == $master) ? 'master' : (($link->thread_id == $slave) ? 'slave' : $link->thread_id));

	if (!mysqli_autocommit($link, true))
		printf("[008] [%d] %s\n");

	printf("[009] Connected to %s\n", ($link->thread_id == $master) ? 'master' : (($link->thread_id == $slave) ? 'slave' : $link->thread_id));

	/* slave because SELECT */
	$res = mst_mysqli_query(10, $link, "SELECT @myrole AS _role, @@autocommit AS auto_commit");
	if ($res) {
		printf("[011] Who is speaking?\n");
		var_dump($res->fetch_assoc());
	}

	/* master because of hint */
	$res = mst_mysqli_query(12, $link, "SELECT @myrole AS _role, @@autocommit AS auto_commit", MYSQLND_MS_MASTER_SWITCH);
	$row = $res->fetch_assoc();
	if (1 != $row['auto_commit'])
		printf("[013] Autocommit should be on, got '%s'\n", $row['auto_commit']);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_autocommit_kill.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_autocommit_kill.ini'.\n");
?>
--EXPECTF--
[007] Connected to slave
[009] Connected to slave
[010] [2006] MySQL server has gone away
done!