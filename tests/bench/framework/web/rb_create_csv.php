<?php
require_once('../config.php');
require_once('../rb_storage_db_normal.php');

try {

  $storage = new rb_storage_db_normal();
  $run_id     = (isset($_REQUEST['run_id'])) ? $_REQUEST['run_id'] : NULL;
  $runlist    = $storage->getRunList();

  $binaries   = (isset($_REQUEST['binaries'])) ? $_REQUEST['binaries'] : NULL;
  $binarylist = ($run_id) ? $storage->getBinaryList($run_id) : NULL;

  $labels     = (isset($_REQUEST['labels'])) ? $_REQUEST['labels'] : NULL;
  $labellist  = ($binaries) ? $storage->getMeasuredTimeLabels($run_id) : NULL;

  if ($run_id && $binaries && $labels) {

    $lines = array();
    $sep = ';';

    $first = true;
    foreach ($binaries as $k => $run_id) {
      $run_info = $storage->getRunInfoByRunID($run_id);
      if ($first) {
        $line = '';
        foreach ($run_info as $label => $v)
          $line .= sprintf("%s%s", $label, $sep);
        $line = substr($line, 0, strlen($sep) * -1);
        $lines[] = $line;
        $first = false;
      }
      $lines[] = implode($sep, $run_info);
    }
    $lines[] = '';

    $file = sys_get_temp_dir() . '/version.php';
    $fp = fopen($file, 'w');
    fwrite($fp, '<?php printf("%s\n%s", PHP_VERSION, mysqli_get_client_info()); ?>');
    fclose($fp);
    $line1 = $line2 = $line3 = $line4 = '';
    $line_bin_label = $sep;
    $mysqlnd_run_id = null;
    foreach ($binaries as $k => $run_id) {
      foreach ($binarylist as $k => $binaryinfo) {
        if ($binaryinfo['run_id'] == $run_id) {
          $line1 .= sprintf("%s%s", $binaryinfo['binary_label'], $sep);
          $line2 .= sprintf("%s%s", $binaryinfo['binary_file'], $sep);
          $cmd = sprintf('%s -f %s', $binaryinfo['binary_file'], $file);
          $output = array();
          exec($cmd, $output);
          $line3 .= sprintf("%s%s", $output[0], $sep);
          $line4 .= sprintf("%s%s", $output[1], $sep);
          if (stristr($output[1], 'mysqlnd'))
            $mysqlnd_run_id = $run_id;
         }
      }
    }
    $line_bin_label .= substr($line1, 0, strlen($sep) * -1);
    $lines[] = substr($line1, 0, strlen($sep) * -1);
    $lines[] = substr($line2, 0, strlen($sep) * -1);
    $lines[] = substr($line3, 0, strlen($sep) * -1);
    $lines[] = substr($line4, 0, strlen($sep) * -1);
    $lines[] = '';
    if (!$mysqlnd_run_id) {
      reset($binaries);
      list($k, $mysqlnd_run_id) = each($binaries);
    }
    unlink($file);

    /* HACK */
    $runtimes = array();
    foreach ($labels as $k => $label) {
      foreach ($binaries as $k => $run_id) {
        $runtime = $storage->getRuntimeByRunIDAndLabel($run_id, $label);
        $runtimes[$label][$run_id] = $runtime['runtime'];
      }
    }

    $lines[] = $line_bin_label;
    foreach ($labels as $k => $label) {
      $fac = (0 == $runtimes[$label][$mysqlnd_run_id]) ? 0 : 100 / $runtimes[$label][$mysqlnd_run_id];
      $line = sprintf("%s%s", $label, $sep);
      foreach ($binaries as $k => $run_id) {
        if ($run_id == $mysqlnd_run_id) {
          if (0 == $fac)
            $line .= sprintf("%s%s", number_format(0, 3, ',', ''), $sep);
          else 
            $line .= sprintf("%s%s", number_format(100, 3, ',', ''), $sep);
        } else {
          $line .= sprintf("%s%s", number_format($runtimes[$label][$run_id] * $fac, 3, ',', ''), $sep);
        }
      }
      $lines[] = substr($line, 0, strlen($sep) * -1);
    }
    $lines[] = '';

    $lines[] = $line_bin_label;
    foreach ($labels as $k => $label) {
      $line = sprintf("%s%s", $label, $sep);
      foreach ($binaries as $k => $run_id) {
        $line .= sprintf("%s%s", str_replace('.', ',', sprintf("%f", $runtimes[$label][$run_id])), $sep);
      }
      $lines[] = substr($line, 0, strlen($sep) * -1);
    }
    $lines[] = '';

    $csv = implode("\n", $lines);

    header("Content-type: application/vnd.ms-excel");
    header("Content-Length: " . strlen($csv));
    header("Content-disposition:  attachment; filename=mysqlnd.csv");
    die($csv);

  }

} catch (Exception $e) {

  printf('<h3>Error</h3><p>%s</p>',
    nl2br(htmlspecialchars($e->getMessage())));

}
?>
<html>
  <head>
    <title>Bench: Extract CVS data for Excel and Co.</title>
  </head>
  <body>
    <form action="<?php print $PHP_SELF; ?>" method="GET">
    <font face="sans-serif">
      <h1>Extract CSV data for Excel and Co.</h1>
      <h3>Back</h3>
      <a href="index.html">Back to the overview page</a>
      <h3>Select Run, Binary and Measured Times</h3>
      <table cellspacing="2" cellpadding="2" border="1">
      <tr>
        <th right="left" valign="top">Run</th>
        <td align="left" valign="top">
        <select name="run_id" size="5">
        <?php
          foreach ($runlist as $k => $runinfo) {
            printf('<option value="%s" %s>%s - %s</option>',
              $runinfo['run_id'],
              ($run_id == $runinfo['run_id']) ? 'selected' : '',
              htmlspecialchars($runinfo['run']),
              htmlspecialchars($runinfo['bench_file']));
          }
        ?>
        </select>
        </td>
      </tr>
      <tr>
        <th right="left" valign="top">Binaries</th>
        <td align="left" valign="top">
        <?php
        if ($binarylist) {
        ?> 
        <select name="binaries[]" multiple="multiple" size="5">
        <?php
          foreach ($binarylist as $k => $binaryinfo) {
            printf('<option value="%s" %s>%s - %s</option>',
              $binaryinfo['run_id'],
              (is_null($binaries) || in_array($binaryinfo['run_id'], $binaries)) ? 'selected' : '',
              htmlspecialchars($binaryinfo['binary_label']),
              htmlspecialchars($binaryinfo['binary_file']));
          }
        ?>
        </select>
        <?php
        } else {
          printf("Select a run to get a list of binaries");
        }
        ?>
        </td>
      </tr>
      <tr>
        <th right="left" valign="top">Measured times</th>
        <td align="left" valign="top">
        <?php
        if ($labellist) {
        ?> 
        <select name="labels[]" multiple="multiple" size="10">
        <?php
          foreach ($labellist as $k => $labelinfo) {
            if (is_null($labels)) {
              $selected = (stristr($labelinfo['label'], 'overall')) ? 'selected' : '';
            } else {
              $selected = '';
              foreach ($labels as $k => $label)
                if ($labelinfo['label'] == $label) {
                  $selected = 'selected';
                  break;
                }
            }
            printf('<option value="%s" %s>%s</option>',
              $labelinfo['label'],
              $selected,
              htmlspecialchars($labelinfo['label']));
          }
        ?>
        </select>
        <?php
        } else {
          printf("Select a binary to get a list of measured times");
        }
        ?>
        </td>
      </tr>
      <tr>
        <td colspan="2" align="right"><input type="submit" value="Proceed"></td>
      </tr>
      </table>
    </font>
    </form>
  </body>
</html>