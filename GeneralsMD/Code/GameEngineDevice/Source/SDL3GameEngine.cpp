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
** SDL3GameEngine.cpp
**
** Linux implementation of GameEngine using SDL3 for windowing/input.
**
** TheSuperHackers @feature CnC_Generals_Linux 07/02/2026
** Provides SDL3-based input and window management for Linux builds.
** Based on fighter19 reference implementation.
*/

#ifndef _WIN32

#include "SDL3GameEngine.h"
#include "OpenALAudioManager.h"
#include "SDL3Device/GameClient/SDL3Mouse.h"
#include "SDL3Device/GameClient/SDL3Keyboard.h"
#include "GameClient/Mouse.h"
#include "GameClient/Keyboard.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Gadget.h"
#include "GameClient/Shell.h"
#include "W3DDevice/GameLogic/W3DGameLogic.h"
#include "W3DDevice/GameClient/W3DGameClient.h"
#include "W3DDevice/Common/W3DModuleFactory.h"
#include "W3DDevice/Common/W3DThingFactory.h"
#include "W3DDevice/Common/W3DFunctionLexicon.h"
#include "W3DDevice/Common/W3DRadar.h"
#include "W3DDevice/GameClient/W3DParticleSys.h"
#include "W3DDevice/GameClient/W3DWebBrowser.h"
#include "StdDevice/Common/StdLocalFileSystem.h"
#include "StdDevice/Common/StdBIGFileSystem.h"
#include "Common/GlobalData.h"
#include "GameClient/Display.h"
#include "GameClient/HeaderTemplate.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

// Extern globals for input devices (set by GameClient)
extern Mouse *TheMouse;
extern Keyboard *TheKeyboard;
extern GameWindowManager *TheWindowManager;

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#include <atomic>

// ---------------------------------------------------------------------------
// iOS app lifecycle
//
// iOS suspends the process when the app leaves the foreground. Any GPU work
// submitted around suspension stalls on drawable acquisition (MoltenVK waits
// out a timeout per present), which surfaces as multi-second input hangs right
// after resuming. SDL warns that lifecycle events can arrive outside the
// normal poll cycle, so they are captured in an event watcher that fires
// immediately on the delivering thread; the engine update loop checks the
// flag and skips simulation + rendering while backgrounded.
// ---------------------------------------------------------------------------
// Two independent reasons to halt the render/sim loop on iOS:
//  - BACKGROUNDED (home / switched away): the process is about to be suspended.
//  - INACTIVE (multitasking switcher open, Control Center, a notification
//    banner): iOS snapshots the window and owns the CAMetalLayer drawable during
//    this window — and crucially, opening the app switcher fires resign-active
//    WITHOUT a full background transition.
// Acquiring a Metal drawable during EITHER state fights iOS for the layer; across
// repeated suspend/switcher cycles MoltenVK is driven into an unrecoverable
// surface state and the app crashes (the reported "crashes after backgrounding /
// multitasking a few times"). Pause whenever either is set.
static std::atomic<bool> s_appBackgrounded{false};
static std::atomic<bool> s_appInactive{false};

static inline bool iosShouldPauseRendering()
{
	return s_appBackgrounded.load() || s_appInactive.load();
}

static bool SDLCALL iosLifecycleWatcher(void *userdata, SDL_Event *event)
{
	switch (event->type) {
		case SDL_EVENT_WILL_ENTER_BACKGROUND:
		case SDL_EVENT_DID_ENTER_BACKGROUND:
			s_appBackgrounded.store(true);
			break;
		case SDL_EVENT_DID_ENTER_FOREGROUND:
			s_appBackgrounded.store(false);
			break;
		// Resign/become active. On iOS, SDL maps applicationWillResignActive ->
		// window focus lost and applicationDidBecomeActive -> window focus gained.
		// Stay paused until fully active again (focus regained), which arrives
		// after DID_ENTER_FOREGROUND.
		case SDL_EVENT_WINDOW_FOCUS_LOST:
			s_appInactive.store(true);
			break;
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			s_appInactive.store(false);
			break;
		default:
			break;
	}
	return true;
}

// ---------------------------------------------------------------------------
// iOS touch -> mouse gesture translation
//
// SDL's automatic touch-mouse synthesis is disabled on iOS (SDL3Main.cpp sets
// SDL_HINT_TOUCH_MOUSE_EVENTS=0); every mouse event the game sees on iOS is
// synthesized here, through the same SDL3Mouse::addSDLEvent path real mice use.
//
// Gestures (matching the game's stock control scheme, which is LMB-centric):
//   1 finger tap/drag     -> left button click / drag (select, command, drag-box)
//   1 finger long-press   -> right button click (deselect), if finger stays put
//   2 finger drag         -> right-button drag at the centroid (camera scroll)
//   2 finger pinch        -> mouse wheel (camera zoom)
// ---------------------------------------------------------------------------
namespace {

struct TouchState {
	enum Phase {
		IDLE,        // no fingers tracked
		PENDING,     // finger1 down, gesture identity not yet known, nothing sent
		DRAGGING,    // finger1 drag in progress, LMB held
		LONGPRESSED, // long-press fired (RMB click sent), swallow until lift
		PAN          // two-finger camera pan, RMB held
	};

