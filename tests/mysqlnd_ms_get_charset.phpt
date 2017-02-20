--TEST--
mysqli_get_charsets()
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host, $slave_host),
		'pick' => 'roundrobin',
		'lazy_connections' => 0,
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_charsets.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);


function test_for_charset($host, $user, $passwd, $db, $port, $socket) {
	if (!$link = mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket))
		die(sprintf("skip Cannot connect, [%d] %s", mysqli_connect_errno(), mysqli_connect_error()));

	if (!($res = mysqli_query($link, 'SELECT version() AS server_version')) ||
			!($tmp = mysqli_fetch_assoc($res))) {
		mysqli_close($link);
		die(sprintf("skip Cannot check server version, [%d] %s\n",
		mysqli_errno($link), mysqli_error($link)));
	}
	mysqli_free_result($res);
	$version = explode('.', $tmp['server_version']);
	if (empty($version)) {
		mysqli_close($link);
		die(sprintf("skip Cannot check server version, based on '%s'",
			$tmp['server_version']));
	}

	if ($version[0] <= 4 && $version[1] < 1) {
		mysqli_close($link);
		die(sprintf("skip Requires MySQL Server 4.1+\n"));
	}

	if ((($res = mysqli_query($link, "SHOW CHARACTER SET LIKE 'latin1'", MYSQLI_STORE_RESULT)) &&
			(mysqli_num_rows($res) == 1)) ||
			(($res = mysqli_query($link, "SHOW CHARACTER SET LIKE 'latin2'", MYSQLI_STORE_RESULT)) &&
			(mysqli_num_rows($res) == 1))
			) {
		// ok, required latin1 or latin2 are available
	} else {
		die(sprintf("skip Requires character set latin1 or latin2\n"));
	}

	if (!$res = mysqli_query($link, 'SELECT @@character_set_connection AS charset'))
		die(sprintf("skip Cannot select current charset, [%d] %s\n", $link->errno, $link->error));

	if (!$row = mysqli_fetch_assoc($res))
		die(sprintf("skip Cannot detect current charset, [%d] %s\n", $link->errno, $link->error));

	return $row['charset'];
}

