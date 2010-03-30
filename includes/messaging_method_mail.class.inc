<?php
// $Id$
/**
 * @file
 * Drupal Messaging Framework - Send_Method class file
 */

/**
 * Base class for mail sending methods
 */
class Messaging_Method_Mail extends Messaging_Send_Method {
  public $group = 'mail';
  public $type = MESSAGING_TYPE_SEND;
  public $anonymous = TRUE;
  
  /**
   * Get address for user account
   */
  function get_user_address($account) {
    return $account->uid && $account->mail ? $account->mail : NULL;
  }
  /**
   * Get uid for address
   */
  function get_address_uid($mail) {
    return db_result(db_query("SELECT uid FROM {users} WHERE mail = '%s'", $mail));
  }
  /**
   * Validate address
   */
  function address_validate($address) {
    return valid_email_address($address);
  }
  /**
   * Get defaults for mail methods
   */
  function get_defaults() {
    return array(
      'name' => t('Mail'),
      'address_name' => t('E-mail address'),
      'group' => 'mail',
      'type' => MESSAGING_TYPE_SEND,
    );
  }
  
  /**
   * Rebuild message in Drupal mail format
   * 
   * @param $destination
   *   Email destination
   * @param $message
   *   Message object
   * @param $params
   *   Aditional parameters
   * @param $alter
   *   Whether to run the mail_alter hook
   */
  static function mail_prepare($destination, $message, $params, $alter = TRUE) {
  
    // The message 'from' will depend on message sender if present, otherwise default to site mail
    $default_from = variable_get('site_mail', ini_get('sendmail_from'));
    
    if (empty($params['from'])) {
      if (!empty($message->sender_account) && !empty($message->sender_account->mail)) {
        $from = check_plain($message->sender_account->name) . ' <'.$message->sender_account->mail.'>';
      } elseif(!empty($message->sender_name) && $default_from) {
        $from = check_plain($message->sender_name) . ' <'.$default_from.'>';
      } else {
        $from = $default_from;
      }
      $params['from'] = $from;
    } else {
      $from = $params['from'];
    }
  
    // Build the mail object, mimic drupal_mail() format
    $mail = array(
      'id'       => 'messaging_'. (!empty($message->type) ? 'message-'.$message->type : 'message'),
      'to'       => $destination,
      'from'     => $from,
      'language' => !empty($message->language) ? $message->language : language_default(),
      'params'   => $params,
      'subject'  => $message->subject,
      'body'     => $message->body,
      'headers'  => messaging_mail_headers($message, $params),
      'attachments' => !empty($message->files) ? $message->files : array(),
    );  
  
    // Invoke hook_mail_alter() to allow all modules to alter the resulting e-mail.
    if ($alter) {
      drupal_alter('mail', $mail);
    }
    return $mail;
  }
  
  /**
   * Get mail headers. Helper function for mail methods
   * 
   */
  static function mail_headers($message, $params) {
    
    $headers = !empty($params['headers']) ? $params['headers'] : array();
    
    // Add some default headers
    $headers += array(
      'MIME-Version'              => '1.0',
      'Content-Type'              => 'text/plain; charset=UTF-8; format=flowed; delsp=yes',
      'Content-Transfer-Encoding' => '8Bit',
      'X-Mailer'                  => 'Drupal'
    );
    
    $default_from = variable_get('site_mail', ini_get('sendmail_from'));
    $from = !empty($params['from']) ? $params['from'] : $default_from;
  
    // Set default headers depending on data
    $headers += array(
      'From' => $from,
      'Reply-To' => $from,
    );
    
    if ($default_from) {
      // To prevent e-mail from looking like spam, the addresses in the Sender and
      // Return-Path headers should have a domain authorized to use the originating
      // SMTP server. Errors-To is redundant, but shouldn't hurt.
      $more_headers['Sender'] = $more_headers['Return-Path'] = $more_headers['Errors-To'] = $default_from;
      $headers += $more_headers;
    }  
  
    return $headers;
  }
  
  /**
   * Get list of mail methods
   */
  static function mail_methods() {
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
  static function mail_update_user($account) {
    if ($account->status) {
      // Update mail for destinations of this user
      db_query("UPDATE {messaging_destination} SET address = '%s' WHERE uid = %d AND type = 'mail'", $account->mail, $account->uid);
      // Update uid for destinations with this mail
      db_query("UPDATE {messaging_destination} SET uid = %d WHERE uid = 0 AND type = 'mail' AND address = '%s'", $account->uid, $account->mail);
    }
  }
  
}