	Phase phase = IDLE;
	SDL_FingerID finger1 = 0;
	SDL_FingerID finger2 = 0;
	float downX = 0.0f, downY = 0.0f;   // finger1 down position (window points)
	float lastX = 0.0f, lastY = 0.0f;   // finger1 latest position
	float panX = 0.0f, panY = 0.0f;     // pan centroid
	float pinchDist = 0.0f;             // finger distance at last wheel step
	Uint64 downTicks = 0;
	float f1x = 0.0f, f1y = 0.0f, f2x = 0.0f, f2y = 0.0f; // normalized per finger
};

TouchState s_touch;

const Uint64 LONG_PRESS_MS = 600;
const float PINCH_STEP_RATIO = 0.06f;  // 6% distance change per wheel tick
const float TAP_DEAD_ZONE_PX = 8.0f;   // jitter below this keeps a tap a tap

void sendSyntheticMouse(SDL3Mouse *mouse, SDL_Window *window, Uint32 type,
                        float x, float y, Uint8 button = 0, float wheelY = 0.0f)
{
	// The windowID must be valid: SDL3Mouse::scaleMouseCoordinates() looks the
	// window up by id to map window points into the game's internal resolution,
	// and silently skips scaling when the lookup fails.
	const SDL_WindowID windowID = SDL_GetWindowID(window);

	SDL_Event ev;
	SDL_zero(ev);
	ev.type = type;
	switch (type) {
		case SDL_EVENT_MOUSE_MOTION:
			ev.motion.windowID = windowID;
			ev.motion.x = x;
			ev.motion.y = y;
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			ev.button.windowID = windowID;
			ev.button.button = button;
			ev.button.down = (type == SDL_EVENT_MOUSE_BUTTON_DOWN);
			ev.button.clicks = 1;
			ev.button.x = x;
			ev.button.y = y;
			break;
		case SDL_EVENT_MOUSE_WHEEL:
			ev.wheel.windowID = windowID;
			ev.wheel.x = 0.0f;
			ev.wheel.y = wheelY;
			ev.wheel.mouse_x = x;
			ev.wheel.mouse_y = y;
			break;
	}
	mouse->addSDLEvent(&ev);
}

void beginPan(SDL3Mouse *mouse, SDL_Window *window, int winW, int winH)
{
	s_touch.panX = (s_touch.f1x + s_touch.f2x) * 0.5f * (float)winW;
	s_touch.panY = (s_touch.f1y + s_touch.f2y) * 0.5f * (float)winH;
	const float dx = (s_touch.f1x - s_touch.f2x) * (float)winW;
	const float dy = (s_touch.f1y - s_touch.f2y) * (float)winH;
	s_touch.pinchDist = SDL_sqrtf(dx * dx + dy * dy);
	sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_MOTION, s_touch.panX, s_touch.panY);
	sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_DOWN,
	                   s_touch.panX, s_touch.panY, SDL_BUTTON_RIGHT);
	s_touch.phase = TouchState::PAN;
}

void handleTouchEvent(SDL3Mouse *mouse, SDL_Window *window, const SDL_Event &event)
{
	int winW = 0, winH = 0;
	SDL_GetWindowSize(window, &winW, &winH);
	const float px = event.tfinger.x * (float)winW;
	const float py = event.tfinger.y * (float)winH;

	switch (event.type) {
	case SDL_EVENT_FINGER_DOWN:
		if (s_touch.phase == TouchState::IDLE) {
			// Defer all BUTTON output: a finger landing could become a tap, a
			// drag-box, a long-press, or the first finger of a camera pan. A
			// premature LMB down+up is a real click to the game (e.g. it sets a
			// rally point when a production building is selected).
			s_touch.finger1 = event.tfinger.fingerID;
			s_touch.phase = TouchState::PENDING;
			s_touch.downX = s_touch.lastX = px;
			s_touch.downY = s_touch.lastY = py;
			s_touch.f1x = event.tfinger.x;
			s_touch.f1y = event.tfinger.y;
			s_touch.downTicks = SDL_GetTicks();
			// Move the cursor to the touch point NOW (motion clicks nothing, so the
			// deferred-tap protection is intact). This lets the GUI process hover
			// over the next frame(s) before the tap commits — hover-driven widgets
			// (e.g. the Generals Challenge general buttons, which are checkboxes
			// that ignore a click unless WIN_STATE_HILITED was set by a prior
			// mouse-enter) then accept the click. Real mice hover before clicking;
			// without this, a synthetic tap teleports + clicks in one instant and
			// the widget is never hilited, so only the default/first item responds.
			sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_MOTION, px, py);
		}
		else if (s_touch.phase == TouchState::PENDING) {
			// Second finger before the first committed to anything: pure pan,
			// no left-click ever happened.
			s_touch.finger2 = event.tfinger.fingerID;
			s_touch.f2x = event.tfinger.x;
			s_touch.f2y = event.tfinger.y;
			beginPan(mouse, window, winW, winH);
		}
		else if (s_touch.phase == TouchState::DRAGGING) {
			// Second finger during a live drag: finish the drag-box, then pan.
			s_touch.finger2 = event.tfinger.fingerID;
			s_touch.f2x = event.tfinger.x;
			s_touch.f2y = event.tfinger.y;
			sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_UP,
			                   s_touch.lastX, s_touch.lastY, SDL_BUTTON_LEFT);
			beginPan(mouse, window, winW, winH);
		}
		// LONGPRESSED / PAN with extra fingers: ignored
		break;

	case SDL_EVENT_FINGER_MOTION:
		if (event.tfinger.fingerID == s_touch.finger1) {
			s_touch.f1x = event.tfinger.x;
			s_touch.f1y = event.tfinger.y;
			s_touch.lastX = px;
			s_touch.lastY = py;
		} else if (s_touch.phase == TouchState::PAN && event.tfinger.fingerID == s_touch.finger2) {
			s_touch.f2x = event.tfinger.x;
			s_touch.f2y = event.tfinger.y;
		} else {
			break;
		}

		if (s_touch.phase == TouchState::PENDING && event.tfinger.fingerID == s_touch.finger1) {
			const float moved = SDL_fabsf(px - s_touch.downX) + SDL_fabsf(py - s_touch.downY);
			if (moved >= TAP_DEAD_ZONE_PX) {
				// Commit to a drag: anchor the LMB at the original touch point so
				// drag-boxes start where the finger first landed.
				sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_MOTION, s_touch.downX, s_touch.downY);
				sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_DOWN,
				                   s_touch.downX, s_touch.downY, SDL_BUTTON_LEFT);
				sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_MOTION, px, py);
				s_touch.phase = TouchState::DRAGGING;
			}
		}
		else if (s_touch.phase == TouchState::DRAGGING && event.tfinger.fingerID == s_touch.finger1) {
			sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_MOTION, px, py);
		}
		else if (s_touch.phase == TouchState::PAN) {
			const float cx = (s_touch.f1x + s_touch.f2x) * 0.5f * (float)winW;
			const float cy = (s_touch.f1y + s_touch.f2y) * 0.5f * (float)winH;
			sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_MOTION, cx, cy);
			s_touch.panX = cx;
			s_touch.panY = cy;

			const float dx = (s_touch.f1x - s_touch.f2x) * (float)winW;
			const float dy = (s_touch.f1y - s_touch.f2y) * (float)winH;
			const float dist = SDL_sqrtf(dx * dx + dy * dy);
			if (s_touch.pinchDist > 1.0f) {
				const float ratio = dist / s_touch.pinchDist;
				if (ratio > 1.0f + PINCH_STEP_RATIO) {
					sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_WHEEL, cx, cy, 0, 1.0f);
					s_touch.pinchDist = dist;
				} else if (ratio < 1.0f - PINCH_STEP_RATIO) {
					sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_WHEEL, cx, cy, 0, -1.0f);
					s_touch.pinchDist = dist;
				}
			}
		}
		break;

	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_CANCELED:
		if (event.tfinger.fingerID != s_touch.finger1 &&
		    !(s_touch.phase == TouchState::PAN && event.tfinger.fingerID == s_touch.finger2)) {
			break;
		}
		switch (s_touch.phase) {
			case TouchState::PENDING:
				// A CANCELED touch (incoming call, notification shade, palm
				// rejection) must not become a committed tap — that would be a
				// phantom select/command/rally-point click at the cancel point.
				if (event.type == SDL_EVENT_FINGER_CANCELED) {
					break;
				}
				// Clean tap: deliver the full click at the exact press position.
				sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_MOTION, s_touch.downX, s_touch.downY);
				sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_DOWN,
				                   s_touch.downX, s_touch.downY, SDL_BUTTON_LEFT);
				sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_UP,
				                   s_touch.downX, s_touch.downY, SDL_BUTTON_LEFT);
				break;
			case TouchState::DRAGGING:
				sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_UP, px, py, SDL_BUTTON_LEFT);
				break;
			case TouchState::PAN:
				sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_UP,
				                   s_touch.panX, s_touch.panY, SDL_BUTTON_RIGHT);
				break;
			default:
				break;
		}
		s_touch.phase = TouchState::IDLE;
		break;
	}
}

