--TEST--
mysqlnd_ms_dump_servers()
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"myapp" => array(
		'master' => array(
			'master1' => array(
				  'host' 	=> 'master1_host',
				  'port' 	=> 'master1_port',
				  'socket' 	=> 'master1_socket',
				  'db'		=> 'master1_db',
				  'user'	=> 'master1_user',
				  'password'=> 'master1_pw',
			),

		),
		'slave' => array(
			array(
			  'host' 	=> 'slave0_host',
			  'port' 	=> 'slave0_port',
			  'socket' 	=> 'slave0_socket',
			  'db'		=> 'slave0_db',
			  'user'	=> 'slave0_user',
			  'password'=> 'slave0_pw',
			),

			array(
			  'host' 	=> 'slave1_host',
			),
		),
		'pick' => 'roundrobin',
		'lazy_connections' => 1,
	),
	"myapp_non_lazy" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'lazy_connections' => 0,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_dump_servers.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.multi_master=1
mysqlnd_ms.config_file=test_mysqlnd_ms_dump_servers.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	var_dump(mysqlnd_ms_dump_servers());
	var_dump(mysqlnd_ms_dump_servers(new stdClass()));

	if (!($link =  mst_mysqli_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	var_dump(mysqlnd_ms_dump_servers($link));

	if (!($link = mst_mysqli_connect("myapp", 'gloabal_user', 'global_pass', 'global_db', 1234, 'global_socket')))
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	var_dump(mysqlnd_ms_dump_servers($link));

	if (!($link = mst_mysqli_connect("myapp_non_lazy", $user, $passwd, $db, $port, $socket)))
		printf("[003] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	var_dump(mysqlnd_ms_dump_servers($link));

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_dump_servers.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_dump_servers.ini'.\n");
?>
--EXPECTF--
Warning: mysqlnd_ms_dump_servers() expects exactly 1 parameter, 0 given in %s on line %d
NULL
bool(false)

Warning: mysqlnd_ms_dump_servers(): (mysqlnd_ms) No mysqlnd_ms connection in %s on line %d
bool(false)
array(2) {
  ["masters"]=>
  array(1) {
    [0]=>
    array(6) {
      ["name_from_config"]=>
      string(7) "master1"
      ["hostname"]=>
      string(12) "master1_host"
      ["user"]=>
      string(12) "master1_user"
      ["port"]=>
      int(3306)
      ["socket"]=>
      string(14) "master1_socket"
      ["thread_id"]=>
      NULL
    }
  }
  ["slaves"]=>
  array(2) {
    [0]=>
    array(6) {
      ["name_from_config"]=>
      string(7) "slave_0"
      ["hostname"]=>
      string(11) "slave0_host"
      ["user"]=>
      string(11) "slave0_user"
      ["port"]=>
      int(3306)
      ["socket"]=>
      string(13) "slave0_socket"
      ["thread_id"]=>
      NULL
    }
    [1]=>
    array(6) {
      ["name_from_config"]=>
      string(7) "slave_1"
      ["hostname"]=>
      string(11) "slave1_host"
      ["user"]=>
      string(12) "gloabal_user"
      ["port"]=>
      int(1234)
      ["socket"]=>
      string(13) "global_socket"
      ["thread_id"]=>
      NULL
    }
  }
}
array(2) {
  ["masters"]=>
  array(1) {
    [0]=>
    array(6) {
      ["name_from_config"]=>
      string(%A
      ["hostname"]=>
      string(%A
      ["user"]=>
      string(%A
      ["port"]=>
      int(%d)
      ["socket"]=>
      string(%A
      ["thread_id"]=>
      int(%d)
    }
  }
  ["slaves"]=>
  array(1) {
    [0]=>
    array(6) {
      ["name_from_config"]=>
      string(%A
      ["hostname"]=>
      string(%A
      ["user"]=>
      string(%A
      ["port"]=>
      int(%d)
      ["socket"]=>
      string(%A
      ["thread_id"]=>
      int(%d)
    }
  }
}
done!
