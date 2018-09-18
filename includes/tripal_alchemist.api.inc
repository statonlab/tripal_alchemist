<?php
/**
 * API methods for Tripal Alchemist.  Some of these are module specific, others
 * are simple helper methods for Tripal Collections or Entities.
 */

/**
 *   Returns an array of all bundle objects.
 *
 * @return array|bool
 * The returned bundles array is composed of Tripal Bundle objects.
 *
 * @ingroup General entities
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
      tripal_report_error('tripal', TRIPAL_ERROR,
        "Unrecognized bundle name '%bundle'.", ['%bundle' => $bundle_name]);

      return FALSE;
    }

    array_push($bundles, $bundle);
  }

  return $bundles;
}

/**
 * Get bundles that map to a particular source table.
 *
 * @param $source_table
 *
 * @return array
 *
 * @ingroup General_entities
 */

function tripal_alchemist_get_matching_bundles($source_table) {
  $options = [];
  $all_bundles_object = tripal_alchemist_get_all_bundles();
  foreach ($all_bundles_object as $bundle) {
    if ($bundle->data_table == $source_table) {
      $options[$bundle->id] = $bundle->label;
    }
  }

  return ($options);
}

/**
 * Dispatch the job.
 *
 * @param $item
 *
 * @throws \Exception
 */
function tripal_alchemist_run_queue($item) {
  tripal_alchemist_run_conversion_job($item->type, $item->args);
}

/**
 * The job call.
 *
 * @param $type
 * @param $args
 *
 * @throws \Exception
 */
function tripal_alchemist_run_conversion_job($type, $args) {
  $source_bundle_id = $args['source_bundle_id'];
  $destination_bundle_id = $args['destination_bundle_id'];
  $source_bundle = tripal_load_bundle_entity(['id' => $source_bundle_id]);
  $destination_bundle = tripal_load_bundle_entity(['id' => $destination_bundle_id]);

  $source_entities = isset($args["source_entities"]) ? $args["source_entities"] : NULL;
  $collection_id = isset($args["collection_id"]) ? $args["collection_id"] : NULL;

  switch ($type) {
    case "Manual":
      tripal_alchemist_convert_select_entities($source_bundle,
        $destination_bundle, $source_entities, NULL);
      break;

    case "Collections" :
      $collection = tripal_get_collection(["collection_id" => $collection_id]);
      tripal_alchemist_convert_select_entities($source_bundle,
        $destination_bundle, NULL, $collection);
      break;

    case "Automatic":
      tripal_alchemist_dispatch($source_bundle, $destination_bundle,
        $args['trigger_hooks']);
      break;
  }
}

function tripal_alchemist_dispatch(
  $source_bundle,
  $destination_bundle,
  $trigger_hooks
) {
  if (!$source_bundle || !$destination_bundle) {
    tripal_set_message("Error: Destination and source bundle not provided.  Cannot convert entities without source and destination bundles.",
      TRIPAL_ERROR);
  }

  $type_linker_table = $destination_bundle->type_linker_table;

  if (!$type_linker_table && $destination_bundle->type_column) {
    tripal_alchemist_convert_all($source_bundle, $destination_bundle,
      $trigger_hooks);
  }
  else {
    tripal_alchemist_convert_by_prop($source_bundle, $destination_bundle);
  }
}

function tripal_alchemist_convert_all(
  $source_bundle,
  $destination_bundle,
  $trigger_hooks = TRUE
) {
  $source_bundle_table = "public.chado_" . $source_bundle->name;
  $destination_term = $destination_bundle->term_id;
  $destination_table = "public.chado_" . $destination_bundle->name;
  $chado_base_table = "chado." . $source_bundle->data_table;

  $type_id = $destination_bundle->type_id;
  $type_column = $destination_bundle->type_column;

  $query = db_select($chado_base_table, 'CBT');
  $query->fields('SET', ['entity_id', 'record_id']);
  $query->innerJoin($source_bundle_table, 'SET',
    'SET.record_id = CBT.' . $source_bundle->data_table . '_id');
  $query->leftJoin($destination_table, 'DET', 'SET.record_id = DET.record_id');
  $query->condition('CBT.' . $type_column, $type_id);
  $query->condition('DET.record_id', NULL, 'IS');

  $total = (int) $query->countQuery()->execute()->fetchField();
  $results = $query->execute();
  $completed = 0;

  print "Starting alchemist job. There are $total entities to convert\n";

  while ($result = $results->fetchObject()) {
    $record_id = $result->record_id;
    $entity_id = $result->entity_id;

    if (!$record_id) {
      tripal_report_error("Could not retrieve record id for entity $entity_id.  Aborting.",
        TRIPAL_ERROR);
    }

    // Insert into the destination table
    // TODO Ignore duplicates
    try {
      $result = db_insert($destination_table)
        ->fields(['entity_id' => $entity_id, 'record_id' => $record_id])
        ->execute();
    } catch (Exception $e) {
    }

    //update the tripal entity table
    $result = db_update("public.tripal_entity")->fields([
      'bundle' => $destination_bundle->name,
      'term_id' => $destination_term,
    ])->condition('id', $entity_id, '=')->execute();

    // Delete the old entities, but only if they are present in the destination table
    //delete from source
    //TODO: CHECK THAT IT WAS SUCCESSFUL BEFORE DELETING
    db_delete($source_bundle_table)
      ->condition('entity_id', $entity_id)
      ->condition('record_id', $record_id)
      ->execute();

    // Trigger entity update hook
    if ($trigger_hooks) {
      $entities = tripal_load_entity('TripalEntity', [$entity_id], TRUE, [],
        FALSE);
      $entity = reset($entities);
      module_invoke_all('entity_update', $entity, $entity->type);
    }

    $completed++;

    if (($completed % 500) === 0) {
      print "Completed $completed / $total\n";
    }
  }
}

