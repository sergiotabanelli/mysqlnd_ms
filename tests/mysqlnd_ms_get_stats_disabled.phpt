--TEST--
mysqlnd_ms_get_stats() and mysqlnd_ms disabled
--SKIPIF--
<?php
require_once('skipif.inc');
?>
--INI--
mysqlnd_ms.enable=0
mysqlnd_ms.collect_statistics=1
--FILE--
<?php
	if (NULL !== ($ret = mysqlnd_ms_get_stats()))
	  printf("[001] Expecting NULL got %s/%s\n", gettype($ret), $ret);

	print "done!";
?>
--EXPECTF--
done!