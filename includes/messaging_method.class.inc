<?php
// $Id$
/**
 * @file
 * Drupal Messaging Framework - Send_Method class file
 */

/**
 * Base class for all Incoming and Sending methods
 */
abstract class Messaging_Method {
  // Method with push delivery. Messages will be pushed to the user using messaging sending methods.
  const TYPE_PUSH = 1;
  // Method type with pull delivery. Messages will be pulled using messaging pull methods
  const TYPE_PULL = 2;
  // Outgoing method
  const TYPE_OUTGOING = 4;
  // Incoming method
  const TYPE_INCOMING = 8;
  // Shorthand type: Push + Outgoing
  const TYPE_SEND = 4;
  // Common properties
  // Method key
  public $method;
  public $type;
  public $title;
  public $name;
  public $group;
  
  /** Message composition and rendering **/
  
  /**
   * Renders full message with header and body
   * 
   * @param $message
   *   Message object
   * @param $info
   *   Sending method info for rendering (glue and filter options)
   */
  static function default_render($message, $info) {
    messaging_include('text.inc');
    messaging_debug('Rendering message', array('message' => $message, 'info' => $info));
    // Apply footer prefix if provided and the message has a footer element. 
    // Note: If message body is a string the final isset($message['body']['footer']) will be true
    if (!empty($info['footer']) && is_array($message->body) && isset($message->body['footer'])) {
      $message->body['footer'] = array('#prefix' => $info['footer'], '#text' => $message->body['footer']);
    }
    // Render separately subject and body info, adding default parameters
    $info += array('glue' => '', 'subject_glue' => '', 'filter' => NULL);
    $message->subject = self::check_subject(self::text_render($message->subject, $info['subject_glue']));
    $message->body = self::text_render($message->body, $info['glue'], $info['filter']); 
    $message->rendered = 1;  
    messaging_debug('Rendered message', array('message' => array('subject' => $message->subject, 'body' => $message->body), 'info' => $info));
    return $message;
  }

  /**
   * Converts strings to plain utf-8 single line
   */
  static function check_subject($text) {
    $text = messaging_text_check_plain($text, NULL);
    // taken from _sanitizeHeaders() in PEAR mail() : http://pear.php.net/package/Mail
    $text = preg_replace('=((0x0A/%0A|0x0D/%0D|\\n|\\r)\S).*=i', NULL, $text);
    return $text;
  }
  
  /**
   * Composes message from different parts, recursively and applies filter
   * 
   * Filter is applied now only once
   * 
   * @param $text
   *   Simple string or array of message parts
   *   It may have named elements like #prefix and #text
   *   or it may be single strings to render straight forward
   * @param $glue
   *   Text to glue all lines together
   * @param $filter
   *   Input format to apply to the results
   */
  static function text_render($text, $glue = '', $filter = NULL) {
    return messaging_text_render($text, $glue, $filter);
  }
  
  /**
   * Clean text of HTML stuff and optionally of line endings
   * 
   * @param $text
   *   Dirty HTML text to be filtered
   * @param $newline
   *   Optional string to be used as line ending
   */
  static function text_clean($text, $newline = NULL) {
    return messaging_text_clean($text, $newline);
  }
  
  /**
   * Truncate messages to given length.  Adapted from node_teaser() in node.module
   */
  static function text_truncate($text, $length) {
    return messaging_text_truncate($text, $length);
  }
}

/**
 * Sending method, implements all specific method functionality
 * 
 * Old callback functions are
 * - send
 * - destination
 */
class Messaging_Send_Method extends Messaging_Method {
  // Default type is send (Outgoing + Push)
  public $type = Messaging_Method::TYPE_SEND;
  // Address type for this method
  public $address_type = 'user';
  public $destination;
  public $glue;
  public $footer;
  public $description;
  public $address_name;
  public $filter;
  // Suitable for anonymous users
  public $anonymous = FALSE;
  // Remaining info array
  protected $info;
  
  function __construct($method, $info = array()) {
    $this->method = $method;
    foreach (array('title', 'name', 'group', 'destination', 'type', 'glue', 'footer', 'description', 'address_name', 'anonymous', 'filter') as $key) {
      if (isset($info[$key])) {
        $this->$key = $info[$key];
        //unset($info[$key]);
      }
    }
    $this->info = $info;
  }
  
