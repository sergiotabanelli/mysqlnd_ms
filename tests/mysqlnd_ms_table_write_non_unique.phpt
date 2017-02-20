--TEST--
table filter: multiple master
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
			"table" => array(
				"rules" => array(
					$db . ".%" => array(
						"master" => array("master1", "master2"),
						"slave" => array("slave1"),
					),
				),
			),
			"roundrobin" => array(),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_table_write_non_unique.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_table_write_non_unique.ini
mysqlnd_ms.multi_master=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* shall use host = forced_master_hostname_abstract_name from the ini file */
	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	$masters = array();
	/* master 1 */
	mst_mysqli_query(2, $link, "DROP TABLE IF EXISTS test");
	$masters[] = $link->thread_id;

	/* master 2 */
	mst_mysqli_query(3, $link, "DROP TABLE IF EXISTS test");
	$masters[] = $link->thread_id;

	/* master 1 */
	mst_mysqli_query(4, $link, "DROP TABLE IF EXISTS test");
	$masters[] = $link->thread_id;

	/* master 2 */
	mst_mysqli_query(5, $link, "DROP TABLE IF EXISTS test");
	$masters[] = $link->thread_id;

	$last = NULL;
	foreach ($masters as $k => $thread_id) {
		printf("%d -> %d\n", $k, $thread_id);
		if (!is_null($last)) {
			if ($last != $thread_id)
				printf("Server switch\n");
			else
				printf("No server switch\n");
		}
		$last = $thread_id;
	}

	print "done!";
?>
--XFAIL--
Discuss if allowed.
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_table_write_non_unique.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_table_write_non_unique.ini'.\n");
?>
--EXPECTF--
0 -> %d
1 -> %d
Server switch
2 -> %d
Server switch
3 -> %d
Server switch
done!