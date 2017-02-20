<?PHP
/* Config settings should go at the very beginning */
$runs = 10000;

// Set a description
$description = 'Call stmt_execute() for "SELECT 1"';


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
$times['execute'] = 0;

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
  
  if (!$stmt = mysqli_stmt_init($link)) {
    $errors[] = sprintf("stmt_init() (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
    break;
  }
  
  if (!mysqli_stmt_prepare($stmt, "SELECT 1")) {
    $errors[] = sprintf("stmt_prepare() (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
    break;
  }
  
  $start = microtime(true);
  for ($i =  0; $i < $runs; $i++) {
    if (!@mysqli_stmt_execute($stmt)) {
      $errors[] = sprintf("stmt_execute() (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
      break 2;
    }    
    mysqli_stmt_free_result($stmt);   
  }
  $times['execute'] = microtime(true) - $start;
  

  @mysqli_stmt_close($stmt);
  mysqli_close($link);
  $times['overall'] = (microtime(true) - $times['overall']);
  
} while (false);
?>