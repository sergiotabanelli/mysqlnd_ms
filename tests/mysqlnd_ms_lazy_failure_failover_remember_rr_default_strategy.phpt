--TEST--
RR, Remember failed, default strategy
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
		'slave' => array("unreachable:6033", $slave_host),
		'pick' 	=> array('roundrobin'),
		'lazy_connections' => 1,
		'failover' => array("remember_failed" => true),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_settings_lazy_failure_failover_remember_rr_default_strategy.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_settings_lazy_failure_failover_remember_rr_default_strategy.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	/* slave 1 - failure */
	if ($res = @mst_mysqli_query(2, $link, "SELECT 2 FROM DUAL"))
		var_dump($res->fetch_assoc());

	/* slave 2 - no failure */
	if ($res = mst_mysqli_query(3, $link, "SELECT 3 FROM DUAL"))
		var_dump($res->fetch_assoc());

	/* slave 2 - no failure */
	if ($res = mst_mysqli_query(4, $link, "SELECT 4 FROM DUAL"))
		var_dump($res->fetch_assoc());

	/* slave 2 - no failure */
	if ($res = mst_mysqli_query(5, $link, "SELECT 5 FROM DUAL"))
		var_dump($res->fetch_assoc());


	/* master 1 - no failure */
	mst_mysqli_query(7, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_fech_role(mst_mysqli_query(8, $link, "SELECT @myrole AS _role"));


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_settings_lazy_failure_failover_remember_rr_default_strategy.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_settings_lazy_failure_failover_remember_rr_default_strategy.ini'.\n");
?>
--EXPECTF--
Connect error, [002] %s
array(1) {
  [3]=>
  string(1) "3"
}
array(1) {
  [4]=>
  string(1) "4"
}
array(1) {
  [5]=>
  string(1) "5"
}
This is '' speaking
done!
