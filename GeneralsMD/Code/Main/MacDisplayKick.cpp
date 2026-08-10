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

// GeneralsX @bugfix 05/08/2026 Last-resort display kick -- the programmatic form of replugging.
//
// This lives in its own translation unit on purpose. <CoreGraphics/CoreGraphics.h> drags in
// <MacTypes.h>, whose "typedef UInt8 Byte" collides with the engine's own "typedef char Byte"
// (Core/Libraries/Include/Lib/BaseTypeCore.h:120) -- a hard error, not a warning. Nothing here
// includes an engine header, so the two Byte definitions never meet.

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)

#include <CoreGraphics/CoreGraphics.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace
{
	void SleepMilliseconds(long ms)
	{
		struct timespec request;
		request.tv_sec = ms / 1000;
		request.tv_nsec = (ms % 1000) * 1000000L;
		while (nanosleep(&request, &request) != 0) {
			// Finish the wait even if a signal interrupts it.
		}
	}

	/// IOKit's kDisplayModeNativeFlag, as surfaced through CGDisplayModeGetIOFlags. Not exported by
	/// any public CoreGraphics header, so it is spelled out here rather than guessed at each use.
	const uint32_t kGXDisplayModeNativeFlag = 0x02000000;
}

/// The display panel's own pixel grid, which is a third size the port previously had no way to name.
///
/// macOS reports two sizes for a scaled display and neither one is the panel: on the development
/// monitor a 2560x1440 panel driven through a scaled mode reports 2048x1152 logical points backed by
/// a 4096x2304 framebuffer, which WindowServer then resamples down to the 2560x1440 the panel
/// actually accepts. Rendering at the point size throws away a fifth of the panel's detail in each
/// axis; rendering at the framebuffer size draws 2.56x the pixels the panel can ever show. The
/// panel's real resolution is the useful ceiling, and only the mode list knows it -- CoreGraphics
/// tags the modes matching the panel's native timing with kDisplayModeNativeFlag, so the largest
/// pixel size among the flagged modes is the panel.
///
/// Returns false when nothing is flagged, which is normal: a plain non-HiDPI display, a virtual
/// display, and some external panels report no native mode at all. Callers must keep their existing
/// behavior in that case rather than treating a failure as "no scaling".
extern "C" bool GXGetPanelNativePixelSize(int* outWidth, int* outHeight)
{
	if (outWidth == NULL || outHeight == NULL) {
		return false;
	}
	*outWidth = 0;
	*outHeight = 0;

	const CGDirectDisplayID display = CGMainDisplayID();
	CFArrayRef modes = CGDisplayCopyAllDisplayModes(display, NULL);
	if (modes == NULL) {
		return false;
	}

	size_t bestW = 0, bestH = 0, bestArea = 0;
	const CFIndex count = CFArrayGetCount(modes);
	for (CFIndex i = 0; i < count; ++i) {
		CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
		if (mode == NULL) {
			continue;
		}
		if ((CGDisplayModeGetIOFlags(mode) & kGXDisplayModeNativeFlag) == 0) {
			continue;
		}
		const size_t pw = CGDisplayModeGetPixelWidth(mode);
		const size_t ph = CGDisplayModeGetPixelHeight(mode);
		if (pw * ph > bestArea) {
			bestArea = pw * ph;
			bestW = pw;
			bestH = ph;
		}
	}
	CFRelease(modes);

	if (bestW == 0 || bestH == 0) {
		return false;
	}
	*outWidth = (int)bestW;
	*outHeight = (int)bestH;
	return true;
}

