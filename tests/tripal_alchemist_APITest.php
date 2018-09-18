<?php

namespace Tests;

use StatonLab\TripalTestSuite\DBTransaction;
use StatonLab\TripalTestSuite\TripalTestCase;


class tripal_alchemist_APITest extends TripalTestCase {

  use DBTransaction;

  public function test_alchemist_get_all_bundles() {
    $bundles = tripal_alchemist_get_all_bundles();
    $this->assertInternalType('array', $bundles);
    $this->assertNotEmpty($bundles);
  }

  public function testTripal_alchemist_get_matching_bundles() {

    $bundles = tripal_alchemist_get_matching_bundles("organism");
    $this->assertInternalType('array', $bundles);
    $entry = array_pop($bundles);
    $this->assertEquals('Organism', $entry);

  }

  /**
   * Creates an mRNA and a gene, and publish.  Then update the type_id of the
   * gene so that it is mRNA, qualifying it for alchemist automatic conversion.
   */
  private function create_automatic_entities() {

    $gene_term = chado_get_cvterm(['id' => 'SO:0000704']);
    $mrna_term = chado_get_cvterm(['id' => 'SO:0000234']);

    $gene = factory('chado.feature')->create(['type_id' => $gene_term->cvterm_id]);
    $mrna = factory('chado.feature')->create(['type_id' => $mrna_term->cvterm_id]);
    $this->publish('feature');

    $gene_bundle = tripal_load_bundle_entity(['accession' => 'SO:0000704']);
    $mrna_bundle = tripal_load_bundle_entity(['accession' => 'SO:0000234']);

    $match = ['feature_id' => $gene->feature_id];
    $uvalues = ['type_id' => $mrna_term->cvterm_id];

    $updated = chado_update_record('feature', $match, $uvalues);

    $args = [
      'gene' => $gene,
      'mrna' => $mrna,
    ];

    return $args;
  }

  /**
   * @group api
   * @group form
   */
  public function test_alchemist_build_transaction_table() {

    $this->create_automatic_entities();
    $gene_bundle = tripal_load_bundle_entity(['accession' => 'SO:0000704']);
    $mrna_bundle = tripal_load_bundle_entity(['accession' => 'SO:0000234']);

    $table = tripal_alchemist_build_transaction_table($gene_bundle, $mrna_bundle);

    $this->assertGreaterThan(0, $table['count']);
    $table = $table['table'];

    $this->assertNotEmpty($table);
    $this->assertArrayHasKey('header', $table);
    $this->assertArrayHasKey('rows', $table);
    $this->assertNotEmpty($table['rows']);
  }


  public function test_tripal_alchemist_convert_all() {

    $args = $this->create_automatic_entities();

    $gene = $args['gene'];//type: 496
    $mrna = $args['mrna'];//type: 422
    $gene_bundle = tripal_load_bundle_entity(['accession' => 'SO:0000704']);
    $mrna_bundle = tripal_load_bundle_entity(['accession' => 'SO:0000234']);


    $source = $gene_bundle;
    $destination = $mrna_bundle;
    $trigger = FALSE;

    tripal_alchemist_convert_all($source, $destination, $trigger);

    $result = db_select('chado.feature', 'f')
      ->fields('f', ['type_id'])
      ->condition('feature_id', $gene->feature_id)
      ->execute()
      ->fetchObject();

    $this->assertNotFalse($result);
    //Confirm the type_id changed in Chado.
    $this->assertEquals($mrna->type_id, $result->type_id);


    $record = chado_get_record_entity_by_bundle($mrna_bundle, $gene->feature_id);
    $this->assertNotFalse($record);

    $record = chado_get_record_entity_by_bundle($gene_bundle, $gene->feature_id);
    $this->assertFalse($record);
  }


  /**
   *
   * @group wip
   */
  public function test_tripal_alchemist_convert_select_entities_collection() {

    $args = $this->create_automatic_entities();

    $gene = $args['gene'];//type: 496
    $mrna = $args['mrna'];//type: 422
    $gene_bundle = tripal_load_bundle_entity(['accession' => 'SO:0000704']);
    $mrna_bundle = tripal_load_bundle_entity(['accession' => 'SO:0000234']);


    //get entity_id of the gene to convert

    $eid = chado_get_record_entity_by_table('feature', $gene->feature_id);


    $this->actingAs(1);

    global $user;

    $fields = field_info_instances('TripalEntity', $gene_bundle->name);

    $field_ids = [];

    foreach ($fields as $field) {
      $field_ids[] = $field['id'];
    }
    $collection_details = [
      'uid' => $user->uid,
      'collection_name' => 'alchemist_test_collection_PHPUNIT',
      'bundle_name' => $gene_bundle->name,
      'ids' => [$eid],
      'description' => NULL,
      'fields' => $field_ids,
    ];


    module_load_include('tripal', 'inc', 'includes/TripalFieldDownloaders/TripalTabDownloader');

    module_load_include('tripal', 'inc', 'includes/TripalFieldDownloaders/TripalCSVDownloader');


    $tec = tripal_create_collection($collection_details);

    $source = $gene_bundle;
    $destination = $mrna_bundle;
    $source_entities = [];
    //  $collection = $tec_id;


    tripal_alchemist_convert_select_entities($source, $destination, $source_entities, $tec);


    $result = db_select('chado.feature', 'f')
      ->fields('f', ['type_id'])
      ->condition('feature_id', $gene->feature_id)
      ->execute()
      ->fetchObject();

    $this->assertNotFalse($result);
    //Confirm the type_id changed in Chado.
    $this->assertEquals($mrna->type_id, $result->type_id);

    $record = chado_get_record_entity_by_bundle($mrna_bundle, $gene->feature_id);
    $this->assertNotFalse($record);

    $record = chado_get_record_entity_by_bundle($gene_bundle, $gene->feature_id);
    $this->assertFalse($record);

  }

}