$master_charset = test_for_charset($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
$slave_charset = test_for_charset($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

if ($master_charset != $slave_charset) {
	die(sprintf("skip Master (%s) and slave (%s) must use the same default charset.", $master_charset, $slave_charset));
}
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_charsets.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function check_charset($offset, $link, $expected) {

		$charset = $link->get_charset();

		if ($charset->charset != $expected['charset'])
			printf("[%03d + 01] Expecting charset '%s' got '%s'\n", $offset, $expected['charset'], $charset->charset);

		if ($charset->collation != $expected['collation'])
			printf("[%03d + 02] Expecting collation '%s' got '%s'\n", $offset, $expected['collation'], $charset->collation);

		if ($charset->max_length != $expected['max_length'])
			printf("[%03d + 03] Expecting max length %d got %d '\n", $offset, $expected['charset'], $charset->max_length);

		if (($charset->min_length < 0) || ($charset->min_length > $expected['max_length']))
			printf("[%03d + 04] Expecting min length 0..%d got %d\n", $offset, $expected['max_length']. $charset->min_length);

	}

	/*
	Charset changes made through the user API are applied to all
	connections. This is important and a security must because string
	escaping depends on the current charset. If the plugin would
	not take care of it, the following sequence would bear a security risk.

	INSERT - master, charset latin1
    $somestring = escape_string(link, "somestring") - would use latin1 from last used connection
    SELECT * FROM test WHERE column = $somestring - slave, slave may be using latin2, $somestring should have been escaped using latin2
	*/
	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);
	mst_mysqli_query(3, $link, "SET @myrole='slave 1'", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(4, $link, "SET @myrole='slave 2'", MYSQLND_MS_SLAVE_SWITCH);


	/* master */
	$res = mst_mysqli_query(5, $link, "SHOW CHARACTER SET LIKE 'latin1'");
	if (!$row = $res->fetch_assoc())
		printf("[006] [%d] %s\n", $link->errno, $link->error);

	$latin1 = array(
		'charset' 		=> (isset($row['Charset'])) ? $row['Charset'] : NULL,
		'collation'		=> (isset($row['Default collation'])) ? $row['Default collation'] : NULL,
		'max_length'	=> (isset($row['Maxlen'])) ? $row['Maxlen'] : NULL,
	);

	/* master */
	if (!$res = mst_mysqli_query(7, $link, "SHOW CHARACTER SET LIKE 'latin2'"))
		printf("[008] [%d] %s\n", $link->errno, $link->error);

	if (!$row = $res->fetch_assoc())
		printf("[009] [%d] %s\n", $link->errno, $link->error);

	$latin2 = array(
		'charset' 		=> (isset($row['Charset'])) ? $row['Charset'] : NULL,
		'collation'		=> (isset($row['Default collation'])) ? $row['Default collation'] : NULL,
		'max_length'	=> (isset($row['Maxlen'])) ? $row['Maxlen'] : NULL,
	);

	/* slave 1 */
	if (!$res = mst_mysqli_query(4, $link, "SELECT @@character_set_connection AS charset", MYSQLND_MS_SLAVE_SWITCH))
		printf("[010] [%d] %s\n", $link->errno, $link->error);

	if (!$row = $res->fetch_assoc())
		printf("[011] [%d] %s\n", $link->errno, $link->error);

	$current_charset = $row['charset'];
	if ('latin1' == $current_charset) {
		$new_charset = 'latin2';
		$new_expected = $latin2;
		check_charset(12, $link, $latin1);
	} else {
		$new_charset = 'latin1';
		$new_expected = $latin1;
		check_charset(12, $link, $latin2);
	}


	/* shall be run on *all* configured machines - all masters, all slaves */
	$link->set_charset($new_charset);

	/* slave 1 */
	if (!$res = mst_mysqli_query(13, $link, "SELECT @@character_set_connection AS charset", MYSQLND_MS_LAST_USED_SWITCH))
		printf("[014] [%d] %s\n", $link->errno, $link->error);

	if (!$row = $res->fetch_assoc())
		printf("[015] [%d] %s\n", $link->errno, $link->error);

	$current_charset = $row['charset'];
	if ($current_charset != $new_charset)
		printf("[016] Expecting charset '%s' got '%s'\n", $new_charset, $current_charset);

	check_charset(17, $link, $new_expected);

	/* master */
	if (!$res = mst_mysqli_query(11, $link, "SELECT @@character_set_connection AS charset", MYSQLND_MS_MASTER_SWITCH))
		printf("[018] [%d] %s\n", $link->errno, $link->error);

	if (!$row = $res->fetch_assoc())
		printf("[019] [%d] %s\n", $link->errno, $link->error);

	$current_charset = $row['charset'];
	if ($current_charset != $new_charset)
		printf("[020] Expecting charset '%s' got '%s'\n", $new_charset, $current_charset);

	if ($link->character_set_name() != $new_charset)
		printf("[021] Expecting charset '%s' got '%s'\n", $new_charset, $link->character_set_name);

	check_charset(22, $link, $new_expected);

	/* slave 2 */
	if (!$res = mst_mysqli_query(13, $link, "SELECT @@character_set_connection AS charset", MYSQLND_MS_SLAVE_SWITCH))
		printf("[023] [%d] %s\n", $link->errno, $link->error);

	if (!$row = $res->fetch_assoc())
		printf("[024] [%d] %s\n", $link->errno, $link->error);

	$current_charset = $row['charset'];
	if ($current_charset != $new_charset)
		printf("[025] Expecting charset '%s' got '%s'\n", $new_charset, $current_charset);

	check_charset(26, $link, $new_expected);

	print "done!";

?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_charsets.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_charsets.ini'.\n");
?>
--EXPECTF--
done!