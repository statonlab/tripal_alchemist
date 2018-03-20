<?php
/**
 * Form hooks and general form helper functions (building tables etc)
 *
 */

/**
 * Implements hook_form to provide base Alchemist admin form.
 *
 * @param $form
 * @param $form_state
 *
 * @return mixed
 */

function tripal_alchemist_converter_form($form, &$form_state) {

  $all_bundles_object = tripal_alchemist_get_all_bundles();

  $all_bundles_array = [];
  $bundle_options = [];

  foreach ($all_bundles_object as $bundle) {
    // Do I really need to do this?
    if ($bundle->type != 'TripalEntity') {
      continue;
    }
    $bundle_options[$bundle->id] = $bundle->label;
    $all_bundles_array[$bundle->id] = $bundle;
  }

  $form['instructions'] = [
    '#markup' => '<p>Welcome to Tripal Alchemist. This module is designed to transform Tripal Entities from one type to another (a process called "transmutation" in classical alchemy).  Your destination bundle must already exist before transforming.</p><p>Please note that Tripal Alchemist will only convert <b>Tripal 3 Entities</b>.  Tripal 2 content types (nodes) must be migrated.</p>
        <p>Full documentation of this module is available on <a href = "https://github.com/statonlab/tripal_alchemist/">the projects Github page.</a></p>
        ',
  ];

  $form['transmutation_type'] = [
    '#type' => 'radios',
    '#title' => 'Transformation Method',
    '#description' => 'Tripal Alchemist can automatically convert all entities from a source bundle to a destination bundle, if those entities already meet the criteria for the destination bundle.  This typically means that a type_id column or property is already set.  You can instead choose to manually select entities to convert. Alternatively, you can create a collection, and convert the entities within that collection.',
    '#options' => [
      'Automatic' => 'Automatic',
      'Manual' => 'Manual',
      'Collections' => 'Collections',
    ],
    '#default_value' => 'Automatic',
    '#ajax' => [
      'callback' => 'tripal_alchemist_source_bundle_callback',
      'wrapper' => 'div1_wrapper',
      'effect' => 'fade',
    ],
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
      'wrapper' => 'div1_wrapper',
      'effect' => 'fade',
    ],
  ];

  // Create the divs for AJAX rebuilding.  One for the destination bundle select, one for the transaction overview
  $form['div1'] = [
    '#type' => 'fieldset',
    '#collapsible' => FALSE,
    '#prefix' => '<div id="div1_wrapper">',
    '#suffix' => '</div>',
  ];

  $form['div2'] = [
    '#type' => 'fieldset',
    '#collapsible' => FALSE,
    '#prefix' => '<div id="div2_wrapper">',
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
      '#markup' => "<p>Your selected source bundle <b>$source_bundle->label</b> has <b>no entities</b> in the database.  You must populate the source bundle first, either by creating content, migrating Tripal 2 nodes, or publishing Chado content.</p>",
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
      'wrapper' => 'div2_wrapper',
      'effect' => 'fade',
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

  $transmutation_type = $form_state['values']['transmutation_type'];//expect automatic, manual, collection

  if ($transmutation_type == 'Automatic') {
    $table = tripal_alchemist_build_transaction_table($source_bundle, $destination_bundle);

    if (!$table) {
      $form['div2']['kill'] = ['#markup' => '<p>No qualifying entries.  Please pick another source or destination table.</p>'];
      return $form;
    }
    $markup = '<h3>Qualifying Entities</h3><p>The first 10 Entities from the source bundle that meet the Destination bundle storage criteria are listed below.</p><p><b>ALL</b> of the qualifying entities will be transformed into the destination entity bundle type.</p>';

    $markup .= theme('table', $table);

    $form['div2']['transaction_summary'] = [
      '#type' => 'markup',
      '#markup' => $markup,
    ];
  }

  //if manual, we're going to do instead build a checkbox table
  if ($transmutation_type == 'Manual') {
    $form = tripal_alchemist_build_choosable_table($form, $source_bundle);
  }

  if ($transmutation_type == 'Collections') {
    //provide dropdown list of collections
    $form = tripal_alchemist_provide_collection_list($form, $source_bundle);
  }

  $form['div2']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Transform (Submit)'),
  ];

  return $form;
}


