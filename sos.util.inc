<?
/*
 * Several utility functions and configuration used only by the sos module.
 */

define('SOS_SESSION_COOKIE_PREFIX', 'sos');
define('FORCE_SSL', FALSE);
define('DEBUG', FALSE);

//function _sos_generate_global_session_id($userid, $nonce) {
function _sos_generate_sos_session_id() {
	$sessid = hash("sha256", mt_rand());
	return $sessid;
}

// Get the token that will be sent to the browser and thereby distributed to the federated sites.
function _sos_get_sos_session_token($sos_sid) {
	$sesskey = variable_get('sos_session_id_private_key', FALSE);
	if ($sesskey === FALSE) {
		watchdog('sos', "No private session key established", WATCHDOG_ERROR);
		return FALSE; // %%% return something else? abort request?
	}
	$sesstoken = hash_hmac("sha256", $sos_sid, $sesskey);
	return $sesstoken;
}

function _sos_check_cookie_integrity($key, $value) {
	$prefix_length = strlen(SOS_SESSION_COOKIE_PREFIX);
	if (substr($key, 0, $prefix_length) !== SOS_SESSION_COOKIE_PREFIX) {
		return FALSE;
	}
	$token = substr($key, $prefix_length);
	$token_comp = _sos_get_sos_session_token($value);
	if ($token !== $token_comp || empty($token)) {
		return FALSE;
	}
	return TRUE;
}

// Given a subdomain, is it part of the SOS federation and enabled?
function _sos_is_subdomain_enabled($subdomain) {
	$site = db_fetch_object(db_query("SELECT * FROM {sos_sites} WHERE subdomain='%s'", $subdomain));
	if ($site) {
		if ($site->status == 1) {
			return true;
		}
	}
	return false;	
}

// Parse the subdomain from the REFERER header.
function _sos_referer_subdomain() {
	$ref = $_SERVER['HTTP_REFERER'];
	$refinfo = parse_url($ref);
	return $refinfo['host'];
}

function _sos_get_sos_sid() {
	static $sos_sid = FALSE;
	if ($sos_sid !== FALSE) return $sos_sid;

	// Get the session identifier local to this domain.
	$sid = session_id();

	// Check for a sos_sid previously entered in the sessions table.
	$sos_sid_database = _sos_get_sos_sid_database();
	$sos_token_database = _sos_get_sos_session_token($sos_sid_database);

	// Retrieve any previously set cookie and check its authenticity.
	$cookie_key = SOS_SESSION_COOKIE_PREFIX.$sos_token_database;
	if (isset($_COOKIE[$cookie_key])) {
		$sos_sid_cookie = $_COOKIE[$cookie_key];
		if (!_sos_check_cookie_integrity($cookie_key, $sos_sid_cookie)) {
			return FALSE;
		}
	}

	// If this is a linking request to sos-set-cookie or sos-logo, then get the sos_sid from the request parameters.
	if (isset($_GET['sos_sid'])) {
		// Check the REFERER header to make sure this request is from an authorized site.
		$refsubdomain = _sos_referer_subdomain();
		if (_sos_is_subdomain_enabled($refsubdomain)) {
			$sos_sid_request = $_GET['sos_sid'];
		} else {
			watchdog('sos', "SOS request received from referrer not in federation: $refsubdomain", WATCHDOG_WARNING);
		}
	}

	// Verify the consistency of all of the sos session ids collected so far and choose one to use.
	$sos_sid = _sos_reconsile_sos_session_ids($sos_sid_cookie, $sos_sid_database, $sos_sid_request);

	// %%% useful debugging, remove before deployment
	if (DEBUG) {
		drupal_set_message("db: ".$sos_sid_database);
		drupal_set_message("ck: ".$sos_sid_cookie);
		drupal_set_message("rq: ".$sos_sid_request);
		drupal_set_message("sid: ".$sos_sid);
	}

	return $sos_sid;
}

/*
 * Determine if the current request is secure.
 */
function _sos_is_secure() {
	return (isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] != 'off');
}

/*
 * Return the constructed HTML for the block of federated logos for SOS ID propagation.
 */
function _sos_render_logos() {
	$sos_sid = _sos_get_sos_sid();
	//$sos_token = _sos_get_sos_session_token($sos_sid);
	$thissite = sos_get_local_site();

	$sos_markup = "";
	if (FORCE_SSL && !_sos_is_secure()) {
		watchdog('sos', "Attempt to display SOS logos in an insecure context", WATCHDOG_WARNING);
		$sos_markup .= "Unable to display SOS logos";
	} else {
		foreach (sos_get_all_active_sites() as $site) {
			// Don't need an extra request to initiate a session for the current domain.
			if ($site->site_id == $thissite->site_id) {
				continue;
			}

			// Construct the base url for SOS synchronization.
			$sos_url = ((FORCE_SSL || _sos_is_secure()) ? "https" : "http").'://'.$site->subdomain.'/sos/logo';
			$sos_url .= '?sos_sid='.$sos_sid;

			$sos_markup .= '<div id="sos-container-'.$site->site_id.'" class="sos-container">';
			// %%% Optionally output site name and domain, or surround logo with a link.
			$sos_markup .= '<img id="sos-logo-'.$site->site_id.'" class="sos-logo" src="'.$sos_url.'" />';
			$sos_markup .= '</div>';
			//$sos_markup .= '<iframe id="sos-frame-'.$dom['domain_id'].'" class="sos-frame" src="'.$sos_url.'" />';
		}
	}

	return $sos_markup;
}

