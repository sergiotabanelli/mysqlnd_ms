--TEST--
PDO Basics with options & query starting with parenthesis
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("pdo_mysql"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_pdo_basics_options.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_pdo_basics_options.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	$options = array(
		PDO::MYSQL_ATTR_INIT_COMMAND => "SET @myinitcommand = 'something'",
    	PDO::MYSQL_ATTR_SSL_KEY    => $client_key,
    	PDO::MYSQL_ATTR_SSL_CERT   => $client_cert,
    	PDO::MYSQL_ATTR_SSL_CA     => $ca_cert,
		PDO::MYSQL_ATTR_SSL_VERIFY_SERVER_CERT => false
	);
	$host = "myapp";
	$dsn = sprintf("mysql:host=%s;port=%d;dbname=%s", $host, $port, $db);
	try {
		$pdo = new PDO($dsn,  $user, $passwd, $options);
		$query = sprintf("/*%s*/%s", MYSQLND_MS_MASTER_SWITCH, "(SELECT @myinitcommand AS _myinit)");
		$stmt = $pdo->query($query);
		$row = $stmt->fetch();
		var_dump($row);
		if ('something' != $row['_myinit']) {
			printf("[002] Expecting 'something' got '%s'\n", $row['_myinit']);
		}
		$query = sprintf("/*%s*/%s", MYSQLND_MS_SLAVE_SWITCH, "(SELECT @myinitcommand AS _myinit)");
		$stmt = $pdo->query($query);
		$row = $stmt->fetch();
		var_dump($row);
		if ('something' != $row['_myinit']) {
			printf("[003] Expecting 'something' got '%s'\n", $row['_myinit']);
		}

		$query = sprintf("/*%s*/%s", MYSQLND_MS_MASTER_SWITCH, "SHOW STATUS LIKE 'Ssl_cipher'");
		$stmt = $pdo->query($query);
		$row = $stmt->fetch();
		var_dump($row);
		if (!$row['Value'] || !strlen($row['Value'])) {
			printf("[004] Expecting Ssl_cipher got nothing\n");
		}
		$query = sprintf("/*%s*/%s", MYSQLND_MS_SLAVE_SWITCH, "SHOW STATUS LIKE 'Ssl_cipher'");
		$stmt = $pdo->query($query);
		$row = $stmt->fetch();
		var_dump($row);
		if (!$row['Value'] || !strlen($row['Value'])) {
			printf("[005] Expecting Ssl_cipher got nothing\n");
		}
	} catch (Exception $e) {
		printf("[001] %s\n", $e->__toString());
	}


	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_pdo_basics_options.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_pdo_basics.ini'.\n");
?>
--EXPECTF--
array(2) {
  ["_myinit"]=>
  string(9) "something"
  [0]=>
  string(9) "something"
}
array(2) {
  ["_myinit"]=>
  string(9) "something"
  [0]=>
  string(9) "something"
}
array(4) {
  ["Variable_name"]=>
  string(10) "Ssl_cipher"
  [0]=>
  string(10) "Ssl_cipher"
  ["Value"]=>
  string(18) "DHE-RSA-AES256-SHA"
  [1]=>
  string(18) "DHE-RSA-AES256-SHA"
}
array(4) {
  ["Variable_name"]=>
  string(10) "Ssl_cipher"
  [0]=>
  string(10) "Ssl_cipher"
  ["Value"]=>
  string(18) "DHE-RSA-AES256-SHA"
  [1]=>
  string(18) "DHE-RSA-AES256-SHA"
}
done!