// Called once per engine frame (not just per touch event): a perfectly
// stationary finger produces no SDL events, so the long-press timer must be
// polled from the frame loop or it would never fire.
void updateTouchLongPress(SDL3Mouse *mouse, SDL_Window *window)
{
	if (s_touch.phase == TouchState::PENDING &&
	    (SDL_GetTicks() - s_touch.downTicks) >= LONG_PRESS_MS) {
		// No LMB was sent yet (deferred), so this is a pure right-click.
		sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_MOTION, s_touch.downX, s_touch.downY);
		sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_DOWN,
		                   s_touch.downX, s_touch.downY, SDL_BUTTON_RIGHT);
		sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_UP,
		                   s_touch.downX, s_touch.downY, SDL_BUTTON_RIGHT);
		s_touch.phase = TouchState::LONGPRESSED;
	}
}

} // anonymous namespace
#endif // TARGET_OS_IPHONE

namespace {

Bool DecodeNextUtf8Codepoint(const char* text, size_t length, size_t& offset, UnsignedInt& outCodepoint)
{
	outCodepoint = 0;
	if (!text || offset >= length) {
		return false;
	}

	const unsigned char first = static_cast<unsigned char>(text[offset]);
	if (first == 0) {
		return false;
	}

	if (first < 0x80) {
		outCodepoint = first;
		offset += 1;
		return true;
	}

	if ((first & 0xE0) == 0xC0 && offset + 1 < length) {
		const unsigned char second = static_cast<unsigned char>(text[offset + 1]);
		if ((second & 0xC0) == 0x80) {
			outCodepoint = ((first & 0x1F) << 6) | (second & 0x3F);
			offset += 2;
			return true;
		}
	}

	if ((first & 0xF0) == 0xE0 && offset + 2 < length) {
		const unsigned char second = static_cast<unsigned char>(text[offset + 1]);
		const unsigned char third = static_cast<unsigned char>(text[offset + 2]);
		if ((second & 0xC0) == 0x80 && (third & 0xC0) == 0x80) {
			outCodepoint = ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F);
			offset += 3;
			return true;
		}
	}

	if ((first & 0xF8) == 0xF0 && offset + 3 < length) {
		const unsigned char second = static_cast<unsigned char>(text[offset + 1]);
		const unsigned char third = static_cast<unsigned char>(text[offset + 2]);
		const unsigned char fourth = static_cast<unsigned char>(text[offset + 3]);
		if ((second & 0xC0) == 0x80 && (third & 0xC0) == 0x80 && (fourth & 0xC0) == 0x80) {
			outCodepoint = ((first & 0x07) << 18) | ((second & 0x3F) << 12) | ((third & 0x3F) << 6) | (fourth & 0x3F);
			offset += 4;
			return true;
		}
	}

	// Invalid UTF-8 sequence: skip one byte and keep processing.
	offset += 1;
	return false;
}

}

/**
 * Constructor: Initialize SDL3 game engine state
 */
SDL3GameEngine::SDL3GameEngine()
	: GameEngine(),
	  m_SDLWindow(nullptr),
	  m_IsInitialized(false),
	  m_IsActive(false),
	  m_IsTextInputActive(false),
	  m_TextInputFocusWindow(nullptr)
{
	fprintf(stderr, "DEBUG: SDL3GameEngine::SDL3GameEngine() created\n");
}

/**
 * Destructor: Cleanup SDL3 resources
 */
SDL3GameEngine::~SDL3GameEngine()
{
	if (m_SDLWindow && m_IsTextInputActive) {
		SDL_StopTextInput(m_SDLWindow);
		m_IsTextInputActive = false;
		m_TextInputFocusWindow = nullptr;
	}

	if (m_IsInitialized) {
		// Window cleanup is done in reset/shutdown
	}
	fprintf(stderr, "DEBUG: SDL3GameEngine::~SDL3GameEngine() destroyed\n");
}

/**
 * From GameEngine: init() - initialize subsystems
 * 
 * GeneralsX @bugfix felipebraz 16/02/2026
 * Simplified to follow fighter19 pattern - SDL3/Vulkan initialized in SDL3Main.cpp
 * before GameEngine is created. This init() only delegates to parent GameEngine::init().
 * ApplicationHWnd and TheSDL3Window are already set by main() before this is called.
 */
