<?php
// $Id$
/**
 * @file
 *   Database storage for the messaging framework
 */

// Max number of rows to process for each step before clean up
define('MESSAGING_STEP_ROWS', 1000);

// Minimum amount of seconds the process will need for clean-up tasks
// Just to make sure that after exhausting cron assigned time we'll have a few spare seconds for some cleanup
define('MESSAGING_TIME_MARGIN', 5);

/**
 * Process messages on cron
 */
function messaging_store_cron_process() {
  // Pass on a time out condition, that will be based on 'max_execution_time'
  $limit['timeout'] = variable_get('cron_semaphore', 0) + ini_get('max_execution_time') - MESSAGING_TIME_MARGIN;
  return messaging_store_queue_process($limit);
}
/**
 * Process and send messages in queue, to be called from cron
 * 
 * It will check for predefined limits and repeat the cycle
 *   [fetch] -> [send] -> [check]
 * until the queue is empty or any of the limits are met
 * 
 * The limits array may contain any of these conditions:
 * - time, absolute max execution time
 * - timeout, calculated time out (like for cron, based on the time we've been already running)
 * - message, max number of messages sent
 * - percent, max % of page execution time that can be spent on cron processing
 * 
 * @param $limits
 *   Optional limits for queue processing
 * @return
 *   Array of results indexed by message id
 */
function messaging_store_queue_process($limit = array()) {
  $results = array();
  $limit += variable_get('messaging_process_limit', array('message' => 0, 'time' => 0, 'percent' => 0));
  // Calculate time limit. We get the smaller of all these times in seconds
  if (!empty($limit['timeout'])) {
    $times[] = $limit['timeout'];
  }
  if (!empty($limit['time'])) {
    $times[] = time() + $limit['time'];
  }
  if (!empty($limit['percent'])) {
    $times[] = time() + ini_get('max_execution_time') * $limit['percent'] / 100;
  }
  $timelimit = !empty($times) ? min($times) : 0;
  
  // Processing loop. Will stop when we run out of rows or reach time / messages limit
  $count = 0;
  $max = !empty($limit['message']) ? $limit['message'] : 0;
  do {
    $step = $max ? min(MESSAGING_STEP_ROWS, $max - $count) : MESSAGING_STEP_ROWS;
    $result = messaging_store_queue_process_step($step, $timelimit);
    $number = count($result);
    $count += $number;
    $results = array_merge($results, $result);
  } 
  while ($number == $step && (!$timelimit || time() <= $timelimit) && (!$max || $max > $count));

  return $results;
}

/**
 * Retrieve and send queued messages
 * 
 * @param $limit
 *   Maximum number of queued messages to process for this step
 * @param $timeout
 *   Optional time limit for processing, will return when if reached during processing
 * @return
 *   Array of sending results indexed by message id
 */
function messaging_store_queue_process_step($limit, $timeout = 0) {
  $count = 0;
  $sent = $unsent = $processed = array();
  $result = db_query_range("SELECT * FROM {messaging_store} WHERE queue = 1 AND cron = 1 ORDER BY mqid", 0, $limit);
  while ($object = db_fetch_object($result)) {
    $message = messaging_store_unpack($object, TRUE);
    // Do not queue again but send out
    $message->queue = 0;
    if (!empty($message->destinations)) {
      $success = $message->send_multiple();
    }
    else {
      $success = $message->send();
    }
    $processed[$message->mqid] = $success;
    if ($success) {
      $sent[] = $message->mqid;
    } else {
      $unsent[] = $message->mqid;
    }
    $count++;
    // Check timeout after each message
    if ($timeout && time() > $timeout) break;
  }
  if ($sent) {
    messaging_store_sent($sent);
  }
  if ($unsent) {
    messaging_store_sent($unsent, TRUE);
  }
  return $processed;
}

/**
 * Queue clean up
 * - Remove expired logs (include errors)
 * - Remove expired queued messages
 * 
 * @param $force
 *   Whether to force removal of logs and errors (even if logging not set)
 */
function messaging_store_queue_cleanup($force = FALSE) {
  $count = 0;
  if (($expire_logs = variable_get('messaging_log', 0)) || $force) {
    $time = time() - $expire_logs;  
    db_query('DELETE FROM {messaging_store} WHERE log = 1 AND queue = 0 AND sent < %d OR error = 1 AND created < %d', $time, $time);
    $count += db_affected_rows();
  }
  if ($expire_messages = variable_get('messaging_queue_expire', 0)) {
    $time = time() - $expire_messages;
    db_query('DELETE FROM {messaging_store} WHERE created < %d', $time);
    $count += db_affected_rows();
  }
  return $count;
}

/**
 * Retrieve from messaging database storage
 * 
 * @param $params
 *   Array of field value pairs
 * @param $order
 *   Optional array of field names to order by
 * @param $limit
 *   Optional maximum number of rows to retrieve
 * @param $pager
 *   Optional pager element for pager queries
 * @param $unpack
 *   Optional fully load stored data
 */
