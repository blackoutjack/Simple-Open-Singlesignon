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

  $form['sos']['sos_enabled'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enabled'),
    '#default_value' => variable_get('sos_enabled', false),
    '#description' => t('Switch SOS services on and off.'),
  );

  $form['#validate'][] = 'sos_settings_validate';

  return system_settings_form($form);
}

/**
 * Validate callback for SOS configuration form.
 */
function sos_settings_validate($form, &$form_state) {

}

