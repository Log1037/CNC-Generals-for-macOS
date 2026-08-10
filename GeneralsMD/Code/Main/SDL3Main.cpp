/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
** SDL3Main.cpp
**
** Entry point for Linux builds using SDL3 windowing and DXVK graphics.
**
** TheSuperHackers @feature CnC_Generals_Linux 07/02/2026
** Entry point replaces WinMain() for Linux builds.
** Instantiates SDL3GameEngine and calls GameMain().
*/

#ifndef _WIN32

// SYSTEM INCLUDES
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
// On iOS, SDL renames main() to SDL_main and provides its own UIApplicationMain
// bootstrap; the app lifecycle (suspend/resume, window) is owned by SDL.
#include <SDL3/SDL_main.h>
#include <filesystem>
#include <string>
#endif
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <unistd.h>   // _exit()
#include <glob.h>     // glob() for Vulkan ICD discovery
#include <pthread.h>  // shutdown watchdog thread
#include <signal.h>   // kill() from the shutdown watchdog
#include <ctime>      // nanosleep() in the shutdown watchdog
#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
// Defined in MacDisplayKick.cpp, which cannot be included from here: <CoreGraphics/CoreGraphics.h>
// pulls in <MacTypes.h>, whose "typedef UInt8 Byte" is a hard conflict with the engine's own
// "typedef char Byte" in BaseTypeCore.h. Declaring the one symbol keeps the two apart.
extern "C" void GXKickDisplayConfiguration(void);
extern "C" bool GXGetPanelNativePixelSize(int* outWidth, int* outHeight);
#endif

// USER INCLUDES (match WinMain.cpp pattern)
#include "Lib/BaseType.h"
#include "Common/CommandLine.h"
#include "Common/CriticalSection.h"
#include "Common/GlobalData.h"
#include "Common/GameEngine.h"
#include "Common/GameMemory.h"
#include "Common/Debug.h"
#include "Common/version.h"  // GeneralsX @bugfix BenderAI 14/02/2026 Version class + TheVersion extern
#include "SDL3GameEngine.h"

// DXVK WSI
#define DXVK_WSI_SDL3 1
#include <wsi/native_wsi.h>

// CRITICAL SECTIONS (Linux needs these too)
static CriticalSection critSec1;
static CriticalSection critSec2;
static CriticalSection critSec3;
static CriticalSection critSec4;
static CriticalSection critSec5;

// GLOBAL COMMAND LINE ARGUMENTS
// TheSuperHackers @build felipebraz 13/02/2026
// Store argc/argv from main() for use by CommandLine.cpp parseCommandLine() on Linux
// Windows provides these automatically; Linux needs explicit globals
int __argc = 0;          ///< global argument count
char** __argv = nullptr; ///< global argument vector

// GLOBAL WINDOW HANDLE
// TheSuperHackers @build felipebraz 13/02/2026
// ApplicationHWnd is declared extern in GeneralsMD/Code/Main/WinMain.h
// On Linux, we cast SDL_Window* to HWND type for compatibility
HWND ApplicationHWnd = nullptr;  ///< our application window handle

// GLOBAL SDL3 WINDOW
// GeneralsX @feature felipebraz 16/02/2026
// SDL3 window created in main() before GameMain(), stored globally for engine access
SDL_Window* TheSDL3Window = nullptr;

// GAME TEXT FILE PATHS
// TheSuperHackers @build felipebraz 13/02/2026
// GameText.cpp uses these paths to load CSF and STR files (game localization)
// Format %s is replaced with language code in GameTextManager::init()
// GeneralsX @bugfix BenderAI 13/02/2026 - Fix case-sensitivity on Linux (generals.csf vs Generals.csf)
const Char *g_csfFile = "data/%s/generals.csf";  ///< CSF file path (lowercase for Linux compatibility)
const Char *g_strFile = "data/Generals.str";     ///< STR file path

// Extern declarations (from GameMain.cpp)
extern Int GameMain();

#if !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
// GeneralsX @bugfix 02/08/2026 A hung shutdown must never be able to hold the screen.
//
// Observed once for real: the engine logged "exited main loop", "GameMain() returned with code 0"
// and "Exiting with code 0", so the process reached _exit() -- yet the fullscreen picture stayed on
// screen for three and a half minutes until the machine was restarted by hand. Force Quit could not
// offer the game because by then there was no game process left to offer, and Cmd-Tab was equally
// useless: what remained in front was the fullscreen Space, not a window belonging to anything.
//
// The mechanism is not what it first looks like, and the first two attempts at a fix both missed it.
// SDL_DestroyWindow does not unwind a fullscreen *desktop* window's Space: SDL_video.c bails out of
// SDL_UpdateFullscreenMode early when is_destroying is set, deliberately, to avoid a double
// transition. So the Space is never left mid-transition -- it is never asked to leave at all, and
// what actually happens is that the only window inside it is closed and the process exits. The Space
// survives both, with nothing left inside it and nobody left owning it.
//
// That makes the leaving of fullscreen the engine's job, early, while its window is still key and the
// app still frontmost -- see SDL3GameEngine::leaveFullscreenForShutdown. Two things here back it up:
// the guard in the teardown below, which would rather exit with an intact fullscreen window than
// close one inside a live Space, and this watchdog, which bounds the whole teardown so a wedge in
// DXVK's worker joins, MoltenVK/Metal or the memory manager cannot leave the process sitting mid-exit
// owning the screen.
//
// Known limit: the watchdog only covers userspace deadlock. A thread stuck in an uninterruptible
// kernel wait cannot be signalled away.
//
// Update 05/08/2026: fullscreen is no longer a macOS Space by default -- see the
// SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES block before SDL_InitSubSystem. A borderless window cannot be
// orphaned the way a Space can, which removes the failure mode this watchdog was fighting rather than
// only bounding it. The watchdog stays because it also covers a wedge in DXVK, MoltenVK/Metal or the
// memory manager, and because GX_MAC_FULLSCREEN_SPACES=1 can still put a Space back.
static void* GXShutdownWatchdogThread(void* arg)
{
	const double seconds = *static_cast<double*>(arg);
	delete static_cast<double*>(arg);

	struct timespec remaining;
	remaining.tv_sec = static_cast<time_t>(seconds);
	remaining.tv_nsec = static_cast<long>((seconds - static_cast<double>(remaining.tv_sec)) * 1e9);
	while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {
		// Restart the wait with whatever is left; a signal must not shorten the grace period.
	}

	fprintf(stderr, "FATAL: shutdown watchdog expired after %.1fs -- killing the process so the "
		"display cannot stay captured\n", seconds);
	fflush(stderr);
	kill(getpid(), SIGKILL);
	return nullptr;
}