function messaging_store_get($params, $order = NULL, $limit = NULL, $pager = NULL, $unpack = FALSE) {
  $messages = $where = $args = array();
  list ($where, $args) = messaging_store_query($params);
  $sql = 'SELECT * FROM {messaging_store}';
  $sql .= $where ? ' WHERE '.implode(' AND ', $where) : '';
  $sql .= $order ? ' ORDER BY '.implode(', ', $order) : '';

  if (!is_null($pager)) {
    $result = pager_query($sql, $limit, $pager, NULL, $args);
  } elseif ($limit) {
    $result = db_query_range($sql, $args, 0, $limit);
  } else {
    $result = db_query($sql, $args);
  }
  while ($msg = db_fetch_object($result)) {
    $messages[$msg->mqid] = messaging_store_unpack($msg, $unpack);
  }
  return $messages;
}

/**
 * Load single message from store
 */
function messaging_store_load($mqid) {
  if ($message = db_fetch_object(db_query('SELECT * FROM {messaging_store} WHERE mqid = %d', $mqid))) {
    return messaging_store_unpack($message, TRUE);
  } 
}

/**
 * Build query with field conditions
 * 
 * This function supports IN() conditions when passing array field values
 * @param $query
 *   Array of field => value pars
 */
function messaging_store_query($fields) {
  $where = $args = array();
  foreach ($fields as $key => $value) {     
    if (is_array($value)) {
      // Special processing for array parameters. Many ints are expected for 'mqid' field
      $type = ($key == 'mqid') ? 'int' : 'varchar';
      $where[] = $key . ' IN(' . db_placeholders($value, $type) . ')';
      $args = array_merge($args, $value);
    } else {
      $where[] = $key . " = '%s'";
      $args[] = $value;
    }
  }
  return array($where, $args);
}

/**
 * Unpack stored messages
 * 
 * @param $message
 *   Array as retrieved from the db store
 * @param $full
 *   True for loading the account data if this message is intended for a user
 *   And loading the file objects associated too
 */
function messaging_store_unpack($message, $full = FALSE) {
  // Unserialize stored parameters
  if ($message->params) {
    $message->params = unserialize($message->params);
  }
  // Unserialize data field
  drupal_unpack($message);
  // Saved messages are prepared and rendered
  $message->prepared = $message->rendered = TRUE;
  return new Messaging_Message($message);
}

/**
 * Mark messages as sent, either deleting them, or keeping logs
 * 
 * @param $mqid
 *   Single message id or array of message ids
 * @param $error
 *   Optional, just mark as error move queue messages to log, for messages on which sending failed 
 */
function messaging_store_sent($mqid, $error = FALSE) {
  $mqid = is_array($mqid) ? $mqid : array($mqid);
  list($where, $args) = messaging_store_query(array('mqid' => $mqid));
  if ($error) {
    // Error, log them all, sent = 0
    $sent = 0;
  } else {
    // First delete the ones that are not for logging, then mark as sent
    db_query("DELETE FROM {messaging_store} WHERE log = 0 AND ".implode(' AND ', $where) , $args);
    $sent = time();
  }

  // Now unmark the rest for queue processing, as logs
  $args = array_merge(array($sent), $args);
  db_query("UPDATE {messaging_store} SET queue = 0, cron = 0, log = 1, sent = %d WHERE ".implode(' AND ', $where), $args);
}

/**
 * Delete multiple messages from queue
 */
function messaging_store_del($params) {
  list($where, $args) = messaging_store_query($params);
  db_query("DELETE FROM {messaging_store} WHERE ".implode(' AND ', $where), $args);
}

/**
 * Delete single message from store
 */
function messaging_store_delete($message) {
  $mqid = is_object($message) ? $message->mqid : $message;
  db_query("DELETE FROM {messaging_store} WHERE mqid = %d", $mqid);
}

/**
 * Put into database storage, create one line for each destination
 * 
 * If there are many destinations they will be stored as 'multiple'
 * 
 * @param $message
 *   Message object
 */
function messaging_store_save($message) {
  // Check we have a message object
  $message = messaging_message_build($message);
  messaging_store_prepare($message);
  $update = empty($message->mqid) ? array() : 'mqid';
  drupal_write_record('messaging_store', $message);
  // Return message object for backwards compatibility
  return $message;
}

/**
 * Prepare for storage
 */
function messaging_store_prepare($message) {
  // Normalize some values. Boolean parameters must be 0|1
  foreach (array('queue', 'log', 'cron') as $field) {
    $message->$field = empty($message->$field) ? 0 : 1;
  }
  // Check for multiple destinations, just store 'multiple'
  // Bulk sending modules may store each destination differently
  if (!empty($message->destinations) && count($message->destinations) > 1) {
    $message->destination = 'multiple';      
  }
  // Add data fields to serialize
  foreach ($message->data_fields() as $field) {
    if (isset($message->$field)) {
      $data[$field] = $message->$field;
    }
  }
  $message->data = !empty($data) ? $data : NULL;
}
