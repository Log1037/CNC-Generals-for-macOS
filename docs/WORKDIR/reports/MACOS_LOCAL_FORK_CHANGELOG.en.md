# macOS Local Fork Engineering Log — English Edition

[Bilingual overview / 中英对照总览](MACOS_LOCAL_FORK_CHANGELOG.md) · [中文完整版本](MACOS_LOCAL_FORK_CHANGELOG.zh-CN.md)

- Last consolidated: 2026-08-10
- Local baseline: `ammaarreshi/Generals-Mac-iOS-iPad` `main` at `c5c8c4d3e757033d9ab464f6bd6e15e91e0e742f`
- Primary target: Command & Conquer: Generals — Zero Hour on Apple Silicon macOS

## Purpose

This document records the local second-stage work built on top of the existing
GeneralsX/macOS/iOS port. It is a release-oriented summary, not a chronological
debug transcript. For each area it separates the user-visible problem, the root
cause established during investigation, and the implementation that remains in
the current working tree.

The detailed investigation history remains in
`docs/DEV_BLOG/2026-07-DIARY.md`. That diary includes rejected approaches,
instrumentation, and machine-specific measurements. This document describes the
surviving changes that would need to be reviewed, split into commits, and made
portable before publishing the fork.

## Lineage and scope

The local work does not replace the upstream port. It extends a chain of work:

1. EA's GPLv3 source release.
2. TheSuperHackers' engine modernization.
3. Fighter19's Unix/SDL3/DXVK porting work.
4. fbraz3/GeneralsX's Linux and macOS integration.
5. ammaarreshi's Apple Silicon macOS/iOS/iPadOS fork.
6. This local macOS-focused customization layer, driven by issues found while
   installing and playing the Chinese Zero Hour release from an external disk.

No retail game assets are part of this source change set. A user-supplied copy
of Generals and Zero Hour is still required.

## Current repository state

As of this consolidation pass:

- `HEAD` and `origin/main` both point to `c5c8c4d3e`.
- The customization exists primarily as an uncommitted working tree.
- The tracked delta and the untracked file count are deliberately not quoted here.
  Earlier passes of this document carried hard figures (80 files / 5,075 insertions
  / 892 deletions / eight untracked files) that later work invalidated, and a stale
  number in a changelog is worse than no number because it reads as verified.
  Measure at branch time instead: `git diff --shortstat` and
  `git status --porcelain | grep -c '^??'`.
- The local user reports that the current installed build now provides a good
  overall play experience.
- This documentation pass did not launch or automate the game and did not rerun
  an isolated A/B test for every fix.

This is therefore a good local integration build, but not yet a reviewable
public release branch.

## What is intended to go public