void SDL3GameEngine::init(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::init() starting\n");

	if (TheGlobalData && TheGlobalData->m_headless) {
		// GeneralsX @bugfix Copilot 17/05/2026 Allow headless replay path to initialize engine subsystems without an SDL window.
		fprintf(stderr, "INFO: SDL3GameEngine::init() headless mode - skipping SDL window binding\n");
		m_SDLWindow = nullptr;
		m_IsInitialized = true;
		m_IsActive = true;
		GameEngine::init();
		return;
	}

	// Verify window was created by SDL3Main.cpp
	extern SDL_Window* TheSDL3Window;
	extern HWND ApplicationHWnd;
	
	if (!TheSDL3Window || !ApplicationHWnd) {
		fprintf(stderr, "FATAL: SDL3 window not initialized before GameEngine::init()\n");
		fprintf(stderr, "FATAL: TheSDL3Window=%p, ApplicationHWnd=%p\n", TheSDL3Window, ApplicationHWnd);
		return;
	}

	// Store window reference locally
	m_SDLWindow = TheSDL3Window;
	m_IsInitialized = true;
	m_IsActive = true;

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
	// Lifecycle events can fire outside the poll cycle on iOS; catch them
	// immediately so rendering halts before the process is suspended.
	SDL_AddEventWatch(iosLifecycleWatcher, nullptr);
#endif

	fprintf(stderr, "INFO: SDL3GameEngine using pre-initialized window\n");

	// Call parent init to initialize game subsystems
	GameEngine::init();
}

/**
 * From GameEngine: reset() - reset system to starting state
 */
void SDL3GameEngine::reset(void)
{
	fprintf(stderr, "DEBUG: SDL3GameEngine::reset()\n");
	if (m_SDLWindow && m_IsTextInputActive) {
		SDL_StopTextInput(m_SDLWindow);
		m_IsTextInputActive = false;
		m_TextInputFocusWindow = nullptr;
	}
	GameEngine::reset();
}

/**
 * From GameEngine: update() - per-frame update
 */
void SDL3GameEngine::update(void)
{
	pollSDL3Events();
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
	// Pause sim + render while backgrounded OR inactive (see iosLifecycleWatcher).
	// Acquiring a Metal drawable in these windows fights iOS for the layer and,
	// across repeated suspend/switcher cycles, crashes MoltenVK. Keep polling so
	// we still catch the resume events; just don't touch the GPU.
	if (iosShouldPauseRendering()) {
		SDL_Delay(50);
		return;
	}
#endif
	GameEngine::update();
}

#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
/**
 * Leave the fullscreen Space here, at the top of shutdown, while this engine is still whole.
 *
 * GeneralsX @bugfix 03/08/2026 A fullscreen Space that outlives its process holds the screen.
 *
 * This was tried one layer further out first -- in SDL3Main.cpp, after GameMain() returns -- and it
 * failed in the field for a reason worth recording: by then ~SDL3GameEngine has already run, so the
 * window is no longer key and the app is no longer frontmost, and macOS will not animate a Space
 * transition for a window in that state. The log from the failure is unambiguous:
 *
 *     INFO: leaving fullscreen before teardown
 *     WARNING: SDL_SyncWindow on exit timed out: No window has focus
 *     Exiting with code 0
 *
 * The process then died with the transition half-done, and the empty Space stayed in front of
 * everything for minutes -- nothing for Force Quit to list, since the process was genuinely gone,
 * and nothing for Cmd-Tab to leave, since a Space is not a window.
 *
 * Here the window is still key, the app is still frontmost and the event loop is still the one Cocoa
 * expects, so the transition actually runs. Cocoa_SyncWindow's own budget is 2.5s, which is not
 * enough when the machine is paging hard -- and paging hard is exactly the state that produces this
 * bug, since the same session had already lost MTLCompilerService and audioanalyticsd to jetsam.
 * So the wait here is longer and it watches the flag that actually matters instead.
 */
