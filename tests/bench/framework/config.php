<?PHP
// 
// Check for PHP's run-test.php and use it's environment variables?
// If enabled MYSQL_TEST_USER etc. will overwrite settings specified here!
// 
define('RB_USE_TEST_ENV', true);

//
// PHP binaries to use
//

/* Yes, a global variable... this is a little tool not a design study */

/* 
$rb_binaries = array(
  'PHP mysqlnd_ms' => array(
	'binary' => getenv("TEST_PHP_EXECUTABLE"),
	'ini' => getcwd() . DIRECTORY_SEPARATOR . DIRECTORY_SEPARATOR . 'micro_benches' . DIRECTORY_SEPARATOR . 'mysqlnd_ms_php.ini'
  ),
  'PHP' => array(
	'binary' => getenv("TEST_PHP_EXECUTABLE"),
	'ini' => NULL
  ),
);
*/
$rb_binary = array();
if (empty($rb_binaries) && RB_USE_TEST_ENV) {
  // fallback to run-test settings

  // Format: rb_binaries[display name] = array('binary' => executable, 'ini' => ini file or NULL)
  $rb_binaries['PHP'] = array('binary' => getenv("TEST_PHP_EXECUTABLE"), 'ini' => NULL);
}



if (!RB_USE_TEST_ENV) {

  //
  // Database connection parameter
  //

  // database user
  define('RB_DB_USER', 'root');
  // database password
  define('RB_DB_PASSWD', 'root');
  // database 
  define('RB_DB_DB', 'microbench');
  // host, for mysqlnd: localhost = socket, 127.0.0.1 = tcp/ip
  define('RB_DB_HOST', '127.0.0.1');
  // port
  define('RB_DB_PORT', 3306);
  // socket
  define('RB_DB_SOCKET', '');
  // database engine
  define('RB_DB_ENGINE', 'InnoDB');

} else {

  // let the environment define the settings

  if ($tmp = getenv('MYSQL_TEST_USER'))
    define('RB_DB_USER', $tmp);
  else
    define('RB_DB_USER', 'root');

  if ($tmp = getenv('MYSQL_TEST_PASSWD'))
    define('RB_DB_PASSWD', $tmp);
  else
    define('RB_DB_PASSWD', 'root');


  if ($tmp = getenv('MYSQL_TEST_DB'))
    define('RB_DB_DB', $tmp);
  else
     define('RB_DB_DB', 'microbench');

  if ($tmp = getenv('MYSQL_TEST_HOST'))
    define('RB_DB_HOST', $tmp);
  else
    define('RB_DB_HOST', '127.0.0.1');
    
  if ($tmp = getenv('MYSQL_TEST_PORT'))
    define('RB_DB_PORT', $tmp);
  else
    define('RB_DB_PORT', 3306);
    
  if ($tmp = getenv('MYSQL_TEST_SOCKET'))
    define('RB_DB_SOCKET', $tmp);
  else
    define('RB_DB_SOCKET', '/tmp/mysql.sock');
    
  if ($tmp = getenv('MYSQL_TEST_ENGINE'))
    define('RB_DB_ENGINE', $tmp);
  else 
    define('RB_DB_ENGINE', 'InnoDB');
}

//
// oprofile profiling
//

// run oprofile by default?
define('RB_DEFAULT_OPROFILE', false);
// binaries and co
define('RB_OPROFILE_OPCONTROL', (RB_USE_TEST_ENV && getenv('MYSQL_TEST_OPCONTROL')) ? getenv('MYSQL_TEST_OPCONTROL') : '/usr/bin/opcontrol');
define('RB_OPROFILE_OPREPORT',  (RB_USE_TEST_ENV && getenv('MYSQL_TEST_OPREPORT')) ? getenv('MYSQL_TEST_OPREPORT') : '/usr/bin/opreport');
define('RB_OPROFILE_VMLINUX',   (RB_USE_TEST_ENV && getenv('MYSQL_TEST_VMLINUX')) ? getenv('MYSQL_TEST_VMLINUX') : '/usr/src/linux/vmlinux');

//
// program options
//

// activate HTML output generation by default?
define('RB_DEFAULT_OUTPUT_HTML', true);
// where to store the HTML files
define('RB_OUTPUT_HTML_DIR', './web/');
// where to store the HTML that can be used for some wikis
define('RB_OUTPUT_WIKI_DIR', './web/wiki/');


// activate storing results into the DB by default?
define('RB_DEFAULT_STORAGE_DB', true);
?>
