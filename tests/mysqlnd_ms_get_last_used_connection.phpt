--TEST--
mysqlnd_ms_get_last_used_connection()
--SKIPIF--
<?php
require_once('skipif.inc');
if (version_compare(PHP_VERSION, '5.3.99', "<")) {
	die("SKIP Function not available before PHP 5.4.0");
}
_skipif_check_extensions(array("mysqli"));
_skipif_connect($host, $user, $passwd, $db, $port, $socket);
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
		'lazy_connections' => 0,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_get_last_used_connection.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_get_last_used_connection.ini
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

	if (!is_null($ret = @mysqlnd_ms_get_last_used_connection()))
		printf("[001] Expecting NULL got %s\n", var_export($ret, true));

	if (false !== ($ret = @mysqlnd_ms_get_last_used_connection("test")))
	  printf("[002] Expecting FALSE got %s\n", var_export($ret, true));

	if (!is_null($ret = @mysqlnd_ms_get_last_used_connection("test", "test")))
	  printf("[003] Expecting NULL got %s\n", var_export($ret, true));

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

	/* mysqli */
	$link = new mysqli();
	$conn = mysqlnd_ms_get_last_used_connection($link);
	conn_diff(4, $conn, $members);

	/* non MS */
	if (!$link = mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket))
		printf("[005] Cannot connect to the server using host=%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
			$host, $user, $db, $port, $socket);

	$expected = array(
		"host" 				=> ("localhost" == $host) ? "" : $host,
		"host_info" 		=> $link->host_info,
		"port"				=> (int)$port,
		"socket_or_pipe"	=> ("localhost" == $host) ? (($socket) ? $socket : "/tmp/mysql.sock") : "",
		"thread_id" 		=> $link->thread_id,
		"errno" 			=> $link->errno,
		"error" 			=> $link->error,
		"sqlstate" 			=> $link->sqlstate,
	);

	if ("localhost" != $host && !$socket) {
		$expected["scheme"] = sprintf("tcp://%s:%d", $host, $port);
	}
	$conn = mysqlnd_ms_get_last_used_connection($link);
	if (!isset($expected["scheme"]) && isset($conn["scheme"]))
		/* accept whatever "&/"&/"§ default socket there may be... */
		$expected["scheme"] = $conn["scheme"];
			/* this is hackish but I can't think of a better way of implementing at the C level */
	conn_diff(6, $conn, $members, $expected, (0 == $expected['port']) ? array('port') : array());
	/* error on non MS */
	@$link->query("PLEASE, LET THIS BE INVALID My-S-Q-L");
	$expected["errno"] = $link->errno;
	$expected["error"] = $link->error;
	$expected["sqlstate"] = $link->sqlstate;
	$conn = mysqlnd_ms_get_last_used_connection($link);
	conn_diff(7, $conn, $members, $expected, (0 == $expected['port']) ? array('port') : array());

	@$link->query("YEAH, HEY, OK, HEY, ..");
	$expected["errno"] = $link->errno;
	$expected["error"] = $link->error;
	$expected["sqlstate"] = $link->sqlstate;
	$conn = mysqlnd_ms_get_last_used_connection($link);
	conn_diff(8, $conn, $members, $expected, (0 == $expected['port']) ? array('port') : array());

	if (!$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))
		printf("[011] Cannot connect to the server using host=%s, user=%s, passwd=***, dbname=%s, port=%s, socket=%s\n",
			$host, $user, $db, $port, $socket);

	$expected = array(
		"host" 				=> ("localhost" == $master_host_only) ? "" : $master_host_only,
		"host_info" 		=> $link->host_info,
		"port"				=> (int)$master_port,
		"socket_or_pipe"	=> ("localhost" == $master_host_only) ? (($master_socket) ? $master_socket : "/tmp/mysql.sock") : "",
		"thread_id" 		=> $link->thread_id,
		"errno" 			=> $link->errno,
		"error" 			=> $link->error,
		"sqlstate" 			=> $link->sqlstate,
	);
	if ("localhost" != $master_host_only && !$master_socket) {
		$expected["port"] = (int)$master_port;
		$expected["scheme"] = sprintf("tcp://%s:%d", $master_host_only, $master_port);
	}

	$conn = mysqlnd_ms_get_last_used_connection($link);
	if (!isset($expected["scheme"]) && isset($conn["scheme"]))
		/* accept whatever "&/"&/"§ default socket there may be... */
		$expected["scheme"] = $conn["scheme"];

	conn_diff(12, $conn, $members, $expected, (0 == $expected['port']) ? array('port') : array());

	/* should go to the master */
	/* error on MS */
	@$link->query("PLEASE, LET THIS BE INVALID My-S-Q-L");
	$expected["errno"] = $link->errno;
	$expected["error"] = $link->error;
	$expected["sqlstate"] = $link->sqlstate;
	$conn = mysqlnd_ms_get_last_used_connection($link);
	conn_diff(13, $conn, $members, $expected, (0 == $expected['port']) ? array('port') : array());

	/* should go to the master */
	@$link->query("PLEASE, LET THIS BE INVALID My-S-Q-L");
	$expected["errno"] = $link->errno;
	$expected["error"] = $link->error;
	$expected["sqlstate"] = $link->sqlstate;
	$conn = mysqlnd_ms_get_last_used_connection($link);
	conn_diff(14, $conn, $members, $expected, (0 == $expected['port']) ? array('port') : array());

	/* should go to the master */
	@$link->select_db("My-S-Q-L rocks, My-S-Q-L");
	$expected["errno"] = $link->errno;
	$expected["error"] = $link->error;
	$expected["sqlstate"] = $link->sqlstate;
	$conn = mysqlnd_ms_get_last_used_connection($link);
	conn_diff(15, $conn, $members, $expected, (0 == $expected['port']) ? array('port') : array());

	$link->kill($link->thread_id);
	/* give server think time */
	sleep(1);
	$expected["errno"] = $link->errno;
	$expected["error"] = $link->error;
	$expected["sqlstate"] = $link->sqlstate;
	$conn = mysqlnd_ms_get_last_used_connection($link);
	conn_diff(16, $conn, $members, $expected, (0 == $expected['port']) ? array('port') : array());

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_get_last_used_connection.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_get_last_used_connection.ini'.\n");
?>
--EXPECTF--
done!