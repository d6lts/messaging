<?php
// $Id$
/**
 * @file
 * Messaging Framework - Admin UI
 */

/*
 * Overview of message parts and sending methods
 */
function messaging_template_admin_template() {
  $output = '';
  // List message groups
  $templates = module_invoke_all('messaging', 'message groups');
  // Check fallbacks and get root templates for tree-like display
  $tree = messaging_template_build_tree($templates);
  // Now build table which will have some indentation
  $rows = messaging_template_format_tree($tree);
  $header = array(t('Message groups'), t('Parts')); //t('Description'), t('Parts'));
  $output .= theme('table', $header, $rows);

  // List sending methods
  $rows = array();
  messaging_method_list();
  foreach (messaging_method_info() as $method => $info) {
    $rows[] = array(
      '<strong>'. $info['name'] .'</strong>',
      !empty($info['description']) ? $info['description'] : ''
    );
  }
  $output .= theme('box', t('Sending methods'), theme('table', NULL, $rows));
  return $output;
}

/**
 * Format template tree as table rows
 */
function messaging_template_format_tree($tree, $depth = 0) {
  $rows = array();
  foreach ($tree as $key => $template) {
    $options['attributes'] = !empty($template['description']) ? array('title' => $template['description']) : array();
    $rows[] = array(
      theme('indentation', $depth) . l($template['name'], 'admin/messaging/template/edit/'.$key, $options),
      ($parts = module_invoke_all('messaging', 'message keys', $key)) ? implode(', ', $parts) : '',
    );
    if (!empty($template['children'])) {
      $rows = array_merge($rows, messaging_template_format_tree($template['children'], $depth + 1));
    }
  }
  return $rows;
}

/**
 * Build fallback tree
 */
function messaging_template_build_tree($templates) {
  $tree = array();
  foreach ($templates as $key => &$template) {
    if (empty($template['fallback'])) {
      $tree[$key] = &$template;
    }
    else {
      $templates[$template['fallback']]['children'][$key] = &$template;
    }
  }
  return $tree;
}

/**
 * Message groups edit page
 */
function messaging_template_admin_template_edit($group, $langcode = NULL) {
  $language_list = language_list();
  $language = $language_list[$langcode];
  $output = '';
  if ($group && $language) {
    drupal_set_title(t('Template for %name', array('%name' => $group['name'])));
    $output .= drupal_get_form('messaging_template_admin_message_form', $group, $language);
  }
  return $output;
}

/**
 * Edit message formats
 */
function messaging_template_admin_message_form($form_state, $group_info, $language) {
  $group = $group_info['group'];
  $form['group'] = array('#type' => 'value', '#value' => $group);
  $form['language'] = array('#type' => 'value', '#value' => $language);
  $form['module'] = array('#type' => 'value', '#value' => isset($group_info['module']) ? $group_info['module'] : '');
  $keylist = module_invoke_all('messaging', 'message keys', $group, $language);
  $send_methods = array('default' => t('Default'));
  $send_methods += messaging_method_list();
  $form['messages'] = array('#tree' => TRUE);
  foreach ($keylist as $key => $keyname) {
    $form['messages'][$key] = array(
      '#type' => 'fieldset',
      '#title' => $keyname,
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );
    foreach ($send_methods as $method => $methodname) {
      $text = messaging_template_message_part($group, $key, $method, $language, FALSE);
      $form['messages'][$key][$method] = array(
        '#title' => $methodname,
        '#type' => 'textarea',
        '#default_value' => $text,
        // Adjust size to actual number of lines
        '#rows' => count(explode("\n", $text)),        
      );    
    }
  }
  // Tokens for text replacement
  if ($tokens = messaging_template_tokens_get_list($group)) {
    $headers = array(t('Token'), t('Replacement value'));
    $rows = array();
    foreach ($tokens as $token => $token_description) {
        $row = array();
        $row[] = '[' . $token . ']';
        $row[] = $token_description;
        $rows[] = $row;
    }
    $form['tokens'] = array(
      '#title' => t('Available tokens'),
      '#type' => 'fieldset',
      '#description' => t('These special strings will be replaced by their real value at run time.'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );
    $form['tokens']['list'] = array(
      '#value' => theme('table', $headers, $rows, array('class' => 'description'))
    );
  }
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );
  $form['reset'] = array(
    '#type' => 'submit',
    '#value' => t('Reset to defaults'),
  );
  return $form;
}

/**
 * Get list of tokens for text replacement
 *
 * @param $group
 *   Message group to get tokens for
 * @param $tokens
 *
 */
function messaging_template_tokens_get_list($group) {
  // First compile token types for this message group
  $type_list = module_invoke_all('messaging', 'tokens', $group);
  // Add known global tokens, will be always available
  $type_list[] = 'global';

  // Now get token list from token module for each type
  $return = array();
  foreach ($type_list as $type) {
    // This is a shortcut for single tokens for digests, with the form (token, description)
    if (is_array($type)) {
      list($type, $token) = $type;
      $list = token_get_list($type);
      $return[$token] = $list[$type][$token];
    }
    elseif ($list = token_get_list($type)) {
      foreach ($list as $category => $tokens) {
        foreach ($tokens as $token => $description) {
          $return[$token] = $description;
        }
      }
    }
  }
  return $return;
}

/**
 * Process and save message parts
 */
function messaging_template_admin_message_form_submit($form, &$form_state) {
  $group = $form_state['values']['group'];
  $language = $form_state['values']['language'];
  $module = $form_state['values']['module'];
  switch ($form_state['values']['op']) {
    case t('Reset to defaults'):
      db_query("DELETE FROM {messaging_message_parts} WHERE type = '%s' AND language = '%s'", $group, $language->language);
      drupal_set_message('The message templates have been reset to default values.');
      break;
    case t('Save'):
      foreach ($form_state['values']['messages'] as $key => $messages) {
        foreach ($messages as $method => $text) {
          db_query("DELETE FROM {messaging_message_parts} WHERE type = '%s' AND msgkey = '%s' AND method = '%s' AND language = '%s'", $group, $key, $method, $language->language);
          if (trim($text)) {
            db_query("INSERT INTO {messaging_message_parts} (type, msgkey, method, language, module, message) VALUES('%s', '%s', '%s', '%s', '%s' '', '%s')", $group, $key, $method, $language->language, $module, $text);
          }
        }
      }
      drupal_set_message('The message templates have been updated.');
      break;
  }
 
}
