<?php
	require_once(__DIR__."/config.inc");
	/*
	Default values are "localhost", "root",
	database "stest" and empty password.
	Change the MYSQL_TEST environment values
	if you want to use another configuration
	*/

	$driver    = new mysqli_driver;
    $stop_eslave = getenv("MYSQL_TEST_STOP_ESLAVE");
    $start_eslave = getenv("MYSQL_TEST_START_ESLAVE");
    $real_memcached = getenv("MYSQL_TEST_REAL_MEMCACHED");
	$memcached_host      = getenv("MYSQL_TEST_MEMCACHED_HOST")     ? getenv("MYSQL_TEST_MEMCACHED_HOST") : "127.0.0.1";
	$memcached_port      = getenv("MYSQL_TEST_MEMCACHED_PORT")     ? getenv("MYSQL_TEST_MEMCACHED_PORT") : 11211;
	$client_key      = getenv("MYSQL_TEST_SSL_KEY")     ? getenv("MYSQL_TEST_SSL_KEY") : "client-key.pem";
	$client_cert      = getenv("MYSQL_TEST_SSL_CERT")     ? getenv("MYSQL_TEST_SSL_CERT") : "client-cert.pem";
	$ca_cert      = getenv("MYSQL_TEST_SSL_CA")     ? getenv("MYSQL_TEST_SSL_CA") : "ca.pem";

	$host      = getenv("MYSQL_TEST_HOST")     ? getenv("MYSQL_TEST_HOST") : "127.0.0.1";
	$port      = getenv("MYSQL_TEST_PORT")     ? getenv("MYSQL_TEST_PORT") : 3306;
	$user      = getenv("MYSQL_TEST_USER")     ? getenv("MYSQL_TEST_USER") : "root";
	$passwd    = getenv("MYSQL_TEST_PASSWD")   ? getenv("MYSQL_TEST_PASSWD") : "";
	$db        = getenv("MYSQL_TEST_DB")       ? getenv("MYSQL_TEST_DB") : "test";
	$engine    = getenv("MYSQL_TEST_ENGINE")   ? getenv("MYSQL_TEST_ENGINE") : "InnoDB";
	$socket    = getenv("MYSQL_TEST_SOCKET")   ? getenv("MYSQL_TEST_SOCKET") : null;
	$skip_on_connect_failure  = getenv("MYSQL_TEST_SKIP_CONNECT_FAILURE") ? getenv("MYSQL_TEST_SKIP_CONNECT_FAILURE") : true;
	$connect_flags = getenv("MYSQL_TEST_CONNECT_FLAGS") ? (int)getenv("MYSQL_TEST_CONNECT_FLAGS") : 0;
	if ($socket) {
		ini_set('mysqli.default_socket', $socket);
	}
	/* We assume that user/password are the same for all machines but alternative port/socket can be configured like host[;port|socket] */
	$master_host = $master_host_only = getenv("MYSQL_TEST_MASTER_HOST") ? getenv("MYSQL_TEST_MASTER_HOST") : $host;
	$master_socket = $socket;
	$master_port = $port;
	if (false !== ($pos = strpos($master_host, ':'))) {
		if ('/' == substr($master_host, $pos + 1, 1)) {
			$master_socket = substr($master_host, $pos + 1);
			$master_port = NULL;
		} else {
			$master_socket = NULL;
			$master_port = substr($master_host, $pos + 1);
		}
		$master_host_only = substr($master_host, 0, $pos);
	}

	$slave_host = $slave_host_only = getenv("MYSQL_TEST_SLAVE_HOST") ? getenv("MYSQL_TEST_SLAVE_HOST") : $host;
	$slave_socket = $socket;
	$slave_port = $port;
	if (false !== ($pos = strpos($slave_host, ':'))) {
		if ('/' == substr($slave_host, $pos + 1, 1)) {
			$slave_socket = substr($slave_host, $pos + 1);
			$slave_port = NULL;
		} else {
			$slave_socket = NULL;
			$slave_port = substr($slave_host, $pos + 1);
		}
		$slave_host_only = substr($slave_host, 0, $pos);
	}

	$emulated_master_host = $emulated_master_host_only = getenv("MYSQL_TEST_EMULATED_MASTER_HOST") ? getenv("MYSQL_TEST_EMULATED_MASTER_HOST") : $host;
	$emulated_master_socket = $socket;
	$emulated_master_port = $port;
	if (false !== ($pos = strpos($emulated_master_host, ':'))) {
		if ('/' == substr($emulated_master_host, $pos + 1, 1)) {
			$emulated_master_socket = substr($emulated_master_host, $pos + 1);
			$emulated_master_port = NULL;
		} else {
			$emulated_master_socket = NULL;
			$emulated_master_port = substr($emulated_master_host, $pos + 1);
		}
		$emulated_master_host_only = substr($emulated_master_host, 0, $pos);
	}

	$emulated_slave_host = $emulated_slave_host_only = getenv("MYSQL_TEST_EMULATED_SLAVE_HOST") ? getenv("MYSQL_TEST_EMULATED_SLAVE_HOST") : $host;
	$emulated_slave_socket = $socket;
	$emulated_slave_port = $port;
	if (false !== ($pos = strpos($emulated_slave_host, ':'))) {
		if ('/' == substr($emulated_slave_host, $pos + 1, 1)) {
			$emulated_slave_socket = substr($emulated_slave_host, $pos + 1);
			$emulated_slave_port = NULL;
		} else {
			$emulated_slave_socket = NULL;
			$emulated_slave_port = substr($emulated_slave_host, $pos + 1);
		}
		$emulated_slave_host_only = substr($emulated_slave_host, 0, $pos);
	}

	/* Memcached setting */
	$memcached_port_add_hack = getenv("MYSQL_TEST_MEMCACHED_PORT_ADD_HACK") ? getenv("MYSQL_TEST_MEMCACHED_PORT_ADD_HACK") : 7905;
	
	/* Development setting: test experimal features and/or feature requests that never worked before? */
	$TEST_EXPERIMENTAL = (in_array(getenv("MYSQL_TEST_EXPERIMENTAL"), array(0, 1))) ?
				((1 == getenv("MYSQL_TEST_EXPERIMENTAL")) ? true : false) :
				false;

	$IS_MYSQLND = stristr(mysqli_get_client_info(), "mysqlnd");
	if (!$IS_MYSQLND) {
		$MYSQLND_VERSION = NULL;
	} else {
		/*
		The formatting of the version reported by mysqli_get_client_info()
		has changed significantly in the past. To get tests working properly
		with PHP 5.3.0 and up, we set everything that looks like prior to
		PHP 5.3.0 to version 5.0.4 = 5 * 10000 + 0 * 100 + 4 = 50004.
		PHP 5.3.0	reports mysqlnd 5.0.5 dev (= 5 * 10000 + 0 * 100 + 5 = 50005.
		*/
		if (preg_match('@^mysqlnd (\d+)\.(\d+)\.(\d+).*@ism', mysqli_get_client_info(), $matches)) {
			/* formatting schema used by PHP 5.3.0 */
			$MYSQLND_VERSION = (int)$matches[1] * 10000 + (int)$matches[2] * 100 + (int)$matches[3];
		} else if (preg_match('@^mysqlnd/PHP 6.0.0-dev@ism', mysqli_get_client_info(), $matches)) {
			/*
				PHP 6.0 at the time of the first PHP 5.3.0 release.
				HEAD and 5.3 have been in sync when 5.3.0 was released.
				It is at least 5.0.5-dev.
			*/
			$MYSQLND_VERSION = 50005;
		} else {
			/* unknown */
			$MYSQLND_VERSION = -1;
		}

	}

	if (!function_exists('sys_get_temp_dir')) {
		function sys_get_temp_dir() {

			if (!empty($_ENV['TMP']))
				return realpath( $_ENV['TMP'] );
			if (!empty($_ENV['TMPDIR']))
				return realpath( $_ENV['TMPDIR'] );
			if (!empty($_ENV['TEMP']))
				return realpath( $_ENV['TEMP'] );

			$temp_file = tempnam(md5(uniqid(rand(), TRUE)), '');
			if ($temp_file) {
				$temp_dir = realpath(dirname($temp_file));
				unlink($temp_file);
				return $temp_dir;
			}
			return FALSE;
		}
	}

	if (!function_exists('mst_mysqli_connect')) {

		function my_mysql_connect($host, $user, $passwd, $db, $port, $socket, $flags = NULL, $persistent = false) {
			global $connect_flags;

			$flags = ($flags === NULL) ? $connect_flags : $flags;

			if ($socket)
				$host = sprintf("%s:%s", "localhost", $socket);
			else if ($port)
				$host = sprintf("%s:%s", $host, $port);

			if ($persistent) {
				$link = mysql_pconnect($host, $user, $passwd, $flags);
			} else {
				$link = mysql_connect($host, $user, $passwd, true, $flags);
			}

			if ($db)
				mysql_select_db($db, $link);

			return $link;
		}


		/**
		* Whenever possible, please use this wrapper to make testing ot MYSQLI_CLIENT_COMPRESS (and potentially SSL) possible
		*
		* @param enable_env_flags Enable setting of connection flags through 	env(MYSQL_TEST_CONNECT_FLAGS)?
		*/
		function mst_mysqli_connect($host, $user, $passwd, $db, $port, $socket, $enable_env_flags = true) {
			global $connect_flags;

			$flags = ($enable_env_flags) ? $connect_flags : false;

			if ($flags !== false) {
				$link = mysqli_init();
				if (!mysqli_real_connect($link, $host, $user, $passwd, $db, $port, $socket, $flags))
					$link = false;
			} else {
				$link = mysqli_connect($host, $user, $passwd, $db, $port, $socket);
			}

			return $link;
		}

		/**
		* Whenever possible, please use this wrapper to make testing ot MYSQLI_CLIENT_COMPRESS (and potentially SSL) possible
		*
		* @param enable_env_flags Enable setting of connection flags through env(MYSQL_TEST_CONNECT_FLAGS)
		*/
		function mst_mysqli_real_connect($link, $host, $user, $passwd, $db, $port, $socket, $flags = 0, $enable_env_flags = true) {
			global $connect_flags;

			if ($enable_env_flags)
				$flags & $connect_flags;

			return mysqli_real_connect($link, $host, $user, $passwd, $db, $port, $socket, $flags);
		}

		class mst_mysqli extends mysqli {
			public function __construct($host, $user, $passwd, $db, $port, $socket, $enable_env_flags = true) {
				global $connect_flags;

				$flags = ($enable_env_flags) ? $connect_flags : false;

				if ($flags !== false) {
					parent::init();
					$this->real_connect($host, $user, $passwd, $db, $port, $socket, $flags);
				} else {
					parent::__construct($host, $user, $passwd, $db, $port, $socket);
				}
			}
		}

		function mst_mysqli_get_charsets($link) {

			/* Those tree are set by SET NAMES */
			$charsets = array(
				'client' 		=> NULL,
				'results'		=> NULL,
				'connection'	=> NULL,
			);

			if (!($res = mysqli_query($link, "SHOW VARIABLES LIKE '%character%'"))) {
				printf("[%d] %s\n", mysqli_errno($link), mysqli_error($link));
				return $charsets;
			}

			$names = array();
			while ($row = mysqli_fetch_assoc($res)) {
				$names[$row['Variable_name']] = $row['Value'];
			}
			mysqli_free_result($res);

			if (!($res = mysqli_query($link, sprintf("SHOW CHARACTER SET LIKE '%s'", $names['character_set_client']))) ||
				!($details = mysqli_fetch_assoc($res))) {
				printf("[%d] %s\n", mysqli_errno($link), mysqli_error($link));
				return $charsets;
			}
			mysqli_free_result($res);

			$charsets['client'] = array(
				'charset' 	=> $details['Charset'],
				'desc'		=> $details['Description'],
				'collation'	=> $details['Default collation'],
				'maxlen'	=> $details['Maxlen'],
				'nr'		=> NULL,
			);

			if (!($res = mysqli_query($link, sprintf("SHOW COLLATION LIKE '%s'", $details['Default collation']))) ||
				!($collation = mysqli_fetch_assoc($res))) {
				printf("[%d] %s\n", mysqli_errno($link), mysqli_error($link));
				return $charsets;
			}
			mysqli_free_result($res);
			$charsets['client']['nr'] = $collation['Id'];

			if (!($res = mysqli_query($link, sprintf("SHOW CHARACTER SET LIKE '%s'", $names['character_set_results']))) ||
				!($details = mysqli_fetch_assoc($res))) {
				printf("[%d] %s\n", mysqli_errno($link), mysqli_error($link));
				return $charsets;
			}
			mysqli_free_result($res);

			$charsets['results'] = array(
				'charset' 	=> $details['Charset'],
				'desc'		=> $details['Description'],
				'collation'	=> $details['Default collation'],
				'maxlen'	=> $details['Maxlen'],
				'nr'		=> NULL,
			);

			if (!($res = mysqli_query($link, sprintf("SHOW COLLATION LIKE '%s'", $details['Default collation']))) ||
				!($collation = mysqli_fetch_assoc($res))) {
				printf("[%d] %s\n", mysqli_errno($link), mysqli_error($link));
				return $charsets;
			}
			mysqli_free_result($res);
			$charsets['results']['nr'] = $collation['Id'];


			if (!($res = mysqli_query($link, sprintf("SHOW CHARACTER SET LIKE '%s'", $names['character_set_connection']))) ||
				!($details = mysqli_fetch_assoc($res))) {
				printf("[%d] %s\n", mysqli_errno($link), mysqli_error($link));
				return $charsets;
			}
			mysqli_free_result($res);

			$charsets['connection'] = array(
				'charset' 	=> $details['Charset'],
				'desc'		=> $details['Description'],
				'collation'	=> $details['Default collation'],
				'maxlen'	=> $details['Maxlen'],
				'nr'		=> NULL,
			);

			if (!($res = mysqli_query($link, sprintf("SHOW COLLATION LIKE '%s'", $details['Default collation']))) ||
				!($collation = mysqli_fetch_assoc($res))) {
				printf("[%d] %s\n", mysqli_errno($link), mysqli_error($link));
				return $charsets;
			}
			mysqli_free_result($res);
			$charsets['connection']['nr'] = $collation['Id'];

			return $charsets;
		}

		if (extension_loaded("pdo_mysql")) {
			function my_pdo_connect($host, $user, $passwd, $db, $port, $socket, $options = NULL) {

				if (is_null($options))
					$options = array(
					  PDO::ATTR_EMULATE_PREPARES => true,
					  PDO::MYSQL_ATTR_DIRECT_QUERY => true,
					);

				if ($port) {
				  $dsn = sprintf("mysql:host=%s;port=%d;dbname=%s", $host, $port, $db);
				} else {
				  $dsn = sprintf("mysql:host=%s;unix_socket=%s;dbname=%s", $host, $socket, $db);
				}
				return new PDO($dsn, $user, $passwd, $options);
			}
		}

	} else {
		printf("skip Eeeek/BUG/FIXME - connect.inc included twice! skipif bug?\n");
	}

	if (!function_exists("mst_create_config")) {
		function mst_create_config($file, $settings, $replace_file = true, $format = 'json') {

			switch ($format) {
			  case 'json':
				return mst_create_config_json($file, $settings, $replace_file);
				break;
			  case 'ini':
			  default:
				return mst_create_config_ini($file, $settings, $replace_file);
				break;
			}

		}

		function mst_create_config_json($file, $settings, $replace_file) {
			if ($replace_file && file_exists($file) && !@unlink($file))
				return sprintf("Cannot unlink existing file '%s'.", $file);

			if (!$fp = @fopen($file, "w"))
				return sprintf("Cannot open file '%s' for writing.", $file);

			$json = array();
			foreach ($settings as $application => $config) {

				$json[$application] = array();

				if (isset($config['xa'])) {
					$json[$application]['xa'] = $config['xa'];
				}

				if (isset($config['fabric'])) {
					$json[$application]['fabric'] = $config['fabric'];

					if (isset($config['fabric']['hosts']) && is_array($config['fabric']['hosts'])) {
						$json[$application]['fabric']['hosts'] = array();
						/* restructure */
						foreach ($config['fabric']['hosts'] as $host_info) {
							$host = new stdClass();
							if (isset($host_info['host'])) {
								$host->host = $host_info['host'];
							}
							if (isset($host_info['port'])) {
								$host->port = $host_info['port'];
							}
							$json[$application]['fabric']['hosts'][] = $host;
						}
					}
				}

				if (isset($config['master'])) {
					if (!is_array($config['master']))
						return sprintf("The master must be set using a hash");

					$json[$application]['master'] = array();
					foreach ($config['master'] as $idx => $host) {
						if (is_array($host)) {
							/* 1.1. syntax */
							if (is_string($idx)) {
							  $json[$application]['master'][$idx] = $host;
							} else {
							  $json[$application]['master']['master_' . $idx] = $host;
							}
						} else {
							$host_config = array();
							if (false !== stristr($host, ':')) {
								/* 1.0 legacy */
								list($real_host, $port) = explode(':', $host);
								$host_config['host'] = $real_host;
								if ($port) {
									if (!is_numeric($port)) {
										$host_config['socket'] = $port;
									} else {
										$host_config['port'] = $port;
									}
								}
							} else {
								$host_config['host'] = $host;
							}
							$json[$application]['master']['master_' . $idx] = $host_config;
						}
					}
				}
				if (isset($config['slave'])) {
					$json[$application]['slave'] = array();
					foreach ($config['slave'] as $idx => $host) {
						if (is_array($host)) {
							if (is_string($idx)) {
								$json[$application]['slave'][$idx] = $host;
							} else {
								$json[$application]['slave']['slave_' . $idx] = $host;
							}
						} else {
							$host_config = array();
							/* 1.0 legacy */
							if (false !== stristr($host, ':')) {
								list($real_host, $port) = explode(':', $host);
								$host_config['host'] = $real_host;
								if ($port) {
									if (!is_numeric($port)) {
										$host_config['socket'] = $port;
									} else {
										$host_config['port'] = $port;
									}
								}
							} else {
								$host_config['host'] = $host;
							}
							$json[$application]['slave']['slave_' . $idx] = $host_config;
						}
					}
				}

				if (isset($config['filters'])) {
					$json[$application]['filters'] = $config['filters'];
				} else if (isset($config['pick'])) {
					if (!is_array($config['pick']))
						$config['pick'] = array($config['pick']);

					/*
					  1. array('random')  -- is_int()==true, take $method_values
					  2. array('random' => array('once' => '1')) -- is_int()==false, take $method for name
					*/
					foreach ($config['pick'] as $method => $method_values) {
						$json[$application]['filters'][is_int($method)? (string)$method_values:(string)$method] =
								is_int($method)? array() :
												 (is_array($method_values)? $method_values :
												 							array($method_values));
					}
				}
				if (isset($config['lazy_connections'])) {
				  $json[$application]['lazy_connections'] = (string)$config['lazy_connections'];
				}
				if (isset($config['server_charset'])) {
				  $json[$application]['server_charset'] = (string)$config['server_charset'];
				}
				if (isset($config['failover'])) {
				  $json[$application]['failover'] = $config['failover'];
				}
				if (isset($config['master_on_write'])) {
				  $json[$application]['master_on_write'] = (string)$config['master_on_write'];
				}
				if (isset($config['trx_stickiness'])) {
				  $json[$application]['trx_stickiness'] = (string)$config['trx_stickiness'];
				}
				if (isset($config['global_transaction_id_injection'])) {
				  $json[$application]['global_transaction_id_injection'] = $config['global_transaction_id_injection'];
				}
				if (isset($config['transient_error'])) {
				  $json[$application]['transient_error'] = $config['transient_error'];
				}
			}

			fwrite($fp, json_encode($json));
			fclose($fp);
			return '';
		}

	}

	function mst_error_handler($errno, $error, $file, $line) {
		/* KLUDGE */
		global $mst_ignore_errors;
		static $errcodes = array();

		if (empty($errcodes)) {
			$constants = get_defined_constants();
			foreach ($constants as $name => $value) {
				if (substr($name, 0, 2) == "E_")
					$errcodes[$value] = $name;
			}
		}
		$msg = sprintf("[%s] %s in %s on line %s\n",
			(isset($errcodes[$errno])) ? $errcodes[$errno] : $errno,
			 $error, $file, $line);

		if (isset($mst_ignore_errors) && is_array($mst_ignore_errors)) {
			foreach ($mst_ignore_errors as $pattern) {
				if (stristr($msg, $pattern))
					return true;
			}
		}

		echo $msg;

		return true;
	}
?>
