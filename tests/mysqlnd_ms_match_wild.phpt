--TEST--
mysqlnd_ms_match_wild(string table_name, string wildcard)
--SKIPIF--
<?php
require_once('skipif.inc');
?>
--INI--
mysqlnd_ms.enable=1
--FILE--
<?php

	if (!is_null($ret = @mysqlnd_ms_match_wild()))
		printf("[001] Expecting NULL got %s\n", var_export($ret, true));

	if (!is_null($ret = @mysqlnd_ms_match_wild("test")))
	  printf("[002] Expecting NULL got %s\n", var_export($ret, true));

	$pattern = array(
		array('', '', true),
		array('\0', '\0', false),
		array('_', '_', true),
		array('%', '%', true),
		array('', '', true),
		array('tablename', 'tablename', true),
		/* % = zero or more */
		array('tablename', 'tablenam%', true),
		array('tablename', '%', true),
		array('tablename', '%e', true),
		array('tablename', 't%e', true),
		array('tablename', 't%%e', true),
		array('tablename', 'tablename%', true),
		array('tablename', 'tablenam%', true),
		array('', '%', true),
		/* _ = one */
		array('', '_', false),
		array('tablename', 'tablenam_', true),
		array('tablename', '_ablename', true),
		array('tablename', 't_blename', true),
		array('tablename', 't___ename', true),
		/* escaping */
		array('\\', '\\', false),
		array('\\a', '\\b', false),
		array('tabl_name', 'tabl\_nam_', true),
		array('tabl_name', 'tabl\_name', true),
		array('tabl%name', 'tabl\%na%', true),
		array('tabl%name', 'tabl\%name', true),
	);

	foreach ($pattern as $details) {
		if (($ret = mysqlnd_ms_match_wild($details[0], $details[1])) !== $details[2]) {
			printf("[003] Expecting %s got %s when checking '%s' against pattern '%s'.\n",
			  ($details[2]) ? 'true' : 'false', ($ret) ? 'true' : 'false',
			  $details[0], $details[1]);
		}
	}

	print "done!";

?>
--EXPECTF--
done!