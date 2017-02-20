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
      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_all() overall']   = 0;
      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_all() SELECT']    = 0;
      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_all() fetch']    = 0;
      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_array() overall'] = 0;
      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_array() SELECT'] = 0;
      $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_array() fetch'] = 0;
    }
  }
}
$description = 'Connect, create n rows with varchar of size m, o-times SELECT all rows (buffered) - with fetch_all and fetch_array, close. n, m, o vary.';

$errors = array();

function mysqli_fetch_all_vs_fetch_array($type, $len, $runs, $rows, $host, $user, $passwd, $db, $port, $socket, $flag_original_code) {

  $errors = $times = array();

  foreach ($rows as $k => $num_rows) {

    foreach ($runs as $k => $run) {      

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

        $client = mysqli_get_client_info();
        $have_mysqlnd =  ((substr($client, 0, 7) == 'mysqlnd')) ? true : false;

        if ($have_mysqlnd) {
          $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_all() overall'] = microtime(true);
          for ($i = 0; $i < $run; $i++) {

            $start = microtime(true);
            $res = mysqli_query($link, "SELECT id, label FROM test");
            $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_all() SELECT'] += (microtime(true) - $start);

            if (!$res) {
              $errors[] = sprintf("%d rows: SELECT %s %dx insert failure (original code = %s): [%d] %s", 
                              $rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
              break 4;
            }

            $start = microtime(true);
            $result1 = mysqli_fetch_all($res, MYSQLI_NUM);
            $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_all() fetch'] += (microtime(true) - $start);

            if ((count($result1) != $num_rows) || ($result1[0][0] != 1)) {              
              $errors[] = sprintf("%d rows: fetch_all() results  seem faulty %s %dx failure (original code = %s): [%d] %s", 
                              $rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
              break 4;
            }
            mysqli_free_result($res);
          }        
          $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_all() overall'] = microtime(true) - $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_all() overall'];
        }
        
        $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_array() overall'] = microtime(true);
        for ($i = 0; $i < $run; $i++) {

          $start = microtime(true);
          $res = mysqli_query($link, "SELECT id, label FROM test");
          $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_array() SELECT'] += (microtime(true) - $start);

          if (!$res) {
            $errors[] = sprintf("%d rows: SELECT %s %dx failure (original code = %s): [%d] %s", 
                              $rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
            break 4;
          }        

          $start = microtime(true);
          $result2 = array();          
          while ($result2[] = mysqli_fetch_array($res, MYSQLI_NUM))
            ;
          $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_array() fetch'] += (microtime(true) - $start);
          // we have one NULL at the end of result2[] because of the loop
          if ((count($result2) != ($num_rows + 1)) || ($result2[0][0] != 1)) {            
            $errors[] = sprintf("%d rows: fetch_array() results  seem faulty %s %dx failure (original code = %s): [%d] %s", 
                              $rows, $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
            break 4;
          }
          mysqli_free_result($res);
        } 
        $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_array() overall'] = microtime(true) - $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_array() overall'];   
              
        
        if (!$have_mysqlnd) {
          // NOTE: for mysqli @ libmysql (which does not have a fetch_all()), we use the fetch_array values!          
          $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_all() overall'] = 
            $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_array() overall'];
            
          $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_all() SELECT'] =
            $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_array() SELECT'];
          
          $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_all() fetch'] =
            $times[$num_rows . ' rows: SELECT ' . $type . ' ' . $run . 'x fetch_array() fetch'];
        }

        mysqli_close($link); 

      } while (false);      

    }

  }

  return array($errors, $times);
}


foreach ($types as $len => $type) {
  list ($errors, $tmp_times) = mysqli_fetch_all_vs_fetch_array($type,  $len, $runs, $rows, $host, $user, $passwd, $db, $port, $socket, $flag_original_code);
  $times = array_merge($times, $tmp_times);
  if (!empty($errors))  { 
    break;
  }
}

// sort by labels
ksort($times);
?>