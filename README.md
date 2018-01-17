
Tripal alchemist allows you to **transmute** (convert) entities from one type to another.


Tripal 3 provides migrations for most base content types.  Some content types (namely Analysis) convert all analysis nodes to the analysis bundle.  This is not great if you make heavy use of analysis submodules and node types: analysis_expression, analysis_unigene, etc.  You also might decide later down the road that you want to redefine some of your `mrna` features as `mrna_contig`, for example.

This module is under active development and is not yet released.

# Useage

* Define a destination bundle that is the same base table as your source bundle
* Run **Transmute**
* You're done!