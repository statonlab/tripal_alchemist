<?php

namespace Tests;

use StatonLab\TripalTestSuite\DBTransaction;
use StatonLab\TripalTestSuite\TripalTestCase;

class tripal_alchemist_APITest extends TripalTestCase {
  // Uncomment to auto start and rollback db transactions per test method.
   use DBTransaction;

  public function testHasBundles() {
    $bundles = tripal_alchemist_get_all_bundles();
    $this->assertInternalType('array', $bundles);
  }

public function testTripal_alchemist_get_matching_bundles() {

    $bundles = tripal_alchemist_get_matching_bundles("organism");
    $this->assertInternalType('array', $bundles);


 
}
}
