<?php
// $Id$
/**
 * @file
 * Drupal Messaging Framework - Message class file
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
  public $mqid;
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
  public $created = 0;
  public $sent = 0;
  // Processing parameters
  public $queue = 0;
  public $send = 0;
  public $cron = 0; 
  public $log;
  // Error code, error text
  public $error = 0;
  public $error_msg;
  // Serialized data
  public $data;
  // Temporary processing variables 
  // These are not stored
  public $operations;
  public $discard;
  // Message status
  public $prepared = FALSE;
  public $rendered = FALSE;
  public $processed = FALSE;
  public $queued = FALSE;
  public $redirect = FALSE;
  public $retry = FALSE;
  // Result after each operation
  public $process;
  public $result;
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
    // Set logging option
    if (!isset($this->log)) {
      $this->log = variable_get('messaging_log', 0);
    }
    if (!isset($this->created)) {
      $this->created = time();
    }
  }
  /**
   * Load from db
   */
  public static function load($mqid) {
    return messaging_store('load', $mqid);
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
      $this->method = $destination->method;
      $this->destination = $destination->address;
      $this->destinations = array($destination->address);
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
      $this->queue = 0;
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
      $this->cron = 1;
      return $this->process('prepare', 'queue');
    }
    if (!$this->queued && $this->queue) {
      $this->success = $this->queued = TRUE;
      $this->invoke_callbacks('queue', 'afterqueue');
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
      $this->store();
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
   * Save to store if not saved yet
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
    messaging_store('save', $this);
    $this->updated = FALSE;
  }
  /**
   * Run callbacks on message
   */
  function invoke_callbacks() {
    $callback_keys = func_get_args();
    $this->process = TRUE;
    $this->success = TRUE;
    while (!empty($this->process) && ($method = array_shift($callback_keys))) {
      // Invoke callback message_callback on send method object
      call_user_func(array($this->send_method(), 'message_' . $method), $this, $this->method_info());
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
      $this->method = $destination->method;
    }
  }

  /**
   * Add destination to the list
   * 
   * This will also de-dupe destinations
   */
  function add_destination($destination) {
    $message->destinations[$destination->index()] = $destination;
  }

  /**
   * Get sending method parameters
   */
  function get_params($method = NULL) {
    $method = $method ? $method : $this->method;
    // First get specific parameters for this sending method
    $params = isset($this->params[$method]) ? $this->params[$method] : array();
    // Check for specific parameters for this method group
    $group = messaging_method_info($method, 'group');
    if ($group && !empty($this->params[$group])) {
      $params += $this->params[$group];
    }
    return $params;
  }
  /**
   * Get language as object
   */
  function get_language() {
    if (isset($this->language) && ($languages = language_list()) && isset($languages[$this->language])) {
      return $languages[$this->language];
    }
    else {
      return language_default();
    }
  }
  /**
   * Delete if already on store
   */
  function delete() {
    if (!empty($this->mqid)) {
      $result = messaging_store('delete', $this->mqid);
      unset($this->mqid);
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
    $this->updated = TRUE;
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
      default:
        return isset($this->$name);
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
      return $sender->name;
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
  // Get table name for storage
  public static function db_table() {
    return self::DB_TABLE;
  }
  // Get key field name for storage
  public static function db_key() {
    return self::DB_KEY;
  }
}

