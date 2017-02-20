--TEST--
PS and commit
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

_skipif_check_extensions(array("mysqli"));

if (($master_host == $slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
		'filters' => array(
			"roundrobin" => array(),
		),
		'trx_stickiness' => 'disabled',
		'lazy_connections' => 1
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_ps_commit.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_ps_commit.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!$link->query("DROP TABLE IF EXISTS test") ||
		!$link->query("CREATE TABLE test(id INT) ENGINE=InnoDB"))
		printf("[002] [%d] %s\n", $link->errno, $link->error);

	/* statement created in autocommit mode on master connection */
	if (!($stmt = $link->prepare("INSERT INTO test(id) VALUES (1)")))
		printf("[003] [%d] %s\n", $link->errno, $link->error);

	if (!$stmt->execute())
		printf("[004] [%d] %s\n", $stmt->errno, $stmt->error);

	if (!($res = $link->query(sprintf("/*%s*//*%d*/SELECT COUNT(*) AS _num_rows FROM test", MYSQLND_MS_MASTER_SWITCH, 5))))
		printf("[006] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	printf("Rows %d\n", $row['_num_rows']);

	/* switch to slave connection */
	if (!($res = $link->query("SELECT 'Ahoy' AS _msg FROM DUAL")))
		printf("[007] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	printf("Slave says: %s\n", $row['_msg']);

	/* must dispatch autocommit to master and stmt */
	if (!$link->autocommit(false))
		printf("[008] Can't change autocommit mode, [%d] %s\n", $link->errno, $link->error);

	if (!$stmt->execute())
		printf("[009] [%d] %s\n", $stmt->errno, $stmt->error);

	/* master */
	if (!($res = $link->query(sprintf("/*%s*//*%d*/SELECT COUNT(*) AS _num_rows FROM test", MYSQLND_MS_MASTER_SWITCH, 10))))
		printf("[011] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	printf("Rows %d\n", $row['_num_rows']);

	/* 'trx_stickiness' => 'disabled' -> slave */
	if (!($res = $link->query("SELECT 'Ahoy' AS _msg FROM DUAL")))
		printf("[012] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	printf("Slave says: %s\n", $row['_msg']);

	/* no effect because comitted on slave */
	if (!$link->commit())
		printf("[013] Failed to commit, [%d] %s\n", $link->errno, $link->error);

	/* master */
	if (!$stmt->execute())
		printf("[014] [%d] %s\n", $stmt->errno, $stmt->error);

	/* master */
	if (!($res = $link->query(sprintf("/*%s*//*%d*/SELECT COUNT(*) AS _num_rows FROM test", MYSQLND_MS_MASTER_SWITCH, 15))))
		printf("[016] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	printf("Rows %d\n", $row['_num_rows']);

	/* rollback on master */
	if (!$link->rollback())
		printf("[017] Cannot roll back, [%d] %s\n", $link->errno, $link->error);

	/* master */
	if (!($res = $link->query(sprintf("/*%s*//*%d*/SELECT COUNT(*) AS _num_rows FROM test", MYSQLND_MS_MASTER_SWITCH, 19))))
		printf("[020] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	printf("Rows %d\n", $row['_num_rows']);

	if (!$stmt->execute())
		printf("[021] [%d] %s\n", $stmt->errno, $stmt->error);

	/* comitted on master */
	if (!$link->commit())
		printf("[022] Failed to commit, [%d] %s\n", $link->errno, $link->error);

	/* master */
	if (!($res = $link->query(sprintf("/*%s*//*%d*/SELECT COUNT(*) AS _num_rows FROM test", MYSQLND_MS_MASTER_SWITCH, 23))))
		printf("[024] [%d] %s\n", $link->errno, $link->error);

	$row = $res->fetch_assoc();
	printf("Rows %d\n", $row['_num_rows']);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_ps_commit.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_ps_commit.ini'.\n");
?>
--EXPECTF--
Rows 1
Slave says: Ahoy
Rows 2
Slave says: Ahoy
Rows 3
Rows 1
Rows 2
done!