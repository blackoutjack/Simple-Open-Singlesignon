<?
/*
 * Message types
 *
 */
define('SOS_PING', 0);
define('SOS_LOGIN', 1);
define('SOS_LOGOUT', 2);
define('SOS_GLOBAL_SID', 3);

/*
 * General configuration that probably shouldn't be changed.
 */
define('USE_SSL', TRUE);
define('COMMUNICATION_ENDPOINT', 'sos/reception');

/*
 * PUBLIC functions
 */

function sos_send_login_messages($uid) {
	$sites = sos_get_all_active_sites();
	$thissite = sos_get_local_site();
	foreach ($sites as $site) {
		// Don't send a message to yourself.
		if ($site->subdomain == $thissite->subdomain) continue;

		sos_send_message($site, SOS_LOGIN, $uid);
	}
}

function sos_send_logout_messages($uid) {
	$sites = sos_get_all_active_sites();
	$thissite = sos_get_local_site();
	foreach ($sites as $site) {
		// Don't send a message to yourself.
		if ($site->subdomain == $thissite->subdomain) continue;

		sos_send_message($site, SOS_LOGOUT, $uid);
	}
}

function sos_send_message($recipient, $message_type=SOS_PING, $uid=NULL) {
	
	// Construct payload and HTTP method based on message_type and contents.
	switch ($message_type) {
		case SOS_PING:
			$method = 'GET';
			$payload = 'ping';
			break;
		case SOS_LOGIN:
			$method = 'POST';
			
			// %%%	
			break;
		case SOS_LOGOUT:
			$method = 'POST';
			// %%%
			break;
		case SOS_GLOBAL_SID:
			$method = 'POST';
			// %%%
			break;
		default:
			watchdog('argument', "Unknown message type: $message_type", WATCHDOG_WARNING);
			return FALSE;
	}

	_sos_communicate($recipient->subdomain, $payload, $method);
}



/*
 * PRIVATE functions - not really, but should probably only be called from within this file.
 */

function _sos_communicate($host, $payload, $method='GET') {
	$endpoint = COMMUNICATION_ENDPOINT;
	// %%% need per-site endpoint
	$host = (USE_SSL ? 'https' : 'http').'://'.$host.'/'.$endpoint;
	$method = strtoupper($method);

	$conn = curl_init($host);

	curl_setopt($conn, CURLOPT_RETURNTRANSFER, TRUE);
	if ($method == 'POST') {
		curl_setopt($conn, CURLOPT_POST, TRUE);
	}
	//curl_setopt($curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC); 
	//curl_setopt($curl, CURLOPT_USERPWD, "username:password"); 
	//curl_setopt($curl, CURLOPT_SSLVERSION,3); 
	//curl_setopt($curl, CURLOPT_SSL_VERIFYPEER, FALSE); 
	//curl_setopt($curl, CURLOPT_SSL_VERIFYHOST, 2); 
	//curl_setopt($curl, CURLOPT_HEADER, true); 
	//curl_setopt($curl, CURLOPT_POST, true); 
	//curl_setopt($curl, CURLOPT_POSTFIELDS, $params ); 
	curl_setopt($curl, CURLOPT_USERAGENT, "Mozilla/4.0 (compatible; MSIE 5.01; Windows NT 5.0)"); // %%% something cool?

	$response = curl_exec($conn);
	if ($response === FALSE) {
		watchdog('sos_comm', "Error while communicating with $host", WATCHDOG_ERROR);
	}

	return $response;
}


