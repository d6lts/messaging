<?php
// $Id$
/**
 * @file
 * Drupal Messaging Framework - Default class file
 */

/**
 * Message destination class
 */
class Messaging_Destination extends Messaging_Cached_Object {
  // Database properties
  const DB_TABLE = 'messaging_destination';
  const DB_KEY = 'mdid';

  // Destination status
  const STATUS_PENDING = 0;
  const STATUS_ACTIVE = 1;

  // Object unique id
  public $mdid;
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
  
  /**
   * Constructor
   */
  function __construct($template = NULL) {
    parent::__construct($template);
    // Set destination type if missing
    if (empty($this->type) && !empty($this->method)) {
      $this->type = messaging_method_info($this->method, 'group');
    }
  }

  /**
   * Get from db using conditions
   */
  public static function get($params) {
    if ($object = db_fetch_object(self::db_query("SELECT *", $params))) {
      $dest = self::build($object);
      $dest->cache_save();
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
    if (is_array($template)) {
      $method = NULL;
      $template = (object)$template;
    }
    if (is_object($template)) {
      $object = $template;
      $method = $template->method;
    }
    else {
      $method = $template;
      $object = array('method' => $method);
    }
    // The type may define its own class
    if ($method && ($class = messaging_method_info($method, 'destination_class'))) {
      return new $class($object);
    }
    else {
      return new Messaging_Destination($object);
    }    
  }
  /**
   * Load object from database
   */
  public static function load($key) {
    return self::object_load(self::DB_TABLE, self::DB_KEY, $key, 'Messaging_Destination');
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
      $destination = self::build(array('method' => $method, 'address' => $address, 'uid' => $uid));
      $destination->save();
      return $destination;
    }
  }
  /**
   * Get destination by method and address. This allows advanced caching.
   */
  public static function get_by_address($method, $address) {
    $cached = self::cache_by_address($method, $address);
    if (isset($cached)) {
      return $cached;
    }
    else {
      return self::get(array('method' => $method, 'address' => $address));
    }
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
    $name = messaging_method_info($this->method, 'address_name');
    return $name ? $name : t('Address');
  }
  /**
   * Get method name
   */
  function method_name() {
    return messaging_method_info($this->method, 'name');
  }
  /**
   * Format address
   */
  function format_address($html = FALSE) {
    if ($callback = messaging_method_info($this->method, 'format address callback')) {
      $address = $callback($this->address, $html);
      return $address ? $address : t('Unknown');
    }
    else {
      return check_plain($this->address);
    }
  }
  /**
   * Get user account
   */
  function get_account() {
    return $this->get_user();
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
  // Get table name for storage
  public static function db_table() {
    return self::DB_TABLE;
  }
  // Get key field name for storage
  public static function db_key() {
    return self::DB_KEY;
  }
  /**
   * Save object to cache
   */
  public function cache_save() {
    if ($this->key()) {
      $this->cache_set($this->key(), $this);
      $this->cache_by_address($this->method, $this->address, $this);
    }
  }
  // Store into cache
  public static function cache_set($key, $object) {
    return self::object_cache_set('Notifications_Destination', $key, $object);
  }
  // Get from cache
  public static function cache_get($key) {
    return self::object_cache_get('Notifications_Destination', $key);
  }
  /**
   * Cache get/set by method and address
   */
  public static function cache_by_address($method, $address, $object = NULL) {
    if (isset($object)) {
      return self::cache_set("$method:$address", $object);
    }
    else {
      return self::cache_get("$method:$address");
    }
  }
}
