--TEST--
Constants
--SKIPIF--
<?php
require_once('skipif.inc');
?>
--FILE--
<?php
	$expected = array(
		"MYSQLND_MS_VERSION" => true,
		"MYSQLND_MS_VERSION_ID" => true,

		/* SQL hints */
		"MYSQLND_MS_MASTER_SWITCH" => true,
		"MYSQLND_MS_SLAVE_SWITCH" => true,
		"MYSQLND_MS_LAST_USED_SWITCH" => true,

		/* Return values of mysqlnd_ms_is_select() */
		"MYSQLND_MS_QUERY_USE_LAST_USED" => true,
		"MYSQLND_MS_QUERY_USE_MASTER" => true,
		"MYSQLND_MS_QUERY_USE_SLAVE" => true,
	);

	if (defined("MYSQLND_MS_HAVE_FILTER_TABLE_PARTITION")) {
		$expected["MYSQLND_MS_HAVE_FILTER_TABLE_PARTITION"] = false;
	}

	if (defined("MYSQLND_MS_HAVE_CACHE_SUPPORT")) {
		$expected["MYSQLND_MS_HAVE_CACHE_SUPPORT"] = false;
	}

	if (version_compare(PHP_VERSION, '5.3.99', ">")) {
		$expected["MYSQLND_MS_QOS_CONSISTENCY_STRONG"] = false;
		$expected["MYSQLND_MS_QOS_CONSISTENCY_SESSION"] = false;
		$expected["MYSQLND_MS_QOS_CONSISTENCY_EVENTUAL"] = false;
		$expected["MYSQLND_MS_QOS_OPTION_GTID"] = false;
		$expected["MYSQLND_MS_QOS_OPTION_AGE"] = false;
		$expected["MYSQLND_MS_QOS_OPTION_CACHE"] = false;
	}

	$constants = get_defined_constants(true);
	$constants = (isset($constants['mysqlnd_ms'])) ? $constants['mysqlnd_ms'] : array();
	ksort($constants);
	foreach ($constants as $name => $value) {
		if (!isset($expected[$name])) {
			printf("[001] Unexpected constants: %s/%s\n", $name, $value);
		} else {
			if ($expected[$name])
				printf("%s = '%s'\n", $name, $value);
			unset($expected[$name]);
		}
	}
	if (!empty($expected)) {
		printf("[002] Dumping list of missing constants\n");
		var_dump($expected);
	}

	print "done!";
?>
--EXPECTF--
MYSQLND_MS_LAST_USED_SWITCH = 'ms=last_used'
MYSQLND_MS_MASTER_SWITCH = 'ms=master'
MYSQLND_MS_QUERY_USE_LAST_USED = '2'
MYSQLND_MS_QUERY_USE_MASTER = '0'
MYSQLND_MS_QUERY_USE_SLAVE = '1'
MYSQLND_MS_SLAVE_SWITCH = 'ms=slave'
MYSQLND_MS_VERSION = '1.6.0-alpha'
MYSQLND_MS_VERSION_ID = '10600'
done!