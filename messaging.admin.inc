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
  // Logging settings  
  $period = array(0 => t('Disabled')) + drupal_map_assoc(array(3600, 10800, 21600, 32400, 43200, 86400, 172800, 259200, 604800, 1209600, 2419200, 4838400, 9676800), 'format_interval');
  $form['general']['messaging_log'] = array(
    '#title' => t('Logging'),
    '#type' => 'select',
    '#options' => $period,
    '#default_value' => variable_get('messaging_log', 0),
    '#description' => t('If enabled all messages will be logged and kept for the specified time after they\'re sent.'),
  );

  // Processing limits
  $limit = variable_get('messaging_process_limit', array('message' => 0, 'percent' => 0, 'time' => 0));
  $form['messaging_process_limit'] = array(
    '#type' => 'fieldset',
    '#title' => t('Limits for queue processing'),
    '#tree' => TRUE,
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
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
 * Default sending methods settings
 */
function messaging_admin_method_settings() {
  // Sending methods settings
  if ($info = messaging_method_info()) {
    $form['methods'] = array('#theme' => 'messaging_admin_method_settings');
    foreach ($info as $method => $options) {
      $key = 'messaging_method_'.$method;
      // This will preserve settings for disabled modules
      $form['methods'][$key]['#tree'] = TRUE;
      $form['methods'][$key]['title'] = array(
        '#value' => $options['title'],
      );
      // Display name
      $form['methods'][$key]['name'] = array(
        '#type' => 'textfield',
        '#default_value' => $options['name'],
        '#size'          => 20,
      );      
      // Output filter applied to message body
      $form['methods'][$key]['description'] = array(
        '#type' => 'textfield',
        '#default_value' => $options['description'],
        '#size' => 40,
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
    $form['methods'] = array('#theme' => 'messaging_admin_method_filters');
    foreach ($info as $method => $options) {
      $key = 'messaging_filters_'.$method;
      // This will preserve settings for disabled modules
      $form['methods'][$key]['#tree'] = TRUE;
      $form['methods'][$key]['title'] = array(
        '#value' => $options['title'],
      );
      // Output filter applied to message body
      $form['methods'][$key]['body_format'] = array(
        '#type' => 'select',
        '#default_value' => isset($options['body_format']) ? $options['body_format'] : $format_default,
        '#options' => $format_options,
      );
      // Final filter applied to message body
      $form['methods'][$key]['filter'] = array(
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
 * Theme method settings
 */
function theme_messaging_admin_method_settings($form) {
  $header = array(t('Method'), t('Name for display'), t('Description'));
  $rows = array();
  foreach (element_children($form) as $key) {
    $rows[] = array(
      drupal_render($form[$key]['title']),
      drupal_render($form[$key]['name']),
      drupal_render($form[$key]['description']),
    );
  }
  $output = theme('table', $header, $rows);
  $output .= drupal_render($form);
  return $output;
}

/**
 * Theme method settings
 */
function theme_messaging_admin_method_filters($form) {
  $header = array(t('Method'), t('Format filter'), t('Final filter'));
  $rows = array();
  foreach (element_children($form) as $key) {
    $rows[] = array(
      drupal_render($form[$key]['title']),
      drupal_render($form[$key]['body_format']),
      drupal_render($form[$key]['filter']),
    );
  }
  $output = theme('table', $header, $rows);
  $output .= drupal_render($form);
  return $output;
}