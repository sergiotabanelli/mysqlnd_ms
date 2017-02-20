--TEST--
XA state store: failed commit
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
if ($error = mst_create_config("test_mysqlnd_ms_xa_mysql_errors_commit.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_xa_mysql_errors_commit.ini
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

	mst_stats_diff(1, mysqlnd_ms_get_stats());
	$xa_id = mt_rand(0, 1000);

	if ($error = mst_mysqli_flush_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)) {
		printf("[002] %s\n", $error);
	}

	if (!($link_store = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket)))
		printf("[003] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (!($link_kill = mst_mysqli_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket)))
		printf("[004] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	/* sequence to monitor */

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[005] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	if (true !== mysqlnd_ms_xa_begin($link, $xa_id)) {
		printf("[005] [%d] %s\n", $link->errno, $link->error);
	}
	mst_stats_diff(6, mysqlnd_ms_get_stats());

	/*
	global table should have an entry for xa id but there shall be no participants
	*/
	$res = mst_mysqli_query(7, $link_store,
		sprintf("SELECT COUNT(*) AS _num, state, intend, modified, started, timeout FROM mysqlnd_ms_xa_trx WHERE gtrid = '%d'",
			$xa_id));
	$global_trx = $res->fetch_assoc();
	if ($global_trx['_num'] != 1) {
		printf("[008] Expecting one entry, found %d for gtrid = %d. Test update required?",
			$global_trx['_num'], $xa_id);
	}

	if (($global_trx['state'] != 'XA_NON_EXISTING') || ($global_trx['state'] != $global_trx['intend'])) {
		printf("[009] Expecting state and intend = 'XA_NON_EXISTING', got state = '%s', intend = '%s'\n",
			$global_trx['state'], $global_trx['intend']);
	}
	if (($global_trx['started'] == '') || ($global_trx['started'] != $global_trx['modified'])) {
		printf("[010] Started and modified times should be eq, started = '%s', modified = '%s'\n",
			$global_trx['started'], $global_trx['modified']);
	}

	date_default_timezone_set("UTC");
	if (!strtotime($global_trx['started']) ||
		!strtotime($global_trx['timeout']) ||
		((strtotime($global_trx['timeout']) - strtotime($global_trx['started'])) != 60)) {
		printf("[011] Default timeout changed? timeout = %d\n",
			$global_trx['timeout']);
	}

	$participants = get_participants(12, $link_store, 0);
	if (!empty($participants)) {
		printf("[012] Dumping unexpected list of participants\n");
		var_dump($participants);
	}

	/* slave: XA BEGIN -> kill */
	mst_mysqli_query(13, $link, "SELECT 1");
	$thread_id = $link->thread_id;
	mst_stats_diff(14, mysqlnd_ms_get_stats());


	/* Global table shall be eq... */
	$res = mst_mysqli_query(15, $link_store,
		sprintf("SELECT COUNT(*) AS _num, state, intend, modified, started, timeout FROM mysqlnd_ms_xa_trx WHERE gtrid = '%d'",
			$xa_id));

	$global_trx_new = $res->fetch_assoc();
	if ($global_trx != $global_trx_new) {
		printf("[016] Unexpected global table changes, dumping\n");
		var_dump($global_trx);
		var_dump($global_trx_new);
	}

	/* participants table shall be updated */
	$participants = get_participants(17, $link_store, 1);
	$first_participant = $participants[0];

	mst_stats_diff(20, mysqlnd_ms_get_stats());

	/* master XA BEGIN ... XA ROLLBACK */
	mst_mysqli_query(21, $link, "SET @myrole='master'");
	mst_stats_diff(22, mysqlnd_ms_get_stats());

	$participants = get_participants(23, $link_store, 2);
	if ($participants[0] != $first_participant) {
		printf("[024] State of first participant should not have changed, dumping\n");
		var_dump($first_participant);
		var_dump($participants[0]);
	}
	$second_participant = $participants[1];

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
	compare_participants(27, $fields, $first_participant, $second_participant);

	if (!$link_kill->kill($thread_id)) {
		printf("[028] [%d] %s\n", $link_kill->errno, $link_kill->error);
	}

	/* slave: killed */
	@mst_mysqli_query(29, $link, "SELECT 2");
	/* XA logic will see the same error, at least it should, ...*/
	$client_errno = $link->errno;
	$client_error = $link->error;
	mst_stats_diff(30, mysqlnd_ms_get_stats());

	/* participants table shall not yet be updated */
	$participants = get_participants(31, $link_store, 2);
	if ($participants[0] != $first_participant) {
		printf("[032] State of first participant should not have changed, dumping\n");
		var_dump($first_participant);
		var_dump($participants[0]);
	}
	$first_participant = $participants[0];

	compare_participants(34, $fields, $first_participant, $second_participant);

	var_dump(mysqlnd_ms_xa_commit($link, $xa_id));
	mst_stats_diff(35, mysqlnd_ms_get_stats());

	/* participants table shall now be updated, first participant error must be recorded */
	$participants = get_participants(36, $link_store, 2);
	$first_participant = $participants[0];

	$fields['client_errno'] = 'neq';
	$fields['client_error'] = 'neq';
	$fields['health'] = 'neq';

	if (($first_participant['client_errno'] != $client_errno) ||
		($first_participant['client_error'] != $client_error) ||
		($first_participant['health'] != 'CLIENT ERROR')) {
		printf("[037] Expecting CLIENT ERROR/%d/%s got %s/%d/%s\n",
			$client_errno,
			$client_error,
			$first_participant['health'],
			$first_participant['client_errno'],
			$first_participant['client_error']
		);
	}
	compare_participants(38, $fields, $first_participant, $second_participant);

	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	if (!unlink("test_mysqlnd_ms_xa_mysql_errors_commit.ini")) {
		printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_xa_mysql_errors_commit.ini'.\n");
	}

	if (($error = mst_mysqli_drop_xa_tables($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket))) {
		printf("[clean] %s\n", $error);
	}
?>
--EXPECTF--
[006] xa_begin: 0 -> 1
[006] pool_masters_total: 0 -> 1
[006] pool_slaves_total: 0 -> 1
[006] pool_masters_active: 0 -> 1
[006] pool_slaves_active: 0 -> 1
[012] No participants recorded!
[014] use_slave: 0 -> 1
[014] use_slave_guess: 0 -> 1
[014] lazy_connections_slave_success: 0 -> 1
[014] xa_participants: 0 -> 1
[022] use_master: 0 -> 1
[022] use_master_guess: 0 -> 1
[022] lazy_connections_master_success: 0 -> 1
[022] xa_participants: 1 -> 2
[029] [%d] %s
[030] use_slave: 1 -> 2
[030] use_slave_guess: 1 -> 2

Warning: mysqlnd_ms_xa_commit(): (mysqlnd_ms) Failed to switch participant to XA_IDLE state: %s in %s on line %d
bool(false)
[035] xa_commit_failure: 0 -> 1
done!