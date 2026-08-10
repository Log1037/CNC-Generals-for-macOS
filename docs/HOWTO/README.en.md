# HOWTO Guides

[中文](README.md) | [English](README.en.md)

This directory contains end-user tutorials for GeneralsX.

## Paired Chinese and English guides

| Guide | Description |
|---|---|
| [macOS customized-fork quick start](MACOS_LOCAL_FORK_QUICK_START.en.md) | Transfer lawful Windows game data, build, deploy, and package the macOS app |
| [macOS build guide](../BUILD/MACOS.en.md) | Apple Silicon source builds, DXVK, deployment, and troubleshooting |
| [Customization engineering log](../WORKDIR/reports/MACOS_LOCAL_FORK_CHANGELOG.en.md) | Observed problems, implementation categories, validation, and publication record |

## Other existing tutorials

| Guide | Description |
|---|---|
| [Installation](INSTALLATION.md) | Install GeneralsX on Linux with Flatpak or on macOS |
| [Getting the Game Files](GETTING_THE_GAME_FILES.md) | Prepare game data through Steam, CrossOver, or SteamCMD |
| [SagePatch Configuration](SAGEPATCH_CONFIGURATION.md) | Configure camera height, scroll speed, terrain distance, and related options |
| [Russian Localization](RUSSIAN_LOCALIZATION.md) | English and Russian localization instructions |

## Adding a tutorial

1. Create a Markdown file in this directory.
2. Use `UPPERCASE_WITH_UNDERSCORES.md` naming.
3. For release-oriented documentation, provide a Chinese default file and a separate `.en.md` edition.
4. Add links to both the Chinese and English indexes.
5. Include the purpose, steps, verification method, and troubleshooting notes.
