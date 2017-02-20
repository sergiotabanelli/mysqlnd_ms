--TEST--
Filter QOS
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $master_host_only,
				'port' 		=> (int)$master_port,
				'socket' 	=> $master_socket,
			),
		),
		'slave' => array(
			"slave1" => array(
				'host' 	=> $slave_host_only,
				'port' 	=> (int)$slave_port,
				'socket' => $slave_socket,
			),
		 ),

		'lazy_connections' => 0,
		'filters' => array(
			"quality_of_service" => array(
				"strong_consistency" => 1,
			),
			"random" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_filter_qos.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_filter_qos.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	/* master */
	mst_mysqli_query(2, $link, "SET @myrole='master'");

	/* master, if strong_consistency or session_consistency */
	if ($res = mst_mysqli_query(4, $link, "SELECT @myrole FROM DUAL"))
		var_dump($res->fetch_assoc());

	/* master - ignore SQL hint */
	if ($res = mst_mysqli_query(6, $link, "SELECT @myrole FROM DUAL", MYSQLND_MS_SLAVE_SWITCH))
		var_dump($res->fetch_assoc());

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_filter_qos.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_filter_qos.ini'.\n");
?>
--EXPECTF--
array(1) {
  ["@myrole"]=>
  string(6) "master"
}
array(1) {
  ["@myrole"]=>
  string(6) "master"
}
done!