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
  // Value for destination when there are more destinations
  const MULTIPLE_DESTINATION = 'multiple';
  // Unique id
  public $mqid = 0;
  // Message type: outgoing, incoming
  public $type;
  // Sending method
  public $method;
  // Send method object
  protected $send_method;
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
  public $built = FALSE;
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
   * Build object from template
   */
  public static function build($template) {
    return self::object_build($template, 'Messaging_Message');
  }

  /**
   * Load from db
   */
  public static function load($mqid) {
    return messaging_store()->message_load($mqid);
  }

  /**
   * Check parameters and go for alter hook
   */
  public function message_build() {
    if (empty($this->built)) {
      $this->set_destination();
      // If message priority > 0 this is not to be queued
      if ($this->priority > 0) {
        $this->queue = 0;
      }
      // Provides a hook for other modules to modify the message before sending
      // This is the old one, lacks namespace. OBSOLETED
      $info = $this->send_method() ? $this->send_method()->get_info() : array();
      drupal_alter('message', $this, $info);
      drupal_alter('messaging_message', $this);
      $this->built = TRUE;
    }
    return $this->built && !$this->discard;
  }

  /**
   * Prepare for sending through given method
   * 
   * At this stage, message can be redirected to a different method
   */
  public function prepare() {
    if (empty($this->prepared)) {
      if ($this->message_build()) {
        $this->set_destination();
        // Prepare invoking one or more sending methods
        $this->prepared = $this->process_method('message_prepare');
        messaging_debug("Prepared for send method.");
      }
      else {
        // Message building failed, we don't do anything else
        $this->prepared = FALSE;
        $this->message_log("Failed message build.");
      }
    }
    return $this->prepared;
  }

  /**
   * Render through sending method
   */
  public function render() {
    $this->prepare();
    if (empty($this->rendered)) {
      $this->send_method()->message_render($this);
      $this->rendered = time();
    }
  }

  /**
   * Send message through sending method.
   * 
   * The message may need to be processed and it can be queued or discarded yet.
   */
  public function send($destination = NULL) {
    if (isset($destination)) {
      $this->set_destination($destination);
    }
    if (!$this->processed) {
      return $this->process();
    }
    else {
      // The message has been processed and it is for sending
      $this->message_log('Sending message.');
      return $this->send_method()->message_send($this);
    }
  }

  /**
   * Send to multiple destinations, same method.
   * 
   * The message may need to be processed and it can be queued or discarded yet.
   */
  function send_multiple($addresses = NULL, $method = NULL) { 
    $this->destination = self::MULTIPLE_DESTINATION;
    if (isset($method)) {
      $this->method = $method;
    }
    if ($addresses) {
      $this->destinations = $addresses;
    }
    if (!$this->processed) {
      return $this->process();
    }
    else {
      $this->message_log('Sending to multiple destinations.', array('destinations' => $this->destinations));
      $this->success = $this->send_method()->message_multisend($this);
      return $this->success;
    }
  }

  /**
   * Queue message using the messaging store.
   * 
   * This should happen only once per message, we check it hasn't been queued before.
   */
  public function queue() {
    $this->queue = 1;
    if (!$this->processed) {
      return $this->process();
    }
    elseif (!$this->queued) {
      $this->queued = $this->send_method()->message_queue($this);
      $this->send_method()->message_afterqueue($this);
    }
    return $this->queued;
  }
  
  /**
   * Get send method object
   */
  public function send_method() {
    // Create send method object the first time or when 'method' changed
    if (!isset($this->send_method) || $this->method != $this->send_method->method) {
      $this->send_method = !empty($this->method) ? messaging_send_method($this->method) : NULL;
    }
    return $this->send_method;
  }


  /**
   * Check whether the message is to be sent / queued
   * 
   * @return boolean
   *   True if it needs further processing
   */
  protected function process() {
    // Preprocess: Build and prepare if not done before
    if (!$this->processed) {
      if (!$this->discard && !$this->prepared) {
        $this->prepare();
      }
    }
    // If everything is ok after preprocessing go for actual send/queue
    if ($this->prepared && !$this->discard && !$this->error) {
      // Mark as processed, so it is actually queued/sent
      $this->processed = TRUE;
      if ($this->test) {
        messaging_message_test($this);
        $this->success = TRUE;
      } 
      elseif ($this->queue) {
        $this->success = $this->queue();
        $this->module_invoke('message queued');
      }
      else {
        // Send to one or more destinations
        if ($this->destination == self::MULTIPLE_DESTINATION) {
          $this->success = $this->send_multiple();
        }
        else {
          $this->success = $this->send();
        }
        if ($this->success) {
          $message->sent = time();
        }
        $this->send_method()->message_aftersend($this);
        $this->module_invoke('message sent');
      }
      // Message done, maybe log and return
      $this->done();
      return $this->success;    
    }
    else {
      $this->discard('Failed processing.');
      messaging_debug('Discarded message during processing.', array('message' => $this));
      return FALSE;
    }
  }
  /**
   * Process through sending method
   * 
   * This can go through multiple redirections, we stop it when we get to the same method again or process = FALSE
   */
  protected function process_method($operation) {
    if ($this->send_method()) {
      $this->process = TRUE;
      $methods = array();
      // The send method can stop this by setting 'process' to FALSE or 'discard' to TRUE
      while ($this->process && !$this->discard && !in_array($this->method, $methods)) {
        $methods[] = $this->method;
        $this->send_method()->$operation($this);
      }
      return TRUE;
    } else {
      return FALSE;
    }
  }
  /**
   * Invoke hook_messaging_message() on all modules
   */
  protected function module_invoke($op) {
    return module_invoke_all('messaging_message', $op, $this);
  }
  /**
   * Discard message
   */
  function discard($reason = 'No reason.') {
    $this->success = FALSE;
    $this->message_log('Message discarded during process', array('reason' => $reason));
    $this->delete();
  }

  /**
   * After the message has been processed
   */
  function done() {
    // Depending on parameters and what's happened so far we make the final queue/log decision
    if ($this->discard) {
      $this->discard('Done processing');
    } if ($this->error || $this->log) {
      $this->log();
    }
  }

  /**
   * Get method info
   */
  function method_info() {
    return messaging_method_info($this->method);
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
   * Save to store / update
   */
  function save() {
    $result = messaging_store()->message_save($this);
    if ($result) {
      $this->updated = FALSE;
    }
    else {
      $this->set_error('The message cannot be saved.');
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
   * Set destination object or figure out a destination if not set
   * 
   * @param $destination
   *   Destination object, single address, or array of addresses
   */
  function set_destination($destination = NULL) {
    if (isset($destination)) {
      if (is_object($destination)) {
        $this->uid = $destination->uid;
        $this->destination = $destination->address;
        $this->destination_object = $destination;
      }
      elseif (is_string($destination) || is_empty($destination)) {
        $this->set_multiple($destination);
        $this->destination = $destination;
        $this->destination_object = FALSE;
      }
    }
    else {
      if (empty($this->method)) {
        // If we don't have a method we can get it from the user account
        $this->method = messaging_method_default($this->get_user());
      }
      // Same for address, but only for registered users
      if ($this->send_method() && empty($this->destination) && $this->uid) {
        $this->destination = $this->send_method()->get_user_address($this->get_user());
      }
    }
  }

  /**
   * Set multiple sestinations
   * 
   * @param $destinations
   *   Array of addresses
   */
  function set_multiple($destinations) {
    $this->destination = self::MULTIPLE_DESTINATION;
    $this->destinations = $destinations;
  }
  
  /**
   * Set user object and related properties
   * 
   * If not already set, method and destination will be searched for user account
   */
  function set_user($account) {
    parent::set_user($account);
    $this->set_destination();
    // The send method can have some specific bits for user
    $this->process_method('message_user');
    // Check we have method and address
    if (empty($this->method)) {
      $this->set_error('Cannot find a suitable send method for this user.');
      return FALSE;
    }
    elseif (empty($this->destination)) {
      $this->set_error('Cannot find a suitable address for this user.');
      return FALSE;
    }
    else {
      return !$this->discard;
    }
  }

  /**
   * Check we have a sending method and a destination
   * 
   * @param $error
   *   Whether to mark this message as error if destination not ok
   */
  function check_destination($error = FALSE) {
    $this->set_destination();
    return !empty($this->method) && !empty($this->destination);
  }
  
  /**
   * Add destination to the list
   * 
   * 
   */
  function add_destination($destination) {
    $this->destination = self::MULTIPLE_DESTINATION;
    // This will also de-dupe destinations
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
      $result = messaging_store()->message_delete($this);
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
  function set_error($text = 'ERROR', $code = 1) {
    // This will stop processing if we are in the middle of anything
    $this->process = FALSE;
    $this->success = FALSE;
    $this->error = $code;
    $this->error_msg = $text;
    $this->message_log('Error processing message.');
    // By default, messages are set to be logged when errors happen
    if ($this->log_error) {
      $this->log = 1;
      $this->updated = TRUE;
    }
    elseif (!$this->log) {
      // Not for logging, discard
      $this->discard = TRUE;
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

  /**
   * Log message with some data
   */
  protected function message_log($text, $variables = array()) {
    messaging_log($text, $variables + array('message' =>(string)$this));
  }
  
  /**
   * Get list of fields to serialize for storage
   */
  public function data_fields() {
    return array('destinations', 'files', 'parts', 'sender_name', 'error_msg');
  }

  // Magic function, format as string
  public function __toString() {
    $subject = !empty($this->subject) ? check_plain($this->subject) : '<none>';
    $method = !empty($this->method) ? $this->method : '<none>';
    $destination = !empty($this->destination) ? $this->destination : '<none>';
    $text = "Message: method= $method, destination= $destination, subject= $subject";
    if (!empty($this->error)) {
      $text .= " error=$this->error, $this->error_msg";
    }
    return $text;
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
        if (is_array($value)) {
          $this->set_multiple($value);
        }
        else {
          $this->set_destination($value);
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
  // Get table name for storage
  public static function db_table() {
    return self::DB_TABLE;
  }
  // Get key field name for storage
  public static function db_key() {
    return self::DB_KEY;
  }
}

