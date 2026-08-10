#!/bin/bash
# GeneralsX @build BenderAI 24/02/2026 Deploy macOS build to runtime directory
# Copies GeneralsXZH binary and required dylibs to ~/GeneralsX/GeneralsZH (legacy fallback: ~/GeneralsX/GeneralsMD)

set -e

# GeneralsX @bugfix BenderAI 09/03/2026 Resolve repository root correctly from scripts/build/macos.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/macos-vulkan"
SDL3_LIB_DIR="${BUILD_DIR}/_deps/sdl3-build"
SDL3_IMAGE_LIB_DIR="${BUILD_DIR}/_deps/sdl3_image-build"
OPENAL_LIB_DIR="${BUILD_DIR}/_deps/openal_soft-build"
FONTCONFIG_ETC_DIR="${BUILD_DIR}/vcpkg_installed/arm64-osx/etc/fonts"
GAMESPY_LIB="${BUILD_DIR}/libgamespy.dylib"
# GeneralsX @bugfix BenderAI 09/03/2026 Resolve DXVK dylib paths from both install copy and Meson output to avoid stale runtime libs.
DXVK_D3D8_LIB_INSTALL="${BUILD_DIR}/libdxvk_d3d8.0.dylib"
DXVK_D3D9_LIB_INSTALL="${BUILD_DIR}/libdxvk_d3d9.0.dylib"
DXVK_D3D8_LIB_MESON="${BUILD_DIR}/_deps/dxvk-build-macos/src/d3d8/libdxvk_d3d8.0.dylib"
DXVK_D3D9_LIB_MESON="${BUILD_DIR}/_deps/dxvk-build-macos/src/d3d9/libdxvk_d3d9.0.dylib"
# Local install override: keep runtime and licensed game assets on an external disk.
# Defaults remain fully compatible with upstream when GX_RUNTIME_ROOT is unset.
GX_RUNTIME_ROOT="${GX_RUNTIME_ROOT:-${HOME}/GeneralsX}"
# GeneralsX @bugfix BenderAI 01/04/2026 Align deploy runtime selection with launcher logic by preferring the directory that contains .big assets.
PREFERRED_RUNTIME_DIR="${GX_RUNTIME_ROOT}/GeneralsZH"
LEGACY_RUNTIME_DIR="${GX_RUNTIME_ROOT}/GeneralsMD"
RUNTIME_DIR="${PREFERRED_RUNTIME_DIR}"

