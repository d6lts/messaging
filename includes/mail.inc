<?php
// $Id$
/**
 * @file
 * Drupal Messaging Framework - Mail specific functions
 */

/**
 * Rebuild message in Drupal mail format
 */
function messaging_mail_build($destination, $message, $params, $alter = TRUE) {
  return Messaging_Method_Mail::mail_build($destination, $message, $params, $alter);
}

/**
 * Get mail headers. Helper function for mail methods
 * 
 */
function messaging_mail_headers($params) {
  return Messaging_Method_Mail::mail_headers($params);
}

/**
 * Get list of mail methods
 */
function messaging_mail_methods() {
  $mail_methods = array();
  foreach (messaging_method_info() as $method => $info) {
    if (!empty($info['group']) && $info['group'] == 'mail') {
      $mail_methods[] = $method;
    }
  }
  return $mail_methods;
}

/**
 * Update destinations when a user account is updated, created
 * 
 * The destinations for that mail account will be assigned to the user
 */
function messaging_mail_update_user($account) {
  if ($account->status) {
    // Update mail for destinations of this user
    db_query("UPDATE {messaging_destination} SET address = '%s' WHERE uid = %d AND type = 'mail'", $account->mail, $account->uid);
    // Update uid for destinations with this mail
    db_query("UPDATE {messaging_destination} SET uid = %d WHERE uid = 0 AND type = 'mail' AND address = '%s'", $account->uid, $account->mail);
  }
}

/**
 * Specific mail settings
 */
function messaging_mail_admin_settings($form_state) {
  $default_mail = variable_get('site_mail', ini_get('sendmail_from'));

  $form['mail'] = array(
    '#type' => 'fieldset',
    '#title' => t('Default mail addresses'),
  );
  $form['mail']['messaging_mail_default_from'] = array(
    '#title' => t('Sender mail for outgoing messages coming from the site.'),
    '#type' => 'textfield',
    '#default_value' => variable_get('messaging_mail_default_from', $default_mail),
  );
  $form['mail']['messaging_mail_returnpath'] = array(
    '#title' => t('Mail for return-path and errors-to email address.'),
    '#type' => 'textfield',
    '#default_value' => variable_get('messaging_mail_returnpath', $default_mail),
  );

  $form['format'] = array(
    '#type' => 'fieldset',
    '#title' => t('Sender format'),
    '#description' => t('You can use <i>[name]</i> and <i>[mail]</i> tokens depending on the sender (that may be a user or the system) or use a fixed name and e-mail address for all outgoing mails.'),
  );
  $form['format']['messaging_mail_sender_format'] = array(
    '#title' => t('Format for sender name and address'),
    '#type' => 'textfield',
    '#default_value' => variable_get('messaging_mail_sender_format', '[name] <[mail]>'),
  );
  return system_settings_form($form);
}