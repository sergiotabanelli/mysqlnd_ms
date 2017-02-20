--TEST--
Prepared Statement prepare() and transient error (lazy)
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
		'lazy_connections' =>  1,
		'transient_error' => array('mysql_error_codes' => array(1064), "max_retries" => 2, "usleep_retry" => 22),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_prepare_transient_error_lazy.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_prepare_transient_error_lazy.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	if (!$stmt = $link->prepare("/*".MYSQLND_MS_MASTER_SWITCH."*/ Gurkensalat erfrischt"))
		printf("[005] [%d] %s\n", $link->errno, $link->error);

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);


	if (!$stmt = $link->prepare("SELECT @myrole AS _role"))
		printf("[006] [%d] %s\n", $link->errno, $link->error);

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);

	if (!$stmt->execute())
		printf("[007] [%d] %s\n", $stmt->errno, $stmt->error);

	$role = NULL;
	if (!$stmt->bind_result($role))
		printf("[008] [%d] %s\n", $stmt->errno, $stmt->error);

	while ($stmt->fetch())
		printf("Role = '%s'\n", $role);

	$stats =  mysqlnd_ms_get_stats();
	printf("Transient error retries: %d\n", $stats['transient_error_retries']);


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_prepare_transient_error_lazy.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_prepare_transient_error_lazy.ini'.\n");
?>
--EXPECTF--
Transient error retries: 0
[005] [1064] %s
Transient error retries: 2
Transient error retries: 2
Role = ''
Transient error retries: 2
done!