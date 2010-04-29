<?php
// $Id$
/**
 * @file
 * Messaging Framework - Admin UI
 * 
 * This file includes the settings page and administration for message templates
 * 
 * By Development Seed, http://wwww.developmentseed.org
 */

/**
 * Admin settings form
 */
function messaging_admin_settings() {
  // Get plug-in information and produce big warning if none enabled.
  $methods = messaging_method_list();
  if (!$methods) {
    // Get message from requirements
    if ($reqs = messaging_requirements('runtime')) {
      drupal_set_message($reqs['messaging']['value'], 'error');
    }    
  }  
  $form['general'] = array(
    '#type' => 'fieldset',
    '#title' => t('General settings'),
  );
  $form['general']['messaging_default_method'] = array(
    '#title' => t('Default send method'),
    '#type' => 'radios',
    '#options' => $methods,
    '#default_value' => variable_get('messaging_default_method', ''),
  );

  return system_settings_form($form);
}

/**
 * Default sending methods settings
 */
function messaging_admin_method_settings() {
  // Sending methods settings
  if ($info = messaging_method_info()) {
    $form['methods'] = array('#theme' => 'messaging_admin_settings_table');
    foreach ($info as $method => $options) {
      $options += array('log' => 0, 'queue' => 0);
      $key = 'messaging_method_'.$method;
      // This will preserve settings for disabled modules
      $form['methods'][$key]['#tree'] = TRUE;
      $form['methods'][$key]['title'] = array(
        '#title' => t('Method'),
        '#value' => $options['title'],
      );
      // Display name
      $form['methods'][$key]['name'] = array(
        '#title' => t('Name'),
        '#type' => 'textfield',
        '#default_value' => $options['name'],
        '#size'          => 20,
      );
      // Log and queue settings
      $form['methods'][$key]['queue'] = array(
        '#title' => t('Queue'),
        '#type' => 'checkbox',
        '#default_value' => $options['queue'],
        '#disabled' => $options['type'] & Messaging_Send_Method::TYPE_PULL, // Pull methods will be always queued
      );
      $form['methods'][$key]['log'] = array(
        '#title' => t('Log'),
        '#type' => 'checkbox',
        '#default_value' => $options['log'],
        '#disabled' => variable_get('messaging_log', 0) == 0,
      );
      // Description
      $form['methods'][$key]['description'] = array(
        '#title' => t('Description'),
        '#value' => $options['description'],
      );
    }
  } else {
    $form['warning'] = array('#value' => t('You should enable some messaging method plug-ins for this to work.'));
  }
  $form = system_settings_form($form);
  // Refresh strings after update if translation enabled
  if (module_exists('i18nstrings')) {
    $form['#submit'][] = 'messaging_locale_refresh';
  }
  return $form;
}

/**
 * Settings for filter and formatting for each sending method
 * 
 * This page requires 'administer filters' permission so it doesn't need further checking
 */
function messaging_admin_method_filters() {
  // Add information about input formats
  messaging_include('text.inc');
  // List of Input formats
  $format_options = $filter_options = array();
  foreach (filter_formats() as $format) {
    $format_options[$format->format] = $format->name;
  }
  $format_default = filter_resolve_format(FILTER_FORMAT_DEFAULT);
  // List of messaging filters
  $filter_info = array();
  foreach (messaging_text_filter_info() as $key => $filter) {
    $filter_options[$key] = $filter['name'];
    $filter_info[] = array($filter['name'], $filter['description']);
  }
  // We add this last for it not bo be default
  $filter_options[''] = t('No filter (Insecure)');
  $format_options[''] = t('No filter (Insecure)');
  $form['filter_info'] = array(
    '#title' => t('Available filters'),
    '#type' => 'item',
    '#value' => theme('table', array(), $filter_info),
  );
  // Sending methods settings
  if ($info = messaging_method_info()) {
    $form['methods'] = array('#theme' => 'messaging_admin_settings_table');
    foreach ($info as $method => $options) {
      $key = 'messaging_filters_'.$method;
      // This will preserve settings for disabled modules
      $form['methods'][$key]['#tree'] = TRUE;
      $form['methods'][$key]['title'] = array(
        '#title' => t('Method'),
        '#value' => $options['title'],
      );
      // Output filter applied to message body
      $form['methods'][$key]['body_format'] = array(
        '#title' => t('Format filter'),
        '#type' => 'select',
        '#default_value' => isset($options['body_format']) ? $options['body_format'] : $format_default,
        '#options' => $format_options,
      );
      // Final filter applied to message body
      $form['methods'][$key]['filter'] = array(
        '#title' => t('Final filter'),
        '#type' => 'select',
        '#default_value' => $options['filter'],
        '#options' => $filter_options,
      );
    }
  } else {
    $form['warning'] = array('#value' => t('You should enable some messaging method plug-ins for this to work.'));
  }
  
  $form = system_settings_form($form);

  return $form;
}

