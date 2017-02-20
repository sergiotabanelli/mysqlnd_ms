--TEST--
Lazy connect, slave failure and existing slave, random
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

if (($master_host == $slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array("unreachable:6033", "unreachable2:6033", $slave_host, "unreachable3:6033"),
		'pick' 	=> array('random'),
		'lazy_connections' => 1
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_lazy_slave_failure_random.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_lazy_slave_failure_random.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$connections = array(
		'master' => array(),
		'slave' => array(),
		'slave (no fallback)' => array(),
	);

	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	$connections['master'][] = $link->thread_id;

	mst_mysqli_query(3, $link, "SET @myrole='slave'", MYSQLND_MS_SLAVE_SWITCH, true, false, true, version_compare(PHP_VERSION, '5.3.99', ">"));
	$connections['slave (no fallback)'][0] = $link->thread_id;

	$states = array("failure" => 0, "connect" => 0);
	do {
	  $res = mst_mysqli_query(4, $link, "SELECT CONNECTION_ID() AS _role", NULL, true, false, true, version_compare(PHP_VERSION, '5.3.99', ">"));
	  if ($res) {
		  $row = $res->fetch_assoc();
		  $res->close();
		  if (0 == $states['connect']) {
			printf("This is '%s' speaking\n", $row['_role']);
			$connections['slave'][] = $link->thread_id;
		  }
		  $states['connect']++;
	  } else {
		  $states['failure']++;
		  $connections['slave (no fallback)'][1] = $link->thread_id;
	  }
	} while ((0 == $states['connect']) || (0 == $states['failure']));

	foreach ($connections as $role => $details) {
		printf("Role %s -\n", $role);
		foreach ($details as $id)
		  printf("... %d\n", $id);
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_lazy_slave_failure_random.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_lazy_slave_failure_random.ini'.\n");
?>
--EXPECTF--
This is %s speaking
Role master -
... %d
Role slave -
... %d
Role slave (no fallback) -
... %d
... 0
done!
