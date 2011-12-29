<?php

// Constant value for the title of the orphaned products table row.
define('COMMERCE_PDM_ORPHANS_ROW_TITLE', t('Products not assigned to any display node'));

/**
 * Creates the display manager form.
 */
function commerce_pdm_product_display_manager_form($form, &$form_state) {
  $form = array();

  $form['#tree'] = TRUE;

  $product_display_types = _commerce_pdm_get_product_display_types(FALSE);
  $nodes_query = db_select('node', 'n')
    ->fields('n', array('nid', 'title'))
    ->orderBy('n.title')
    ->addTag('node_access');

  $conditions = db_or();
  foreach ($product_display_types as $product_display_type) {
    $conditions->condition('type', $product_display_type['machine_name']);
  }
  if ($conditions->count()) {
    $nodes_query->condition($conditions);
  }
  $nodes_query_result = $nodes_query->execute();

  $current_path = $_GET['q'];
  if ($nodes_query_result->rowCount() > 0) {
    // Add all display nodes and their referenced products to the form.
    foreach($nodes_query_result as $node_obj) {
      $form[] = array(
        'title' => array(
          '#type' => 'item',
          '#markup' => $node_obj->title,
        ),
        'edit_link' => array(
          '#type' => 'link',
          '#title' => t('Edit'),
          '#href' => 'node/' . $node_obj->nid . '/edit',
              '#options' => array('query' => array('destination' => $current_path)),
        ),
        'delete_link' => array(
          '#type' => 'link',
          '#title' => t('Delete'),
          '#href' => 'node/' . $node_obj->nid . '/delete',
          '#options' => array('query' => array('destination' => $current_path)),
        ),
        'nid' => array(
          '#type' => 'hidden',
          '#value' => $node_obj->nid,
        ),
      );

      $products_referenced = _commerce_pdm_get_products_referenced($node_obj->nid);
      if (!empty($products_referenced)) {
        // Store db_or conditions for loading orphan products later.
        if (!isset($orphan_conditions)) {
          $orphan_conditions = db_and();
        }

        $products_query = db_select('commerce_product', 'cp')
          ->fields('cp', array('product_id', 'sku', 'title'));
        $cp_conditions = db_or();
        foreach ($products_referenced as $product_id) {
          $cp_conditions->condition('product_id', $product_id);

          // Add a NOT condition for the orphan query.
          $orphan_conditions->condition('product_id', $product_id, '!=');
        }
        $products_query->condition($cp_conditions);
        $products_result = $products_query->execute();
        $products_result_count = $products_result->rowCount();

        foreach ($products_result as $key => $related_product) {
          $form[] = array(
            'title' => array(
              '#type' => 'item',
              '#markup' => $related_product->title,
            ),
            'edit_link' => array(
              '#type' => 'link',
              '#title' => t('Edit'),
              '#href' => 'admin/commerce/products/' . $related_product->product_id . '/edit',
              '#options' => array('query' => array('destination' => $current_path)),
            ),
            'delete_link' => array(
              '#type' => 'link',
              '#title' => t('Delete'),
              '#href' => 'admin/commerce/products/' . $related_product->product_id . '/delete',
              '#options' => array('query' => array('destination' => $current_path)),
            ),
            'pid' => array(
              '#type' => 'hidden',
              '#default_value' => $related_product->product_id,
            ),
            'delta' => array(
              '#type' => 'weight',
              '#title' => t('Delta'),
              '#title_display' => 'invisible',
            ),
          );

          $referenced_product_ids[] = $related_product->product_id;
        }
      }
    }

    // Load all products currently not referenced.
    $orphans_query = db_select('commerce_product', 'cp')
      ->fields('cp', array('product_id', 'sku', 'title'));
    if (isset($orphan_conditions)) {
      $orphans_query->condition($orphan_conditions);
    }
    $orphans_result = $orphans_query->execute();
    $orphans_result_count = $orphans_result->rowCount();

    $form[] = array(
      'title' => array(
        '#type' => 'item',
        '#markup' => COMMERCE_PDM_ORPHANS_ROW_TITLE
      ),
      'nid' => array(
        '#type' => 'hidden',
        '#value' => 0,
      ),
    );

    // Add all orphan products to the table.
    if ($orphans_result_count > 0) {
      foreach ($orphans_result as $orphan_key => $orphan_product) {
        $form[] = array(
          'title' => array(
            '#type' => 'item',
            '#markup' => $orphan_product->title,
          ),
          'edit_link' => array(
            '#type' => 'link',
            '#title' => t('Edit'),
            '#href' => 'admin/commerce/products/' . $orphan_product->product_id . '/edit',
            '#options' => array('query' => array('destination' => $current_path)),
          ),
          'delete_link' => array(
            '#type' => 'link',
            '#title' => t('Delete'),
            '#href' => 'admin/commerce/products/' . $orphan_product->product_id . '/delete',
            '#options' => array('query' => array('destination' => $current_path)),
          ),
          'pid' => array(
            '#type' => 'hidden',
            '#default_value' => $orphan_product->product_id,
          ),
          'orphan' => array(
            '#type' => 'hidden',
            '#default_value' => TRUE,
          ),
          'delta' => array(
            '#type' => 'weight',
            '#title' => t('Delta'),
            '#title_display' => 'invisible',
          ),
        );
      }
    }
  }
  else { // Nothing to display.
    $form['no_product_displays'] = array(
      '#markup' => '<div style="margin-bottom:20px;">' . t('Your store does not yet contain any product display nodes.') . '</div>',
    );
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );

  // Adds JavaScript and CSS used by form.
  $form['#after_build'] = array('_commerce_pdm_product_display_manager_form_attach');

  // Turns it into a table.
  $form['#theme'] = 'commerce_pdm_product_display_manager_table';

  return $form;
}

