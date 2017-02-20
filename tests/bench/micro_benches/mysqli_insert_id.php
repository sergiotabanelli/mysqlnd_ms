<?php
$rows   = array(100000); // equals #rows
foreach ($rows as $k => $num_rows) {
  $times[$num_rows . 'rows: overall'] = 0;
  $times[$num_rows . 'rows: insert_id()'] = 0;
}
$description = 'Connect, n-times INSERT a row and call insert_id(), close.';
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
  
  $sql = "CREATE TABLE test(id INT NOT NULL PRIMARY KEY AUTO_INCREMENT, label varchar(2))";
  if (!mysqli_query($link, $sql)) {
    $errors[] = sprintf("%s failed (converted code: %s): [%d] %s\n", 
      $sql,
      ($flag_original_code) ? 'no' : 'yes',
      mysqli_errno($link), mysqli_error($link));
    break;
  }
  
  foreach ($rows as $k => $num_rows) {
    
    if (!mysqli_query($link, "DELETE FROM test")) {
      $errors[] = sprintf("DELETE failed (%s) (converted code: %s): [%d] %s\n", 
          $num_rows,
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
      break 2;
    }
    
    $times[$num_rows . 'rows: overall'] = microtime(true);
    for ($i = 0; $i < $num_rows; $i++) {
      
      if (!mysqli_query($link, "INSERT INTO test(label) VALUES ('a')")) {
        $errors[] = sprintf("DELETE failed (%s) (converted code: %s): [%d] %s\n", 
          $num_rows,
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
        break 3;
      }
      
      $start = microtime(true);
      if (0 == mysqli_insert_id($link)) {
        $errors[] = sprintf("insert_id() failed (%s) (converted code: %s): [%d] %s\n", 
          $num_rows,
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
        break 3;
      }
      $times[$num_rows . 'rows: insert_id()'] += (microtime(true) - $start);
      
    }
    $times[$num_rows . 'rows: overall'] = microtime(true) - $times[$num_rows . 'rows: overall'];     
    
  }
  
} while (false);
?>