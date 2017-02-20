<?php
/*
Simple sharding/partitioning demo

See README for hints and instructions!
*/
function run_query($conn, $query) {
	global $queries;

	/* PECL/mysqlnd_ms will either choose the master or one of the slaves */
	$ret = $conn->query($query);

	/* After the query has been send, $conn either points to master or slave.
	By recording the connection thread id we can reverse-engineer which
	queries have been send via which connection. One cannot tell based on
	the thread id if a connection is a master or slave connection but
	common sense will tell you if you look at the recorded data... */
	if (!isset($queries[$conn->thread_id]))
		$queries[$conn->thread_id] = array($query);
	else
		$queries[$conn->thread_id][] = $query;

	if (!$ret)
		/* KLUDGE - you will do proper error handling, won't you? */
		die(sprintf("[%d] %s\n", $conn->errno, $conn->error));

	return $ret;
}

require_once("./config.php");

printf("\n");

if (!($conn = new mysqli("myapp", DB_USER, DB_PASSWORD, DB_SCHEMA)) || mysqli_connect_errno()) {
	die(sprintf("Please check the *config.json used and config.php, failed to connect: [%d] %s\n",
		mysqli_connect_errno(), mysqli_connect_error()));
}

$queries = array();

printf("Creating a test table. Statements should be send to the master...\n");
run_query($conn, "DROP TABLE IF EXISTS test");
run_query($conn, "CREATE TABLE test(id INT)");
run_query($conn, "INSERT INTO test(id) VALUES (1)");

printf("Dumping list of connections. Should be only one, the master connection...\n");
foreach ($queries as $thread_id => $details) {
  printf("\t... Connection %d has run\n", $thread_id);
  foreach ($details as $query)
	printf("\t\t... %s\n", $query);
}

printf("Running a SELECT, it should be send to the first slave...\n");
run_query($conn, "SELECT 'slave1' FROM DUAL");

printf("Running a SELECT, it should be send to the second slave because of round robin...\n");
run_query($conn, "SELECT 'slave2' FROM DUAL");

printf("Dumping list of connections. Should be three: master and both slaves, one SELECT on each slave\n");
foreach ($queries as $thread_id => $details) {
  printf("\t... Connection %d has run\n", $thread_id);
  foreach ($details as $query)
	printf("\t\t... %s\n", $query);
}

printf("\n");
printf("Running a SELECT, it should be send the slaves configured for partition A only. This is the first slave only...\n");
run_query($conn, "/*Partition_A*/SELECT 'slave1' FROM DUAL");

printf("Running a SELECT, it should be send the slaves configured for partition A only. This is the first slave only...\n");
run_query($conn, "/*Partition_A*/SELECT 'slave1' FROM DUAL");

printf("Dumping list of connections. Should be three: master and both slaves, three queries for slave one, one query for slave two\n");
foreach ($queries as $thread_id => $details) {
  printf("\t... Connection %d has run\n", $thread_id);
  foreach ($details as $query)
	printf("\t\t... %s\n", $query);
}

printf("
If everything has gone right, you should see how the queries prefixed with the SQL hint /*Partition_A*/
have been executed on the only slave that can handle this partition.
This can be used for partitioning, including manual sharding but also to work around hotspots.\n");

printf("\n");
?>