<?

// Buffer output so that errors don't cause partial responses.
ob_start();

include_once 'sos.util.inc';
include_once 'sos.comm.inc';

$ipaddress = $_SERVER['REMOTE_ADDR'];
$remotehost = $_SERVER['REMOTE_HOST'] ? $_SERVER['REMOTE_HOST'] : gethostbyaddr($ipaddress);

if (USE_SSL && (empty($_SERVER['HTTPS']) || $_SERVER['HTTPS'] == 'off')) {
	_sos_reception_watchdog("Non-secure request received");
	return;
}

$method = $_SERVER['REQUEST_METHOD'];

// Pings are the only GET request.
if ($method == 'GET') {
	echo "pong";
} else if ($method == 'POST') {
	// %%%
} else {
	_sos_reception_watchdog("Unimplemented request method received");
}


$output = ob_get_clean();

echo $output;

// Even though this file is an include, we can stop execution here.
exit;

function _sos_reception_watchdog($message) {
	global $ipaddress, $remotehost;
	$message = "$message\r\t$ipaddress\r\t$remotehost";
	watchdog('sos-warning', $message, WATCHDOG_WARNING);
}
