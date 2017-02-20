--TEST--
RW-split, random, no slaves
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"myapp" => array(
		'master' => array($master_host, $master_host),
		'slave'  => array(),
		'pick' => array("roundrobin"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_enable_rw_random.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.multi_master=1
mysqlnd_ms.disable_rw_split=0
mysqlnd_ms.config_file=test_mysqlnd_ms_enable_rw_random.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
	set_error_handler('mst_error_handler');

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$connections = array();

	mst_mysqli_query(3, $link, "SET @myrole='master1'", MYSQLND_MS_MASTER_SWITCH);
	$connections['master1'] = array($link->thread_id);
	@mst_mysqli_query(4, $link, "SET @myrole='master2'", MYSQLND_MS_MASTER_SWITCH);
	$connections['master2'] = array($link->thread_id);

	$res = mst_mysqli_query(5, $link, "SELECT @myrole AS _role");
	$connections['master1'][] = $link->thread_id;
	if ($res && ($row = $res->fetch_assoc()) && ($row['_role'] != 'master1'))
		printf("[006] [%d] %s, wrong results\n", $link->errno, $link->error);

	mst_mysqli_query(7, $link, "SELECT @myrole AS _role");
	$connections['master2'][] = $link->thread_id;

	foreach ($connections as $role => $ids) {
		printf("Role: %s\n", $role);
		$last_thread_id = NULL;
		foreach ($ids as $k => $thread_id) {
			printf("  %d\n", $thread_id);
			if (is_null($last_thread_id)) {
				$last_thread_id = $thread_id;
			} else if ($last_thread_id != $thread_id) {
				printf(" [008]  wrong thread id!\n");
			}
		}
	}
	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_enable_rw_random.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_enable_rw_random.ini'.\n");
?>
--EXPECTF--
[E_WARNING] mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate slave connection. 0 slaves to choose from. Something is wrong in %s on line %d
[E_WARNING] mysqli::query(): (mysqlnd_ms) No connection selected by the last filter in %s on line %d
[005] [2000] (mysqlnd_ms) No connection selected by the last filter
[E_WARNING] mysqli::query(): (mysqlnd_ms) Couldn't find the appropriate slave connection. 0 slaves to choose from. Something is wrong in %s on line %d
[E_WARNING] mysqli::query(): (mysqlnd_ms) No connection selected by the last filter in %s on line %d
[007] [2000] (mysqlnd_ms) No connection selected by the last filter
Role: master1
  %d
  %d
Role: master2
  %d
  %d
 [008]  wrong thread id!
done!