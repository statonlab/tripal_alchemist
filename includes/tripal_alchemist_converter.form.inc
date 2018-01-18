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
    '#markup' => '<p>Welcome to Tripal Alchemist. This module is designed to convert Tripal Entities from one type to another.  Your destination bundle must already exist before transmuting.</p><p>Please note that Tripal Alchemist will only convert <b>Tripal 3 Entities</b>.  Tripal 2 content types (nodes) must be migrated.</p>
        <p>Full documentation of this module is available on <a href = "https://github.com/statonlab/tripal_alchemist/">the projects Github page.</a></p>
        ',
  ];

  $form['source_bundle'] = [
    '#type' => 'select',
    '#title' => t('Source Bundle'),
    '#description' => t('The source bundle, which contains the entities you would like to transform.'),
    '#empty_option' => t('- Select -'),
    '#options' => $bundle_options,
    '#default_value' => NULL,
    '#ajax' => [
      'callback' => 'tripal_alchemist_source_bundle_callback',
      'wrapper' => 'destination_bundle_wrapper',
      'effect' => 'fade',
    ],
  ];


  //create the divs for AJAX rebuilding.  One for the destination bundle select, one for the transaction overview
  $form['div1'] = [
    '#type' => 'fieldset',
    '#collapsible' => FALSE,
    '#prefix' => '<div id="destination_bundle_wrapper">',
    '#suffix' => '</div>',
  ];

  $form['div2'] = [
    '#type' => 'fieldset',
    '#collapsible' => FALSE,
    '#prefix' => '<div id="report_on_migrations_wrapper">',
    '#suffix' => '</div>',
  ];

  $selected_base_table = isset($form_state['values']['source_bundle']) ? $form_state['values']['source_bundle'] : NULL;

  if (!$selected_base_table) {
    $form['div1']['instructions_no_select'] = [
      '#markup' => '<h2>Destination Bundle</h2><p>First, select a <b>Source Bundle</b> above.  You will then be able to choose a destination bundle from al bundles sharing a base table.</p>',
    ];
    return $form;
  }


  $source_bundle = $all_bundles_array[$selected_base_table];
  $source_table = $source_bundle->data_table;

  //How many records here?

  $source_count = tripal_alchemist_fetch_current_bundle_entities($source_bundle);


  if (!$source_count) {
    $form['div1']['feedback'] = [
      '#markup' => "<p>Your selected source bundle <b>$source_bundle->label</b> has <b>no entities</b> in the database.  You must populate the source bundle first, either by creating content or migrating Tripal 2 nodes.</p>",
    ];
  }
  $form['div1']['feedback'] = [
    '#markup' => "<p>There are  <b>$source_count</b> records in the $source_bundle->label bundle.</p>",
  ];

  //Create dropdown list of bundles for source that match that table

  $matching_destination_bundles = tripal_alchemist_get_matching_bundles($source_table);
  unset($matching_destination_bundles[$selected_base_table]);//remove the source bundle

  if (count($matching_destination_bundles) == 0) {
    $form['div1']['instructions_no_matches'] = [
      '#markup' => "<p>Your selected source bundle is the only bundle using the base table <b>$source_table.</b></p><p>Please select a different source bundle, or define a new bundle with the same base table to serve as your destination bundle.</p>",
    ];

    return $form;

  }

  $form['div1']['destination_bundle'] = [
    '#type' => 'select',
    '#title' => t('Destination Bundle'),
    '#description' => t('Destination bundle.  Your selected entities will be converted to Tripal Entities of this bundle type.'),
    '#default' => NULL,
    '#empty_option' => t('- Select -'),
    '#options' => $matching_destination_bundles,
    '#ajax' => [
      'callback' => 'tripal_alchemist_destination_bundle_callback',
      'wrapper' => 'report_on_migrations_wrapper',
    ],
  ];


  $selected_destination = isset($form_state['values']['destination_bundle']) ? $form_state['values']['destination_bundle'] : NULL;

  if (!$selected_destination) {
    $form['div2']['need_destination'] = [
      '#markup' => "<p>Please select a destination bundle</p>",
    ];
    return $form;
  }

  $destination_bundle = $all_bundles_array[$selected_destination];

  $table = tripal_alchemist_build_transaction_table($source_bundle, $destination_bundle);


  if (!$table) {
    $form['div2']['kill'] = ['#markup' => '<p>No qualifying entries.  Please pick another source or destination table.</p>'];

    return $form;
  }

  $markup = '<h3>Qualifying Entities</h3><p>Entities from the source bundle that meet the Destination bundle storage criteria are listed below.</p><p><b>ALL</b> of the below entities will be transformed into the destination entity bundle type.  Future versions of Tripal Alchemist will allow transforming select entities or overwriting properties.</p>';

  $markup .= theme('table', $table);

  $form['div2']['transaction_summary'] = [
    '#type' => 'markup',
    '#markup' => $markup,
  ];


  $form['div2']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Submit'),
  ];

  return $form;
}


