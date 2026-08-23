# Current ProGen Branch Changes

`progen` is an additive branch based on `main` for the local ProGen 26 setup. It inherits the original Zero Hour/shared macOS fixes and then adds the following ProGen-specific layer.

## Source changes

- Adds the `SPECIAL_TOMAHAWK_STORM` special-power type and its Zero Hour name-table entry.
- Allows Tomahawk Storm through location, object, and general special-power validation.
- Routes Tomahawk Storm through superweapon construction-complete, ready, and enemy notification behavior.
- Includes Tomahawk Storm status in the in-game superweapon notification draw path.
- During the terminal dive of `ProGen_TomahawkStormMissile`, aligns the client model with the real physics velocity so the missile follows its visible trajectory. This does not change position, trajectory, explosion, or damage logic.

## ProGen mods and overlays

The branch tracks the active mod files and `GeneralsX.biglist` under `游戏文件/GeneralsZH-ProGen26/`:

- ProGen 26 Art, Data, English, Maps, Scripts, and Window packages.
- `!000_ProGenLaserComancheFix.big` for the Laser Comanche fix used by this setup.
- `!MenuMusicOriginal.big` for restoring the original menu music in the local setup.
- GenTool / Control Bar Pro-related 4K Control Bar overlays.
- The Boss AI package and references to the Expanded LAN Lobby Menu and DecalsZH files shared with `main`.

`GeneralsX.biglist` also names the original Zero Hour BIG files required at runtime, but those commercial game assets are not committed. This directory is not a standalone copy of the game; users must supply their own lawful original data.

## Purpose

Most changes respond to bugs, compatibility problems, or differences between community mods and the GeneralsX/macOS runtime observed during local play. A smaller group are personal single-player comfort changes, including display scaling, Control Bar layout, shortcuts, camera behavior, and menu music.

Thanks to the ProGen authors and maintainers for the ProGen gameplay and assets. Thanks also to GenTool, including the Control Bar Pro materials used by this local setup. This fork claims only its compatibility fixes and personal adjustments, not authorship of the original third-party mod content.

## Validation boundary

The branch collects work that has undergone local diagnosis and individual tests. The GitHub publication state has been checked for source differences, BIG-file boundaries, file sizes, and manifest contents, but the final combined commit has not yet received a fresh full build and in-game regression pass. It should not be treated as a fully validated binary release.
