<?php
/* Shut down notice about HTTP_RAW_POST_DATA (deprec in 5.6) */
require_once("config.inc");
require_once("util.inc");
require_once("connect.inc");

class ms_emulated_fabric {

	protected $methods = array(
		/* Test */
		'emulate_defect' => 'emulate_defect',
		/* Fabric */
		'sharding.lookup_servers' 	=> 'sharding_lookup_servers',
		'dump.servers'			=> "dump_servers",
		'dump.shard_index'		=> "dump_shard_index",
		'dump.shard_maps'		=> "dump_shard_maps",
		'dump.shard_tables'		=> "dump_shard_tables",
	);

	protected $server;
	protected $defects;

	function __construct($post) {
		ms_emulated_fabric_server_log(sprintf("Handling request process %d", getmypid()));
		$this->load_defects();
		$this->server = xmlrpc_server_create();
		foreach ($this->methods as $rpc_name => $method) {
			xmlrpc_server_register_method($this->server, $rpc_name, array(&$this, $method));
		}
		Header("Content-type: text/xml");
		echo xmlrpc_server_call_method($this->server, $post, null);
	}

	protected function get_connection() {
		global $emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket;
		static $link = NULL;
		if (is_null($link)) {
			$link = mst_mysqli_connect($emulated_master_host_only, $user, $passwd, $db, $emulated_master_port, $emulated_master_socket);
		}
		if (mysqli_connect_errno()) {
			ms_emulated_fabric_server_log(sprintf("[%d] %s",
				mysqli_connect_errno(), mysqli_connect_error()));
		}
		return $link;
	}

	protected function persist_defect($method, $params) {
		$link = $this->get_connection();
		if (!$link->query("CREATE TABLE IF NOT EXISTS _mysqlnd_ms_fabric_emulator(
			method VARCHAR(255) NOT NULL PRIMARY KEY,
			params BLOB)")) {
			ms_emulated_fabric_server_log(sprintf("[%d] %s", $link->errno, $link->error));
			return false;
		}
		$sql = sprintf("REPLACE INTO _mysqlnd_ms_fabric_emulator(method, params) VALUES ('%s', '%s')",
			$link->real_escape_string($method),
			$link->real_escape_string(serialize($params)));
		if ($link->query($sql)) {
			$this->defects[$method] = $params;
			return true;
		} else {
			ms_emulated_fabric_server_log(sprintf("[%d] %s", $link->errno, $link->error));
		}
		return false;
	}

	protected function consume_defect($method) {
		unset($this->defects[$method]);

		$link = $this->get_connection();
		if (!$link->query(sprintf("DELETE FROM _mysqlnd_ms_fabric_emulator WHERE method = '%s'",
			$link->real_escape_string($method)))) {
			ms_emulated_fabric_server_log(sprintf("[%d] %s", $link->errno, $link->error));
			return false;
		}
		return true;
	}

	protected function load_defects() {
		$link = $this->get_connection();
		if (!$link->query("CREATE TABLE IF NOT EXISTS _mysqlnd_ms_fabric_emulator(
			method VARCHAR(255) NOT NULL PRIMARY KEY,
			params BLOB)")) {
			ms_emulated_fabric_server_log(sprintf("[%d] %s", $link->errno, $link->error));
			return false;
		}

		$res = $link->query("SELECT * FROM _mysqlnd_ms_fabric_emulator");
		if (!$res) {
			ms_emulated_fabric_server_log(sprintf("[%d] %s", $link->errno, $link->error));
			return false;
		}
		while ($row = $res->fetch_assoc()) {
			$this->defects[$row['method']] = unserialize($row['params']);
		}
		return true;
	}

	public function emulate_defect($method, $params, $data) {
		ms_emulated_fabric_server_log(sprintf("Emulate defect '%s'", var_export($params, true)));

		if (count($params) < 2)
			return 'Needs two or more parameters';

		if (!isset($this->methods[$params[0]])) {
			return sprintf("Unknown method '%s'", $params[0]);
		}
		if ($this->persist_defect($params[0], $params[1])) {
			ms_emulated_fabric_server_log("Defect registered");
		}

		return $this->defects;
	}

	public function sharding_lookup_servers($method, $params, $data) {
		/* hackish... one fine day... */
		global $emulated_master_host_only, $emulated_master_port, $emulated_master_socket,
				$emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket;

		ms_emulated_fabric_server_log(sprintf("Request '%s': %s", $method, var_export($params, true)));

		$reply = NULL;
		if ($this->standard_defects($method, $reply)) {
			return $reply;
		}

		if ($params[0] == 'fabric_sharding.test') {
			/* send proper reply */
			$reply = array(
				true,
				"",
				array(
					array(
						"emulated master",
						$emulated_master_host_only . ':' . $emulated_master_port,
						true /* Is master? */
					),
					array(
						"emulated slave",
						$emulated_slave_host_only . ':' . $emulated_slave_port,
						false /* Is master? */
					)
				)
			);
		} else {
			$reply = array(
				false,
				"unknown table",
				true
			);
		}
		return $reply;
	}

