<?php
/*
Multi master demo

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

printf("Running some statements in a roundrobin fashion\n");

run_query($conn, "SELECT 'Query 1, round robin: master1'");
run_query($conn, "SELECT 'Query 2, round robin: master2'");
run_query($conn, "SELECT 'Query 3, round robin: master1'");

printf("Dumping list of connections. Should be two. Queries distributed in a roundrobin fashion.\n");
foreach ($queries as $thread_id => $details) {
  printf("\t... Connection %d has run\n", $thread_id);
  foreach ($details as $query)
	printf("\t\t... %s\n", $query);
}

printf("\n");
?>