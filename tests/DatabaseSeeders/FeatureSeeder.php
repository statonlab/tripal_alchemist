<?php

namespace Tests\DatabaseSeeders;

use StatonLab\TripalTestSuite\Database\Seeder;

class FeatureSeeder extends Seeder
{
    /**
     * Seeds the database with users.
     */
    public function up()
    {
      $mrna_term = chado_get_cvterm(['id' => 'SO:0000234']);
      
      factory('chado.feature', 1000000)->create(['type_id' => $mrna_term->cvterm_id]);
    }
}
