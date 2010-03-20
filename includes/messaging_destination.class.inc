<?php
// $Id$
/**
 * @file
 * Drupal Messaging Framework - Default class file
 */

/**
 * Message destination class
 */
class Messaging_Destination {
  // Destination status
  const STATUS_PENDING = 0;
  const STATUS_ACTIVE = 1;

  // Object unique id
  public $mdid;
  // User id
  public $uid;
  // Destination type
  public $type;
  // Sending method
  public $method;
  // Address for this sending method
  public $address;
  // Date sent
  public $sent = 0;
  // Status
  public $status = 0;
  // Processing values
  protected $account;
  
  /**
   * Constructor
   */
  function __construct($method = NULL, $object = NULL) {
    $this->method = $method;
    if ($object) {
      foreach ($object as $field => $value) {
        $this->$field = $value;
      }
    }
    // Set destination type if missing
    if (empty($this->type) && !empty($this->method)) {
      $this->type = messaging_method_info($this->method, 'group');
    }
  }
  /**
   * Load single destination
   */
  public static function load($mdid) {
    $destinations = &messaging_static('messaging_destination');
    if (!$destinations || !isset($destinations[$mdid])) {
      $object = db_fetch_object(db_query("SELECT * FROM {messaging_destination} WHERE mdid = %d", $mdid));
      $destinations[$mdid] = $object ? self::build($object) : FALSE;
    }
    return $destinations[$mdid];
  }

  /**
   * Get from db using conditions
   */
  public static function get($params) {
    if ($dest = db_fetch_object(self::db_query("SELECT *", $params))) {
      $dest = self::build($dest);
      $destinations = &messaging_static('messaging_destination');
      $destinations[$dest->mdid] = $dest;
      $destinations[$dest->method][$dest->address] = $dest;
      return $dest;
    }
    else {
      return FALSE;
    }
  }

  /**
   * Build destination with partial parameters
   * 
   * @param $template
   *   Object template or messaging method
   */
  public static function build($template) {
    if (is_object($template)) {
      $object = $template;
      $method = $tepmplate->method;
    }
    else {
      $method = $template;
      $object = NULL;
    }
    // The type may define its own class
    if ($class = messaging_method_info($method, 'destination_class')) {
      return new $class($method, $object);
    }
    else {
      return new Messaging_Destination($method, $object);
    }    
  }
  /**
   * Validate values to create a destination
   */
  public static function validate($method, $address, $account = 0) {
    $account = is_object($account) ? $account : messaging_load_user($account);
    $params = array('method' => $method, 'address' => $address);
    // First check permission
    if (messaging_method_permission($method, $account)) {
      if ($existing = self::get_by_address($method, $address)) {
        // There's already a destination with these parameters, check user
        // It will be ok if users match or it was anonymous before
        return !$existing->uid || $existing->uid == $account->uid;
      }
      else {
        // @todo Check if anonymous destinations match an existing user
        return TRUE;
      }
    }
  }
  /**
   * Create with parameters
   */
  public static function create($method, $address, $uid) {
    if ($existing = self::get_by_address($method, $address)) {
      if ($existing->uid != $uid) {
        $existing->uid = $uid;
        $existing->save();
      }
      return $existing;
    }
    // Transitional case, row for user with no address, update it
    elseif ($uid && ($existing = self::get(array('uid' => $uid, 'method' => $method, 'address' => '')))) {
      $existing->address = $address;
      $existing->save();
      return $existing;
    }
    else {
      $destination = new Stdclass();
      $destination->method = $method;
      $destination->address = $address;
      $destination = self::build($destination);
      $destination->save();
      return $destination;
    }
  }
  
  public static function get_by_address($method, $address) {
    $destinations = &messaging_static('messaging_destination');
    if (!isset($destinations[$method][$address])) {
      $destinations[$method][$address] = self::get(array('method' => $method, 'address' => $address));
    }
    return $destinations[$method][$address];
  }
  /**
   * Get unique index for this destination
   */
  function index() {
    return $this->uid . ':' . $this->method . ':' . $this->address;
  }
  /**
   * Get address name
   */
  function address_name() {
    return messaging_method_info($this->method, 'address_name');
  }
  /**
   * Get method name
   */
  function method_name() {
    return messaging_method_info($this->method, 'name');
  }
  /**
   * Get user account
   */
  function get_account() {
    if (!isset($this->account) && isset($this->uid)) {
      $this->account = messaging_load_user($this->uid);
    }
    return isset($this->account) ? $this->account : NULL;
  }
  /**
   * Save to store: Create or update
   */
  function save() {
    $update = empty($this->mdid) ? array() : 'mdid';
    $result = drupal_write_record('messaging_destination', $this, $update);
    if ($result && !$update) {
      $destinations = &messaging_static('messaging_destination');
      $destinations[$this->mdid] = $this;
      $destinations[$this->method][$this->address] = $this;
    }
    return $result;
  }
  /**
   * Delete messaging destination object/s
   */
  public static function delete_multiple($params) {
    return self::db_query("DELETE", $params);
  }
  // Magic function, format as string
  public function __toString() {
    return 'Destination: ' . $this->index();
  }
  /**
   * Db query for destinations table
   */
  protected static function db_query($sql, $params) {
    $query = _messaging_query_conditions('messaging_destination', $params);
    return db_query($sql . ' FROM {messaging_destination} WHERE ' . $query['where'], $query['args']);
  }
}
