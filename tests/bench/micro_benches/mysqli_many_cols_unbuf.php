<?PHP
// measured in KB, that means 1 = 1024 bytes
$columns = array(1, 3, 5, 10, 15, 20, 30, 40, 50, 100, 300, 500);
$column_types = array(
                  array("len" => 1, "name" => "c_tinyint", "type" => "TINYINT", "value" => 1),
                  array("len" => 2, "name" => "c_smallint", "type" => "SMALLINT", "value" => 1),
                  array("len" => 200, "name" => "c_varchar_199", "type" => "varchar(199)", "value" => "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678"),
                  array("len" => 3, "name" => "c_mediumint", "type" => "MEDIUMINT", "value" => 1),
                  array("len" => 20, "name" => "c_varchar_19", "type" => "varchar(19)", "value" => "0123456789012345678"),
                  array("len" => 4, "name" => "c_int", "type" => "INT", "value" => 1),
                  array("len" => 8, "name" => "c_bigint", "type" => "BIGINT", "value" => 1),
                  array("len" => 50, "name" => "c_varchar_49", "type" => "varchar(49)", "value" => "0123456789012345678901234567890123456789012345678"),
                  array("len" => 4, "name" => "c_float_24", "type" => "FLOAT(24)", "value" => 1),
                  array("len" => 150, "name" => "c_varchar_149", "type" => "varchar(149)", "value" => "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678"),
                  array("len" => 8, "name" => "c_float_25", "type" => "FLOAT(25)", "value" => 1),
                  array("len" => 4, "name" => "c_float", "type" => "FLOAT", "value" => 1),                  
                  array("len" => 8, "name" => "c_double", "type" => "DOUBLE", "value" => 1),
                  array("len" => 100, "name" => "c_varchar_99", "type" => "varchar(99)", "value" => "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678"),
                  array("len" => 3, "name" => "c_date", "type" => "DATE", "value" => "2007-01-17"),
                  array("len" => 8, "name" => "c_datetime", "type" => "DATETIME", "value" => "2007-01-17 11:53:00"),
                  array("len" => 4, "name" => "c_timestamp", "type" => "TIMESTAMP", "value" => "2007-01-17 11:54:00"),
                  array("len" => 3, "name" => "c_time", "type" => "TIME", "value" => "11:54:00"),
                  array("len" => 1, "name" => "c_year", "type" => "YEAR", "value" => "2007"),
                  array("len" => 10, "name" => "c_char_10", "type" => "CHAR(10)", "value" => "0123456789"),
                  array("len" => 1, "name" => "c_enum", "type" => "ENUM('true', 'false')", "value" => "true"),
                  // let's assume latin1                  
                  array("len" => 300, "name" => "c_varchar_99", "type" => "varchar(298)", "value" => "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567"),                  
                );
$rows = array(2000);


foreach ($rows as $k => $num_rows) {
  foreach ($columns as $k => $num_columns) {  
    
    $num_column_types = count($column_types);
    $total_len = 0;
    for ($i = 0; $i < $num_columns; $i++) {
      $total_len += $column_types[$i % $num_column_types]['len'];
    }
    $times[$num_rows . ' row[s]: ' . $num_columns . ' column[s] (' . $total_len . ' bytes) overall'] = 0;
    $times[$num_rows . ' row[s]: ' . $num_columns . ' column[s] (' . $total_len . ' bytes) fetch()'] = 0;
  }
}
$description = 'Connect, create n-rows with one m-columns of different types, SELECT all (buffered), close. n and m vary.';
$errors = array();

do {   
  
  if (!$link = mysqli_connect($host, $user, $passwd, $db, $port, $socket)) {
    $errors[] = sprintf("Connect failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
    break;
  }
  
  foreach ($rows as $k => $num_rows) {    
    
    foreach ($columns as $k => $num_columns) {
    
      if (!mysqli_query($link, "DROP TABLE IF EXISTS test")) {
        $errors[] = sprintf("DROP TABLE failed (converted code: %s): [%d] %s\n", 
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
        break 3;
      }
      
      $num_column_types = count($column_types);      
      $total_len = 0;
      $column_names = $column_defs = $column_values = '';
      for ($i = 0; $i < $num_columns; $i++) {
        $index = $i % $num_column_types;
        $total_len += $column_types[$index]['len'];
        $column_names .= sprintf("%s_%d, ", $column_types[$index]['name'], $i);
        $column_defs .= sprintf("%s_%d %s, ", $column_types[$index]['name'], $i, $column_types[$index]['type']);
        $column_values .= sprintf("%s%s%s, ",
          (is_string($column_types[$index]['value'])) ? '"' : '',
          $column_types[$index]['value'],
          (is_string($column_types[$index]['value'])) ? '"' : ''
        );        
      }
      $last_column = sprintf("%s_%d", $column_types[$index]['name'], $i - 1);
      $last_value = $column_types[$index]['value'];
  
      $sql = sprintf("CREATE TABLE test(%s)", substr($column_defs, 0, -2));
      if (!mysqli_query($link, $sql)) {
        $errors[] = sprintf("%s failed (converted code: %s): [%d] %s\n", 
          $sql,
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
        break 3;
      }
      
      $sql = sprintf("INSERT INTO test(%s) VALUES (%s)", substr($column_names, 0, -2), substr($column_values, 0, -2));
      for ($i = 0; $i < $num_rows; $i++) {
        if (!mysqli_query($link, $sql)) {
          $errors[] = sprintf("%s failed (converted code: %s): [%d] %s\n", 
            $sql,
            ($flag_original_code) ? 'no' : 'yes',
            mysqli_errno($link), mysqli_error($link));
          break 4;
        }
      }
      
      $times[$num_rows . ' row[s]: ' . $num_columns . ' column[s] (' . $total_len . ' bytes) overall'] = microtime(true);
      if (!mysqli_real_query($link, 'SELECT * FROM test')) {
        $errors[] = sprintf("SELECT failed (converted code: %s): [%d] %s\n", 
          $sql,
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
        break 3;
      }
      
      if (!$res = mysqli_use_result($link)) {
         $errors[] = sprintf("use_result() failed (converted code: %s): [%d] %s\n", 
          $sql,
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
        break 3;
      }
      
      $start = microtime(true);
      while ($row = mysqli_fetch_assoc($res))
        if ($row[$last_column] != $last_value) {
          $errors[] = sprintf("fetch() failed (converted code: %s): [%d] %s\n",             
            ($flag_original_code) ? 'no' : 'yes',
            mysqli_errno($link), mysqli_error($link));
            var_dump($row);
            var_dump($last_column);
            var_dump($last_value);
          break 4;
          
        }
      $times[$num_rows . ' row[s]: ' . $num_columns . ' column[s] (' . $total_len . ' bytes) fetch()'] += (microtime(true) - $start);
      
      mysqli_free_result($res);
      $times[$num_rows . ' row[s]: ' . $num_columns . ' column[s] (' . $total_len . ' bytes) overall'] = microtime(true) - $times[$num_rows . ' row[s]: ' . $num_columns . ' column[s] (' . $total_len . ' bytes) overall'];
      
            
      
    }
  }
  

  mysqli_close($link);
  
} while (false);
?>