/// Force a display reconfiguration, which is what unplugging and replugging a monitor does.
///
/// Measured on the development machine: a fullscreen Space left holding the *only* display recovers
/// the instant the external monitor is unplugged and replugged, and not a moment before -- the
/// arriving display raises a full CGDisplayReconfiguration, WindowServer rebuilds its Space list, and
/// the orphaned Space has nothing left to attach to. A display mode change raises that same
/// reconfiguration, so this asks for one directly instead of making anyone reach behind the monitor.
///
/// Called only if the window is still fullscreen at teardown, i.e. only after
/// leaveFullscreenForShutdown() has already spent its whole budget being refused. Verified end to end
/// on 05/08/2026 by forcing that state with GX_EXIT_TEST_REFUSE=1: the kick ran (2048x1152 -> 800x600
/// -> back) and the display came straight back, which is the first direct evidence that a
/// reconfiguration really does collapse an orphaned Space rather than merely correlating with the
/// manual replug that inspired it. Set GX_EXIT_DISPLAY_KICK=0 to skip it.
extern "C" void GXKickDisplayConfiguration(void)
{
	if (const char* override = getenv("GX_EXIT_DISPLAY_KICK")) {
		if (override[0] == '0') {
			fprintf(stderr, "INFO: exit display kick disabled by GX_EXIT_DISPLAY_KICK\n");
			return;
		}
	}

	const CGDirectDisplayID display = CGMainDisplayID();
	CGDisplayModeRef current = CGDisplayCopyDisplayMode(display);
	if (!current) {
		fprintf(stderr, "WARNING: exit display kick: no current mode for display %u\n",
			(unsigned)display);
		return;
	}

	// Any mode with different dimensions will do; the reconfiguration is the point, not the mode.
	CFArrayRef modes = CGDisplayCopyAllDisplayModes(display, NULL);
	CGDisplayModeRef other = NULL;
	if (modes) {
		const CFIndex count = CFArrayGetCount(modes);
		for (CFIndex i = 0; i < count; ++i) {
			CGDisplayModeRef candidate = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
			if (!candidate) {
				continue;
			}
			if (CGDisplayModeGetWidth(candidate) == CGDisplayModeGetWidth(current) &&
				CGDisplayModeGetHeight(candidate) == CGDisplayModeGetHeight(current)) {
				continue;
			}
			other = candidate;
			break;
		}
	}

	if (other) {
		// Keep the geometry, not just the ref: the ref cannot be trusted across the reconfiguration.
		// The desktop mode here is one BetterDisplay injects (scaled HiDPI 2048x1152), and BetterDisplay
		// rebuilds its injected mode list whenever the display is reconfigured -- which is precisely what
		// the switch-away below does. The standalone kick-display tool hit this on 05/08/2026: the
		// restore failed with kCGErrorFailure and left the monitor on 1920x1080 until BetterDisplay
		// re-asserted the override minutes later. Nothing had noticed, because return codes were ignored.
		const size_t curW = CGDisplayModeGetWidth(current);
		const size_t curH = CGDisplayModeGetHeight(current);

		fprintf(stderr, "INFO: kicking the display configuration (%zux%zu -> %zux%zu -> back)\n",
			curW, curH, CGDisplayModeGetWidth(other), CGDisplayModeGetHeight(other));

		CGError err = CGDisplaySetDisplayMode(display, other, NULL);
		if (err != kCGErrorSuccess) {
			// No reconfiguration happened, so there is nothing to restore -- but the Space still needs
			// collapsing, and this is the one route left.
			fprintf(stderr, "WARNING: exit display kick: switch away failed (%d); falling back to "
				"CGRestorePermanentDisplayConfiguration\n", (int)err);
			CGRestorePermanentDisplayConfiguration();
			SleepMilliseconds(400);
		}
		else {
			SleepMilliseconds(400);  // let WindowServer settle before asking for the next one

			// Re-resolve by geometry, which survives the rebuild; only then trust the original ref.
			CGDisplayModeRef restore = NULL;
			CFArrayRef fresh = CGDisplayCopyAllDisplayModes(display, NULL);
			if (fresh) {
				const CFIndex n = CFArrayGetCount(fresh);
				for (CFIndex i = 0; i < n; ++i) {
					CGDisplayModeRef m = (CGDisplayModeRef)CFArrayGetValueAtIndex(fresh, i);
					if (m && CGDisplayModeGetWidth(m) == curW && CGDisplayModeGetHeight(m) == curH) {
						restore = m;
						break;
					}
				}
			}

			err = CGDisplaySetDisplayMode(display, restore ? restore : current, NULL);
			if (err != kCGErrorSuccess && restore) {
				// Re-resolved ref was rejected; the pre-reconfiguration one may still be valid.
				err = CGDisplaySetDisplayMode(display, current, NULL);
			}
			if (err != kCGErrorSuccess) {
				fprintf(stderr, "WARNING: exit display kick: restore to %zux%zu failed (%d); falling back "
					"to CGRestorePermanentDisplayConfiguration\n", curW, curH, (int)err);
				CGRestorePermanentDisplayConfiguration();
			}
			else {
				fprintf(stderr, "INFO: display kick complete, restored %zux%zu\n", curW, curH);
			}
			SleepMilliseconds(400);

			if (fresh) {
				CFRelease(fresh);
			}
		}
	}
	else {
		fprintf(stderr, "WARNING: exit display kick: no alternate mode on display %u, falling back to "
			"CGRestorePermanentDisplayConfiguration\n", (unsigned)display);
		CGRestorePermanentDisplayConfiguration();
		SleepMilliseconds(400);
	}

	CGDisplayModeRelease(current);
	if (modes) {
		CFRelease(modes);
	}
}

#endif // __APPLE__ && !TARGET_OS_IPHONE
