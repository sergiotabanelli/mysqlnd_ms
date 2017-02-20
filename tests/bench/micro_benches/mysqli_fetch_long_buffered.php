<?PHP
// measured in KB, that means 1 = 1024 bytes
$long_lengths   = array(1, 2, 3, 5, 10, 15, 20, 30, 50, 100, 200, 300, 500, 1000, 1500, 2000, 3000, 5000, 10000);
$rows = array(1, 10, 100);

foreach ($rows as $k => $num_rows) {
  foreach ($long_lengths as $k => $len) {
    $times[$num_rows . " row[s]: " . $len . " k SELECT overall"] = 0;
    $times[$num_rows . " row[s]: " . $len . " k SELECT query()"] = 0;
  }
}
$description = 'Connect, create n-rows with one m-kb LONG column, SELECT all (buffered), close. n and m vary.';
$errors = array();

do {

  if (!$link = mysqli_connect($host, $user, $passwd, $db, $port, $socket)) {
    $errors[] = sprintf("Connect failure (converted code: %s)\n", ($flag_original_code) ? 'no' : 'yes');
    break;
  }

  if (!mysqli_query($link, "DROP TABLE IF EXISTS test")) {
    $errors[] = sprintf("DROP TABLE failed (converted code: %s): [%d] %s\n",
      ($flag_original_code) ? 'no' : 'yes',
      mysqli_errno($link), mysqli_error($link));
    break;
  }

  $sql = "CREATE TABLE test(id INT NOT NULL PRIMARY KEY AUTO_INCREMENT, label LONGBLOB)";
  if (!mysqli_query($link, $sql)) {
    $errors[] = sprintf("%s failed (converted code: %s): [%d] %s\n",
      $sql,
      ($flag_original_code) ? 'no' : 'yes',
      mysqli_errno($link), mysqli_error($link));
    break;
  }

  sort($long_lengths);
  $max_len = end($long_lengths) * 1024;
  if (!mysqli_query($link, $sql = sprintf("SET GLOBAL max_allowed_packet = %d", $max_len + 100))) {
    $errors[] = sprintf("%s failed (converted code: %s): [%d] %s\n",
      $sql,
      ($flag_original_code) ? 'no' : 'yes',
      mysqli_errno($link), mysqli_error($link));
    break;
  }

  foreach ($rows as $k => $num_rows) {
    foreach ($long_lengths as $k => $len) {

      if (!mysqli_query($link, "DELETE FROM test")) {
        $errors[] = sprintf("DELETE failed (converted code: %s): [%d] %s\n",
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
        break 3;
      }

      $length_bytes = $len * 1024;
      $current_len = 20;

      if (!mysqli_query($link, "INSERT INTO test(label) VALUES ('12345678901234567890')")) {
        $errors[] = sprintf("INSERT failed (converted code: %s): [%d] %s\n",
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
        break 3;
      }
      $id = mysqli_insert_id($link);
      while ($current_len < ($length_bytes / 2)) {
        if (!mysqli_query($link, "UPDATE test SET label = CONCAT(label, label)")) {
            $errors[] = sprintf("UPDATE failed (converted code: %s): [%d] %s\n",
            ($flag_original_code) ? 'no' : 'yes',
            mysqli_errno($link), mysqli_error($link));
          break 4;
        }
        $current_len += $current_len;
      }

      if (!mysqli_query($link, sprintf("UPDATE test SET label = CONCAT(label, LEFT(label, %d))", ($length_bytes - $current_len)))) {
            $errors[] = sprintf("UPDATE failed (converted code: %s): [%d] %s\n",
            ($flag_original_code) ? 'no' : 'yes',
            mysqli_errno($link), mysqli_error($link));
        break 3;
      }
      for ($i = 1; $i < $num_rows; $i++) {
        if (!mysqli_query($link, sprintf("INSERT INTO test(label) SELECT label FROM test WHERE id = %d", $id))) {
            $errors[] = sprintf("INSERT rows failed (converted code: %s): [%d] %s\n",
            ($flag_original_code) ? 'no' : 'yes',
            mysqli_errno($link), mysqli_error($link));
          break 4;
        }
      }

      $times[$num_rows . " row[s]: " . $len . " k SELECT overall"] = $start = microtime(true);
      if (!$res = mysqli_query($link, "SELECT id, label FROM test")) {
        $errors[] = sprintf("SELECT failed (converted code: %s): [%d] %s\n",
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
        break 3;
      }
      $times[$num_rows . " row[s]: " . $len . " k SELECT query()"] += (microtime(true) - $start);
      while ($row = mysqli_fetch_assoc($res))
        if (strlen($row['label']) != $length_bytes) {
          $errors[] = sprintf("fetch() failed (converted code: %s): [%d] %s\n",
          ($flag_original_code) ? 'no' : 'yes',
          mysqli_errno($link), mysqli_error($link));
          break 4;
        }


      mysqli_free_result($res);
      $times[$num_rows . " row[s]: " . $len . " k SELECT overall"] = microtime(true) - $times[$num_rows . " row[s]: " . $len . " k SELECT overall"];

    }
  }

  mysqli_close($link);

} while (false);
?>