--TEST--
Many connections and references to them
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host, $slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_connection_references.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_connection_references.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$num_links = 2;
	$links = array();
	$references = array();

	for ($i = 0; $i < $num_links; $i++) {
		if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
			printf("[001] Cannot connect, [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
		$links[$i] = $link;
		$references[$i] = $links[$i];
	}

	$last_closed = null;
	do {
		$idx = mt_rand(0, $num_links);

		if (isset($references[$last_closed])) {
			unset($references[$last_closed]);
		}
		if (isset($links[$idx])) {
			mysqli_close($links[$idx]);
			unset($links[$idx]);
			$last_closed = $idx;
		}
	} while (count($references));

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_connection_references.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_connection_references.ini'.\n");
?>
--EXPECTF--
done!