function tripal_alchemist_converter_form_submit($form, &$form_state) {

  global $user;

  $source_bundle_id = $form_state['values']['source_bundle'];
  $destination_bundle_id = $form_state['values']['destination_bundle'];
  $transmutation_type = $form_state['values']['transmutation_type'];

  $source_bundle = tripal_load_bundle_entity(['id' => $source_bundle_id]);
  $destination_bundle = tripal_load_bundle_entity(['id' => $destination_bundle_id]);

  $job_args = [
    "source_bundle_id" => $source_bundle_id,
    "destination_bundle_id" => $destination_bundle_id,
  ];

  $job_name = "Alchemist transform " . $source_bundle->label . " to " . $destination_bundle->label;


  //Add in extra arguments for manual and collection
  if ($transmutation_type == 'Manual') {
    $source_entities = $form_state['values']['manual_entity_select'];
    $source_entities = array_filter($source_entities);//remove unchecked selections
    $job_args["source_entities"] = $source_entities;
  }
  if ($transmutation_type == 'Collections') {
    $collection_id = $form_state['values']['collection_id'];
    $job_args["collection_id"] = $collection_id;
  }

  tripal_add_job($job_name, 'tripal_alchemist', 'tripal_alchemist_run_conversion_job', [$transmutation_type, $job_args], $user->uid);

  tripal_set_message("Job submitted!", TRIPAL_INFO);

}

function tripal_alchemist_source_bundle_callback($form) {
  return $form['div1'];

}

function tripal_alchemist_destination_bundle_callback($form) {
  return $form['div2'];
}

/**
 * Builds the chooseable table for manual transformation.
 *
 * @param $form
 * @param $source_bundle
 *
 * @return null
 *
 * @ingroup Tripal_Alchemist_Form
 */

function tripal_alchemist_build_choosable_table($form, $source_bundle) {

  //TODO this would be much better if it was searchable by property or anything really...

  $source_table = "chado." . $source_bundle->data_table;//CBT  CHADO BASE TABLE
  $source_entity_table = "public.chado_" . $source_bundle->name; //SET SOURCE ENTITY TABLE

  $type_linker_table = NULL;

  //TODO:  Joining on the data_table_id is risky.  This works for analysis but which ones does it fail on?
  //TODO:  CBT.name: is this fair?  What can I get that's relaiably human readable?
  $query = db_select($source_table, 'CBT');
  $query->fields('SET', ['record_id', 'entity_id']);
  $query->fields('CBT', ['name']);
  $query->innerJoin($source_entity_table, 'SET', 'SET.record_id = CBT.' . $source_bundle->data_table . '_id');
  $results = $query->execute()->fetchAll();

  $rows = [];

  $result_count = 0;
  $overflow = FALSE;

  foreach ($results as $result) {

    if ($result_count > 500) {
      $overflow = TRUE;
      break;
    }
    $rows[$result->entity_id] = [
      'record_id' => $result->record_id,
      'entity_id' => $result->entity_id,
      'name' => $result->name,
    ];
    $result_count++;
  }

  if (!$rows) {
    return NULL;
  }

  $headers = [
    'record_id' => t('Record ID'),
    'entity_id' => t('Entity ID'),
    'name' => t('Name'),
  ];

  $form['div2']['overflow'] = [
    '#markdown' => '<p>Warning: over 500 qualifying entities, only the first 500 are    displayed below. You can still manually select from this table.  To convert more entities, please set the type property and use the automatic converter.
</p>',
  ];
  $form['div2']['manual_entity_select'] = [
    '#type' => 'tableselect',
    '#header' => $headers,
    '#options' => $rows,
    '#empty' => t('The source bundle is empty'),
  ];

  return ($form);
}

/**
 * Provides a list of collections that include entities within the source bundle.
 *
 * @param $form
 * @param $source_bundle
 *
 * @return mixed
 *
 * @ingroup Tripal_Alchemist_form
 */

