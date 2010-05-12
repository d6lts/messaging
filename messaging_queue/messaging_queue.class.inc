<?php 
// $Id$
/**
 * @file
 * Messaging Queue class
 */

/**
 * This class overrides Messaging_Store so it uses Drupal Queue for queueing messages
 * but the regular messaging store for anything else (pull messages, message logs)
 *
 */
class Messaging_Queue extends Messaging_Store {
  /**
   * Remove expired messages from queue
   */
  public static function queue_expire_messages() {
    // Do nothing, we cannot 'see' messages on the queue without going to the db bypassing the Queue interface
    // Anyway we check whether messages are expired before sending them using the queue worker
    // @see messaging_queue_worker()
    return 0;
  }
  
  /**
   * Get Drupal Queue object
   */
  protected static function queue() {
    return drupal_queue_get('messaging');
  }
  /**
   * Queue message for next delivery
   */
  public static function message_queue($message) {
    $message->queue = 1;
    $message->queued = TRUE;
    // Set created date if not previously set
    if (empty($message->created)) {
      $message->created = time();
    }
    return self::queue()->createItem($message);
  }
  
  /**
   * Count queued messages
   */
  public static function queue_count() {
    return self::queue()->numberOfItems();
  }
  /**
   * Process single messag from queue. Note we change visibility for this method
   */
  public static function queue_process_message($message) {
    $result = parent::queue_process_message($message);
    // If it needs logging use the default messaging_store
    if ($message->log && !$message->logged) {
      self::message_log($message);
    }
    return $result;
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
  public static function queue_process($limits = array()) {
    $results = array();
    $limit = self::process_limits($limits);
    return self::queue_process_step($limit['message'], $limit['timeout']);
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
  protected static function queue_process_step($limit, $timeout = 0) {
    $count = 0;
    $sent = $unsent = $processed = array();
    $queue = self::queue();
    while ((!$limit || $count <= $limit) && (!$timeout || time() <= $timeout) && ($item = $queue->claimItem())) {
      $message = $item->data;
      $success = self::queue_process_message($message);
      $queue->deleteItem($item);

      $processed[] = $success;
      if ($success) {
        $sent[] = $message->mqid;
        messaging_debug('Processed message from queue', array('message' => $message, 'success' => $success));
      } else {
        $unsent[] = $message->mqid;
        watchdog('messaging', 'Failed queue processing for @message', array('@message' => (string)$message), WATCHDOG_WARNING);
      }
      $count++;
    }
    return $processed;
  }
 
  /**
   * Get status summary
   * 
   * @param $fields
   *   Fields to queue and group by
   * @param $conditions
   *   Array of field conditions to restrict the query
   *   
   * @return array
   *   Array of arrays with the status fields and a 'total' counter for each row
   */
  public static function get_status($fields, $conditions = array()) {
    // Get status about logs/errors from messaging_store
    $status = parent::get_status($fields, $conditions);
    
    // We can just provide information about the number of items for all methods
    $status[] = t('There are @count queued messages for all methods.', array('@count' => self::queue_count()));

    return $status;
  }
  
  /**
   * Delete all message from queue and logs
   */
  public static function delete_all() {
    self::queue()->deleteQueue();
    $count = db_affected_rows();
    $count += parent::delete_all();
    return $count;
  }

  /**
   * Get help for admin pages
   */
  public static function admin_help() {
    return array(
      'name' => t('Messaging Queue.'),
      'description' => t('It uses <i>Drupal Queue</i> module for handling queued messages.'),
      'queue' => t('Queued messages can be processed on cron or using multiple workers with <i>drush queue-cron</i>.'),
      'logs' => t('Logs will be kept if enabled using the <i>Messaging Store</i> tables.'),
    );
  }
}