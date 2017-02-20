--TEST--
table filter: db name detection
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_check_feature(array("table_filter"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array(
			"master1" => array(
				'host' 		=> $master_host_only,
				'port' 		=> (int)$master_port,
				'socket' 	=> $master_socket,
			),
			"master2" => array(
				"host" => "unknown_master_i_hope",
			),
		),

		'slave' => array(
			"slave1" => array(
				'host' 	=> $slave_host_only,
				'port' 	=> (int)$slave_port,
				'socket' => $slave_socket,
			),
			"slave2" => array(
				"host" => "unknown_slave_i_hope",
			),
		 ),

		'lazy_connections' => 1,
		'filters' => array(
			"table" => array(
				"rules" => array(
					$db . ".%" => array(
						"master"=> array("master2"),
						"slave" => array("slave2"),
					),
				),
			),

			"random" => array('sticky' => '1'),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_table_db_detection.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_table_db_detection.ini
mysqlnd_ms.multi_master=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}


	$threads = array();

	/* db.test -> db.% rule -> master2 */
	mst_mysqli_query(3, $link, "DROP TABLE IF EXISTS test");
	$threads[$link->thread_id] = array("master2");

	/* db.test -> db.% rule -> slave2 */
	mst_mysqli_query(4, $link, "SELECT * FROM test");
	$threads[$link->thread_id][] = "slave2";

	/* db.test -> db.% rule -> slave2 */
	mst_mysqli_query(5, $link, sprintf("SELECT * FROM %s.test", $db));
	$threads[$link->thread_id][] = "slave2";

	if ($link->select_db("i_hope_this_db_does_not_exist"))
		printf("[006] Database 'i_hope_this_db_does_not_exist' exists, test will fail\n");

	/* current db unchanged, db.test -> db.% rule -> slave2 */
	mst_mysqli_query(7, $link, "SELECT * FROM test");
	$threads[$link->thread_id][] = "slave2";

	foreach ($threads as $thread_id => $roles) {
		printf("%d: ", $thread_id);
		foreach ($roles as $k => $role)
		  printf("%s,", $role);
		printf("\n");
	}

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");

	if (!unlink("test_mysqlnd_ms_table_db_detection.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_table_db_detection.ini'.\n");
?>
--EXPECTF--
%Aonnect error, [003] [%d] %s
%Aonnect error, [004] [%d] %s
%Aonnect error, [005] [%d] %s
%Aonnect error, [007] [%d] %s
0: master2,slave2,slave2,slave2,
done!