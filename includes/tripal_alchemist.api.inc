<?php

/**returns an array of all bundle objects.  Requires further processing to create menu items.
 *
 * @return array|bool
 *
 */
function tripal_alchemist_get_all_bundles() {

  $bundles = [];

  // Get all bundle names to cycle through.
  $all_bundles = db_select('tripal_bundle', 'tb')
    ->fields('tb', ['name'])
    ->execute()
    ->fetchAll();

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


/**Get bundles that map to a particular source table
 *
 * @param $source_table
 * @param $all_bundles_object
 *
 * @return array
 */

function tripal_alchemist_get_matching_bundles($source_table) {
  $options = [];

  $all_bundles_object = tripal_alchemist_get_all_bundles();

  foreach ($all_bundles_object as $bundle) {
    if ($bundle->data_table == $source_table) {
      //TODO Count how many eligible entities here
      $options[$bundle->id] = $bundle->label;
    }
  }
  return ($options);
}

//This call is deprecated and refactored by tripal_alchemist_convert_all_entities

///**Converts entities from one type to another based on the prop linker table
// *
// * @param $source_bundle
// * @param $destination_bundle
// * @param $descriminating_cvterm
// * @param $descriminating_prop_value
// */
//
//
//function tripal_alchemist_convert_entity($source_bundle, $destination_bundle, $descriminating_cvterm, $descriminating_prop_value) {
//
//
//  $descriminating_type_cvterm_id = $descriminating_cvterm->cvterm_id;
//
//  $source_bundle_table = "public.chado_" . $source_bundle->name;
//
//  $destination_term = $destination_bundle->term_id;
//  $destination_table = "public.chado_" . $destination_bundle->name;
//
//  print ("\nconverting " . $source_bundle_table . " to " . $destination_table . "\n");
//
//  $chado_base_table = "chado." . $source_bundle->data_table;
//  $chado_prop_table = $chado_base_table . "prop";
//
//  //  Copy the plain analysis entities to the new bundle type
//
//  $sql = "INSERT INTO " . $destination_table . " (entity_id, record_id)
//SELECT AB.entity_id AS entity_id, A.analysis_id AS record_id FROM " . $chado_base_table . "  A
//INNER JOIN  " . $chado_prop_table . " AP  ON A.analysis_id = AP.analysis_id
//INNER JOIN chado.cvterm CVT ON AP.type_id = CVT.cvterm_id
//INNER JOIN  " . $source_bundle_table . " AB ON A.analysis_id = AB.record_id
//WHERE CVT.cvterm_id = :cvterm_id
//AND AP.value = :descrim_prop_value";
//
//  $results = db_query($sql, [
//    ":cvterm_id" => $descriminating_type_cvterm_id,
//    ":descrim_prop_value" => $descriminating_prop_value,
//  ]);
//
//  //Update the tripal_entity table
//
//  $sql = "UPDATE public.tripal_entity AS TE SET bundle = :destination_bundle, term_id = :destination_term
//WHERE TE.id  IN (SELECT AB.entity_id AS entity_id FROM chado.analysis A
//INNER JOIN " . $chado_prop_table . " AP  ON A.analysis_id = AP.analysis_id
//INNER JOIN chado.cvterm CVT ON AP.type_id = CVT.cvterm_id
//INNER JOIN  " . $source_bundle_table . " AB ON A.analysis_id = AB.record_id
//WHERE CVT.cvterm_id = :cvterm_id
//AND AP.value = :descrim_prop_value )";
//
//  $params = [
//    ":cvterm_id" => $descriminating_type_cvterm_id,
//    ":descrim_prop_value" => $descriminating_prop_value,
//    ':destination_term' => $destination_term,
//    ":destination_bundle" => $destination_bundle->name,
//  ];
//
//  $results = db_query($sql, $params);
//
//  // Delete the old entities, but only if they are present in both the destination table
//  $sql = "DELETE  FROM " . $source_bundle_table . " AB
//WHERE AB.record_id IN
//(SELECT A.analysis_id AS record_id
//FROM chado.analysis A
//INNER JOIN " . $chado_prop_table . " AP  ON A.analysis_id = AP.analysis_id
//INNER JOIN chado.cvterm CVT ON AP.type_id = CVT.cvterm_id
//INNER JOIN " . $destination_table . " AS DT ON AB.record_id = DT.record_id
//WHERE CVT.cvterm_id = :cvterm_id
//AND AP.value = :descrim_prop_value)";
//
//  $results = db_query($sql, [
//    ":cvterm_id" => $descriminating_type_cvterm_id,
//    ":descrim_prop_value" => $descriminating_prop_value,
//  ]);
//}

function tripal_alchemist_convert_all_entities($args) {
  $source_bundle = $args['source_bundle'];
  $destination_bundle = $args['destination_bundle'];
  $descriminating_type_cvterm_id = $destination_bundle->type_id;
  $descriminating_prop_value = $destination_bundle->type_value;
  $type_column = $destination_bundle->type_column;
  $source_bundle_table = "public.chado_" . $source_bundle->name;
  $destination_term = $destination_bundle->term_id;
  $destination_table = "public.chado_" . $destination_bundle->name;
  $chado_base_table = "chado." . $source_bundle->data_table;
  $chado_prop_table = $chado_base_table . "prop";

  //TODO:  add any hard-coded exceptions here since we guess the primary record column
  $base_record_column = $source_bundle->data_table . "_id";

  if (!$source_bundle || !$destination_bundle) {
    tripal_set_message("Error: Destination and source bundle not provided.  Cannot convert entities without source and destination bundles.", TRIPAL_ERROR);
  }

  $type_linker_table = NULL;

  if (!$destination_bundle->type_linker_table && $destination_bundle->type_column) {
    //this is a type_id column situation, not a linker table situation


    //copy the entity over from source to destination

    $type_id = $destination_bundle->type_id;
    $query = db_select($chado_base_table, 'CBT');
    $query->fields('SET', ['record_id', 'entity_id']);
    $query->fields('CBT', ['name']);
    $query->innerJoin($source_bundle_table, 'SET', 'SET.record_id = CBT.' . $source_bundle->data_table . '_id');
    $query->leftJoin($destination_table, 'DET', 'SET.record_id = DET.record_id');
    $query->condition('CBT.' . $type_column, $type_id);
    $query->condition('DET.record_id', NULL, 'IS');
    $results = $query->execute()->fetchAll();

    //Update the tripal_entity table

    // Delete the old entities, but only if they are present in the destination table



  }

  //Need to use query builder instead
  //on conflict will ignore duplicate insert attempts from, say, migrating 2x and resulting in duplicate id's

  //TODO:  UPDATE THIS TO MODERN QUERY BUILDER, SEE FORM
  //copy the entity over from source to destination
  $sql = "INSERT INTO " . db_escape_table($destination_table) . " (entity_id, record_id)
          (SELECT AB.entity_id AS entity_id, AB.record_id AS record_id FROM " . db_escape_table($chado_base_table) . "  A
          INNER JOIN  " . db_escape_table($chado_prop_table) . " AP  ON A." . db_escape_field($base_record_column) . " = AP. " . db_escape_field($base_record_column) . "
          INNER JOIN chado.cvterm CVT ON AP." . db_escape_field($type_column) . " = CVT.cvterm_id
          INNER JOIN  " . db_escape_table($source_bundle_table) . " AB ON A." . ($base_record_column) . " = AB.record_id
          LEFT JOIN  " . db_escape_table($destination_table) . " DT ON A." . db_escape_field($base_record_column) . " = DT.record_id
          WHERE CVT.cvterm_id = :cvterm_id 
            AND AP.value = :descrim_prop_value 
            AND DT.record_id IS NULL)";

  //removed: ON CONFLICT DO NOTHING; for older versions of postgres


  $results = db_query($sql, [
    ":cvterm_id" => $descriminating_type_cvterm_id,
    ":descrim_prop_value" => $descriminating_prop_value,
  ]);

  //Update the tripal_entity table

  $sql = "UPDATE public.tripal_entity AS TE SET bundle = :destination_bundle, term_id = :destination_term
WHERE TE.id  IN (SELECT AB.entity_id AS entity_id FROM " . $chado_base_table . " A
INNER JOIN  " . $chado_prop_table . " AP  ON A." . db_escape_field($base_record_column) . " = AP. " . db_escape_field($base_record_column) . "
INNER JOIN chado.cvterm CVT ON AP." . db_escape_field($type_column) . " = CVT.cvterm_id
INNER JOIN  " . $source_bundle_table . " AB ON A." . db_escape_field($base_record_column) . " = AB.record_id
WHERE CVT.cvterm_id = :cvterm_id
AND AP.value = :descrim_prop_value )";

  $params = [
    ":cvterm_id" => $descriminating_type_cvterm_id,
    ":descrim_prop_value" => $descriminating_prop_value,
    ':destination_term' => $destination_term,
    ":destination_bundle" => $destination_bundle->name,
  ];

  $results = db_query($sql, $params);

  // Delete the old entities, but only if they are present in the destination table
  $sql = "DELETE FROM " . $source_bundle_table . " AB
WHERE AB.record_id IN
(SELECT A.analysis_id AS record_id
FROM " . $chado_base_table . " A
INNER JOIN " . $chado_prop_table . " AP ON  A." . db_escape_field($base_record_column) . " = AP. " . db_escape_field($base_record_column) . "
INNER JOIN chado.cvterm CVT ON AP.type_id = CVT.cvterm_id
INNER JOIN " . $destination_table . " AS DT ON AB.record_id = DT.record_id
WHERE CVT.cvterm_id = :cvterm_id
AND AP.value = :descrim_prop_value)";

  $results = db_query($sql, [
    ":cvterm_id" => $descriminating_type_cvterm_id,
    ":descrim_prop_value" => $descriminating_prop_value,
  ]);
}


function tripal_alchemist_convert_select_entities($args) {

  $source_bundle = $args['source_bundle'];
  $destination_bundle = $args['destination_bundle'];
  $source_entities = $args['source_entities'];


  if (!$source_bundle || !$destination_bundle) {
    tripal_set_message("Error: Destination and source bundle not provided.  Cannot convert entities without source and destination bundles.", TRIPAL_ERROR);
  }
  if ($source_bundle->data_table != $destination_bundle->data_table) {
    tripal_set_message("Error: destination and source bundles have the same base table.", TRIPAL_ERROR);
  }

  //get all our source and destination table info
  $descriminating_type_cvterm_id = $destination_bundle->type_id;
  $descriminating_prop_value = $destination_bundle->type_value;
  $type_column = $destination_bundle->type_column;
  $source_bundle_table = "public.chado_" . $source_bundle->name;
  $destination_term = $destination_bundle->term_id;
  $destination_table = "public.chado_" . $destination_bundle->name;
  $chado_base_table = "chado." . $source_bundle->data_table;
  $chado_prop_table = $chado_base_table . "prop";

  //TODO:  add any hard-coded exceptions here since we guess the primary record column
  $base_record_column = $source_bundle->data_table . "_id";


  //First, set the property in Chado for each selected record.

  $entity_to_record = [];

  foreach ($source_entities as $entity_id) {
    $chado_entity_table = chado_get_bundle_entity_table($source_bundle);
    $record_id = db_select($chado_entity_table, 'CE')
      ->fields('CE', ['record_id'])
      ->condition('CE.entity_id', $entity_id)
      ->execute()
      ->fetchField();

    if (!$record_id) {
      tripal_report_error("Could not retrieve record id for entity $entity_id.  Aborting.", TRIPAL_ERROR);
    }

    $record = ['table' => $source_bundle->data_table, 'id' => $record_id];
    $property = [
      'type_id' => $descriminating_type_cvterm_id,
      'value' => $descriminating_prop_value,
    ];

    $options = ['update_if_present' => TRUE];

    if (!chado_insert_property($record, $property, $options)) {
      tripal_report_error("Could not update property for entity $entity_id.  Aborting.", TRIPAL_ERROR);
      return;
    }

    //insert into the destination table
    //Ignore duplicates
    try {
      $result = db_insert($destination_table)
        ->fields(['entity_id' => $entity_id, 'record_id' => $record_id])
        ->execute();
    } catch (Exception $e) {
      // ignored
    }

    //update the tripal entity table

    $result = db_update("public.tripal_entity")
      ->fields([
        'bundle' => $destination_bundle->name,
        'term_id' => $destination_term,
      ])
      ->condition('id', $entity_id, '=')
      ->execute();

    //delete from source
    //TODO: CHECK THAT IT WAS SUCCESFUL BEFORE DELETING
    $result = db_delete($source_bundle_table)
      ->condition('entity_id', $entity_id)
      ->condition('record_id', $record_id)
      ->execute();
  }

}


/**
 * Given a bundle object, count the number of entries for that bundle.
 *
 * @param $bundle
 *
 * @return mixed
 */

function tripal_alchemist_fetch_current_bundle_entities($bundle) {

  $sql = "SELECT COUNT(*) FROM public.chado_" . $bundle->name;
  $results = db_query($sql)->fetchObject();
  return ($results->count);
}


//function tripal_alchemist_fetch_elligible_bundle_entities() {
//
//}

function function_tripal_alchemist_get_bundle($bundle_id) {


  $query = "SELECT TB.name, TB.label FROM public.tripal_bundle TB WHERE TB.id = :bundle_id";
  $result = db_query($query, [':bundle_id' => $bundle_id])->fetchObject();
  return $result;
}

function tripal_alchemist_get_all_entities_from_bundle($bundle) {

  $base_table = $bundle->table;
  $tb_table = "chado_" . $bundle->name;

  $query = db_select($tb_table, 'tb');

}