void SDL3GameEngine::leaveFullscreenForShutdown(void)
{
	if (!m_SDLWindow) {
		return;
	}
	if ((SDL_GetWindowFlags(m_SDLWindow) & SDL_WINDOW_FULLSCREEN) == 0) {
		return;
	}

	// GeneralsX @bugfix 05/08/2026 12s, not 5s. The 05/08/2026 incident spent the whole 5s budget
	// with the flag never clearing -- macOS refused both attempts -- and the display was then held by
	// an orphaned Space until the monitor was replugged by hand. 5s was chosen against Cocoa_SyncWindow's
	// 2.5s, which was the wrong yardstick: what has to fit in the budget is a Space transition that
	// WindowServer is free to defer, not a window-property sync. This runs before GXArmShutdownWatchdog,
	// so a longer wait here does not eat into the watchdog's window; the cost of raising it is a slower
	// quit in the bad case, and the cost of leaving it low is the bug.
	double budgetSeconds = 12.0;
	if (const char* override = getenv("GX_EXIT_UNFULLSCREEN_SECONDS")) {
		char* end = nullptr;
		const double parsed = strtod(override, &end);
		if (end != override && parsed >= 0.0) {
			budgetSeconds = parsed;
		}
	}
	if (budgetSeconds <= 0.0) {
		fprintf(stderr, "INFO: exit un-fullscreen disabled by GX_EXIT_UNFULLSCREEN_SECONDS\n");
		return;
	}

	// Make SDL's own Cocoa path wait for the Space transition instead of firing and forgetting.
	// SDL_HINT_VIDEO_SYNC_WINDOW_OPERATIONS defaults to false (SDL_video.c:235), and that default is
	// why SDL passes blocking=false to Cocoa_SetWindowFullscreenSpace and returns while the transition
	// is still in flight. With the hint on, that function runs its own three-attempt loop, pumping
	// events and re-issuing toggleFullScreen: whenever an attempt was interrupted -- exactly the
	// recovery this needs on a machine that is paging hard. Nothing after this point reads the hint,
	// so there is no reason to put it back.
	SDL_SetHint(SDL_HINT_VIDEO_SYNC_WINDOW_OPERATIONS, "1");

	// Cocoa will not run a Space transition for a window that is not key and an app that is not
	// frontmost -- that is the documented failure this whole function was moved earlier to avoid
	// ("No window has focus", above). Being called first thing after the main loop usually means both
	// are already true, but not always: a crash path can get here with a modal error dialog having
	// taken focus, and by then asking is free. Do it before the request, not after.
	SDL_RaiseWindow(m_SDLWindow);
	SDL_PumpEvents();

	// GeneralsX 05/08/2026 Fault injection, so the failure path can be reached on purpose.
	//
	// The recovery machinery below -- the periodic re-issue and, past the budget, the display kick in
	// SDL3Main.cpp -- only ever runs when macOS refuses to leave the Space. That refusal is rare and
	// the conditions for it are not reliably reproducible: three consecutive runs exited cleanly on the
	// first request, one of them with swap 88% full at pressure level 2, so simply playing more is not
	// a test. Untested recovery code is indistinguishable from no recovery code, and the assumption it
	// rests on (that a CGDisplayReconfiguration collapses an orphaned Space) is still only an inference
	// from the fact that replugging the monitor by hand worked.
	//
	// With GX_EXIT_TEST_REFUSE=1 the un-fullscreen request is simply never sent. The flag therefore
	// stays set for the whole budget, which is exactly the state the real bug produces, and everything
	// downstream runs for real. Deliberately stranding the display is the point; keep the budget short
	// (GX_EXIT_UNFULLSCREEN_SECONDS=2) so the kick comes quickly, and have tools/rescue-display.sh
	// reachable over SSH before using this.
	const bool testRefuse = []() {
		const char* v = getenv("GX_EXIT_TEST_REFUSE");
		return v && v[0] == '1';
	}();

	fprintf(stderr, "INFO: leaving fullscreen at start of shutdown (budget %.1fs)\n", budgetSeconds);
	if (testRefuse) {
		fprintf(stderr, "WARNING: GX_EXIT_TEST_REFUSE=1 -- deliberately NOT sending the un-fullscreen "
			"request, to exercise the timeout and display-kick path\n");
	}
	else if (!SDL_SetWindowFullscreen(m_SDLWindow, false)) {
		fprintf(stderr, "WARNING: SDL_SetWindowFullscreen(false) at shutdown failed: %s\n", SDL_GetError());
		return;
	}

	// Drive the transition from the loop Cocoa is waiting on, and wait on the flag clearing rather
	// than on SDL's pending-operation bookkeeping -- the flag is the thing that decides whether a
	// Space is left behind.
	const Uint64 start = SDL_GetTicks();
	const Uint64 deadline = start + static_cast<Uint64>(budgetSeconds * 1000.0);
	bool left = false;

	// GeneralsX @bugfix 05/08/2026 Re-ask on a fixed interval instead of once halfway through.
	//
	// Cocoa drops a toggleFullScreen: that arrives while another transition is unwinding --
	// setFullscreenSpace: records it as a pending operation and returns (SDL_cocoawindow.m:930) -- so
	// any single request can simply be lost, and a request scheduled at one fixed moment can be lost
	// for the whole budget. That is what the 05/08/2026 log shows: two attempts, both refused, five
	// seconds of nothing. Asking every 1.5s means a request lands soon after each time WindowServer
	// becomes willing to act, without hammering it hard enough to keep interrupting its own
	// transition.
	const Uint64 retryIntervalMs = 1500;
	Uint64 nextRetry = start + retryIntervalMs;
	int retries = 0;
	for (;;) {
		SDL_PumpEvents();
		SDL_Event drained;
		while (SDL_PollEvent(&drained)) {
			// The engine is shutting down; nothing downstream wants these any more.
		}

		if ((SDL_GetWindowFlags(m_SDLWindow) & SDL_WINDOW_FULLSCREEN) == 0) {
			left = true;
			break;
		}
		const Uint64 now = SDL_GetTicks();
		if (now >= deadline) {
			break;
		}
		if (now >= nextRetry) {
			nextRetry = now + retryIntervalMs;
			++retries;
			fprintf(stderr, "INFO: still fullscreen after %.1fs, re-issuing the un-fullscreen request "
				"(attempt %d)\n", (now - start) / 1000.0, retries + 1);
			if (!testRefuse) {
				// Focus can be lost again mid-shutdown, and without it the request is refused outright.
				SDL_RaiseWindow(m_SDLWindow);
				SDL_SetWindowFullscreen(m_SDLWindow, false);
			}
		}
		SDL_Delay(10);
	}

	if (left) {
		// SDL clears the flag when it starts believing the window is windowed; give Cocoa the last
		// word on the frame and level changes that follow.
		SDL_SyncWindow(m_SDLWindow);
		fprintf(stderr, "INFO: left fullscreen for shutdown\n");
	}
	else {
		// Worth being loud about: this is the state that strands the display. It is no longer the last
		// line of defence -- teardown in SDL3Main.cpp notices the flag is still set and forces a display
		// reconfiguration after the window is closed -- but that is recovery from a bad state, not a
		// substitute for leaving fullscreen properly. If this shows up in a log, the budget above and
		// whether the window still had focus are the two things to question.
		fprintf(stderr, "WARNING: still fullscreen after %.1fs (%d attempts) -- shutdown continues, "
			"teardown will kick the display configuration to collapse the orphaned Space\n",
			budgetSeconds, retries + 1);
	}
}
#endif

/**
 * From GameEngine: execute() - main game loop
 */
void SDL3GameEngine::execute(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::execute() - entering main loop\n");
	GameEngine::execute();
	fprintf(stderr, "INFO: SDL3GameEngine::execute() - exited main loop\n");

#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
	// First thing after the loop, before any subsystem is torn down: this is the last moment the
	// window is still key and the app still frontmost, which is what the Space transition needs.
	leaveFullscreenForShutdown();
#endif
}

/**
 * From GameEngine: serviceWindowsOS() - native OS service
 * On Linux, process SDL3 events
 */
void SDL3GameEngine::serviceWindowsOS(void)
{
	pollSDL3Events();
}

/**
 * Check if game has OS focus
 */
Bool SDL3GameEngine::isActive(void)
{
	return m_IsActive;
}

/**
 * Set OS focus status
 */
void SDL3GameEngine::setIsActive(Bool isActive)
{
	m_IsActive = isActive;
}

/**
 * Poll and process SDL3 events
 * Handles keyboard, mouse, window, and quit events
 */