/// Start a detached thread that SIGKILLs this process if teardown has not finished in time.
/// Set GX_EXIT_WATCHDOG_SECONDS=0 to disable (useful when debugging shutdown under lldb).
static void GXArmShutdownWatchdog(void)
{
	double seconds = 10.0;
	if (const char* override = getenv("GX_EXIT_WATCHDOG_SECONDS")) {
		char* end = nullptr;
		const double parsed = strtod(override, &end);
		if (end != override && parsed >= 0.0) {
			seconds = parsed;
		}
	}
	if (seconds <= 0.0) {
		fprintf(stderr, "INFO: shutdown watchdog disabled by GX_EXIT_WATCHDOG_SECONDS\n");
		return;
	}

	pthread_attr_t attr;
	if (pthread_attr_init(&attr) != 0) {
		return;
	}
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_t thread;
	double* payload = new double(seconds);
	if (pthread_create(&thread, &attr, GXShutdownWatchdogThread, payload) != 0) {
		delete payload;
		fprintf(stderr, "WARNING: could not start the shutdown watchdog\n");
	}
	else {
		fprintf(stderr, "INFO: shutdown watchdog armed (%.1fs)\n", seconds);
	}
	pthread_attr_destroy(&attr);
}

#endif // !TARGET_OS_IPHONE

#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
// GeneralsX @feature 26/07/2026 Render scale as a percentage of the display's physical pixel
// size; see the GXRenderScalePercent read in ConfigureMacOSAppRuntime.
static Int s_macRenderScalePercent = 100;

static Int ReadMacOSIntegerOption(const char* path, const char* requestedKey,
	Int fallback, Int minimum, Int maximum)
{
	FILE* file = fopen(path, "r");
	if (file == nullptr) {
		return fallback;
	}

	char line[2048] = { 0 };
	while (fgets(line, sizeof(line), file) != nullptr) {
		char* equals = strchr(line, '=');
		if (equals == nullptr) {
			continue;
		}
		*equals = '\0';

		char* key = line;
		while (*key != '\0' && std::isspace(static_cast<unsigned char>(*key))) {
			++key;
		}
		char* keyEnd = key + strlen(key);
		while (keyEnd > key && std::isspace(static_cast<unsigned char>(keyEnd[-1]))) {
			*--keyEnd = '\0';
		}
		if (strcasecmp(key, requestedKey) != 0) {
			continue;
		}

		char* valueText = equals + 1;
		while (*valueText != '\0' && std::isspace(static_cast<unsigned char>(*valueText))) {
			++valueText;
		}
		const Int value = atoi(valueText);
		fclose(file);
		return std::max(minimum, std::min(maximum, value));
	}

	fclose(file);
	return fallback;
}

