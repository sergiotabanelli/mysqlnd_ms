<?php
set_time_limit(0);
ini_set("memory_limit", -1);
$flag_original_code = true;
$host = "192.168.2.29";
$user = "root";
$passwd = "root";
$db= "microbench";
$port= "3306";
$socket= "RB_DB_SOCKET";
$engine= "InnoDB";

$times = $errors = array();
include("/home/nixnutz/src/mysqlnd/tests/ext/mysqli/bench/micro_benches/mysqli_data_seek_random.php");
$all = array("times" => $times, "errors" => $errors, "memory" => (isset($memory)) ? $memory : NULL);
$fp = fopen("/home/nixnutz/src/mysqlnd/tests/ext/mysqli/bench/micro_benches/mysqli_data_seek_random.php" . ".res", "w");
fwrite($fp, serialize($all));
fclose($fp);
print "done!";
?>
