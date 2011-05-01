<?
/*
 * Several utility functions and configuration used only by the sos module.
 */

define('SOS_GLOBAL_SESSION_COOKIE', 'SOS');
define('FORCE_SSL', FALSE);
define('DEBUG', FALSE);

//function _sos_generate_global_session_id($userid, $nonce) {
function _sos_generate_global_session_id() {
	$gsessid = hash_hmac("sha256", mt_rand());
	return $gsessid;
}

// Get the token that will be sent to the browser and thereby distributed to the federated sites.
function _sos_get_global_session_token($global_session_id) {
	$sesskey = variable_get('sos_global_session_id_private_key', FALSE);
	if ($sesskey === FALSE) {
		watchdog('sos', "No private session key established", WATCHDOG_ERROR);
		return FALSE; // %%% return something else? abort request?
	}
	$gsesstoken = hash_hmac("sha256", $global_session_id, $sesskey);
	return $gsesstoken;
}

function _sos_get_global_sid() {
	static $global_sid = FALSE;
	if ($global_sid !== FALSE) return $global_sid;

	// Get the session identifier local to this domain.
	$sid = session_id();

	// Retrieve any previously set global_session_id cookie.
	if (isset($_COOKIE['global_session_id'])) {
		$global_sid_cookie = $_COOKIE[SOS_GLOBAL_SESSION_COOKIE];
	}

	// If this is a linking request to sos-set-cookie or sos-logo, then get the global_sid from the request parameters.
	if (isset($_GET['global_sid'])) {
		// %%% use nonce to verify
		$global_sid_request = $_GET['global_sid'];
	}

	// Check for a global_sid previously entered in the sessions table.
	$global_sid_database = _sos_get_global_sid_database();

	// Verify the consistency of all of the global session ids collected so far and choose one to use.
	$global_sid = _sos_reconsile_global_session_ids($global_sid_cookie, $global_sid_database, $global_sid_request);

	// %%% useful debugging, remove before deployment
	if (DEBUG) {
		drupal_set_message("db: ".$global_sid_database);
		drupal_set_message("ck: ".$global_sid_cookie);
		drupal_set_message("rq: ".$global_sid_request);
		drupal_set_message("gsid: ".$global_sid);
	}

	return $global_sid;
}
/*
 * Generate a unique and consistent hash of the subdomain string.
 */
function _sos_calculate_global_site_id($subdomain) {
	$sitekey = variable_get('sos_global_site_id_private_key', FALSE);
	if ($sitekey === FALSE) {
		watchdog('sos', "No private site key established", WATCHDOG_ERROR);
		return FALSE; // %%% return something else? exit?
	}
	
	$gsiteid = hash("sha256", $subdomain.$sitekey);
	return $gsiteid;
}

/*
 * Determine if the current request is secure.
 */
function _sos_is_secure() {
	return (isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] != 'off');
}

/*
 * Return the constructed HTML for the block of federated logos for global ID propagation.
 */
