<?php

function tripal_alchemist_converter_form($form, &$form_state) {

  //Fetch all bundles

  $all_bundles_object = tripal_alchemist_get_all_bundles();

  $all_bundles_array = [];
  $bundle_options = [];

  foreach ($all_bundles_object as $bundle) {
    //Do I really need to do this?
    if ($bundle->type != 'TripalEntity') {
      continue;
    }
    $bundle_options[$bundle->id] = $bundle->label;
    $all_bundles_array[$bundle->id] = $bundle;
  }

  $form['instructions'] = [
    '#markup' => "<p>Welcome to Tripal Alchemist.  Instructions go here.</p><p>Please note that Tripal Alchemist will only convert <b>Tripal 3 Entities</b>.  Tripal 2 content types (nodes) must be migrated.</p>"
  ];

  $form['source_bundle'] = [
    '#type' => 'select',
    '#title' => t('Source Bundle'),
    '#description' => t('Source'),
    '#options' => $bundle_options,
    '#default_value' => NULL,
    '#ajax' => [
      'callback' => 'tripal_alchemist_source_bundle_callback',
      'wrapper' => 'destination_bundle_wrapper',
      'effect' => 'fade',
    ],
  ];

  $selected_base_table = isset($form_state['values']['source_bundle']) ? $form_state['values']['source_bundle'] : NULL;

  $form['#prefix'] = '<div id="destination_bundle_wrapper">';
  $form['#suffix'] = '</div>';

  //Create dropdown list of bundles for source that match that table
  if (!$selected_base_table) {

    $form['instructions_no_select'] = [
      '#type' => '#markup',
      '#markup' => '<p>To begin, select a Source Bundle above.</p>',

    ];

    return $form;

  }
  $source_bundle = $all_bundles_array[$selected_base_table];
  $source_table = $source_bundle->data_table;

  $matching_destination_bundles = tripal_alchemist_get_matching_bundles($source_table, $all_bundles_object);
  unset($matching_destination_bundles[$selected_base_table]);//remove the source bundle

  if (count($matching_destination_bundles) == 0){
    $form['instructions_no_matches'] = [
      '#markup' => "<p>Your selected source bundle is the only bundle using the base table $source_table.</p><p>Please select a different source bundle, or define a new bundle with the same base table.</p>",
    ];

    return $form;


  }

  $form['destination_bundle'] = [
    '#type' => 'select',
    '#title' => t('Destination Bundle'),
    '#description' => t('Destination bundle.  Your selected entities will be converted to Tripal Entities of this bundle type.'),
    '#options' => $matching_destination_bundles,
  ];

  //NOTE: SHOULDNT WE ABLE TO AUTOMATICALLY KNOW THE CVTERM BECAUSE YOU DEFINED IT IN THE BUNDLE? IN THE STORAGE SETTINGS????!?!!?!?


//  //Form to select prop CVterm
//
//  $form['prop_type_select'] = [];
//
//  //Form to select cvterm value
//
//  $form['cvterm_value_select'] = [];

  return $form;
}


function tripal_alchemist_converter_form_submit($form, &$form_state) {


}


function tripal_alchemist_get_all_bundles() {

  $bundles = [];

  // Get all bundle names to cycle through.
  $all_bundles = db_select('tripal_bundle', 'tb')
    ->fields('tb', ['name'])
    ->execute()->fetchAll();

  foreach ($all_bundles as $bundle_name) {
    // Get the bundle object.
    $bundle = tripal_load_bundle_entity(['name' => $bundle_name->name]);
    if (!$bundle) {
      tripal_report_error('tripal', TRIPAL_ERROR, "Unrecognized bundle name '%bundle'.",
        ['%bundle' => $bundle_name]);
      return FALSE;
    }

    array_push($bundles, $bundle);
  }

  return $bundles;
}

function tripal_alchemist_source_bundle_callback($form) {
  return $form;
}

function tripal_alchemist_get_matching_bundles($source_table, $all_bundles_object) {
  $options = [];


  foreach ($all_bundles_object as $bundle) {
    if ($bundle->data_table == $source_table) {
      $options[$bundle->id] = $bundle->label;
    }
  }

  return ($options);
}
