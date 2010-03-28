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
  $groups = module_invoke_all('messaging', 'message groups');
  // Check fallbacks and get root templates for tree-like display
  $root = array();
  foreach ($groups as $group => $group_info) {
    if (empty($group_info['fallback'])) {
      $root[] = $group;
    } else {
      $groups[$group_info['fallback']]['children'][] = $group;
    }
  }
  // Now build table which will have some indentation
  $rows = messaging_template_admin_template_tree($root, $groups);
  $header = array(t('Message groups'), t('Description'), t('Parts'));
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
 * Recursively build a table tree for fallback methods
 */
function messaging_template_admin_template_tree($parents, $groups, $depth = 0) {
  $rows = array();
  foreach ($parents as $key) {
    $info = $groups[$key];    
    $rows[] = array(
      theme('indentation', $depth) . l($info['name'], 'admin/messaging/template/edit/'.$key),
      !empty($info['description']) ? $info['description'] : '',
      ($parts = module_invoke_all('messaging', 'message keys', $key)) ? implode(', ', $parts) : '',
    );
    if (!empty($info['children'])) {
      $rows = array_merge($rows, messaging_template_admin_template_tree($info['children'], $groups, $depth + 1));
    }
  }
  return $rows;
}

/**
 * Message groups edit page
 */
function messaging_template_admin_template_edit($group) {
  $output = '';
  $groups = module_invoke_all('messaging', 'message groups');
  if (isset($groups[$group])) {
    drupal_set_title(t('Message template for %name', array('%name' => $groups[$group]['name'])));
    $output .= drupal_get_form('messaging_template_admin_message_form', $group, $groups[$group]);

  }
  return $output;
}

/**
 * Edit message formats
 */
function messaging_template_admin_message_form($form_state, $group, $group_info) {
  $form['group'] = array('#type' => 'value', '#value' => $group);
  $keylist = module_invoke_all('messaging', 'message keys', $group);
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
      $text = messaging_template_message_part($group, $key, $method, FALSE);
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
  foreach ($form_state['values']['messages'] as $key => $messages) {
    foreach ($messages as $method => $text) {
      db_query("DELETE FROM {messaging_message_parts} WHERE type = '%s' AND msgkey = '%s' AND method = '%s'", $group, $key, $method);
      if (trim($text)) {
        db_query("INSERT INTO {messaging_message_parts} (type, msgkey, method, module, message) VALUES('%s', '%s', '%s', '', '%s')", $group, $key, $method, $text);
      }
    }
  }
  drupal_set_message('The message templates have been updated');
}
