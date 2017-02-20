<?PHP
require_once('config.php');

abstract class rb_testrunner {

  abstract public function runTest($file, $binary, $options);
  abstract public function saveResults($label, $rundatetime, $file, $binary_file, $binary_label, $results);
  abstract public function getRenderer();
  abstract public function clearOldResults();

}

abstract class rb_renderer {

  abstract public function renderRunTxt($run_label, $file, $run_datetime);
  abstract public function renderOverviewTxt($run_label, $run_datetime);
  abstract public function renderRunHTML($run_label, $file, $run_datetime);
  abstract public function renderOverviewHTML($run_label, $run_datetime);
  abstract public function renderRunWiki($run_label, $file, $run_datetime);

  public function renderRunInfoTxt($info) {

    printf("%-16s: %s on %s\n", "Run", $info['label'], $info['run']);
    printf("%-16s: %s\n", "System", $info['sysinfo']);

  }

}

abstract class rb_storage_db {

  protected $link = null;

  public function __construct() {

    if (!$this->link = mysqli_connect(RB_DB_HOST, RB_DB_USER, RB_DB_PASSWD, RB_DB_DB, RB_DB_PORT, RB_DB_SOCKET)) {
      throw new Exception(sprintf('Cannot connect to database, [%d] %s\n', mysqli_connect_errno(), mysqli_connect_error()));
    }

  }

