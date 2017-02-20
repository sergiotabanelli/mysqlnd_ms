<?PHP
require_once('config.php');
require_once('abstract.php');

class rb_renderer_normal extends rb_renderer {

  protected $link = null;
  protected $storage = null;

  public function __construct($storage) {

    $this->storage = $storage;

  }

  public function renderRunTxt($run_label, $file, $run_datetime) {

    $binaries = $this->storage->getBinaries($run_label, $file, $run_datetime);
    $num_repeat = (41 + count($binaries) * 17);

    printf("\n");
    printf("%s\n", str_repeat('=', $num_repeat));
    printf("%-16s: %-40s\n", "File", basename($file));    
    printf("%s\n", str_repeat('=', $num_repeat));
    printf("\n");

    if (count($binaries) > 4) {

      reset($binaries);
      list($master_bin_label, $master_bin_file) = each($binaries);
      $master_times = $this->storage->getRuntimes($run_label, $file, $run_datetime, $master_bin_label, $master_bin_file);       
      $this->renderTimesTxt($master_times, $master_bin_label, $master_bin_label, $master_bin_file, $master_times);

      $this->renderRunInfoTxt($this->storage->getRunInfo($run_label, $file, $run_datetime));

      foreach ($binaries as $binary_label => $binary_file) {      
        $times = $this->storage->getRuntimes($run_label, $file, $run_datetime, $binary_label, $binary_file);
        $this->renderTimesTxt($master_times, $master_bin_label, $binary_label, $binary_file, $times);
      }

      return;
    }

    $this->renderRunInfoTxt($this->storage->getRunInfo($run_label, $file, $run_datetime));

    reset($binaries);
    list($master_bin_label, $master_bin_file) = each($binaries);
    $all_runtimes[$master_bin_label] = $this->storage->getRuntimes($run_label, $file, $run_datetime, $master_bin_label, $master_bin_file);    

    while (list($bin_label, $bin_file) = each($binaries)) {
      $all_runtimes[$bin_label] = $this->storage->getRuntimes($run_label, $file, $run_datetime, $bin_label, $bin_file);
    }

    foreach($binaries as $bin_label => $bin_file) {
      printf("%-16s: %s\n", $bin_label, $bin_file);
    }
    printf("\n");
    printf("   WARNING: 'Total' SAYS NOTHING, CHECK '* overall' VALUES!\n");
    printf("\n");

    printf("%-40s ", "");
    foreach ($binaries as $bin_label => $bin_file) {
      printf("%-17s ", $bin_label);
    }
    printf("\n");
    printf("%s\n", str_repeat('-', $num_repeat));

    array_shift($binaries);

    foreach ($all_runtimes[$master_bin_label] as $runtime_label => $master_runtime) {
      printf("%-40s", $runtime_label);
      printf(" %03.4fs ( 100%%) ", $master_runtime);
      foreach ($binaries as $bin_label => $bin_file) {
        printf(" %03.4fs (%4d%%) ", 
          $all_runtimes[$bin_label][$runtime_label],
          (($master_runtime > 0) ? (100 / $master_runtime) * $all_runtimes[$bin_label][$runtime_label] : 0));
      }
      printf("\n");
    }
    printf("%s\n", str_repeat('=', $num_repeat));    
    printf("%-40s", "Total");
    $master_sum = array_sum($all_runtimes[$master_bin_label]);
    printf(" %03.4fs ( 100%%) ", $master_sum);
    foreach ($binaries as $bin_label => $bin_file) {
      printf(" %03.4fs (%4d%%) ", 
          array_sum($all_runtimes[$bin_label]),
          (($master_sum > 0) ? (100 / $master_sum) * array_sum($all_runtimes[$bin_label]) : 0));
    }
    printf("\n");
    printf("%s\n", str_repeat('-', $num_repeat));     
    printf("\n");
  }


