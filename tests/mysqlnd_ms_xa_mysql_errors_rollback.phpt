--TEST--
XA state store: failed rollback
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

if (($error = mst_mysqli_setup_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket,'mysqlnd_ms_xa_trx', 'mysqlnd_ms_xa_participants')) ||
	($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket, 'mysqlnd_ms_xa_trx', 'mysqlnd_ms_xa_participants'))) {
	die(sprintf("SKIP %s\n", $error));
}

$settings = array(
	"myapp" => array(
		'master' => array($emulated_master_host),
		'slave' => array($emulated_slave_host),
		'xa' => array(
			'state_store' => array(
				'participant_localhost_ip' => '127.0.0.1',
				'mysql' =>
				array(
					'host' => $emulated_master_host_only,
					'user' => $user,
					'password' => $passwd,
					'db'   => $db,
					'port' => $emulated_master_port,
					'socket' => $emulated_master_socket,
					'global_trx_table' => 'mysqlnd_ms_xa_trx',
					'participant_table' => 'mysqlnd_ms_xa_participants',
					'participant_localhost_ip' => 'pseudo_ip_for_localhost'
			))),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_xa_mysql_errors_rollback.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_mysql_errors_rollback.ini
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	function get_participants($offset, $link_store, $expected) {

		$res = mst_mysqli_query($offset, $link_store, sprintf("SELECT * FROM mysqlnd_ms_xa_participants ORDER BY bqual ASC"));
		$participants = $res->fetch_all(MYSQLI_ASSOC);
		if (!$participants) {
			printf("[%03d] No participants recorded!\n", $offset);
			return array();
		}

		if (count($participants) != $expected) {
			printf("[%03d] Expecting %d participant[s], got %d\n", $offset, $expected, count($participants));
		}

		return $participants;
	}

	function compare_participants($offset, $fields, $first_participant, $second_participant) {
		foreach ($first_participant as $k => $v) {
			if (isset($fields[$k])) {
				if ('eq' == $fields[$k]) {
					if ($first_participant[$k] != $second_participant[$k]) {
						printf("[%03d] Expecting identical values, field='%s', first='%s', second='%s'\n",
							$offset, $k, $v, $second_participant[$k]);
					}
				} else if ('neq' == $fields[$k]) {
					if ($first_participant[$k] == $second_participant[$k]) {
						printf("[%03d] Expecting different values, field='%s', first='%s', second='%s'\n",
							$offset, $k, $v, $second_participant[$k]);
					}
				} else if ('ignore' == $fields[$k]) {
					;
				} else {
					printf("[%03d] Unknown command, update/fix test\n", $offset);
				}
				unset($fields[$k]);
			} else {
				printf("Unknown field='%s', first='%s', second='%s'\n",
					$offset, $k,$v, $second_participant[$k]);
			}
		}
	}

	$xa_id = mt_rand(0, 1000);

	if ($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)) {
		printf("[001] %s\n", $error);
	}

	if (!($link_store = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)))
		printf("[002] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link_kill = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)))
		printf("[003] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[004] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (true !== mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[005] [%d] %s\n", $link->errno, $link->error);
	}

	mst_mysqli_query(6, $link, "SELECT 1");
	mst_mysqli_query(7, $link, "SET @myrole='master'");
	$thread_id = $link->thread_id;

	$fields = array(
		'fk_store_trx_id' => 'ignore',
		'bqual' => 'neq',
		'participant_id' => 'ignore',
		'server_uuid' => 'ignore' /* TODO */,
		'scheme' => 'neq',
		'host' => 'ignore',
		'port' => 'ignore',
		'socket' => 'ignore',
		'user' => 'eq',
		'password' => 'eq',
		'state' => 'eq',
		'health' => 'eq',
		'connection_id' => 'neq',
		'client_errno' => 'eq',
		'client_error' => 'eq',
		'modified' => 'ignore',
	);

	$participants = get_participants(8, $link_store, 2);
	$first_participant = $participants[0];
	$second_participant = $participants[1];
	compare_participants(9, $fields, $first_participant, $second_participant);

	if (!$link_kill->kill($thread_id)) {
		printf("[010] [%d] %s\n", $link_kill->errno, $link_kill->error);
	}

	/* master/second_participant: killed */
	@mst_mysqli_query(11, $link, "SELECT 1", MYSQLND_MS_MASTER_SWITCH);
	$client_errno = $link->errno;
	$client_error = $link->error;
	mst_stats_diff(12, mysqlnd_ms_get_stats());

	$participants = get_participants(13, $link_store, 2);
	if ($participants[1] != $second_participant) {
		printf("[014] State of second participant should not have changed, dumping\n");
		var_dump($second_participant);
		var_dump($participants[1]);
	}
	$second_participant = $participants[1];

	var_dump(mysqlnd_ms_xa_rollback($link, $xa_id));
	mst_stats_diff(15, mysqlnd_ms_get_stats());

	/* participants table shall now be updated, first participant error must be recorded */
	$participants = get_participants(16, $link_store, 2);
	$first_participant = $participants[0];
	$second_participant = $participants[1];

	if (($second_participant['client_errno'] != $client_errno) ||
		($second_participant['client_error'] != $client_error) ||
		($second_participant['health'] != 'CLIENT ERROR')) {
		printf("[017] Expecting CLIENT ERROR/%d/%s got %s/%d/%s\n",
			$client_errno,
			$client_error,
			$second_participant['health'],
			$second_participant['client_errno'],
			$second_participant['client_error']
		);
	}
	/* note: the plugin will try to clean up as many RMs as possible to release locks early */
	if (($first_participant['state'] != 'XA_ROLLBACK') ||
		($second_participant['state'] != 'XA_ACTIVE')) {
		printf("[018] Expecting first=XA_ROLLBACK, second=XA_ACTIVE, got first=%s, second=%s\n",
			$first_participant['state'],
			$second_participant['state']);
	}

	$fields['client_errno'] = 'neq';
	$fields['client_error'] = 'neq';
	$fields['health'] = 'neq';
	$fields['state'] = 'neq';

	compare_participants(19, $fields, $first_participant, $second_participant);

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!unlink("test_mysqlnd_ms_xa_mysql_errors_rollback.ini")) {
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_mysql_errors_rollback.ini'.\n");
	}

	if (($error = mst_mysqli_drop_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
		printf("[clean] %s\n", $error);
	}
?>
--EXPECTF--
[011] [%d] %s

Warning: mysqlnd_ms_xa_rollback(): (mysqlnd_ms) Failed to switch participant to XA_IDLE state: %s in %s on line %d
bool(false)
[015] xa_rollback_failure: 0 -> 1
done!