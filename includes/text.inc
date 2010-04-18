<?php
// $Id$
/**
 * @file
 * Drupal Messaging Framework - Text filtering functions
 */

/**
 * HTML to text simple filtering.
 * - Replace some tags with line endings: p, br, hr, li, h1, h2, h3, h4
 * Strip out all HTML tags and decode entities
 * 
 * @param $text
 *   Text to clean up
 * @param $break
 *   Optional character to replace tags for line breaks
 */
function messaging_text_check_plain($text, $break = "\n") {
  // This have to be done before the filtering because tag markers may have been previously parsed with check_plain
  $text = str_replace(array('&lt;', '&gt;'), array('<', '>'), $text);
  // Clean up the HTML and replace some tags with line endings
  if (isset($break)) {
    $text = _filter_htmlcorrector($text);
    $text = str_replace(array('</p>', '<br />', '<hr />', '</li>', '</h1>', '</h2>', '</h3>', '</h4>'), $break, $text);
  }
  // Final text clean up
  return messaging_text_clean($text);
}


/**
 * Clean text of HTML stuff and optionally of line endings
 * 
 * @param $text
 *   Dirty HTML text to be filtered
 * @param $newline
 *   Optional string to be used as line ending
 */
function messaging_text_clean($text, $newline= NULL) {
  // HTML entities to plain text conversion.
  $text = decode_entities($text);  
  // Filters out all remaining HTML tags
  $text = filter_xss($text, array());
  // Optionally, replace new lines
  if (!is_null($newline)) {
    $text = str_replace("\n", $newline, $text);
  }
  // Trim out remaining beginning/ending spaces
  $text = trim($text);
  return $text;
}

/**
 * Apply filter to message text
 */
function messaging_text_filter($text, $filter) {
  if (!$filter) {
    // Not filtering for this method
    return $text;
  }
  elseif ($function = messaging_text_filter_info($filter, 'filter callback')) {
    return $function($text);
  }
  else {
    // Filter not valid or not available anymore. Apply plaintext filter.
    return messaging_text_check_plain($text);
  }
}

/**
 * Get information of available filters for message texts
 */
function messaging_text_filter_info($name = NULL, $property = NULL) {
  $info = &messaging_static(__FUNCTION__);
  
  if (!$info) {
    $info = module_invoke_all('messaging', 'text filters');
    // Allow altering by other modules
    drupal_alter('messaging_filters', $info);
  }

  return messaging_array_info($info, $name, $property);
}

/**
 * Get filter information for a method
 * 
 * @param $method
 *   Send method key
 * @param $get_info
 *   Whether to get full filter info array or just filter key
 */
function messaging_text_method_filter($method, $get_info = TRUE) {
  $info = messaging_method_info($method);
  if (!empty($info['filter']) && ($filter_info = messaging_text_filter_info($info['filter']))) {
    // We have a valid filter for this method
    return $get_info ? $filter_info : $info['filter'];
  }
  elseif (isset($info['filter']) && empty($info['filter'])) {
    // This method has no filtering
    return 0;
  }
  else {
    // Invalid filter or no filter set, return the default one
    return $get_info ? messaging_text_filter_info('messaging_plaintext') : 'messaging_plaintext';
  }
}

/**
 * Get built in filters info
 */
function _messaging_text_filter_info() {
  $filters['messaging_plaintext'] = array(
    'name' => t('Plain text'),
    'description' => t('Filters out all HTML tags and replaces HTML entities by characters, respects HTML line breaks.'),
    'filter callback' => 'messaging_text_check_plain',
  );
  $filters['messaging_html'] = array(
    'name' => t('HTML text'),
    'description' => t('Filters out dangerous HTML tags, fixes faulty HTML, converts line breaks into HTML, and turns web and e-mail addresses into clickable links. '),
    'filter callback' => 'messaging_text_format_html',
  );
  $filters['drupal_html2text'] = array(
    'name' => t('HTML to text'),
    'description' => t('Replaces HTML tags and entities with plain text formatting, moving links at the end.'),
    'filter callback' => 'drupal_html_to_text',
  );
  // Add the default input format too so we can have some configurable filter
  $format = filter_formats(_messaging_text_filter_default());
  $filters['messaging_filter'] = array(
    'name' => $format->name,
    'description' => theme('filter_tips', _filter_tips($format->format, FALSE)),
    'filter callback' => 'check_markup',
  );       
  return $filters;  
}

