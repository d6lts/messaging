<?php
// $Id$
/**
 * @file
 * Drupal Messaging Framework - Message class file
 * 
 * The steps the message follows are
 * - prepare
 *   Parameters are prepared, alter hook is invoked, etc
 *   The sending method may change at this stage 
 * - presend, send, aftersend
 *   If the message is to be sent right away
 * - queue, afterqueue
 *   If the message is to be queued
 * 
 * The message will be rendered right before sending or before being stored
 */

/**
 * Message class
 * 
 * This is a message with all the information, ready for sending
 */
class Messaging_Message extends Messaging_Object {
  // Database properties
  const DB_TABLE = 'messaging_store';
  const DB_KEY = 'mqid';
  // Unique id
  public $mqid = 0;
  // Message type: outgoing, incoming
  public $type;
  // Sending method
  public $method;
  protected $method_info;
  // Message source, mixed data
  public $source;
  // Sending method specific params
  public $params;
  // Message destination
  public $uid;
  public $destination;
  protected $user;
  // Used when sending to multiple destinations
  public $destinations;
  // Rendered message parts
  public $subject;
  public $body;
  public $files;
  public $parts;
  // Sender information
  public $sender;
  public $sender_name;
  public $sender_address;
  protected $sender_account;
  // Timestamps
  public $created;
  public $sent = 0;
  // Processing parameters
  public $queue = 0;
  public $send = 0;
  public $cron = 0; 
  public $log = 0;
  // Message priority. If > 0 it won't be queued.
  public $priority = 0;
  // Log when we've got an error
  public $log_error = TRUE;
  // Error code, error text
  public $error = 0;
  public $error_msg;
  // Serialized data
  public $data;
  // Temporary processing variables 
  // These are not stored
  public $operations;
  
  // Message status
  public $prepared = FALSE;
  public $rendered = FALSE;
  public $processed = FALSE;
  public $queued = FALSE;
  public $logged = FALSE;
  public $redirect = FALSE;
  public $retry = FALSE;
  // Result after each operation
  public $process = TRUE;
  public $result = TRUE;
  public $discard = FALSE;
  // Test message, not really for sending
  public $test = FALSE;
  public $success = TRUE;
  // The message has been updated after being saved, needs to be saved again
  public $updated = FALSE;
  /**
   * Constructor, with predefined array of data
   */ 
  function __construct($data = array()) {
    parent::__construct($data);
  }
  /**
   * Load from db
   */
  public static function load($mqid) {
    return messaging_store()->message_load($mqid);
  }
  /**
   * Build from object or array
   */
  public static function build($template) {
    return new Messaging_Message($template);
  }
  /**
   * Actual message sending
   */
  function send($destination = NULL) {
    messaging_debug('Message send', array('message' => $this));
    if ($destination) {
      $this->destination = $destination->address;
      $this->destinations = array($destination->address);
    }
    // The message can be for queueing or just for testing
    if ($this->discard) {
      return FALSE;
    }
    elseif ($this->test) {
      return messaging_message_test($this);
    }
    elseif ($this->queue) {
      return $this->queue();
    }
    elseif (!$this->processed) {
      // This still needs full processing before being sent
      $this->queue = 0;
      return $this->process('prepare', 'presend', 'send', 'aftersend');
    }
    else {
      $this->success = $this->send_method()->send_address($this->destination, $this);
      return $this->success;
    }
  }

