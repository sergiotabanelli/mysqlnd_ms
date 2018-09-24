--TEST--
Issue 9 zts performance drop analysis
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.3.99-dev', '<'))
	die(sprintf("SKIP Requires PHP >= 5.3.99, using " . PHP_VERSION));

require_once('skipif.inc');
require_once("connect.inc");

_skipif_check_extensions(array("mysqli"));
_skipif_check_extensions(array("pthreads"));
_skipif_connect($master_host_only, $user, $passwd, $db, $master_port, $master_socket);
_skipif_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
_skipif_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);

include_once("util.inc");

$ret = mst_is_slave_of($slave_host_only, $slave_port, $slave_socket, $master_host_only, $master_port, $master_socket, $user, $passwd, $db);
if (is_string($ret))
	die(sprintf("SKIP Failed to check relation of configured master and slave, %s\n", $ret));

if (true != $ret)
	die("SKIP Configured master and slave seem not to be part of a replication cluster\n");

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
			"slave2" => array(
				'host' 	=> $emulated_slave_host_only,
				'port' 	=> (int)$emulated_slave_port,
				'socket' => $emulated_slave_socket,
			),
		),
		'filters' => array(
        	"random" => array(
        		"sticky" => "1"
			)
		),
		"failover" => array (
        	"strategy" => "loop_before_master",
        	"remember_failed" => true,
        	"max_retries" => 0
		)
	)
);

if ($error = mst_create_config("test_mysqlnd_ms_zts_issue_9.ini", $settings))
	die(sprintf("SKIP %s\n", $error));
if ($error = mst_mysqli_drop_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
	die(sprintf("SKIP Failed to drop test table on master %s\n", $error));
if ($error = mst_mysqli_drop_test_table($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket))
	die(sprintf("SKIP Failed to drop test table on emulated slave %s\n", $error));
if ($error = mst_mysqli_create_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
	die(sprintf("SKIP Failed to create test table on master %s\n", $error));
if ($error = mst_mysqli_create_test_table($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket))
	die(sprintf("SKIP Failed to create test table on emulated slave %s\n", $error));
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=test_mysqlnd_ms_zts_issue_9.ini
--FILE--
<?php
	require_once("connect.inc");
	require_once("util.inc");
	function mini_bench_to($arg_t, $arg_ra=false) 
	{
		$tttime=round((end($arg_t)-$arg_t['start'])*1000,4);
		if ($arg_ra) $ar_aff['total_time']=$tttime;
		else $aff="total time : ".$tttime."ms\n";
		$prv_cle='start';
		$prv_val=$arg_t['start'];

		foreach ($arg_t as $cle=>$val)
		{
			if($cle!='start')    
			{
				$prcnt_t=round(((round(($val-$prv_val)*1000,4)/$tttime)*100),1);
				if ($arg_ra) $ar_aff[$prv_cle.' -> '.$cle]=$prcnt_t;
				else $aff.=$prv_cle.' -> '.$cle.' : '.$prcnt_t." %\n";
				$prv_val=$val;
				$prv_cle=$cle;
			}
		}
		if ($arg_ra) return $ar_aff;
		return $aff;
	}

	class Perftest extends Thread
	{
		public $link;
		public $runfor;

		public function __construct($link, $runfor) {
			$this->link = $link;
			$this->runfor = $runfor;
		}

		public function run()
		{
			$i = 0;
			$t = time();
			while(time() < $t + $this->runfor)
			{
				$i++;
				$mysqli = mst_mysqli_connect($this->link['myapp'], $this->link['user'], $this->link['passwd'], $this->link['db'], $this->link['port'], $this->link['socket']);
				if (mysqli_connect_errno()) {
					printf("On connection [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
					break;
				}
				$mysqli->query("/* $i */SELECT * FROM test");
				if (mysqli_connect_errno()) {
					printf("On query [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
					break;
				}
				$mysqli->close();
			}
		}
	}
	$iterations = 50; // Threads number
	$wait = 6; // This is the maximum running time for the mysql stop command, needed to be sure that the 
				// Com_select qps will include $sleep seconds of queries  
	$sleep = 10; // Qps measurement interval
	$tbuf = 3; // Run threads for $tbuf seconds more
	$al['myapp'] = 'myapp';
	$al['user'] = $user;
	$al['passwd'] = $passwd;
	$al['db'] = $db;
	$al['port'] = $port;
	$al['socket'] = $socket;
	for ($i=0;$i<$iterations;$i++)
	{
		$workers[$i] = new Perftest($al, 2*$sleep + $wait + $tbuf); // 3 seconds buffer
		$workers[$i]->start();
	}
	$link = mst_mysqli_connect($slave_host_only, $user, $passwd, $db, $slave_port, $slave_socket);
	$link1 = mst_mysqli_connect($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket);
	$result= $link->query("show global status like 'Com_select'");
	$start = (int)($result->fetch_row()[1]);
	$result= $link1->query("show global status like 'Com_select'");
	$start1 = (int)($result->fetch_row()[1]);
	sleep($sleep);
	$result= $link->query("show global status like 'Com_select'");
	$end = (int)($result->fetch_row()[1]);
	$result= $link1->query("show global status like 'Com_select'");
	$end1 = (int)($result->fetch_row()[1]);
	$qpsnf = ($end-$start)/$sleep;
	$qpsnf1 = ($end1-$start1)/$sleep;
	echo "select qps nofail = ". $qpsnf . " $end - $start\n";
	echo "select qps1 nofail = ". $qpsnf1 . " $end1 - $start1\n";
	$wtil = time() + $wait;
	exec($stop_eslave);
	while(time() < $wtil + 1) {
		sleep(1);
	}
	$result= $link->query("show global status like 'Com_select'");
	$start = (int)($result->fetch_row()[1]);
	sleep($sleep);
	$result= $link->query("show global status like 'Com_select'");
	$end = (int)($result->fetch_row()[1]);
	$qpsf = ($end-$start)/$sleep;
	echo "select qps fail = ". $qpsf . " $end - $start\n";
	for ($i=0;$i<$iterations;$i++)
	{
		$workers[$i]->join();
	}
	if ($qpsnf > $qpsf) {
		$drop = (($qpsnf - $qpsf)*100)/$qpsnf;
		if ($drop > 10) {
			echo "Performance drop: $drop %\n";
		} 
	}
	exec($start_eslave);
	print "done!";
?>
--CLEAN--
<?php
	require_once("connect.inc");
	require_once("util.inc");
	if (!unlink("test_mysqlnd_ms_zts_issue_9.ini"))
	  printf("[clean] Cannot unlink ini file 'test_mysqlnd_ms_issue_9.ini'.\n");
	if ($error = mst_mysqli_drop_test_table($master_host_only, $user, $passwd, $db, $master_port, $master_socket))
		printf("[clean] Failed to drop test table on master %s\n", $error);
	if ($error = mst_mysqli_drop_test_table($emulated_slave_host_only, $user, $passwd, $db, $emulated_slave_port, $emulated_slave_socket))
		printf("[clean] Failed to drop test table on emulated slave %s\n", $error);
?>
--EXPECTF--
done!
