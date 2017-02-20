--TEST--
lazy + real escape + set_charset
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

if (!function_exists("iconv"))
	die("SKIP needs iconv extension\n");

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host, $slave_host),
		'pick' => array("roundrobin"),
		'lazy_connections' => 1,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_lazy_real_escape.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_lazy_real_escape.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* From a user perspective MS and non MS-Connection are now in the same state: connected */
	if (!($link_ms = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[003] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket)))
		printf("[004] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());


	$string = "";
	for ($i = 0; $i < 256; $i++) {
		$char = @iconv("UTF-8", $charset, chr($i));
		if ($char)
			$string .= $char;
		else
			$string .= chr($i);
	}
	$no_ms = $link->real_escape_string($string);
	$ms = $link_ms->real_escape_string($string);

	print "done!";

?>
--CLEAN--
<?php
if (!unlink("test_mysqlnd_ms_lazy_real_escape.ini"))
	printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_lazy_real_escape.ini'.\n");
?>
--EXPECTF--
Warning: mysqli::real_escape_string(): (mysqlnd_ms) string escaping doesn't work without established connection. Possible solution is to add server_charset to your configuration in %s on line %d
done!