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
  function __construct($object = NULL) {
    if ($object) {
      foreach ($object as $field => $value) {
        $this->$field = $value;
      }
    }
  }
  /**
   * Load single destination
   */
  public static function load($mdid) {
    $destinations = &messaging_static('messaging_destination');
    if (!$destinations || !isset($destinations[$mdid])) {
      $object = db_fetch_object(db_query("SELECT * FROM {messaging_destination} WHERE mdid = %d", $mdid));
      $destinations[$mdid] = $object ? new Messaging_Destination($object) : FALSE;
    }
    return $destinations[$mdid];
  }

  /**
   * Get from db using conditions
   */
  public static function get($params) {
    if ($dest = db_fetch_object(self::db_query("SELECT *", $params))) {
      $dest = new Messaging_Destination($dest);
      $destinations = &messaging_static('messaging_destination');
      $destinations[$dest->mdid] = $dest;
      return $dest;
    }  
  }

  /**
   * Build destination with partial parameters
   * 
   * If the destination(method, address) exists for a different user, it will return FALSE
   * @return
   *   Destination object if possible, 
   */
  public static function build($method, $address, $uid = 0) {
    $params = array('method' => $method, 'address' => $address);
    if ($destination = self::get($params)) { 
      if ($destination->uid != $uid) {
        // There's already a destination with these parameters but different user
        if ($uid && !$destination->uid) {
          // This was an anonymous destination, now it must be assigned to the user
          $destination->uid = $uid;
          $destination->save();
          return $destination;
        }
        else {
          // Not valid: it was a destination for another user
          return FALSE;
        }
      }
      else {
        return $destination;
      }
    }
    // Transitional case, row for user with no address, update it
    if ($uid && ($destination = self::get(array('uid' => $uid, 'method' => $method, 'address' => '')))) {
      $params['mdid'] = $destination->mdid;
    }
    // Create a new destination object
    $destination = new Messaging_Destination((object)$params);
    $destination->save();
    return $destination;
  }

  /**
   * Get unique index for this destination
   */
  function index() {
    return $this->uid . ':' . $this->method . ':' . $this->address;
  }

  /**
   * Save to store: Create or update
   */
  function save() {
    $update = empty($this->mdid) ? array() : 'mdid';
    return drupal_write_record('messaging_destination', $this, $update);
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
