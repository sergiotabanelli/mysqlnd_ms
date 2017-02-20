--TEST--
phpinfo() section
--SKIPIF--
<?php
require_once('skipif.inc');
?>
--INI--
mysqlnd_ms.enable=1
--FILE--
<?php
	ob_start();
	phpinfo(INFO_MODULES);
	$tmp = ob_get_contents();
	ob_end_clean();

	if (!stristr($tmp, 'mysqlnd_ms support'))
		printf("[001] mysqlnd_ms section seems to be missing. Check manually\n");

	if (!stristr($tmp, 'Mysqlnd master/slave plugin version'))
		printf("[002] mysqlnd_ms version seems to be missing. Check manually\n");

	print "done!";
?>
--EXPECTF--
done!