void SDL3GameEngine::pollSDL3Events(void)
{
	if (!m_SDLWindow) {
		return;
	}

	updateTextInputState();

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_EVENT_QUIT:
				m_quitting = true;
				break;

			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				m_quitting = true;
				break;

			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				m_IsActive = true;
				if (TheMouse) {
					TheMouse->regainFocus();
					TheMouse->refreshCursorCapture();
				}
#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
				if (TheShell) {
					TheShell->queueShellMapRefresh();
				}
#endif
				break;

			case SDL_EVENT_WINDOW_FOCUS_LOST:
				m_IsActive = false;
				if (m_IsTextInputActive) {
					SDL_StopTextInput(m_SDLWindow);
					m_IsTextInputActive = false;
					m_TextInputFocusWindow = nullptr;
				}
				if (TheMouse) {
					TheMouse->loseFocus();
				}
				break;

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
			// App suspension/resume: mirror the desktop focus handling so audio
			// and mouse state pause cleanly (the render gate lives in update()).
			case SDL_EVENT_DID_ENTER_BACKGROUND:
				m_IsActive = false;
				if (TheMouse) {
					TheMouse->loseFocus();
				}
				break;

			case SDL_EVENT_DID_ENTER_FOREGROUND:
				m_IsActive = true;
				if (TheMouse) {
					TheMouse->regainFocus();
					TheMouse->refreshCursorCapture();
				}
				break;
#endif

			case SDL_EVENT_WINDOW_MOUSE_ENTER:
				if (TheMouse) {
					TheMouse->onCursorMovedInside();
				}
				break;

			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
				if (TheMouse) {
					TheMouse->onCursorMovedOutside();
				}
				break;

			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
				// GeneralsX @feature 27/07/2026 Cmd+G gives the cursor back to the desktop.
				//
				// Handled here, ahead of the keyboard device, so the G never reaches the game: G is a
				// hotkey in the stock control scheme and the retail key map has to keep working. Cmd is
				// not a modifier the game uses at all, which is what makes it safe to claim.
				//
				// Swallowing both the down and the up matters -- forwarding a lone key-up leaves the
				// keyboard device holding a key it never saw pressed.
				if (event.key.key == SDLK_G && (event.key.mod & SDL_KMOD_GUI) != 0) {
					if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && TheMouse) {
						const Bool released = TheMouse->toggleUserCursorRelease();

						// GeneralsX @bugfix 27/07/2026 Let the released cursor reach the menu bar.
						//
						// Dropping the grab is not enough in a fullscreen Space. The menu bar is hidden by
						// the Space itself, and whether it comes back on hover is SDL's call, made from this
						// hint -- the default answer for a fullscreen SDL entered programmatically is no. So
						// the released cursor could move to the top of the screen and find nothing there.
						//
						// Tied to the release rather than set once, because the bar is in the way the rest of
						// the time: edge scrolling lives at the top of the screen, and a menu bar unhiding
						// over it would be worse than the problem. Confirmed by measurement that the bar
						// reveals on hover while set, hides on leave, and stops appearing once cleared.
						SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_MENU_VISIBILITY, released ? "1" : "0");

						fprintf(stderr, "INFO: Cmd+G: cursor %s\n",
							released ? "released to the desktop" : "captured to the window");
					}
					break;
				}
#endif
				// Fighter19 pattern: direct addSDLEvent() call
				// GeneralsX @refactor felipebraz 16/02/2026 Simplified event routing
				if (TheKeyboard) {
					SDL3Keyboard* keyboard = dynamic_cast<SDL3Keyboard*>(TheKeyboard);
					if (keyboard) {
						keyboard->addSDLEvent(&event);
					}
				}
				break;

			case SDL_EVENT_TEXT_INPUT:
				forwardTextInputEvent(event.text.text);
				break;

			case SDL_EVENT_MOUSE_MOTION:
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			case SDL_EVENT_MOUSE_WHEEL:
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
				// Belt-and-braces: drop SDL's own touch-synthesized mouse events.
				// The gesture translator owns all touch->mouse conversion; double
				// delivery would produce phantom second clicks.
				if (event.motion.which == SDL_TOUCH_MOUSEID) {
					break;
				}
#endif
				// Fighter19 pattern: direct addSDLEvent() call with raw SDL_Event
				// GeneralsX @refactor felipebraz 16/02/2026 Simplified event routing
				if (TheMouse) {
					SDL3Mouse* mouse = dynamic_cast<SDL3Mouse*>(TheMouse);
					if (mouse) {
						mouse->addSDLEvent(&event);
					}
				}
				break;

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
			case SDL_EVENT_FINGER_DOWN:
			case SDL_EVENT_FINGER_MOTION:
			case SDL_EVENT_FINGER_UP:
			case SDL_EVENT_FINGER_CANCELED:
				if (TheMouse && m_SDLWindow) {
					SDL3Mouse* mouse = dynamic_cast<SDL3Mouse*>(TheMouse);
					if (mouse) {
						handleTouchEvent(mouse, m_SDLWindow, event);
					}
				}
				break;
#endif

			case SDL_EVENT_WINDOW_RESIZED:
				handleWindowEvent(event.window);
				break;

#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
			// GeneralsX @bugfix 27/07/2026 A window can be put out of reach without being resized.
			//
			// The off-screen title bar was first seen alongside a resize runaway, so the correction lived
			// in the resize path. But a fullscreen exit restores a frame -- position included -- and Cocoa
			// can land it anywhere it likes, resize or no resize. Answering the move event as well means
			// the position is checked whenever it actually changes.
			case SDL_EVENT_WINDOW_MOVED:
			{
				extern void GeneralsX_EnsureWindowTitleBarOnScreen(void);
				GeneralsX_EnsureWindowTitleBarOnScreen();
				break;
			}

			// GeneralsX @feature 27/07/2026 React to macOS's own fullscreen toggle.
			//
			// Ctrl+Cmd+F and the green zoom button already reached SDL; nothing was listening. Entering is
			// handled straight away; leaving only records the mode, because the window frame is not back
			// yet -- see GeneralsX_OnFullscreenChanged.
			case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
			case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
			{
				extern void GeneralsX_OnFullscreenChanged(Bool nowFullscreen);
				GeneralsX_OnFullscreenChanged(
					event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN ? TRUE : FALSE);
				break;
			}
#endif

			default:
				// Ignore other events for now
				break;
		}

		updateTextInputState();
	}

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
	// Poll the long-press timer every frame; a stationary finger emits no events.
	if (TheMouse && m_SDLWindow) {
		SDL3Mouse* touchMouse = dynamic_cast<SDL3Mouse*>(TheMouse);
		if (touchMouse) {
			updateTouchLongPress(touchMouse, m_SDLWindow);
		}
	}
#endif
}

// GeneralsX @bugfix felipebraz 01/04/2026 Enable SDL text input only while an entry gadget owns focus.
void SDL3GameEngine::updateTextInputState(void)
{
	if (!m_SDLWindow || !TheWindowManager) {
		return;
	}

	GameWindow* focusedWindow = TheWindowManager->winGetFocus();
	const Bool wantsTextInput =
		focusedWindow != nullptr && BitIsSet(focusedWindow->winGetStyle(), GWS_ENTRY_FIELD);

	if (wantsTextInput) {
		if (!m_IsTextInputActive) {
			if (SDL_StartTextInput(m_SDLWindow)) {
				m_IsTextInputActive = true;
			}
		}
		m_TextInputFocusWindow = focusedWindow;
	} else {
		if (m_IsTextInputActive) {
			SDL_StopTextInput(m_SDLWindow);
			m_IsTextInputActive = false;
		}
		m_TextInputFocusWindow = nullptr;
	}
}

