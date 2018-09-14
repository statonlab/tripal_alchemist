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

  // Merge the arrays
  $bundles = [
    'select' => '-- Select a Bundle --'
  ] + $bundles;

  $form['bundles'] = [
    '#title' => t('Select a bundle'),
    '#type' => 'select',
    '#options' => $bundles,
    '#description' => t('Bundles that have entities with no associated chado records are listed above.'),
  ];

  $form['submit'] = [
    '#type' => 'submit',
    '#value' => 'Permanently Delete Orphaned Entities',
  ];

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

  $counts = [];

  foreach ($bundles as $bundle) {
    $count = tripal_alchemist_get_orphaned_counts_for_bundle($bundle);
    if ($count > 0) {
      $counts[$bundle->bundle_id] = "{$bundle->label} has {$count} orphaned entities.";
    }
  }

  return $counts;
}

/**
 * Get the count of orphaned entities per bundle.
 *
 * @param $bundle
 *
 * @return int
 */
function tripal_alchemist_get_orphaned_counts_for_bundle($bundle) {
  $bundle_table = db_escape_table("chado_bio_data_{$bundle->bundle_id}");
  $chado_table = db_escape_table($bundle->data_table);
  $schema = chado_get_schema($chado_table);
  $primary_key = is_array($schema) ? $schema['primary key'][0] : NULL;
  $count = 0;

  if ($primary_key) {
    $count = db_query('SELECT count(*) FROM ' . $bundle_table . ' BT
                        WHERE BT.record_id NOT IN (
                          SELECT ' . $primary_key . ' FROM chado.' . $chado_table . '
                        )')->fetchField();
  }

  return (int) $count;
}