/*
 * Generate a cryptographically secure key.
 */
function _sos_generate_key() {
	// %%% !!!
	return hash("sha256", mt_rand());
}

/*
 * Compare all the global session ids that could come from various sources, and return the correct one.
 */
function _sos_reconsile_sos_session_ids($sid_cookie=null, $sid_database=null, $sid_request=null) {
	
	// If we've got nothing, generate a brand new global_sid.
	if (empty($sid_cookie) && empty($sid_database) && empty($sid_request)) {
		$sid = _sos_generate_sos_session_id();
		return $sid;
	}

	// The cookie and database values should always be the same except the initial request (when cookie is blank).
	if ($sid_cookie != $sid_database) {
		if (empty($sid_cookie)) {
			// Cookie assumes the value from the database; it has been set by sos_boot, but is not detectable yet.
			// %%% This is an substantial kludge, but it's currently just for safety since it shouldn't affect below.
			$sid_cookie = $sid_database;
		} else {
			watchdog('sos', "Unable to reconsile sos_sids:\n\tcookie: $sid_cookie\n\tdatabase: $sid_database\n\trequest: $sid_request", WATCHDOG_ERROR); 
			return null;
		}
	}

	// If all values are the same, that's easy.
	if ($sid_database == $sid_request) return $sid_database;

	// Normal case where we're not trying to actively link domains together
	if (empty($sid_request)) return $sid_database;

	// If we get here, sid_request is set and a domain is actively trying to link to this one (or somebody's hacking).
	
	// Normal linking case whether the browser hasn't visited this site previously.
	if (empty($sid_cookie) && empty($sid_database)) return $sid_request;

	// If we get here, all 3 values are set, sid_cookie and sid_database are the same, but sid_request is different.
	
	// This could happen if the browser had previously visited this site.
	// If the sessions are both for an anonymous user, we can link them.
	$thisdom = domain_get_domain();
	$session_result = db_query("SELECT uid FROM {sessions} WHERE sos_sid='%s' AND domain='%s'", $sid_database, $thisdom);
	while ($sess = db_fetch_object($session_result)) {
		$sessuid = $sess->uid;
		if ($sessuid != 0) {
			// This could happen if the user logged in to this or another affiliated site previously
			// and the session linking process failed.
			// %%% Is there something we can do to reconsile this? After login maybe?
			watchdog('sos', "Unable to reconsile SOS ids:\n\tcookie: $sid_cookie\n\tdatabase: $sid_database\n\trequest: $sid_request", WATCHDOG_ERROR); 
			return null;
		}
	}
	
	// This means that the browser established an anonymous session with this site previously
	// and is at an affiliate site. We can sync the sessions.
	return $sid_request;
}

function _sos_login_connected_sites($account, $sos_sid) {
	if (module_exists('domain')) {
		$sites = sos_get_all_active_sites();
		$thissite = sos_get_local_site();
		foreach ($sites as $site) {
			if ($site->domain_id > -1 && $site->site_id != $thissite->site_id) {
				db_query("UPDATE {sessions} SET uid=%d WHERE sos_sid='%s' AND domain=%d", $account->uid, $sos_sid, $site->domain_id);
			}
		}
	}
}

function _sos_logout_connected_sites($account, $sos_sid) {
	if (module_exists('domain')) {
		$sites = sos_get_all_active_sites();
		$thissite = sos_get_local_site();
		foreach ($sites as $site) {
			// %%% need to clean up obsolete cookies; maybe mark sessions entry for cleanup and handle when site is
			// directly accessed
			if ($site->domain_id > -1 && $site->site_id != $thissite->site_id) {
				db_query("DELETE FROM {sessions} WHERE uid=%d AND domain=%d", $account->uid, $site->domain_id);
			}
		}
	}
}

// The sos_sid should have been populated during hook_boot.
function _sos_get_sos_sid_database() {
	static $sos_sid;
	if (!empty($sos_sid)) return $sos_sid;

	$sid = session_id();
	$sess = db_fetch_object(db_query("SELECT sos_sid FROM {sessions} WHERE sid='%s'", $sid));
	$sos_sid = $sess->sos_sid;

	return $sos_sid;
}

function _sos_deactivate_site($siteid) {
	if (!is_numeric($siteid)) {
		watchdog('argument', "Invalid siteid for deactivation: $siteid", WATCHDOG_WARNING);
		return FALSE;
	}
	// %%% some other kind of validation? check if site id exists and is active?
	
	// %%% notify other sites? other actions?
	
	db_query("UPDATE {sos_sites} SET status=0 WHERE site_id=%d", $siteid);
}

function _sos_activate_site($siteid) {
	if (!is_numeric($siteid)) {
		watchdog('argument', "Invalid siteid for activation: $siteid", WATCHDOG_WARNING);
		return FALSE;
	}
	// %%% some other kind of validation? check if site id exists and is inactive?
	
	db_query("UPDATE {sos_sites} SET status=1 WHERE site_id=%d", $siteid);
}


