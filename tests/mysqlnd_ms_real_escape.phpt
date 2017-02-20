--TEST--
real escape
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
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_real_escape.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_real_escape.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link_ms = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket)))
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$charsets = array();
	/* master */
	if (!$res = mysqli_query($link_ms, "SHOW CHARACTER SET"))
		printf("[003] Cannot get list of character sets\n");

	while ($tmp = mysqli_fetch_assoc($res)) {
		if ('ucs2' == $tmp['Charset'] || 'utf16' == $tmp['Charset'] || 'utf32' == $tmp['Charset'])
			continue;
		$charsets[$tmp['Charset']] = $tmp['Charset'];
	}
	mysqli_free_result($res);

	foreach ($charsets as $charset) {
		if (!@$link->set_charset($charset) || !@$link_ms->set_charset($charset))
			continue;

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

		if ($no_ms !== $ms) {
			printf("[004] Encoded strings differ for charset '%s', MS = '%s', no MS = '%s'\n",
			  $charset, $ms, $no_ms);
		}
	}

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_real_escape.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_real_escape.ini'.\n");
?>
--EXPECTF--
done!