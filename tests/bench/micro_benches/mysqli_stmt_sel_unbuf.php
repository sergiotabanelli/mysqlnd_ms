<?PHP
$runs   = array(10, 100);
$rows   = array(100, 1000);
$types = array(
              127   => 'varchar(127)',                  
              255   => 'varchar(255)',
              512   => 'varchar(512)',
              1024  => 'varchar(1024)',
              2048  => 'varchar(2048)',
              4096  => 'varchar(4096)',
              8192  => 'varchar(8192)',
              16384 => 'varchar(16384)',
              32768 => 'varchar(32768)',
              65000 => 'varchar(65000)',
          );
$times = array();
foreach ($rows as $k => $num_rows) {
  foreach ($runs as $k => $run) {
    foreach ($types as $len => $type) {
      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x query()'] = 0;
      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x overall'] = 0;
      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_assoc()'] = 0;
    }
  }
}
$description = 'Connect, create n rows with varchar of size m, o-times SELECT all rows (unbuffered), close. n, m, o vary.';
$errors = array();

function mysqli_query_select_varchar_unbuffered($type, $len, $runs, $rows, $host, $user, $passwd, $db, $port, $socket, $flag_original_code) {

  $errors = $times = array();

  foreach ($rows as $k => $num_rows) {

    foreach ($runs as $k => $run) {

       $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x overall'] = microtime(true);

      do {
        if (!$link = @mysqli_connect($host, $user, $passwd, $db, $port, $socket)) {
          $errors[] = sprintf("%d rows: SELECT %s %dx connect failure (original code = %s)", 
                              $num_rows, $type, $run, ($flag_original_code) ? 'yes' : 'no');
          break 3;
        }
          if (!mysqli_query($link, "DROP TABLE IF EXISTS test")) {
          $errors[] = sprintf("%d rows: SELECT %s %dx drop table failure (original code = %s): [%d] %s", 
                              $num_rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
          break 3;
        }

        if (!mysqli_query($link, sprintf("CREATE TABLE test(id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, label %s)", $type))) {
          $errors[] = sprintf("%d rows: SELECT %s %dx create table failure (original code = %s): [%d] %s", 
                                      $num_rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
          break 3;
        }

        $label = '';
        for ($i = 0; $i < $len; $i++)
          $label .= chr(mt_rand(65, 90));
        
        if (!$stmt = mysqli_stmt_init($link)) {
            $errors[] = sprintf("%d rows: SELECT %s %dx stmt_init() for INSERT failed (original code = %s): [%d] %s", 
                                      $num_rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
          break 3;
        }
        if (!mysqli_stmt_prepare($stmt, "INSERT INTO test(id, label) VALUES (?, ?)")) {
            $errors[] = sprintf("%d rows: SELECT %s %dx stmt_prepare() for INSERT failed (original code = %s): [%d] %s", 
                                      $num_rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
          break 3;
        }
        if (!mysqli_stmt_bind_param($stmt, "is", $i, $label)) {
            $errors[] = sprintf("%d rows: SELECT %s %dx stmt_bind_param() for INSERT failed (original code = %s): [%d] %s", 
                                      $num_rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
          break 3;                            
        }

        for ($i = 1; $i <= $num_rows; $i++) {
          if (!mysqli_stmt_execute($stmt)) {
            $errors[] = sprintf("%d - $i  rows: SELECT %s %dx insert failure (original code = %s): [%d] %s", 
                              $num_rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
            break 4;
          }
        }
        mysqli_stmt_close($stmt);               
        
        for ($i = 0; $i < $run; $i++) {      
            
          if (!$stmt = mysqli_stmt_init($link)) {
            $errors[] = sprintf("%d rows: SELECT %s %dx stmt_init() for SELECT failed (original code = %s): [%d] %s",                                       $num_rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
            break 4;
          }
          if (!mysqli_stmt_prepare($stmt, "SELECT id, label FROM test")) {
            $errors[] = sprintf("%d rows: SELECT %s %dx stmt_prepare() for SELECT failed (original code = %s): [%d] %s", 
                                     $num_rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
            break 4;
          }

          $start = microtime(true);
          $ret = mysqli_stmt_execute($stmt);          
          $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x stmt_execute()'] += (microtime(true) - $start);

          if (!$ret) {
            $errors[] = sprintf("%d rows: SELECT %s %dx stmt_execute() SELECT failure (original code = %s): [%d] %s", 
                            $rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
            break 4;
          }

          $id2 = $label2 = null;
          if (!mysqli_stmt_bind_result($stmt, $id2, $label2)) {
            $errors[] = sprintf("%d rows: SELECT %s %dx stmt_prepare() for SELECT failed (original code = %s): [%d] %s", 
                                     $num_rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
            break 4;
          }
            
          $start = microtime(true);
          while (mysqli_stmt_fetch($stmt))
            ;
          $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x stmt_fetch()'] += (microtime(true) - $start);
          mysqli_stmt_close($stmt);
          
        }
        
        mysqli_close($link); 

      } while (false);

      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x overall'] = microtime(true) - $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x overall'];

    }

  }

  return array($errors, $times);
}


foreach ($types as $len => $type) {
  list ($errors, $tmp_times) = mysqli_query_select_varchar_unbuffered($type,  $len, $runs, $rows, $host, $user, $passwd, $db, $port, $socket, $flag_original_code);
  $times = array_merge($times, $tmp_times);
  if (!empty($errors))  { 
    break;
  }
}

// sort by labels
ksort($times);
?>
