#!/usr/bin/env bash
#
# GeneralsX @feature 27/07/2026 Build the double-clickable Zero Hour app bundle.
#
# The bundle that shipped on this machine was assembled by hand, so it went stale the moment the
# engine was rebuilt: it still carried a July binary while the deploy directory had a current one,
# and nothing in the repository could reproduce it. This script replaces that hand assembly.
#
# It is deliberately separate from bundle-macos-zh.sh. That script builds a portable release bundle;
# this one builds the local Chinese-named launcher a player double-clicks and bakes in the path to
# user-supplied assets. The path is selected by --game-dir or environment variables, never by a
# machine-specific source-code constant.
#
# Usage:
#   scripts/build/macos/package-macos-zh-app.sh [--game-dir <dir>] [--generals-dir <dir>]
#       [--runtime <dir>]
#       [--engine <file>] [--icon <png-or-icns>] [--output <path.app>] [--sign <id>]
#
# Defaults put the bundle next to the repository, which is where the existing one lives.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

BUNDLE_NAME="将军：零点行动"
OUTPUT_APP="$(cd "${REPO_ROOT}/.." && pwd)/${BUNDLE_NAME}.app"
GAME_DIR="${GX_GAME_DIRECTORY:-}"
GENERALS_DIR="${GX_GENERALS_DIRECTORY:-}"
RUNTIME_DIR=""
ENGINE_BINARY=""
SIGN_IDENTITY="-"		# ad-hoc; enough for local Gatekeeper, no developer account needed
ICON_SRC="${GX_APP_ICON:-${REPO_ROOT}/assets/generalsx-zh_icon.png}"

while [[ $# -gt 0 ]]; do
	case "$1" in
		--output) OUTPUT_APP="$2"; shift 2 ;;
		--game-dir) GAME_DIR="$2"; shift 2 ;;
		--generals-dir) GENERALS_DIR="$2"; shift 2 ;;
		--runtime) RUNTIME_DIR="$2"; shift 2 ;;
		--engine) ENGINE_BINARY="$2"; shift 2 ;;
		--icon) ICON_SRC="$2"; shift 2 ;;
		--sign) SIGN_IDENTITY="$2"; shift 2 ;;
		-h|--help) sed -n '2,34p' "${BASH_SOURCE[0]}"; exit 0 ;;
		*) echo "error: unknown option $1" >&2; exit 1 ;;
	esac
done

if [[ -z "${GAME_DIR}" ]]; then
	if [[ -n "${GX_RUNTIME_ROOT:-}" ]]; then
		GAME_DIR="${GX_RUNTIME_ROOT}/GeneralsZH"
	elif [[ -f "${REPO_ROOT}/游戏文件/GeneralsZH/INIZH.big" ]]; then
		GAME_DIR="${REPO_ROOT}/游戏文件/GeneralsZH"
	else
		GAME_DIR="${HOME}/GeneralsX/GeneralsZH"
	fi
fi

if [[ -z "${GENERALS_DIR}" ]]; then
	if [[ -f "$(dirname "${GAME_DIR}")/Generals/INI.big" ]]; then
		GENERALS_DIR="$(dirname "${GAME_DIR}")/Generals"
	elif [[ -n "${GX_RUNTIME_ROOT:-}" ]]; then
		GENERALS_DIR="${GX_RUNTIME_ROOT}/Generals"
	else
		GENERALS_DIR="${HOME}/GeneralsX/Generals"
	fi
fi

# deploy-macos-zh.sh places the engine runtime beside the assets. --runtime remains available when
# a developer intentionally keeps build products and retail data in separate directories.
RUNTIME_DIR="${RUNTIME_DIR:-${GAME_DIR}}"
LAUNCHER_SRC="${REPO_ROOT}/packaging/macos/GeneralsXLauncher.m"
ICON_FILENAME="GeneralsXZH.${ICON_SRC##*.}"

