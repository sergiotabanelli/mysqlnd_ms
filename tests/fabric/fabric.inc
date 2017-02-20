<?php

const RANGE = 0;
const HASH = 1;

function test_convert_fabric_dump(array $a) {
	$retval = '';

	$retval .= pack("i", count($a['shard_table']));
	foreach ($a['shard_table'] as $t) {
		$retval .= pack("ia65a65a65x", $t[0], $t[1], $t[2], $t[3]);
	}
	$retval .= pack("i", count($a['shard_mapping']));
	foreach ($a['shard_mapping'] as $m) {
		$retval .= pack("iia65x3", $m[0], $m[1], $m[2]);
	}

	$retval .= pack("i", count($a['shard_index']));
	foreach ($a['shard_index'] as $i) {
		$retval .= pack("iiia65x3", $i[0], $i[1], $i[2], $i[3]);
	}

	$retval .= pack("i", count($a['server']));
	foreach ($a['server'] as $s) {
		$retval .= pack("ia41x3ia65x3ia65x3Iiid",
			strlen($s[0]), $s[0],
			strlen($s[1]), $s[1],
			strlen($s[2]), $s[2],
			$s[3], $s[4], $s[5], $s[6]
		);
	}
	return $retval;
}

