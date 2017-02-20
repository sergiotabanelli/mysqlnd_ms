<?PHP
require_once('config.php');
require_once('rb_testrunner_normal.php');

class rb_main {

  protected $run_label = null;
  protected $run_datetime = null;
  protected $options = array();
  protected $files = array();

  public function __construct($run_label = null, $run_datetime = null) {

    $this->run_label = ($run_label) ? $run_label : date('Y-m-d H:i:s');
    $this->run_datetime = ($run_datetime) ? $run_datetime : date('Y-m-d H:i:s');
    $this->initOptions();

  }

  public function runTests($save_results, $render = true) {

    if (!$testrunner = $this->createTestrunner())
      throw new Exception(sprintf("No test runner specified!\n"));

    sort($this->files);

    foreach ($this->options['binaries'] as $binary_label => $binary) {
      if (!file_exists($binary['binary']))
        throw new Exception(sprintf("Cannot find binary '%s' => '%s'\n", $binary_label, $binary['binary']));

      if (!is_null($binary['ini']) && !file_exists($binary['ini']))
        throw new Exception(sprintf("Cannot find ini '%s' => '%s'\n", $binary_label, $binary['ini']));

      foreach ($testrunner as $k => $runner) {

        if ($this->options['verbose']) {
          printf("\n");
          printf("[%s] Starting run for binary '%s' and runner '%s'...\n", 
            date('Y-m-d H:i:s'),
            $binary_label, 
            get_class($runner));
          flush();
        }

        foreach ($this->files as $k => $file) {
          $results = $runner->runTest($file, $binary, $this->options);
          if ($save_results)
            $runner->saveResults($this->run_label, $this->run_datetime, $file, $binary['binary'], $binary_label, $results);
        }

      }
    }

    if ($render) {

      if ($this->options['verbose']) {
        printf("\n");
        printf("[%s] Starting to render results...\n", 
          date('Y-m-d H:i:s'));
        flush();
      }

      foreach ($testrunner as $k => $runner) {
        $runner->getRenderer()->renderOverviewTxt($this->run_label, $this->run_datetime);
        if (RB_DEFAULT_OUTPUT_HTML) {
            $runner->getRenderer()->renderOverviewHTML($this->run_label, $this->run_datetime);
            $runner->getRenderer()->renderOverviewWiki($this->run_label, $this->run_datetime);
        }

        foreach ($this->files as $k => $file) {
          $runner->getRenderer()->renderRunTxt($this->run_label, $file, $this->run_datetime);
          if (RB_DEFAULT_OUTPUT_HTML) {
            $runner->getRenderer()->renderRunHTML($this->run_label, $file, $this->run_datetime);
            $runner->getRenderer()->renderRunWiki($this->run_label, $file, $this->run_datetime);
          }
        }
      }

    }

    return true;
  }

  public function createTestrunner() {

    $testrunner = array();
    if ($this->options['run_normal'])
      $testrunner[] = new rb_testrunner_normal();
    /*
    if ($this->options['run_oprofile'])
      $this->testrunne[] = new rb_testrunner_oprofile();
    */

    return $testrunner;
  }

  public function parseArgs($argc, $argv) {

    $this->initOptions();
    $this->files = array();

    // Fetch options specified directly after the run-bench.php script name
    // which we are going to shiff off the array first
    array_shift($argv);
    if (empty($argv))
      throw new Exception(sprintf("Missing file and/or directory specification!"));


    while (!empty($argv) && substr($argv[0], 0, 1) == '-') {  

      $opt = substr($argv[0], 0, 2);
      switch ($opt) {
        case '-v':
          $this->options['verbose'] = true;
          array_shift($argv); 
          break;

        case '-h':
          return sprintf("No error, but you asked for help... :-)\n");
          break;

        case '-q':
          $this->options['output_html'] = false;
          $this->options['output_txt'] = false;
          $this->options['verbose'] = false;
          array_shift($argv);
          break;

        case '-p':
          $this->options['oprofile'] = true;
          array_shift($argv);
          break;

        default:
          break;

      }

    }

    if (empty($argv))
      throw new Exception(sprintf("Missing file and/or directory specification!"));

    while ($tmp = array_shift($argv)) {
      if (is_file($tmp) && !$this->addFile($tmp, $this->options['name']))
        throw new Exception(sprintf("Cannot read '%s'\n", $tmp));
      if (is_dir($tmp) && !$this->addDir($tmp, $this->options['name']))
        throw new Exception(sprintf("Cannot read '%s'\n", $tmp));
    }

    if (empty($this->files))
      throw new Exception(sprintf("No benchmark files found!\n"));

  }


