<?PHP
$query_lengths   = array(100, 200, 300, 400, 500, 750, 1000, 1250, 1500, 2000, 3000, 5000, 10000, 50000); 
$runs = array(100, 1000, 5000);
foreach ($runs as $k => $run) {
  foreach ($query_lengths as $k => $len) {
    $times[$run . "x, " . $len . " characters long SELECT overall"] = 0;
    $times[$run . "x, " . $len . " characters long SELECT query()"] = 0;
  }
}
$description = 'Connect, run n times >SELECT "aaaa..."< command with a total length of m-characters, close. n and m vary.';
$errors = array();

do {   
  
  if (!$link = mysqli_connect($host, $user, $passwd, $db, $port, $socket)) {
    $errors[] = sprintf("Connect failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
    break;
  }
  
  foreach ($runs as $k => $run) {
    foreach ($query_lengths as $k => $len) {
      
      $sql = 'SELECT "a';
      $where = ' FROM DUAL WHERE 1 = 2';
      $remaining = $len - strlen($sql) - strlen($where) - 7;
      $sql = sprintf('%s%s" AS a %s', $sql, str_repeat('a', $remaining), $where);
      
      $times[$run . "x, " . $len . " characters long SELECT overall"] = microtime(true);
      for ($i = 0; $i < $run; $i++) {
        $start = microtime(true);
        if (!$res = mysqli_query($link, $sql)) {           
          $errors[] = sprintf("SELECT failed (converted code: %s): [%d] %s\n", 
            ($flag_original_code) ? 'no' : 'yes',
            mysqli_errno($link), mysqli_error($link));
          break;
        }
        $times[$run . "x, " . $len . " characters long SELECT query()"] += (microtime(true) - $start);
        mysqli_free_result($res);
      }
      $times[$run . "x, " . $len . " characters long SELECT overall"] = microtime(true) - $times[$run . "x, " . $len . " characters long SELECT overall"];
      
    }    
  }
  
  mysqli_close($link);
  
} while (false);
?>