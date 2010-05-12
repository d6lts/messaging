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
   * Capabilities: whether this Queue can expire old messages
   */
  public static function can_expire() {
    return FALSE;
  }
  /**
   * Prevent processing messages on cron
   */
  public static function cron_process() {
    // Do nothing
  }
  /**
   * Queue message for next delivery
   */
  public static function message_queue($message) {
    $message->queue = 1;
    $message->queued = TRUE;
    return drupal_queue_get('messaging')->createItem($message);
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
    $status[] = t('The number of queued messages for all methods is: @count', array('@count' => drupal_queue_get('messaging')->numberOfItems()));

    return $status;
  }

  /**
   * Get help for admin pages
   */
  public static function admin_help() {
    return array(
      'name' => t('Messaging Queue.'),
      'description' => t('It uses <i>Drupal Queue</i> module instead of <i>cron</i> for handling queued messages.'),
      'queue' => t('Queued messages can be processed using multiple workers with <i>drush queue-cron</i>.'),
      'logs' => t('Configure your logging settings on the next tab.'),
    );
  }
  /**
   * Get more settings for Admin page
   */
  public static function admin_settings() {
    // No more settings
  }
}