  /**
   * Get send method class
   */
  function send_method() {
    return messaging_send_method($this->method);
  }
  /**
   * Send to multiple destinations, same method
   */
  function send_multiple($method = NULL, $addresses = NULL) {
    messaging_debug('Message send multiple', array('message' => $this));
    if ($method && $addresses) {
      $this->method = $method;
      $this->destination = 'multiple';
      $this->destinations = $addresses;
    }
      // The message can be for queueing or just for testing
    if ($this->test) {
      return messaging_message_test($this);
    }
    elseif ($this->queue) {
      return $this->queue();
    }
    elseif (!$this->processed) {
      // This still needs full processing before being sent
      return $this->process('prepare', 'presend', 'multisend', 'aftersend');
    }
    else {
      $this->invoke_callbacks('multisend');
      return $this->success;
    }
  }
  /**
   * Process message to queue, send or discard
   * 
   * This will go through all the operations in order. Any of the callbacks can:
   * - Stop processing: set 'process' property to FALSE
   * - Discard the message: set 'discard' property to TRUE
   * - Change the operations, that are in the 'operations' array
   * 
   * @param
   *   Multiple operations in order
   */
  function process() {
    $this->operations = func_get_args();
    $this->process = TRUE;
    $this->discard = FALSE;
    $this->processed = FALSE;
    while ($this->process && !$this->discard && ($op = array_shift($this->operations))) {
      messaging_debug('Message processing', array('op' => $op));
      switch ($op) {
        // Message methods. These can change the $operations array
        case 'prepare':
        case 'render':
          $this->$op();
          break;

        // Invoke callbacks. These can change the $operations array
        case 'presend':
        case 'aftersend':
          $this->invoke_callbacks($op);
          break;

        // Decission on whether this is for sending, queueing, etc...
        case 'queue':
          $this->processed = 'queue'; 
          $this->queue();
          break;
        case 'send':
          $this->processed = 'send';
          $this->send();
          break;
        case 'multisend':
          $this->processed = 'multisend';
          $this->send_multiple();
          break;
      }
    }
    // Processing has stopped because any of the callbacks decided so.
    if (!$this->processed) {
      $this->success = FALSE;
      $this->set_error("Process aborted, operation: $op");
    }
    // End message processing store or dispose
    $this->done();
    return $this->success;
  }
  /**
   * Queue message
   */
  public function queue() {
    if (!$this->processed) {
      $this->queue = 1;
      return $this->process('prepare', 'queue');
    }
    // Check queue property because it may have been changed during processing
    if (!$this->queued && $this->queue) {
      $this->invoke_callbacks('queue', 'afterqueue');
      $this->queued = TRUE;
      return $this->success;
    }
  }
  /**
   * Discard message
   */
  function discard($reason = '') {
    messaging_log('Message discarded during process', array('reason' => $reason, 'message' => (string)$this));
    $this->delete();
  }
  /**
   * After the message has been processed
   */
  function done($text = '') {
    // Depending on parameters and what's happened so far we make the final queue/log decision
    if ($this->discard) {
      $this->discard($text);
    } if ($this->error || $this->log) {
      $this->log();
    }
  }
  /**
   * Get method info
   */
  function method_info() {
    if (!isset($this->method_info)) {
      $this->method_info = messaging_method_info($this->method);
    }
    return $this->method_info;
  }
  /**
   * Save to store if not saved yet or updated
   */
  function store() {
    if (empty($this->mqid) || $this->updated) {
      $this->save();
    }
  }
  /**
   * Prepare for sending
   */
  function prepare() {
    if (empty($this->prepared)) {
      $this->send_method()->message_prepare($this);
      $this->prepared = TRUE;
      // Provides a hook for other modules to modify the message before sending
      $info = $this->send_method()->get_info();
      drupal_alter('message', $this, $info);
    }
  }
  /**
   * Render through sending method
   */
  function render() {
    if (empty($this->rendered)) {
      $this->process = TRUE;
      $this->send_method()->message_render($this);
      $this->rendered = TRUE;
    }
  }
  /**
   * Save to store / update
   */
  function save() {
    // Make sure this is rendered before saving
    $this->prepare();
    $this->render();
    $result = messaging_store()->message_save($this);
    if ($result) {
      $this->updated = FALSE;
      $this->queued = $this->queue;
      $this->logged = $this->log;
    }
    else {
      $this->set_error(t('The message cannot be saved.'));
    }
  }
  /**
   * Create log for message
   */
  function log() {
    if (!$this->logged) {
      messaging_store()->message_log($this);
      $this->logged = TRUE;
    }
  }
  /**
   * Run callbacks on message
   */
  function invoke_callbacks() {
    $callback_keys = func_get_args();
    $this->process = TRUE;
    $this->success = TRUE;
    while (!empty($this->process) && ($callback = array_shift($callback_keys))) {
      // Invoke callback message_callback on send method object
      $function = 'message_' . $callback;
      $this->send_method()->$function($this, $this->method_info());
    }
    return $this->success;
  }
  /**
   * Set sender account and related properties
   * 
   * @param $account
   *   Sender user account
   * @param $address
   *   Opational method's address for this account
   */
  function set_sender($account, $address = NULL) {
    $this->sender = $account->uid;
    $this->sender_name = $account->name;
    $this->sender_account = $account;
    if (isset($address)) {
      $this->sender_address = $address;
    }
  }
  /**
   * Set destination object
   */
  function set_destination($destination) {
    if (empty($destination)) {
      $this->destination = '';
      $this->destination_object = FALSE;
    }
    elseif (is_object($destination)) {
      $this->uid = $destination->uid;
      $this->destination = $destination->address;
      $this->destination_object = $destination;
    }
  }
  /**
   * Set user object and related properties
   * 
   * If not already set, method and destination will be searched for user account
   */
  function set_user($account) {
    parent::set_user($account);
    $this->check_user($account);
    return !$this->discard && !empty($this->method) && !empty($this->destination);
  }