static void ConfigureMacOSAppRuntime(int& argc, char**& argv)
{
	const char* home = getenv("HOME");
	if (home != nullptr) {
		char logDirectory[4096] = { 0 };
		char logPath[4096] = { 0 };
		snprintf(logDirectory, sizeof(logDirectory), "%s/Library/Logs/GeneralsX", home);
		mkdir(logDirectory, 0755);
		snprintf(logPath, sizeof(logPath), "%s/ZeroHour.log", logDirectory);
		const int logFd = open(logPath, O_WRONLY | O_CREAT | O_APPEND, 0644);
		if (logFd >= 0) {
			dup2(logFd, STDOUT_FILENO);
			dup2(logFd, STDERR_FILENO);
			close(logFd);
		}
	}

	// GeneralsX @bugfix 10/08/2026 Derive optional standalone asset paths from GX_RUNTIME_ROOT.
	// Public builds must not contain a developer's external-volume path. The Cocoa launcher and
	// run.sh normally set CNC_* explicitly; this remains a portable fallback for direct execution.
	const char* runtimeRoot = getenv("GX_RUNTIME_ROOT");
	if (runtimeRoot != nullptr && runtimeRoot[0] != '\0') {
		static char zeroHourAssetsBuffer[4096] = { 0 };
		static char generalsAssetsBuffer[4096] = { 0 };
		snprintf(zeroHourAssetsBuffer, sizeof(zeroHourAssetsBuffer), "%s/GeneralsZH", runtimeRoot);
		snprintf(generalsAssetsBuffer, sizeof(generalsAssetsBuffer), "%s/Generals", runtimeRoot);
		if (getenv("CNC_GENERALS_ZH_PATH") == nullptr) {
			setenv("CNC_GENERALS_ZH_PATH", zeroHourAssetsBuffer, 1);
		}
		if (getenv("CNC_GENERALS_PATH") == nullptr) {
			setenv("CNC_GENERALS_PATH", generalsAssetsBuffer, 1);
		}
		if (getenv("CNC_GENERALS_INSTALLPATH") == nullptr) {
			setenv("CNC_GENERALS_INSTALLPATH", generalsAssetsBuffer, 1);
		}
	}

	const char* zeroHourAssets = getenv("CNC_GENERALS_ZH_PATH");
	if (zeroHourAssets != nullptr && chdir(zeroHourAssets) != 0) {
		fprintf(stderr, "WARNING: macOS app could not enter asset directory '%s': %s\n",
			zeroHourAssets, strerror(errno));
	}

	char contentsDirectory[4096] = { 0 };
	if (argc > 0 && argv[0] != nullptr) {
		strncpy(contentsDirectory, argv[0], sizeof(contentsDirectory) - 1);
		char* slash = strrchr(contentsDirectory, '/');
		if (slash != nullptr) {
			*slash = '\0'; // Contents/MacOS
			slash = strrchr(contentsDirectory, '/');
			if (slash != nullptr) {
				*slash = '\0'; // Contents
			}
		}
	}
	if (contentsDirectory[0] != '\0') {
		char icdPath[4096] = { 0 };
		snprintf(icdPath, sizeof(icdPath), "%s/Frameworks/MoltenVK_icd.json", contentsDirectory);
		if (getenv("VK_ICD_FILENAMES") == nullptr) {
			setenv("VK_ICD_FILENAMES", icdPath, 1);
		}
		if (getenv("VK_DRIVER_FILES") == nullptr) {
			setenv("VK_DRIVER_FILES", icdPath, 1);
		}
	}

	if (zeroHourAssets != nullptr) {
		char fontConfig[4096] = { 0 };
		char fontPath[4096] = { 0 };
		snprintf(fontConfig, sizeof(fontConfig), "%s/fontconfig/fonts.conf", zeroHourAssets);
		snprintf(fontPath, sizeof(fontPath), "%s/fontconfig", zeroHourAssets);
		if (getenv("FONTCONFIG_FILE") == nullptr) {
			setenv("FONTCONFIG_FILE", fontConfig, 1);
		}
		if (getenv("FONTCONFIG_PATH") == nullptr) {
			setenv("FONTCONFIG_PATH", fontPath, 1);
		}
	}

	setenv("DXVK_WSI_DRIVER", "SDL3", 0);
	setenv("DXVK_HUD", "0", 0);

	Int renderFps = 60;
	Int speedTenths = 10;
	if (home != nullptr) {
		char optionsPath[4096] = { 0 };
		snprintf(optionsPath, sizeof(optionsPath),
			"%s/Library/Application Support/GeneralsX/GeneralsZH/Options.ini", home);
		renderFps = ReadMacOSIntegerOption(optionsPath, "GXRenderFPS", 60, 30, 240);
		speedTenths = ReadMacOSIntegerOption(optionsPath, "GXGameSpeedTenths", 10, 5, 60);
		// GeneralsX @feature 26/07/2026 Percentage of the display's physical pixel size to render
		// at. 100 is native HiDPI. Rendering ~5 Mpix through DXVK's emulated fixed-function
		// pipeline is far more expensive than the 1024x768 this engine shipped for, so this exists
		// as a sharpness/performance dial: values below 100 render smaller and let the existing
		// pillarbox blit upscale, which is strictly better than being forced back to a blurry
		// point-sized backbuffer. Read here so it applies before the first D3D device is made.
		s_macRenderScalePercent = ReadMacOSIntegerOption(optionsPath, "GXRenderScalePercent", 100, 50, 100);

		// GeneralsX @feature 27/07/2026 Hand the percentage to W3DDisplay as well. The windowed
		// sizing rule needs it to convert the render resolution into window points, and the in-game
		// clarity switch reads it back, so both paths have to share one value rather than each
		// keeping its own copy of a preference that can change at runtime.
		{
			extern void GeneralsX_SetRenderScalePercent(int percent);
			GeneralsX_SetRenderScalePercent(s_macRenderScalePercent);
		}
	}
	const Int logicFps = (speedTenths * 30 + 5) / 10;
	// GeneralsX @bugfix 26/07/2026 The render cap is no longer raised to meet the logic rate. The
	// fixed-step accumulator in the main loop runs as many logic steps per rendered frame as the
	// ratio requires, so a fast simulation does not need a matching frame rate, and forcing one
	// silently discarded whatever cap the user had asked for.
	if (getenv("GX_RENDER_FPS") == nullptr) {
		char value[16] = { 0 };
		snprintf(value, sizeof(value), "%d", renderFps);
		setenv("GX_RENDER_FPS", value, 1);
	}
	if (getenv("GX_LOGIC_FPS") == nullptr) {
		char value[16] = { 0 };
		snprintf(value, sizeof(value), "%d", logicFps);
		setenv("GX_LOGIC_FPS", value, 1);
	}
	// GeneralsX @refactor 26/07/2026 GX_CINEMATIC_LOGIC_FPS is gone. Nothing ever consumed it: a
	// separate cutscene logic rate was never wired into the frame pacer, so the option only looked
	// like it worked. Scripted sequences now follow the one game speed setting like everything else,
	// which is also what makes them respond to the speed slider at last.

	bool hasWindowMode = false;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-fullscreen") == 0 || strcmp(argv[i], "-win") == 0) {
			hasWindowMode = true;
			break;
		}
	}
	if (!hasWindowMode && argc < 63) {
		static char fullscreenFlag[] = "-fullscreen";
		static char* appArgv[64] = { nullptr };
		for (int i = 0; i < argc; ++i) {
			appArgv[i] = argv[i];
		}
		appArgv[argc++] = fullscreenFlag;
		appArgv[argc] = nullptr;
		argv = appArgv;
	}

	fprintf(stderr,
		"\nNative macOS app launch: assets='%s' render=%d logic=%d speed=%.2fx\n",
		zeroHourAssets != nullptr ? zeroHourAssets : "(unset)",
		renderFps,
		logicFps,
		static_cast<double>(logicFps) / 30.0);
}
#endif

/**
 * FilterSoftwareVulkanICDs
 *
 * Sets VK_DRIVER_FILES to only hardware Vulkan ICDs, excluding LLVMpipe/lavapipe.
 *
 * Workaround for Mesa/LLVM 20.x bug: libvulkan_lvp.so (LLVMpipe Vulkan ICD) crashes
 * during dlopen() static initialization with a null-ptr deref in llvm::Regex::Regex().
 * The Vulkan loader loads ALL ICDs found in the ICD directories when
 * vkEnumerateInstanceExtensionProperties() is called, which triggers the crash.
 * Filtering hardware-only ICDs via VK_DRIVER_FILES prevents loading libvulkan_lvp.so.
 *
 * Only applied when neither VK_DRIVER_FILES nor VK_ICD_FILENAMES is already set,
 * so the user can always override by setting those variables externally.
 *
 * GeneralsX @bugfix BenderAI 06/03/2026
 */