  public function init() {

    if (!mysqli_query($this->link, "
        CREATE TABLE IF NOT EXISTS rb_res_run (
            run_id INT AUTO_INCREMENT PRIMARY KEY NOT NULL,
            run datetime not null,
            file varchar(255) not null,
            label varchar(255) not null,
            sysinfo varchar(255) not null,
            binary_file varchar(255) not null,
            binary_label varchar(255) not null,
            unique index(run, file, label, binary_file, binary_label),
            index(binary_label, file),
            index(binary_file, file)
        ) Engine = InnoDB")) {
      throw new Exception(sprintf("Cannot create table rb_res_run, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

  }

  abstract public function save($run_label, $run_datetime, $file, $binary_file, $binary_label, $data) ;

  public function delete($run_label) {

    if (!mysqli_query($this->link, sprintf("
        DELETE FROM rb_res_run WHERE label = '%s'",
          mysqli_real_escape_string($this->link, $run_label)))) {
      throw new Exception(sprintf("Cannot delete from rb_res_run, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

  }

  public function deleteBefore($run_datetime) {

    if (!mysqli_query($this->link, sprintf("
        DELETE FROM rb_res_run WHERE label <= '%s'",
          mysqli_real_escape_string($this->link, $run_datetime)))) {
      throw new Exception(sprintf("Cannot delete from rb_res_run, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

  }

  public function deleteAll() {

    if (!mysqli_query("DELETE FROM rb_res_run")) {
      throw new Exception(sprintf("Cannot delete from rb_res_run, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

  }

  public function reset() {

    if (!mysqli_query($this->link, "DROP TABLE IF EXISTS rb_res_run")) {
      throw new Exception(sprintf("Cannot drop table rb_res_run, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

  }

  public function getBinaries($run_label, $run_file, $run_datetime) {

    if (!$res = mysqli_query($this->link, $this->mySprintf('SELECT DISTINCT binary_file, binary_label
          FROM rb_res_run WHERE label = "%s" AND file = "%s" AND run = "%s" ORDER by binary_label, binary_file', $run_label, $run_file, $run_datetime))) {
      throw new Exception(sprintf("Cannot get distinct binaries, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

    $binaries = array();
    while ($row = mysqli_fetch_assoc($res)) {
      $binaries[$row['binary_label']] = $row['binary_file'];
    }

    mysqli_free_result($res);
    return $binaries;
  }

  public function getRunInfo($run_label, $run_file, $run_datetime) {

    if (!$res = mysqli_query($this->link, $this->mySprintf('SELECT * FROM rb_res_run WHERE label = "%s" AND file = "%s" AND run = "%s"', $run_label, $run_file, $run_datetime))) {
      throw new Exception(sprintf("Cannot get run info, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }
    if (!$info = mysqli_fetch_assoc($res))
      throw new Exception(sprintf("Cannot fetch run info,[%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));

    mysqli_free_result($res);
    return $info;
  }

  public function getRunInfoByRunID($run_id) {

    if (!$res = mysqli_query($this->link, sprintf('SELECT * FROM rb_res_run WHERE run_id = %d', $run_id))) {
      throw new Exception(sprintf("Cannot get run info, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }
    if (!$info = mysqli_fetch_assoc($res))
      throw new Exception(sprintf("Cannot fetch run info,[%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));

    mysqli_free_result($res);
    return $info;
  }

  public function getRunList($min_run_datetime = null, $sort = 'desc', $limit = 30) {

    if (is_null($min_run_datetime))
      $min_run_datetime = mktime(date('H'), date('i'), date('s'), date('m') - 1, date('d'), date('Y'));

    if (!$res = mysqli_query($this->link, $this->mySprintf('
            SELECT
              run_id,
              run,
              SUBSTRING(file FROM LOCATE("micro_benches", file) + 14) AS bench_file,
              label, binary_label, binary_file
            FROM
              rb_res_run
            GROUP BY
              CONCAT(run, file)
            ORDER BY run %s, file ASC
              LIMIT %d',
            ('desc' == strtolower($sort)) ? 'DESC' : 'ASC',
            ($limit > 0) ? $limit : 30))) {
      throw new Exception(sprintf("Cannot get list of runs, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

    $run_list = array();
    while ($info = mysqli_fetch_assoc($res))
      $run_list[] = $info;
    mysqli_free_result($res);

    return $run_list;
  }

  public function getBinaryList($run_id) {

    if (!$res = mysqli_query($this->link, sprintf('
          SELECT
            r1.run_id, r1.binary_label, r1.binary_file
          FROM
            rb_res_run AS r1,
            rb_res_run AS r2
          WHERE
            r1.run = r2.run AND
            r1.file = r2.file AND
            r2.run_id = %d
          ORDER BY
            r1.binary_label', $run_id))) {
      throw new Exception(sprintf("Cannot get list of binaries, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

    $binary_list = array();
    while ($info = mysqli_fetch_assoc($res))
      $binary_list[] = $info;

    return $binary_list;
  }

  public function getMeasuredTimeLabels($run_id) {

    if (!$res = mysqli_query($this->link, sprintf('
          SELECT
            DISTINCT(label)
          FROM
            rb_res_normal_times
          WHERE
            fk_run_id = %d
          ORDER BY
            id', $run_id))) {
      throw new Exception(sprintf("Cannot get list of measured times, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

    $labellist = array();
    while ($labellist[] = mysqli_fetch_assoc($res))
      ;

    return $labellist;
  }

  //
  // protected
  //

  protected function saveRuninfo($run_label, $run_datetime, $file, $binary_file, $binary_label) {

    $sysinfo = '';
    if (function_exists("posix_uname")) {
      foreach (posix_uname() as $k => $v)
        $sysinfo .= sprintf("%s  ", $v);
      $sysinfo = substr($sysinfo, 0, -2);
    } else {
      $sysinfo = "(posix_uname() unavailable, likely some Windows OS)";
    }

    if (!mysqli_query($this->link, $sql = $this->mySprintf('INSERT INTO rb_res_run
      (run, file, label, sysinfo, binary_file, binary_label)
        VALUES
      ("%s", "%s", "%s", "%s", "%s", "%s")', $run_datetime, $file, $run_label, $sysinfo, $binary_file, $binary_label))) {

      throw new Exception(sprintf("Cannot insert into table rb_res_run, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

    return mysqli_insert_id($this->link);
  }

  protected function mySprintf() {

    $args       = func_get_args();
    $pattern    = array_shift($args);
    $call_args  = array($pattern);

    foreach ($args as $k => $v)
      $call_args[] = mysqli_real_escape_string($this->link, $v);

    $ret = call_user_func_array('sprintf', $call_args);
    return $ret;
  }

}
?>