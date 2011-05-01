<?php
// $Id: sos.admin.inc,v 0.1 2011/02/20 05:12:18 blackoutjack Exp $

/**
 * @file
 * Administration code for Simple Open Singlesignon
 *
 * Enable or disable single sign-on services.
 */

/**
 * Menu callback: SOS configuration form.
 */
function sos_admin_settings() {
  // Controller settings.
  $form['sos'] = array(
    '#type' => 'fieldset',
    '#title' => t('Simple Open Singlesignon'),
    '#description' => t("Configure the SOS module."),
  );

  $form['sos']['sos_global_site_id_private_key'] = array(
    '#type' => 'textfield',
    '#title' => t('Global Site ID Private Key'),
    '#default_value' => variable_get('sos_global_site_id_private_key', ''),
    '#description' => t('This key is used to validate other domains that this site will coordinate with.'),
	'#size' => 40,
	'#maxlength' => 256,
  );
  
  // %%% safe to communicate this via ahah?
  $form['sos']['sos_global_site_id_key_generate'] = array(
    '#type' => 'button',
	'#value' => t('Generate'),
	'#ahah' => array(
		'path' => 'sos/generator/site',
		'event' => 'click',
		'method' => 'replace',
		'wrapper' => 'edit-sos-global-site-id-private-key-wrapper',
	),
  );

  $form['sos']['sos_global_session_id_private_key'] = array(
    '#type' => 'textfield',
    '#title' => t('Global Session ID Private Key'),
    '#default_value' => variable_get('sos_global_session_id_private_key', ''),
    '#description' => t('This key is used to validate global user session IDs that are initiated by other sites.'),
	'#size' => 40,
	'#maxlength' => 256,
  );
  
  // %%% safe to communicate this via ahah?
  $form['sos']['sos_global_session_id_key_generate'] = array(
    '#type' => 'button',
	'#value' => t('Generate'),
	'#ahah' => array(
		'path' => 'sos/generator/session',
		'event' => 'click',
		'method' => 'replace',
		'wrapper' => 'edit-sos-global-session-id-private-key-wrapper',
	),
  );

  $form['sos']['sos_enabled'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enabled'),
    '#default_value' => variable_get('sos_enabled', false),
    '#description' => t('Switch SOS services on and off.'),
  );

  $form['#validate'][] = 'sos_settings_validate';

  return system_settings_form($form);
}

/*
 * Callback for ahah key generation buttons.
 */
function sos_ahah_generate_key($type = 'session') {
	$form_state = array('storage' => NULL, 'submitted' => TRUE);
	$form_build_id = $_POST['form_build_id'];
	$form = form_get_cache($form_build_id, $form_state);

	$args = $form['#parameters'];
	$form_id = array_shift($args);
	$form_state['post'] = $form['#post'] = $_POST;
	
	$form['#programmed'] = $form['#redirect'] = FALSE;

	drupal_process_form($form_id, $form, $form_state);	
	$form = drupal_rebuild_form($form_id, $form_state, $args, $form_build_id);

	// Render a new field (with a filled in value) to replace the old one.
	$item_name = ($type == "site") ? 'sos_global_site_id_private_key' : 'sos_global_session_id_private_key';
	$item = $form['sos'][$item_name];
	unset($item['#prefix'], $item['#suffix']);
	$item['#value'] = _sos_generate_key();
	$output = drupal_render($item);

	// Suppress "configuration has been saved" message
	theme('status_messages');

	// Final rendering callback.
	drupal_json(
		array(
			'status' => true,
			'data' => $output,
		)
	);
}

/**
 * Validate callback for SOS configuration form.
 *
 * Force the keys to be filled in if SOS is enabled.
 * Unset the 'Generate' button values so they aren't stored.
 */
function sos_settings_validate($form, &$form_state) {
	$enabled = $form_state['values']['sos_enabled'];
	if ($enabled) {
		$sitekey = $form_state['values']['sos_global_session_id_private_key'];
		$sesskey = $form_state['values']['sos_global_site_id_private_key'];
		// %%% !!! implement stricter checks for key strength
		if (empty($sitekey)) {
			form_set_error('sos_global_site_id_private_key', 'Site key must be set if SOS is enabled.');
		}
		if (empty($sesskey)) {
			form_set_error('sos_global_session_id_private_key', 'Session key must be set if SOS is enabled.');
		}
	}

	unset($form_state['values']['sos_global_session_id_key_generate']);
	unset($form_state['values']['sos_global_site_id_key_generate']);
}

