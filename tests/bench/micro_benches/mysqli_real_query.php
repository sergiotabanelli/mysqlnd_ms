<?PHP
$runs   = array(100, 1000, 10000, 100000, 500000);
foreach ($runs as $k => $num_runs) {
  $times[$num_runs . 'x overall'] = 0;
  $times[$num_runs . 'x mysqli_real_query()'] = 0;
}
$description = 'Connect, n-times mysqli_real_query(SELECT 1) [no payload], close. n varies.';
$errors = array();

foreach ($runs as $k => $num_runs) {

  $times[$num_runs . 'x overall'] = microtime(true);
  do {
    if (!$link = @mysqli_connect($host, $user, $passwd, $db, $port, $socket)) {
      $errors[] = sprintf("'SELECT 1' connect failure");
      break 2;
    }
  
    for ($i = 0; $i < $num_runs; $i++) {
      $start = microtime(true); 
      mysqli_real_query($link, "SELECT 1");
      $times[$num_runs . 'x mysqli_real_query()'] += (microtime(true) - $start);
    }
 
    mysqli_close($link);
  
  } while (false);
  $times[$num_runs . 'x overall'] = microtime(true) - $times[$num_runs . 'x overall']; 
}

// sort by runtime in descending order
arsort($times);
?>