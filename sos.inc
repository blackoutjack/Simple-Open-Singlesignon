<?
/*
 * Several utility functions used only by the sos module.
 */

function _sos_generate_global_session_id() {
	$gsid = mt_rand(); // %%%
	return $gsid;
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
		watchdog('sos_error', "Unable to reconsile global_ids:\n\tcookie: $gsid_cookie\n\tdatabase: $gsid_database\n\trequest: $gsid_request" 
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
			watchdog('sos_error', "Unable to reconsile global_ids:\n\tcookie: $gsid_cookie\n\tdatabase: $gsid_database\n\trequest: $gsid_request" 
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
function _sos_register_remote_site($domain, $iconurl, $sitename='', $global_sid=0) {

	db_query(
		"INSERT INTO {sos_sites} (subdomain,site_name,icon_url,status,domain_id,global_site_id)".
		" VALUES ('%s','%s','%s',%d,%d,%d)", $domain, $sitename, $iconurl, 0, 
	);

}


function _sos_activate_site($siteid) {
	if (!is_numeric($siteid)) {
		watchdog('argument', "Non-numeric siteid: $siteid", WATCHDOG_WARNING);
		return FALSE;
	}
	// %%% some other kind of validation?
	
	// Test communication with other site
	_sos_contact_remote_site($siteid, 'ping');

	// %%% test icon_url?
	
	db_query("UPDATE {sos_sites} SET status=1 WHERE site_id=%d", $siteid);
}

// Send a message to a remote site.
function _sos_contact_remote_site($siteid, $payload) {
	

}