/*
 * List current sites in the ring and manually add others.
 */
function sos_sites_list(&$form_state) {
	$form['sos_sites'] = array(
		'#type' => 'fieldset',
		'#title' => t('SOS Sites'),
		'#description' => t('A list of the sites currently in the SOS ring'),
	);

	$thissite = sos_get_local_site();

	$header = array(
		array('data' => t('Site Id'), 'field' => 's.site_id'),
		array('data' => t('Global Id'), 'field' => 's.global_site_id'),
		array('data' => t('Domain'), 'field' => 's.subdomain'),
		array('data' => t('Site name'), 'field' => 's.site_name'),
		array('data' => t('Status'), 'field' => 's.status'),
	);

	$sql = 'SELECT s.* FROM {sos_sites} s '.tablesort_sql($header);
	$result = pager_query($sql, 25); // %%% configure max list size

	$sites = array();
	while ($data = db_fetch_object($result)) {
		$sites[] = $data;
	}

	foreach ($sites as $site) {
		// Set the status description.
		$status = ($site->status == 1) ? t('Active') : t('Inactive');

		$row = array($site->site_id, $site->global_site_id, check_plain($site->subdomain), check_plain($site->site_name), $status);

		$actions = array();
		$actions[] = l(t('edit'), 'admin/build/sos/sites/edit/'.$site->site_id);
		if ($site->site_id != $thissite->site_id) {
			$actions[] = l(t('delete'), 'admin/build/sos/sites/delete/'.$site->site_id); // %%% force disable first?
		}

		// Add the actions.
		$row[] = theme('item_list', $actions);
		$rows[] = $row;
	}
	
	$output = '';
	if (!empty($rows)) {
		$output .= theme_table($header, $rows, array('id' => 'sos-site-list'));
		$output .= theme('pager', NULL, 25, 0); // %%% same configuration of max list size
	} else {
		$output .= t('No domains have been configured.');
	}

	return $output;
}

function sos_load_site($site_id) {
	if (!is_numeric($site_id)) {
		return NULL;
	}
	if ($site_obj = db_fetch_object(db_query('SELECT s.* FROM {sos_sites} s WHERE site_id=%d', $site_id))) {
		return $site_obj;
	}
	return NULL;
}

function sos_site_edit_form(&$form_state, $site_id = NULL) {
	$thissite = sos_get_local_site();
	if ($site_id != NULL) {
		$site = sos_load_site($site_id); // %%% then test disabling site 3, then take screenshot, send to Sue
		$form_title = t('Edit SOS site');
		$form_description = t('Alter the attributes of a site in the SOS ring.');
	} else {
		$site = NULL;
		$form_title = t('Add SOS site');
		$form_description = t('Add a new site to the SOS ring.');
	}

	/*
	if ($thissite->site_id == $site->site_id) {
		watchdog('sos', "Attempt to edit the local SOS site", WATCHDOG_ERROR);
		return FALSE;
	}
	*/


	$form['site_id'] = array(
		'#type' => 'hidden',
		'#value' => $site_id,
	);
	$form['sos_edit'] = array(
		'#type' => 'fieldset',
		'#title' => $form_title,
		'#description' => $form_description,
	);

	$form['sos_edit']['site_name'] = array(
		'#type' => 'textfield',
		'#title' => 'Site name',
		'#size' => 40,
		'#maxlength' => 256,
		'#description' => t('Enter the name of the site. This is only used for identification.'),
		'#required' => TRUE,
		'#default_value' => $site->site_name,
	);
	$form['sos_edit']['subdomain'] = array(
		'#type' => 'textfield',
		'#title' => 'Site domain',
		'#size' => 40,
		'#maxlength' => 256,
		'#description' => t('Enter the hostname (domain or subdomain) of the site.  No http:// or slashes.'),
		'#required' => TRUE,
		'#default_value' => $site->subdomain,
	);
	
	if ($site) {
		$form['sos_edit']['global_site_id'] = array(
			'#type' => 'markup',
			'#value' => t('Global site identifier').': '.$site->global_site_id,
		);
	}
	$form['sos_edit']['status'] = array(
		'#type' => 'checkbox',
		'#title' => 'Active',
		'#description' => t("Sites must be active to participate in SOS."),
		'#default_value' => $site->status,
	); 
	$form['sos_edit']['submit'] = array(
		'#type' => 'submit',
		'#title' => 'Save',
		'#default_value' => 'Save',
	);

	return $form;
}

