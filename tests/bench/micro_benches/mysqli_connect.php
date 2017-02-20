<?PHP
$runs   = 10000;
$times  = array(
  'Connect TCP/IP success'      => 0,
  'Connect TCP/IP failure'      => 0,
  'Connect Socket success'      => 0,
  'Connect Socket failure'      => 0,
);
$description = 'Connect without closing.';
$errors = array();

$times['Connect TCP/IP success'] = microtime(true);
for ($i = 0; $i < $runs; $i++) {
  if (!$link = @mysqli_connect($host, $user, $passwd, $db, $port, $socket)) {
    $errors[] = sprintf("'Connect TCP/IP success' failure [original code: %s]", 
                  ($flag_original_code) ? 'yes' : 'no');
    break;
  }
}
$times['Connect TCP/IP success'] = microtime(true) - $times['Connect TCP/IP success'];

$times['Connect TCP/IP failure'] = microtime(true);
for ($i = 0; $i < $runs; $i++) {
  if ($link = @mysqli_connect($host, $user, $passwd . '_unknown', $db, $port, $socket)) {
    $errors[] = sprintf("'Connect TCP/IP failure' failure [original code: %s]",
                  ($flag_original_code) ? 'yes' : 'no');
    break;
  }
}
$times['Connect TCP/IP failure'] = microtime(true) - $times['Connect TCP/IP failure']; 

$times['Connect Socket success'] = microtime(true);
for ($i = 0; $i < $runs; $i++) {
  if (!$link = @mysqli_connect($host, $user, $passwd, $db, $port, $socket)) {
    $errors[] = sprintf("'Connect Socket success' failure [original code: %s]",
                  ($flag_original_code) ? 'yes' : 'no');
    break;
  }
}
$times['Connect Socket success'] = microtime(true) - $times['Connect Socket success'];

$times['Connect Socket failure'] = microtime(true);
for ($i = 0; $i < $runs; $i++) {
  if ($link = @mysqli_connect($host, $user, $passwd . '_unknown', $db, $port, $socket)) {
    $errors[] = sprintf("'Connect Socket failure' failure [original code: %s]",
                  ($flag_original_code) ? 'yes' : 'no');
    break;
  }
}
$times['Connect Socket failure'] = microtime(true) - $times['Connect Socket failure'];

// sort by runtime in descending order
arsort($times);
?>