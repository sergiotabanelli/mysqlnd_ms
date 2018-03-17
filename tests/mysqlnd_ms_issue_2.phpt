--TEST--
Test of github issue #2
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_issue_2.ini
--SKIPIF--
<?php
require_once("skipif.inc");
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));


$settings = array(
	"localhost" => array(
		'master' => array("master_0" => array(
				'host' 		=> $emulated_master_host_only,
				'port' 		=> (int)$emulated_master_port,
			)
		),
		'slave' => array("slave_0" => array(
				'host' 	=> $emulated_master_host_only,
				'port' 	=> (int)$emulated_master_port,
			)),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_issue_2.ini", $settings))
	die(sprintf("SKIP %s\n", $error));


_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);

/* Emulated ID does not work with replication */
include_once("util.inc");
/*
msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
*/
?>
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("localhost", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
    $a = array();
    $sql_set = "SET @myrole='master'";
    $sql_get = "SELECT @@hostname as _hostname, @myrole AS _role";
    // set the variable
    if (!$link->query($sql_set)) {
        $a[] = 'Error setting @myrole ['.$link->errno.'] '.$link->error;
    }
    // available cases
    $cases = array();
    $cases[] = array('name' => 'Normal', 'flag' => false);
    // we have mysqlnd_ms library active > adding Master/Slave cases
    $cases[] = array('name' => 'Slave', 'flag' => MYSQLND_MS_SLAVE_SWITCH);
    $cases[] = array('name' => 'Last connection after Slave', 'flag' => MYSQLND_MS_LAST_USED_SWITCH);
    $cases[] = array('name' => 'Master', 'flag' => MYSQLND_MS_MASTER_SWITCH);
    $cases[] = array('name' => 'Last connection after Master', 'flag' => MYSQLND_MS_LAST_USED_SWITCH);
    // we get the variable value in all the cases
    foreach ($cases as $k => $case) {
        $sql_case = $sql_get;
        if ($case['flag'] !== false) {
            // the flag is set > we add it to the query
            $sql_case = sprintf("/*%s*/".$sql_case, $case['flag']);
        }
        if (!($res = $link->query($sql_case))) {
            $a[] = $case['name'].': Error ['.$link->errno.'] '.$link->error;
        } else {
            $row = $res->fetch_assoc();
            $res->close();
            $a[] = $case['name'].': > flag: ['.$case['flag'].'] | @@hostname: ['.$row['_hostname'].'] | @myrole: ['.$row['_role'].']';
        }
    }
    var_dump($a);
	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_issue_2.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_issue_2.ini'.\n");
?>
--EXPECTF--
array(5) {
  [0]=>
  string(61) "Normal: > flag: [] | @@hostname: [%s] | @myrole: []"
  [1]=>
  string(68) "Slave: > flag: [ms=slave] | @@hostname: [%s] | @myrole: []"
  [2]=>
  string(94) "Last connection after Slave: > flag: [ms=last_used] | @@hostname: [%s] | @myrole: []"
  [3]=>
  string(76) "Master: > flag: [ms=master] | @@hostname: [%s] | @myrole: [master]"
  [4]=>
  string(101) "Last connection after Master: > flag: [ms=last_used] | @@hostname: [%s] | @myrole: [master]"
}
done!