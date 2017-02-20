<?php 
require_once('abstract.php');
require_once('config.php');

class rb_storage_db_normal extends rb_storage_db {

  public function init() {

    parent::init();
    if (!mysqli_query($this->link, "
        CREATE TABLE IF NOT EXISTS rb_res_normal_times (
            id INT AUTO_INCREMENT PRIMARY KEY,
            fk_run_id INT NOT NULL,
            label VARCHAR(255) NOT NULL,
            runtime DECIMAL(9,6) NOT NULL,
            INDEX(fk_run_id),
            FOREIGN KEY (fk_run_id) REFERENCES rb_res_run(run_id),
            INDEX(label),
            INDEX(runtime)
        ) Engine = InnoDB")) {
      throw new Exception(sprintf("Cannot create table rb_res_normal_times, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

  }

  public function save($run_label, $run_datetime, $file, $binary_file, $binary_label, $data) {

    $fk_run_id = $this->saveRuninfo($run_label, $run_datetime, $file, $binary_file, $binary_label);

    foreach ($data['times'] as $label => $time) {
      if (!mysqli_query($this->link, sprintf('INSERT INTO rb_res_normal_times
            (fk_run_id, label, runtime) VALUES (%d, "%s", %f)',
          $fk_run_id, mysqli_real_escape_string($this->link, $label), $time))) {

            $msg = sprintf("Cannot insert into rb_res_normal_times, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link));

            try {
              $this->delete($run_label);
            } catch (Exception $e) {
              throw new Exception(sprintf("%s\n%s\n", $msg, $e->getMessage()));
            }
            throw new Exception($msg);
      }
    }

  }

  public function delete($run_label) {

      if (!mysqli_query($this->link, sprintf('DELETE FROM rb_res_normal_times WHERE label = "%s"', 
            mysqli_real_escape_string($this->link, $run_label)))) {
        throw new Exception(sprintf("Cannot delete from rb_res_normal_times, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
      }

  }

  public function deleteBefore($run_datetime) {

    if (!mysqli_query($this->link, sprintf('DELETE FROM rb_res_normal_times WHERE runtime <= "%s"', 
            mysqli_real_escape_string($this->link, $run_datetime)))) {
        throw new Exception(sprintf("Cannot delete from rb_res_normal_times, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

  }

  public function deleteAll() {

    if (!mysqli_query($this->link, sprintf('DELETE FROM rb_res_normal_times'))) {
        throw new Exception(sprintf("Cannot delete from rb_res_normal_times, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

  }

  public function reset() {
    if (!mysqli_query($this->link, "DROP TABLE IF EXISTS rb_res_normal_times")) {
      throw new Exception(sprintf("Cannot create table, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }
    try {
      parent::reset();
    } catch (Exception $e) {
      // igore issues dropping rb_res_run ?
      print $e->getMessage();      
    }
  }

  public function getRuntimes($run_label, $file, $run_datetime, $binary_label, $binary_file) {

    if (!$res = mysqli_query($this->link, $sql = $this->mySprintf('
        SELECT 
          r2.label AS label, 
          r2.runtime AS runtime 
        FROM 
          rb_res_run r1,
          rb_res_normal_times r2
        WHERE 
          r1.run            = "%s" AND
          r1.file           = "%s" AND
          r1.label          = "%s" AND
          r1.binary_label   = "%s" AND
          r1.binary_file    = "%s" AND
          r2.fk_run_id      = r1.run_id
        ORDER BY
          r2.id',
        $run_datetime,
        $file,
        $run_label,
        $binary_label,
        $binary_file))) {
      throw new Exception(sprintf("Cannot fetch runtimes, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

    $runtimes = array();
    while ($row = mysqli_fetch_assoc($res)) {
      $runtimes[$row['label']] = $row['runtime'];
    }
    mysqli_free_result($res);

    return $runtimes;
  } 


  public function getRuntimeByRunIDAndLabel($run_id, $label) {

    if (!$res = mysqli_query($this->link, $sql = $this->mySprintf('
        SELECT 
          runtime
        FROM 
          rb_res_normal_times
        WHERE 
          fk_run_id = %d AND
          label = "%s"',
        $run_id,
        $label
        ))) {
      throw new Exception(sprintf("Cannot fetch runtimes, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

    $runtime = mysqli_fetch_assoc($res);
    mysqli_free_result($res);

    return $runtime;
  }

  public function getFastestBinaries($run_label, $run_datetime) {

    if (!$res = mysqli_query($this->link, $this->mySprintf('
          SELECT 
            COUNT(*) AS num, 
            r1.file AS file,
            r1.binary_label AS binary_label,
            t1.label AS time_label
          FROM rb_res_run AS r1 
            INNER JOIN rb_res_normal_times AS t1 ON (t1.fk_run_id = r1.run_id) 
            INNER JOIN rb_res_run AS r2 ON (r1.run_id <> r2.run_id AND r1.run = r2.run AND r1.label = r2.label AND r1.file = r2.file) 
            INNER JOIN rb_res_normal_times AS t2 ON (t2.fk_run_id = r2.run_id AND t1.label = t2.label AND t1.runtime < t2.runtime) 
          WHERE 
            r1.run = "%s" AND r1.label = "%s"
          GROUP BY 
            r1.binary_label, t1.label 
          HAVING 
            num = (SELECT (COUNT(DISTINCT(binary_label)) - 1) FROM rb_res_run WHERE run = "%s" AND label = "%s")
          ORDER BY
            r1.file 
        ',
        $run_datetime,
        $run_label,
        $run_datetime,
        $run_label))) {
      throw new Exception(sprintf("Cannot find fastest, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }
// var_dump($sql);
    $fastest = array();
    while ($row = mysqli_fetch_assoc($res)) {
      $fastest[$row['file']][$row['time_label']] = $row['binary_label'];
    }
    mysqli_free_result($res);

    return $fastest;
  }


  public function getFastestBinaryByFile($run_label, $run_datetime, $file) {

    if (!$res = mysqli_query($this->link, $this->mySprintf('
          SELECT 
            COUNT(*) AS num, 
            r1.binary_label AS binary_label,
            t1.label AS time_label
          FROM rb_res_run AS r1 
            INNER JOIN rb_res_normal_times AS t1 ON (t1.fk_run_id = r1.run_id) 
            INNER JOIN rb_res_run AS r2 ON (r1.run_id <> r2.run_id AND r1.run = r2.run AND r1.label = r2.label AND r1.file = r2.file) 
            INNER JOIN rb_res_normal_times AS t2 ON (t2.fk_run_id = r2.run_id AND t1.label = t2.label AND t1.runtime < t2.runtime) 
          WHERE 
            r1.run = "%s" AND r1.label = "%s" AND r1.file = "%s"
          GROUP BY 
            r1.binary_label, t1.label 
          HAVING 
            num = (SELECT (COUNT(*) - 1) FROM rb_res_run WHERE run = "%s" AND label = "%s" AND file = "%s")',
        $run_datetime,
        $run_label,
        $file,
        $run_datetime,
        $run_label,
        $file))) {
      throw new Exception(sprintf("Cannot find fastest, [%d] %s\n", mysqli_errno($this->link), mysqli_error($this->link)));
    }

    $fastest = array();
    while ($row = mysqli_fetch_assoc($res)) {
      $fastest[$row['time_label']] = $row['binary_label'];
    }
    mysqli_free_result($res);

    return $fastest;
  }

}
?>