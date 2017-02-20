<?PHP
$runs   = array(10, 100);
$rows   = array(100, 1000);
$types  = array(
            16    => 'varchar(16)',
            32    => 'varchar(32)',
            64    => 'varchar(64)',
            128   => 'varchar(128)',
            256   => 'varchar(256)',
            512   => 'varchar(512)',
            1024  => 'varchar(1024)',
            2048  => 'varchar(2048)',
            4192  => 'varchar(4192)',
            8384  => 'varchar(8384)',
            16768 => 'varchar(16768)',
            33536 => 'varchar(33536)',
            65500 => 'varchar(65500)'
            
          );
foreach ($runs as $k => $num_runs) {
  foreach ($rows as $k => $num_rows) {    
    foreach ($types as $len => $type) {
      $times[$num_rows . ' rows: ' . $num_runs . 'x ' . $type . ' overall'] = 0;
      $times[$num_rows . ' rows: ' . $num_runs . 'x ' . $type . ' data_seek()'] = 0;
    }
  }
}
$description = 'Connect, create n-rows with varchar(m), SELECT all, o-times [$runs] mysqli_stmt_data_seek(mt_rand()), close. n, m and o vary.';
$errors = array();

do {
  
  if (!$link = mysqli_connect($host, $user, $passwd, $db, $port, $socket)) {
    $errors[] = sprintf("Connect failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
    break;
  }    
  
  foreach ($types as $len => $type) {
    
    if (!mysqli_query($link, "DROP TABLE IF EXISTS test")) {
      $errors[] = sprintf("DROP TABLE failed (converted code: %s): [%d] %s\n", 
        ($flag_original_code) ? 'no' : 'yes',
        mysqli_errno($link), mysqli_error($link));
      break 2;
    }
  
    $sql = sprintf("CREATE TABLE test(id INT, label %s)", $type);
    if (!mysqli_query($link, $sql)) {
      $errors[] = sprintf("CREATE TABLE (%s) failed (converted code: %s): [%d] %s\n", 
        $sql,
        ($flag_original_code) ? 'no' : 'yes',
        mysqli_errno($link), mysqli_error($link));
      break 2;
    }
    
    foreach ($rows as $k => $num_rows) {
      foreach ($runs as $k => $num_runs) {
        
        // create n-rows
        if (!mysqli_query($link, "DELETE FROM test")) {
          $errors[] = sprintf("DELETE failed (%s) (converted code: %s): [%d] %s\n", 
            $type,
            ($flag_original_code) ? 'no' : 'yes',
            mysqli_errno($link), mysqli_error($link));
          break 4;
        }
               
        $label = str_repeat('a', $len);
        for ($i = 0; $i < $num_rows; $i++) {
          $sql = sprintf("INSERT INTO test(id, label) VALUES (%d, '%s')", $i, $label);
          if (!mysqli_query($link, $sql)) {
            $errors[] = sprintf("INSERT failed (%s) (converted code: %s): [%d] %s\n", 
              $type,
              ($flag_original_code) ? 'no' : 'yes',
              mysqli_errno($link), mysqli_error($link));
            break 5;
          }          
        }
        
        if (!$stmt = mysqli_stmt_init($link)) {
          $errors[] = sprintf("stmt_init() failed (converted code: %s)\n",             
            ($flag_original_code) ? 'no' : 'yes');
          break 4;
        }
        if (!mysqli_stmt_prepare($stmt, 'SELECT id, label FROM test ORDER BY id')) {
          $errors[] = sprintf("stmt_prepare() failed (converted code: %s)\n",             
            ($flag_original_code) ? 'no' : 'yes');
          break 4;
        }
        if (!mysqli_stmt_execute($stmt)) {
          $errors[] = sprintf("stmt_execute() failed (converted code: %s)\n",             
            ($flag_original_code) ? 'no' : 'yes');
          break 4;
        }
        $id = $label = null;
        if (!mysqli_stmt_bind_result($stmt, $id, $label)) {
          $errors[] = sprintf("stmt_bind_result() failed (converted code: %s)\n",             
            ($flag_original_code) ? 'no' : 'yes');
          break 4;
        }
        if (!mysqli_stmt_store_result($stmt)) {
          $errors[] = sprintf("stmt_store_result() failed (converted code: %s)\n",             
            ($flag_original_code) ? 'no' : 'yes');
          break 4;
        }        
        
        mt_srand();
        $times[$num_rows . ' rows: ' . $num_runs . 'x ' . $type . ' overall'] = microtime(true);
        for ($i = 0; $i < $num_runs; $i++) {
          
          $pos = mt_rand(0, $num_rows - 1);
          
          $start = microtime(true);
          mysqli_stmt_data_seek($stmt, $pos);
          $times[$num_rows . ' rows: ' . $num_runs . 'x ' . $type . ' data_seek()'] += (microtime(true) - $start);
          
          if (!mysqli_stmt_fetch($stmt)) {
            $errors[] = sprintf("fetch() for %dth of %d rows failed (%s) (converted code: %s): [%d] %s\n", 
              $pos, $num_rows, $type,
              ($flag_original_code) ? 'no' : 'yes',
              mysqli_stmt_errno($stmt), mysqli_stmt_error($stmt));
            break 5;
          }
          
          if ($id != $pos) {
            $errors[] = sprintf("seek() + fetch() for %dth of %d rows did not work (%s) (converted code: %s): [%d] %s\n", 
              $pos, $num_rows, $type,
              ($flag_original_code) ? 'no' : 'yes',              
              mysqli_stmt_errno($stmt), mysqli_stmt_error($stmt));
              var_dump($id);
              var_dump($pos);
            break 5;
          }

        }
        $times[$num_rows . ' rows: ' . $num_runs . 'x ' . $type . ' overall'] = microtime(true) - $times[$num_rows . ' rows: ' . $num_runs . 'x ' . $type . ' overall'];
        
        mysqli_stmt_close($stmt);                
      }
    }
    
  } // end foreach types
 
  
  mysqli_close($link);
  
} while (false);
?>