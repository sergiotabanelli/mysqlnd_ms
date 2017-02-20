--TEST--
No RW-split, random
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"myapp" => array(
		'master' => array($master_host, "unreachable", $master_host),
		'slave'  => array(),
		'pick' => array("roundrobin"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_disable_rw_rr.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.multi_master=1
mysqlnd_ms.disable_rw_split=1
mysqlnd_ms.config_file=test_mysqlnd_ms_disable_rw_rr.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$connections = array();

	mst_mysqli_query(3, $link, "SET @myrole='master1'");
	$connections['master1'] = array($link->thread_id);
	mst_mysqli_query(4, $link, "SET @myrole='master2'", NULL, true, true, true, true);
	$connections['master2'] = array($link->thread_id);
	mst_mysqli_query(5, $link, "SET @myrole='master3'");
	$connections['master3'] = array($link->thread_id);

	$res = mst_mysqli_query(6, $link, "SELECT @myrole AS _role", MYSQLND_MS_MASTER_SWITCH);
	$connections['master1'][] = $link->thread_id;
	$row = $res->fetch_assoc();
	if ($row['_role'] != 'master1')
		printf("[007] [%d] %s, wrong results\n", $link->errno, $link->error);

	mst_mysqli_query(8, $link, "SELECT @myrole AS _role", MYSQLND_MS_MASTER_SWITCH, true, true, true, true);
	$connections['master2'][] = $link->thread_id;

	$res = mst_mysqli_query(9, $link, "SELECT @myrole AS _role");
	$connections['master3'][] = $link->thread_id;
	$row = $res->fetch_assoc();
	if ($row['_role'] != 'master3')
		printf("[010] [%d] %s, wrong results\n", $link->errno, $link->error);

	mst_mysqli_query(11, $link, "SET @myrole='master1'");
	$connections['master1'][] = $link->thread_id;

	mst_mysqli_query(12, $link, "SET @myrole='master2'", NULL, true, true, true, true);
	$connections['master2'][] = $link->thread_id;

	$res = mst_mysqli_query(14, $link, "SELECT @myrole AS _role", MYSQLND_MS_SLAVE_SWITCH);
	$connections['master3'][] = $link->thread_id;
	$row = $res->fetch_assoc();
	if ($row['_role'] != 'master3')
		printf("[015] [%d] %s, wrong results\n", $link->errno, $link->error);

	$res = mst_mysqli_query(16, $link, "SELECT @myrole AS _role", MYSQLND_MS_LAST_USED_SWITCH);
	$connections['master3'][] = $link->thread_id;
	$row = $res->fetch_assoc();
	if ($row['_role'] != 'master3')
		printf("[017] [%d] %s, wrong results\n", $link->errno, $link->error);

	mst_mysqli_query(18, $link, "SET @myrole='master1'");
	$connections['master1'][] = $link->thread_id;

	foreach ($connections as $role => $ids) {
		printf("Role: %s\n", $role);
		$last_thread_id = NULL;
		foreach ($ids as $k => $thread_id) {
			printf("  %d\n", $thread_id);
			if (is_null($last_thread_id)) {
				$last_thread_id = $thread_id;
			} else if ($last_thread_id != $thread_id) {
				printf(" [016]  wrong thread id!\n");
			}
		}
	}
	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_disable_rw_rr.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_disable_rw_rr.ini'.\n");
?>
--EXPECTF--
Role: master1
  %d
  %d
  %d
  %d
Role: master2
  %d
  %d
  %d
Role: master3
  %d
  %d
  %d
  %d
done!