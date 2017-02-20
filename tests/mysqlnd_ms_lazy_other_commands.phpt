--TEST--
lazy connections and assorted commands called before connect (mysqlnd >= 5.0.9)
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");
if ($MYSQLND_VERSION < 50009) {
  die("SKIP Requires mysqlnd 5.0.9+, found $MYSQLND_VERSION");
}
_skipif_check_extensions(array("mysqli"));
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
		),

		'slave' => array(
			"slave1" => array(
				'host' 	=> $slave_host_only,
				'port' 	=> (int)$slave_port,
				'socket' => $slave_socket,
			),
		 ),

		'lazy_connections' => 1,
		'filters' => array(
			"random" => array('sticky' => '1'),
		),
	),

);
if ($error = mst_create_config("test_mysqlnd_ms_lazy_escape.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_lazy_escape.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function callme($stream, &$buffer, $buflen, &$errmsg) {
		$buffer = fgets($stream);
		echo $buffer;
		// convert to upper case and replace "," delimiter with [TAB]
		$buffer = strtoupper(str_replace(",", "\t", $buffer));
		return strlen($buffer);
	}

	function check_codes($offset, $link, $ret, $silent = false) {
		if (mysqli_connect_errno()) {
			printf("[%03d] [%d] %s\n", $offset, mysqli_connect_errno(), mysqli_connect_error());
			return;
		}
		if ($link->errno) {
			printf("[%03d] [%d/%s] %s\n", $offset, $link->errno, $link->sqlstate, $link->error);
			return;
		}
		if (!$silent)
		  printf("[%03d] %s\n", $offset, var_export($ret, true));
	}

	$link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket);
	if (mysqli_connect_errno()) {
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
	}

	check_codes(3, $link, $link->errno);
	check_codes(4, $link, $link->error);
	check_codes(5, $link, $link->sqlstate);
	check_codes(6, $link, $link->thread_id);
	check_codes(7, $link, $link->server_version);
	check_codes(8, $link, $link->server_info);
	check_codes(9, $link, $link->host_info);
	check_codes(10, $link, $link->protocol_version);
	check_codes(11, $link, $link->insert_id);
	check_codes(12, $link, $link->affected_rows);
	check_codes(13, $link, $link->warning_count);
	check_codes(14, $link, $link->field_count);
	check_codes(15, $link, $link->client_version);
	check_codes(16, $link, $link->client_info);
	check_codes(17, $link, $link->info);
	check_codes(18, $link, $link->autocommit(false));
	check_codes(19, $link, $link->autocommit(true));
	check_codes(20, $link, $link->dump_debug_info());
	check_codes(21, $link, $link->ping());
	check_codes(22, $link, $link->autocommit(false));
	check_codes(23, $link, $link->options(MYSQLI_OPT_CONNECT_TIMEOUT, 10));
	check_codes(24, $link, $link->autocommit(false));
	check_codes(25, $link, $link->dump_debug_info());
	check_codes(26, $link, $link->stmt_init(), 1);
	check_codes(29, $link, $link->kill(-1));
	check_codes(30, $link, $link->select_db($db));
	check_codes(31, $link, $link->ssl_set('blubb_server-key.pem','blubb_server-cert.pem', 'blubb_cacert.pem', NULL, NULL));
	check_codes(32, $link, $link->dump_debug_info());
	check_codes(33, $link, $link->change_user($user, $passwd, $db));

/*
	check_codes(71, $link, mysqli_get_server_version($link));
	check_codes(81, $link, mysqli_get_server_info($link));
	check_codes(91, $link, mysqli_get_host_info($link));
	check_codes(101, $link, mysqli_get_proto_info($link));
	check_codes(102, $link, mysqli_character_set_name($link));
	check_codes(103, $link, mysqli_stat($link));
	check_codes(104, $link, $link->dump_debug_info());
	check_codes(105, $link, mysqli_dump_debug_info($link));
*/
	check_codes(45, $link, $link->close());

	if (function_exists('mysqli_set_local_infile_handler')) {
		mysqli_set_local_infile_handler($link, "callme");
	}
	if (function_exists('mysqli_set_local_infile_default')) {
		mysqli_set_local_infile_default($link);
	}

	if (function_exists('mysqli_get_client_stats')) {
		mysqli_get_client_stats();
	}
	if (function_exists('mysqli_get_connection_stats')) {
		$link->get_connection_stats();
	}

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");

	if (!unlink("test_mysqlnd_ms_lazy_escape.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_lazy_escape.ini'.\n");
?>
--EXPECTF--
[003] %d
[004] ''
[005] '00000'
[006] 0
[007] %d
[008] %s
[009] %s
[010] 10
[011] 0
[012] 0
[013] 0
[014] 0
[015] %d
[016] '%s'
[017] NULL
[018] true
[019] true
[020] true
[021] true
[022] true
[023] true
[024] true
[025] true

Warning: mysqli::kill(): processid should have positive value in %s on line %d
[029] false
[030] true
[031] true
[032] true
[033] true

Warning: check_codes(): Couldn't fetch mysqli in %s on line %d
[045] true

Warning: mysqli::get_connection_stats(): Couldn't fetch mysqli in %s on line %d
done!