/**
 * Queue options form
 */
function messaging_admin_queue_settings($form_state) {
  $form['messaging_queue_process_cron'] = array(
    '#type' => 'radios',
    '#title' => t('Process on cron'),
    '#default_value' => variable_get('messaging_queue_process_cron', 1),
    '#options' => array(t('Disabled'), t('Enabled')),
    '#description' => t('If disabled you must use code elsewhere to actually send your queued messages such as a <a href="http://drupal.org/project/drush">drush</a> script.'),
  );
  // Logging and expiration settings  
  $period = array(0 => t('Disabled')) + drupal_map_assoc(array(3600, 10800, 21600, 32400, 43200, 86400, 172800, 259200, 604800, 1209600, 2419200, 4838400, 9676800), 'format_interval');
  $form['messaging_log'] = array(
    '#title' => t('Logging'),
    '#type' => 'select',
    '#options' => $period,
    '#default_value' => variable_get('messaging_log', 0),
    '#description' => t('If enabled all messages for the <a href="@messaging-methods">selected methods</a> will be logged and kept for the specified time after they\'re sent.', array('@messaging-methods' => url('admin/messaging/settings/method'))),
  );
  $form['messaging_queue_expire'] = array(
    '#title' => t('Expiration'),
    '#type' => 'select',
    '#options' => $period,
    '#default_value' => variable_get('messaging_queue_expire', 0),
    '#description' => t('If enabled all queued messages will be deleted after the selected time, even when they haven\'t been sent yet.'),
  );
  /*
  $form['messaging_queue_pending_limit'] = array(
    '#title' => t('Limit pending messages'),
    '#type' => 'textfield',
    '#size' => 10,
    '#default_value' => variable_get('messaging_queue_pending_limit', 0),
    '#description' => t('Discard older messages when the queue exceeds this limit. Some messages may be lost but this will keep your processing under control. Set 0 (zero) for no limit.'),
  );
  */
  // Processing limits
  $limit = variable_get('messaging_process_limit', array('message' => 0, 'percent' => 0, 'time' => 0));
  $form['messaging_process_limit'] = array(
    '#type' => 'fieldset',
    '#title' => t('Limits for queue processing'),
    '#tree' => TRUE,
    '#collapsible' => TRUE, '#collapsed' => !array_filter($limit),
    '#description' => t('These are the limits for each cron run on queue processing. The process will stop when it first meets any of them. Set to 0 for no limit.'),
  );
  $form['messaging_process_limit']['message'] = array(
    '#title' => t('Number of messages sent'),
    '#type' => 'textfield',
    '#size' => 10,
    '#default_value' => $limit['message'],
  );
  $form['messaging_process_limit']['time'] = array(
    '#title' => t('Time (seconds)'),
    '#type' => 'textfield',
    '#size' => 10,
    '#default_value' => $limit['time'],
  );
  $form['messaging_process_limit']['percent'] = array(
    '#title' => t('Time (% of cron time)'),
    '#type' => 'textfield',
    '#size' => 10,
    '#default_value' => $limit['percent'],
    '#description' => t('Maximum percentage of cron time the process may use.'),
  );
  return system_settings_form($form);  
}

/**
 * Queue status page
 */
function messaging_admin_queue_status() { 
  $output = '';

  // Compile data from db
  $status = array('queue' => 0, 'error' => 0, 'log' => 0, 'other' => 0);
  //$result = db_query("SELECT method, queue, cron, error, log, count(mqid) AS total FROM {messaging_store} GROUP BY method, queue, cron, error, log");
  $summary = messaging_store()->get_status(array('method', 'queue', 'cron', 'error', 'log'));
  if ($summary) {
    foreach ($summary as $data) {
      $method = $data['method'];
      if (!isset($methods[$method])) {
        $methods[$method] = array('queue' => 0, 'error' => 0, 'log' => 0, 'other' => 0);
      }
      $value = (int)$data['total'];
      if ($data['error']) {
        $field = 'error';
      }
      elseif ($data['queue']) {
        $field = 'queue';
      }
      elseif ($data['log']) {
        $field = 'log';
      }
      else {
        $field = 'other';
      }
      $methods[$method][$field] += $value;
      $status[$field] += $value;
    }
  }
  // Build table if we've got any data
  if ($methods) {
    $send_methods = messaging_method_list();
    $header = array(t('Method'), t('Queued'), t('Errors'), t('Logs'), t('Unknown'), t('total'));
    foreach ($methods as $method => $data) {
      $rows[] = array(
        isset($send_methods[$method]) ? '<strong>' . $send_methods[$method] . '</strong>' : $method . ' ' . t('disabled'),
        $data['queue'], $data['error'], $data['log'], $data['other'], array_sum($data)
      );
    }
    $status['total'] = array_sum($status);
    $rows[] = array(t('Total'), $status['queue'], $status['error'], $status['log'], $status['other'], $status['total']);
    $content = theme('table', $header, $rows);
    $output .= drupal_get_form('messaging_admin_queue_operations_form', $status);
  }
  else {
    $content = t('No data in queue.');
  }
  $output .= theme('box', t('Queue status'), $content);
  return $output;
}

