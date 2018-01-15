<?php
	/*
	For whatever reasons run-tests.php sets new environment variables
	and gives no acceess to original environment :(. If it would at least
	preserve and overwrite only selected ones ...
	*/
	$baseport = 20393;
	$mport = $baseport;
	$sport = $baseport+1;
	$emport = $baseport+2;
	$esport = $baseport+3;
	
	putenv("MYSQL_TEST_HOST=127.0.0.1");
	putenv("MYSQL_TEST_PORT=$emport");
	putenv("MYSQL_TEST_USER=root");
	putenv("MYSQL_TEST_PASSWD=msandbox");
	putenv("MYSQL_TEST_DB=test");
	putenv("MYSQL_TEST_ENGINE=InnoDB");
	putenv("MYSQL_TEST_MEMCACHED_PORT_ADD_HACK=7905");
	
	putenv("MYSQL_TEST_SKIP_CONNECT_FAILURE=1");
	putenv("MYSQL_TEST_CONNECT_FLAGS=0");
	putenv("MYSQL_TEST_EXPERIMENTAL=0");

	/* Whether to run Fabric related tests */
	putenv("MYSQL_TEST_FABRIC=0");
	/* MySQL Fabric */
	putenv("MYSQL_TEST_FABRIC_HOST=127.0.0.1");
	putenv("MYSQL_TEST_FABRIC_PORT=8080");
	/* MySQL Fabric emulator bind address */
	putenv("MYSQL_TEST_FABRIC_EMULATOR_HOST=127.0.0.1");
	putenv("MYSQL_TEST_FABRIC_EMULATOR_PORT=8082");
	/* This PHP must support the xmlrpc extension */
	putenv("MYSQL_TEST_FABRIC_EMULATOR_PHP_EXECUTABLE=/usr/local/bin/php");
	/* Path calculations never go smooth, thus set ABSOLUTE path to server script */
	putenv("MYSQL_TEST_FABRIC_EMULATOR_SERVER_SCRIPT=tests/util_fabric_server.inc");
	/* If set, enabled emulated fabric server log */
	putenv("MYSQL_TEST_FABRIC_EMULATOR_LOG=/tmp/fabric_emulator.log");

    /*
	Tests will automatically detect server roles and will be skipped if required.

	For running all tests you need all four different servers. For running
	most tests you just need the two emulated once. Some tests require only one
	server, this emulated defaults to MYSQL_TEST_HOST. If that's not enough, the
	test is skipped automatically.
	*/

	/* emulated master - standalone MySQL, not part of a replication cluster */
	putenv("MYSQL_TEST_EMULATED_MASTER_HOST=127.0.0.1:$emport");
	/* emulated slave - standalone MySQL, not part of a replication cluster */
	putenv("MYSQL_TEST_EMULATED_SLAVE_HOST=127.0.0.1:$esport");

	/*
	Some tests require real replication cluster.
	Tests will check if the condfigured servers are part of a cluster.
	They default to MYSQL_TEST_EMULATED*, because some old tests may
	still use them, although they only need the emulated ones and should use them.

	Yeah, I shall fix that...
	*/

	/* master - master of a replication cluster */
	putenv("MYSQL_TEST_MASTER_HOST=127.0.0.1:$mport");
	/* slave - slave of the replication cluster using the above master */
	putenv("MYSQL_TEST_SLAVE_HOST=127.0.0.1:$sport");
?>