# Prefer the freshly built binary over the deployed copy, so packaging right after a build picks up
# that build even if deploy has not been run again.
if [[ -z "${ENGINE_BINARY}" ]]; then
	for candidate in \
		"${REPO_ROOT}/build/macos-vulkan/GeneralsMD/GeneralsXZH" \
		"${RUNTIME_DIR}/GeneralsXZH"
	do
		if [[ -x "${candidate}" ]]; then ENGINE_BINARY="${candidate}"; break; fi
	done
fi

for required in "${LAUNCHER_SRC}" "${ICON_SRC}" "${ENGINE_BINARY}"; do
	if [[ -z "${required}" || ! -e "${required}" ]]; then
		echo "error: missing required input: ${required:-<engine binary>}" >&2
		echo "       build first: cmake --build build/macos-vulkan --target z_generals -j8" >&2
		exit 1
	fi
done
if [[ ! -d "${RUNTIME_DIR}" ]]; then
	echo "error: runtime directory not found: ${RUNTIME_DIR}" >&2
	echo "       deploy first: ./scripts/build/macos/deploy-macos-zh.sh" >&2
	exit 1
fi
if [[ ! -f "${GAME_DIR}/INIZH.big" ]]; then
	echo "error: no Zero Hour assets at ${GAME_DIR} (INIZH.big not found)" >&2
	exit 1
fi
if [[ ! -f "${GENERALS_DIR}/INI.big" ]]; then
	echo "error: no base Generals assets at ${GENERALS_DIR} (INI.big not found)" >&2
	exit 1
fi

echo "==> Packaging ${BUNDLE_NAME}.app"
echo "    engine : ${ENGINE_BINARY}"
echo "    runtime: ${RUNTIME_DIR}"
echo "    ZH data: ${GAME_DIR}"
echo "    base   : ${GENERALS_DIR}"
echo "    output : ${OUTPUT_APP}"

# Stage in a sibling directory and swap at the end, so an interrupted run cannot leave a half-built
# bundle where a working one used to be.
STAGING="$(mktemp -d "${TMPDIR:-/tmp}/gx-app.XXXXXX")"
trap 'rm -rf "${STAGING}"' EXIT
CONTENTS="${STAGING}/${BUNDLE_NAME}.app/Contents"
mkdir -p "${CONTENTS}/MacOS" "${CONTENTS}/Frameworks" "${CONTENTS}/Resources"

echo "==> Compiling launcher"
clang -arch arm64 -mmacosx-version-min=13.0 -fobjc-arc -O2 \
	-Wall -Wextra -Wno-unused-parameter \
	-DGX_GAME_DIRECTORY="\"${GAME_DIR}\"" \
	-DGX_GENERALS_DIRECTORY="\"${GENERALS_DIR}\"" \
	-framework Cocoa \
	-o "${CONTENTS}/MacOS/GeneralsXLauncher" \
	"${LAUNCHER_SRC}"

echo "==> Copying engine and runtime libraries"
cp "${ENGINE_BINARY}" "${CONTENTS}/MacOS/GeneralsXZH"
chmod +x "${CONTENTS}/MacOS/GeneralsXZH"

