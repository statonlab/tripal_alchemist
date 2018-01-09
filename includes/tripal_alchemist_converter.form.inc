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
    '#markup' => "<p>Welcome to Tripal Alchemist. This module is designed to convert Tripal Entities from one type to another.</p><p>Please note that Tripal Alchemist will only convert <b>Tripal 3 Entities</b>.  Tripal 2 content types (nodes) must be migrated.</p>",
  ];

  $form['source_bundle'] = [
    '#type' => 'select',
    '#title' => t('Source Bundle'),
    '#description' => t('Source'),
    "#empty_option" => t('- Select -'),
    '#options' => $bundle_options,
    '#default_value' => NULL,
    '#ajax' => [
      'callback' => 'tripal_alchemist_source_bundle_callback',
      'wrapper' => 'destination_bundle_wrapper',
      'effect' => 'fade',
    ],
  ];


  $form['div1'] = ['#markup' => '<div id="destination_bundle_wrapper">'];
  $selected_base_table = isset($form_state['values']['source_bundle']) ? $form_state['values']['source_bundle'] : NULL;

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

  $matching_destination_bundles = tripal_alchemist_get_matching_bundles($source_table);
  unset($matching_destination_bundles[$selected_base_table]);//remove the source bundle

  if (count($matching_destination_bundles) == 0) {
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
    '#ajax' => [
      'callback' => 'tripal_alchemist_destination_bundle_callback',
      'wrapper' => 'report_on_migrations_wrapper',
    ],
  ];

  $form['div2'] = ['#markup' => '<div id="report_on_migrations_wrapper"> </div>'];


  $selected_destination = isset($form_state['values']['destination_bundle']) ? $form_state['values']['destination_bundle'] : NULL;

  if (!$selected_destination) {
    $form['need_destination'] = [
      '#markup' => "<p>Please select a destination bundle</p>",
    ];

    return $form;
  }


  $destination_bundle = $all_bundles_array[$selected_destination];

  $headers = [];
  $rows = [];

  $table = [
    'header' => $headers,
    'rows' => $rows,
  ];

  $table = tripal_alchemist_build_transaction_table($source_bundle, $destination_bundle);

  $markup = theme_table($table);

  $form['transaction_summary'] = [
    '#type' => 'markup',
    '#markup' => $markup,
  ];


  //  type_linker_table (String, 12 characters ) analysisprop
  //type_column (String, 7 characters ) type_id
  //type_id (String, 4 characters ) 2864
  //type_value (String, 17 characters ) interpro_analysis


  $form['div1close'] = ['#markup' => '</div>'];

  $form['Submit'] = [
    '#type' => 'submit',
    '#value' => t('Submit'),
  ];

  return $form;
}


function tripal_alchemist_converter_form_submit($form, &$form_state) {

}


function tripal_alchemist_source_bundle_callback($form) {
  return $form['div1'];
}

function tripal_alchemist_destination_bundle_callback($form) {
  return $form['div2'];
}

function tripal_alchemist_build_transaction_table($source_bundle, $destination_bundle) {


}

