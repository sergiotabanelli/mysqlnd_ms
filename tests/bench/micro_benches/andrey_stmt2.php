<?PHP
/* Config settings should go at the very beginning */
$rows = array(100, 1000, 10000, 100000, 1000000);

// Set a description
$description = 'This is a skeleton for a benchmark that does nothing.';


/*
  This is a hash that stores the runtimes
  It is dumped in all reports. The hash keys will be used as labels/descriptions. 
  $times['select()'] = 0.01; => shown in reports like 'select() : 0.01s'
*/
$times = array();
/*
  It's recommended to initialize the times hash with 0 values
  for all entries. This often makes debugging easier. 
  Make sure you have one 'overall' entry in your list which 
  holds the 'entire runtime' of you test (e.g. create table + insert + select) and not only 
  a fraction of it (e.g. select).
*/
foreach ($rows as $k => $num_rows) {
  $times[sprintf("%7d: overall", $num_rows)] = 0;
  $times[sprintf("%7d: query", $num_rows)] = 0;
  $times[sprintf("%7d: fetch()", $num_rows)] = 0;
}

/*
  List of errors that happened during the test run. You should stop
  the benchmark after the first error
*/
$errors = array();


do {
  
  if (!$link = mysqli_connect($host, $user, $passwd, $db, $port, $socket)) {    
    $errors[] = sprintf("Connect failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
    break;
  }
  
  if (!mysqli_query($link, "DROP TABLE IF EXISTS test")) {
    $errors[] = sprintf("DROP TABLE IF EXISTS failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
    break;
  }
  
  if (!mysqli_query($link, "CREATE TABLE test(a INT, b CHAR(255), c CHAR(255), d CHAR(255))")) {
    $errors[] = sprintf("CREATE TABLE failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
    break;
  } 
  
  foreach ($rows as $k => $num_rows) {
    
    if (!mysqli_query($link, "TRUNCATE TABLE test")) {
      $errors[] = sprintf("TRUNCATE failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
      break 2;
    }
    $label = str_repeat('a', 255);
    for ($i = 0; $i < $num_rows; $i++) {
      if (!mysqli_query($link, sprintf("INSERT INTO test(a, b, c, d) VALUES (%d, '%s', '%s', '%s')", 
                $i, $label, $label, $label))) {
        $errors[] = sprintf("TRUNCATE failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
        break 3;
      }      
    }
           
    $times[sprintf("%7d: overall", $num_rows)] = $start = microtime(true);
        
    if (!$stmt = mysqli_stmt_init($link)) {
      $errors[] = sprintf("stmt_init() failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
      break 2;
    }
    if (!mysqli_stmt_prepare($stmt, 'SELECT * FROM test')) {
      $errors[] = sprintf("stmt_prepare() failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
      break 2;
    }
    if (!mysqli_stmt_execute($stmt)) {
      $errors[] = sprintf("stmt_execute() failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
      break 2;
    }
    
    $row = array(0 => null, 1 => null, 2 => null, 3 => null);
    if (!mysqli_stmt_bind_result($stmt, $row[0], $row[1], $row[2], $row[3])) {
      $errors[] = sprintf("stmt_execute() failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
      break 2;
    }    
    $times[sprintf("%7d: query", $num_rows)] = (microtime(true) - $start);
    
    $start = microtime(true);
    while (mysqli_stmt_fetch($stmt))
      ;
    $times[sprintf("%7d: stmt_fetch()", $num_rows)] = (microtime(true) - $start);
        
    $times[sprintf("%7d: overall", $num_rows)] = (microtime(true) - $times[sprintf("%7d: overall", $num_rows)]);
  }  

  mysqli_close($link);
    
} while (false);
?>