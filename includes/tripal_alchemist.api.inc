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
      //TODO Count how many elligible entities here
      $options[$bundle->id] = $bundle->label;
    }
  }
  return ($options);
}


/**Converts entities from one type to another based on the prop linker table
 *
 * @param $source_bundle
 * @param $destination_bundle
 * @param $descriminating_cvterm
 * @param $descriminating_prop_value
 */


function tripal_alchemist_convert_entity($source_bundle, $destination_bundle, $descriminating_cvterm, $descriminating_prop_value) {


  $descriminating_type_cvterm_id = $descriminating_cvterm->cvterm_id;

  $source_bundle_table = "public.chado_" . $source_bundle->name;

  $destination_term = $destination_bundle->term_id;
  $destination_table = "public.chado_" . $destination_bundle->name;

  print ("\nconverting " . $source_bundle_table . " to " . $destination_table . "\n");

  $chado_base_table = "chado." . $source_bundle->data_table;
  $chado_prop_table = $chado_base_table . "prop";

  //  Copy the plain analysis entities to the new bundle type

  $sql = "INSERT INTO " . $destination_table . " (entity_id, record_id)
SELECT AB.entity_id AS entity_id, A.analysis_id AS record_id FROM " . $chado_base_table . "  A
INNER JOIN  " . $chado_prop_table . " AP  ON A.analysis_id = AP.analysis_id
INNER JOIN chado.cvterm CVT ON AP.type_id = CVT.cvterm_id
INNER JOIN  " . $source_bundle_table . " AB ON A.analysis_id = AB.record_id
WHERE CVT.cvterm_id = :cvterm_id
AND AP.value = :descrim_prop_value";

  $results = db_query($sql, [
    ":cvterm_id" => $descriminating_type_cvterm_id,
    ":descrim_prop_value" => $descriminating_prop_value,
  ]);

  //Update the tripal_entity table

  $sql = "UPDATE public.tripal_entity AS TE SET bundle = :destination_bundle, term_id = :destination_term
WHERE TE.id  IN (SELECT AB.entity_id AS entity_id FROM chado.analysis A
INNER JOIN " . $chado_prop_table . " AP  ON A.analysis_id = AP.analysis_id
INNER JOIN chado.cvterm CVT ON AP.type_id = CVT.cvterm_id
INNER JOIN  " . $source_bundle_table . " AB ON A.analysis_id = AB.record_id
WHERE CVT.cvterm_id = :cvterm_id
AND AP.value = :descrim_prop_value )";

  $params = [
    ":cvterm_id" => $descriminating_type_cvterm_id,
    ":descrim_prop_value" => $descriminating_prop_value,
    ':destination_term' => $destination_term,
    ":destination_bundle" => $destination_bundle->name,
  ];

  $results = db_query($sql, $params);

  // Delete the old entities, but only if they are present in both the destination table
  $sql = "DELETE  FROM " . $source_bundle_table . " AB
WHERE AB.record_id IN
(SELECT A.analysis_id AS record_id
FROM chado.analysis A
INNER JOIN " . $chado_prop_table . " AP  ON A.analysis_id = AP.analysis_id
INNER JOIN chado.cvterm CVT ON AP.type_id = CVT.cvterm_id
INNER JOIN " . $destination_table . " AS DT ON AB.record_id = DT.record_id
WHERE CVT.cvterm_id = :cvterm_id
AND AP.value = :descrim_prop_value)";

  $results = db_query($sql, [
    ":cvterm_id" => $descriminating_type_cvterm_id,
    ":descrim_prop_value" => $descriminating_prop_value,
  ]);
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


function tripal_alchemist_fetch_elligible_bundle_entities() {

}