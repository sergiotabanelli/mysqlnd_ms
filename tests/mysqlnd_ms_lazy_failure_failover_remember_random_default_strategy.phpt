--TEST--
Random, Remember failed, default strategy (= disabled)
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);

if (($master_host == $slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array("unreachable:6033"),
		'pick' 	=> array('random'),
		'lazy_connections' => 1,
		'failover' => array("remember_failed" => true),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_settings_lazy_failure_failover_remember_random_default_strategy.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_settings_lazy_failure_failover_remember_random_default_strategy.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(1, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	$failed = 0 ;
	for ($i = 0; $i < 100; $i++) {
		if (!@$link->query("SELECT 1 FROM DUAL")) {
			/* failed slave ? */
			$failed++;
		} else {
			if (!($res = $link->query(sprintf("/*%s*/SELECT @myrole AS _role", MYSQLND_MS_LAST_USED_SWITCH)))) {
				printf("[002] Failed to check role [%d] %s\n", $link->errno, $link->error);
				break;
			}
			$row = $res->fetch_assoc();
			if ($row['_role'] == 'master') {
				printf("[003] Master has been used.\n");
				break;
			}
		}
	}
	if (0 == $failed) {
		printf("[004] No failed slaves, verify whether its a bug or a false positive\n");
	}
	printf("Failed: %d\n", $failed);

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_settings_lazy_failure_failover_remember_random_default_strategy.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_settings_lazy_failure_failover_remember_random_default_strategy.ini'.\n");
?>
--EXPECTF--
Failed: 100
done!