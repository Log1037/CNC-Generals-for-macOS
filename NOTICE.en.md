# CNC Generals for iOS/macOS: Modification and Attribution Notice

[中文](NOTICE.md) | [English](NOTICE.en.md)

**CNC Generals for iOS/macOS** is the public name of this personal enhancement fork; it does not imply that this project independently created the Apple-platform port. This repository is a modified GPLv3 fork. It is not an official release by Electronic Arts, Westwood, EA Pacific, or any upstream community project.

## Direct upstream

- **[`ammaarreshi/Generals-Mac-iOS-iPad`](https://github.com/ammaarreshi/Generals-Mac-iOS-iPad)**
- This personal customization was consolidated from upstream commit `c5c8c4d3e757033d9ab464f6bd6e15e91e0e742f`.

The Apple-native porting base, iOS/macOS infrastructure, and corresponding achievements in this repository are inherited directly from that upstream project. Read the upstream README for its complete feature description, porting history, and original notes. This repository links to that material instead of copying it or presenting upstream achievements as original work by this fork.

## Earlier major sources

- [Electronic Arts GPLv3 source release](https://github.com/electronicarts/CnC_Generals_Zero_Hour)
- [TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode)
- [Fighter19/CnC_Generals_Zero_Hour](https://github.com/Fighter19/CnC_Generals_Zero_Hour)
- [fbraz3/GeneralsX](https://github.com/fbraz3/GeneralsX)

## Scope of this fork

In 2026 this fork was modified around the maintainer's Apple Silicon macOS setup, Chinese game data, external-disk installation, and real play requirements. The added work primarily covers:

- Retina, HiDPI, windowed mode, and native macOS fullscreen behavior;
- independent render cadence and simulation-speed controls;
- an in-game display, speed, camera, and local single-player panel;
- Chinese font fallback, glyph selection, text sizing, and UI legibility;
- fixes for videos, cinematics, lighting, scaled effects, and offscreen rendering;
- SDL input, cursor capture, menu robustness, and safe shutdown;
- game-data discovery, external-disk use, and local `.app` packaging.

This is not an official update from the direct upstream authors. Fork-specific problems should be reported in [this repository's issue tracker](https://github.com/Log1037/CNC-Generals-for-iOS-macOS/issues).

## Game-asset boundary

This repository does not provide commercial assets from Command & Conquer: Generals or Zero Hour. Users must own and supply lawful copies.

Game names, story, artwork, audio, maps, video, and other commercial assets remain the property of their respective rights holders. Do not commit `.big` archives, audio, video, maps, private fonts, or other commercial data to this repository.

A personal iPhone or iPad build may embed game data lawfully owned by its user at packaging time for installation on that user's own devices. Any `.ipa` or `.app` containing those commercial assets is a private build artifact and must not be committed to the source repository, attached to GitHub Releases, or distributed publicly.

## License

Source remains available under the repository's [GPLv3 license and EA additional terms](LICENSE.md). Third-party components retain their respective licenses.