Everything under version control is intended for public release. The local/public
split is enforced by `.gitignore` rather than by remembering what to leave out of a
commit: `游戏文件/` (this machine's runtime directory) is ignored at
`.gitignore:106`, so nothing in it can reach the public repository even by
accident.

That is what lets the local install and the public default differ without
maintaining two versions of any code.

### Tracked, and therefore public

| Area | Files |
|---|---|
| Chinese font resolution | `Core/Libraries/Source/WWVegas/WW3D2/render2dsentence.cpp`, `.h` |
| Ctrl+G panel layout | `GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/ExtrasMenu.cpp` |
| Font-size policy revert | `Core/GameEngine/Include/GameClient/GlobalLanguage.h`, `Core/GameEngine/Source/GameClient/GlobalLanguage.cpp`, `Core/GameEngine/Source/GameClient/Input/Mouse.cpp`, both `ControlBar.cpp` |
| Launcher and packaging | `packaging/macos/GeneralsXLauncher.m`, `scripts/build/macos/package-macos-zh-app.sh` |
| Deploy and run scripts | `scripts/build/macos/deploy-macos-zh.sh`, `scripts/build/macos/run-macos-zh.sh` |

Two deliberate decisions are encoded there:

- **Ship a real Song font, do not detect the host's.** `deploy-macos-zh.sh` fetches
  Noto Serif SC (SIL OFL 1.1) from a pinned commit and verifies it by sha256, and
  the engine looks in `fonts/` before it looks at the host. Host detection cannot be
  trusted as the primary path because `FcFontMatch()` never reports "not found" — it
  always returns *something*, so a machine without a Song face fails silently rather
  than falling back. If the fetch fails, deploy prints four numbered recovery
  options and continues rather than aborting the install.
- **Ship no font-size policy of its own.** The `TooltipFontSizeAdjustment` scaler
  was removed before publishing; every font now takes the engine's normal
  resolution adjustment.

### Local-only, and therefore never published

| Path | Local value | Public equivalent |
|---|---|---|
| `游戏文件/GeneralsZH/gx-font.conf` | `PingFang SC` (sans) | absent, so `fonts/` Noto Serif SC (serif 宋体) is used |
| `游戏文件/GeneralsZH/fonts/` | `NotoSerifSC-Regular.otf`, `README.txt` | created by deploy on first run |
| `游戏文件/GeneralsZH/fontconfig/conf.d/99-generalsx-private-fonts.conf` | adds `cwd/fonts` plus this machine's `AssetsV2/…Font8` and `Fonts/Supplemental` | written by deploy |
| `游戏文件/GeneralsZH/run.sh` | generated, machine paths baked in | written by deploy |
| `游戏文件/GeneralsZH/Data/**`, `*.big` | retail assets | user-supplied, never redistributed |

Net effect: the same committed code renders sans-serif (PingFang) on this machine
and serif (宋体, via the fetched Noto Serif SC) on a fresh public install. The only
difference is one ignored file.

### Branch-preparation status

- A dedicated `agent/macos-personal-customizations` branch now protects the
  working tree while it is split into reviewable commits.
- Engine and launcher defaults no longer contain this machine's external-volume
  path. The launcher supports environment overrides, remembered user choices,
  package-time paths, standard directories, and a native file/folder picker.
- The public font path has been tested for cold fetch, sha256 verification,
  idempotent re-run, corrupted-file repair, and unreachable-URL fallback, plus
  OS/2 PANOSE and cmap parsing of the asset. It has **not** been tested as a clean
  install on another machine.

## 1. Retina, render resolution, and macOS window management

### Problems observed

- The game rendered at logical point resolution and was then enlarged into a
  Retina drawable, making terrain, text, and 3D geometry soft.
- Physical pixels, logical points, engine resolution, and swapchain size were
  mixed together. This caused offset mouse input, incorrect pillarboxing, and
  fullscreen or windowed output at the wrong scale.
- A resizable window could change size while the engine kept its old render
  resolution. DXVK followed the window, while SAGE kept drawing the old target.
- The first implementation derived window size from render size while also
  deriving render size from window size. At reduced render scale this became a
  feedback loop and could grow or crop the window.
- Native fullscreen transitions worked at the Cocoa/SDL layer, but the engine
  did not update its windowed state or render target.
- A maximum window size based on the usable desktop area also limited the
  fullscreen content size, producing black bars.
- Leaving fullscreen could restore a window with an inaccessible title bar.

### Changes retained

- Request `SDL_WINDOW_HIGH_PIXEL_DENSITY` on Apple platforms.
- Make the macOS unit contract explicit:
  - SDL window geometry uses logical points.
  - the drawable, DXVK swapchain, and engine render target use physical pixels.
  - the backing density is carried separately and never applied twice.
- Patch DXVK's SDL3 WSI size query to use
  `SDL_GetWindowSizeInPixels` for swapchain sizing.
- Create fullscreen windows at the display size before the first D3D device is
  created, avoiding a bootstrap 1024x768 backbuffer.
- Add `GXRenderScalePercent` as a 50–100% render-scale preference:
  - 100% renders at native Retina pixels;
  - point-for-point mode derives the percentage from the live display density;
  - reduced scale changes the render target, not the visible window size.
- Store window point size as explicit state. A resolution choice, a user resize,
  and launch may change it; changing render scale may not.
- Implement the previously empty SDL window resize handler. It recalculates the
  engine render size from the settled window pixel extent and current render
  scale.
- Centralize macOS window sizing in the SDL3 display path and stop
  `DX8Wrapper::Resize_And_Position_Window` from issuing a second, point/pixel
  confused resize.
- Rebuild resolution-dependent resources after a live render-size change:
  header templates, mouse limits and tooltip fonts, shell layouts, the control
  bar, custom UI resources, and the tactical view.
- Follow native `ENTER_FULLSCREEN`/`LEAVE_FULLSCREEN` events instead of trying
  to own Cocoa's transition. The leaving path waits for the restored resize
  event before deriving a new render size.
- Use non-exclusive native fullscreen so the macOS menu bar can be revealed
  when the cursor is intentionally released.
- Keep the window maximum at full display bounds rather than usable bounds, so
  it prevents runaway window growth without capping the fullscreen drawable.
- Reserve and learn title-bar height when fitting a window to the usable area,
  preserve aspect ratio when Cocoa clamps one axis, and move an off-screen title
  bar back into reach.
- Mark both local app bundles as high-resolution capable.

### Main implementation areas

- `GeneralsMD/Code/Main/SDL3Main.cpp`
- `GeneralsMD/Code/GameEngineDevice/Source/SDL3GameEngine.cpp`
- `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp`
- `Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp`
- `cmake/apply-dxvk-macos-retina.cmake`
- `scripts/build/macos/bundle-macos-generals.sh`
- `scripts/build/macos/bundle-macos-zh.sh`

## 2. Safe fullscreen exit and recovery

### Problem observed

Under memory or WindowServer pressure, the game could exit while its native
fullscreen Space remained in front. The process could already be gone, leaving
an empty black Space with nothing useful in Force Quit and, on a single-display
setup, no normal way back to the desktop.

### Changes retained

- Leave fullscreen at the start of engine shutdown, while the window is still
  key, the app is still frontmost, and the SDL event loop is intact.
- Allow a 12-second default transition budget and periodically re-issue the
  un-fullscreen request rather than trying only once.
- Arm a detached shutdown watchdog before low-level teardown so a DXVK,
  MoltenVK, OpenAL, or worker-thread deadlock cannot hold the machine
  indefinitely.
- If the window is still fullscreen after the transition budget, close it and
  perform a last-resort CoreGraphics display reconfiguration. This reproduces
  the WindowServer reset caused by reconnecting a display, then restores the
  previous display geometry.
- Keep real macOS fullscreen Spaces enabled. A borderless-window substitute was
  tested and rejected because it did not provide correct coverage or pointer
  mapping.
- Add controlled diagnostic overrides for exercising or disabling the recovery
  path.

### Main implementation areas

- `GeneralsMD/Code/GameEngineDevice/Source/SDL3GameEngine.cpp`
- `GeneralsMD/Code/Main/SDL3Main.cpp`
- `GeneralsMD/Code/Main/MacDisplayKick.cpp`
- `GeneralsMD/Code/Main/CMakeLists.txt`
- `cmake/config-build.cmake`

## 3. Independent render cadence and game speed

### Problems observed

- The legacy loop effectively allowed one logic step per rendered frame, so
  render FPS and simulation speed were coupled.
- Raising game speed raised the render cap; lowering render FPS slowed the game.
- Campaign scripts using `SET_FPS_LIMIT 20` for 2003-era hardware also slowed
  scripted sequences on modern hardware.
- The old scripted time multiplier skipped rendered frames to create fast
  forward, producing uneven output.
- Camera rotation, pitch, and zoom advanced once per rendered frame even though
  their authored durations were expressed in logic frames.
- An earlier experiment scaled `timeGetTime`, which also scaled FFmpeg video,
  menu fades, timeouts, and other wall-clock consumers.

### Changes retained

- Add a fixed-step accumulator capable of running multiple ordered logic steps
  for one rendered frame.
- Treat render cadence and logic cadence as independent controls:
  - render cap: 30–240 FPS or uncapped;
  - game speed: 0.5x–6.0x around the retail 30 Hz simulation baseline.
- Keep network games on their network-controlled cadence. Local time-scale
  controls do not run a network simulation ahead of peers.
- Apply `SET_TIME_MULTIPLIER` to logic cadence rather than render skipping.
- Treat a map's `SET_FPS_LIMIT` as a render hint that cannot lower the user's
  chosen cap.
- Make client-side animation scaling agree with the actual ratio between logic
  time and render frames.
- Advance scripted camera motion by fractional logic-frame time, making authored
  duration independent of render FPS.
- Restore `timeGetTime` as monotonic real time and add a 64-bit
  `GeneralsXGetRealTimeMilliseconds` helper. FFmpeg video remains on real time.
- Allow the render/game-speed controls through the cinematic no-input gate while
  continuing to block gameplay commands.
- Make the `-file` direct-map launch path available in release builds for
  reproducible campaign and cinematic testing.

### Current behavior note

There is no separate cinematic-speed engine path in the current implementation.
Scripted in-engine scenes follow the same game-speed setting as gameplay, and
pre-rendered FFmpeg video follows real time. The previously proposed
`GXCinematicSpeedTenths`/`GX_CINEMATIC_LOGIC_FPS` path was removed from the
engine because it was not actually connected to the frame pacer.

### Main implementation areas

- `Core/GameEngine/Include/Common/FramePacer.h`
- `Core/GameEngine/Source/Common/FramePacer.cpp`
- `Core/GameEngine/Source/Common/FrameRateLimit.cpp`
- `GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp`
- `GeneralsMD/Code/GameEngine/Source/GameLogic/ScriptEngine/ScriptActions.cpp`
- `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp`
- `GeneralsMD/Code/CompatLib/Source/time_compat.cpp`
- `GeneralsMD/Code/GameEngine/Source/GameClient/MessageStream/CommandXlat.cpp`

## 4. In-game display, speed, clarity, and trainer panel

### Problems observed

- The earlier Extras menu depended on a loose `.wnd` file that was not deployed.
- Its full-screen parent could outlive the shell and overlay gameplay.
- Port-specific labels had no CSF entries and rendered as missing strings.
- Hard-coded 800x600 widget sizes produced tiny slider thumbs, stretched layout,
  invisible artwork, and overflowing buttons at high resolution.
- The requested controls were hidden behind shortcuts rather than visible in a
  panel.

### Changes retained

- Rebuild the panel programmatically so it owns its `WindowLayout`, has no loose
  game-data dependency, and has a defined lifetime in both shell and gameplay.
- Provide English and Simplified Chinese built-in fallbacks while still allowing
  CSF labels to override them later.
- Scale the panel uniformly from an 800x600 design space and scale its fonts by
  the same factor.
- Add live rows for:
  - render FPS;
  - game speed;
  - camera pitch;
  - scroll speed;
  - terrain draw distance;
  - local-player money.
- Add a live clarity switch between native HiDPI and point-for-point rendering.
- Save display/speed/view preferences only from the Save button. Money is never
  persisted and is excluded from Defaults.
- Restrict the money slider to local single-player play. It is disabled in the
  shell, network games, and replay playback to avoid lockstep desync or replay
  corruption.
- Edit money through deposit/withdraw without playing the income sound or
  recording it as earned income.
- Use retail three-piece button art where available and a deliberate color-drawn
  horizontal slider where no mapped slider art exists.
- Make horizontal sliders respect the live thumb geometry for hit testing, tick
  spacing, resizing, and max-position placement.
- Swallow panel mouse input so a slider drag cannot also select units underneath.
- Add an always-visible cadence/cash status line to the in-game quit menu.
- Size every text box for CJK rather than Latin text. A box of `H` design units
  gets `H * scale` pixels, but the font it holds is asked for `P * scale` points
  and the engine converts points to pixels at 96 DPI, so the glyphs occupy
  `P * scale * 96/72`. A CJK line then needs roughly `1.34x` its em for ascent
  plus descent, giving `H >= 1.79 * P`. The old numbers failed that: a 10-point
  label had 15 units where it needed 18, and a 14-point title had 22 where it
  needed 26. Since both sides scale together the shortfall was proportional, so
  Chinese text clipped at *every* resolution and merely grew more obvious as the
  screen did. Latin text hid the defect because its ascent plus descent is nearer
  `1.0x` the em. Row stride, the mode block, the button row and the panel height
  all moved to keep the layout clear of the enlarged boxes.
  Measured, not assumed: Songti SC, PingFang SC and SimSun all report
  `upem 1000, ascent 33, descent 10` at a 32px em, so the choice of font does not
  affect this and switching to 宋体 did not cause it.
- Fix the one box that pass missed: the footer status line. Its height was written
  as a bare literal at the call site rather than as a named constant with the rest
  of the layout, so it kept 12 units while carrying the same 10-point label font as
  every row label, against the 18 the rule above requires. The descender of its
  Chinese line was cut off, and only on the very last line of the panel, which is
  the hardest place to attribute a clipped box to anything. It is now
  `FOOTER_HEIGHT`, sitting beside `FOOTER_GAP` and `FOOTER_TOP`.
- Then size that footer from the font instead of from the design constant, because
  `1.79` is itself derived from one family's metrics and families differ. Songti SC
  and PingFang SC both report `ascent 1060 / descent -340` per 1000 em (`1.40` of
  em), but Noto Serif SC reports `1151 / -286` (`1.44`). The public build ships
  Noto, so a constant validated against the host's Songti was about 3% short there
  and would have clipped on exactly the machines no local test can observe. The
  footer now takes `max(FOOTER_HEIGHT * scale, labelFont->height)`, where
  `GameFont::height` is `W3DGameFont`'s `Get_Char_Height` — the `CharAscent +
  descent` cell that `render2dsentence` computes after expanding both to the face's
  own FreeType bounding box, i.e. the cell the glyphs are actually rasterized into.
  Correct for any face, including one a user supplies.
- Drop `PANEL_HEIGHT` as a constant, since the panel can no longer know its own
  height before the font is resolved. It is now
  `FOOTER_TOP * scale + footerH + PANEL_MARGIN * scale`, derived from the same
  measured `footerH` the footer box uses, so the bottom margin equals
  `PANEL_MARGIN` by construction whatever the font turns out to be. In design units
  this still comes to `348 + 18 + 16 = 382`; a taller face now grows the footer and
  the panel together rather than pushing the text past a fixed edge. Fonts are
  therefore resolved before any dimension is computed in `buildPanel`.

### Controls

| Control | Effect |
|---|---|
| `Ctrl+G` | Toggle the display/game-speed panel |
| `Ctrl+[` / `Ctrl+]` | Decrease/increase render cap |
| `Shift+Ctrl+[` / `Shift+Ctrl+]` | Decrease/increase game speed |
| `Alt+N` | Add 10,000 cash in non-multiplayer play |

### Main implementation areas

- `GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/ExtrasMenu.cpp`
- `Core/GameEngine/Source/GameClient/GUI/Gadget/GadgetHorizontalSlider.cpp`
- `Core/GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DHorizontalSlider.cpp`
- `GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/QuitMenu.cpp`
- `GeneralsMD/Code/GameEngine/Source/GameClient/MessageStream/MetaEvent.cpp`

## 5. Chinese fonts, glyph integrity, and legibility

### Problems observed

- Some Chinese characters were missing, substituted by hollow squares, or
  rendered through an unsuitable fallback family.
- CJK glyphs could be vertically clipped because Latin-oriented ascender and
  descender metrics were shorter than the font's ideographic bounding box.
- A global font-size increase fixed some small labels but overflowed other
  fixed-height menu controls.
- The control-bar build tooltip used a hard-coded 12-point Unicode override and
  therefore became relatively smaller as resolution increased.
- SDL3 computed cursor tooltips but never drew them, and high-DPI mouse noise
  repeatedly reset the hover delay.

### Changes retained

- Ship an open-licensed Song (宋体) face rather than depending on the host having
  one. `deploy-macos-zh.sh` fetches Noto Serif SC (SIL OFL 1.1) into
  `<runtime>/fonts`, pinned to a commit SHA and checked against a known sha256.
  Nothing is installed system-wide and no private font is redistributed.
  Host-font detection is still there and still verified, but it is now the
  fallback rather than the primary route: the search can legitimately find
  nothing on a machine with no Song font, and Chinese text then degrades to a
  sans face or to hollow boxes. Fetching one makes every install look the same.
  A failed download is deliberately non-fatal — the deploy still succeeds, prints
  what to do about it, and the engine falls through to the host's own Songti SC.
- Verify every candidate before accepting it, because three separate faults were
  stacked here and each hid the next:
  - `Language.ini` is GBK, so the requested family arrives as raw bytes
    (`CB CE CC E5` for 宋体) that Fontconfig cannot parse.
  - `FcFontMatch()` never reports "not found"; it returns the closest remaining
    candidate. A miss was therefore indistinguishable from a hit, and the Chinese
    UI had most likely been rendering in the default sans all along.
  - The FreeType face index was hard-coded to 0. In a collection that is not the
    Regular weight: macOS `Songti.ttc` face 0 is *Songti SC Black* and Regular is
    face 6, so every CJK glyph came out in the heaviest weight available.
- Accept a face only if it has a Unicode charmap and real ideograph coverage
  (U+6C49, U+4E2D, U+56FD), and, when auto-detecting, only if its OS/2 PANOSE
  serifStyle byte is outside the sans range 11-15. That last test is what rejects
  a sans face carrying the filename `simsun.ttc`.
- Compare against every `FC_FAMILY` value rather than the first. A font carries
  one name per language, so on a Chinese-locale machine index 0 of PingFang SC is
  the localized `苹方-简`, and checking only that rejected a valid match.
- Prefer the language-selected Unicode family first, then use a CJK-aware
  fallback order including SimSun, Songti SC, PingFang SC, Noto CJK, and DejaVu.
- Explicitly select the Unicode FreeType charmap for collections that otherwise
  expose a legacy charmap first.
- Expand scalable font cell metrics to the declared font bounding box when CJK
  glyphs need more ascent or descent.
- Resolution-scale the Unicode control-bar tooltip override rather than leaving
  it at a flat 12-point size, so it stops shrinking away from its own container
  as the resolution rises.
- Reverted before publishing: a `TooltipFontSizeAdjustment` key (default 1.25)
  and an `adjustTooltipFontSize` helper that enlarged hint text in self-sizing
  boxes. Tooltips and the build description now take plain `adjustFontSize`, the
  same resolution scaling as every other font, so the fork ships no font-size
  policy of its own.
- Implement SDL3 cursor-overlay drawing so unit hints, window tooltips, and
  cursor text appear on top of the rendered UI.
- Use a small anchored movement tolerance before restarting the tooltip delay,
  preventing modern high-DPI mouse jitter from suppressing tooltips.
- Remove the high-volume `[GX-ISSUE144]` font diagnostics after the fallback
  investigation was closed; retain only a one-shot Unicode-charmap warning.

### Choosing the Chinese font

The default is a Song (宋体) face, because that is what Windows Generals uses. It
is a preference, not a requirement, and there are three ways to change it, in
increasing order of permanence.

One run only:

```bash
GX_CJK_SERIF_FONT="PingFang SC" ./run.sh
```

Per install: put one family name in `<runtime>/gx-font.conf`. Both launchers
(`run.sh` and `scripts/build/macos/run-macos-zh.sh`) read it, and an exported
variable still wins. Deploy writes the file with every line commented out, which
means "use the Song font in `fonts/`";
`GX_CJK_FONT="PingFang SC" ./scripts/build/macos/deploy-macos-zh.sh`
writes a pinned choice instead. A file already present is never silently replaced,
since that is where someone would have made a manual edit.

By font file: drop a font into `<runtime>/fonts` named `song.otf`, `simsun.ttc`,
`simsun.ttf` or `songti.ttc`. It is used ahead of any system font, and at the same
priority as the `NotoSerifSC-Regular.otf` the deploy script puts there.

Resolution order, highest first: `GX_CJK_SERIF_FONT` → `gx-font.conf` →
`<runtime>/fonts` → the host's own Song face → an open-licensed Song family via
fontconfig. Both ends are verified on this machine: with `gx-font.conf` pinned to
PingFang SC the log reports
`GX CJK font: override family 'PingFang SC' -> .../PingFang.ttc (face 3)`, and with
that file moved aside it reports
`GX CJK serif font: runtime fonts/ fonts/NotoSerifSC-Regular.otf (face 0)`.
Those `INFO:` lines go to stderr, which the app redirects, so they land in
`~/Library/Logs/GeneralsX/ZeroHour.log` and not in `GeneralsXZH_d3d9.log`.

An explicitly named font is *not* held to the serif test, so sans families such
as PingFang SC are accepted on purpose. Auto-detection keeps the test, because
there the name is a guess and a mislabelled file is the common case. List the
installed candidates with `fc-list :lang=zh family`.

Name a *family*, not a path, wherever possible: macOS keeps PingFang under
`/System/Library/AssetsV2/com_apple_MobileAsset_Font8/<hash>.asset/...`, and that
hash changes with OS updates.

### Distribution note

No private font is redistributed, and no font binary is committed. The one font
that ships is Noto Serif SC under SIL OFL 1.1, and it is fetched at deploy time
rather than stored in the repository, which keeps an 11.6MB binary out of git
history. `resources/fonts/` is still gitignored and the deploy script still copies
nothing out of it.

The URL is pinned to commit `9b0f143` of `notofonts/noto-cjk` rather than to
`main`, so the bytes cannot change underneath us and fail a checksum on some later
deploy for no visible reason, and the file is verified against sha256
`e8f396de…83a1cd3` before it is moved into place. The download lands on a `.part`
name first, so an interrupted transfer can never leave a truncated font at the real
path where the engine would find it and reject it at startup. Re-running deploy is
idempotent: a file already present and matching its checksum is left alone, and one
that fails is replaced.

The SubsetOTF SC build (11.6MB) is used rather than the full CJK OTF (24MB); it
still covers every codepoint the Simplified Chinese localization needs. It was
checked against the engine's own acceptance tests before being pinned: PANOSE
serifStyle 2, and U+6C49 / U+4E2D / U+56FD all present, so it passes the serif and
coverage checks rather than merely being named like a Song font.

The bundled vcpkg `fonts.conf` needed one addition: it lists only the older
`Assets/com_apple_MobileAsset_Font3` and `Font4` directories, so several current
macOS system fonts, PingFang among them, were invisible to the game while being
perfectly visible to `fc-match` run against Homebrew's config. Deploy now adds
the `AssetsV2` container directories and `/System/Library/Fonts/Supplemental`.

### Main implementation areas

- `Core/GameEngine/Source/GameClient/GlobalLanguage.cpp`
- `Core/Libraries/Source/WWVegas/WW3D2/render2dsentence.cpp`
- `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/GUI/W3DGameFont.cpp`
- `Core/GameEngine/Source/GameClient/Input/Mouse.cpp`
- `GeneralsMD/Code/GameEngine/Source/GameClient/GUI/ControlBar/ControlBar.cpp`
- `scripts/build/macos/deploy-macos-zh.sh`
- `scripts/build/macos/run-macos-zh.sh`

## 6. Videos, challenge presentation, and cutscene continuity

### Problems observed

- Video textures could be reported as supported by the D3D8 capability path but
  fail when FFmpeg tried to lock them through DXVK/MoltenVK.
- Challenge mode could select a legacy low-memory path. Many challenge missions
  provide no useful voice duration there, so the general-versus-general intro
  and announcer sequence disappeared almost immediately.
- Earlier game-time scaling made video timing diverge from audio.

### Changes retained

- Try a validated sequence of video-buffer formats and require both texture
  creation and a successful lock before accepting one. Prefer 32-bit XRGB on
  Apple platforms.
- Guard video-buffer validity against a null underlying D3D texture and emit
  useful allocation diagnostics.
- Keep FFmpeg presentation on the 64-bit monotonic wall clock.
- On macOS, prefer the complete animated Challenge intro rather than the legacy
  low-memory static path, and make progress division safe for very short clips.
- Add lifecycle diagnostics for challenge intro open, allocation, playback,
  abort, and completion.

### Main implementation areas

- `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DVideoBuffer.cpp`
- `Core/GameEngineDevice/Source/VideoDevice/FFmpeg/FFmpegVideoPlayer.cpp`
- `GeneralsMD/Code/GameEngineDevice/Source/VideoDevice/FFmpeg/FFmpegVideoPlayer.cpp`
- `Core/GameEngine/Source/GameClient/GUI/LoadScreen.cpp`

## 7. Rendering correctness under scaled/offscreen targets

### Problems observed

- Distant or low-LOD units, especially infantry, could become abnormally dark.
- Screen filters restored the swapchain backbuffer captured at device init
  instead of the render target that was active when a render-to-texture pass
  began. Under pillarbox/render scaling, later UI or filter output could be sent
  to a surface that was subsequently cleared and overwritten.
- Heat-distortion smudges sampled the previous upscaled backbuffer instead of the
  live scene target. At reduced render scale the UV range collapsed, producing a
  displaced miniature view around effects such as the microwave tank.
- Stale or missing tree draw-module types could be dereferenced when a moving
  unit entered their partition.

### Changes retained

- Backport DXVK fixed-function `COLOR1` behavior from upstream commit
  `95d591a8d3f2`:
  - include `SpecularEnabled` in the fixed-function vertex-shader key;
  - dirty the vertex shader when `D3DRS_SPECULARENABLE` changes;
  - pass through the original vertex `COLOR1` when specular lighting is off.
- Apply the DXVK patch reproducibly from CMake for both local-fork and remote
  DXVK source modes.
- Capture the live render target and depth surface at the start of each
  render-to-texture pass and restore those exact surfaces at the end.
- Resize the filter RTT texture to the live scene target's size and format.
- Make the smudge manager copy and sample the currently bound scene target,
  initialize all resource members, and recreate its background texture when the
  target size changes.
- Guard tree type indices and null draw-module data before applying toppling or
  push-aside behavior.
- Log the unimplemented `D3DRS_PATCHSEGMENTS` warning once per process rather
  than once per material application, avoiding synchronous log-I/O stalls.

### Verification note

The earlier handoff recorded the `COLOR1` change as a candidate pending isolated
visual confirmation. The current user reports a good overall experience, but
this documentation pass did not rerun a controlled before/after capture. It
should therefore be described in a public pull request as an implemented and
locally used fix, with its standalone regression test status stated explicitly.

### Main implementation areas

- `cmake/apply-dxvk-macos-retina.cmake`
- `cmake/dx8.cmake`
- `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp`
- `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DSmudge.cpp`
- `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTreeBuffer.cpp`

## 8. SDL input and cursor integration

### Problems observed

- Numeric keypad scancodes fell through to `KEY_NONE`, disabling the stock
  keypad camera controls.
- Fullscreen cursor capture could only be broken by switching away from the
  game.
- A direct cursor release would be immediately undone by the next focus or mode
  refresh.
- The menu bar remained hard-hidden even after the cursor was released.

### Changes retained

- Map all numeric keypad digits and operators, including Num Lock/Clear, in both
  Generals and Zero Hour SDL3 keyboard backends.
- Add a persistent user-release reason to the existing cursor-capture policy.
- Bind `Cmd+G` ahead of the engine keyboard device to release or recapture the
  cursor without sending the `G` hotkey into the game.
- Reveal the macOS menu bar on hover while the cursor is released in fullscreen,
  then hide it again when capture resumes.
- Re-evaluate cursor limits and capture rules after resolution and native
  fullscreen changes.

## 9. Shell, menu, and UI robustness

### Problems observed

- Returning focus after a swapchain rebuild could leave the animated 3D main
  menu black even though the shell map and camera still existed.
- Empty `TEXT = ""` fields in the quit-menu layouts crashed the parser.
- Challenge mode forced the render cap back to the 30 Hz logic baseline.
- A partially initialized image slider could dereference missing art.

### Changes retained

- Queue a delayed shell-map refresh after macOS focus restore and track shell-map
  ownership separately from potentially stale game-mode state.
- Bound the `.wnd` opening-quote scan and treat an empty text label as valid.
- Preserve the user's render cap when Challenge mode starts.
- Fall back to the color slider renderer when the image triplet is incomplete.
- Remove the obsolete dynamically created Options-menu Extras button and the
  deleted loose `ExtrasMenu.wnd`; the new panel is a self-owned overlay.

## 10. External-disk assets and filesystem behavior

### Problems observed

- The port assumed a runtime under `~/GeneralsX`, while the real installation and
  licensed assets lived on an external APFS volume.
- Recursive BIG discovery descended into unrelated directories and could block
  for minutes on removable storage.
- The local file listing implementation matched only extensions, did not fully
  implement Windows wildcards, and manually recursed in a fragile way.
- A stale duplicate BIG under a loose `Data` tree could be discovered.

### Changes retained

- Add `GX_RUNTIME_ROOT` to asset download, deploy, and run scripts while
  preserving the upstream home-directory default.
- Add an optional root-level `GeneralsX.biglist` manifest for an explicit archive
  load order.
- On Unix, scan only the install root for `.big` archives through `opendir` when
  no manifest is present; do not recurse through the external volume.
- Implement case-insensitive `*`/`?` wildcard matching and robust optional
  recursive iteration in the general local filesystem path.
- Preflight `INIZH.big` directly instead of enumerating the entire removable
  directory from the launcher.
- Add a legacy BIG-entry extraction helper for controlled resource recovery.

## 11. Reproducible macOS app packaging and deployment

### Problems observed

- The double-clickable Chinese app had been assembled manually and could carry a
  stale engine or stale DXVK libraries after a successful rebuild.
- Its `CFBundleExecutable` could point directly at the engine, bypassing required
  environment setup.
- The deploy directory and the real play directory could contain different
  binaries and runtime libraries.
- Vulkan discovery assumed a LunarG layout and did not reliably accept a
  Homebrew loader/MoltenVK pair.
- A JSON ICD manifest under `Contents/Frameworks` caused nested-code signing
  validation problems.

### Changes retained

- Add a compiled Cocoa launcher as the app entry point. It establishes:
  - the Zero Hour and Generals asset roots;
  - working directory;
  - `DYLD_LIBRARY_PATH` for DXVK's bare-name `dlopen`;
  - Vulkan ICD environment;
  - Fontconfig paths;
  - cadence environment;
  - a persistent user log under `~/Library/Logs/GeneralsX`.
- Show a Chinese launch error when the external volume, core assets, runtime
  libraries, or engine executable are missing.
- Add `--check` to validate launcher inputs without starting the game.
- Add `package-macos-zh-app.sh` to compile the launcher, stage engine and dylibs,
  move the ICD manifest to Resources, write `Info.plist`, sign inside-out, run
  the preflight, and atomically replace the local app.
- Add an icon and a proper bundle identity/display name.
- Teach deploy to include OpenAL, DXVK D3D8/D3D9, Vulkan/MoltenVK, Fontconfig,
  fonts, GameSpy, wrapper scripts, and ICD data.
- Accept either a complete LunarG SDK or a Homebrew Vulkan-loader/MoltenVK pair.
- Use `install -m 644` for Vulkan dylibs so a read-only Homebrew source mode does
  not break the next deploy.
- Mirror only engine-produced payloads into additional real play directories via
  `GX_MIRROR_DIRS` or asset-directory auto-detection. Retail archives, saves,
  replays, and loose data are not overwritten.
- Refresh the app automatically at the end of deploy, with
  `GX_SKIP_APP_BUNDLE=1` as an opt-out.
- Make the launcher read `gx-font.conf`, the file that selects the Chinese font.
  Only `run.sh` read it before, so the same install rendered a different font
  depending on how it was started: launching from a terminal honored the chosen
  family, while double-clicking the app silently fell through to auto-detection and
  picked a Song face. Nothing on screen indicates which launch path produced the
  window, so this presented as the setting simply not working. The launcher now
  parses the file the same way `run.sh` does — first line that is neither blank nor
  a `#` comment — and an already-exported `GX_CJK_SERIF_FONT` still wins, so
  `GX_CJK_SERIF_FONT="PingFang SC" open -a ...` remains a valid one-off override.
  Confirmed from the log: runs started by `run.sh` recorded
  `override family 'PingFang SC'` while runs started by the app recorded
  `system Songti.ttc (face 6)`.

### Verification note

Two symptoms that looked like failed fixes were both stale-artifact problems, not
logic errors, and the distinction is worth recording because the diagnosis path was
misleading.

The app bundle carried an engine built *before* the panel fix, so re-testing
through the app kept reproducing the already-fixed layout. Deploying to
`游戏文件/GeneralsZH/` does not refresh the bundle; packaging must be re-run. The
check that settles it is comparing the bundle's engine against the build output
after stripping signatures — currently 2 differing bytes at offsets 1474–1475, a
signature-related header field, with the code otherwise identical.

`DEBUG_LOG` is a no-op in this `RelWithDebInfo` configuration, so the panel's
`ExtrasMenu: panel …` line does not exist in the shipped binary and never appears
in the log. Any verification plan that depends on reading it is invalid; binary
comparison is the reliable check.

### Main implementation areas

- `packaging/macos/GeneralsXLauncher.m`
- `packaging/macos/GeneralsXZH.icns`
- `scripts/build/macos/package-macos-zh-app.sh`
- `scripts/build/macos/deploy-macos-zh.sh`
- `scripts/build/macos/run-macos-zh.sh`
- `scripts/get-assets.sh`
- `docs/BUILD/MACOS.md`

## 12. User-facing preferences and diagnostics

### Persistent preferences

| Key | Meaning | Current code range/default |
|---|---|---|
| `GXRenderFPS` | presentation/render cap | 30–240, default 60 |
| `GXGameSpeedTenths` | simulation speed in tenths | 5–60, launcher default 10 |
| `GXRenderScalePercent` | render pixels relative to native drawable | 50–100, default 100 |

### Operational environment variables

| Variable | Purpose |
|---|---|
| `GX_RUNTIME_ROOT` | choose an asset/runtime root outside `~/GeneralsX` |
| `GX_MIRROR_DIRS` | colon-separated deploy mirror targets |
| `GX_SKIP_APP_BUNDLE` | skip automatic local app refresh |
| `SAGE_PATCH_ENABLED` | opt in to the optional SagePatch interposer |
| `GX_STATE_PROBE` | enable the cadence/cinematic state diagnostic |
| `GX_EXIT_UNFULLSCREEN_SECONDS` | override fullscreen-exit wait budget |
| `GX_EXIT_WATCHDOG_SECONDS` | override hard shutdown watchdog budget |
| `GX_EXIT_DISPLAY_KICK` | disable the last-resort display reconfiguration with `0` |
| `GX_EXIT_TEST_REFUSE` | deliberately exercise the stranded-fullscreen recovery path |

`GX_EXIT_TEST_REFUSE` is a destructive diagnostic for controlled testing, not a
player-facing option.

## 13. Validation evidence retained in the diary

The development diary records the following forms of validation:

- in-process construction and interaction tests for the programmatic panel;
- slider geometry and exact value round-tripping at windowed and native Retina
  resolutions;
- independent render cap and 4.0x logic-rate behavior;
- repeated panel open/close and live clarity switching;
- real window resize handling and no-op pillarbox at matching sizes;
- Finder launch of a freshly packaged and signed app;
- native fullscreen round trips and cursor release;
- removal of fullscreen black bars by correcting the window ceiling;
- quit-menu parsing without a crash;
- challenge-video allocation and timing diagnostics;
- forced fullscreen-shutdown failure followed by successful display recovery;
- clean removal of high-volume font diagnostics.

The current user's overall assessment is positive. Before a public tag, repeat a
short manual matrix on a clean build rather than treating an old local binary as
proof:

1. native and reduced-scale fullscreen;
2. windowed launch, manual resize, and scale switch;
3. fullscreen enter/leave from both the green button and `Ctrl+Cmd+F`;
4. normal quit and a controlled recovery-path test;
5. campaign scripted camera and speed changes;
6. Challenge intro video and announcer audio;
7. distant infantry lighting and screen-filter effects;
8. Chinese menus, build tooltips, cursor tooltips, and mixed-script glyphs;
9. single-player trainer behavior, replay playback, and a network safety smoke
   test.

## 14. Publication preparation and follow-up

### Completed for source publication

1. Created the `agent/macos-personal-customizations` safety branch.
2. Kept retail assets, saves, replays, SDKs, build products, private fonts, and
   machine-local archives outside the public file set.
3. Removed machine-specific external-volume defaults from the engine and Cocoa
   launcher.
4. Removed stale `GXCinematicSpeedTenths` / `GX_CINEMATIC_LOGIC_FPS` launcher
   handling and the obsolete render-FPS-to-logic-FPS coupling.
5. Updated `docs/BUILD/MACOS.md` to match the engine cadence model.
6. Standardized the clean-install and panel Defaults behavior at 60 FPS / 1.0x.
7. Added a native, persistent, fault-tolerant picker for both Zero Hour and base
   Generals assets, including marker-file, game-folder, and parent-folder input.
8. Added bilingual attribution, engineering-log, and end-user setup documents.
9. Switched public packaging to the icon already tracked by upstream; the local
   custom ICNS is ignored.

### Follow-up after source publication

1. **Make DXVK changes reviewable upstream.** Keep the reproducible CMake patch
   until the changes are represented by dedicated commits in the DXVK fork.
2. **Review platform boundaries.** Consolidate cross-layer macOS helper `extern`
   declarations behind a small platform interface where practical.
3. **Expand platform validation.** Check base Generals, Linux, iOS, replay
   determinism, and network lockstep for shared `Core/` changes.
4. **Run a clean-machine install.** Validate the complete guide on another Apple
   Silicon Mac without this machine's prior preferences or runtime state.
5. **Review binary-release licensing.** Recheck third-party libraries, fonts,
   icons, and attribution before attaching compiled release assets.

### Product decisions retained in this fork

- Public Defaults use 60 render FPS and 1.0x game speed. A user may still save
  2.2x or another preferred value.
- The guarded `Alt+N` binding and money slider remain an intentional personal-fork
  feature. They are disabled in the shell, replays, and network play.
- The CoreGraphics display kick is intentionally a last resort and changes the
  display mode briefly. Keep the strong guard, document BetterDisplay/scaled-mode
  interaction, and test on more than one monitor configuration.
- Confirm Generals, Linux, iOS, replay determinism, and network behavior for
  shared `Core/` changes. The local validation effort was centered on Zero Hour
  on Apple Silicon macOS.
- Public packaging uses the icon already present in the upstream asset set. The
  machine-local custom ICNS is excluded.
- Convert the informal date-only code annotations to the project's required
  author/date form and preserve upstream attribution.

## 15. Suggested commit series

A public history should not land as one 5,000-line commit. A reviewable order is:

1. `fix(filesystem): make asset discovery external-disk safe`
2. `build(macos): make local app packaging reproducible`
3. `fix(macos-hidpi): separate points, pixels, and render scale`
4. `fix(macos-window): follow native fullscreen and window resize`
5. `fix(macos-shutdown): recover stranded fullscreen spaces`
6. `fix(timing): decouple rendering from fixed-step simulation`
7. `fix(cinematics): make camera and video timing frame-rate independent`
8. `feat(extras): add localized display and speed panel`
9. `feat(trainer): add guarded single-player cash controls`
10. `fix(fonts): improve CJK fallback, metrics, and tooltip sizing`
11. `fix(input): restore keypad and SDL cursor overlays`
12. `fix(rendering): repair scaled render-target effects`
13. `fix(dxvk-macos): backport fixed-function color and pixel sizing`
14. `docs(macos): publish setup, controls, validation, and limitations`

Each commit should build independently where practical. Keep the trainer commit
separable so maintainers can accept the platform and correctness fixes without
taking a gameplay-altering feature.

## 16. File-group inventory

### Timing and simulation

- `Core/GameEngine/Include/Common/FramePacer.h`
- `Core/GameEngine/Source/Common/FramePacer.cpp`
- `Core/GameEngine/Include/Common/FrameRateLimit.h`
- `Core/GameEngine/Source/Common/FrameRateLimit.cpp`
- `GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp`
- `GeneralsMD/Code/GameEngine/Source/GameLogic/ScriptEngine/ScriptActions.cpp`
- `GeneralsMD/Code/CompatLib/Source/time_compat.cpp`

### Display, DXVK, and effects

- `Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp`
- `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp`
- `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp`
- `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DSmudge.cpp`
- `Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DVideoBuffer.cpp`
- `cmake/apply-dxvk-macos-retina.cmake`
- `cmake/dx8.cmake`

### UI, fonts, and input

- `GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/ExtrasMenu.cpp`
- `Core/GameEngine/Source/GameClient/GUI/Gadget/GadgetHorizontalSlider.cpp`
- `Core/GameEngine/Source/GameClient/GlobalLanguage.cpp`
- `Core/Libraries/Source/WWVegas/WW3D2/render2dsentence.cpp`
- `GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/GUI/W3DGameFont.cpp`
- `Core/GameEngine/Source/GameClient/Input/Mouse.cpp`
- both Generals and Zero Hour SDL3 keyboard/mouse backends

### macOS lifecycle and packaging

- `GeneralsMD/Code/Main/SDL3Main.cpp`
- `GeneralsMD/Code/Main/MacDisplayKick.cpp`
- `GeneralsMD/Code/GameEngineDevice/Source/SDL3GameEngine.cpp`
- `packaging/macos/GeneralsXLauncher.m`
- `scripts/build/macos/package-macos-zh-app.sh`
- `scripts/build/macos/deploy-macos-zh.sh`
- `scripts/build/macos/run-macos-zh.sh`

### Assets and filesystem

- `Core/GameEngineDevice/Source/StdDevice/Common/StdBIGFileSystem.cpp`
- `Core/GameEngineDevice/Source/StdDevice/Common/StdLocalFileSystem.cpp`
- `scripts/get-assets.sh`
- `scripts/legacy/compat/extract_big_entry.py`
- `安装将军正版资源.command`

## Closing status

The local fork has moved beyond a launch-only port: it now contains a coherent
macOS presentation layer, independent cadence control, a usable high-DPI window
model, localized in-engine controls, packaging, and several renderer/input
correctness fixes discovered through real play. The next phase is not more
feature work. It is source hygiene: preserve the current state, remove private
and machine-specific material, reconcile stale launcher behavior, split the
changes into reviewable commits, and run the clean-build validation matrix.