  public function getConfig() {

    return array(
        'RB_DB_USER'     => RB_DB_USER,
        'RB_DB_PASSWD'   => RB_DB_PASSWD,
        'RB_DB_DB'       => RB_DB_DB,
        'RB_DB_HOST'     => RB_DB_HOST,
        'RB_DB_PORT'     => RB_DB_PORT,
        'RB_DB_SOCKET'   => RB_DB_SOCKET,
        'RB_DB_ENGINE'   => RB_DB_ENGINE,
    );
  }


  public function getCommandlineSyntax() {

    $syntax = "Syntax:\n";
    $syntax.= "  program [options] dir_or_file [dir_or_file [dir_or_file ...]]\n";
    $syntax.= "\n";
    $syntax.= "Options:\n";    
    $syntax.= "  -v                - Verbose\n";
    $syntax.= "  -h                - Help\n";
    $syntax.= "  -q                - Quiet, suppress output\n";    
    // $syntax.= "  -o html[=dir]  - Create HTML output. Default: " . ((RB_DEFAULT_OUTPUT_HTML) ? 'on' : 'off') . "(see config.php)\n";
    // $syntax.= "  -o txt         - Print textual results. Default: on\n";
    // $syntax.= "  -b label=path  - Test binary [path], use the [label] to refer to it\n";
    // $syntax.= "  -n name        - File name pattern. Default: .*\\.php\n";
    $syntax.= "  -p                - Profile with oprofile (see config.php)\n";
    $syntax.= "\n";
    // $syntax.= "Watch the defaults defined in config.php!\n\n";
    /*
    $syntax.= "Examples:\n";
    $syntax.="\n";
    $syntax.= "  -o html=/usr/local/apache/htdocs/rb/\n";
    $syntax.= "  Write HTML output to /usr/local/apache/htdocs/rb/.\n";
    $syntax.= "\n";
    $syntax.= "\n";
    $syntax.= "  -b mysqlnd=../php/sapi/cli/php -b libmysql=/usr/bin/php -o html\n";    
    $syntax.= "  Run benchmarks with two binaries, label results for binaries\n";
    $syntax.= "  with 'mysqlnd' and 'libmysql'. Create HTML output in the current directory\n";    
    */
    return $syntax;
  }

  protected function initOptions() {

    $this->options = array(
      'output_html'   => RB_DEFAULT_OUTPUT_HTML,
      'output_txt'    => true,
      'recursive'     => true,
      'verbose'       => false,
      'run_oprofile'  => false,
      'run_normal'    => true,
      'storage_db'    => RB_DEFAULT_STORAGE_DB,
      'name'          => '/.*\.php$/i',
      'oprofile'      => RB_DEFAULT_OPROFILE,
      'binaries'      => $GLOBALS['rb_binaries'],
    );
  }

  protected function addDir($dirname, $pattern) {

    if (!is_dir($dirname))
      return false;

    try {
      $dir = new DirectoryIterator($dirname);
      foreach ($dir as $file) {
        if ($dir->isDot())
          continue;

        $ffile = $file->getPathname();
        if ($file->isDir() && !$this->addDir($ffile, $pattern))
          return false;

        $this->addFile($ffile, $pattern);
      }
    } catch (Exception $e) {
      return false;
    }

    return true;
  }

  protected function addFile($filename, $pattern) {
    if (!is_file($filename))
      return false;

    if (!is_readable($filename))
      return false;

    if (!preg_match($pattern, $filename))
      return false;

    $this->files[] = realpath($filename);

    return true;
  }


} // end class rb_main
?>
