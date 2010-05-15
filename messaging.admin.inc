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