  /**
   * Get info property
   */
  function get_info($property = NULL) {
    if ($property) {
      return isset($this->info[$property]) ? $this->info[$property] : NULL;
    }
    else {
      return $this->info;
    }
  }
  /**
   * Get address info property
   */
  function get_address_info($property = NULL) {
    return messaging_address_info($this->address_type, $property);
  }
  /**
   * Send message to address, use sending callback
   */
  function send_address($address, $message) {
    $message->render();
    if ($send_callback = $this->get_info('send callback')) {
      return call_user_func($send_callback, $address, $message, $message->get_params($this->method));
    }
    else {
      watchdog('messaging', 'Message could not be delivered for method %method', array('%method' => $this->method), WATCHDOG_ERROR);
    }
  }

  /**
   * Send message to destination, use sending callback
   */
  function send_destination($destination, $message) {
    return $this->send_address($destination->address, $message);
  }
  /**
   * Get address for user account
   * 
   * @param $account
   *   User account object or uid
   */
  function get_user_address($account) {
    if ($function = $this->get_address_info('user2address callback')) {
      return $function($account);
    }
    elseif ($property = $this->get_address_info('account_property')) {        
      return messaging_user_property($account, $property);
    }
    elseif ($function = $this->get_info('destination callback')) {
      return $function(messaging_user_object($account));
    }
  }

  /**
   * Check user access to this method
   */
  function user_access($account) {
    if (!$account->uid && !$this->anonymous) {
      return FALSE;
    }
    if ($permission = $this->get_info('access')) {
      return user_access($permission, $account);
    }
    else {
      return TRUE;
    }
  }
  /**
   * Get address name
   */
  function address_name() {
    if (empty($this->address_name)) {
      if ($name = $this->get_address_info('name')) {
        $this->address_name = $name;
      }
      else {
        $this->address_name = t('Address');
      }
    } 
    return $this->address_name; 
  }

  /**
   * Get uid for address
   */
  function get_address_uid($address) {
    if ($this->address_type == 'user') {
      return (int)$address;
    }
    elseif ($function = $this->get_address_info('address2uid callback')) {
      return $function($address);
    }
  }
  /**
   * Validate address
   */
  function address_validate($address) {
    if ($function = $this->get_address_info('validate callback')) {
      return $function($address);
    }
    else {
      // The default address will be valid if not empty
      return !empty($address);
    }
  }
  /**
   * Format address for display
   */
  function format_address($address, $html = FALSE) {
    if ($function = $this->get_address_info('format callback')) {
      return $function($address, $html);
    }
    else {
      return check_plain($address);
    }
  }

  /**
   * Message default callback: preprocessing
   * 
   * Decide on queue, log, cron and send options, prepare parameters
   */
  function message_prepare($message) {
    if ($function = $this->get_info('prepare callback')) {
      return $function($message, $this->get_info());
    }
    else {
      return $this->default_prepare($message, $this->type);
    }    
  }
  /**
   * Renders full message with header and body
   * 
   * @param $message
   *   Message object
   * @param $info
   *   Sending method info for rendering (glue and filter options)
   */  
  function message_render($message) {
    if ($function = $this->get_info('render callback')) {
      return $function($message, $this->get_info());
    }
    else {
      return $this->default_render($message, $this->get_info());
    }
  }

  /**
   * The message is about to be sent
   */
  function message_presend($message) {    
  }

  /**
   * The message has been sent
   */
  function message_aftersend($message) {
  }

  /**
   * Message default callback: send iterating over all destinations
   */
  function message_multisend($message) {
    $success = TRUE;
    foreach ($message->destinations as $to) {
      // Be careful with the order of function && value, so they both are evaluated
      $success = $this->send_address($to, $message) && $success;
    }
    // If sent, set time. If failed force logging.
    $success ? ($message->sent = time()) : ($message->log = 1);
    $message->success = $success;
    return $message;
  }
  
  /**
   * Queue message for next delivery
   * 
   * By default it is saved to the store
   */
  function message_queue($message) {
    $message->store();
    $message->success = TRUE;
  }
  
  /**
   * The message has been queued
   */
  function message_afterqueue($message) {
  }
  
  /**
   * Message default callback: preprocessing
   * 
   * Decide on queue, log, cron and send options, prepare parameters
   */
  static function default_prepare($message, $type) {
    messaging_debug('Preparing message for sending', array('message' => $message));
    // If the messaging method is of type push, cron processing will be enabled
    if ($message->queue && ($type & MESSAGING_TYPE_PUSH)) {
      $message->cron = 1;
    }
    // It will be queued always for pull methods, cron disabled though so it will wait till it's pulled
    if (!$message->queue && ($type & MESSAGING_TYPE_PULL)) {
      $message->queue = 1;
      $message->cron = 0;
    }
    return $message;
  }
  
}
