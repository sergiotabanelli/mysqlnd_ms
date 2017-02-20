<?PHP
require_once('config.php');
require_once('abstract.php');
require_once('rb_storage_db_normal.php');
require_once('rb_renderer_normal.php');

class rb_testrunner_normal extends rb_testrunner {

  protected $storage;
  protected $renderer;

  public function runTest($file, $binary, $options) {

    if ($options['verbose']) {
      printf("  ... running file '%s'\n", $file);
      flush();
    }

    // Ugly, but things once worked very different before ext/mysqlnd development was cancelled
    // When ext/mysqlnd still existed this all was a beautiful, little include() ...
    $code = sprintf('<?php%s', "\n");
    $code.= sprintf('set_time_limit(0);%s', "\n");
    $code.= sprintf('ini_set("memory_limit", -1);%s', "\n");
    $code.= sprintf('$flag_original_code = true;%s', "\n");
    $code.= sprintf('$host = "%s";%s', RB_DB_HOST, "\n");
    $code.= sprintf('$user = "%s";%s', RB_DB_USER, "\n");
    $code.= sprintf('$passwd = "%s";%s', RB_DB_PASSWD, "\n");
    $code.= sprintf('$db= "%s";%s', RB_DB_DB, "\n");
    $code.= sprintf('$port= "%s";%s', RB_DB_PORT, "\n");
    $code.= sprintf('$socket= "%s";%s', RB_DB_SOCKET, "\n");
    $code.= sprintf('$engine= "%s";%s', RB_DB_ENGINE, "\n");
    $code.= sprintf('%s', "\n");
    $code.= sprintf('$times = $errors = array();%s', "\n");
    $code.= sprintf('include("%s");%s', $file, "\n");
    $code.= sprintf('$all = array("times" => $times, "errors" => $errors, "memory" => (isset($memory)) ? $memory : NULL);%s', "\n");
    $code.= sprintf('$fp = fopen("%s" . ".res", "w");%s', $file, "\n");
    $code.= sprintf('fwrite($fp, serialize($all));%s', "\n");
    $code.= sprintf('fclose($fp);%s', "\n");
    $code.= sprintf('print "done!";%s', "\n");
    $code.= sprintf('?>%s', "\n");

    $mycode = str_replace('.', '_', $file) . '_run_normal.php';
    @unlink($mycode);
    $results = sprintf('%s.res', $file);
    @unlink($results);    
    if (!$fp = fopen($mycode, 'w'))
      throw new Exception("Cannot create temporary file\n");

    fwrite($fp, $code);
    fclose($fp);

    $cmd = sprintf('%s %s -f %s', $binary['binary'], (is_null($binary['ini'])) ? '' : sprintf(' -c %s ', $binary['ini']), $mycode);
    if ($options['verbose']) {
      printf("  ...'%s'\n", $cmd);
      flush();
    }
    exec($cmd, $output, $ret);
    if ($ret !== 0 || empty($output) || $output[0] != 'done!') {      
      throw new Exception(sprintf("Cannot fetch output from %s, return code: %d, output: %s\n", $binary['binary'], $ret, implode('', $output)));      
    }

    if (!$tmp = file_get_contents($results))
      throw new Exception("Cannot read data exchange file");

    if (!$all = unserialize($tmp))
      throw new Exception("Cannot unserialize data exchange file contents");

    unlink($results);
    unlink($mycode);

    if (!empty($all['errors'])) 
      throw new Exception(sprintf("Errors during bench run: %s\n", implode("\n", $all['errors'])));

    if ($options['verbose']) {
      printf("  ... microbench finished, %d errors, %d times recorded, %d mem info recorded\n", 
        count($all['errors']), count($all['times']), count($all['memory']));
      flush();
    }

    return $all;
  }

  public function saveResults($run_label, $run_datetime, $file, $binary_file, $binary_label, $data) {

    $this->initStorage();
    $this->storage->init();
    $this->storage->save($run_label, $run_datetime, $file, $binary_file, $binary_label, $data);

  }

  public function getRenderer() {

    if (!$this->renderer) {
      $this->initStorage();
      $this->renderer = new rb_renderer_normal($this->storage);
    }

    return $this->renderer;
  }

  public function clearOldResults() {}

  protected function initStorage() {
    if (is_null($this->storage))
      $this->storage = new rb_storage_db_normal();
  }

}
?>