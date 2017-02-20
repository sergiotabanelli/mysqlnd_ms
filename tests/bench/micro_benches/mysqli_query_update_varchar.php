<?PHP
$runs   = array(1, 10, 100, 1000, 5000);
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
foreach ($runs as $k => $run) {
  foreach ($types as $len => $type) {
    $times['UPDATE ' . $type . ' ' . $run . 'x query()'] = 0;
    $times['UPDATE ' . $type . ' ' . $run . 'x overall'] = 0;
  }
}
$description = 'Connect, create 100 random rows with varchar of size m, o-times UPDATE random row, close. n, m, o vary.';
$errors = array();

function mysqli_query_update($type, $len, $runs, $host, $user, $passwd, $db, $port, $socket) {

  $errors = $times = array();

  foreach ($runs as $k => $run) {
     $times['UPDATE ' . $type . ' ' . $run . 'x overall'] = microtime(true);
    do {
      if (!$link = @mysqli_connect($host, $user, $passwd, $db, $port, $socket)) {
        $errors[] = sprintf("UPDATE %s %dx connect failure (original code = %s)", $type, $run, ($flag_original_code) ? 'yes' : 'no');
        break 2;
      }
        if (!mysqli_query($link, "DROP TABLE IF EXISTS test")) {
        $errors[] = sprintf("UPDATE %s %dx drop table failure (original code = %s): [%d] %s", $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
        break 2;
      }

      if (!mysqli_query($link, sprintf("CREATE TABLE test(id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, label %s)", $type))) {
        $errors[] = sprintf("UPDATE %s %dx create table failure (original code = %s): [%d] %s", $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
        break 2;
      }

      $label = '';
      for ($i = 0; $i < $len; $i++)
        $label .= chr(mt_rand(65, 90));
      $label = mysqli_real_escape_string($link, $label);

      for ($i = 1; $i <= 100; $i++) {
        if (!mysqli_query($link, "INSERT INTO test(id, label) VALUES ($i, '$label')")) {
          $errors[] = sprintf("UPDATE %s %dx insert failure (original code = %s): [%d] %s", $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
          break 3;
        }
      }

      $label = str_repeat('a', $len);
      $id = 0;

      for ($i = 0; $i < $run; $i++) {
        $id = (++$id > 100) ? 1 : $id;
        $sql = "UPDATE test SET label = '" . $label . "' WHERE id = " . $id;
        $start = microtime(true);
        $ret = mysqli_query($link, $sql);
        $times['UPDATE ' . $type . ' ' . $run . 'x query()'] += microtime(true) - $start;
        if (!$ret) {
          $errors[] = sprintf("UPDATE %s %dx insert failure (original code = %s): [%d] %s", $type, $run, ($flag_original_code) ? 'yes' : 'no', mysqli_errno($link), mysqli_error($link));
          break 3;
        }
      }
      mysqli_close($link);
      $times['UPDATE ' . $type . ' ' . $run . 'x overall'] = microtime(true) - $times['UPDATE ' . $type . ' ' . $run . 'x overall'];
    } while (false);
  }
  
  return array($errors, $times);
}

ksort($types);
foreach ($types as $len => $type) {
  list ($errors, $tmp_times) = mysqli_query_update($type,  $len, $runs, $host, $user, $passwd, $db, $port, $socket);
  $times = array_merge($times, $tmp_times);
  if (!empty($errors))  { 
    break;
  }
}

// sort by labels
ksort($times);
?>