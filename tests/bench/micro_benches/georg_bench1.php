<?PHP
$times = $errors = array();
$runs = 20000;

$times = array(
  'overall'             => 0,
  'INSERT'              => 0,
  'Connect and SELECT'  => 0,
);


do {
  $times['overall'] = microtime(true);
    
  if (!$mysqli = new mysqli($host, $user, $passwd, $db, $port, $socket)) {
    $errors[] = sprintf("Cannot connect: [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
    break;
  }
  
  if (!$mysqli->query("DROP TABLE IF EXISTS t1")) {
    $errors[] = sprintf("Cannot DROP: [%d] %s\n", $mysqli->errno, $mysqli->error);
    break;
  }
  
  if (!$mysqli->query("CREATE TABLE t1 (a int not null auto_increment primary key, b varchar(48), c varchar(48), d varchar(48))")) {
    $errors[] = sprintf("Cannot CREATE: [%d] %s\n", $mysqli->errno, $mysqli->error);
    break;
  }

  $times['INSERT'] = microtime(true);
  for ($i = 0; $i < $runs; $i++) { 
	 $val = md5(microtime(true));
	 if (!$mysqli->query("INSERT INTO t1 VALUES (0, '$val', '$val', '$val')")) {
	   $errors[] = sprintf("Cannot INSERT: [%d] %s\n", $mysqli->errno, $mysqli->error);
      break;
	 }
  }
  $times['INSERT'] = microtime(true) - $times['INSERT'];
  $mysqli->close();

  $client = mysqli_get_client_info();
  $host = ((substr($client, 0, 7) == 'mysqlnd')) ? 'p:' . $host : $host;

  $times['Connect and SELECT'] = microtime(true);
  for ($i = 0; $i < $runs; $i++) {
    if (!$mysqli = new mysqli($host, $user, $passwd, $db, $port, $socket)) {
      $errors[] = sprintf("Cannot connect (2): [%d] %s\n", mysqli_connect_errno(), mysqli_connect_error());
      break 2;
    }
	  $val = md5(microtime());
	  if (!$result = $mysqli->query("SELECT * FROM t1 LIMIT 3")) {
      $errors[] = sprintf("Cannot SELECT: [%d] %s\n", $mysqli->errno, $mysqli->error);
      break 2;
	  }
	  $j = 0;
    while ($row = $result->fetch_row())
      $j++;
    if ($j != 3) {
      $errors[] = sprintf("Fetching failed: [%d] %s\n", $mysqli->errno, $mysqli->error);
      break 2;
    }      
    $result->close();
	  $mysqli->close();
  }
  $times['Connect and SELECT'] = microtime(true) - $times['Connect and SELECT'];
  $times['overall'] = microtime(true) - $times['overall'];
  
} while (false);


?>