--TEST--
Local vs. global trx
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP Emulated master and emulated slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_xa_local_trx.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_local_trx.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/*
	Within the context of a given client connection, XA transactions and
	local (non-XA) transactions are mutually exclusive.
	This test is a bit on the server side but better check twice than
	mess up something as tricky as an XA trx manager
	*/
	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	/*
	We may get a false positive if another test is using the same xa id.
	I am not aware of a simple way to avoid this.
	*/
	$xa_id = mt_rand(0, 1000);

	/* autocommit on */
	if (false == mysqlnd_ms_xa_begin($link, $xa_id)) {
		die(sprintf("[002] [%d] Failed to start XA transaction, could be false positive, could be something else. Check manually. Stopping: %s\n",
			$link->errno, $link->error));
	}
	mysqlnd_ms_xa_rollback($link, $xa_id);

	/* Local trx via API */
	$link->begin_transaction();
	/* error on proxy conn */
	if (false != mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[004] XA begin should not be allowed\n");
	} else {
		printf("[004] [%d] '%s'\n", $link->errno, $link->error);
	}

	if (!$link->commit() || !$link->begin_transaction()) {
		printf("[005] [%d] %s\n", $link->errno, $link->error);
	}
	$res = mst_mysqli_query(6, $link, "SELECT 1");
	/* error on last used conn */
	if (false != mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[007] XA begin should not be allowed\n");
	} else {
		printf("[007] [%d] '%s'\n", $link->errno, $link->error);
	}
	var_dump($res->fetch_assoc());

	if (!$link->commit()) {
		printf("[008] [%d] %s\n", $link->errno, $link->error);
	}

	$link->autocommit(false);
	if (false != mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[009] XA begin should not be allowed\n");
	} else {
		printf("[009] [%d] '%s'\n", $link->errno, $link->error);
	}

	$link->close();
	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[010] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$link->autocommit(false);
	if (false != mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[011] XA begin should not be allowed\n");
	} else {
		printf("[011] [%d] '%s'\n", $link->errno, $link->error);
	}

	$link->autocommit(true);
	/* Verify error gets reset. Note that we have no physical connection at this point! */
	printf("[012] [%d] '%s'\n", $link->errno, $link->error);

	/*
	*
	* Other way around, begin XA, then local trx.
    *
	*/
	if (false == mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[013] XA begin should not fail\n");
	} else {
		printf("[013] [%d] '%s'\n", $link->errno, $link->error);
	}
	/* There is no connection yet, the server cannot help us detecting this situation
	of a global trx started and now a local trx being requested! */
	$link->autocommit(false);
	printf("[014] [%d] '%s'\n", $link->errno, $link->error);

	if ($res = $link->query("SELECT 'should not be allowed, have global trx'")) {
		var_dump($res->fetch_assoc());
	}
	printf("[015] [%d] '%s'\n", $link->errno, $link->error);

	$link->close();
	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[016] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if ($res = $link->query("SELECT 'lazy but connected'")) {
		var_dump($res->fetch_assoc());
	}

	if (false == mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[017] XA begin should not fail\n");
	} else {
		printf("[018] [%d] '%s'\n", $link->errno, $link->error);
	}
	$link->autocommit(false);
	printf("[019] [%d] '%s'\n", $link->errno, $link->error);

	if ($res = $link->query("SELECT 'should not be allowed, have global trx'")) {
		var_dump($res->fetch_assoc());
	}
	printf("[020] [%d] '%s'\n", $link->errno, $link->error);


	$link->close();
	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[021] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if ($res = $link->query("SELECT 'lazy but connected'")) {
		var_dump($res->fetch_assoc());
	}

	if (false !=  @mysqlnd_ms_xa_rollback($link, $xa_id)) {
		printf("[022] XA rollback should have failed: nothing to rollback\n");
	} else {
		printf("[023] [%d] '%s'\n", $link->errno, $link->error);
	}

	$link->autocommit(false);
	printf("[024] [%d] '%s'\n", $link->errno, $link->error);

	if ($res = $link->query("SELECT 'should  be allowed, have no global trx'")) {
		var_dump($res->fetch_assoc());
	}
	printf("[025] [%d] '%s'\n", $link->errno, $link->error);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_xa_local_trx.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_local_trx.ini'.\n");
?>
--EXPECTF--
Warning: mysqlnd_ms_xa_begin(): (mysqlnd_ms) Some work is done outside global transaction. You must end the active local transaction first in %s on line %d
[004] [1400] '(mysqlnd_ms) Some work is done outside global transaction. You must end the active local transaction first'

Warning: mysqlnd_ms_xa_begin(): (mysqlnd_ms) Some work is done outside global transaction. You must end the active local transaction first in %s on line %d
[007] [1400] '(mysqlnd_ms) Some work is done outside global transaction. You must end the active local transaction first'
array(1) {
  [1]=>
  string(1) "1"
}

Warning: mysqlnd_ms_xa_begin(): (mysqlnd_ms) Some work is done outside global transaction. You must end the active local transaction first in %s on line %d
[009] [1400] '(mysqlnd_ms) Some work is done outside global transaction. You must end the active local transaction first'

Warning: mysqlnd_ms_xa_begin(): (mysqlnd_ms) Some work is done outside global transaction. You must end the active local transaction first in %s on line %d
[011] [1400] '(mysqlnd_ms) Some work is done outside global transaction. You must end the active local transaction first'
[012] [0] ''
[013] [0] ''
[014] [0] ''

Warning: mysqli::query(): (mysqlnd_ms) Some work is done outside global transaction. You must end the active local transaction first in %s on line %d
[015] [1400] '(mysqlnd_ms) Some work is done outside global transaction. You must end the active local transaction first'
array(1) {
  ["lazy but connected"]=>
  string(18) "lazy but connected"
}
[018] [0] ''
[019] [0] ''

Warning: mysqli::query(): (mysqlnd_ms) Some work is done outside global transaction. You must end the active local transaction first in %s on line %d
[020] [1400] '(mysqlnd_ms) Some work is done outside global transaction. You must end the active local transaction first'
array(1) {
  ["lazy but connected"]=>
  string(18) "lazy but connected"
}
[023] [2000] '(mysqlnd_ms) There is no active XA transaction to rollback'
[024] [0] ''
array(1) {
  ["should  be allowed, have no global trx"]=>
  string(38) "should  be allowed, have no global trx"
}
[025] [0] ''
done!