  /**
   * Check the message method is ok for this user
   */
  function check_user($account) {
    if (empty($this->method)) {
      $this->method = messaging_method_default($account);
    }
    // If we have a method, check user and availability if there's a callback for that. 
    // Depending on the sending method, the message can be queued / discarded / redirected  
    if (!$this->method) {
      $this->set_error('Cannot find a suitable send method for this user.');
    }
    else {
      $methods = array();
      // This can go through multiple redirections, we stop it when we get to the same method again or process = FALSE
      while ($this->process && !in_array($this->method, $methods) && !$this->queue && !$this->discard) {
        $methods[] = $this->method;
        $this->send_method()->message_user($this, $account);
      }
      // The sending method may have decided the message is to be discarded or queued.
      if (!$this->discard && empty($this->destination)) {
        if ($destination = $this->send_method()->get_user_address($account)) {
          $this->destination = $destination;
        }
        else {
          $this->set_error('Cannot find a suitable address for this user.');
        }
      }
    }
    return !$this->discard;
  }
  
  /**
   * Add destination to the list
   * 
   * This will also de-dupe destinations
   */
  function add_destination($destination) {
    $this->destinations[$destination->index()] = $destination;
  }

  /**
   * Get sending method parameters
   */
  function get_params($method = NULL) {
    $method = $method ? $method : $this->method;
    // First get specific parameters for this sending method
    $params = isset($this->params[$method]) ? $this->params[$method] : array();
    // Check for specific parameters for this method group
    $group = $this->send_method()->get_info('group');
    if ($group && !empty($this->params[$group])) {
      $params += $this->params[$group];
    }
    return $params;
  }

  /**
   * Delete if already on store
   */
  function delete() {
    if ($this->is_instance()) {
      $result = messaging_store()->message_delete($this->mqid);
      $this->deleted = TRUE;
      return $result;
    }
  }

  /**
   * Set error condition and stop processing
   * 
   * @param $text
   *   Error message to be stored
   */
  function set_error($text, $code = 1) {
    // This will stop processing if we are in the middle of anything
    $this->process = FALSE;
    $this->result = FALSE;
    $this->error = $code;
    $this->error_msg = $text;
    
    if ($this->log_error) {
      $this->log = TRUE;
      $this->updated = TRUE;
    }
    elseif (!$this->log) {
      // Not for logging, discard
      $this->discard = TRUE;
    }
  } 

  /**
   * Get list of fields to serialize for storage
   */
  function data_fields() {
    return array('files', 'parts', 'sender_name', 'error_msg');
  }
  // Magic function, format as string
  public function __toString() {
    $subject = $this->subject ? check_plain($this->subject) : '<none>';
    return "Message: method=$this->method, destination=$this->destination, subject=$subject";
  }
  /**
   * Magic method, set protected properties
   * 
   * Warning: This also runs for unset properties
   */
  public function __set($name, $value) {
    switch ($name) {
      case 'sender_account':
        $this->sender_account = $value;
        if (!isset($this->sender)) {
          $this->sender = $value ? $value->uid : 0;
        }
        break;
      case 'destination':
        if (is_object($value)) {
          $this->set_destination($value);
        }
        else {
          $this->destination = $value;
        }
        break;
      case 'user':
      case 'account':
        $this->set_user($value);
        break;
      default:
        $this->$name = $value;
        break;
    }
  }
  /**
   * Magic method, get protected properties
   */
  public function __get($name) {
    switch ($name) {
      case 'sender_account':
        return $this->get_sender();
      case 'user':
      case 'account':
        return $this->get_user();
    }
  }
  /**
   * Magic method, isset protected properties
   */
  public function __isset($name) {
    switch ($name) {
      case 'destination':
      case 'user';
        return !empty($this->$name);
    }
  }
  /**
   * Get sender account
   */
  function get_sender() {
    if (!isset($this->sender_account)) {
      $sender = !empty($this->sender) ? messaging_load_user($this->sender) : NULL;
      $this->sender_account = $sender ? $sender : FALSE;
    }
    return $this->sender_account;
  }
  /**
   * Get sender name
   */
  function get_sender_name() {
    if (!empty($this->sender_name)) {
      return $this->sender_name;
    }
    elseif ($sender = $this->get_sender()) {
      return check_plain($sender->name);
    }
    else {
      return variable_get('site_name', 'Drupal');
    }
  }
  /**
   * Get rendered subject
   */
  function get_subject() {
    $this->render();
    return $this->subject;
  }
  /**
   * Get rendered body
   */
  function get_body() {
    $this->render();
    return $this->body;
  }
  /**
   * Get message files
   */
  function get_files() {
    return !empty($this->files) ? $this->files : array();
  }
  /**
   * Build a simple text with message subject and body
   * 
   * @param $glue
   *   Separator to glue subject and body together
   */
  function get_text($glue = ' ') {
    $parts = array(trim($this->get_subject()), trim($this->get_body()));
    $parts = array_filter($parts);
    $text = $parts ? implode($glue, $parts) : '';
    return $text;
  }
  // Get table name for storage
  public static function db_table() {
    return self::DB_TABLE;
  }
  // Get key field name for storage
  public static function db_key() {
    return self::DB_KEY;
  }
}

