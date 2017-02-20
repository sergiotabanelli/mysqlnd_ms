--TEST--
GC config options
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");
require_once("util.inc");

if (($emulated_master_host == $emulated_slave_host)) {
	die("SKIP Emulated master and emulated slave seem to the the same, see tests/README");
}

_skipif_check_extensions(array("mysqli"));
_skipif_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

if (($error = mst_mysqli_setup_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)) ||
	($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
	die(sprintf("SKIP %s\n", $error));
}

$settings = array(
	"max_retries_type" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"max_retries" => array(1)
			),
		),
	),
	"max_retries_min" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"max_retries" => -1,
			),
		),
	),
	"max_retries_max" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"max_retries" => 101,
			),
		),
	),
	"max_retries_overflow" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"max_retries" => PHP_INT_MAX + 1,
			),
		),
	),
	"max_retries_valid" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"max_retries" => 0,
			),
		),
	),

	"probability_type" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"probability" => array(1)
			),
		),
	),
	"probability_min" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"probability" => -1,
			),
		),
	),
	"probability_max" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"probability" => 1001,
			),
		),
	),
	"probability_overflow" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"probability" => PHP_INT_MAX + 1,
			),
		),
	),

	"probability_valid" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"probability" => 0,
			),
		),
	),


	"per_run_type" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"max_transactions_per_run" => array(1)
			),
		),
	),
	"per_run_min" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"max_transactions_per_run" => 0,
			),
		),
	),
	"per_run_max" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"max_transactions_per_run" => 32769,
			),
		),
	),
	"per_run_overflow" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"max_transactions_per_run" => PHP_INT_MAX + 1,
			),
		),
	),

	"per_run_valid" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"max_transactions_per_run" => 1,
			),
		),
	),

);

foreach ($settings as $host => $setting) {
	$setting['xa'] = array_merge($setting['xa'], array('state_store' =>
			array(
				'participant_localhost_ip' => '127.0.0.1',
				'mysql' =>
					array(
						'host' => $emulated_master_host_only,
						'user' => $user,
						'password' => $passwd,
						'db'   => $db,
						'port' => $emulated_master_port,
						'socket' => $emulated_master_socket,
					)
			)
		));
	$settings[$host] = $setting;
}
$settings['no_store'] = array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			"garbage_collection" => array(
				"max_transactions_per_run" => 1,
			),
		),
	);
if ($error = mst_create_config("test_mysqlnd_ms_xa_gc_config.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_gc_config.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	set_error_handler('mst_error_handler');


	if (!($link = mst_mysqli_connect("max_retries_type", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("max_retries_min", $user, $passwd, $db, $port, $socket)))
		printf("[002] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("max_retries_max", $user, $passwd, $db, $port, $socket)))
		printf("[003] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("max_retries_overflow", $user, $passwd, $db, $port, $socket)))
		printf("[004] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("max_retries_valid", $user, $passwd, $db, $port, $socket)))
		printf("[005] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("probability_type", $user, $passwd, $db, $port, $socket)))
		printf("[006] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("probability_min", $user, $passwd, $db, $port, $socket)))
		printf("[007] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("probability_max", $user, $passwd, $db, $port, $socket)))
		printf("[008] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("probability_overflow", $user, $passwd, $db, $port, $socket)))
		printf("[009] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("probability_valid", $user, $passwd, $db, $port, $socket)))
		printf("[010] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("per_run_type", $user, $passwd, $db, $port, $socket)))
		printf("[011] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("per_run_min", $user, $passwd, $db, $port, $socket)))
		printf("[012] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("per_run_max", $user, $passwd, $db, $port, $socket)))
		printf("[013] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("per_run_overflow", $user, $passwd, $db, $port, $socket)))
		printf("[014] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("per_run_valid", $user, $passwd, $db, $port, $socket)))
		printf("[015] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("no_store", $user, $passwd, $db, $port, $socket)))
		printf("[016] [%d] '%s'\n", mysqli_connect_errno(), mysqli_connect_error());

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!unlink("test_mysqlnd_ms_xa_gc_config.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_gc_config.ini'.\n");

	if (($error = mst_mysqli_drop_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
		printf("[clean] %s\n", $error);
	}
?>
--EXPECTF--
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) 'max_retries' from 'garbage_collection' must be a number (0...100) in %s on line %d
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) 'max_retries' from 'garbage_collection' must be a number between 0 and 100 in %s on line %d
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) 'max_retries' from 'garbage_collection' must be a number between 0 and 100 in %s on line %d
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) 'probability' from 'garbage_collection' must be a number (0...1000) in %s on line %d
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) 'probability' from 'garbage_collection' must be a number between 0 and 1000 in %s on line %d
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) 'probability' from 'garbage_collection' must be a number between 0 and 1000 in %s on line %d
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) 'max_transactions_per_run' from 'garbage_collection' must be a number (1...32768) in %s on line %d
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) 'max_transactions_per_run' from 'garbage_collection' must be a number between 1 and 32768 in %s on line %d
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) 'max_transactions_per_run' from 'garbage_collection' must be a number between 1 and 32768 in %s on line %d
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) 'max_transactions_per_run' from 'garbage_collection' must be a number between 1 and 32768 in %s on line %d
[E_RECOVERABLE_ERROR] mysqli_real_connect(): (mysqlnd_ms) Garbage collection is unavailable. Either no state store was configured or setting up a state store failed. All settings from 'garbage_collection' will be ignored in %s on line %d
done!