  public function renderOverviewTxt($run_label, $run_datetime) {

    $fastest = $this->storage->getFastestBinaries($run_label, $run_datetime);
    list($file, ) = each($fastest);
    $binaries = $this->storage->getBinaries($run_label, $file, $run_datetime);
    $num_repeat = 44 + count($binaries) * 16;

    printf("%s\n", str_repeat("=", $num_repeat));
    printf("Summary %s (%s)\n", $run_label, $run_datetime);
    printf("%s\n", str_repeat("=", $num_repeat));
    printf("\n");

    $clean_stats = array();
    foreach ($binaries as $bin_label => $bin_file) {
      printf("%-20s: %s\n", $bin_label, substr($bin_file, 0, $num_repeat - 20)); 
      $clean_stats[$bin_label] = 0;
    }
    printf("\n");

    printf("  Counters show for how many time figures a certain binary\n");
    printf("  has been the fastest. For example, the following says that\n");
    printf("  three figures are available for 'file.php' and for one figure\n");
    printf("  'Binary A' was faster than the other binaries and for the\n");
    printf("  other two figures 'Binary C' was the fastest:\n\n");
    printf("                          Binary A     Binary B    Binary C\n");
    printf("  ---------------------------------------------------------\n");
    printf("  file.php (03 = 100%%):    1 (33%%)       0 (0%%)     2 (66%%)\n");
    printf("\n\n");

    printf("%-44s", "");
    foreach ($binaries as $bin_label => $bin_file)
      printf("%16s", $bin_label);
    printf("\n");
    printf("%s\n", str_repeat("-", $num_repeat));

    foreach ($fastest as $file => $filestats) {
      $stats = $clean_stats;
      foreach ($filestats as $time_label => $bin_label)
        $stats[$bin_label]++;
      $total = array_sum($stats);
      printf("%-30s (%02d = 100%%): ", substr(basename($file), 0, 30), $total);
      foreach ($binaries as $bin_label => $bin_file)
        printf("%16s", sprintf("%d (%3d%%)", $stats[$bin_label], ($total) ? (100 / $total) * $stats[$bin_label] : 0));
      printf("\n");
    }
    printf("%s\n", str_repeat("-", $num_repeat));
    printf("\n");   

  }

