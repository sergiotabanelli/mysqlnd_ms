<?PHP
$runs   = array(1000);
$fields = array(1, 2, 4, 8, 16, 32, 64, 128);
$types  = array(16 => 'varchar(16)', 255 => 'varchar(255)');

foreach ($runs as $k => $num_runs) {
  foreach ($fields as $k => $num_fields) {    
    foreach ($types as $len => $type) {
      $times[$num_fields . ' fields: ' . $num_runs . 'x ' . $type . ' overall'] = 0;
      $times[$num_fields . ' fields: ' . $num_runs . 'x ' . $type . ' data_seek()'] = 0;
    }
  }
}
$description = 'Connect, create 1 row with n varchar(m) columns, SELECT all, o-times mysqli_fetch_field_direct(n...0), close. n, m and o vary.';
$errors = array();

do {
  
  if (!$link = mysqli_connect($host, $user, $passwd, $db, $port, $socket)) {
    $errors[] = sprintf("Connect failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
    break;
  }    
  
  foreach ($types as $len => $type) {  
    
    foreach ($fields as $k => $num_fields) {
      
      if (!mysqli_query($link, "DROP TABLE IF EXISTS test")) {
        $errors[] = sprintf("DROP TABLE failed (converted code: %s): [%d] %s\n", 
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
        break 3;
      }
  
      // create 1 row with n varchar(m) columns
      $sql = 'CREATE TABLE test(';
      for ($i = 0; $i < $num_fields; $i++) {
        $sql .= sprintf("c%d %s, ", $i, $type);
      }
      $sql = substr($sql, 0, -2) . ')';
      
      if (!mysqli_query($link, $sql)) {
        $errors[] = sprintf("%s failed (converted code: %s): [%d] %s\n", 
      
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
        break 3;
      }
            
      $label = str_repeat('a', $len);
      $columns = $values = '';
      for ($i = 0; $i < $num_fields; $i++) {
        $columns .= sprintf("c%d, ", $i);
        $values  .= sprintf("'%s', ", $label);
      }
      $sql = sprintf("INSERT INTO test(%s) VALUES (%s)", substr($columns, 0, -2), substr($values, 0, -2));
      if (!mysqli_query($link, $sql)) {
        $errors[] = sprintf("INSERT failed (%s) (converted code: %s): [%d] %s\n", 
          $type,
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
        break 3;
      } 

      $times[$num_fields . ' fields: ' . $num_runs . 'x ' . $type . ' overall'] = microtime(true);
      
      if (!$res = mysqli_query($link, "SELECT * FROM test")) {
        $errors[] = sprintf("SELECT failed (%s) (converted code: %s): [%d] %s\n", 
          $type,
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
        break 3;
      }           
      
      foreach ($runs as $k => $num_runs) {
        
        for ($i = $num_fields - 1; $i >= 0; $i--) {
          $start = microtime(true);
          for ($j = 0; $j < $num_runs; $j++) {
            if (!is_object($obj = mysqli_fetch_field_direct($res, $i))) {            
              $errors[] = sprintf("fetch_field_direct() failed (%s) (converted code: %s): [%d] %s\n", 
                $type,
                ($flag_original_code) ? 'no' : 'yes',
                mysqli_errno($link), mysqli_error($link));
              break 6;
            }
          }
          $times[$num_fields . ' fields: ' . $num_runs . 'x ' . $type . ' data_seek()'] += (microtime(true) - $start);
          if ($obj->name != "c$i") {
            $errors[] = sprintf("fetched object seems wrong (%s) (converted code: %s): [%d] %s\n", 
              $type,
              ($flag_original_code) ? 'no' : 'yes',
              mysqli_errno($link), mysqli_error($link));
            
            break 5;            
          }
        }        
      }
      mysqli_free_result($res);
      
      $times[$num_fields . ' fields: ' . $num_runs . 'x ' . $type . ' overall'] = microtime(true) - $times[$num_fields . ' fields: ' . $num_runs . 'x ' . $type . ' overall'];
    }
    
  } // end foreach types
 
  
  mysqli_close($link);
  
} while (false);
?>