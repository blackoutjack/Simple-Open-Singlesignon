<?
/*
 * Several utility functions and configuration used only by the sos module.
 */

define('SOS_GLOBAL_SESSION_COOKIE', 'sos_global_session');
define('FORCE_SSL', FALSE);

//function _sos_generate_global_session_id($userid, $nonce) {
function _sos_generate_global_session_id() {
	$sesskey = variable_get('sos_global_session_id_private_key', FALSE);
	if ($sesskey === FALSE) {
		watchdog('sos', "No private session key established", WATCHDOG_ERROR);
		return FALSE; // %%% return something else? exit?
	}

	//$gsid = hash("sha256", $userid.$nonce.$ringsesskey); // %%%
	$gsessid = hash("sha256", mt_rand().$sesskey);
	return $gsessid;
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
	if (empty($gsid_cookie) && empty($gsid_database) && empty($gsid_request)) return _sos_generate_global_session_id();

	// The cookie and database values should always be the same.
	// %%% handle this
	if ($gsid_cookie != $gsid_database) {
		watchdog('sos', "Unable to reconsile global_ids:\n\tcookie: $gsid_cookie\n\tdatabase: $gsid_database\n\trequest: $gsid_request", WATCHDOG_ERROR); 
		return null;
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
	
	// This means that the browser visited this site previously but then went to an affiliate and has accessed the
	// login page.  We can sync the sessions.
	return $gsid_request;
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


