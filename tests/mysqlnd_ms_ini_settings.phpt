--TEST--
PHP configuration settings
--SKIPIF--
<?php
require_once('skipif.inc');
?>
--FILE--
<?php
	$expected = array(
		"mysqlnd_ms.enable" 			=> "mysqlnd_ms.enable",
		"mysqlnd_ms.force_config_usage" => "mysqlnd_ms.force_config_usage",
		"mysqlnd_ms.config_file" 		=> "mysqlnd_ms.config_file",
		"mysqlnd_ms.collect_statistics"	=> "mysqlnd_ms.collect_statistics",
		"mysqlnd_ms.multi_master"		=> "mysqlnd_ms.multi_master",
		"mysqlnd_ms.disable_rw_split"	=> "mysqlnd_ms.disable_rw_split",
	);

	$inigetall_list = ini_get_all("mysqlnd_ms", true);
	foreach ($expected as $ini_name) {
		if (!isset($inigetall_list[$ini_name])) {
			printf("[001] %s not found.\n", $ini_name);
		} else {
			$access = $inigetall_list[$ini_name]['access'];
			switch ($access) {
				case 1:
					$access = 'PHP_INI_USER';
					break;
				case 4:
					$access = 'PHP_INI_SYSTEM';
					break;
				case 2:
				case 6:
					$access = 'PHP_INI_PERDIR';
					break;
				case 7:
					$access = 'PHP_INI_ALL';
					break;
				default:
					break;
			}
			printf("%s [%s] '%s'\n", $ini_name, $access, $inigetall_list[$ini_name]['local_value']);
			unset($inigetall_list[$ini_name]);
		}
	}

	if (!empty($inigetall_list)) {
		printf("[002] Dumping list of unexpected ini settings\n");
		var_dump($inigetall_list);
	}
	print "done!";
?>
--EXPECTF--
mysqlnd_ms.enable [PHP_INI_SYSTEM] '0'
mysqlnd_ms.force_config_usage [PHP_INI_SYSTEM] '0'
mysqlnd_ms.config_file [PHP_INI_SYSTEM] ''
mysqlnd_ms.collect_statistics [PHP_INI_SYSTEM] '0'
mysqlnd_ms.multi_master [PHP_INI_SYSTEM] '0'
mysqlnd_ms.disable_rw_split [PHP_INI_SYSTEM] '0'
done!