  public function renderRunHTML($run_label, $file, $run_datetime) {

    if (!file_exists(RB_OUTPUT_HTML_DIR) && !mkdir(RB_OUTPUT_HTML_DIR, 0644, true))      
      throw new Exception(sprintf("Cannot create output directory '%s'", RB_OUTPUT_HTML_DIR));

    $htmlfile = sprintf('%s/%s.html', RB_OUTPUT_HTML_DIR, str_replace('.', '_', basename($file)));
    if (!$fp = fopen($htmlfile, 'w'))
      throw new Exception(sprintf("Cannot open output file '%s'", $htmlfile));

    $info = $this->storage->getRunInfo($run_label, $file, $run_datetime);
    $binaries = $this->storage->getBinaries($run_label, $file, $run_datetime);

    $short_file = basename($file);
    $html = <<<EOT
<html>
  <head>
    <title>Bench: $run_label ($run_datetime) $file</title>
  </head>
  <body>
    <font face="sans-serif">
      <h1><a href="index.html">$run_label ($run_datetime)</a></h1>
      <h2>Bench: $short_file</h2>      
      <h3>Run information</h3>
      <table cellspacing="2" cellpadding="2" border="1">
        <tr>
          <th align="left" valign="top">Run-ID</th>
          <td align="left" valign="top">{$info['run_id']}</td>
        </tr>
        <tr>
          <th align="left" valign="top">Label</th>
          <td align="left" valign="top">{$info['label']}</td>
        </tr>
        <tr>
          <th align="left" valign="top">Datetime</th>
          <td align="left" valign="top">{$info['run']}</td>
        </tr>
        <tr>
          <th align="left" valign="top">System</th>
          <td align="left" valign="top">{$info['sysinfo']}</td>
        </tr>
      </table>
EOT;

    reset($binaries);
    list($master_bin_label, $master_bin_file) = each($binaries);
    $all_runtimes[$master_bin_label] = $this->storage->getRuntimes($run_label, $file, $run_datetime, $master_bin_label, $master_bin_file);    
    while (list($bin_label, $bin_file) = each($binaries)) {
      $all_runtimes[$bin_label] = $this->storage->getRuntimes($run_label, $file, $run_datetime, $bin_label, $bin_file);
    }

    $html.= <<<EOT
      <h3>Binaries</h3>
      <table cellspacing="2" cellpadding="2" border="1">      
EOT;
    foreach ($binaries as $bin_label => $bin_file) {      
      $anchor = urlencode($bin_label);
      $html.= <<<EOT
        <tr>
          <th align="left" valign="top"><a href="#$anchor">$bin_label</a></th>
          <td align="left" valign="top"><a href="#$anchor">$bin_file</a></td>
        </tr>
EOT;
    }

    $html .= '</table>' . "\n";

    $html .= '<h3>Run times</h3>' . "\n";
    $html .= '<table cellspacing="2" cellpadding="2" border="1">' . "\n";
    $html .= '<tr bgcolor="#e0e0e0">' . "\n";
    $html .= '<th>&nbsp;</th>' . "\n";
    foreach ($binaries as $bin_label => $bin_file) {
      $html .= sprintf('<th align="center" valign="top" colspan="2">%s</th>', $bin_label);
    }
    $html .= '</tr>' . "\n";
    array_shift($binaries);
    $i = 0;
    foreach ($all_runtimes[$master_bin_label] as $runtime_label => $master_runtime) {
      $i++;
      if ($i % 2 == 0) {
        $html .= '<tr bgcolor="#f0f0f0">' . "\n";
      } else {
        $html .= '<tr>' . "\n";
      }
      $html .= sprintf('<th align="left" valign="top">%s</th>', $runtime_label);
      $html .= sprintf('<td align="right" valign="top">%3.5fs</td>', $master_runtime);
      $html .= sprintf('<td align="right" valign="top">(100%%)</td>');
      foreach ($binaries as $bin_label => $bin_file) {
        $html .= sprintf('<td align="right" valign="top">%3.5fs</td>', $all_runtimes[$bin_label][$runtime_label]);
        $html .= sprintf('<td align="right" valign="top">(%4d%%)</td>', (($master_runtime > 0) ? (100 / $master_runtime) * $all_runtimes[$bin_label][$runtime_label] : 0));
      }
      $html .= '</tr>' . "\n";
    }    
    $html .= '</table>' . "\n";

    $html.= <<<EOT
      <h3>Source code of the micro benchmark</h3>
      <p>
EOT;
    $html .= highlight_file($file, true);
    $html.= '</p>';

    $sql = sprintf('
SELECT
  r1.run_id AS run_id,
  r1.run AS run,    
  RIGHT(r1.file, 40) AS file,
  r1.binary_label AS binary_label,
  t1.label AS label,
  t1.runtime AS runtime
FROM
  rb_res_run AS r1
  INNER JOIN rb_res_normal_times AS t1 
    ON (r1.run_id = t1.fk_run_id)
WHERE
  r1.run_id = %d AND
  r1.file = "%s"
ORDER BY 
  t1.label', 
    $info['run_id'],
    $file); 

    $html.= <<<EOT
      <h3>Database</h3>
      <p>
      Use the following SQL statement as a starting point, if you
      want to analyze the results manually.
      </p>
      <p>
      <pre>
$sql
      </pre>
      </p>
EOT;

    $html .= <<<EOT
      <h3>php -i for every binary</h3>
      <table cellspacing="2" cellpadding="2" border="1"> 
EOT;

    $binaries = array_merge(array($master_bin_label => $master_bin_file), $binaries);
    $i = 0;
    foreach ($binaries as $bin_label => $bin_file) {
      $color = (++$i % 2) ? ' bgcolor="#f0f0f0" ' : '';
      $anchor = urlencode($bin_label);
      $cmd = sprintf("%s -i", $bin_file);
      $output = array();
      $ret = 0;
      exec($cmd, $output, $ret);
      $output = htmlspecialchars(implode("\n", $output));
      $html.= <<<EOT
        <tr $color>
          <th align="left" valign="top"><a name="$anchor">$bin_label ($bin_file)</a></th>          
        </tr>
        <tr>
          <td align="left" valign="top"><pre><code>$output</code></pre></td>
        </tr>
EOT;

    }

    $html.= <<<EOT
      </table>
    </font>
  </body>
</html>
EOT;

    fwrite($fp, $html);
    fclose($fp);

  }