/**
 * Queue operations form
 */
function messaging_admin_queue_operations_form($form_state, $status) {
  $form = $help = array();
  if ($status['queue']) {
    $operations['queue'] = array('#type' => 'submit', '#value' => t('Process queue'));
  }
  if ($status['total']) {
    $operations['clean'] = array('#type' => 'submit', '#value' => t('Clean up'));
    $help[] = t('The <i>Clean up</i> operation will remove expired logs and messages (Or all logs if no expiration set).');
    $operations['delete'] = array(
      '#type' => 'submit', '#value' => t('Delete all'),
      '#prefix' => '<div style="float:right;">',
      '#suffix' => '</div>',
    );
  }
  if (!empty($operations)) {
    // Pass current max row id, so any operation doesn't affect new rows being produced.
    $form['mqid'] = array(
      '#type' => 'value',
      '#value' => db_result(db_query("SELECT MAX(mqid) FROM {messaging_store}")),
    );
    $form['operations'] = $operations += array('#title' => t('Operations'), '#type' => t('fieldset'));
    if ($help) {
      $form['help']['#value'] = theme('item_list', $help);
    }
  }
  return $form;
}

/**
 * Submit queue operations form
 */
function messaging_admin_queue_operations_form_submit($form, $form_state) {
  $mqid = $form_state['values']['mqid'];
  $deleted = $processed = $errors = 0;

  switch ($form_state['values']['op']) {
    case t('Process queue'):
      // If cron is running, we better let the queue alone
      if (variable_get('messaging_queue_process_cron', TRUE) && variable_get('cron_semaphore', FALSE)) {
        drupal_set_message(t('We cannot manually process messages while cron is running. Try again later and/or disable queue processing on cron.'), 'warning');
      }
      else {
        $limit = variable_get('messaging_process_limit', array('message' => 0, 'time' => 0, 'percent' => 0));
        $results = messaging_store()->queue_process($limit);
        if ($results) {
          $processed = count($results);
          $success = count(array_filter($results));
          $errors = $processed - $success;
        }
      }
      break;
 
    case t('Clean up');
      //$deleted = messaging_store('queue_clean', TRUE);
      $deleted = messaging_store()->queue_clean(TRUE);
      break;

    case t('Delete all');
      messaging_store()->delete_multiple(array('max_mqid' => $mqid));
      $deleted = db_affected_rows();
      break;

  }
  if ($processed) {
    drupal_set_message(t('Processed @count messages from queue.', array('@count' => $processed)));
    if ($errors) {
      drupal_set_message(t('Found @count errors during queue processing.', array('@count' => $errors)), 'warning');
    }
  }
  if (!empty($deleted)) {
    drupal_set_message(t('Deleted @count messages from queue.', array('@count' => $deleted)));
  }
}

/**
 * Theme settings as table
 */
function theme_messaging_admin_settings_table($elements) {
  if (isset($elements['#table_header'])) {
    $header = $elements['#table_header'];
    $extract_headers = FALSE;
  }
  else {
    $header = array();
    $extract_headers = TRUE;
  }
  $rows = array();
  foreach (element_children($elements) as $index) {
    $row = array();
    foreach (element_children($elements[$index]) as $key) {
      if ($extract_headers) {
        // Extract the titles from elements
        $header[] = $elements[$index][$key]['#title'];
      }
      if (isset($elements[$index][$key]['#title'])) {
        unset($elements[$index][$key]['#title']);
      }
      $row[] = drupal_render($elements[$index][$key]);
    }
    $rows[] = $row;
    $extract_headers = FALSE;
  }
  if ($rows) {
    $output = theme('table', $header, $rows);
  }
  else {
    $output = t('No data available');
  }
  $output .= drupal_render($elements);
  return $output;  
}