function _sos_render_logos() {
	$global_sid = _sos_get_global_sid_database();
	$thissite = sos_get_local_site();

	$sos_markup = "";
	if (FORCE_SSL && !_sos_is_secure()) {
		watchdog('sos', "Attempt to display SOS logos in an insecure context", WATCHDOG_WARNING);
		$sos_markup .= "Unable to display SOS logos";
	} else {
		foreach (sos_get_all_active_sites() as $site) {
			// Don't need an iframe to initiate a session for the current domain.
			if ($site->site_id == $thissite->site_id) {
				continue;
			}

			// Construct the base url for SOS synchronization.
			$sos_url = ($_SERVER['HTTPS'] ? "https" : "http").'://'.$site->subdomain.'/sos/logo';
			$sos_url .= '?global_sid='.$global_sid;

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
	return "here is a key";
}

/*
 * Compare all the global session ids that could come from various sources, and return the correct one.
 */
function _sos_reconsile_global_session_ids($gsid_cookie=null, $gsid_database=null, $gsid_request=null) {

	// If we've got nothing, generate a brand new global_sid.
	if (empty($gsid_cookie) && empty($gsid_database) && empty($gsid_request)) {
		$gsessid = _sos_generate_global_session_id();
		return $gsessid;
	}

	// The cookie and database values should always be the same except the initial request (when cookie is blank).
	if ($gsid_cookie != $gsid_database) {
		if (empty($gsid_cookie)) {
			// Cookie assumes the value from the database; it has been set by sos_boot, but is not detectable yet.
			// %%% This is an substantial kludge, but it's currently just for safety since it shouldn't affect below.
			$gsid_cookie = $gsid_database;
		} else {
			watchdog('sos', "Unable to reconsile global_ids:\n\tcookie: $gsid_cookie\n\tdatabase: $gsid_database\n\trequest: $gsid_request", WATCHDOG_ERROR); 
			return null;
		}
	}

	// If all values are the same, that's easy.
	if ($gsid_database == $gsid_request) return $gsid_database;

	// Normal case where we're not trying to actively link domains together
	if (empty($gsid_request)) return $gsid_database;

	// If we get here, gsid_request is set and a domain is actively trying to link to this one (or somebody's hacking).
	
	// Normal linking case whether the browser hasn't visited this site previously.
	if (empty($gsid_cookie) && empty($gsid_database)) return $gsid_request;

	// If we get here, all 3 values are set, gsid_cookie and gsid_database are the same, but gsid_request is different.
	
	// This could happen if the browser had previously visited this site.
	// If the sessions are both for an anonymous user, we can link them.
	$thisdom = domain_get_domain();
	$session_result = db_query("SELECT uid FROM {sessions} WHERE sos_sid='%s'", $gsid_database);
	while ($sess = db_fetch_object($session_result)) {
		$sessuid = $sess->uid;
		if ($sessuid != 0) {
			// This could happen if the user logged in to this or another affiliated site previously.
			// %%% Is there something we can do to reconsile this? After login maybe?
			watchdog('sos', "Unable to reconsile global_ids:\n\tcookie: $gsid_cookie\n\tdatabase: $gsid_database\n\trequest: $gsid_request", WATCHDOG_ERROR); 
			return null;
		}
	}
	
	// This means that the browser established a session with this site previously and is at an affiliate site.
	// We can sync the sessions.
	return $gsid_request;
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
		//$fl = fopen("c:/projects/satarababy.com/test.txt", 'w');
		//fwrite($fl, "Testing: ".var_export($account,true)."\n");
		foreach ($sites as $site) {
			if ($site->domain_id > -1 && $site->site_id != $thissite->site_id) {
				//fwrite($fl, "Site: ".var_export($site,true)."\n");
				db_query("DELETE FROM {sessions} WHERE uid=%d AND domain=%d", $account->uid, $site->domain_id);
			}
		}
		//fclose($fl);
	}
}

// The global_sid should have been populated during hook_boot.
function _sos_get_global_sid_database() {
	static $global_sid;
	
	if (!empty($global_sid)) return $global_sid;

	$sid = session_id();
	$sess = db_fetch_object(db_query("SELECT sos_sid FROM {sessions} WHERE sid='%s'", $sid));
	$global_sid = $sess->sos_sid;

	return $global_sid;
}

/*
 * Another site has requested to be part of the SOS ring. Authenticate and add it to the database.
 * The site must be explicitly activated by calling _sos_activate_site().
 */
function _sos_register_remote_site($domain, $iconurl, $sitename='', $global_site_id=0) {

	db_query(
		"INSERT INTO {sos_sites} (subdomain,site_name,icon_url,status,domain_id,global_site_id)".
		" VALUES ('%s','%s','%s',%d,%d,%d)", $domain, $sitename, $iconurl, 0, $global_site_id
	);

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
	
	// Test communication with other site
	_sos_contact_remote_site($siteid, 'ping');

	// %%% test icon_url?
	
	db_query("UPDATE {sos_sites} SET status=1 WHERE site_id=%d", $siteid);
}

// Send a message to a remote site.
function _sos_contact_remote_site($siteid, $payload) {
	

}

/*
 * Ensure consistency between global site ids and the subdomain they're base on.
 */
function _sos_change_subdomain($siteid, $newsubdomain) {
	$gsiteid = _sos_calculate_global_site_id($newsubdomain);
	db_query("UPDATE {sos_sites} SET global_site_id='%s', subdomain='%s' WHERE site_id=%d", $gsiteid, $newsubdomain, $siteid);
}


