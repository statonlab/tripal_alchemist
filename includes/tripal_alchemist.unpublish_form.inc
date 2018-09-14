<?php

/**
 * Unpublish orphaned entities form.
 *
 * @param $form
 * @param $form_state
 */
function tripal_alchemist_unpublish_form($form, &$form_state) {
  $bundles = tripal_alchemist_get_orphaned_bundles();

  if (empty($bundles)) {
    $form['message'] = [
      '#type' => 'markup',
      '#markup' => t("<p>No orphaned entities detected.</p>"),
    ];

    return $form;
  }

  $form['bundles'] = [
    '#title' => t('Select a bundle'),
    '#type' => 'select',
    '#options' => $bundles,
    '#empty_option' => t('-- Select a Bundle --'),
    '#description' => t('Bundles that have entities with no associated chado records are listed above.'),
    '#ajax' => [
      'callback' => 'tripal_alchemist_unpublish_form_callback',
      'wrapper' => 'bundle_info_fieldset_wrapper',
    ],
  ];

  $form['bundle_info_fieldset'] = [
    '#type' => 'fieldset',
    '#collapsible' => FALSE,
    '#prefix' => '<div id="bundle_info_fieldset_wrapper">',
    '#suffix' => '</div>',
  ];

  $selected_bundle_id = isset($form_state['values']['bundles']) ? $form_state['values']['bundles'] : NULL;
  if ($selected_bundle_id) {
    $count = tripal_alchemist_get_orphaned_counts_for_bundle($selected_bundle_id);
    $form['bundle_info_fieldset']['message'] = [
      '#type' => 'markup',
      '#markup' => t('<p><strong>There are ' . $count . ' orphaned entities in this bundle.</strong></p>'),
    ];

    if ($count > 0) {
      $form['bundle_info_fieldset']['submit'] = [
        '#type' => 'submit',
        '#value' => 'Permanently Delete Orphaned Entities',
      ];
    }
  }

  return $form;
}

/**
 * Validate form entries.
 *
 * @param array $form
 * @param array $form_state
 */
function tripal_alchemist_unpublish_form_validate($form, &$form_state) {
  $bundle_id = isset($form_state['values']['bundles']) ? $form_state['values']['bundles'] : NULL;

  if (empty($bundle_id) || !is_numeric($bundle_id)) {
    form_set_error('bundles', t('Please select a valid bundle.'));
  }
}

/**
 * Process unpublish form.
 *
 * @param array $form
 * @param array $form_state
 */
function tripal_alchemist_unpublish_form_submit($form, &$form_state) {
  global $user;
  $bundle_id = isset($form_state['values']['bundles']) ? $form_state['values']['bundles'] : NULL;
  tripal_add_job('Delete Orphaned Entities', 'tripal_alchemist',
    'tripal_alchemist_delete_orphaned_entities', [$bundle_id], $user->uid, 10);

  drupal_set_message('Job submitted successfully!');
}

/**
 * Get bundles that have orphaned entities.
 *
 * @return array
 */
function tripal_alchemist_get_orphaned_bundles() {
  $bundles = db_query('SELECT bundle_id, data_table, label 
             FROM chado_bundle CB
             INNER JOIN tripal_bundle TB ON TB.id = CB.bundle_id')->fetchAll();

  $mapped = [];

  foreach ($bundles as $bundle) {
    $mapped[$bundle->bundle_id] = "{$bundle->label}";
  }

  return $mapped;
}

/**
 * Get the count of orphaned entities per bundle.
 *
 * @param int $bundle
 *
 * @return int
 */
function tripal_alchemist_get_orphaned_counts_for_bundle($bundle_id) {
  // Get the bundle
  $bundle = db_query('SELECT bundle_id, data_table, label 
             FROM chado_bundle CB
             INNER JOIN tripal_bundle TB ON TB.id = CB.bundle_id
             WHERE bundle_id = :id', [':id' => $bundle_id])->fetchObject();

  $bundle_table = db_escape_table("chado_bio_data_{$bundle->bundle_id}");
  $chado_table = db_escape_table($bundle->data_table);
  $schema = chado_get_schema($chado_table);
  $primary_key = is_array($schema) ? $schema['primary key'][0] : NULL;
  $count = 0;

  // Get the count
  if ($primary_key) {
    $count = db_query('SELECT count(*) FROM ' . $bundle_table . ' BT
                        LEFT JOIN chado.'.$chado_table.' CT ON BT.record_id = CT.'.$primary_key.'
                        WHERE CT.'.$primary_key.' IS NULL')->fetchField();
  }

  return (int) $count;
}

function tripal_alchemist_unpublish_form_callback($form) {
  return $form['bundle_info_fieldset'];
}