/**
 * Get input format available for messaging. Hidden variable that can be set
 */
function _messaging_text_filter_default() {
  return variable_get('messaging_default_filter', filter_resolve_format(FILTER_FORMAT_DEFAULT));
}

/**
 * Default html filter. 
 * 
 * We use a fake 'messaging' format so all variables affecting this filtering can be overridden
 * - filter_html_messaging = FILTER_HTML_STRIP || FILTER_HTML_ESCAPE
 * - allowed_html_messaging, defaults to '<a> <em> <strong> <cite> <code> <ul> <ol> <li> <dl> <dt> <dd>'
 * - filter_url_length_messaging, defaults to 72
 */
function messaging_text_format_html($text, $format = 'messaging') {
  // Turn URLs into links
  $text = _filter_url($text, $format);
  // Filter out dangerous HTML.
  $text = _filter_html($text, $format);
  // Convert line breaks
  $text = _filter_autop($text);
  // Fix faulty HTML
  $text = _filter_htmlcorrector($text);
  return $text;
}

/**
 * Composes message from different parts, recursively and applies filter
 * 
 * Filter is applied now only once
 * 
 * @param $text
 *   Simple string or array of message parts
 *   It may have named elements like #prefix and #text
 *   or it may be single strings to render straight forward
 * @param $glue
 *   Text to glue all lines together
 * @param $filter
 *   Input format to apply to the results
 */
function messaging_text_render($text, $glue = '', $filter = NULL) {
  $output = '';

  if (is_array($text)) {
    if (isset($text['#prefix'])) {
      $output .= $text['#prefix'].$glue;
      unset($text['#prefix']);
    }
    if (isset($text['#text'])) {
      $output .= $text['#text'];
      return $output;
    }
    foreach (element_children($text) as $key) {
      // The filter is not passed along
      $text[$key] = messaging_text_render($text[$key], $glue);
    }
    $output .= implode($glue, $text);
  } else {
    $output .= $text;
  }
  // The filter is applied now only once
  if (isset($filter)) {
    return messaging_text_filter($output, $filter);
  }
  else {
    return $output;
  }
}

/**
 * Truncate messages to given length.  Adapted from node_teaser() in node.module
 */
function messaging_text_truncate($text, $length) {
  // If we have a short message, return the message
  if (drupal_strlen($text) < $length) {
    return $text;
  }  
  // Initial slice.
  $teaser = truncate_utf8($text, $length);
  $position = 0;
  // Cache the reverse of the message.
  $reversed = strrev($teaser); 
  // split at paragraph boundaries.
  $breakpoints = array('</p>' => 0, '<br />' => 6, '<br>' => 4, "\n" => 1);
  // We use strpos on the reversed needle and haystack for speed.
  foreach ($breakpoints as $point => $offset) {
    $length = strpos($reversed, strrev($point));
    if ($length !== FALSE) {
      $position = - $length - $offset;
      return ($position == 0) ? $teaser : substr($teaser, 0, $position);
    }
  } 
  // When even the first paragraph is too long, we try to split at the end of
  // the last full sentence.
  $breakpoints = array('. ' => 1, '! ' => 1, '? ' => 1, ' ' => 0);
  $min_length = strlen($reversed);
  foreach ($breakpoints as $point => $offset) {
    $length = strpos($reversed, strrev($point));
    if ($length !== FALSE) {
      $min_length = min($length, $min_length);
      $position = 0 - $length - $offset;
    }
  }
  return ($position == 0) ? $teaser : substr($teaser, 0, $position);
}


// Some other function wrappers, for backwards compatibility with old sending methods

/**
 * HTML to text simple filtering.
 */
function messaging_check_plain($text, $break = NULL) {
  return messaging_text_check_plain($text, $break);
}

/**
 * Converts strings to plain utf-8 single line
 */
function messaging_check_subject($text) {
  return Messaging_Method::check_subject($text);
}

/**
 * Build a simple text with message subject and body
 */
function messaging_text_build($message, $glue = ' ') {
  return $message->get_text($glue);
}
