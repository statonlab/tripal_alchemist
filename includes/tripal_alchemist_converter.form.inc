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


  $form['div1'] = [
    '#type' => 'fieldset',
    '#collapsible' => FALSE,
  ];

  $form['div1']['markup_1'] = ['#markup' => '<div id="destination_bundle_wrapper">'];

  $selected_base_table = isset($form_state['values']['source_bundle']) ? $form_state['values']['source_bundle'] : NULL;

  if (!$selected_base_table) {
    $form['div1']['instructions_no_select'] = [
      '#type' => '#markup',
      '#markup' => '<p>To begin, select a Source Bundle above.</p>',
    ];
    return $form;
  }

  //Create dropdown list of bundles for source that match that table

  $source_bundle = $all_bundles_array[$selected_base_table];
  $source_table = $source_bundle->data_table;

  $matching_destination_bundles = tripal_alchemist_get_matching_bundles($source_table);
  unset($matching_destination_bundles[$selected_base_table]);//remove the source bundle

  if (count($matching_destination_bundles) == 0) {
    $form['div1']['instructions_no_matches'] = [
      '#markup' => "<p>Your selected source bundle is the only bundle using the base table $source_table.</p><p>Please select a different source bundle, or define a new bundle with the same base table.</p>",
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

  $form['div1close'] = ['#type' => 'markup', '#markup' => '</div>'];


  $form['div2'] = [
    '#type' => 'fieldset',
    '#collapsible' => FALSE,
  ];

  $form['div2']['markup_2'] = ['#markup' => '<div id="report_on_migrations_wrapper">'];

  $selected_destination = isset($form_state['values']['destination_bundle']) ? $form_state['values']['destination_bundle'] : NULL;

  if (!$selected_destination) {
    $form['div2']['need_destination'] = [
      '#markup' => "<p>Please select a destination bundle</p>",
    ];

    return $form;
  }


  dpm($selected_destination);

  $destination_bundle = $all_bundles_array[$selected_destination];

  $table = tripal_alchemist_build_transaction_table($source_bundle, $destination_bundle);

  dpm("here is table");
  dpm($table);

  $markup = '<h3>Qualifying Entities</h3><p>Entities from the source bundle that meet the Destination bundle storage criteria are listed below.</p><p><b>ALL</b> of the below entities will be transformed into the destination entity bundle type.  Future versions of Tripal Alchemist will allow transforming select entities or overwriting properties.</p>';

  if (!$table) {
$form['kill'] = ['#markup' =>'<p>No qualifying entries.  Please pick another destination table.</p>' ];

return $form;
  }
 // $markup .= theme_table($table);

  dpm("here");

  $form['div2']['transaction_summary'] = [
    '#type' => 'markup',
    '#markup' => $markup,
  ];


  $form['div2close'] = ['#type' => 'markup', '#markup' => '</div>'];

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


/**
 * Builds a table to display the entities that will be converted given the source and destination bundle.
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

  $type_linker_table = "chado." . $destination_bundle->type_linker_table; //CLT CHADO LINKER TABLE
  $type_column = $destination_bundle->type_column;
  $type_id = $destination_bundle->type_id;
  $type_value = $destination_bundle->type_value;

  //TODO:  joining on the data_table_id is risky.  This works for analysis but which ones does it fail on?
  //TODO:  CBT.name: is this fair?  What can i get thats relaiably human readable?

  $sql = "SELECT SET.record_id, SET.entity_id, CBT.name  FROM {" . db_escape_table($source_table) . "} CBT 
          INNER JOIN {" . db_escape_table($source_entity_table) . "} SET ON SET.record_id = CBT." . $source_bundle->data_table . "_id
          INNER JOIN {" . db_escape_table($type_linker_table) . "} CLT ON CBT." . $source_bundle->data_table . "_id = CLT." . $source_bundle->data_table . "_id 
          WHERE CLT." . $type_column . " = :type_id
          AND CLT.value = :type_value 
          ";
  dpm($sql);

  $results = db_query($sql, [
    ":type_id" => $type_id,
    ":type_value" => $type_value,
  ]);

  $rows = [];

  foreach ($results as $result) {
    $entry = [];
    dpm($result);
    $entry = [
      'record_id' => $result->record_id,
      'entity_id' => $result->entity_id,
    ];
    array_push($rows, $entry);
  }

  if (!$rows){
    return null;
  }

  $headers = ['record ID', 'entity ID', 'Name'];

  $table = [
    'header' => $headers,
    'rows' => $rows,
  ];

  return($table);
}

