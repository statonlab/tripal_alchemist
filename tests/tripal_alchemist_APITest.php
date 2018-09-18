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

  public function test_alchemist_build_transaction_table() {

    //by default, we have genes and mRNA.  let's convert from one to the other.

    $gene_term = chado_get_cvterm(['id' => 'SO:0000704']);
    $mrna_term = chado_get_cvterm(['id' => 'SO:0000234']);

    $gene = factory('chado.feature')->create(['type_id' => $gene_term->cvterm_id]);
    $mrna = factory('chado.feature')->create(['type_id' => $mrna_term->cvterm_id]);
    $this->publish('feature');

    $mrna_bundle = tripal_load_bundle_entity(['accession' => 'SO:0000234']);
    $gene_bundle = tripal_load_bundle_entity(['accession' => 'SO:0000704']);


    $match = ['feature_id' => $gene->feature_id];
    $uvalues = ['type_id' => $mrna_term->cvterm_id];

    $updated = chado_update_record('feature', $match, $uvalues);

    //    tripal_alchemist_run_conversion_job();


    $table = tripal_alchemist_build_transaction_table($gene_bundle, $mrna_bundle);

    $this->assertGreaterThan(0, $table['count']);
    $table = $table['table'];

    $this->assertNotEmpty($table);
    $this->assertArrayHasKey('header', $table);
    $this->assertArrayHasKey('rows', $table);
    $this->assertNotEmpty($table['rows']);
  }


}
