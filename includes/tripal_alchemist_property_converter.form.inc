<?php

function tripal_alchemist_property_converter_form($form, &$form_state) {

  $form['instructions'] = [
    '#markup' => '<p>Welcome to Tripal Alchemist property converter.

This form will allow you to modify the properties of selected analyses so that they will be elligible to be transformed to new content types.
<p>Content types that share a chado table have a type, either via a property linker table or a type_id column. This tool will allow you to change existing entries to match a new type, so that they can be converted.</p>
        <p>Full documentation of this module is available on <a href = "https://github.com/statonlab/tripal_alchemist/">the projects Github page.</a></p>
        ',
  ];


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


  $source_bundle = "analysis";

  $source_options = tripal_alchemist_get_all_entities_from_bundle($source_bundle);


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
      'callback' => 'tripal_alchemist_properties_destination_bundle_callback',
      'wrapper' => 'report_on_migrations_wrapper',
    ],
  ];


  $selected_destination = isset($form_state['values']['destination_bundle']) ? $form_state['values']['destination_bundle'] : NULL;

  $destination_bundle = $all_bundles_array[$selected_destination];


  $source_options = tripal_alchemist_get_all_entities_from_bundle($source_bundle);

  $header = ['entity_id', 'name'];

  $form['div2']['select_entities_to_convert'] = [
    '#type' => 'tableselect',
    '#header' => $header,
    '#options' => $options,
    '#multiple' => TRUE

  ];

  return($form);
}


function tripal_alchemist_properties_destination_bundle_callback($form) {
  return $form['div1'];
}