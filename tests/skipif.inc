<?php
require_once(__DIR__."/connect.inc");

function _skipif_check_extensions($extensions) {
	if (!is_array($extensions))
		$extensions = array($extensions);

	foreach ($extensions as $k => $extension) {
		if (!extension_loaded($extension))
			die(sprintf("SKIP ext/%s not available", $extension));
	}

}

function _skipif_connect($host, $user, $passwd, $db, $port, $socket) {
	if (!$link = mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket))
		die(sprintf("SKIP (h=%s,u=%s,p=%s,db=%s,P=%s,S=%s), [%d] %s\n",
			$host, $user, ($passwd) ? "***" : '', $db, $port, $socket, mysqli_connect_errno(), mysqli_connect_error()));
}

function _skipif_can_connect($host, $user, $passwd, $db, $port, $socket, $hint = NULL) {
	if ($link = mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket))
		die(sprintf("SKIP Can connect(hint: %s, h=%s,u=%s,p=%s,db=%s,P=%s,S=%s), [%d] %s\n",
			$hint, $host, $user, ($passwd) ? "***" : '', $db, $port, $socket,
			mysqli_connect_errno(), mysqli_connect_error()));
}

function _skipif_have_feature($feature) {
	$ret = false;

	switch ($feature) {
		case "table_filter":
			$ret = defined("MYSQLND_MS_HAVE_FILTER_TABLE_PARTITION");
			break;
		case "parser":
			$ret = true;
			break;
		default:
			$ret = false;
			break;
	}
	return $ret;
}

function _skipif_check_feature($features) {
	if (!is_array($features))
		$features = array($features);

	foreach ($features as $k => $feature) {
		if (!_skipif_have_feature($feature))
			die(sprintf("SKIP Feature '%s' not available", $feature));
	}
}

_skipif_check_extensions("mysqlnd_ms");
?>
