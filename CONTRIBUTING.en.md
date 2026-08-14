# Contributing

[中文](CONTRIBUTING.md) | [English](CONTRIBUTING.en.md)

Thank you for improving CNC Generals for iOS/macOS. This repository is a
personal enhancement fork of
[`ammaarreshi/Generals-Mac-iOS-iPad`](https://github.com/ammaarreshi/Generals-Mac-iOS-iPad).
Keep changes explainable, verifiable, and maintainable.

## Report first, then implement

- Search the existing [Issues](https://github.com/Log1037/CNC-Generals-for-iOS-macOS/issues).
- Use the bug-report form for reproducible problems and the feature-request form for experience improvements.
- Open an issue before a substantial code change and describe the user scenario, expected behavior, and intended boundary.
- If the problem also reproduces in the direct upstream without this fork's changes, say so explicitly. Do not submit the same disconnected report to several repositories.

## What to include in a report

- The commit or branch used;
- platform, device model, and operating-system version;
- relevant window/fullscreen, resolution, Retina/HiDPI, and UI-scale settings;
- game edition, language, and any unofficial fixes, maps, or mods;
- the shortest reproduction steps, expected result, and actual result;
- logs with account names, usernames, signing Team IDs, and private paths removed.

Do not upload commercial `.big` archives, video, audio, maps, or an `.app` or
`.ipa` containing those assets. Apple certificates, provisioning profiles,
device UDIDs, and account details must also remain private.

## Pull requests

1. Create a focused branch from this repository's `main`; address one clear concern at a time.
2. Read the root [AGENTS.md](AGENTS.md) and its linked build, code, and commit instructions.
3. Keep Zero Hour first. Mirror only clearly shared, low-risk platform changes to base Generals.
4. Add the required `GeneralsX @keyword` annotation for user-facing code and update the current development diary.
5. Use Conventional Commits-style commit and PR titles, and document the motivation, impact, and validation in the PR.
6. Commit only open-source code, configuration, and documentation—not generated apps, IPAs, game data, machine-local paths, or signing material.

AI-assisted code is accepted, but the submitter remains responsible for reviewing, polishing, and personally validating every change. Do not ask maintainers to finish a large unreviewed generated patch.

## Validation boundaries

- **macOS:** This is the primary use platform. Report at least the build result and whether the relevant windowed/fullscreen, UI, or gameplay path was manually verified.
- **iOS / iPadOS:** This fork's build with personal changes remains experimental. Building, signing, installing, launching, and reaching gameplay are separate milestones; claim only what was observed on a physical device. This testing boundary does not characterize the direct upstream build.
- **Linux:** Keep the shared engine buildable. Isolate macOS-specific behavior to platform code and state when Linux regression testing is outstanding.

Changes to render cadence, simulation speed, or game logic must also account for replay compatibility and determinism. Separate candidate fixes from player-confirmed fixes in the PR.

## License and lineage

By contributing, you agree that your changes may be distributed under the repository's [GPLv3 license and EA additional terms](LICENSE.md). Preserve author and upstream attribution, and do not copy closed-source plugins, commercial assets, or material with unclear licensing.
