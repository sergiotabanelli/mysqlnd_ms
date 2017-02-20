--TEST--
Limits: Asynchronous queries
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
		'pick' => array("roundrobin"),
		'lazy_connections' => 0,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_limits_async.ini", $settings))
  die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_limits_async.ini
--FILE--
<?php
	require_once("connect.inc");
	$threads = array();

	$link1 = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (0 !== mysqli_connect_errno())
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!$link1->query("SET @my_role='master'"))
		printf("[002] [%d] %s\n", $link1->errno, $link1->error);

	$threads['set'] = $link1->thread_id;

	if (!$link1->query("SELECT @my_role AS _msg", MYSQLI_ASYNC))
		printf("[003] [%d] %s\n", $link1->errno, $link1->error);

	$threads['async select'] = $link1->thread_id;

	$all_links = array($link1);
	$processed = 0;
	do {
		$links = $errors = $reject = array();
		foreach ($all_links as $link) {
			$links[] = $errors[] = $reject[] = $link;
		}
		if (!mysqli_poll($links, $errors, $reject, 1)) {
			usleep(100);
			continue;
		}
		foreach ($links as $link) {
			if ($result = $link->reap_async_query()) {
				$processed++;

				$row = $result->fetch_assoc();
				printf("fetch %d - thread %d - '%s'\n", $processed, $link->thread_id, $row['_msg']);
				mysqli_free_result($result);
			}
		}
	} while ($processed < count($all_links));

	if (!($res = $link1->query("SELECT @my_role AS _msg")))
		printf("[004] [%d] %s\n", $link1->errno, $link1->error);

	$threads["sync select"] = $link1->thread_id;
	foreach ($threads as $task => $id)
		  printf("%s - %d\n", $task, $id);

	if ($threads["sync select"] != $threads["async select"])
		printf("[005] Asynchronous SELECT and SELECT have been send over different connections\n");

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_limits_async.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_limits_async.ini'.\n");
?>
--EXPECTF--

Catchable fatal error: mysqli::query(): (mysqlnd_ms) Asynchronous queries are not supported in %s on line %d