<?PHP
$runs   = array(10, 100);
$rows   = array(100, 100);
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
      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x overall simple query()'] = 0;
      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x overall real_query()'] = 0;
      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_assoc()'] = 0;
      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x real_query()'] = 0;
      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x store_result()'] = 0;
      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_assoc() @ store_result()'] = 0;
    }
  }
}
$description = 'Connect, create n rows with varchar of size m, o-times SELECT all rows (buffered), close. n, m, o vary.';
$errors = array();

function mysqli_query_select_varchar_buffered($type, $len, $runs, $rows, $host, $user, $passwd, $db, $port, $socket, $flag_original_code) {

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
        $label = mysqli_real_escape_string($link, $label);

        for ($i = 1; $i <= $num_rows; $i++) {
          if (!mysqli_query($link, "INSERT INTO test(id, label) VALUES ($i, '$label')")) {
            $errors[] = sprintf("%d rows: SELECT %s %dx insert failure (original code = %s): [%d] %s", 
                              $num_rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
            break 3;
          }
        }

        $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x overall simple query()'] = microtime(true);
        for ($i = 0; $i < $run; $i++) {

          $start = microtime(true);
          $res = mysqli_query($link, "SELECT id, label FROM test");
          $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x query()'] += (microtime(true) - $start);

          if (!$res) {
            $errors[] = sprintf("%d rows: SELECT %s %dx insert failure (original code = %s): [%d] %s", 
                              $rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
            break 4;
          }

          $start = microtime(true);
          while ($row = mysqli_fetch_assoc($res))
            ;
          $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_assoc()'] += (microtime(true) - $start);

          mysqli_free_result($res);
        }        
        $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x overall simple query()'] = microtime(true) - $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x overall simple query()'];
        
        $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x overall real_query()'] = microtime(true);
        for ($i = 0; $i < $run; $i++) {

          $start = microtime(true);
          $res = mysqli_real_query($link, "SELECT id, label FROM test");
          $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x real_query()'] += (microtime(true) - $start);

          if (!$res) {
            $errors[] = sprintf("%d rows: SELECT %s %dx insert failure (original code = %s): [%d] %s", 
                              $rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
            break 4;
          }
          
          $start = microtime(true);
          if (!$res = mysqli_store_result($link)) {
            $errors[] = sprintf("%d rows: store_result() failed (original code = %s): [%d] %s", 
                              $rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
            break 4;
          }
            
          $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x store_result()'] += (microtime(true) - $start);
          

          $start = microtime(true);
          while ($row = mysqli_fetch_assoc($res))
            ;
          $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_assoc() @ store_result()'] += (microtime(true) - $start);

          mysqli_free_result($res);
        } 
        $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x overall real_query()'] = microtime(true) - $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x overall real_query()'];   

        mysqli_close($link); 

      } while (false);

      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x overall'] = microtime(true) - $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x overall'];

    }

  }

  return array($errors, $times);
}


foreach ($types as $len => $type) {
  list ($errors, $tmp_times) = mysqli_query_select_varchar_buffered($type,  $len, $runs, $rows, $host, $user, $passwd, $db, $port, $socket, $flag_original_code);
  $times = array_merge($times, $tmp_times);
  if (!empty($errors))  { 
    break;
  }
}

// sort by labels
ksort($times);
?>