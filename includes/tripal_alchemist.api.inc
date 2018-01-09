<?php

function tripal_alchemist_check_base_tables($source_bundle, $destination_bundle){


}


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