  public function renderOverviewHTML($run_label, $run_datetime) {


    $fastest = $this->storage->getFastestBinaries($run_label, $run_datetime);
    list($file, ) = each($fastest);
    $binaries = $this->storage->getBinaries($run_label, $file, $run_datetime);

    if (!file_exists(RB_OUTPUT_HTML_DIR) && !mkdir(RB_OUTPUT_HTML_DIR, 0755, true))
      throw new Exception(sprintf("Cannot create output directory '%s'", RB_OUTPUT_HTML_DIR));

    $htmlfile = sprintf('%s/index.html', RB_OUTPUT_HTML_DIR);
    if (!$fp = fopen($htmlfile, 'w'))
      throw new Exception(sprintf("Cannot open output file '%s'", $htmlfile));

    $html = <<<EOT
<html>
  <head>
    <title>Bench: $run_label ($run_datetime) $file</title>
  </head>
  <body>
    <font face="sans-serif">
      <h1>$run_label ($run_datetime)</h1>
      <h3>Binaries</h3>
      <table cellspacing="2" cellpadding="2" border="1">
EOT;

    $clean_stats = array();
    foreach ($binaries as $bin_label => $bin_file) {
      $anchor = urlencode($bin_label);
      $html .= <<<EOT
      <tr>
        <th align="left" valign="top"><a href="#$anchor">$bin_label</a></th>
        <td align="left" valign="top"><a href="#$anchor">$bin_file</a></td>
      </tr>
EOT;
      $clean_stats[$bin_label] = 0;
    }
    $html .= <<<EOT
      </table>
      <h3>Data export</h3>
      <a href="rb_create_csv.php">Export test results</a>
      <h3>Summary</h3>
      <p>
      Counters show for how many time figures a certain binary
      has been the fastest. For example, the following says that
      three figures are available for 'file.php' and for one figure
      'Binary A' was faster than the other binaries and for the
      other two figures 'Binary C' was the fastest:
      <table cellspacing="2" cellpadding="2" border="1">
        <tr>
          <td>&nbsp;</td>
          <th align="left" valign="top">Binary A</th>
          <th align="left" valign="top">Binary B</th>
          <th align="left" valign="top">Binary C</th>
        </tr>
        <tr>
          <td align="left valign="top">file.php</td>
          <td align="right" valign="top">1 (33%)</td>
          <td align="right" valign="top">0 (0%)</td>
          <td align="right" valign="top">2 (66%)</td>
        </tr>
      </table>      
      </p>
      <table cellspacing="2" cellpadding="2" border="1">
        <tr>
          <td colspan="3">&nbsp;</td>
EOT;

    foreach ($binaries as $bin_label => $bin_file)
      $html .= sprintf('<th colspan="2" align="left" valign="top">%s</th>', $bin_label);
 
    $html .= '</tr>' . "\n";

    $i = 0;
    foreach ($fastest as $file => $filestats) {

      $stats = $clean_stats;
      $color = (++$i % 2) ? ' bgcolor="#f0f0f0" ' : '';

      foreach ($filestats as $time_label => $bin_label)
        $stats[$bin_label]++;

      $total = array_sum($stats);
      $html .= sprintf('<tr%s><th align="left" valign="top">
                        <a href="%s.html">%s</a></th>
                        <td align="right" valign="top">%d</td>
                        <td align="right" valign="top">100%%</td>',
                        $color,
                        str_replace('.', '_', basename($file)),
                        basename($file), 
                        $total);

      foreach ($binaries as $bin_label => $bin_file) {
        $html .= sprintf('<td align="right" valign="top">%d</td><td align="right" valign="top">%3d%%</td>',
                  $stats[$bin_label], ($total) ? (100 / $total) * $stats[$bin_label] : 0);
      }
      $html .= '</tr>' . "\n";
    }

    $html.= <<<EOT
      <h3>php -i for every binary</h3>
      <table cellspacing="2" cellpadding="2" border="1"> 
EOT;

    $i = 0;
    foreach ($binaries as $bin_label => $bin_file) {
      $color = (++$i % 2) ? ' bgcolor="#f0f0f0" ' : '';
      $anchor = urlencode($bin_label);
      $cmd = sprintf("%s -i", $bin_file);
      $output = array();
      $ret = 0;
      exec($cmd, $output, $ret);
      $output = htmlspecialchars(implode("\n", $output));

      $html.= <<<EOT
        <tr $color>
          <th align="left" valign="top"><a name="$anchor">$bin_label ($bin_file)</a></th>
        </tr>
        <tr>
          <td align="left" valign="top"><pre><code>$output</code></pre></td>
        </tr>
EOT;

    }

    $html.= <<<EOT
      </table>
    </font>
  </body>
</html>
EOT;

    fwrite($fp, $html);
    fclose($fp);

  }

