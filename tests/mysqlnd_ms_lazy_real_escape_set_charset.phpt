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
if ($error = mst_create_config("test_mysqlnd_ms_lazy_real_escape_set_charset.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_lazy_real_escape_set_charset.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	$charsets = array();
	if (!$res = mysqli_query($link, "SHOW CHARACTER SET"))
		printf("[002] Cannot get list of character sets\n");

	while ($tmp = mysqli_fetch_assoc($res)) {
		if ('ucs2' == $tmp['Charset'] || 'utf16' == $tmp['Charset'] || 'utf32' == $tmp['Charset'])
			continue;
		$charsets[$tmp['Charset']] = $tmp['Charset'];
	}
	mysqli_free_result($res);
	$link->close();

	/* From a user perspective MS and non MS-Connection are now in the same state: connected */
	if (!($link_ms = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[003] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket)))
		printf("[004] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());


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

		if (($ms === "") && ($no_ms !== "")) {
			printf("[005] MS has returned an empty string!\n");
			printf("[006] [%d/%s] '%s'\n", $link->errno, $link->sqlstate, $link->error);
			printf("[007] [%d/%s] '%s'\n", $link_ms->errno, $link_ms->sqlstate, $link_ms->error);
			break;
		}

		if ($no_ms !== $ms) {
			printf("[008] Encoded strings differ for charset '%s', MS = '%s', no MS = '%s'\n",
				$charset, $ms, $no_ms);
			printf("[009] [%d/%s] '%s'\n", $link->errno, $link->sqlstate, $link->error);
			printf("[010] [%d/%s] '%s'\n", $link_ms->errno, $link_ms->sqlstate, $link_ms->error);
			break;
		}
	}

	print "done!";
?>
--CLEAN--
<?php
if (!unlink("test_mysqlnd_ms_lazy_real_escape_set_charset.ini"))
	printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_lazy_real_escape_set_charset.ini'.\n");
?>
--EXPECTF--
done!