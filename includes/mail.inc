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
    if (!empty($method['group']) && $method['group'] == 'mail') {
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