static void FilterSoftwareVulkanICDs()
{
	if (getenv("VK_DRIVER_FILES") || getenv("VK_ICD_FILENAMES")) {
		return;
	}

	auto icd_is_software = [](const char *name) -> bool {
		char low[256] = "";
		for (int i = 0; name[i] && i < 255; ++i) {
			low[i] = (char)tolower((unsigned char)name[i]);
		}
		return strstr(low, "lvp") || strstr(low, "lavapipe") || strstr(low, "softpipe") || strstr(low, "llvmpipe");
	};

	static char hw_icds[4096] = "";
	const char *patterns[] = {
		"/usr/share/vulkan/icd.d/*.json",
		"/etc/vulkan/icd.d/*.json",
		nullptr
	};

	glob_t gl = {};
	int gflags = 0;
	for (int i = 0; patterns[i]; ++i) {
		if (glob(patterns[i], gflags, nullptr, &gl) == 0) {
			gflags = GLOB_APPEND;
		}
	}

	bool found_hw = false;
	for (size_t i = 0; i < gl.gl_pathc; ++i) {
		const char *path = gl.gl_pathv[i];
		const char *base = strrchr(path, '/');
		base = base ? base + 1 : path;
		if (icd_is_software(base)) {
			fprintf(stderr, "INFO: Vulkan ICD filter: skipping software ICD '%s'\n", base);
			continue;
		}
		if (found_hw) {
			strncat(hw_icds, ":", sizeof(hw_icds) - strlen(hw_icds) - 1);
		}
		strncat(hw_icds, path, sizeof(hw_icds) - strlen(hw_icds) - 1);
		found_hw = true;
	}
	globfree(&gl);

	if (found_hw) {
		setenv("VK_DRIVER_FILES", hw_icds, 1);
		fprintf(stderr, "INFO: Vulkan ICD filter: VK_DRIVER_FILES=%s\n", hw_icds);
	} else {
		fprintf(stderr, "WARNING: Vulkan ICD filter: no hardware ICDs found, LLVMpipe exclusion skipped\n");
		fprintf(stderr, "WARNING: If startup crashes in libvulkan_lvp.so, set VK_DRIVER_FILES manually\n");
	}
}

/**
 * FilterPipeWireOpenAL
 *
 * Sets ALSOFT_DRIVERS to skip PipeWire, falling back to pulse/alsa.
 *
 * Workaround for openal-soft PipeWire backend crash: alcOpenDevice() segfaults
 * inside the PipeWire backend while opening the default playback device.
 * The crash occurs in PipeWire's stream/context internals and is unrecoverable
 * from userspace. Excluding PipeWire via ALSOFT_DRIVERS causes openal-soft to
 * fall back to the PulseAudio backend, which works correctly on PipeWire systems
 * via the PulseAudio compatibility layer.
 *
 * NOTE: openal-soft reads ALSOFT_DRIVERS from a static global constructor when
 * libopenal.so is loaded by the dynamic linker, which is before main() runs.
 * This function is therefore only effective for builds that use lazy
 * initialization. The authoritative fix is in the launch scripts (run-linux-zh.sh
 * etc.), which set ALSOFT_DRIVERS before the binary starts.
 *
 * Only applied when ALSOFT_DRIVERS is not already set by the user.
 *
 * GeneralsX @bugfix 09/03/2026
 */
static void FilterPipeWireOpenAL()
{
	// GeneralsX @bugfix Copilot 24/03/2026 PipeWire/OpenAL workaround is Linux-only; keep macOS CoreAudio backend selection untouched.
	#if defined(__linux__)
	// Crash: alcOpenDevice() hits 'movaps %xmm1,0x26260(%rbx)' — SSE movaps requires
	// 16-byte alignment; a misaligned ALCdevice struct faults regardless of backend.
	// Disabling CPU extensions forces openal-soft to use scalar code that has no
	// alignment requirements. Also exclude pipewire which has its own crash at
	// device-open time on PipeWire 1.4.x.
	// NOTE: these env vars are authoritative only when set before the binary loads
	// (openal-soft reads them from a static constructor). The launch scripts set them
	// first; this is a best-effort fallback for lazy-init builds.
	if (!getenv("ALSOFT_DISABLE_CPU_EXTS")) {
		setenv("ALSOFT_DISABLE_CPU_EXTS", "all", 1);
		fprintf(stderr, "INFO: OpenAL: ALSOFT_DISABLE_CPU_EXTS=all (movaps alignment crash workaround)\n");
	}
	if (!getenv("ALSOFT_DRIVERS")) {
		setenv("ALSOFT_DRIVERS", "pulse,alsa,oss,jack,null,wave", 1);
		fprintf(stderr, "INFO: OpenAL: ALSOFT_DRIVERS=pulse,alsa,oss,jack,null,wave (pipewire excluded)\n");
	}
	#else
	fprintf(stderr, "INFO: OpenAL: keeping default driver selection on non-Linux platform\n");
	#endif
}

/**
 * CreateGameEngine
 *
 * Factory function for SDL3GameEngine on Linux.
 * Called by GameMain() to instantiate platform-specific engine.
 *
 * @return SDL3GameEngine instance
 */
GameEngine *CreateGameEngine(void)
{
	fprintf(stderr, "INFO: CreateGameEngine() - Creating SDL3GameEngine for Linux\n");
	SDL3GameEngine *engine = NEW SDL3GameEngine();
	return engine;
}

/**
 * main
 *
 * Linux entry point (replaces WinMain on Windows).
 * Initializes subsystems and calls GameMain().
 *
 * @param argc Command line argument count
 * @param argv Command line arguments
 * @return Exit code (0 = success)
 */
