<?PHP
require_once('rb_main.php');
set_time_limit(0);

try {
  
  $rb = new rb_main('My test run label', date('Y-m-d H:i:s'));
  $rb->parseArgs($argc, $argv);
  printf("Config:\n%s\n", implode("\n", $rb->getConfig()));
  $rb->runTests(true);
  
} catch (Exception $e) {
  
  printf("%s\n\n", $e->getMessage());
  printf("%s\n", $rb->getCommandlineSyntax());  
  
  exit(1);
}
exit(0);
?>