function tripal_alchemist_converter_form_submit($form, &$form_state) {
  global $user;

  $source_bundle_id = $form_state['values']['source_bundle'];
  $destination_bundle_id = $form_state['values']['destination_bundle'];


  $source_bundle = tripal_load_bundle_entity(['id' => $source_bundle_id]);
  $destination_bundle = tripal_load_bundle_entity(['id' => $destination_bundle_id]);

  $job_args = [
    "source_bundle" => $source_bundle,
    "destination_bundle" => $destination_bundle,
  ];

  // tripal_add_job("Alchemist transmute:" . $source_bundle->label . "to " . $destination_bundle->label, 'tripal_alchemist', 'tripal_alchemist_convert_all_entities', $job_args, $user->uid, 10);

  tripal_set_message("Transmutation successful!", TRIPAL_INFO);
  tripal_alchemist_convert_all_entities($job_args);
}


function tripal_alchemist_source_bundle_callback($form) {
  return $form['div1'];
}

function tripal_alchemist_destination_bundle_callback($form) {
  return $form['div2'];
}


/**
 * Builds a table to display the entities that will be converted given the
 * source and destination bundle.
 *
 * @param $source_bundle
 * @param $destination_bundle
 *
 * @return array|null
 */
function tripal_alchemist_build_transaction_table($source_bundle, $destination_bundle) {

  // what we want to do is a) Pull all the **source**  bundle entities
  // and look at how many of those qualify for the destination bundle.
  $source_table = "chado." . $source_bundle->data_table;//CBT  CHADO BASE TABLE
  $source_entity_table = "public.chado_" . $source_bundle->name; //SET SOURCE ENTITY TABLE


  if (!$destination_bundle->type_linker_table) {
    //todo this check needs to be more rigorous: check that theres a type_column and a type_linker table first, mabe the bundle just doesnt have a type anything
    tripal_set_message("Your destination bundle uses a type_id rather than a type linker table.  This feature has not yet been developed.", TRIPAL_ERROR);
    return;
  }

  $type_linker_table = "chado." . $destination_bundle->type_linker_table; //CLT CHADO LINKER TABLE
  $type_column = $destination_bundle->type_column;
  $type_id = $destination_bundle->type_id;
  $type_value = $destination_bundle->type_value;

  //TODO:  Based on the above values, we need to modify the SQL to select from a typie_id column or from a linker table.


  //TODO:  joining on the data_table_id is risky.  This works for analysis but which ones does it fail on?
  //TODO:  CBT.name: is this fair?  What can i get thats relaiably human readable?

  $sql = "SELECT SET.record_id, SET.entity_id, CBT.name  FROM {" . db_escape_table($source_table) . "} CBT 
          INNER JOIN {" . db_escape_table($source_entity_table) . "} SET ON SET.record_id = CBT." . $source_bundle->data_table . "_id
          INNER JOIN {" . db_escape_table($type_linker_table) . "} CLT ON CBT." . $source_bundle->data_table . "_id = CLT." . $source_bundle->data_table . "_id 
          WHERE CLT." . $type_column . " = :type_id
          AND CLT.value = :type_value 
          ";

  $results = db_query($sql, [
    ":type_id" => $type_id,
    ":type_value" => $type_value,
  ]);

  $rows = [];

  foreach ($results as $result) {
    $entry = [];
    $entry = [
      $result->record_id,
      $result->entity_id,
      $result->name,
    ];

    $rows[] = $entry;

  }

  if (!$rows) {
    return NULL;
  }

  $headers = ['Record ID', 'Entity ID', 'Name'];

  $table = [
    'header' => $headers,
    'rows' => $rows,
  ];

  return ($table);
}

