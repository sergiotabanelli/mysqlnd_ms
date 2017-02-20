<?php
set_time_limit(0);
ini_set("memory_limit", -1);
$flag_original_code = true;
$host = "127.0.0.1";
$user = "root";
$passwd = "root";
$db= "microbench";
$port= "3306";
$socket= "";
$engine= "InnoDB";

$times = $errors = array();
include("/home/nixnutz/src/mysqlnd/tests/ext/mysqli/bench/micro_benches/mysqli_fetch_long_unbuffered.php");
$all = array("times" => $times, "errors" => $errors, "memory" => (isset($memory)) ? $memory : NULL);
$fp = fopen("/home/nixnutz/src/mysqlnd/tests/ext/mysqli/bench/micro_benches/mysqli_fetch_long_unbuffered.php" . ".res", "w");
fwrite($fp, serialize($all));
fclose($fp);
print "done!";
?>