# -a preserves the dylib symlinks (libSDL3.dylib -> libSDL3.0.dylib and friends); the engine links
# against the unversioned names, so flattening them into copies would double the bundle size for
# nothing and dlopen would still work only by accident.
shopt -s nullglob
for lib in "${RUNTIME_DIR}"/*.dylib; do
	cp -a "${lib}" "${CONTENTS}/Frameworks/"
done
shopt -u nullglob
# The ICD manifest goes in Resources, not Frameworks: codesign treats every file in Frameworks as
# nested code and fails the bundle over a JSON file it cannot sign. Its library_path is resolved
# relative to the manifest, so it is rewritten to reach back across into Frameworks.
cat > "${CONTENTS}/Resources/MoltenVK_icd.json" <<'ICD'
{
    "file_format_version": "1.0.0",
    "ICD": {
        "library_path": "../Frameworks/libMoltenVK.dylib",
        "api_version": "1.4.0",
        "is_portability_driver": true
    }
}
ICD

cp "${ICON_SRC}" "${CONTENTS}/Resources/${ICON_FILENAME}"

echo "==> Writing Info.plist"
# CFBundleExecutable is the launcher, not the engine. The previous hand-built bundle named the engine
# directly, which meant a double-click ran it with none of DYLD_LIBRARY_PATH, the Vulkan ICD, the
# asset root or the cadence variables set -- the launcher was present but unreachable except from a
# terminal. Everything the game needs to start is established by that launcher before it execs the
# engine, so it has to be the entry point.
cat > "${CONTENTS}/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key><string>zh_CN</string>
	<key>CFBundleDisplayName</key><string>${BUNDLE_NAME}</string>
	<key>CFBundleName</key><string>${BUNDLE_NAME}</string>
	<key>CFBundleExecutable</key><string>GeneralsXLauncher</string>
	<key>CFBundleIconFile</key><string>${ICON_FILENAME}</string>
	<key>CFBundleIdentifier</key><string>local.generalsx.zerohour</string>
	<key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
	<key>CFBundlePackageType</key><string>APPL</string>
	<key>CFBundleShortVersionString</key><string>1.0</string>
	<key>CFBundleVersion</key><string>$(date +%Y%m%d)</string>
	<key>LSMinimumSystemVersion</key><string>13.0</string>
	<key>NSHighResolutionCapable</key><true/>
	<key>NSHumanReadableCopyright</key><string>Modified GeneralsX launcher; game assets remain user supplied.</string>
	<key>NSRemovableVolumesUsageDescription</key><string>读取外接盘中的《将军：零点行动》正版资源与原生游戏文件。</string>
</dict>
</plist>
PLIST

echo "==> Signing (${SIGN_IDENTITY})"
# Inside out: nested code has to be sealed before the bundle that contains it, or the outer signature
# is computed over unsigned contents and macOS rejects the lot.
shopt -s nullglob
for lib in "${CONTENTS}/Frameworks"/*.dylib; do
	[[ -L "${lib}" ]] && continue		# symlinks are covered by their target
	codesign --force --timestamp=none --sign "${SIGN_IDENTITY}" "${lib}" >/dev/null 2>&1 || true
done
shopt -u nullglob
codesign --force --timestamp=none --sign "${SIGN_IDENTITY}" "${CONTENTS}/MacOS/GeneralsXZH" >/dev/null 2>&1 || true
codesign --force --timestamp=none --sign "${SIGN_IDENTITY}" "${STAGING}/${BUNDLE_NAME}.app"

echo "==> Verifying"
# --check exercises the launcher's own preflight -- assets present, INIZH.big readable, Frameworks
# populated, engine executable -- and exits before exec'ing the game. Its own output goes to
# ZeroHour.log because the launcher redirects both streams there early; the exit status is what
# matters here. A "sandbox_extension_issue_file_to_process" line from macOS is expected and harmless:
# the staged bundle is in a temporary directory that LaunchServices will not register.
"${CONTENTS}/MacOS/GeneralsXLauncher" --check || {
	echo "error: launcher self-check failed" >&2
	exit 1
}

# Swap only after the new bundle has passed its own check.
if [[ -e "${OUTPUT_APP}" ]]; then
	rm -rf "${OUTPUT_APP}.previous"
	mv "${OUTPUT_APP}" "${OUTPUT_APP}.previous"
fi
mkdir -p "$(dirname "${OUTPUT_APP}")"
mv "${STAGING}/${BUNDLE_NAME}.app" "${OUTPUT_APP}"
rm -rf "${OUTPUT_APP}.previous"

# Finder caches bundle metadata aggressively; without this the new icon and name can take a relaunch
# to appear.
touch "${OUTPUT_APP}"
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister \
	-f "${OUTPUT_APP}" >/dev/null 2>&1 || true

echo "==> Done: ${OUTPUT_APP}"
du -sh "${OUTPUT_APP}" | awk '{print "    size: " $1}'