/**
 * Creates a batch operation that will update all display nodes.
 */
function commerce_pdm_product_display_manager_form_validate($form, &$form_state) {
  $hierarchy = array();
  $current_display_node = NULL;
  foreach ($form_state['input'] as $input) {
    if (is_array($input)) {
      if (isset($input['nid'])) {
        if ($current_display_node) {
          $hierarchy[] = $current_display_node;
        }
        $current_display_node = array('nid' => $input['nid'], 'products' => array());
      }
      else {
        $current_display_node['products'][] = $input['pid'];
      }
    }
  }

  foreach ($hierarchy as $key => $display_node) {


    $node = node_load($display_node['nid']);
    if (!_commerce_pdm_get_number_reference_values_allowed($node, count($display_node['products']))) {
      form_set_error($key . '[nid]', t('Display node with title %note_title cannot contain the specified number of product references, field limit is set to %field_limit.',
        array('%note_title' => $node->title, '%field_limit' => _commerce_pdm_get_num_allowed_references($node))));
    }
  }
}

/**
 * Creates a batch operation that will update all display nodes.
 */
function commerce_pdm_product_display_manager_form_submit($form, &$form_state) {
  $hierarchy = array();
  $current_display_node = NULL;
  foreach ($form_state['input'] as $input) {
    if (is_array($input)) {
      if (isset($input['nid'])) {
        if ($current_display_node) {
          $hierarchy[] = $current_display_node;
        }
        $current_display_node = array('nid' => $input['nid'], 'products' => array());
      }
      else {
        $current_display_node['products'][] = $input['pid'];
      }
    }
  }

  // Create a skeleton for the update batch.
  $batch = array(
    'title' => t('Updating display nodes'),
    'operations' => array(),
    'finished' => '_commerce_pdm_display_nodes_updated',
    'file' => drupal_get_path('module', 'commerce_pdm') . '/commerce_pdm.admin.inc',
  );

  // Add operations.
  foreach ($hierarchy as $product_display_hierarchy) {
    $batch['operations'][] = array(
      '_commerce_pdm_update_display_node',
      array($product_display_hierarchy),
    );
  }

  // Initialize the update batch process.
  batch_set($batch);
}

/**
 * Displays the display manager form as a draggable table.
 */
