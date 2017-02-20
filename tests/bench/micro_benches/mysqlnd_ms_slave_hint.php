<?PHP
/* Config settings should go at the very beginning */
$queries = 100000;

// Set a description
$description = 'Check impact of SQL hint parsing. This benchmark requires one binary featuring mysqlnd_ms. It should be run once with mysqlnd_ms.enable=1 and once with mysqlnd_ms.enable=0. If enabled, ms will parse the SQL slave hint at the beginning of the SQL statement. If not, the hint will be ignored.';


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
$times['overall'] = 0;
$times['select'] = 0;

/*
  List of errors that happened during the test run. You should stop
  the benchmark after the first error
*/
$errors = array();


do {
  
  $times['overall'] = microtime(true);

  /*
    Use this to connect to the database. The variables $host, ... 
    $socket and $flag_original_code are provided by the framework  
  */
  if (!$link = mysqli_connect($host, $user, $passwd, $db, $port, $socket)) {
    
    $errors[] = sprintf("Connect failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
    break;
  }

  if (defined('MYSQLND_MS_SLAVE_SWITCH')) {
    /* make sure all binaries have mysqlnd_ms compiled in */
    $hint = sprintf('/*%s*/', MYSQLND_MS_SLAVE_SWITCH);    
  } else {
    /* make an educated guess... */
    $hint = '/*ms=slave*/';	
  }
  $sql = $hint . 'SELECT 1 FROM DUAL';

  for ($i =  0; $i < $queries; $i++) {

    $start = microtime(true);
    if (!$res = mysqli_query($link, $sql)) {
      $errors[] = sprintf("SELECT failed (converted code: %s)\n",
        ($flag_original_code) ? 'no' : 'yes');
      break 2;
    }
    $times['select'] += (microtime(true) - $start);

    mysqli_free_result($res);
  }  

  mysqli_close($link);
  $times['overall'] = (microtime(true) - $times['overall']);
  
} while (false);
?>