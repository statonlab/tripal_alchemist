Tripal Alchemist allows you to **transmute** (convert) entities from one type to another.

![Tripal Alchemist Hearthstone Card](docs/img/Tripal_Alchemist_hearthstone_logo.png)

## Background 

Tripal 3 provides migrations for most base Chado content types.  Some content types (namely Analysis) convert all nodes to a single bundle type.  This is not great if you make heavy use of submodules that define their own node type: in the case of analysis, this includes analysis_expression, analysis_unigene, etc.  You also might decide later down the road that you want to redefine some of your `mrna` features as `mrna_contig`, for example.

In both cases, Tripal Alchemist provides a simple interface to easily convert entities from one bundle to another, provided that the destination bundle exists, has the same base table as the source bundle, and the destination bundle was defined with the `type column` and `property value` properly set.

This module is under active development, and is released as v0.1.

## Features

* Transform Tripal entities from one type to another based on their existing bundle-defining property
* Manually override entity properties and transform select elements
* **Coming Soon** Use existing properties to transform entities that are not set for the destination bundle based on the bundle-defining property
* **Coming Soon** Transform entities based on their `type_id`.


## Usage
**Notice:** The alchemist interface has changed: the below guide is accurate for **Automatic** conversion.  **Manual** conversion is a new feature which allows you to manually select which entities to convert, regardless of their existing properties.  The guide will be updated to reflect this for the v0.2 release.


* Define a destination bundle that is the same base table as your source bundle.  [You can follow this guide to learn how to define new bundles](docs/defining_a_new_bundle.md).  
* Navigate to the transmuter, located at `/admin/tripal/extension/tripal_alchemist`.
* Select a source bundle.  This is the current content type, and qualifying entities from this type will be transformed.
* Select a destination bundle.  This is what you want to turn your content into.
* Run **Transmute**.
* You're done!

>![The Tripal Alchemist Transmutation form](docs/img/tripal_alchemist_screen_1.png)
> The Tripal Alchemist Transmutation form.  If any entities qualify for your new bundle type from the selected bundle type, they will appear in the table at the bottom of the form.

## License and Contributing

Tripal Alchemist is open source and provided under the [GPL-3.0 license](https://github.com/statonlab/tripal_alchemist/blob/master/LICENSE).  It was created by Bradford Condon and Meg Staton from the University of Tennessee Knoxville.  Tripal Alchemist doesn't do everything I wish it did.  If you have feature requests, bug reports, or contributions, please head to the issues queue and create an issue.  If you would like to make a contribution, simply fork the repo and make a pull request from there.

The Tripal Alchemist "logo" is derived from the collectible card game Hearthstone, copyright © Blizzard Entertainment, Inc. Hearthstone® is a registered trademark of Blizzard Entertainment, Inc. Tripal Alchemist is not affiliated or associated with or endorsed by Hearthstone® or Blizzard Entertainment, Inc.