if [[ -d "${PREFERRED_RUNTIME_DIR}" && -n "$(compgen -G "${PREFERRED_RUNTIME_DIR}/*.big" 2>/dev/null)" ]]; then
    RUNTIME_DIR="${PREFERRED_RUNTIME_DIR}"
    echo "INFO: Detected Zero Hour assets in ${PREFERRED_RUNTIME_DIR}; deploying there"
elif [[ -d "${LEGACY_RUNTIME_DIR}" && -n "$(compgen -G "${LEGACY_RUNTIME_DIR}/*.big" 2>/dev/null)" ]]; then
    RUNTIME_DIR="${LEGACY_RUNTIME_DIR}"
    echo "INFO: Detected Zero Hour assets in legacy runtime ${LEGACY_RUNTIME_DIR}; deploying there"
elif [[ -d "${PREFERRED_RUNTIME_DIR}" ]]; then
    RUNTIME_DIR="${PREFERRED_RUNTIME_DIR}"
elif [[ -d "${LEGACY_RUNTIME_DIR}" ]]; then
    RUNTIME_DIR="${LEGACY_RUNTIME_DIR}"
    echo "INFO: No .big assets found; using existing legacy runtime ${LEGACY_RUNTIME_DIR}"
fi

# Locate the installed Vulkan SDK: explicit $VULKAN_SDK / $VULKAN_SDK_ROOT
# first (issue #1), then the conventional ~/VulkanSDK/<version>/macOS glob.
_VULKAN_SDK_ENV="${VULKAN_SDK:-}"; _VULKAN_SDK_ROOT_ENV="${VULKAN_SDK_ROOT:-}"
VULKAN_SDK_ROOT=""
# GeneralsX @bugfix 26/07/2026 Also accept a Homebrew-provided loader+MoltenVK. Searching only
# ~/VulkanSDK meant a machine with `brew install vulkan-loader molten-vk` silently deployed no
# Vulkan at all, yet still printed the libvulkan/MoltenVK/ICD paths in the summary as if it had --
# the game then died with "Vulkan Portability library doesn't implement VK_KHR_surface".
_BREW_PREFIX="$(brew --prefix 2>/dev/null || echo /opt/homebrew)"
for sdk_candidate in "${_VULKAN_SDK_ENV}" "${_VULKAN_SDK_ROOT_ENV}" "${HOME}/VulkanSDK"/*/macOS "${_BREW_PREFIX}"; do
    [[ -n "${sdk_candidate}" ]] || continue
    if [[ -f "${sdk_candidate}/lib/libvulkan.dylib" && -f "${sdk_candidate}/lib/libMoltenVK.dylib" ]]; then
        VULKAN_SDK_ROOT="${sdk_candidate}"
        break
    elif [[ -f "${sdk_candidate}/macOS/lib/libvulkan.dylib" ]]; then
        VULKAN_SDK_ROOT="${sdk_candidate}/macOS"
        break
    fi
done
if [[ -z "${VULKAN_SDK_ROOT}" ]]; then
    echo "ERROR: no Vulkan loader + MoltenVK found. Install the LunarG SDK to ~/VulkanSDK," >&2
    echo "       or run: brew install vulkan-loader molten-vk" >&2
    echo "       (override with VULKAN_SDK_ROOT=/path/to/sdk)" >&2
    exit 1
fi
echo "  Using Vulkan from: ${VULKAN_SDK_ROOT}"
BINARY_SRC="${BUILD_DIR}/GeneralsMD/GeneralsXZH"

DXVK_D3D8_LIB="${DXVK_D3D8_LIB_INSTALL}"
DXVK_D3D9_LIB="${DXVK_D3D9_LIB_INSTALL}"
if [[ ! -f "${DXVK_D3D8_LIB}" && -f "${DXVK_D3D8_LIB_MESON}" ]]; then
    DXVK_D3D8_LIB="${DXVK_D3D8_LIB_MESON}"
fi
if [[ ! -f "${DXVK_D3D9_LIB}" && -f "${DXVK_D3D9_LIB_MESON}" ]]; then
    DXVK_D3D9_LIB="${DXVK_D3D9_LIB_MESON}"
fi

echo "Deploying GeneralsXZH (macOS) to ${RUNTIME_DIR}"

if [[ ! -f "${BINARY_SRC}" ]]; then
    echo "ERROR: Binary not found at ${BINARY_SRC}"
    echo "Build first: cmake --build build/macos-vulkan --target z_generals"
    exit 1
fi
if [[ ! -s "${BINARY_SRC}" ]]; then
    echo "ERROR: Binary at ${BINARY_SRC} is empty - build may have failed"
    exit 1
fi

mkdir -p "${RUNTIME_DIR}"

echo "  Copying GeneralsXZH..."
cp -v "${BINARY_SRC}" "${RUNTIME_DIR}/GeneralsXZH"
chmod +x "${RUNTIME_DIR}/GeneralsXZH"

echo "  Copying SDL3 libraries..."
cp -v "${SDL3_LIB_DIR}"/libSDL3.0.dylib "${RUNTIME_DIR}/"
ln -sf libSDL3.0.dylib "${RUNTIME_DIR}/libSDL3.dylib" 2>/dev/null || true
cp -v "${SDL3_IMAGE_LIB_DIR}"/libSDL3_image.0.4.0.dylib "${RUNTIME_DIR}/"
ln -sf libSDL3_image.0.4.0.dylib "${RUNTIME_DIR}/libSDL3_image.0.dylib" 2>/dev/null || true
ln -sf libSDL3_image.0.4.0.dylib "${RUNTIME_DIR}/libSDL3_image.dylib" 2>/dev/null || true

echo "  Copying OpenAL library..."
cp -v "${OPENAL_LIB_DIR}"/libopenal.1.24.2.dylib "${RUNTIME_DIR}/"
ln -sf libopenal.1.24.2.dylib "${RUNTIME_DIR}/libopenal.1.dylib" 2>/dev/null || true
ln -sf libopenal.1.dylib "${RUNTIME_DIR}/libopenal.dylib" 2>/dev/null || true

echo "  Copying GameSpy library..."
cp -v "${GAMESPY_LIB}" "${RUNTIME_DIR}/"

echo "  Copying DXVK libraries (d3d9 + d3d8)..."
# d3d8 links against d3d9 via @rpath — both must be present in the runtime dir
if [[ ! -f "${DXVK_D3D9_LIB}" || ! -f "${DXVK_D3D8_LIB}" ]]; then
    echo "ERROR: Required DXVK dylibs were not found in expected locations:"
    echo "  d3d9 install: ${DXVK_D3D9_LIB_INSTALL}"
    echo "  d3d8 install: ${DXVK_D3D8_LIB_INSTALL}"
    echo "  d3d9 meson:   ${DXVK_D3D9_LIB_MESON}"
    echo "  d3d8 meson:   ${DXVK_D3D8_LIB_MESON}"
    echo "Build DXVK first: cmake --build build/macos-vulkan --target dxvk_d3d8_install"
    exit 1
fi
cp -v "${DXVK_D3D9_LIB}" "${RUNTIME_DIR}/libdxvk_d3d9.0.dylib"
ln -sf libdxvk_d3d9.0.dylib "${RUNTIME_DIR}/libdxvk_d3d9.dylib" 2>/dev/null || true
cp -v "${DXVK_D3D8_LIB}" "${RUNTIME_DIR}/libdxvk_d3d8.0.dylib"
ln -sf libdxvk_d3d8.0.dylib "${RUNTIME_DIR}/libdxvk_d3d8.dylib" 2>/dev/null || true

echo "  Deploying Vulkan + MoltenVK libraries..."
if [[ -n "${VULKAN_SDK_ROOT}" ]]; then
    # GeneralsX @bugfix 26/07/2026 Install instead of plain cp. Homebrew ships these dylibs
    # mode 444, so a second deploy failed with "Permission denied" trying to overwrite the
    # read-only copy left by the first one. install replaces the destination and forces 644.
    # libvulkan is the loader DXVK dlopen's by name; libMoltenVK is the ICD behind it.
    install -m 644 "${VULKAN_SDK_ROOT}/lib/libvulkan.dylib" "${RUNTIME_DIR}/libvulkan.dylib"
    if [[ -f "${VULKAN_SDK_ROOT}/lib/libvulkan.1.dylib" ]]; then
        install -m 644 "${VULKAN_SDK_ROOT}/lib/libvulkan.1.dylib" "${RUNTIME_DIR}/libvulkan.1.dylib"
    fi
    install -m 644 "${VULKAN_SDK_ROOT}/lib/libMoltenVK.dylib" "${RUNTIME_DIR}/libMoltenVK.dylib"
    # Write MoltenVK ICD manifest (Vulkan loader needs VK_ICD_FILENAMES to point here)
    cat > "${RUNTIME_DIR}/MoltenVK_icd.json" <<'EOF'
{
    "file_format_version": "1.0.0",
    "ICD": {
        "library_path": "./libMoltenVK.dylib",
        "api_version": "1.4.0",
        "is_portability_driver": true
    }
}
EOF
    echo "  Vulkan SDK libs deployed from: ${VULKAN_SDK_ROOT}"
else
    echo "WARNING: Vulkan SDK not found (checked \$VULKAN_SDK, \$VULKAN_SDK_ROOT, ~/VulkanSDK/*/macOS)."
    echo "  Install the Vulkan SDK from https://vulkan.lunarg.com/"
    echo "  DXVK will fail to find vkGetInstanceProcAddr at runtime."
fi

# Write wrapper run script that sets DYLD_LIBRARY_PATH at launch time
echo "  Deploying dxvk.conf..."
# GeneralsX @bugfix BenderAI 13/03/2026 Make DXVK config deployment explicit and fail fast.
# Missing dxvk.conf silently caused terrain shader debugging to be misleading on macOS.
DXVK_CONF_SRC="${PROJECT_ROOT}/resources/dxvk/dxvk.conf"
DXVK_CONF_LEGACY_SRC="${PROJECT_ROOT}/GeneralsMD/Run/dxvk.conf"
if [[ -f "${DXVK_CONF_SRC}" ]]; then
    cp -v "${DXVK_CONF_SRC}" "${RUNTIME_DIR}/dxvk.conf"
elif [[ -f "${DXVK_CONF_LEGACY_SRC}" ]]; then
    echo "WARNING: Using legacy DXVK config path: ${DXVK_CONF_LEGACY_SRC}"
    cp -v "${DXVK_CONF_LEGACY_SRC}" "${RUNTIME_DIR}/dxvk.conf"
else
    echo "ERROR: ${DXVK_CONF_SRC} not found."
    echo "       Refusing deploy because DXVK runtime config is required for macOS terrain investigation."
    exit 1
fi

# SagePatch (optional, gated by RTS_BUILD_OPTION_SAGE_PATCH at configure time).
# When the dylib exists, deploy it. The engine auto-creates SagePatch.ini with
# defaults in the user data directory on first run.
SAGE_PATCH_LIB="${BUILD_DIR}/Patches/SagePatch/libsage_patch.dylib"
if [[ -f "${SAGE_PATCH_LIB}" ]]; then
    echo "  Deploying SagePatch (libsage_patch.dylib)..."
    cp -v "${SAGE_PATCH_LIB}" "${RUNTIME_DIR}/"
fi

# GeneralsX @refactor 26/07/2026 The ExtrasMenu overlay no longer needs deploying. It used to ship
# as a loose ExtrasMenu.wnd, which was withheld because that layout's full-screen parent lived on the
# shell stack and could persist into gameplay. The panel is now built programmatically and owns its
# own layout, so there is no game data for it at all.

# GeneralsX @bugfix Copilot 24/03/2026 Deploy Fontconfig config into runtime dir so FreeType/Fontconfig can resolve fonts on macOS.
# GeneralsX @bugfix BenderAI 24/03/2026 Guard Fontconfig conf.d copy so missing directory does not abort deploy under set -e.
echo "  Deploying Fontconfig config..."
if [[ -f "${FONTCONFIG_ETC_DIR}/fonts.conf" ]]; then
    mkdir -p "${RUNTIME_DIR}/fontconfig"
    cp -v "${FONTCONFIG_ETC_DIR}/fonts.conf" "${RUNTIME_DIR}/fontconfig/fonts.conf"
    rm -rf "${RUNTIME_DIR}/fontconfig/conf.d"
    if [[ -d "${FONTCONFIG_ETC_DIR}/conf.d" ]]; then
        cp -R "${FONTCONFIG_ETC_DIR}/conf.d" "${RUNTIME_DIR}/fontconfig/conf.d"
    else
        echo "WARNING: Fontconfig conf.d directory not found at ${FONTCONFIG_ETC_DIR}/conf.d."
        echo "  Runtime may fail to resolve some fonts if per-font configs are missing."
    fi

    # GeneralsX @tweak 10/08/2026 Ship an open-licensed font, never a private one.
    #
    # This used to copy a private Windows SimSun into the runtime, which was a redistribution
    # problem. It was then changed to ship nothing and resolve a Song face from the host, which
    # removed the legal issue but left installs at the mercy of what the host owns. What goes here
    # now is Noto Serif SC under SIL OFL 1.1, fetched below, plus the fontconfig search path so
    # that anything in <runtime>/fonts is visible to fontconfig.
    mkdir -p "${RUNTIME_DIR}/fonts" "${RUNTIME_DIR}/fontconfig/conf.d"
    cat > "${RUNTIME_DIR}/fontconfig/conf.d/99-generalsx-private-fonts.conf" << 'FONTCONF'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <!-- run.sh changes cwd to the runtime directory before launching. -->
  <dir prefix="cwd">fonts</dir>

  <!--
    GeneralsX @bugfix 10/08/2026 macOS keeps several system fonts, PingFang among them, outside
    the directories the bundled fonts.conf knows about. vcpkg's copy lists AssetsV2 nowhere and
    stops at the older Assets/Font3 and Font4, so PingFang SC was invisible to the game while
    being perfectly visible to fc-match run against Homebrew's config. Naming the container
    directory is enough; fontconfig walks into the .asset bundles underneath, which matters
    because the leaf path contains a content hash that changes with every OS update.
  -->
  <dir>/System/Library/AssetsV2/com_apple_MobileAsset_Font8</dir>
  <dir>/System/Library/AssetsV2/com_apple_MobileAsset_Font7</dir>
  <dir>/System/Library/AssetsV2/com_apple_MobileAsset_Font6</dir>
  <dir>/System/Library/Fonts/Supplemental</dir>
</fontconfig>
FONTCONF
    # GeneralsX @feature 10/08/2026 Fetch an open-licensed Song face into the runtime.
    #
    # Relying on the host having a Song font was the weak point of the previous arrangement: the
    # engine's search can come up empty, and Chinese text then falls back to a sans face or to
    # hollow boxes, which is exactly the failure the font work was meant to end. Noto Serif SC is
    # SIL OFL 1.1, so it can be redistributed, and it is fetched rather than committed to keep the
    # repository free of an 11MB binary.
    #
    # Pinned to a commit SHA, not a branch: a moving 'main' would change these bytes under us and
    # break the checksum on some later deploy for no visible reason. The SubsetOTF build is the SC
    # subset (11.6MB) rather than the full CJK OTF (24MB), which still covers every codepoint the
    # Simplified Chinese localization uses.
    #
    # Verified against this exact file: sha256 as below, PANOSE serif style 2, and the three
    # ideographs the engine probes (U+6C49 U+4E2D U+56FD) all present, so it passes the engine's
    # serif and coverage checks rather than merely being named like a Song font.
    GX_SONG_URL="https://raw.githubusercontent.com/notofonts/noto-cjk/9b0f1436e455d902de067a2501422e5dc71ad16b/Serif/SubsetOTF/SC/NotoSerifSC-Regular.otf"
    GX_SONG_SHA256="e8f396decc1f0963a016a989c3d8852e863d1350996f573860a80767c83a1cd3"
    GX_SONG_DEST="${RUNTIME_DIR}/fonts/NotoSerifSC-Regular.otf"

    gx_song_sha_ok() {
        [[ -f "$1" ]] || return 1
        local actual
        actual="$(shasum -a 256 "$1" 2>/dev/null | awk '{print $1}')"
        [[ "${actual}" == "${GX_SONG_SHA256}" ]]
    }

    if gx_song_sha_ok "${GX_SONG_DEST}"; then
        echo "  Chinese font already present and verified (NotoSerifSC-Regular.otf)"
    else
        echo "  Fetching open-licensed Song font (Noto Serif SC, ~11.6MB)..."
        # Download to a temporary name so a partial or corrupt transfer never lands on the real
        # path, where it would be picked up first and rejected at startup as an unusable file.
        gx_song_tmp="${GX_SONG_DEST}.part"
        rm -f "${gx_song_tmp}"
        if curl -fsSL --connect-timeout 20 --max-time 600 --retry 2 --retry-delay 2 \
                -o "${gx_song_tmp}" "${GX_SONG_URL}" 2>/dev/null \
           && gx_song_sha_ok "${gx_song_tmp}"; then
            mv -f "${gx_song_tmp}" "${GX_SONG_DEST}"
            echo "  Chinese font installed and checksum verified"
        else
            # Fallback, deliberately non-fatal. A deploy without network, or behind a proxy that
            # blocks raw.githubusercontent.com, still produces a working install: the engine falls
            # through to the host's own Songti SC, which on macOS is almost always there. Only a
            # machine with no Song font at all ends up degraded, and that is the case this message
            # is written for. Exiting here instead would fail a build over a cosmetic asset.
            if [[ -f "${gx_song_tmp}" ]] && [[ -s "${gx_song_tmp}" ]]; then
                echo "  WARNING: downloaded font failed its checksum; discarding it"
            else
                echo "  WARNING: could not download the Chinese font"
            fi
            rm -f "${gx_song_tmp}"
            echo "  The game will still run and will look for a Song face on this machine."
            echo "  If Chinese text shows as boxes or in a sans face, do one of:"
            echo "    1. Re-run this script once network access is available."
            echo "    2. Download it yourself and save it as:"
            echo "       ${GX_SONG_DEST}"
            echo "       ${GX_SONG_URL}"
            echo "    3. Or put any Song font there as song.otf / simsun.ttc / songti.ttc"
            echo "    4. Or name a family you already have in gx-font.conf, e.g. 'Songti SC'"
        fi
        unset gx_song_tmp
    fi

    cat > "${RUNTIME_DIR}/fonts/README.txt" << FONTREADME
Chinese text font
=================

This directory normally holds NotoSerifSC-Regular.otf, an open-licensed Song
(宋体) face fetched by the deploy script. It is used ahead of any font installed
on this machine, so the Chinese UI looks the same on every install.

Licence: SIL Open Font License 1.1. Source:
${GX_SONG_URL}

If the file is missing, the download failed. The game still runs: it looks for a
Song face on this machine, normally Songti SC on macOS. To fix it properly,
re-run the deploy script, or save the file yourself at the name above.

Two ways to use a different font:

1. Drop a font file in this directory named song.otf, simsun.ttc, simsun.ttf or
   songti.ttc. Anything here is used before any system font. Sans faces are
   rejected on purpose, because files named "simsun" often are not SimSun.

2. Name a font family in gx-font.conf, one directory up. That takes priority
   over this directory and bypasses the serif check, so a sans face such as
   "PingFang SC" is accepted. List the installed families with:
       fc-list :lang=zh family
FONTREADME

    # A deployed choice of font, for this machine only. All lines commented out means "use the Song
    # face installed in fonts/ above", which is the default because Windows Generals renders its
    # Chinese UI in 宋体. Set GX_CJK_FONT at deploy time to write a different family into the runtime.
    if [[ ! -f "${RUNTIME_DIR}/gx-font.conf" || -n "${GX_CJK_FONT:-}" ]]; then
        {
            echo "# Chinese UI font family for this install."
            echo "#"
            echo "# Leave every line commented out to use fonts/NotoSerifSC-Regular.otf, the"
            echo "# open-licensed Song (宋体) face this script installs. Song is the default because"
            echo "# that is what Windows Generals renders its Chinese UI in."
            echo "#"
            echo "# Uncomment one line, or write your own family name. Anything named here wins over"
            echo "# the fonts directory and is used as-is, including sans families. List what is"
            echo "# installed with:"
            echo "#     fc-list :lang=zh family"
            echo "#"
            echo "# PingFang SC          - macOS system sans, cleaner on a HiDPI panel"
            echo "# Songti SC            - macOS system Song, closest to the original look"
            echo "# Source Han Serif SC  - open licensed Song, if installed"
            if [[ -n "${GX_CJK_FONT:-}" ]]; then
                echo "${GX_CJK_FONT}"
            else
                echo "# PingFang SC"
            fi
        } > "${RUNTIME_DIR}/gx-font.conf"
        if [[ -n "${GX_CJK_FONT:-}" ]]; then
            echo "  Chinese UI font pinned to '${GX_CJK_FONT}' in gx-font.conf"
        else
            echo "  Wrote gx-font.conf (uses the installed 宋体 in fonts/; edit to override)"
        fi
    fi
else
    echo "WARNING: Fontconfig config not found at ${FONTCONFIG_ETC_DIR}."
    echo "  Runtime may fail to resolve fonts in Save/Load/Replay menus."
fi

echo "  Writing run.sh wrapper..."
cat > "${RUNTIME_DIR}/run.sh" << WRAPPER
#!/bin/bash
# GeneralsX @build BenderAI 24/02/2026 - macOS wrapper for runtime directory
SCRIPT_DIR="\$(cd "\$(dirname "\$0")" && pwd)"

# SDL3 and gamespy dylibs are in same dir; Vulkan/MoltenVK stays in SDK
export DYLD_LIBRARY_PATH="\${SCRIPT_DIR}:\${DYLD_LIBRARY_PATH:-}"

# SagePatch (optional QoL features). Loaded via DYLD_INSERT_LIBRARIES so it
# can interpose SDL3 functions for hot-keys (F11 screenshot, Scroll Lock cursor
# lock, Ctrl+PageUp/PageDown brightness, Ctrl+1..5 window snap).
if [[ -f "\${SCRIPT_DIR}/libsage_patch.dylib" && "\${SAGE_PATCH_DISABLED:-0}" != "1" ]]; then
    if [[ -n "\${DYLD_INSERT_LIBRARIES:-}" ]]; then
        export DYLD_INSERT_LIBRARIES="\${SCRIPT_DIR}/libsage_patch.dylib:\${DYLD_INSERT_LIBRARIES}"
    else
        export DYLD_INSERT_LIBRARIES="\${SCRIPT_DIR}/libsage_patch.dylib"
    fi
fi

# GeneralsX @bugfix fbraz3 20/03/2026 DXVK requires DXVK_WSI_DRIVER on non-Win32; must match game windowing (SDL3)
export DXVK_WSI_DRIVER="SDL3"

# DXVK HUD: kept opt-in. MoltenVK on macOS 26 cannot compile DXVK's HUD
# pipeline shader (uses gl_DrawID / SPIR-V DrawIndex which has no MSL
# equivalent yet), so defaulting it on breaks the swap chain blit pipeline.
# Users wanting an FPS overlay set DXVK_HUD=fps themselves.
export DXVK_HUD="\${DXVK_HUD:-0}"

# MoltenVK ICD manifest — deployed alongside the binary by deploy-macos-zh.sh
if [[ -f "\${SCRIPT_DIR}/MoltenVK_icd.json" ]]; then
    export VK_ICD_FILENAMES="\${SCRIPT_DIR}/MoltenVK_icd.json"
    # GeneralsX @bugfix fbraz3 20/03/2026 Vulkan Loader 1.3.236+ uses VK_DRIVER_FILES; keep VK_ICD_FILENAMES for older loaders
    export VK_DRIVER_FILES="\${SCRIPT_DIR}/MoltenVK_icd.json"
fi

# GeneralsX @bugfix Copilot 24/03/2026 Set bundled Fontconfig config path to avoid "Cannot load default config file: (null)" on macOS.
if [[ -f "\${SCRIPT_DIR}/fontconfig/fonts.conf" ]]; then
    export FONTCONFIG_FILE="\${SCRIPT_DIR}/fontconfig/fonts.conf"
    export FONTCONFIG_PATH="\${SCRIPT_DIR}/fontconfig"
fi

# GeneralsX @feature 10/08/2026 Chinese UI font, chosen per install.
#
# Unset means use the Song (宋体) face in fonts/, matching Windows Generals. gx-font.conf holds one
# family name, or nothing but comments. An already exported GX_CJK_SERIF_FONT wins, so a single
# run can be tried without editing anything:
#     GX_CJK_SERIF_FONT="PingFang SC" ./run.sh
if [[ -z "\${GX_CJK_SERIF_FONT:-}" && -f "\${SCRIPT_DIR}/gx-font.conf" ]]; then
    _gx_font="\$(grep -v '^[[:space:]]*#' "\${SCRIPT_DIR}/gx-font.conf" | grep -v '^[[:space:]]*\$' | head -n 1)"
    # Trim surrounding whitespace without invoking sed on a possibly multibyte name.
    _gx_font="\${_gx_font#"\${_gx_font%%[![:space:]]*}"}"
    _gx_font="\${_gx_font%"\${_gx_font##*[![:space:]]}"}"
    if [[ -n "\${_gx_font}" ]]; then
        export GX_CJK_SERIF_FONT="\${_gx_font}"
    fi
    unset _gx_font
fi

# Auto-detect base Generals install path
if [[ -z "\${CNC_GENERALS_INSTALLPATH:-}" && -d "\${SCRIPT_DIR}/../Generals" ]]; then
    export CNC_GENERALS_INSTALLPATH="\${SCRIPT_DIR}/../Generals/"
fi

# The engine resolves Local FS lookups (e.g. INI overrides under
# Data/INI/Default/...) relative to the binary's cwd. Without this cd, anything
# launched via absolute path (Finder, gtimeout, full-path invocation) misses
# every loose INI / asset and only sees what is bundled inside the BIG files.
cd "\${SCRIPT_DIR}"

exec "./GeneralsXZH" "\$@"
WRAPPER
chmod +x "${RUNTIME_DIR}/run.sh"

# GeneralsX @build 26/07/2026 Mirror the engine payload into any additional install directories.
# RUNTIME_DIR is the throwaway test install, but a real play install lives elsewhere and has its own
# copy of the binary and dylibs next to the .big assets. Deploying only to RUNTIME_DIR meant the
# desktop launchers kept starting a build that was weeks old, which looks exactly like "the fix did
# not work". Only the files this script produces are mirrored; .big archives, the loose Data tree,
# saves and replays are never touched.
MIRROR_DIRS=()
if [[ -n "${GX_MIRROR_DIRS:-}" ]]; then
    IFS=':' read -r -a _explicit_mirrors <<< "${GX_MIRROR_DIRS}"
    for _m in "${_explicit_mirrors[@]}"; do
        [[ -n "${_m}" ]] && MIRROR_DIRS+=("${_m}")
    done
fi
# Auto-detect the local play install so it never falls behind without anyone remembering a variable.
for _candidate in "${PROJECT_ROOT}"/*/GeneralsZH; do
    [[ -d "${_candidate}" ]] || continue
    [[ "${_candidate}" == "${RUNTIME_DIR}" ]] && continue
    [[ -n "$(compgen -G "${_candidate}/*.big" 2>/dev/null)" ]] || continue
    _already_listed=0
    for _m in "${MIRROR_DIRS[@]:-}"; do
        [[ "${_m}" == "${_candidate}" ]] && _already_listed=1
    done
    (( _already_listed )) || MIRROR_DIRS+=("${_candidate}")
done

MIRRORED_DIRS=()
for MIRROR_DIR in "${MIRROR_DIRS[@]:-}"; do
    [[ -n "${MIRROR_DIR}" ]] || continue
    if [[ ! -d "${MIRROR_DIR}" ]]; then
        echo "WARNING: Mirror target ${MIRROR_DIR} does not exist; skipping."
        continue
    fi
    echo "  Mirroring engine payload to ${MIRROR_DIR}..."
    for payload in GeneralsXZH MoltenVK_icd.json dxvk.conf run.sh; do
        [[ -f "${RUNTIME_DIR}/${payload}" ]] && cp -p "${RUNTIME_DIR}/${payload}" "${MIRROR_DIR}/${payload}"
    done
    for payload in "${RUNTIME_DIR}"/*.dylib; do
        [[ -f "${payload}" ]] && cp -p "${payload}" "${MIRROR_DIR}/"
    done
    for payload_dir in fontconfig fonts; do
        if [[ -d "${RUNTIME_DIR}/${payload_dir}" ]]; then
            rm -rf "${MIRROR_DIR}/${payload_dir}"
            cp -Rp "${RUNTIME_DIR}/${payload_dir}" "${MIRROR_DIR}/${payload_dir}"
        fi
    done
    # GeneralsX @feature 10/08/2026 The font choice travels with the payload, but never silently
    # replaces one the mirror already has: it is a per-install preference, and the play install is
    # where someone would have edited it by hand. Only an explicit GX_CJK_FONT overwrites.
    if [[ -f "${RUNTIME_DIR}/gx-font.conf" ]]; then
        if [[ ! -f "${MIRROR_DIR}/gx-font.conf" || -n "${GX_CJK_FONT:-}" ]]; then
            cp -p "${RUNTIME_DIR}/gx-font.conf" "${MIRROR_DIR}/gx-font.conf"
        fi
    fi
    chmod +x "${MIRROR_DIR}/GeneralsXZH" "${MIRROR_DIR}/run.sh" 2>/dev/null || true
    MIRRORED_DIRS+=("${MIRROR_DIR}")
done

echo ""
echo "Deploy complete"
echo "   Executable: ${RUNTIME_DIR}/GeneralsXZH"
echo "   SDL3 libs:  ${RUNTIME_DIR}/libSDL3*.dylib"
echo "   GameSpy:    ${RUNTIME_DIR}/libgamespy.dylib"
echo "   DXVK d3d9:  ${RUNTIME_DIR}/libdxvk_d3d9.0.dylib"
echo "   DXVK d3d8:  ${RUNTIME_DIR}/libdxvk_d3d8.0.dylib"
echo "   Vulkan:     ${RUNTIME_DIR}/libvulkan.dylib"
echo "   MoltenVK:   ${RUNTIME_DIR}/libMoltenVK.dylib"
echo "   VK ICD:     ${RUNTIME_DIR}/MoltenVK_icd.json"
echo "   DXVK conf:  ${RUNTIME_DIR}/dxvk.conf"
# GeneralsX @bugfix BenderAI 24/03/2026 Show Fontconfig status only when deployed to avoid misleading summary output.
if [[ -f "${RUNTIME_DIR}/fontconfig/fonts.conf" ]]; then
    echo "   Fontconfig: ${RUNTIME_DIR}/fontconfig/fonts.conf"
else
    echo "   Fontconfig: (not deployed)"
fi
echo "   Wrapper:    ${RUNTIME_DIR}/run.sh"
for MIRROR_DIR in "${MIRRORED_DIRS[@]:-}"; do
    [[ -n "${MIRROR_DIR}" ]] && echo "   Mirrored:   ${MIRROR_DIR}"
done
# GeneralsX @feature 27/07/2026 Refresh the double-clickable app bundle as part of deploy.
#
# The hand-built bundle went stale precisely because refreshing it was a separate manual step nobody
# remembered: it sat on a July binary while this directory had a current one, which is a confusing
# way to test -- the same "build" behaves differently depending on how it was started. Deploy is the
# point where a new binary becomes the one in use, so it is the right place to keep the app in sync.
#
# Opt out with GX_SKIP_APP_BUNDLE=1. Failure here is reported but does not fail the deploy: the
# command-line path above is complete and usable on its own.
if [[ "${GX_SKIP_APP_BUNDLE:-0}" != "1" ]]; then
    APP_PACKAGER="${PROJECT_ROOT}/scripts/build/macos/package-macos-zh-app.sh"
    if [[ -x "${APP_PACKAGER}" ]]; then
        echo ""
        echo "Refreshing app bundle"
        if "${APP_PACKAGER}" --runtime "${RUNTIME_DIR}" 2>&1 | sed 's/^/   /'; then
            :
        else
            echo "   WARNING: app bundle refresh failed; the command-line deploy above is unaffected"
        fi
    fi
fi

echo ""
echo "Run with:"
echo "  ${PROJECT_ROOT}/scripts/build/macos/run-macos-zh.sh -win"
echo "  or: cd ~/GeneralsX/GeneralsZH && ./run.sh -win"
echo "  or: double-click 将军：零点行动.app (set GX_SKIP_APP_BUNDLE=1 to skip refreshing it)"