function tripal_alchemist_provide_collection_list($form, $source_bundle) {
  global $user;
  $collections = tripal_get_user_collections($user->uid);
  $options = [];
  foreach ($collections as $collection) {

    $entities = $collection->getEntityIDs($source_bundle->name);//Now need to provide the bundle name.

    $matching_entities = [];
    foreach ($entities as $entity) {
      $source_entity_table = "public.chado_" . $source_bundle->name; //SET SOURCE ENTITY TABLE
      $query = db_select($source_entity_table, 'SET')
        ->fields('SET')
        ->condition('SET.entity_id', $entity, "=");
      $result = $query->execute()->fetchAll();
      if ($result) {
        $matching_entities[] = $entity;
      }
    }
    $num_entities = count($entities);
    $num_matching_entities = count($matching_entities);
    $collection_name = $collection->getName();
    $collection_id = $collection->getCollectionID();
    if ($num_matching_entities > 0) {
      $options[$collection_id] = [
        $collection_id => t("$collection_name ($num_matching_entities of $num_entities eligible)"),
      ];
    }
  }//end collection loop

  if (count($options) === 0) {
    $form['div2']['no_collection'] = [
      '#markup' => "<h3>No Collections available</h3><p>You do not currently have any collections with entities matching this source bundle.  Please create a collection of entities you would like to convert.</p>",
    ];
    return $form;
  }

  $form['div2']['collection'] = [
    '#type' => 'select',
    '#title' => t('Available Collections'),
    '#description' => t('Collections.  Alchemist will convert all eligible entities in a collection form the source bundle to the destination bundle.'),
    '#default' => NULL,
    '#empty_option' => t('- Select -'),
    '#options' => $options,
  ];

  return $form;
}


/**
 * Builds a table to display the entities that will be converted given the
 * source and destination bundle.
 *
 * @param $source_bundle
 * @param $destination_bundle
 *
 * @return array|null
 *
 * @ingroup Tripal_Alchemist_Form
 */
function tripal_alchemist_build_transaction_table($source_bundle, $destination_bundle) {

  // What we want to do is a) Pull all the **source**  bundle entities
  // and look at how many of those qualify for the destination bundle.
  $source_table = "chado." . $source_bundle->data_table;//CBT  CHADO BASE TABLE
  $source_entity_table = "public.chado_" . $source_bundle->name; //SET SOURCE ENTITY TABLE
  $destination_entity_table = "public.chado_" . $destination_bundle->name;
  $type_column = $destination_bundle->type_column;
  $type_id = $destination_bundle->type_id;
  $type_value = $destination_bundle->type_value;

  $type_linker_table = NULL;

  if (!$destination_bundle->type_linker_table && $destination_bundle->type_column) {
    //this is a type_id column situation, not a linker table situation
    $query = db_select($source_table, 'CBT');
    $query->fields('SET', ['record_id', 'entity_id']);
    $query->fields('CBT', ['name']);
    $query->innerJoin($source_entity_table, 'SET', 'SET.record_id = CBT.' . $source_bundle->data_table . '_id');
    $query->leftJoin($destination_entity_table, 'DET', 'SET.record_id = DET.record_id');
    $query->condition('CBT.' . $type_column, $type_id);
    $query->condition('DET.record_id', NULL, 'IS');
    $results = $query->execute()->fetchAll();
  }
  elseif ($destination_bundle->type_linker_table && $destination_bundle->type_column && $destination_bundle->type_value) {
    //This is a linker table type transmutation
    $type_linker_table = "chado." . $destination_bundle->type_linker_table; //CLT CHADO LINKER TABLE

    //TODO:  Joining on the data_table_id is risky.  This works for analysis but which ones does it fail on?
    //note: I could use the get_schema() to figure out the primary key
    //TODO:  CBT.name: is this fair?  What can I get that's relaiably human readable?
    $query = db_select($source_table, 'CBT');
    $query->fields('SET', ['record_id', 'entity_id']);
    $query->fields('CBT', ['name']);
    $query->innerJoin($source_entity_table, 'SET', 'SET.record_id = CBT.' . $source_bundle->data_table . '_id');
    $query->innerJoin($type_linker_table, 'CLT', 'CBT.' . $source_bundle->data_table . '_id = CLT.' . $source_bundle->data_table . '_id');
    $query->leftJoin($destination_entity_table, 'DET', 'SET.record_id = DET.record_id');
    $query->condition('CLT.' . $type_column, $type_id);
    $query->condition('CLT.value', $type_value);
    $query->condition('DET.record_id', NULL, 'IS');
    $results = $query->execute()->fetchAll();

  }
  else {
    tripal_set_message("destination bundle is not properly set.", TRIPAL_ERROR);
    return;
  }

  $rows = [];

  $result_count = 0;
  $overflow = FALSE;

  foreach ($results as $result) {
    if ($result_count > 10) {
      $overflow = TRUE;
      break;
    }
    $rows[] = [
      $result->record_id,
      $result->entity_id,
      $result->name,
    ];
    $result_count++;

  }

  if (!$rows) {
    return NULL;
  }

  $headers = ['Record ID', 'Entity ID', 'Name'];

  $table = [
    'header' => $headers,
    'rows' => $rows,
  ];

  return $table;

}

