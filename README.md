Tripal alchemist allows you to **transmute** (convert) entities from one type to another.

Tripal 3 provides migrations for most base content types.  Some content types (namely Analysis) convert all analysis nodes to the analysis bundle.  This is not great if you make heavy use of analysis submodules and node types: analysis_expression, analysis_unigene, etc.  You also might decide later down the road that you want to redefine some of your `mrna` features as `mrna_contig`, for example.

This module is under active development and is not yet released.

## Usage

* Define a destination bundle that is the same base table as your source bundle
* Navigate to the transmuter, located at `/admin/tripal/extension/tripal_alchemist`.
* Select a source bundle.  This is the current content type, and qualifying entities from this type will be transformed.
* Select a destination bundle.  This is what you want to turn your content into.
* Run **Transmute**.
* You're done!

>![The Tripal Alchemist Transmutation form](docs/img/tripal_alchemist_screen_1.png)
> The Tripal Alchemist Transmutation form.  If any entities qualify for your new bundle type from the selected bundle type, they will appear in the table at the bottom of the form.