function theme_commerce_pdm_product_display_manager_table($variables) {
  $form = &$variables['form'];

  drupal_add_tabledrag('commerce_pdm_product_display_manager', 'order', 'sibling', 'delta');

  $header = array (
    t('Product Display Title / Product Title'), t('Edit'), t('Delete'), t('Delta'),
  );

  $rows = array();
	foreach (element_children($form) as $key) {
    $form_element = &$form[$key];

    if (isset($form_element['title'])) {
      if (isset($form_element['nid'])) {  // It's a display node.
        $form_element['nid']['#attributes']['class'] = 'display-nid';

        $row = array();
        $row[] = drupal_render($form_element['title']) . drupal_render($form_element['nid']);
        $row[] = ($form_element['title']['#markup'] == COMMERCE_PDM_ORPHANS_ROW_TITLE) ? '' : drupal_render($form_element['edit_link']);
        $row[] = ($form_element['title']['#markup'] == COMMERCE_PDM_ORPHANS_ROW_TITLE) ? '' : drupal_render($form_element['delete_link']);
        $row[] = '';

        $row_class = ($form_element['title']['#markup'] == COMMERCE_PDM_ORPHANS_ROW_TITLE) ? 'orphans' : '';
        $rows[] = array('data' => $row, 'class' => array('display-node', $row_class));
      }
      else if (!isset($form_element['orphan'])) {  // It's referenced product.
        $form_element['delta']['#attributes']['class'] = array('delta');

        $row = array();
        $row[] = theme('indentation', array('size' => 1)) . drupal_render($form_element['title']);
        $row[] = drupal_render($form_element['edit_link']);
        $row[] = drupal_render($form_element['delete_link']);
        $row[] = drupal_render($form_element['delta']);

        $rows[] = array('data' => $row, 'class' => array('draggable', 'tabledrag-leaf', 'product'));
      }
      else {  // It's an orphan product.
        $form_element['delta']['#attributes']['class'] = array('delta');

        $row = array();
        $row[] = theme('indentation', array('size' => 1)) . drupal_render($form_element['title']);
        $row[] = drupal_render($form_element['edit_link']);
        $row[] = drupal_render($form_element['delete_link']);
        $row[] = drupal_render($form_element['delta']);

        $rows[] = array('data' => $row, 'class' => array('draggable', 'tabledrag-leaf', 'product-orphan'));
      }
		}
	}

  if (!empty($rows)) {
    $rows[0]['class'][] = 'first';
  }

  $output = theme('table', array('header' => $header, 'rows' => $rows, 'attributes' => array('id' => 'commerce_pdm_product_display_manager')));
  $output .= drupal_render_children($form);

  return $output;
}

/*
function commerce_pdm_form_commerce_product_ui_product_delete_form_alter(&$form, &$form_state, $form_id) {

}
function commerce_pdm_form_node_delete_confirm_alter(&$form, &$form_state, $form_id) {

}
*/

/**
 * Updates any given display node to reference to specified products.
 */
function _commerce_pdm_update_display_node($update_data, &$context) {
  $display_node = node_load($update_data['nid']);
  $context['message'] = t('Updating product display node %node_title', array('%node_title' => $display_node->title));

  $reference_field = _commerce_pdm_get_node_reference_field_name($display_node);
  $new_field_values = array();
  foreach($update_data['products'] as $product_id) {
    $new_field_values[] = array('product_id' => $product_id);
  }

  $values_match = ($new_field_values == $display_node->{$reference_field}[$display_node->language]);
  if (!$values_match) {
    if (!isset($display_node->{$reference_field}[$display_node->language])) {
      $display_node->{$reference_field} = array($display_node->language => array());
    }
    $display_node->{$reference_field}[$display_node->language] = $new_field_values;
    node_save($display_node);
  }

}

/**
 * The display node's update batch is complete.
 */
function _commerce_pdm_display_nodes_updated($success, $results, $operations) {
  if ($success) {
    $message = t('Products and their display nodes has been updated successfully.');
  }
  else {
    $message = t('There was an error updating your products and their display nodes.');
  }

  drupal_set_message($message);
}

/**
 * Helper function to return all products that are referenced by a node.
 */
function _commerce_pdm_get_products_referenced($nid) {
  $product_fields = _commerce_pdm_get_product_reference_fields();
  $out = array();
  foreach ($product_fields as $field) {
    $result = db_query(
      'SELECT ' . $field . '_product_id FROM {field_data_' . $field . '} df WHERE df.entity_id = :eid ORDER BY df.delta',
      array(
        ':eid' => $nid,
      )
    );

    foreach ($result as $product_obj) {
      $out[] = $product_obj->{$field . '_product_id'};
    }
  }
  return array_values($out);
}

/**
 *  Add necessary css and JavaScript files to the display manager form.
 */
function _commerce_pdm_product_display_manager_form_attach($form_element) {
  drupal_add_css(drupal_get_path('module', 'commerce_pdm') . '/commerce_pdm_product_display_manager_form.css');
  drupal_add_js(drupal_get_path('module', 'commerce_pdm') . '/commerce_pdm_product_display_manager_form.js');
  return $form_element;
}
