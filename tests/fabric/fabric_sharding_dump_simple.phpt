--TEST--
MySQL Fabric: Simple shard lookups (dump)
--SKIPIF--
<?php
require_once(__DIR__.'/../skipif.inc');
_skipif_check_extensions(array("mysqli"));

if (!function_exists("mysqlnd_ms_debug_set_fabric_raw_dump_data_dangerous")) {
	die("SKIP: Need debug build");
}

file_put_contents("fabric_sharding_dump_simple.json", <<<EOT
{
	"testfabric" : {
		"fabric":{
			"hosts": []
		}
	}
}
EOT
);
?>
--INI--
mysqlnd_ms.enable=1
mysqlnd_ms.config_file=fabric_sharding_dump_simple.json
--FILE--
<?php
require_once("fabric.inc");

$fabric_dump = array(
	'shard_table' => array(
		array(1, "test", "fabric", "id"),
		array(2, "foo", "bar", "id")
	),
	'shard_mapping' => array(
		array(1, RANGE, "global1" ),
		array(2, RANGE, "global2" ),
	),
	'shard_index' => array(
		array(1,     1, 1, "shard1_1"),
		array(1, 30000, 2, "shard1_2"),
		array(1, 40000, 2, "shard1_3"),
		array(2,     1, 3, "shard2_1"),
		array(2,  1000, 4, "shard2_2")
	),
	'server' => array(
		array("0000-0001", "global1",  "global.shard1",  3301, 3, 3, 1.0),
		array("0000-0002", "global2",  "global1.shard2", 3302, 3, 3, 1.0),
		array("0000-0003", "global2",  "global2.shard2", 3303, 1, 2, 1.0),
		array("0000-0004", "shard1_1", "host1.shard1",   3304, 3, 3, 1.0),
		array("0000-0005", "shard1_1", "host2.shard1",   3305, 1, 2, 1.0),
		array("0000-0006", "shard1_2", "host3.shard1",   3306, 3, 3, 1.0),
		array("0000-0007", "shard1_2", "host4.shard1",   3307, 1, 2, 1.0),
		array("0000-0008", "shard1_3", "host5.shard1",   3308, 3, 3, 1.0),
		array("0000-0009", "shard1_3", "host6.shard1",   3309, 1, 2, 1.0),
		array("0000-0010", "shard2_1", "host1.shard2",   3310, 3, 3, 1.0),
		array("0000-0011", "shard2_1", "host2.shard2",   3311, 1, 2, 1.0),
		array("0000-0012", "shard2_2", "host3.shard2",   3312, 3, 3, 1.0),
		array("0000-0013", "shard2_2", "host4.shard2",   3313, 1, 2, 1.0)
	)
);
$data = test_convert_fabric_dump($fabric_dump);

$c = new mysqli("testfabric", "root", "");
mysqlnd_ms_debug_set_fabric_raw_dump_data_dangerous($c, $data);

echo "1.) Selecting global for first sharding group (table test.fabric):\n";
mysqlnd_ms_fabric_select_global($c, "test.fabric");
var_dump(mysqlnd_ms_dump_servers($c));

echo "2.) Selecting global for second sharding group (table foo.bar):\n";
mysqlnd_ms_fabric_select_global($c, "foo.bar");
var_dump(mysqlnd_ms_dump_servers($c));

echo "\n\n3.) Selecting servers for different shards near boundaries:\n\n";

echo "test.fabric(1)\n";
mysqlnd_ms_fabric_select_shard($c, "test.fabric", 1);
var_dump(mysqlnd_ms_dump_servers($c));

foreach ([-1, 1, 29999, 30000, 30001, 39999, 40000, 40001] as $id) {
	echo "test.fabric($id)\n";
	mysqlnd_ms_fabric_select_shard($c, "test.fabric", $id);
	var_dump(mysqlnd_ms_dump_servers($c));
}

echo "\n\n4.) Selecting servers for invalid shard tables:\n";
foreach (["test.fabri", "test.fabric1"] as $table) {
	echo "$table(1)\n";
	mysqlnd_ms_fabric_select_shard($c, $table, 1);
	var_dump(mysqlnd_ms_dump_servers($c));
}
?>
===END===
--CLEAN--
<?php
unlink("fabric_sharding_dump_simple.json");
?>
--EXPECTF--
1.) Selecting global for first sharding group (table test.fabric):
array(2) {
  ["master"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(13) "global.shard1"
      ["port"]=>
      int(3301)
    }
  }
  ["slaves"]=>
  array(0) {
  }
}
2.) Selecting global for second sharding group (table foo.bar):
array(2) {
  ["master"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(14) "global1.shard2"
      ["port"]=>
      int(3302)
    }
  }
  ["slaves"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(14) "global2.shard2"
      ["port"]=>
      int(3303)
    }
  }
}


3.) Selecting servers for different shards near boundaries:

test.fabric(1)
array(2) {
  ["master"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host1.shard1"
      ["port"]=>
      int(3304)
    }
  }
  ["slaves"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host2.shard1"
      ["port"]=>
      int(3305)
    }
  }
}
test.fabric(-1)

Warning: mysqlnd_ms_fabric_select_shard(): Didn't receive usable servers from MySQL Fabric in %s on line %d
array(2) {
  ["master"]=>
  array(0) {
  }
  ["slaves"]=>
  array(0) {
  }
}
test.fabric(1)
array(2) {
  ["master"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host1.shard1"
      ["port"]=>
      int(3304)
    }
  }
  ["slaves"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host2.shard1"
      ["port"]=>
      int(3305)
    }
  }
}
test.fabric(29999)
array(2) {
  ["master"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host1.shard1"
      ["port"]=>
      int(3304)
    }
  }
  ["slaves"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host2.shard1"
      ["port"]=>
      int(3305)
    }
  }
}
test.fabric(30000)
array(2) {
  ["master"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host3.shard1"
      ["port"]=>
      int(3306)
    }
  }
  ["slaves"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host4.shard1"
      ["port"]=>
      int(3307)
    }
  }
}
test.fabric(30001)
array(2) {
  ["master"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host3.shard1"
      ["port"]=>
      int(3306)
    }
  }
  ["slaves"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host4.shard1"
      ["port"]=>
      int(3307)
    }
  }
}
test.fabric(39999)
array(2) {
  ["master"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host3.shard1"
      ["port"]=>
      int(3306)
    }
  }
  ["slaves"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host4.shard1"
      ["port"]=>
      int(3307)
    }
  }
}
test.fabric(40000)
array(2) {
  ["master"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host5.shard1"
      ["port"]=>
      int(3308)
    }
  }
  ["slaves"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host6.shard1"
      ["port"]=>
      int(3309)
    }
  }
}
test.fabric(40001)
array(2) {
  ["master"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host5.shard1"
      ["port"]=>
      int(3308)
    }
  }
  ["slaves"]=>
  array(1) {
    [0]=>
    array(2) {
      ["hostname"]=>
      string(12) "host6.shard1"
      ["port"]=>
      int(3309)
    }
  }
}


4.) Selecting servers for invalid shard tables:
test.fabri(1)

Warning: mysqlnd_ms_fabric_select_shard(): Didn't receive usable servers from MySQL Fabric in %s on line %d
array(2) {
  ["master"]=>
  array(0) {
  }
  ["slaves"]=>
  array(0) {
  }
}
test.fabric1(1)

Warning: mysqlnd_ms_fabric_select_shard(): Didn't receive usable servers from MySQL Fabric in %s on line %d
array(2) {
  ["master"]=>
  array(0) {
  }
  ["slaves"]=>
  array(0) {
  }
}
===END===