function sos_site_edit_form_validate($form, &$form_state) {
	// %%% check for duplicate subdomain
}

function sos_site_edit_form_submit($form, &$form_state) {
	// %%% save to database
	if ($form_state['values']['op'] == $form_state['values']['cancel']) {
		return;
	}
	
	$siteid = $form_state['values']['site_id'];
	$subdomain = $form_state['values']['subdomain'];
	$sitename = $form_state['values']['site_name'];
	$status = $form_state['values']['status'];
	if (DEBUG) {
		dump("sd: ".$subdomain);
		dump("sn: ".$sitename);
		dump("st: ".$status);
	}

	// Update case
	if ($siteid) {
		$orig_subdomain = $form['sos_edit']['subdomain']['#default_value'];
		$orig_sitename = $form['sos_edit']['site_name']['#default_value'];
		$orig_status = $form['sos_edit']['status']['#default_value'];
		if (DEBUG) {
			dump("osd: ".$orig_subdomain);
			dump("osn: ".$orig_sitename);
			dump("ost: ".$orig_status);
		}
	
		// Update non-coordinated data.
		db_query("UPDATE {sos_sites} SET site_name='%s' WHERE site_id=%d", $sitename, $siteid);

		// Change subdomain and/or status through utility calls so that state remains consistent. 
		$domain_change = ($orig_subdomain != $subdomain);
		if ($domain_change) {
			if ($orig_status) {
				_sos_deactivate_site($siteid);
			}

			_sos_change_subdomain($siteid, $subdomain);

			if ($status) {
				_sos_activate_site($siteid);
			}
		} else {
			$status_change = ($orig_status != $status);
			if ($status_change) {
				if ($status) {
					_sos_activate_site($siteid);
				} else {
					_sos_deactivate_site($siteid);
				}
			}
		}
		

	// Insert case
	} else {
		$gsiteid = _sos_calculate_global_site_id($subdomain);
		// Insert but hold off on activation.
		db_query(
			"INSERT INTO {sos_sites} (subdomain, site_name, global_site_id, status) VALUES ('%s', '%s', '%s', 0)",
			$subdomain, $sitename, $gsiteid
		);
		$siteid = db_insert_id();
		if ($siteid && $status) {
			_sos_activate_site($siteid);
		}
	}
}

function sos_site_delete_form(&$form_state, $site_id) {
	$thissite = sos_get_local_site();
	$site = sos_load_site($site_id);

	if (!$site) {
		watchdog('sos', "Invalid site ID $site_id submitted to delete form", WATCHDOG_ERROR);
		return FALSE;
	}
	if ($thissite->site_id == $site->site_id) {
		watchdog('sos', "Attempt to delete the local SOS site", WATCHDOG_ERROR);
		return FALSE;
	}

	$form['site_id'] = array(
		'#type' => 'hidden',
		'#value' => $site_id,
	);
	$question = 'Are you sure you want to delete SOS site <b>'.check_plain($site->site_name).'</b>?';
	$path = 'admin/build/sos/sites';
	$description = NULL;
	$yes = 'Delete';
	$no = 'Cancel';
	$name = 'confirm';
	return confirm_form($form, $question, $path, $description, $yes, $no, $name);
}

function sos_site_delete_form_submit($form, &$form_state) {
	if ($form_state['values']['op'] == $form_state['values']['confirm']) {
		$siteid = $form_state['values']['site_id'];
		// Deactivate site to run any actions that are needed.
		_sos_deactivate_site($siteid);
		// %%% check that deactivation was successful before deleting?
		db_query("DELETE FROM {sos_sites} WHERE site_id=%d", $siteid);
	} else {
		watchdog('sos', "Invalid submission of site delete form", WATCHDOG_ERROR);
	}
}
