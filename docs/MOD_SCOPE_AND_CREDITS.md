# Mod Scope And Credits

This fork is a personal GeneralsX/Zero Hour working tree for macOS play. Most
changes here were made to fix bugs or compatibility problems that appeared in
actual local testing. A smaller part of the work is comfort tuning for personal
single-player use, such as display scaling, Control Bar layout behavior, camera
range, readable UI text, shortcut handling, and local mod loading.

The repository is not a replacement for the original game, ProGen, GenTool, or
other community projects. It does not include the retail game data. You still
need your own lawful game installation, and the base game BIG files remain local
external assets.

The ProGen branch is intended for a local ProGen setup. It keeps ProGen-specific
packages and fixes separate from the shared/original branch so the common macOS
engine work can remain usable without ProGen-only gameplay data.

## Published branch layout

- `main` contains the original Zero Hour/shared macOS source changes and the
  common mod overlays used by that setup.
- `progen` starts from `main` and adds ProGen-only source changes and packages.
- Retail archives such as `INIZH.big`, audio, textures, maps, and other base-game
  data remain external and are never part of either branch.

The tracked `.big` files are mod packages or small local compatibility overlays.
They are committed so the exact tested load set can be reconstructed; their
presence does not change the ownership or licensing of the underlying projects.

Thanks to the ProGen mod authors and maintainers for the ProGen gameplay and
asset work used by the local ProGen setup. Thanks also to GenTool and its
included Control Bar Pro work, which this local setup uses and adapts for the
macOS/GeneralsX runtime. The changes in this fork should be read as local
compatibility fixes and personal adjustments around those projects, not as a
claim of authorship over them.

The repository also carries local overlays or selected files used alongside
Expanded LAN Lobby Menu, DecalsZH, and Boss AI packages. Those names identify
the local load set and are not claims that this fork created the original mods.