/**
 * @param $source_bundle
 * @param $destination_bundle
 */
function tripal_alchemist_convert_by_prop($source_bundle, $destination_bundle) {
  $source_bundle_table = "public.chado_" . $source_bundle->name;
  $destination_term = $destination_bundle->term_id;
  $destination_table = "public.chado_" . $destination_bundle->name;
  $chado_base_table = "chado." . $source_bundle->data_table;
  $chado_prop_table = $chado_base_table . "prop";
  $descriminating_prop_value = $destination_bundle->type_value;
  $descriminating_type_cvterm_id = $destination_bundle->type_id;
  $type_column = $destination_bundle->type_column;

  // TODO: add any hard-coded exceptions here since we guess the primary record column
  $base_record_column = $source_bundle->data_table . "_id";

  // Setting a type_id column in a linker table.

  // Need to use query builder instead
  // on conflict will ignore duplicate insert attempts from, say, migrating 2x
  // and resulting in duplicate id's

  // TODO:  UPDATE THIS TO MODERN QUERY BUILDER
  // copy the entity over from source to destination
  $sql = "INSERT INTO " . db_escape_table($destination_table) . " (entity_id, record_id)
          (SELECT AB.entity_id AS entity_id, AB.record_id AS record_id FROM " . db_escape_table($chado_base_table) . "  A
          INNER JOIN  " . db_escape_table($chado_prop_table) . " AP  ON A." . db_escape_field($base_record_column) . " = AP. " . db_escape_field($base_record_column) . "
          INNER JOIN chado.cvterm CVT ON AP." . db_escape_field($type_column) . " = CVT.cvterm_id
          INNER JOIN  " . db_escape_table($source_bundle_table) . " AB ON A." . ($base_record_column) . " = AB.record_id
          LEFT JOIN  " . db_escape_table($destination_table) . " DT ON A." . db_escape_field($base_record_column) . " = DT.record_id
          WHERE CVT.cvterm_id = :cvterm_id 
            AND AP.value = :descrim_prop_value 
            AND DT.record_id IS NULL)";

  // TODO: THIS WILL FAIL IF TOO MANY ENTITIES
  $results = db_query($sql, [
    ":cvterm_id" => $descriminating_type_cvterm_id,
    ":descrim_prop_value" => $descriminating_prop_value,
  ]);

  // Update the tripal_entity table
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

/**
 * Converts a select list of entities.
 *
 * This list of entities is retrieved either as a list of entity ids, or via a
 * collection, depending on the args passed in.
 *
 * @param $source_bundle
 * @param $destination_bundle
 * @param $source_entities
 * @param $collection
 *
 * @ingroup Convert_entities
 */
function tripal_alchemist_convert_select_entities(
  $source_bundle,
  $destination_bundle,
  $source_entities = NULL,
  $collection = NULL
) {

  if (!$source_bundle || !$destination_bundle) {
    print("Error: Destination and source bundle not provided.  Cannot convert entities without source and destination bundles.\n");

    return;
  }
  if ($source_bundle->data_table != $destination_bundle->data_table) {
    print("Error: destination and source bundles do not have the same base table.\n");

    return;
  }
  if ($collection) {
    //get entities
    $source_entities = tripal_alchemist_get_eligible_entities($collection,
      $source_bundle);
  }

  if (!$source_entities) {
    print("\nError: No qualifying source entities.\n");

    return;
  }

  $total_count = count($source_entities);

  print("\n Preparing to convert " . $total_count . " entities from " . $source_bundle->label . "  to the bundle: " . $destination_bundle->label . "\n");
  //get all our source and destination table info
  $descriminating_type_cvterm_id = $destination_bundle->type_id;
  $descriminating_prop_value = $destination_bundle->type_value;
  $type_column = $destination_bundle->type_column;
  $source_bundle_table = "public.chado_" . $source_bundle->name;
  $destination_term = $destination_bundle->term_id;
  $destination_table = "public.chado_" . $destination_bundle->name;
  $chado_base_table = "chado." . $source_bundle->data_table;
  $chado_prop_table = $chado_base_table . "prop";

  // TODO:  add any hard-coded exceptions here since we guess the primary record column
  $base_record_column = $source_bundle->data_table . "_id";
  $linker_type = "table";

  if (!$destination_bundle->type_linker_table && $destination_bundle->type_column) {
    $linker_type = "column";
  }
  // First, set the property in Chado for each selected record.

  $chado_entity_table = chado_get_bundle_entity_table($source_bundle);

  $count = 0;

  foreach ($source_entities as $entity_id) {

    if ($count % 1000 == 0) {
      print ("Converting entity" . $count . " of " . $total_count . "\n");
    }

    $record_id = db_select($chado_entity_table, 'CE')
      ->fields('CE', ['record_id'])
      ->condition('CE.entity_id', $entity_id)
      ->execute()
      ->fetchField();

    if (!$record_id) {
      tripal_report_error("tripal_alchemist", TRIPAL_ERROR,
        "Could not retrieve record id for entity" . (string) $entity_id . ".  Aborting.");

      return;
    }

    // First update property
    if ($linker_type === "column") {
      // Type column.
      $result = db_update($chado_base_table)
        ->fields([
          $type_column => $descriminating_type_cvterm_id,
        ])
        ->condition($source_bundle->data_table . '_id', $record_id, '=')
        ->execute();
    }
    else {
      // Linker column
      $record = ['table' => $source_bundle->data_table, 'id' => $record_id];
      $property = [
        $type_column => $descriminating_type_cvterm_id,
        'value' => $descriminating_prop_value,
      ];

      $options = ['update_if_present' => TRUE];
      if (!chado_insert_property($record, $property, $options)) {
        tripal_report_error("Tripal_alchemist", TRIPAL_ERROR,
          "Could not update property for entity $entity_id.  Aborting.");

        return;
      }
    }
    // Insert into the destination table
    try {
      $result = db_insert($destination_table)
        ->fields(['entity_id' => $entity_id, 'record_id' => $record_id])
        ->execute();
    } catch (Exception $e) {
    }

    // Update the tripal entity table
    $result = db_update("public.tripal_entity")->fields([
      'bundle' => $destination_bundle->name,
      'term_id' => $destination_term,
    ])->condition('id', $entity_id, '=')->execute();

    // Delete from source
    // TODO: CHECK THAT IT WAS SUCCESSFUL BEFORE DELETING
    $result = db_delete($source_bundle_table)
      ->condition('entity_id', $entity_id)
      ->condition('record_id', $record_id)
      ->execute();

    $count++;
  }
}

/**
 * Given a bundle object, count the number of entries for that bundle.
 *
 * @param $bundle
 *
 * @return mixed
 *
 * @ingroup General_entities
 */

function tripal_alchemist_fetch_current_bundle_entities($bundle) {

  $sql = "SELECT COUNT(*) FROM public.chado_" . $bundle->name;
  $results = db_query($sql)->fetchObject();

  return ($results->count);
}

/**
 * Given the bundle ID, fetch the bundle name and label
 *
 * @param $bundle_id
 *
 * @return mixed
 * @ingroup General_entities
 */
function function_tripal_alchemist_get_bundle($bundle_id) {


  $query = "SELECT TB.name, TB.label FROM public.tripal_bundle TB WHERE TB.id = :bundle_id";
  $result = db_query($query, [':bundle_id' => $bundle_id])->fetchObject();

  return $result;
}

/**
 * Given a collection object, return all entities in the collection from the
 * source bundle.
 *
 * @param $collection
 * @param $source_bundle
 *
 * @return array
 *
 * @ingroup Convert_entities
 */

function tripal_alchemist_get_eligible_entities($collection, $source_bundle) {
  $entities = $collection->getEntityIDs($source_bundle->name);

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

  return $matching_entities;
}

/**
 * Job callback to perform deletion of orphaned entities.
 *
 * @param int $bundle_id
 *
 * @throws \Exception
 */
function tripal_alchemist_delete_orphaned_entities($bundle_id) {
  $bundle = db_query('SELECT bundle_id, data_table 
             FROM chado_bundle CB WHERE bundle_id = :id',
    [':id' => $bundle_id])->fetchObject();

  $bundle_table = db_escape_table("chado_bio_data_{$bundle->bundle_id}");
  $chado_table = db_escape_table($bundle->data_table);
  $schema = chado_get_schema($chado_table);
  $primary_key = is_array($schema) ? $schema['primary key'][0] : NULL;

  $count = (int) db_query('SELECT count(*) FROM ' . $bundle_table . ' BT
                        LEFT JOIN chado.' . $chado_table . ' CT ON BT.record_id = CT.' . $primary_key . '
                        WHERE CT.' . $primary_key . ' IS NULL')->fetchField();

  if ($count === 0) {
    print "Unable to find any orphaned entities.\n";
    return;
  }

  print "Found $count orphaned entities.\n";

  $chunk = 500;
  $progress = 0;
  print "Progress: {$progress}%; Memory: " . number_format(memory_get_usage()) . " bytes\r";

  for ($i = 0; $i < $count; $i += $chunk) {
    // Get a chunk of orphaned entities
    $entities = db_query('SELECT entity_id FROM ' . $bundle_table . ' BT
                          LEFT JOIN chado.' . $chado_table . ' CT ON BT.record_id = CT.' . $primary_key . '
                          WHERE CT.' . $primary_key . ' IS NULL 
                          ORDER BY entity_id desc
                          LIMIT :limit', [
      ':limit' => $chunk,
    ])->fetchAll();

    // Extract entity ids
    $ids = array_map(function ($entity) {
      return (int) $entity->entity_id;
    }, $entities);

    // Tell the user that selected entities are not present
    if (empty($ids)) {
      tripal_report_error('tripal_alchemist', TRIPAL_ERROR,
        'Unable to find selected entities while attempting to delete orphaned entities.');
      continue;
    }

    // This will trigger all needed hooks
    /** @var \TripalEntityController $controller */
    ob_start(function($buffer) {
      unset($buffer);
    });
    putenv('TRIPAL_SUPPRESS_ERRORS=true');
    $controller = entity_get_controller('TripalEntity');
    if ($controller->delete($ids) === FALSE) {
      print "\nFailed to delete chunk {$i}/{$count}!\n";
    }
    $controller->resetCache($ids);
    putenv('TRIPAL_SUPPRESS_ERRORS=false');
    ob_end_clean();

    // TODO: this is a temporary solution until the following issue is resolved
    // TODO: https://github.com/tripal/tripal/issues/652
    db_query('DELETE FROM chado_bio_data_' . $bundle_id . ' 
              WHERE entity_id IN (:ids)', [':ids' => $ids]);

    // Report progress
    $progress = number_format(($i + $chunk) / $count * 100, 2);
    print "Progress: {$progress}%; Memory: " . number_format(memory_get_usage()) . " bytes\r";
  }

  print "Progress: 100%; Memory: " . number_format(memory_get_usage()) . " bytes\n";
  print "Done.\n";
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
function tripal_alchemist_build_transaction_table(
  $source_bundle,
  $destination_bundle
) {

  $result_count = 0;
  $result_count = 0;

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
    $query->innerJoin($source_entity_table, 'SET',
      'SET.record_id = CBT.' . $source_bundle->data_table . '_id');
    $query->leftJoin($destination_entity_table, 'DET',
      'SET.record_id = DET.record_id');
    $query->condition('CBT.' . $type_column, $type_id);
    $query->condition('DET.record_id', NULL, 'IS');
    $count_query = $query;

    $result_count = $count_query->countQuery()->execute()->fetchField();

    $results = $query->range(0, 10)->execute()->fetchAll();
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
    $query->innerJoin($source_entity_table, 'SET',
      'SET.record_id = CBT.' . $source_bundle->data_table . '_id');
    $query->innerJoin($type_linker_table, 'CLT',
      'CBT.' . $source_bundle->data_table . '_id = CLT.' . $source_bundle->data_table . '_id');
    $query->leftJoin($destination_entity_table, 'DET',
      'SET.record_id = DET.record_id');
    $query->condition('CLT.' . $type_column, $type_id);
    $query->condition('CLT.value', $type_value);
    $query->condition('DET.record_id', NULL, 'IS');
    $count_query = $query;

    $result_count = $count_query->countQuery()->execute()->fetchField();

    $results = $query->range(0, 10)->execute()->fetchAll();
  }
  else {
    tripal_set_message("destination bundle is not properly set.", TRIPAL_ERROR);
    return ['table' => NULL, 'count' => $result_count];
  }

  $rows = [];

  $overflow = FALSE;

  if ($result_count === 0) {
    return ['table' => NULL, 'count' => $result_count];
  }

  foreach ($results as $result) {
    $rows[] = [
      $result->record_id,
      $result->entity_id,
      $result->name,
    ];
  }

  $headers = ['Record ID', 'Entity ID', 'Name'];

  $table = [
    'header' => $headers,
    'rows' => $rows,
  ];

  return ['table' => $table, 'count' => $result_count];
}
