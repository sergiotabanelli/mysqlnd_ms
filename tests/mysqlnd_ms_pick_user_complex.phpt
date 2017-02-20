--TEST--
Config settings: pick = user
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

include_once("util.inc");
$ret = mst_is_slave_of($emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket, $emulated_master_host_only, $emulated_master_port, $emulated_master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true == $ret)
	die("SKIP Configured emulated master and emulated slave could be part of a replication cluster\n");

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'pick' 	=> array('user' => array('callback' => 'pick_server')),
		'lazy_connections' => 0,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pick_user_complex.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pick_user_complex.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/*
	* Select/pick a server for running the query on.
	*
	*/
	function pick_server($connected_host, $query, $emulated_master, $emulated_slaves, $last_used_connection, $in_transaction) {
		global $queries, $host, $autocommit;
		static $pick_server_last_used = "";
		flush();

		$print = true;
		$args = func_get_args();
		$num = func_num_args();
		if ($num != 6) {
			printf("[003] Number of arguments should be 6 got %d\n", $num);
			var_dump($args);
		}

		if ($in_transaction == $autocommit) {
			printf("[004] in_transaction should be %d\n", !$autocommit);
		}

		if ("" == $connected_host) {
			printf("[005] Currently connected host is empty\n");
		}
		if (false == stristr($query, "util.inc")) {
			if (!isset($queries[$query])) {
				printf("[006] We are asked to handle the query '%s' which has not been issued by us.\n", $query);
			} else {
				unset($queries[$query]);
			}
		} else {
			/* query to fetch emulated server/thread id */
			$print = false;
			if (isset($queries[$query]))
				unset($queries[$query]);
		}

		if (!is_array($emulated_master)) {
			printf("[007] No list of master servers given.");
		} else {
			/* we can't do much better because we get string representations of the master/slave connections */
			if (1 != ($tmp = count($emulated_master))) {
				printf("[008] Expecting one entry in the list of masters, found %d. Dumping list.\n", $tmp);
				var_dump($emulated_master);
			}
		}

		if (!is_array($emulated_slaves)) {
			printf("[009] No list of slave servers given.");
		} else {
			if (1 != ($tmp = count($emulated_slaves))) {
				printf("[010] Expecting one entry in the list of slaves, found %d. Dumping list.\n", $tmp);
				var_dump($emulated_slaves);
			}
		}

		if ($last_used_connection != $pick_server_last_used) {
			printf("[011] Last used connection should be '%s' but got '%s'.\n", $pick_server_last_used, $last_used_connection);
		}

		$ret = "";
		$where = mysqlnd_ms_query_is_select($query);
		$server = '';
		switch ($where) {
			case MYSQLND_MS_QUERY_USE_LAST_USED:
			  $ret = $last_used_connection;
			  $server = 'last used';
			  break;
			case MYSQLND_MS_QUERY_USE_MASTER:
			  $ret = $emulated_master[0];
			  $server = 'master';
			  break;
			case MYSQLND_MS_QUERY_USE_SLAVE:
 			  $ret = $emulated_slaves[0];
			  $server = 'slave';
			  break;
			default:
			  printf("[012] Unknown return value from mysqlnd_ms_query_is_select, where = %s .\n", $where);
			  $ret = $emulated_master[0];
			  $server = 'unknown';
			  break;
		}

		if ($print)
			printf("'%s' => %s\n", $query, $server);

		$pick_server_last_used = $ret;
		return $ret;
	}


	function my_mysqli_query($offset, $link, $query, $expected) {
		global $queries;

		$queries[$query] = $query;
		if (!$res = $link->query($query)) {
			printf("[%03d + 01] [%d] %s\n", $offset, $link->errno, $link->error);
			return false;
		}
		if ($expected) {
		  $row = $res->fetch_assoc();
		  $res->close();
		  if (empty($row)) {
			  printf("[%03d + 02] [%d] %s, empty result\n", $offset, $link->errno, $link->error);
			  return false;
		  }
		  if ($row != $expected) {
			  printf("[%03d + 03] Unexpected results, dumping data\n", $offset);
			  var_dump($row);
			  var_dump($expected);
			  return false;
		  }
		}
		return true;
	}

	function check_master_slave_threads($offset, $threads) {

		if (isset($threads["slave"]) && isset($threads["master"])) {
			foreach ($threads["slave"] as $server_id => $num_queries) {
				if (isset($threads["master"][$server_id])) {
					printf("[%03d + 01] Slave connection thread=%s is also a master connection!\n",
						$offset, $server_id);
					unset($threads["slave"][$server_id]);
				}
			}
			foreach ($threads["master"] as $server_id => $num_queries) {
				if (isset($threads["slave"][$server_id])) {
					printf("[%03d + 02] Master connection thread=%s is also a slave connection!\n",
						$offset, $server_id);
				}
			}
		}
	}

	$threads = array();
	if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
		printf("[002] Cannot connect to the server using host=%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
			$host, $user, $db, $port, $socket);

	$autocommit = true;
	$link->autocommit($autocommit);

	/* Should go to the first slave */
	$query = "SELECT 'Master Andrey has send this query to a slave.' AS _message FROM DUAL";
	$expected = array('_message' => 'Master Andrey has send this query to a slave.');
	my_mysqli_query(20, $link, $query, $expected);
	$server_id = mst_mysqli_get_emulated_id(21, $link);
	if (!isset($threads["slave"][$server_id])) {
		$threads["slave"][$server_id] = 1;
	} else {
		$threads["slave"][$server_id]++;
	}
	check_master_slave_threads(30, $threads);

	/* Should go to the first master */
	$query = sprintf("/*%s*/SELECT 'master' AS _message FROM DUAL", MYSQLND_MS_MASTER_SWITCH);

	$expected = array('_message' => 'master');
	my_mysqli_query(40, $link, $query, $expected);
	$server_id = mst_mysqli_get_emulated_id(41, $link);
	if (!isset($threads["master"][$server_id])) {
		$threads["master"][$server_id] = 1;
	} else {
		$threads["master"][$server_id]++;
	}
	check_master_slave_threads(50, $threads);

	/* Should go to the first master */
	$query = sprintf("/*%s*/SELECT 'master' AS _message FROM DUAL", MYSQLND_MS_MASTER_SWITCH);
	$expected = array('_message' => 'master');
	my_mysqli_query(60, $link, $query, $expected);
	$server_id = mst_mysqli_get_emulated_id(61, $link);
	if (!isset($threads["master"][$server_id])) {
		$threads["master"][$server_id] = 1;
	} else {
		$threads["master"][$server_id]++;
	}
	check_master_slave_threads(70, $threads);
	if ($threads["master"][$server_id] != 2) {
		printf("[071] Master should have run 2 queries, records report %d\n", $threads["master"][$serverid]);
	}

	/* Should go to the first slave */
	$query = sprintf("/*%s*/SELECT 'slave' AS _message FROM DUAL", MYSQLND_MS_SLAVE_SWITCH);
	$expected = array('_message' => 'slave');
	my_mysqli_query(80, $link, $query, $expected);
	$server_id = mst_mysqli_get_emulated_id(81, $link);
	if (!isset($threads["slave"][$server_id])) {
		$threads["slave"][$server_id] = 1;
	} else {
		$threads["slave"][$server_id]++;
	}
	check_master_slave_threads(90, $threads);
	if ($threads["slave"][$server_id] != 2) {
		printf("[091] Slave should have run 2 queries, records report %d\n", $threads["slave"][$server_id]);
	}

	$autocommit = false;
	$link->autocommit($autocommit);

	/* Should go to the first slave with in_transaction = true */
	$query = sprintf("/*%s*/SELECT 'slave' AS _message FROM DUAL", MYSQLND_MS_SLAVE_SWITCH);
	$expected = array('_message' => 'slave');
	my_mysqli_query(100, $link, $query, $expected);
	$server_id = mst_mysqli_get_emulated_id(101, $link);
	if (!isset($threads["slave"][$server_id])) {
		$threads["slave"][$server_id] = 1;
	} else {
		$threads["slave"][$server_id]++;
	}
	check_master_slave_threads(110, $threads);
	if ($threads["slave"][$server_id] != 3) {
		printf("[111] Slave should have run 3 queries, records report %d\n", $threads["slave"][$server_id]);
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pick_user_complex.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pick_user_complex.ini'.\n");
?>
--EXPECTF--
'SELECT 'Master Andrey has send this query to a slave.' AS _message FROM DUAL' => slave
'/*ms=master*/SELECT 'master' AS _message FROM DUAL' => master
'/*ms=master*/SELECT 'master' AS _message FROM DUAL' => master
'/*ms=slave*/SELECT 'slave' AS _message FROM DUAL' => slave
'/*ms=slave*/SELECT 'slave' AS _message FROM DUAL' => slave
done!