// GeneralsX @bugfix felipebraz 01/04/2026 Forward SDL UTF-8 text input through existing GWM_IME_CHAR path.
void SDL3GameEngine::forwardTextInputEvent(const char* utf8Text)
{
	if (!utf8Text || !TheWindowManager) {
		return;
	}

	// GeneralsX @bugfix felipebraz 01/04/2026 Use tracked text-input focus window to keep SDL text delivery stable.
	GameWindow* targetWindow = m_TextInputFocusWindow;
	if (!targetWindow || !BitIsSet(targetWindow->winGetStyle(), GWS_ENTRY_FIELD)) {
		return;
	}

	const size_t textLength = strlen(utf8Text);
	size_t offset = 0;
	while (offset < textLength) {
		UnsignedInt codepoint = 0;
		if (!DecodeNextUtf8Codepoint(utf8Text, textLength, offset, codepoint)) {
			continue;
		}

		// GeneralsX @bugfix felipebraz 01/04/2026 Clamp IME char forwarding to BMP and reject UTF-16 surrogate range.
		if (codepoint == 0 || codepoint > 0x10FFFFU) {
			continue;
		}

		if (codepoint >= 0xD800U && codepoint <= 0xDFFFU) {
			continue;
		}

		if (codepoint > 0xFFFFU) {
			continue;
		}

		const WideChar wideCharacter = static_cast<WideChar>(codepoint);
		TheWindowManager->winSendInputMsg(targetWindow, GWM_IME_CHAR, static_cast<WindowMsgData>(wideCharacter), 0);
	}
}

/**
 * Handle keyboard event -dispatch to Keyboard manager
 * TheSuperHackers @build 10/02/2026 BenderAI - Phase 1.5 event wiring
 */
void SDL3GameEngine::handleKeyboardEvent(const SDL_KeyboardEvent& event)
{
	// Dispatch to SDL3Keyboard if available
	if (TheKeyboard) {
		SDL3Keyboard* sdlKeyboard = dynamic_cast<SDL3Keyboard*>(TheKeyboard);
		if (sdlKeyboard) {
			sdlKeyboard->addSDL3KeyEvent(event);
		}
	}
}

/**
 * Handle mouse motion event - dispatch to Mouse manager
 * TheSuperHackers @build 10/02/2026 BenderAI - Phase 1.5 event wiring
 */
void SDL3GameEngine::handleMouseMotionEvent(const SDL_MouseMotionEvent& event)
{
	// Dispatch to SDL3Mouse if available
	if (TheMouse) {
		SDL3Mouse* sdlMouse = dynamic_cast<SDL3Mouse*>(TheMouse);
		if (sdlMouse) {
			sdlMouse->addSDL3MouseMotionEvent(event);
		}
	}
}

/**
 * Handle mouse button event - dispatch to Mouse manager
 * TheSuperHackers @build 10/02/2026 BenderAI - Phase 1.5 event wiring
 */
void SDL3GameEngine::handleMouseButtonEvent(const SDL_MouseButtonEvent& event)
{
	// Dispatch to SDL3Mouse if available
	if (TheMouse) {
		SDL3Mouse* sdlMouse = dynamic_cast<SDL3Mouse*>(TheMouse);
		if (sdlMouse) {
			sdlMouse->addSDL3MouseButtonEvent(event);
		}
	}
}

/**
 * Handle mouse wheel event - dispatch to Mouse manager
 * TheSuperHackers @build 10/02/2026 BenderAI - Phase 1.5 event wiring
 */
void SDL3GameEngine::handleMouseWheelEvent(const SDL_MouseWheelEvent& event)
{
	// Dispatch to SDL3Mouse if available
	if (TheMouse) {
		SDL3Mouse* sdlMouse = dynamic_cast<SDL3Mouse*>(TheMouse);
		if (sdlMouse) {
			sdlMouse->addSDL3MouseWheelEvent(event);
		}
	}
}

/**
 * Handle window event (resize, etc.)
 */
