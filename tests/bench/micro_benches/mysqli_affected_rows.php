<?PHP
$runs   = array(10000);
$rows   = array(100, 1000);
foreach ($runs as $k => $num_runs) {
  foreach ($rows as $k => $num_rows) {    
    $times[$num_rows . ' rows: ' . $num_runs . 'x overall'] = 0;
    $times[$num_rows . ' rows: ' . $num_runs . 'x affected_rows()'] = 0;
  }
}
$description = 'Connect, n-times INSERT a row and call m-times mysqli_affected_rows() after the INSERT, close. n and m vary.';
$errors = array();

do {
  
  if (!$link = mysqli_connect($host, $user, $passwd, $db, $port, $socket)) {
    $errors[] = sprintf("Connect failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
    break;
  }
  
  if (!mysqli_query($link, "DROP TABLE IF EXISTS test")) {
    $errors[] = sprintf("DROP TABLE failed (converted code: %s): [%d] %s\n", 
      ($flag_original_code) ? 'no' : 'yes',
      mysqli_errno($link), mysqli_error($link));
    break;
  }
  
  if (!mysqli_query($link, "CREATE TABLE test(id INT, label char(1))")) {
    $errors[] = sprintf("CREATE TABLE failed (converted code: %s): [%d] %s\n", 
      ($flag_original_code) ? 'no' : 'yes',
      mysqli_errno($link), mysqli_error($link));
    break;
  }
  
  foreach ($rows as $k => $num_rows) {
    foreach ($runs as $k => $num_runs) {
      
      // create n-rows
      if (!mysqli_query($link, "DELETE FROM test")) {
        $errors[] = sprintf("Connect DELETE (converted code: %s): [%d] %s\n", 
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
        break 3;
      }
      
      $times[$num_rows . ' rows: ' . $num_runs . 'x overall'] = microtime(true);
      for ($i = 0; $i < $num_rows; $i++) {
        if (!mysqli_query($link, "INSERT INTO test(id, label) VALUES (" . $i . ", 'a')")) {
          $errors[] = sprintf("Connect DROP TABLE (converted code: %s): [%d] %s\n", 
            ($flag_original_code) ? 'no' : 'yes',
            mysqli_errno($link), mysqli_error($link));
          break 4;
        } 
        
        for ($j = 0; $j < $num_runs; $j++) {
          
          $start = microtime(true);
          if (1 != mysqli_affected_rows($link)) {
            $errors[] = sprintf("Connect DROP TABLE (converted code: %s): [%d] %s\n", 
              ($flag_original_code) ? 'no' : 'yes',
              mysqli_errno($link), mysqli_error($link));
            break 5;            
          }
          $times[$num_rows . ' rows: ' . $num_runs . 'x affected_rows()'] += (microtime(true) - $start);
          
        }      
      }
      $times[$num_rows . ' rows: ' . $num_runs . 'x overall'] = microtime(true) - $times[$num_rows . ' rows: ' . $num_runs . 'x overall'];
     
     
    }
  }
  
  mysqli_close($link);
  
} while (false);
?>