int main(int argc, char* argv[])
{
	int exitcode = 1;

#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
	ConfigureMacOSAppRuntime(argc, argv);
#endif

	// TheSuperHackers @build felipebraz 13/02/2026
	// Store command line arguments in globals for CommandLine.cpp parser
	__argc = argc;
	__argv = argv;

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
	// Diagnostic capture: an icon-launched app's stderr goes nowhere we can read,
	// so mirror it to a file in Library/Caches (purgeable, not user-visible). This
	// lets us pull a full engine log after an on-device session — essential for
	// debugging mode-specific issues (e.g. Generals Challenge radar/scripts) that
	// only the user can reproduce. Pull with: devicectl ... copy from
	// Library/Caches/generals-stderr.log. Remove once the relevant bugs are fixed.
	{
		// Quiet DXVK at the source: the d3d8 layer's per-call warns (e.g. an
		// unimplemented render state set every frame) wrote hundreds of MB per
		// long session. The shipped dxvk.conf also sets logLevel=none; the env
		// covers modules that read it before the config.
		setenv("DXVK_LOG_LEVEL", "none", 0);
		const char *diagHome = getenv("HOME");
		if (diagHome != nullptr) {
			char diagPath[1024];
			char prevPath[1024];
			// Documents, not Library/Caches: Caches is purgeable (a device restart or
			// storage pressure can empty it), and Documents is user-reachable via the
			// Files app since the bundle enables UIFileSharingEnabled.
			snprintf(diagPath, sizeof(diagPath), "%s/Documents/generals-stderr.log", diagHome);
			// Keep the previous session's log: a session that ends in a memory kill
			// leaves no OS crash report, so the prior log is often the only evidence.
			snprintf(prevPath, sizeof(prevPath), "%s/Documents/generals-stderr-prev.log", diagHome);
			rename(diagPath, prevPath);
			// Filtered + capped sink instead of a raw freopen: per-frame debug spam
			// (upstream [GX-ISSUE144] font traces, [INI] loader traces, residual DXVK
			// warns) is dropped, and the file stops growing at 8 MB so a marathon
			// session cannot eat device storage. funopen() is fine here: this is
			// Darwin-only code.
			static int s_logFd = -1;
			s_logFd = open(diagPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (s_logFd >= 0) {
				static size_t s_logWritten = 0;
				FILE *sink = funopen(nullptr,
					nullptr,
					[](void *, const char *buf, int len) -> int {
						static const size_t kLogCap = 8u * 1024u * 1024u;
						if (s_logFd < 0) return len;
						if (len > 13 &&
						    (memcmp(buf, "[GX-ISSUE144]", 13) == 0 ||
						     memcmp(buf, "[INI] ", 6) == 0 ||
						     memcmp(buf, "warn:  D3D8De", 13) == 0)) {
							return len;  // drop known per-frame spam, report consumed
						}
						if (s_logWritten >= kLogCap) {
							// past the cap, still record errors — the tail of a dying
							// session is this log's whole reason to exist
							static bool s_capMarked = false;
							if (!s_capMarked) {
								s_capMarked = true;
								const char *mark = "[log capped: non-error lines dropped from here]\n";
								write(s_logFd, mark, strlen(mark));
							}
							if (len > 4 && (memcmp(buf, "err:", 4) == 0 ||
							                memcmp(buf, "ERROR", 5) == 0 ||
							                memcmp(buf, "FATAL", 5) == 0)) {
								write(s_logFd, buf, (size_t)len);
							}
							return len;
						}
						ssize_t w = write(s_logFd, buf, (size_t)len);
						if (w > 0) s_logWritten += (size_t)w;
						return len;
					},
					nullptr, nullptr);
				if (sink != nullptr) {
					*stderr = *sink;  // classic Darwin stderr swap; stderr is a FILE, not a macro here
					setvbuf(stderr, nullptr, _IOLBF, 0);  // line-buffered so a crash still flushes recent lines
				}
			}
		}
	}

	// The engine resolves all game data relative to the working directory.
	// Preferred layout: assets ship read-only INSIDE the signed app bundle
	// (<bundle>/GameData), the iOS-sanctioned home for app resources — the
	// install is then fully self-contained. Dev builds packaged without
	// assets fall back to the Documents folder (Files-app accessible).
	// User data (saves, Options.ini) always lives in Library/Application
	// Support via the engine's user-data path; never in the bundle.
	{
		const char *home = getenv("HOME");

		// <bundle>/GameData, derived from the executable path (argv[0])
		char bundleData[1024] = {0};
		if (argc > 0 && argv[0] != nullptr) {
			const char *slash = strrchr(argv[0], '/');
			if (slash != nullptr) {
				const size_t dirLen = (size_t)(slash - argv[0]);
				if (dirLen < sizeof(bundleData) - 16) {
					memcpy(bundleData, argv[0], dirLen);
					snprintf(bundleData + dirLen, sizeof(bundleData) - dirLen, "/GameData");
				}
			}
		}

		bool usingBundleData = false;
		if (bundleData[0] != '\0' && access(bundleData, R_OK) == 0) {
			if (chdir(bundleData) == 0) {
				usingBundleData = true;
				fprintf(stderr, "INFO: iOS working directory (bundle): %s\n", bundleData);
			}
		}
		if (!usingBundleData && home != nullptr) {
			char docs[1024];
			snprintf(docs, sizeof(docs), "%s/Documents", home);
			if (chdir(docs) != 0) {
				fprintf(stderr, "WARNING: chdir(%s) failed: %s\n", docs, strerror(errno));
			} else {
				fprintf(stderr, "INFO: iOS working directory (Documents): %s\n", docs);
			}
		}

		if (home != nullptr) {
			// Keep DXVK's shader cache in Library/Caches: purgeable under
			// storage pressure, excluded from iCloud backup, invisible in the
			// Files app. Must be set before the d3d8 dylib loads.
			char cacheDir[1024];
			snprintf(cacheDir, sizeof(cacheDir), "%s/Library/Caches", home);
			mkdir(cacheDir, 0755);
			setenv("DXVK_STATE_CACHE_PATH", cacheDir, 0);

			if (usingBundleData) {
				// Seed default settings on first run (full detail instead of the
				// 2003 auto-detect, which drops unknown GPUs to Low).
				char userDataDir[1024], optionsPath[1024];
				snprintf(userDataDir, sizeof(userDataDir),
				         "%s/Library/Application Support/GeneralsX/GeneralsZH", home);
				snprintf(optionsPath, sizeof(optionsPath), "%s/Options.ini", userDataDir);
				if (access(optionsPath, F_OK) != 0 && access("DefaultOptions.ini", R_OK) == 0) {
					std::error_code fsError;
					std::filesystem::create_directories(userDataDir, fsError);
					std::filesystem::copy_file("DefaultOptions.ini", optionsPath, fsError);
					if (!fsError) {
						fprintf(stderr, "INFO: Seeded default Options.ini\n");
					}
				}

				// One-time tidy-up: remove asset copies from Documents now that
				// the bundle carries them. Guarded by a sentinel so it truly runs
				// once — Documents is exposed via the Files app, and anything the
				// user places there later (mods, custom maps) must never be touched.
				// "Maps" is deliberately NOT in the list: it is where user maps live.
				char docs[1024];
				snprintf(docs, sizeof(docs), "%s/Documents", home);
				char sentinel[1024];
				snprintf(sentinel, sizeof(sentinel), "%s/.bundle-assets-tidied", docs);
				if (access(sentinel, F_OK) != 0) {
					std::error_code fsError;
					for (const auto &entry : std::filesystem::directory_iterator(docs, fsError)) {
						const std::string name = entry.path().filename().string();
						const bool isShippedAsset =
							(name.size() > 4 && name.compare(name.size() - 4, 4, ".big") == 0) ||
							name == "Data" || name == "Window" || name == "ZH_Generals" ||
							name == "fonts" || name == "_CommonRedist" ||
							name == "dxvk.conf" || name == "GeneralsXZH.dxvk-cache" ||
							name == "GeneralsXZH_d3d9.log";
						if (isShippedAsset) {
							fprintf(stderr, "INFO: tidy-up removing shipped asset copy: %s\n", name.c_str());
							std::error_code removeError;
							std::filesystem::remove_all(entry.path(), removeError);
						}
					}
					if (!fsError) {  // a failed scan must retry next launch, not fail closed forever
						FILE *s = fopen(sentinel, "w");
						if (s) fclose(s);
					}
				}
			}
		}
	}
#endif

	fprintf(stderr, "=================================================\n");
	fprintf(stderr, " Command & Conquer Generals: Zero Hour (Linux)\n");
	fprintf(stderr, " SDL3 + DXVK Build\n");
	fprintf(stderr, "=================================================\n\n");

	try {
		// Initialize critical sections (required by game engine)
		TheAsciiStringCriticalSection = &critSec1;
		TheUnicodeStringCriticalSection = &critSec2;
		TheDmaCriticalSection = &critSec3;
		TheMemoryPoolCriticalSection = &critSec4;
		TheDebugLogCriticalSection = &critSec5;

		// Initialize memory manager early (required by NEW operator)
		initMemoryManager();

		// GeneralsX @bugfix BenderAI 14/02/2026 Initialize Version singleton
		// GameEngine::init() calls updateWindowTitle() which uses TheVersion
		// Must be created before GameMain() to avoid nullptr dereference
		TheVersion = NEW Version;

		// Parse command line (CommandLine class handles argc/argv internally)
		// TheSuperHackers @build felipebraz 10/02/2026 Phase 1.5
		// Store argc/argv for CommandLine parser to access via _NSGetArgc/_NSGetArgv or /proc/self/cmdline
		// For now, let CommandLine::parseCommandLineForStartup() handle this
		CommandLine::parseCommandLineForStartup();

		// GeneralsX @bugfix Copilot 17/05/2026 Skip SDL3 window bootstrap for CLI/headless replay execution.
		const bool isHeadlessMode = (TheGlobalData != nullptr && TheGlobalData->m_headless);
		if (isHeadlessMode) {
			fprintf(stderr, "INFO: Headless mode detected, skipping SDL3 video/Vulkan window initialization\n");
		} else {

		// GeneralsX @bugfix felipebraz 16/02/2026
		// Initialize SDL3 and Vulkan BEFORE creating GameEngine (fighter19 pattern)
		// This prevents LLVM SIGSEGV crash during Vulkan driver enumeration
		// Must be done here, not in SDL3GameEngine::init() which is too late
		fprintf(stderr, "INFO: Initializing SDL3 video subsystem...\n");
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
		// All mouse events are synthesized by the gesture translator in
		// SDL3GameEngine.cpp; SDL's automatic touch->mouse synthesis would
		// double-deliver finger 1 and fight the two-finger pan logic.
		SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
#endif
#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
		// GeneralsX @bugfix 05/08/2026 Fullscreen stays a real macOS Space. Do not turn this off.
		//
		// Turning SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES off was tried and it is not an acceptable
		// fullscreen. setFullscreenSpace: refuses (SDL_cocoawindow.m:920), SDL_UpdateFullscreenMode
		// falls past the Space shortcut (SDL_video.c:1985), and Cocoa_SetWindowFullscreen builds a
		// borderless window itself -- but in this game that lands as a window smaller than the display
		// with the menu bar, Dock, desktop widgets and other apps' windows all still visible around it,
		// and pointer coordinates no longer line up with what is drawn. Real Space fullscreen is a
		// requirement, so the exit problem has to be solved inside it.
		//
		// The exit problem is that a Space outlives the process that made it: the game exits and the
		// Space can stay in front with no window inside it, nothing for Force Quit to list and nothing
		// for Cmd-Tab to leave. With one usable display there is no second screen to fall back to. The
		// countermeasures are all on the way out, not here: leaveFullscreenForShutdown() in
		// SDL3GameEngine.cpp waits out the un-fullscreen transition, GXKickDisplayConfiguration()
		// forces a CGDisplayReconfiguration (the programmatic equivalent of replugging the monitor) so
		// WindowServer rebuilds its Space list, and the SIGKILL watchdog guarantees the process dies.
		//
		// The hint is set explicitly rather than left at its default because allow_spaces is read
		// exactly once during video init (SDL_cocoavideo.m:217), so this is the only place it can be
		// set at all -- and because it documents that off is a known-bad value here.
		SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "1");
#endif
		if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
			fprintf(stderr, "FATAL: Failed to initialize SDL3: %s\n", SDL_GetError());
			return 1;
		}

		// Set DXVK WSI driver before loading Vulkan
		setenv("DXVK_WSI_DRIVER", "SDL3", 1);

		// GeneralsX @bugfix BenderAI 06/03/2026 - Exclude LLVMpipe Vulkan ICD before loading Vulkan.
		// libvulkan_lvp.so crashes during static initialization with LLVM 20.x when the Vulkan
		// loader enumerates all ICDs. Restrict to hardware ICDs first.
		FilterSoftwareVulkanICDs();
		FilterPipeWireOpenAL();

		// Load Vulkan library for DXVK DirectX8→Vulkan translation
		fprintf(stderr, "INFO: Loading Vulkan library...\n");
		if (!SDL_Vulkan_LoadLibrary(nullptr)) {
			fprintf(stderr, "WARNING: Failed to load Vulkan: %s\n", SDL_GetError());
			fprintf(stderr, "WARNING: Continuing without Vulkan (may use software rendering)\n");
		}

		// Create SDL3 window with Vulkan support
		fprintf(stderr, "INFO: Creating SDL3 Vulkan window...\n");
		Uint32 windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;  // Start hidden, show after D3D init
		int initialWindowW = 1024;
		int initialWindowH = 768;
		bool appleRetinaMode = false;
#if defined(__APPLE__)
		// GeneralsX @bugfix: request a Retina (HiDPI) backing drawable on both iOS and
		// macOS so the game renders at full display sharpness. The internal engine
		// resolution, resolution list and font scaling are all in physical pixels to match
		// (see GeneralsX_GetMacDisplayMetrics in W3DDisplay.cpp for the unit convention);
		// only SDL window geometry and mouse event coordinates remain in points.
		appleRetinaMode = true;
		if (appleRetinaMode) {
			windowFlags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
		}
#endif
#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
		// Enter fullscreen before DXVK creates its first backbuffer. Switching the
		// Cocoa window from 1024x768 to fullscreen later resizes the swapchain, but
		// leaves Generals' D3D8 backbuffer and mouse transform at the bootstrap size.
		bool requestedFullscreen = false;
		for (int i = 1; i < __argc; ++i) {
			if (strcmp(__argv[i], "-fullscreen") == 0) {
				requestedFullscreen = true;
				break;
			}
		}
		if (requestedFullscreen) {
			const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
			if (mode && mode->w > 0 && mode->h > 0) {
				initialWindowW = mode->w;
				initialWindowH = mode->h;
			}
			windowFlags |= SDL_WINDOW_FULLSCREEN;
			fprintf(stderr, "INFO: macOS pre-D3D fullscreen window set to %dx%d\n",
				initialWindowW, initialWindowH);
		}
#endif
		TheSDL3Window = SDL_CreateWindow(
			"Command & Conquer Generals: Zero Hour",
			initialWindowW, initialWindowH,
			windowFlags
		);

		if (!TheSDL3Window) {
			fprintf(stderr, "FATAL: Failed to create SDL3 window: %s\n", SDL_GetError());
			SDL_Quit();
			return 1;
		}

		// Store window handle globally (cast SDL_Window* to HWND for compatibility)
		ApplicationHWnd = (HWND)TheSDL3Window;
		fprintf(stderr, "INFO: SDL3 window created successfully\n");

#if defined(__APPLE__)
		// Match the internal render size to the screen's PHYSICAL PIXEL size, so the engine
		// renders 1:1 into DXVK's Retina swapchain instead of rendering small and being
		// upscaled. Generals' UI geometry (GameWindowManagerScript::parseScreenRect) and font
		// sizing (GlobalLanguage::adjustFontSize) both scale linearly off TheDisplay's
		// dimensions, so raising the internal resolution keeps layout proportions intact while
		// vector-drawn text and 3D geometry gain real detail. Bitmap UI art (ControlBar) is
		// fixed-resolution and stays as soft as its source, which no resolution change can fix.
		// Explicit user -xres/-yres options still take precedence.
		{
			bool userSetRes = false;
			for (int i = 1; i < __argc; ++i) {
				if (strcmp(__argv[i], "-xres") == 0 || strcmp(__argv[i], "-yres") == 0) {
					userSetRes = true;
					break;
				}
			}
			// GeneralsX @bugfix 26/07/2026 Only force the internal resolution in fullscreen.
			// In windowed mode the screen size is the wrong target: the window is smaller than
			// the screen (title bar, menu bar, Dock), so injecting the full screen size made the
			// engine render 4096x2304 into a 4096x2004 backbuffer and the pillarbox path scaled
			// the whole frame again -- the exact blur this injection exists to remove. Windowed
			// mode instead keeps the user's configured resolution and lets
			// SDL3_ApplyWindowModeForRenderConfig size the window to resolution/density points;
			// the render resolution then follows from the window's real pixel extent.
#if !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
			if (!requestedFullscreen) {
				userSetRes = true;
			}
#endif
			int winW = 0, winH = 0;
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
			SDL_GetWindowSizeInPixels(TheSDL3Window, &winW, &winH);
#else
			// GeneralsX @bugfix 26/07/2026 Inject the internal resolution in PHYSICAL PIXELS.
			//
			// This previously injected the display mode's logical point size. Combined with the
			// HIGH_PIXEL_DENSITY drawable and DXVK's pixel-sized swapchain, that made every frame
			// render into a point-sized offscreen target and get upscaled by the backing scale
			// factor in DX8Wrapper::Pillarbox_End -- a 2x blur over the entire game, UI and text
			// included. Matching the engine resolution to the backbuffer is what makes the
			// pillarbox path a no-op and gives genuine 1:1 HiDPI output.
			SDL_DisplayID displayId = SDL_GetDisplayForWindow(TheSDL3Window);
			const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(displayId);
			if (mode) {
				const float density = mode->pixel_density > 0.0f ? mode->pixel_density : 1.0f;
				const int pointW = mode->w;
				const int pointH = mode->h;
				winW = (int)(mode->w * density);
				winH = (int)(mode->h * density);
				if (s_macRenderScalePercent > 0 && s_macRenderScalePercent < 100) {
					// GeneralsX @bugfix 10/08/2026 Launch in point mode at the PANEL's pixels.
					//
					// Point mode means one rendered pixel per physical panel pixel; see
					// GeneralsX_GetFullscreenRenderSize in W3DDisplay.cpp for why the percentage cannot
					// express that on a scaled display and for the mode's full rationale. This is the
					// same rule applied one step earlier, before the display layer exists.
					//
					// Without it the saved Resolution was overwritten every launch by the percentage
					// result, which on a scaled display is the point grid rather than the panel: a
					// 2560x1440 panel presented as 2048x1152 points over a 4096x2304 framebuffer
					// launched the game at 2048x1152 and had to be corrected by hand each time.
					int panelW = 0, panelH = 0;
					if (GXGetPanelNativePixelSize(&panelW, &panelH) && panelW > 0 && panelH > 0)
					{
						fprintf(stderr, "INFO: Apple point mode launch: panel native %dx%d "
							"(points %dx%d, framebuffer %dx%d, %d%% would have given %dx%d)\n",
							panelW, panelH, pointW, pointH, winW, winH, s_macRenderScalePercent,
							(winW * s_macRenderScalePercent) / 100,
							(winH * s_macRenderScalePercent) / 100);
						winW = panelW;
						winH = panelH;
					}
					else {
						winW = (winW * s_macRenderScalePercent) / 100;
						winH = (winH * s_macRenderScalePercent) / 100;
					}
				}
			}
#endif
			if (!userSetRes && winW > 0 && winH > 0 && winW > winH) {
				static char xresVal[16], yresVal[16];
				static char xresFlag[] = "-xres";
				static char yresFlag[] = "-yres";
				const int yres = winH;
				int xres = winW;
				xres &= ~1;  // keep it even
				snprintf(xresVal, sizeof(xresVal), "%d", xres);
				snprintf(yresVal, sizeof(yresVal), "%d", yres);

				static char* newArgv[64];
				int n = 0;
				for (int i = 0; i < __argc && n < 59; ++i) {
					newArgv[n++] = __argv[i];
				}
				newArgv[n++] = xresFlag;
				newArgv[n++] = xresVal;
				newArgv[n++] = yresFlag;
				newArgv[n++] = yresVal;
				newArgv[n] = nullptr;
				__argv = newArgv;
				__argc = n;
				fprintf(stderr, "INFO: Apple %s internal resolution set to %sx%s (target %dx%d, renderScale=%d%%)\n",
				        appleRetinaMode ? "Retina" : "logical",
				        xresVal, yresVal, winW, winH, s_macRenderScalePercent);
			}
		}
#endif
		}

		// Call cross-platform game entry point
		exitcode = GameMain();

		fprintf(stderr, "INFO: GameMain() returned with code %d\n", exitcode);

	} catch (const std::exception& e) {
		fprintf(stderr, "FATAL: Unhandled exception in main(): %s\n", e.what());
		exitcode = 1;
	} catch (...) {
		fprintf(stderr, "FATAL: Unknown exception in main()\n");
		exitcode = 1;
	}

