# Edited graphics

Keep original exports in `../original/`. Save edited copies here using the
same relative paths and filenames. For example:

```
original/media/rom/sprite/n_cmn.farc/texture.farc/merge_d5comp_1.dds
replacements/media/rom/sprite/n_cmn.farc/texture.farc/merge_d5comp_1.dds
```

DDS atlases retain their original names and format. Preserve the DDS compression format and mipmaps when saving edits.
Preserve alpha and atlas layout. Individual sprite crops require atlas repacking
before they can replace atlas content; this is not implemented yet.

The installed game does not read this folder yet. Replacement loading and
repacking are separate work, after you have prepared edited images.
