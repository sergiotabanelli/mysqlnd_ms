<?PHP
$times = $errors = array();
$rows = 200000;

$times = array(
            'overall'               => 0,
            'INSERT warmup'         => 0,
            'SELECT count(*)'       => 0,
            'TRUNCATE'              => 0,
            'INSERT final'          => 0,            
         );

do {
  
  $times['overall'] = microtime(true);

  if (!$conn =  mysqli_connect($host, $user, $passwd, $db, $port, $socket)) {
    $errors[] = sprintf("Cannot connect: [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
    break;
  }
    
  if (!mysqli_query($conn, "DROP TABLE IF EXISTS t1")) {
    $errors[] = sprintf("Cannot drop: [%d] %s\n", mysqli_errno($conn), mysqli_error($conn));
    break;
  }
  
  if (!mysqli_query($conn, "CREATE TABLE t1 (a int not null auto_increment primary
                    key, b varchar(48), c varchar(48), d varchar(48))")) {
    $errors[] = sprintf("Cannot create table: [%d] %s\n", mysqli_errno($conn), mysqli_error($conn));
    break;
  }

  $times['INSERT warmup'] = microtime(true);
  for ($i = 0; $i < $rows; $i++) {
    $val = md5(microtime(true));
    if (!mysqli_query($conn, "INSERT INTO t1 VALUES (0, '$val', '$val', '$val')")) {
      $errors[] = sprintf("Cannot insert (1): [%d] %s\n", mysqli_errno($conn), mysqli_error($conn));
      break 2;
    }
  }
  $times['INSERT warmup'] = microtime(true) - $times['INSERT warmup'];

  $times['SELECT count(*)'] = microtime(true);
  if (!$result = mysqli_query($conn, "SELECT count(*) FROM t1")) {
    $errors[] = sprintf("SELECT failed: [%d] %s\n", mysqli_errno($conn), mysqli_error($conn));
    break;
  }
  if (!$row = mysqli_fetch_row($result)) {
    $errors[] = sprintf("Fetch failed: [%d] %s\n", mysqli_errno($conn), mysqli_error($conn));
    break;
  }
  $times['SELECT count(*)'] = microtime(true) - $times['SELECT count(*)'];

  $times['TRUNCATE'] = microtime(true);
  mysqli_query($conn, "truncate table t1");
  $times['TRUNCATE'] = microtime(true) - $times['TRUNCATE'];
  
  $times['INSERT final'] = microtime(true);
  for ($i = 0; $i < $rows; $i++) {
    $val = md5(microtime(true));
    if (!mysqli_query($conn, "INSERT INTO t1 VALUES (0, '$val', '$val', '$val')")) {
      $errors[] = sprintf("Cannot insert (2): [%d] %s\n", mysqli_errno($conn), mysqli_error($conn));
      break 2;
    }
  }
  $times['INSERT final'] = microtime(true) - $times['INSERT final'];
  mysqli_close($conn);
  
  $times['overall'] = microtime(true) - $times['overall'];
  
} while (false);
?>