  public function renderRunWiki($run_label, $file, $run_datetime) {

    $this->renderRunHTML($run_label, $file, $run_datetime);
    $htmlfile = sprintf('%s/%s.html', RB_OUTPUT_HTML_DIR, str_replace('.', '_', basename($file)));
    $wikifile = sprintf('%s/%s_wiki.txt', RB_OUTPUT_HTML_DIR, str_replace('.', '_', basename($file)));
    preg_match_all("=<body[^>]*>(.*)</body>=siU", file_get_contents($htmlfile), $a);
    $body = explode("\n", $a[1][0]);
    foreach ($body as $k => $line)
      $body[$k] = '|' . str_replace('|', '', $line) . "\n";

    $fp = fopen($wikifile, 'w');
    fwrite($fp, implode('', $body));
    fclose($fp);

  }

  public function renderOverviewWiki($run_label, $run_datetime) {

    $this->renderOverviewHTML($run_label, $run_datetime);
    $htmlfile = sprintf('%s/index.html', RB_OUTPUT_HTML_DIR);
    $wikifile = sprintf('%s/index_wiki.txt', RB_OUTPUT_HTML_DIR);
    preg_match_all("=<body[^>]*>(.*)</body>=siU", file_get_contents($htmlfile), $a);
    $body = explode("\n", $a[1][0]);
    foreach ($body as $k => $line)
      $body[$k] = '|' . str_replace('|', '', $line) . "\n";

    $fp = fopen($wikifile, 'w');
    fwrite($fp, implode('', $body));
    fclose($fp);

  }

  protected function renderTimesTxt($master_times, $master_bin_label, $binary_label, $binary_file, $times) {

    $master_sum = array_sum($master_times);
    $times_sum = array_sum($times);

    printf("Binary   : %-20s %3.5fs (vs. %-20s: %3.5fs = %02d%%)\n",
      $binary_label, 
      $times_sum,
      $master_bin_label,
      $master_sum,
      (($master_sum != 0) ? ((100 / $master_sum) * $times_sum) : 0)
    );

    printf("\n");

    foreach ($times as $label => $runtime) {
      printf("%-40s %3.5fs (%3.5fs = %02d%%)\n",
        $label, 
        $runtime,
        $master_times[$label],
        (($master_times[$label] != 0) ? (100 / $master_times[$label]) * $runtime : 0));
    }
  }


}
?>