// GeneralsX @bugfix 27/07/2026 Re-derive the render resolution when the window is resized.
//
// This was an empty stub, and the blur it caused was easy to mistake for a HiDPI problem. The window
// is created SDL_WINDOW_RESIZABLE, so the user can drag it and macOS restores whatever frame they
// last left it at. DXVK's swapchain follows the window, but the engine's render resolution stayed
// wherever it was set at launch, so DX8Wrapper's pillarbox path stretched the old frame across the
// new backbuffer: a window dragged to 1395x925 points showed a 1440x900 render upscaled to
// 2790x1850. Matching the render resolution to the window's real pixel size is what keeps the
// pillarbox a no-op, which is the one thing this port's HiDPI work depends on.
//
// Fullscreen is left alone: that transition is owned by SDL3_ApplyWindowModeForRenderConfig, and
// reacting to the intermediate sizes Cocoa reports mid-animation would fight it.
void SDL3GameEngine::handleWindowEvent(const SDL_WindowEvent& event)
{
#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
	if (!m_SDLWindow || !TheDisplay || !TheWritableGlobalData) {
		return;
	}
	if ((SDL_GetWindowFlags(m_SDLWindow) & SDL_WINDOW_FULLSCREEN) != 0) {
		return;
	}

	// GeneralsX @bugfix 27/07/2026 Ignore resizes the engine itself caused.
	//
	// A resolution pick resizes the window, Cocoa posts this event, and answering it with another
	// setDisplayMode lands back in the code that resized the window. That recursion is what let the
	// window and the render resolution chase each other up to 5140x2004. Only a resize the *user*
	// performed should move the render resolution.
	extern Bool GeneralsX_IsApplyingWindowMode(void);
	if (GeneralsX_IsApplyingWindowMode()) {
		return;
	}

	int pxW = 0, pxH = 0;
	if (!SDL_GetWindowSizeInPixels(m_SDLWindow, &pxW, &pxH) || pxW <= 0 || pxH <= 0) {
		return;
	}

	// The user dragged the window, so this is now the window size everything else derives from.
	extern void GeneralsX_SetWindowPointSize(int pointW, int pointH);
	int ptW = 0, ptH = 0;
	if (SDL_GetWindowSize(m_SDLWindow, &ptW, &ptH) && ptW > 0 && ptH > 0) {
		GeneralsX_SetWindowPointSize(ptW, ptH);
	}

	// Honour the clarity mode. At 100% the render target is the window's full pixel extent, so the
	// pillarbox is a no-op and output is 1:1. At 50% it is half that in each axis and the swapchain
	// upscales by exactly 2 -- soft, but correctly sized and filling the window.
	extern Bool GeneralsX_GetWindowedRenderSize(int& outW, int& outH);
	extern int GeneralsX_GetRenderScalePercent(void);
	int targetW = 0, targetH = 0;
	if (!GeneralsX_GetWindowedRenderSize(targetW, targetH)) {
		return;
	}
	const int percent = GeneralsX_GetRenderScalePercent();

	if ((Int)TheDisplay->getWidth() == targetW && (Int)TheDisplay->getHeight() == targetH) {
		return;		// already correct; nothing to do and no device reset to provoke
	}

	// Resizing is a device reset, so this must not run while the engine is mid-frame. SDL window
	// events are pumped from serviceWindowsOS at the top of the loop, before update and draw, so
	// this is the safe point in the frame to do it.
	if (!TheDisplay->setDisplayMode(targetW, targetH, TheDisplay->getBitDepth(), TRUE)) {
		fprintf(stderr, "WARNING: window resize to %dx%d rejected, staying at %dx%d\n",
			targetW, targetH, (Int)TheDisplay->getWidth(), (Int)TheDisplay->getHeight());
		return;
	}

	// GeneralsX @bugfix 27/07/2026 Record the window's full pixel extent, not the reduced render size.
	//
	// m_xResolution is the resolution the user picked, and the options screen writes it into the
	// "Resolution" preference. At 50% targetW is half the window, so storing it here silently halved
	// the saved resolution every time the window was resized -- the reduced render target leaking into
	// the user's preferences. The window's pixel size is the picked resolution by definition, at any
	// percentage.
	TheWritableGlobalData->m_xResolution = pxW;
	TheWritableGlobalData->m_yResolution = pxH;

	extern void GeneralsX_NotifyResolutionChanged(void);
	GeneralsX_NotifyResolutionChanged();

	fprintf(stderr, "INFO: window resized to %dx%d points, render resolution now %dx%d (%d%%)\n",
		event.data1, event.data2, targetW, targetH, percent);
#endif
}

/**
 * Factory Methods for GameEngine subsystems
 * TheSuperHackers @build felipebraz 13/02/2026
 * Implementations in .cpp to provide complete type definitions and avoid circular includes
 */

LocalFileSystem *SDL3GameEngine::createLocalFileSystem(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::createLocalFileSystem() -> StdLocalFileSystem\n");
	return NEW StdLocalFileSystem;
}

ArchiveFileSystem *SDL3GameEngine::createArchiveFileSystem(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::createArchiveFileSystem() -> StdBIGFileSystem\n");
	return NEW StdBIGFileSystem;
}

GameLogic *SDL3GameEngine::createGameLogic(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::createGameLogic() -> W3DGameLogic\n");
	return NEW W3DGameLogic;
}

GameClient *SDL3GameEngine::createGameClient(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::createGameClient() -> W3DGameClient\n");
	return NEW W3DGameClient;
}

ModuleFactory *SDL3GameEngine::createModuleFactory(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::createModuleFactory() -> W3DModuleFactory\n");
	return NEW W3DModuleFactory;
}

ThingFactory *SDL3GameEngine::createThingFactory(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::createThingFactory() -> W3DThingFactory\n");
	return NEW W3DThingFactory;
}

FunctionLexicon *SDL3GameEngine::createFunctionLexicon(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::createFunctionLexicon() -> W3DFunctionLexicon\n");
	return NEW W3DFunctionLexicon;
}

// GeneralsX @bugfix Copilot 15/04/2026 Match upstream GameEngine pure-virtual signature after sync.
Radar *SDL3GameEngine::createRadar(Bool dummy)
{
	// GeneralsX @bugfix fbraz 04/05/2026 Respect headless mode and create dummy radar.
	// Upstream reference: Win32GameEngine headless factory behavior, TheSuperHackers/GeneralsGameCode
	// https://github.com/TheSuperHackers/GeneralsGameCode
	if (dummy) {
		fprintf(stderr, "INFO: SDL3GameEngine::createRadar() -> RadarDummy (headless)\n");
		return NEW RadarDummy;
	}
	fprintf(stderr, "INFO: SDL3GameEngine::createRadar() -> W3DRadar\n");
	return NEW W3DRadar;
}

// GeneralsX @bugfix Copilot 24/03/2026 Match upstream GameEngine pure-virtual signature after sync.
ParticleSystemManager* SDL3GameEngine::createParticleSystemManager(Bool dummy)
{
	// GeneralsX @bugfix fbraz 04/05/2026 Respect headless mode and create dummy particle manager.
	if (dummy) {
		fprintf(stderr, "INFO: SDL3GameEngine::createParticleSystemManager() -> ParticleSystemManagerDummy (headless)\n");
		return NEW ParticleSystemManagerDummy;
	}
	fprintf(stderr, "INFO: SDL3GameEngine::createParticleSystemManager() -> W3DParticleSystemManager\n");
	return NEW W3DParticleSystemManager;
}

WebBrowser *SDL3GameEngine::createWebBrowser(void)
{
	// WebBrowser uses Windows COM (CComObject<W3DWebBrowser>)
	// Not available on Linux - return nullptr
	fprintf(stderr, "WARNING: WebBrowser not available on Linux platform\n");
	return nullptr;
}

/**
 * Factory method: AudioManager
 * Select audio backend based on compile flags
 * GeneralsX @bugfix Copilot 15/04/2026 Match upstream GameEngine pure-virtual signature after sync.
 */
AudioManager *SDL3GameEngine::createAudioManager(Bool dummy)
{
	(void)dummy;
	fprintf(stderr, "INFO: SDL3GameEngine::createAudioManager()\n");

#ifdef SAGE_USE_OPENAL
	fprintf(stderr, "INFO: Creating OpenAL audio backend\n");
	return new OpenALAudioManager();
#else
	fprintf(stderr, "INFO: Audio backend not available (SAGE_USE_OPENAL not defined)\n");
	fprintf(stderr, "WARNING: Falls back to parent implementation or silent mode\n");
	return GameEngine::createAudioManager();  // Call parent (may return stub)
#endif
}

#endif // !_WIN32