#if !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
	// GeneralsX @bugfix 02/08/2026 Never let shutdown be the thing that traps the machine.
	//
	// Armed before any teardown runs, including the paths reached by the catch blocks above. Nothing
	// past this point is allowed to take longer than the watchdog's window: DXVK's worker threads,
	// MoltenVK and OpenAL all still have live threads and GPU objects here, and a deadlock among them
	// used to leave a process that was mid-exit -- gone from the Force Quit list, since AppKit had
	// already dropped it, but still owning the screen. SIGKILL from a detached thread is the one
	// escape that needs no cooperation from whatever is stuck.
	GXArmShutdownWatchdog();
#endif

	// Cleanup SDL3 resources
	if (TheSDL3Window) {
#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
		// GeneralsX @bugfix 05/08/2026 If the window is still fullscreen here, the Space is about to
		// be orphaned, so force a display reconfiguration once the window is gone.
		//
		// Reaching this point means leaveFullscreenForShutdown() gave up: macOS refused the
		// un-fullscreen request for its whole budget. SDL_UpdateFullscreenMode early-bails once
		// is_destroying is set, so SDL_DestroyWindow will not unwind the Space either -- it just
		// closes the window and leaves a Space with nothing inside it, in front of everything, on the
		// only display this machine has. That is the black screen.
		//
		// An earlier attempt skipped SDL_DestroyWindow and _exit()ed with the window intact, betting
		// that WindowServer would collapse a fullscreen window whose client had died. The 05/08/2026
		// log disproved it: the branch ran, the process exited 0, and the display stayed black until
		// the monitor was replugged by hand. It also skipped SDL_Quit and the memory manager.
		//
		// What did recover the display was replugging the monitor, i.e. a CGDisplayReconfiguration --
		// WindowServer rebuilds its Space list and the orphaned Space has nothing left to attach to.
		// GXKickDisplayConfiguration() is that replug done in software (mode change and back). Order
		// matters: kick *after* SDL_DestroyWindow, because while the window still exists the Space is
		// legitimately occupied and a reconfiguration will simply rebuild it.
		const bool strandedFullscreen =
			(SDL_GetWindowFlags(TheSDL3Window) & SDL_WINDOW_FULLSCREEN) != 0;
		if (strandedFullscreen) {
			fprintf(stderr, "WARNING: still fullscreen at teardown -- will kick the display "
				"configuration after closing the window\n");
		}
#endif
		SDL_DestroyWindow(TheSDL3Window);
		TheSDL3Window = nullptr;
		ApplicationHWnd = nullptr;
#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
		if (strandedFullscreen) {
			GXKickDisplayConfiguration();
		}
#endif
	}
	SDL_Quit();

	// GeneralsX @bugfix BenderAI 14/02/2026 Cleanup Version singleton
	if (TheVersion) {
		delete TheVersion;
		TheVersion = nullptr;
	}

	// GeneralsX @bugfix BenderAI 19/02/2026 Shutdown memory manager BEFORE nulling critical
	// sections. Without this, global pool destructors (ObjectPoolClass) crash during atexit()
	// because they call ::operator delete after the memory manager is already gone (SIGSEGV).
	// Matches WinMain.cpp cleanup order: TheVersion -> shutdownMemoryManager -> null critSecs.
	shutdownMemoryManager();

	// Cleanup critical sections (after memory manager, which may use them during shutdown)
	TheAsciiStringCriticalSection = nullptr;
	TheUnicodeStringCriticalSection = nullptr;
	TheDmaCriticalSection = nullptr;
	TheMemoryPoolCriticalSection = nullptr;
	TheDebugLogCriticalSection = nullptr;

	fprintf(stderr, "\nExiting with code %d\n", exitcode);

	// GeneralsX @bugfix BenderAI 25/02/2026 — use _exit() to skip C++ global destructors.
	// On macOS, __cxa_finalize_ranges runs ObjectPoolClass<X,256> global dtors after main() returns.
	// Those dtors crash with a corrupted BlockListHead (SIGSEGV at 0x4ade32ec4ade0018) because
	// pool block memory was already reused/overwritten during game shutdown.
	// Windows never had this problem — ExitProcess() terminates without running C++ global dtors.
	// _exit() matches that behavior. Explicit cleanup already done above (SDL_Quit, shutdownMemoryManager).
	_exit(exitcode);
}

#endif // !_WIN32
