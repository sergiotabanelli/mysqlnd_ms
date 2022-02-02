--TEST--
mysqlnd_ms: Invalid ini file
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=a_file_by_this_name_better_not_exists.ini
--FILE--
<?php
echo "DONE";
?>
--EXPECTF--
Warning: Unknown: Failed to open stream: No such file or directory in %s

Warning: Unknown: (mysqlnd_ms) Failed to open server list config file %s
DONE