--TEST--
mysqlnd_ms_get_last_used_connection() switching
--SKIPIF--
<?php
require_once('skipif.inc');
if (version_compare(PHP_VERSION, '5.3.99', "<")) {
	die("SKIP Function not available before PHP 5.4.0");
}

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP master and slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_connect($host, $user, $passwd, $db, $port, $socket);
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
		'slave' => array($emulated_slave_host, $emulated_slave_host),
		'lazy_connections' => 1,
		'filters' => array(
			"roundrobin" => array(),
		),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_get_last_used_connection_switches.ini", $settings))
	die(sprintf("SKIP %s\n", $error));


msg_mysqli_init_emulated_id_skip($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket, "slave[1,2]");
msg_mysqli_init_emulated_id_skip($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, "master");
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_get_last_used_connection_switches.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function conn_diff($offset, $conn, $members, $expected = NULL, $ignore_list = NULL) {

		if (!is_array($conn)) {
			printf("[%03d + 01] No array, got %s\n", $offset, var_export($conn, true));
			return false;
		}

		foreach ($conn as $prop => $value) {
			if (!empty($ignore_list) && in_array($prop, $ignore_list)) {
				unset($members[$prop]);
				continue;
			}

			if (isset($members[$prop])) {
				$type = gettype($value);
				$type = ("integer" == $type) ? "int" : $type;
				if ($type != $members[$prop]) {
					printf("[%03d + 02] Property %s should be of type %s, got %s\n",
					  $offset, $members[$prop], $type);
				}

				if (isset($expected[$prop])) {
					if ($expected[$prop] !== $value) {
						printf("[%03d + 03] Expecting %s = %s, got %s\n",
							$offset, $prop, var_export($expected[$prop], true), var_export($value, true));
					}
					unset($expected[$prop]);
				} else {
					switch ($members[$prop]) {
						case "string":
							if ($value !== "") {
								printf("[%03d + 04] Expecting %s = <empty string>, got %s\n",
									$offset, $prop, var_export($value, true));
							}
							break;
						case "int":
							if ($value !== 0) {
								printf("[%03d + 05] Expecting %s = 0, got %s\n",
								  $offset, $prop, var_export($value, true));
							}
							break;
						case "array":
							if (0 !== count($value)) {
								printf("[%03d + 06] Expecting %s = <empty array>, got %s\n",
									$offset, $prop, var_export($value, true));
							}
							break;
						default:
							break;
					}
				}
				unset($members[$prop]);
			} else {
				if (empty($ignore_list) || !in_array($prop, $ignore_list)) {
					printf("[%03d + 07] Unexpected %s = %s\n",
						$offset, $prop, var_export($value, true));
				}
			}
		}


		if (!empty($members)) {
			printf("[%03d + 08] Dumping list of missing properties\n", $offset);
			var_dump($members);
			return false;
		}

		return true;
	}


	$members = array(
		"scheme" 			=> "string",
		"host_info"			=> "string",
		"host" 				=> "string",
		"port" 				=> "int",
		"socket_or_pipe"	=> "string",
		"thread_id" 		=> "int",
		"last_message" 		=> "string",
		"errno" 			=> "int",
		"error" 			=> "string",
		"sqlstate" 			=> "string",
	);

	if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
		printf("[001] Cannot connect to the server using host=%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
			$host, $user, $db, $port, $socket);

	/* lazy and no stmt run */
	$expected = array("sqlstate" => $link->sqlstate);
	$conn = mysqlnd_ms_get_last_used_connection($link);
	/* this is hackish but I can't think of a better way of implementing at the C level */
	conn_diff(2, $conn, $members,  $expected, (isset($expected['port']) && (0 == $expected['port'])) ? array('port') : array());

	/* master */
	mst_mysqli_query(3, $link, "SET @myrole='master'");

	$expected = array(
		"host" 				=> ("localhost" == $emulated_master_host_only) ? "" : $emulated_master_host_only,
		"host_info" 		=> $link->host_info,
		"port"				=> (int)$emulated_master_port,
		"socket_or_pipe"	=> ("localhost" == $emulated_master_host_only) ? (($emulated_master_socket) ? $emulated_master_socket : "/tmp/mysql.sock") : "",
		"thread_id" 		=> $link->thread_id,
		"errno" 			=> $link->errno,
		"error" 			=> $link->error,
		"sqlstate" 			=> $link->sqlstate,
	);
	if ("localhost" != $emulated_master_host && !$emulated_master_socket) {
		$expected["scheme"] = sprintf("tcp://%s:%d", $emulated_master_host_only, $emulated_master_port);
	}
	$conn = mysqlnd_ms_get_last_used_connection($link);
	if (!isset($expected["scheme"]) && isset($conn["scheme"]))
		/* accept whatever "&/"&/"§ default socket there may be... */
		$expected["scheme"] = $conn["scheme"];

	conn_diff(4, $conn, $members,  $expected, (isset($expected['port']) && (0 == $expected['port'])) ? array('port') : array());
	$threads[mst_mysqli_get_emulated_id(5, $link)] = array($link->thread_id, $conn["scheme"]);

	/* slave 1 */
	mst_mysqli_query(6, $link, "SET @myrole='slave1'", MYSQLND_MS_SLAVE_SWITCH);

	$expected["thread_id"] = $link->thread_id;
	$expected["host"] = ("localhost" == $emulated_slave_host_only) ? "" : $emulated_slave_host_only;
	$expected["port"] = (int)$emulated_slave_port;
	$expected["socket_or_pipe"] = ("localhost" == $emulated_slave_host_only) ? (($emulated_slave_socket) ? $emulated_slave_socket : "/tmp/mysql.sock") : "";
	$expected["host_info"] = $link->host_info;

	if (("localhost" != $emulated_slave_host_only) && (!$emulated_slave_socket)) {
		$expected["scheme"] = sprintf("tcp://%s:%d", $emulated_slave_host_only, $emulated_slave_port);
	} else {
		unset($expected["scheme"]);
	}
	$conn = mysqlnd_ms_get_last_used_connection($link);
	if (!isset($expected["scheme"]) && isset($conn["scheme"]))
		/* accept whatever "&/"&/"§ default socket there may be... */
		$expected["scheme"] = $conn["scheme"];

	conn_diff(7, $conn, $members,  $expected, (isset($expected['port']) && (0 == $expected['port'])) ? array('port') : array());

	$threads[mst_mysqli_get_emulated_id(8, $link)] = array($link->thread_id, $conn["scheme"]);

	/* slave 2 */
	mst_mysqli_query(9, $link, "SET @myrole='slave2'", MYSQLND_MS_SLAVE_SWITCH);

	$expected["thread_id"] = $link->thread_id;
	$conn = mysqlnd_ms_get_last_used_connection($link);
	conn_diff(10, $conn, $members,  $expected, (isset($expected['port']) && (0 == $expected['port'])) ? array('port') : array());
	$threads[mst_mysqli_get_emulated_id(11, $link)] = array($link->thread_id, $conn["scheme"]);

	/* rr: slave 1 */
	$res = mst_mysqli_query(12, $link, "SELECT @myrole AS _role");
	$row = $res->fetch_assoc();
	printf("[013] Hi folks, %s speaking.\n", $row['_role']);

	$conn = mysqlnd_ms_get_last_used_connection($link);
	$exp = $threads[mst_mysqli_get_emulated_id(14, $link)];
	if ($conn["thread_id"] != $exp[0]) {
		printf("[015] Thread id seems wrong. Check manually.\n");
	}
	if ($conn["scheme"] != $exp[1]) {
		printf("[016] Scheme seems wrong. Check manually.\n");
	}


	/* rr: slave 2 */
	$res = mst_mysqli_query(17, $link, "SELECT @myrole AS _role");
	$row = $res->fetch_assoc();
	printf("[018] Hi folks, %s speaking.\n", $row['_role']);

	$conn = mysqlnd_ms_get_last_used_connection($link);
	$exp = $threads[mst_mysqli_get_emulated_id(19, $link)];
	if ($conn["thread_id"] != $exp[0]) {
		printf("[020] Thread id seems wrong. Check manually.\n");
	}
	if ($conn["scheme"] != $exp[1]) {
		printf("[021] Scheme seems wrong. Check manually.\n");
	}

	/* master */
	$res = mst_mysqli_query(22, $link, "SELECT @myrole AS _role", MYSQLND_MS_MASTER_SWITCH);
	$row = $res->fetch_assoc();
	printf("[023] Hi folks, %s speaking.\n", $row['_role']);

	$conn = mysqlnd_ms_get_last_used_connection($link);
	$exp = $threads[mst_mysqli_get_emulated_id(24, $link)];
	if ($conn["thread_id"] != $exp[0]) {
		printf("[025] Thread id seems wrong. Check manually.\n");
	}
	if ($conn["scheme"] != $exp[1]) {
		printf("[026] Scheme seems wrong. Check manually.\n");
	}

	/* master */
	$res = mst_mysqli_query(27, $link, "SELECT @myrole AS _role", MYSQLND_MS_LAST_USED_SWITCH);
	$row = $res->fetch_assoc();
	printf("[028] Hi folks, %s speaking.\n", $row['_role']);

	$conn = mysqlnd_ms_get_last_used_connection($link);
	$exp = $threads[mst_mysqli_get_emulated_id(29, $link)];
	if ($conn["thread_id"] != $exp[0]) {
		printf("[030] Thread id seems wrong. Check manually.\n");
	}
	if ($conn["scheme"] != $exp[1]) {
		printf("[031] Scheme seems wrong. Check manually.\n");
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_get_last_used_connection_switches.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_get_last_used_connection_switches.ini'.\n");
?>
--EXPECTF--
[013] Hi folks, slave1 speaking.
[018] Hi folks, slave2 speaking.
[023] Hi folks, master speaking.
[028] Hi folks, master speaking.
done!