	public function dump_servers($method, $params, $data) {
		/* hackish... one fine day... */
		global $emulated_master_host_only, $emulated_master_port, $emulated_master_socket,
				$emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket;

		ms_emulated_fabric_server_log(sprintf("Request '%s': %s", $method, var_export($params, true)));

		$reply = NULL;
		if ($this->standard_defects($method, $reply)) {
			return $reply;
		}

		/* send proper reply */
			$reply = array(
				true,
				0, 0, "",
				array(
					array(
						"emulated master",
						"group1",
						$emulated_master_host_only,
						$emulated_master_port,
						3, /* mode: 0 = OFFFLINE, 1 = READ_ONLY, 2 = WRITE_ONLY, 3 = READ_WRITE */
						3, /* status: 0 = SPARE, 1 = SCALE, 2 = SECONDARY, 3 = PRIMARY */
						1.0 /* weight */
					),
					array(
						"emulated slave",
						"group1",
						$emulated_slave_host_only,
						$emulated_slave_port,
						1, /* mode: 0 = OFFFLINE, 1 = READ_ONLY, 2 = WRITE_ONLY, 3 = READ_WRITE */
						2, /* status: 0 = SPARE, 1 = SCALE, 2 = SECONDARY, 3 = PRIMARY */
						1.0
					),
					array(
						"emulated master",
						"group_global",
						$emulated_master_host_only,
						$emulated_master_port,
						3, /* mode: 0 = OFFFLINE, 1 = READ_ONLY, 2 = WRITE_ONLY, 3 = READ_WRITE */
						3, /* status: 0 = SPARE, 1 = SCALE, 2 = SECONDARY, 3 = PRIMARY */
						1.0 /* weight */
					),
				)
			);
		return $reply;
	}

	public function dump_shard_index($method, $params, $data) {
		/* hackish... one fine day... */
		global $emulated_master_host_only, $emulated_master_port, $emulated_master_socket,
				$emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket;

		ms_emulated_fabric_server_log(sprintf("Request '%s': %s", $method, var_export($params, true)));

		$reply = NULL;
		if ($this->standard_defects($method, $reply)) {
			return $reply;
		}

		/* send proper reply */
			$reply = array(
				true,
				0, 0, "",
				array(
					array(
						"1", /* Bound - Lower bound */
						"1", /* MapID - shard mapping */
						"1", /* ShardID */
						"group1"
					),
				)
			);
		return $reply;
	}

	public function dump_shard_maps($method, $params, $data) {
		/* hackish... one fine day... */
		global $emulated_master_host_only, $emulated_master_port, $emulated_master_socket,
				$emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket;

		ms_emulated_fabric_server_log(sprintf("Request '%s': %s", $method, var_export($params, true)));

		$reply = NULL;
		if ($this->standard_defects($method, $reply)) {
			return $reply;
		}

		/* send proper reply */
			$reply = array(
				true,
				0, 0, "",
				array(
					array(
						"1", 		/* MapID */
						"RANGE",	/* Type - was... 0=LIST, 1=RANGE, 2=MD5 HAsh, now real name */
						"group_global"  /* Name of the global group */
					),
				)
			);
		return $reply;
	}

	public function dump_shard_tables($method, $params, $data) {
		/* hackish... one fine day... */
		global $emulated_master_host_only, $emulated_master_port, $emulated_master_socket,
				$emulated_slave_host_only, $emulated_slave_port, $emulated_slave_socket;

		ms_emulated_fabric_server_log(sprintf("Request '%s': %s", $method, var_export($params, true)));

		$reply = NULL;
		if ($this->standard_defects($method, $reply)) {
			return $reply;
		}

		/* send proper reply */
			$reply = array(
				true,
				0, 0, "",
				array(
					array(
						"database", 		/* Database */
						"table",		/* Table */
						"column", 		/* Column */
						"1"			/* MapID */
					),
				)
			);
		return $reply;
	}

	protected function standard_defects($method, &$reply) {
		if (!isset($this->defects[$method])) {
			ms_emulated_fabric_server_log(sprintf("No defect registered", $method));
			return false;
		}

		$what = $this->defects[$method];
		$this->consume_defect($method);
		if ('empty' == $what) {
			ms_emulated_fabric_server_log(sprintf("Defect '%s' emulated", $what, $method));
			$reply = array();
			return true;
		} else if ('big' == $what) {
			ms_emulated_fabric_server_log(sprintf("Defect '%s' emulated", $what, $method));
			$reply = str_repeat('0123456789', 250000);
			return true;
		} else {
			ms_emulated_fabric_server_log(sprintf("Unknown defect '%s'", $what));
		}

		return false;
	}
}
if (!isset($HTTP_RAW_POST_DATA)) {
	$post = file_get_contents("php://input");
} else {
	$post = $HTTP_RAW_POST_DATA;
}
$m = new ms_emulated_fabric($post);