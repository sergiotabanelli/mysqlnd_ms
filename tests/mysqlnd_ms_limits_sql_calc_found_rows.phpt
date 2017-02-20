--TEST--
SQL_CALC_FOUND_ROWS and RR
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host, $slave_host),
		'pick' => array("roundrobin"),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_limits_sql_calc_found_rows.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_limits_sql_calc_found_rows.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	mst_mysqli_query(2, $link, "SET @myrole='slave1'", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(3, $link, "SET @myrole='slave2'", MYSQLND_MS_SLAVE_SWITCH);
	mst_mysqli_query(4, $link, "SET @myrole='master'", MYSQLND_MS_MASTER_SWITCH);

	mst_mysqli_create_test_table($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

	/* slave 1 */
	$res = mst_mysqli_query(5, $link, "SELECT SQL_CALC_FOUND_ROWS id FROM test WHERE id < 3 LIMIT 1");
	$rows = $res->num_rows;
	/* round robin: slave 2 -
       found_rows() not set by previous query - found_rows() = 1
	*/
	$res = mst_mysqli_query(6, $link, "SELECT @myrole AS _role, FOUND_ROWS() AS _found");
	$row = $res->fetch_assoc();
	printf("Num rows %d, found rows %d, role %s\n", $rows, $row['_found'], $row['_role']);

	/* slave 1 */
	$res = mst_mysqli_query(5, $link, "SELECT SQL_CALC_FOUND_ROWS id FROM test WHERE id < 3 LIMIT 1");
	$rows = $res->num_rows;

	/* round robin: slave 1  (SQL hint used) -
    found_rows() set by previous query - found_rows() = 2
	*/
	$res = mst_mysqli_query(6, $link, "SELECT @myrole AS _role, FOUND_ROWS() AS _found", MYSQLND_MS_LAST_USED_SWITCH);
	$row = $res->fetch_assoc();
	printf("Num rows %d, found rows %d, role %s\n", $rows, $row['_found'], $row['_role']);


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_limits_sql_calc_found_rows.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_ini_force_config.ini'.\n");
?>
--EXPECTF--
Num rows 1, found rows 0, role slave2
Num rows 1, found rows 2, role slave1
done!