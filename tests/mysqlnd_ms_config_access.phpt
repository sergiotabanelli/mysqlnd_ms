--TEST--
Concurrent config file access
--SKIPIF--
<?php
require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));

if (!function_exists('pcntl_fork'))
	die("skip Process Control Functions not available");

if (!function_exists('posix_getpid'))
	die("skip POSIX functions not available");

$settings = array(
	"myapp" => array(
		'master' => array($master_host),
		'slave' => array($slave_host),
	),
);
if ($error = mst_create_config("test_mysqlnd_ms_config_access.ini", $settings))
	die(sprintf("SKIP %s\n", $error));

_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_config_access.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");

	/* something easy to start with... */

	if (!($link = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket)))
		printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());

	/* This shall work because we have two slaves */
	$pid = pcntl_fork();
	switch ($pid) {
		case -1:
			printf("[002] Cannot fork child");
			break;

		case 0:
			/* child */
			for ($i = 0; $i < 1; $i++) {
				if (!($clink = mst_mysqli_connect("myapp", $user, $passwd, $db, $port, $socket))) {
					printf("[001] [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
					continue;
				}

				$res = mysqli_query($clink, "SELECT 'child' AS _message");
				if (!$res)
					printf("[003] Child cannot store results, [%d] %s\n", $clink->errno, $clink->error);
				else {
					if (!$row = $res->fetch_assoc())
						printf("[004] Child cannot fetch results\n");
					if ($row['_message'] != 'child')
						printf("[005] Expecting 'child' got '%s'\n", $row['_message']);
					$res->free();
				}
				$clink->close();
			}
			exit(0);
			break;

		default:
			/* parent */
			$status = null;
			$wait_id = pcntl_waitpid($pid, $status);
			if (pcntl_wifexited($status) && (0 != ($tmp = pcntl_wexitstatus($status)))) {
				printf("Exit code: %s\n", (pcntl_wifexited($status)) ? pcntl_wexitstatus($status) : 'n/a');
				printf("Signal: %s\n", (pcntl_wifsignaled($status)) ? pcntl_wtermsig($status) : 'n/a');
				printf("Stopped: %d\n", (pcntl_wifstopped($status)) ? pcntl_wstopsig($status) : 'n/a');
			}
			break;
	}

	print "done!";
?>
--CLEAN--
<?php
	if (!unlink("test_mysqlnd_ms_config_access.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_config_access.ini'.\n");
?>
--EXPECTF--
done!