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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: ExtrasMenu.cpp ////////////////////////////////////////////////////////////////////////////
// Desc:   Cadence panel: render frame rate, simulation speed and a few view preferences.
///////////////////////////////////////////////////////////////////////////////////////////////////

// GeneralsX @feature 26/07/2026 The panel is now built in code instead of from a .wnd layout file.
// The previous revision loaded Menus/ExtrasMenu.wnd, which had three problems that together kept
// this menu from ever shipping:
//   1. The .wnd parent was a full screen opaque USER window pushed on the shell stack, so it could
//      survive into gameplay. The deploy script refused to ship the layout for exactly that reason.
//   2. Every .wnd TEXT field is resolved through TheGameText->fetch(), and no .csf in any language
//      carries labels for a port specific panel, so all of them rendered as MISSING: 'x'. A loose
//      .str file is not an option either: it replaces the whole string table rather than merging.
//   3. Shipping a loose layout file means shipping game data, which has to land in the real game
//      directory as well as the deploy directory to be visible at all.
// Building the windows here removes all three at once: nothing to deploy, no string table
// dependency, and the panel owns its own lifetime like the in game options screen does.

#include "PreRTS.h"

#include "Common/FramePacer.h"
#include "Common/GameLOD.h"
#include "Common/GlobalData.h"
#include "Common/NameKeyGenerator.h"
#include "Common/OptionPreferences.h"
#include "GameClient/Display.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetSlider.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameClient.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GlobalLanguage.h"
#include "GameClient/GUICallbacks.h"
#include "GameClient/HeaderTemplate.h"
#include "GameClient/InGameUI.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/Mouse.h"
#include "GameClient/Shell.h"
#include "GameClient/View.h"
#include "GameClient/WindowLayout.h"
#include "GameLogic/GameLogic.h"
#include "Common/Registry.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"

// PANEL GEOMETRY /////////////////////////////////////////////////////////////////////////////////

// The panel is authored against the engine's 800x600 reference layout and then scaled by a single
// uniform factor, so it keeps its proportions and its share of the screen at any resolution. The
// stock .wnd loader scales x and y independently and leaves fonts at their literal point size,
// which is why stock menus look stretched and their text looks small on a 16:9 4K display.
static const Int DESIGN_WIDTH = 800;
static const Int DESIGN_HEIGHT = 600;

static const Int PANEL_WIDTH = 400;
// Six rows now: the trainer's money slider was added below the five display/speed rows, so the panel
// and everything under the rows moved down by exactly one ROW_STRIDE.
//
// There is deliberately no PANEL_HEIGHT constant. The height follows the last element, which is the
// transient-notice footer, and the footer is measured from its font at runtime rather than fixed in
// design units -- see the footerH calculation in buildPanel. In design units the panel comes to
// FOOTER_TOP 348 + FOOTER_HEIGHT 18 + PANEL_MARGIN 16 = 382, but a face whose cell is taller than
// FOOTER_HEIGHT makes both the footer and the panel grow together, which is the whole point.
static const Int PANEL_MARGIN = 16;

static const Int TITLE_TOP = 10;
// GeneralsX @bugfix 10/08/2026 Text boxes sized for Latin text clipped every Chinese line.
//
// A box of H design units gets H*scale pixels, but the font it holds is asked for P*scale points
// and the engine converts points to pixels at 96 DPI, so the glyphs occupy P*scale*96/72 pixels.
// A CJK line then needs about 1.34x its em for ascent plus descent (measured: Songti SC, PingFang
// and SimSun all report asc 33 + desc 10 = 43px at 32px em), so the box must satisfy
// H >= P * 96/72 * 1.34, i.e. H >= 1.79 * P. At the old numbers a 10 point label needed 18 and had
// 15, and a 14 point title needed 26 and had 22. Because both sides scale together the shortfall
// was proportional, so it clipped at every resolution and merely became more visible as the screen
// grew. Latin text hid it: its ascent plus descent is nearer 1.0x the em.
static const Int TITLE_HEIGHT = 26;

static const Int ROW_TOP = 40;
// A row is a label above a slider. Stride is label height plus the slider block plus a small gap:
// 18 + (20 - 18) + 15 = 35, rounded up to 38 so consecutive rows do not touch.
static const Int ROW_STRIDE = 38;
static const Int ROW_LABEL_HEIGHT = 18;
static const Int ROW_SLIDER_TOP = 20;
static const Int ROW_SLIDER_HEIGHT = 15;

// The thumb is sized here rather than left at the engine's fixed 13x16, which is authored in the
// 800x600 reference space and never scaled: at 4K it comes out a dozen pixels wide on a track over a
// thousand pixels long. Square and as tall as the track reads clearly at every scale.
static const Int ROW_THUMB_WIDTH = 15;

// The scaling mode row sits below the sliders: a caption line naming the active mode and the
// resolution it produces, with a switch button under it. The button is wide because it spells out
// the mode it switches to, and the Chinese caption is eight full width glyphs.
// Six rows of ROW_STRIDE from ROW_TOP end at 40 + 6*38 = 268, so this clears them.
static const Int MODE_TOP = 272;
static const Int MODE_LABEL_HEIGHT = 18;
static const Int MODE_BUTTON_TOP = 294;
static const Int MODE_BUTTON_WIDTH = 168;
static const Int MODE_BUTTON_HEIGHT = 22;

// Three buttons plus two gaps is 360 against a content width of 368, so the row fits inside the
// margins with room to spare. It used to total 354 against 348 and hung over both edges.
static const Int BUTTON_TOP = 324;
static const Int BUTTON_WIDTH = 112;
static const Int BUTTON_HEIGHT = 22;
static const Int BUTTON_GAP = 12;

// The normally empty line under the buttons is reserved for short save/error confirmations. Live
// FPS and logic-step telemetry used to be drawn here, but constantly changing text was distracting.
//
// GeneralsX @bugfix 10/08/2026 This was the one box the CJK sizing pass above missed, because its
// height was written as a bare literal at the call site instead of a named constant here. It held
// 12 units while carrying the same 10 point labelFont as every row label, and the rule documented
// at TITLE_HEIGHT requires H >= 1.79 * P, i.e. 18. The descender of a Chinese line was therefore
// cut off, and only on the last line of the panel, which is exactly where a clipped box is hardest
// to attribute to anything. Named here so the next sizing change cannot skip it again.
static const Int FOOTER_GAP = 2;
static const Int FOOTER_HEIGHT = 18;
static const Int FOOTER_TOP = BUTTON_TOP + BUTTON_HEIGHT + FOOTER_GAP;

// CONTROL DEFINITIONS ////////////////////////////////////////////////////////////////////////////

enum ExtrasRow CPP_11(: Int)
{
	ROW_RENDER_FPS = 0,			///< render frame rate cap
	ROW_GAME_SPEED,					///< simulation speed, in tenths of the retail 30 Hz cadence
	ROW_CAMERA_PITCH,				///< camera pitch angle
	ROW_SCROLL_SPEED,				///< keyboard/edge scroll factor, percent
	ROW_MAX_ZOOM,					///< maximum user camera height, percent of the normal limit
	ROW_MONEY,							///< trainer: the local player's cash, live, single player only

	ROW_COUNT
};

struct RowDef
{
	const char *textLabel;		///< .csf label consulted first, so a translation can be dropped in later
	const WideChar *fallback;	///< used when the label is absent, which is the normal case today
	const WideChar *fallbackZh;	///< used instead when the Chinese localization is the active one
	Int minValue;
	Int maxValue;
};

// Game speed is carried in tenths so the slider can express 0.5x through 6.0x without a float.
// 10 tenths is the retail 30 Hz simulation cadence.
static const RowDef s_rows[ROW_COUNT] =
{
	{ "GUI:GXRenderFps",		L"Render frame rate",	L"渲染帧率",		30,		240 },
	{ "GUI:GXGameSpeed",		L"Game speed",				L"游戏速度",		5,		60 },
	{ "GUI:GXCameraPitch",	L"Camera pitch",			L"镜头俯角",		20,		60 },
	{ "GUI:GXScrollSpeed",	L"Scroll speed",			L"卷屏速度",		1,		200 },
	{ "GUI:GXMaxZoom",			L"Maximum zoom range",	L"最大缩放范围",	100,	150 },
	{ "GUI:GXMoney",				L"Money",							L"资金",				0,		500000 },
};

// GeneralsX @tweak 10/08/2026 Keep the public default at original game speed.
// A saved local preference may still use 2.2x, but Defaults means 60 render FPS with the original
// 30 Hz simulation instead of silently accelerating a fresh install. The money entry remains a
// placeholder and is never applied by the Defaults button.
static const Int s_rowDefaults[ROW_COUNT] = { 60, 10, 37, 100, 100, 0 };

// GeneralsX @feature Codex 21/08/2026 Keep specialist controls on a second page. The first entry is
// the manual minimum terrain range; ordinary zooming raises the effective range automatically.
enum MoreRow CPP_11(: Int)
{
	MORE_ROW_DRAW_DISTANCE = 0,
	MORE_ROW_HEALTH_BARS,
	MORE_ROW_COUNT
};

static const RowDef s_moreRows[MORE_ROW_COUNT] =
{
	{ "GUI:GXDrawDistance", L"Minimum draw distance", L"最低绘制距离", 100, 200 },
	{ "GUI:GXHealthBars", L"Health bars", L"单位血条", 0, 2 },
};
static const Int s_moreRowDefaults[MORE_ROW_COUNT] = { 100, 0 };

enum ExtrasPage CPP_11(: Int)
{
	EXTRAS_PAGE_MAIN = 0,
	EXTRAS_PAGE_MORE,
};

// PANEL STATE ////////////////////////////////////////////////////////////////////////////////////

static WindowLayout *s_layout = nullptr;
static GameWindow *s_panel = nullptr;
static GameWindow *s_rowLabel[ROW_COUNT] = { nullptr };
static GameWindow *s_rowSlider[ROW_COUNT] = { nullptr };
static GameWindow *s_titleLabel = nullptr;
static GameWindow *s_footerLabel = nullptr;
static GameWindow *s_buttonDefaults = nullptr;
static GameWindow *s_buttonSave = nullptr;
static GameWindow *s_buttonClose = nullptr;
static GameWindow *s_modeLabel = nullptr;
static GameWindow *s_buttonMode = nullptr;
static GameWindow *s_buttonMore = nullptr;
static GameWindow *s_buttonBack = nullptr;
static GameWindow *s_moreRowLabel[MORE_ROW_COUNT] = { nullptr };
static GameWindow *s_moreRowSlider[MORE_ROW_COUNT] = { nullptr };
static const Int HEALTH_MODE_COUNT = 3;
static GameWindow *s_healthModeButton[HEALTH_MODE_COUNT] = { nullptr };
static ExtrasPage s_activePage = EXTRAS_PAGE_MAIN;
static Bool s_registeredWithInGameUI = FALSE;
enum FooterNotice CPP_11(: Int)
{
	FOOTER_NOTICE_NONE = 0,
	FOOTER_NOTICE_SAVED,
	FOOTER_NOTICE_SAVE_FAILED,
	FOOTER_NOTICE_SCALE_FAILED,
};
static FooterNotice s_footerNotice = FOOTER_NOTICE_NONE;
static UnsignedInt s_footerNoticeUntil = 0;
static Bool s_pendingResolutionRebuild = FALSE;
static ExtrasPage s_pendingResolutionPage = EXTRAS_PAGE_MAIN;
// A clarity switch changes both the render coordinate system and every UI hit-test coordinate.
// Store the requested mode rather than applying it from the button's mouse-up callback. The engine
// consumes it at the beginning of a later outer tick, after the old input dispatch has unwound.
static Int s_pendingScaleMode = -1;


// LOCALIZATION ///////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
/** TRUE when the Chinese localization is the active one.
	* The engine has no LANGUAGE_ID for Chinese -- the enum in Language.h stops at Korean, and the
	* Chinese release is a translation patch that ships as its own Data/Chinese tree rather than a new
	* language id. GetRegistryLanguage is what actually selects that tree (see HeaderTemplate.cpp and
	* GameText.cpp, which both build their paths from it), so it is the honest signal here too.
	* UserPreferences.cpp already tests it the same way. */
//-------------------------------------------------------------------------------------------------
static Bool isChineseLocale()
{
	static Bool cached = FALSE;
	static Bool result = FALSE;
	if (!cached) {
		cached = TRUE;
		result = (GetRegistryLanguage().compareNoCase("chinese") == 0);
	}
	return result;
}

//-------------------------------------------------------------------------------------------------
/** Pick the built in caption for the active language.
	* A .csf label still wins when one exists, so a future translation drops in without touching this. */
//-------------------------------------------------------------------------------------------------
static const WideChar *localFallback( const WideChar *english, const WideChar *chinese )
{
	return isChineseLocale() ? chinese : english;
}

//-------------------------------------------------------------------------------------------------
static UnicodeString panelText( const char *label, const WideChar *english, const WideChar *chinese )
{
	if (!TheGameText) {
		return UnicodeString(localFallback(english, chinese));
	}
	return TheGameText->FETCH_OR_SUBSTITUTE(label, localFallback(english, chinese));
}

// SCALING MODE ///////////////////////////////////////////////////////////////////////////////////

#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#define GX_HAS_SCALE_MODE 1

// Defined in W3DDevice/GameClient/W3DDisplay.cpp. Declared here rather than included because this
// file lives in the platform independent GameEngine layer and must not pull in device headers; the
// same local-extern pattern W3DDisplay.cpp itself uses for TheSDL3Window.
extern bool GeneralsX_GetMacDisplayMetrics(int& outPointsW, int& outPointsH,
																					 int& outPixelsW, int& outPixelsH, float& outDensity);
extern void GeneralsX_SetRenderScalePercent(int percent);
extern int GeneralsX_GetRenderScalePercent(void);
extern Bool GeneralsX_ApplyRenderScaleToWindow(void);
extern Bool GeneralsX_GetWindowPointSize(int& pointW, int& pointH);
extern Bool GeneralsX_GetWindowPositionInPoints(int& pointX, int& pointY);
extern Bool GeneralsX_GetFullscreenRenderSize(int& outW, int& outH);

// GeneralsX @tweak 10/08/2026 The two clarity paths, which exist for two DIFFERENT reasons. Only one
// of them is about sharpness, and describing them as "sharper vs cheaper" is what previously led point
// mode to target the point grid instead of the panel.
//
//   NATIVE   follow the system's scaled framebuffer -- what macOS calls HiDPI. The point of this mode
//            is UI SIZE: the game's text and panels come out the size the rest of the desktop taught
//            the user to expect, and rendering at the full framebuffer is what stops that from costing
//            any sharpness. Costs the most, because this engine's fixed function pipeline is being
//            emulated and was written for 1024x768.
//
//   POINT    one rendered pixel per PHYSICAL PANEL PIXEL, which is the sharpest image the display can
//            physically show. Its accepted cost is a smaller UI: fonts scale at 0.7x the resolution
//            ratio (ResolutionFontAdjustment) while layout boxes scale proportionally, so everything
//            reads smaller at a higher render resolution. That tradeoff is the mode's purpose.
//
// On a display whose framebuffer already IS its panel -- a stock Retina Mac at default scaling -- both
// modes resolve to the same resolution. That is the honest answer, not a collapsed setting: there,
// rendering at the system framebuffer already is 1:1 with the panel.
//
// The stored percentage no longer determines the POINT target on Apple desktops; see
// GeneralsX_GetFullscreenRenderSize in W3DDisplay.cpp. It remains the persisted mode selector and the
// fallback for displays that report no native mode.
enum ScaleMode CPP_11(: Int)
{
	SCALE_MODE_NATIVE = 0,
	SCALE_MODE_POINT,

	SCALE_MODE_COUNT
};

//-------------------------------------------------------------------------------------------------
/** Render scale percentage that corresponds to point-for-point rendering on this display.
	* Derived from the live density rather than assumed to be 50: a 2x panel gives 50, but a 1.5x
	* scaled mode gives 67, and a non HiDPI display gives 100 (where both modes are the same thing). */
//-------------------------------------------------------------------------------------------------
static Int pointModePercent()
{
	int ptW = 0, ptH = 0, pxW = 0, pxH = 0;
	float density = 1.0f;
	if (!GeneralsX_GetMacDisplayMetrics(ptW, ptH, pxW, pxH, density) || density <= 0.0f) {
		return 100;
	}
	const Int percent = REAL_TO_INT_FLOOR(100.0f / density + 0.5f);
	return clamp<Int>(50, percent, 100);
}

//-------------------------------------------------------------------------------------------------
/** TRUE when this display has no separate point and pixel grid, so the mode switch is meaningless. */
//-------------------------------------------------------------------------------------------------
static Bool displayHasHiDpi()
{
	int ptW = 0, ptH = 0, pxW = 0, pxH = 0;
	float density = 1.0f;
	if (!GeneralsX_GetMacDisplayMetrics(ptW, ptH, pxW, pxH, density)) {
		return FALSE;
	}
	return density > 1.01f;
}

//-------------------------------------------------------------------------------------------------
/** The active mode, read from the preference SDL3Main actually consumes at launch.
	* Reading the stored percentage rather than comparing live resolutions keeps this correct in
	* windowed mode, where the engine resolution is the user's configured size and matches neither
	* the screen's point size nor its pixel size. */
//-------------------------------------------------------------------------------------------------
static Int getScaleMode()
{
	// GeneralsX @bugfix 27/07/2026 Read the live value, not the file.
	//
	// During a deferred display reset the file can still contain the preceding mode, so it does not
	// answer "which mode am I in right now". Asking the display layer does, and it is the same value
	// every sizing path uses, so the label cannot disagree with what is on screen.
	return (GeneralsX_GetRenderScalePercent() >= 100) ? SCALE_MODE_NATIVE : SCALE_MODE_POINT;
}

//-------------------------------------------------------------------------------------------------
/** Switch modes and apply the choice live when the display can take it.
	* Returns TRUE if the change took effect now, FALSE if the display rejected it.
	*
	* Persistence is deliberately handled after this returns successfully. A scale change rebuilds
	* the display and closes Extras, so requiring a separate Remember click after it would mean asking
	* the player to reopen the panel merely to keep the mode they just selected. */
//-------------------------------------------------------------------------------------------------
static Bool applyScaleMode( Int mode )
{
	const Int percent = (mode == SCALE_MODE_POINT) ? pointModePercent() : 100;
	const Int oldPercent = GeneralsX_GetRenderScalePercent();

	if (!TheDisplay) {
		return FALSE;
	}

	// Tell the display layer first: both paths below size themselves against this.
	GeneralsX_SetRenderScalePercent(percent);

	// GeneralsX @bugfix 27/07/2026 In a window the switch changes the render resolution, not the
	// window. The window keeps whatever size the user gave it; the mode decides how many pixels are
	// drawn into it -- its full pixel extent at 100%, half in each axis at 50% with the swapchain
	// upscaling by 2. An earlier version resized the window instead, which is what made 50% render
	// the game at double size with the window framing only its top-left corner.
	if (TheDisplay->getWindowed()) {
		if (GeneralsX_ApplyRenderScaleToWindow()) {
			return TRUE;
		}
		GeneralsX_SetRenderScalePercent(oldPercent);
		return FALSE;
	}

	// GeneralsX @bugfix 10/08/2026 Ask the display layer for the target instead of recomputing it.
	//
	// The percentage is no longer the whole story in point mode -- on a supersampling display it
	// resolves to the panel's own pixel grid, which no integer percentage of the framebuffer hits.
	// Sharing the helper is what keeps this switch, the fullscreen transition, and the launch path
	// from disagreeing about what point mode means.
	Int targetW = 0, targetH = 0;
	if (!GeneralsX_GetFullscreenRenderSize(targetW, targetH)) {
		GeneralsX_SetRenderScalePercent(oldPercent);
		return FALSE;
	}
	if ((Int)TheDisplay->getWidth() == targetW && (Int)TheDisplay->getHeight() == targetH) {
		return TRUE;	// already there
	}

	const Int oldW = TheDisplay->getWidth();
	const Int oldH = TheDisplay->getHeight();

	// Same sequence the stock options screen uses for a resolution change, including the two
	// onResolutionChanged notifications; skipping those leaves fonts and the cursor at the old scale.
	if (!TheDisplay->setDisplayMode(targetW, targetH, TheDisplay->getBitDepth(), FALSE)) {
		fprintf(stderr, "WARNING: ExtrasMenu: scale mode %d -> %dx%d rejected, staying at %dx%d\n",
			mode, targetW, targetH, oldW, oldH);
		GeneralsX_SetRenderScalePercent(oldPercent);
		return FALSE;
	}

	if (TheWritableGlobalData) {
		TheWritableGlobalData->m_xResolution = targetW;
		TheWritableGlobalData->m_yResolution = targetH;
	}
	extern void GeneralsX_NotifyResolutionChanged(void);
	GeneralsX_NotifyResolutionChanged();

	fprintf(stderr, "INFO: ExtrasMenu: scale mode %s applied, %dx%d -> %dx%d (percent %d)\n",
		(mode == SCALE_MODE_POINT) ? "POINT" : "NATIVE", oldW, oldH, targetW, targetH, percent);

	return TRUE;
}

#endif // Apple desktop

// GEOMETRY AND FONT HELPERS //////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
/** One uniform scale factor for the whole panel, plus the offset that centres it. */
//-------------------------------------------------------------------------------------------------
static Real getPanelScale()
{
	if (!TheDisplay) {
		return 1.0f;
	}

	const Real xScale = (Real)TheDisplay->getWidth() / (Real)DESIGN_WIDTH;
	const Real yScale = (Real)TheDisplay->getHeight() / (Real)DESIGN_HEIGHT;

	// The smaller of the two keeps the panel square on wide displays. Taking them independently is
	// what stretches the stock menus on 16:9.
	Real scale = min(xScale, yScale);
	if (scale < 0.5f) {
		scale = 0.5f;
	}
	return scale;
}

//-------------------------------------------------------------------------------------------------
static Int scaleValue( Int designValue, Real scale )
{
	return REAL_TO_INT_FLOOR((Real)designValue * scale);
}

//-------------------------------------------------------------------------------------------------
/** Resolve a font at a point size that tracks the panel scale.
	* The name and weight come from the header template so the localized face is used: the Chinese
	* and Korean packages replace Arial with a face that actually has the glyphs. Only the point
	* size is recomputed, because the template point size is tuned for the reference layout. */
//-------------------------------------------------------------------------------------------------
static GameFont *resolvePanelFont( const char *templateName, Int designPoint, Real scale )
{
	AsciiString fontName;
	Bool bold = FALSE;

	if (TheHeaderTemplateManager) {
		HeaderTemplate *ht = TheHeaderTemplateManager->findHeaderTemplate(AsciiString(templateName));
		if (ht && ht->m_fontName.isNotEmpty()) {
			fontName = ht->m_fontName;
			bold = ht->m_bold;
		}
	}

	if (fontName.isEmpty() && TheGlobalLanguageData &&
			TheGlobalLanguageData->m_defaultWindowFont.name.isNotEmpty()) {
		fontName = TheGlobalLanguageData->m_defaultWindowFont.name;
		bold = TheGlobalLanguageData->m_defaultWindowFont.bold;
	}

	if (fontName.isEmpty()) {
		fontName = "Arial";
	}

	Int point = REAL_TO_INT_FLOOR((Real)designPoint * scale);
	point = clamp<Int>(8, point, 100);	// FontLibrary::getFont rejects anything outside 1..100

	GameFont *font = TheWindowManager ? TheWindowManager->winFindFont(fontName, point, bold) : nullptr;
	if (!font && TheWindowManager) {
		// Some faces only exist at a few sizes. Fall back to the unscaled size rather than no font.
		font = TheWindowManager->winFindFont(fontName, designPoint, bold);
	}
	return font;
}

// TRAINER ////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
/** The local player's wallet, or nullptr when editing it would not be legitimate.
	*
	* Unlike every other row on this panel, money is simulation state: it is part of the lockstep model
	* and it is CRC'd. Writing it outside a local single player game would desync a network match or
	* invalidate a replay, which the port's determinism rule forbids outright. So this returns nullptr
	* in LAN and internet games, during replay playback, and in the shell -- where the "local player"
	* is whatever the menu background map happens to own and there is no game to cheat in. Skirmish is
	* allowed: it is a local game against the AI, with no peer to disagree with. */
//-------------------------------------------------------------------------------------------------
static Money *trainerMoney()
{
	if (!TheGameLogic || !ThePlayerList) {
		return nullptr;
	}
	if (!TheGameLogic->isInGame() || TheGameLogic->isInShellGame()) {
		return nullptr;
	}
	if (TheGameLogic->isInMultiplayerGame() || TheGameLogic->isInReplayGame()) {
		return nullptr;
	}

	Player *player = ThePlayerList->getLocalPlayer();
	return player ? player->getMoney() : nullptr;
}

// VALUE PLUMBING /////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
static Int getLiveRowValue( Int row )
{
	switch (row)
	{
		case ROW_RENDER_FPS:
			return TheFramePacer ? TheFramePacer->getFramesPerSecondLimit() : s_rowDefaults[row];

		case ROW_GAME_SPEED:
		{
			if (!TheFramePacer) {
				return s_rowDefaults[row];
			}
			// Round to the nearest tenth so a 66 FPS logic cadence reads back as 2.2x, not 2.1x.
			return (TheFramePacer->getLogicTimeScaleFps() * 10 + LOGICFRAMES_PER_SECOND / 2) / LOGICFRAMES_PER_SECOND;
		}

		// These three are stored as Reals, so the readback has to round rather than truncate.
		// Flooring makes a value round-trip badly: 105 stored as 1.05f comes back as 104.99998,
		// which floors to 104, and the slider then drifts down by one every time the panel opens.
		case ROW_CAMERA_PITCH:
			return TheGlobalData ? REAL_TO_INT_FLOOR(TheGlobalData->m_cameraPitch + 0.5f) : s_rowDefaults[row];

		case ROW_SCROLL_SPEED:
			return TheGlobalData ? REAL_TO_INT_FLOOR(TheGlobalData->m_keyboardScrollFactor * 100.0f + 0.5f) : s_rowDefaults[row];

		case ROW_MAX_ZOOM:
			return TheGlobalData ? REAL_TO_INT_FLOOR(TheGlobalData->m_maxCameraHeightScale * 100.0f + 0.5f) : s_rowDefaults[row];

		case ROW_MONEY:
		{
			// Clamped to the slider's range so a player who is already richer than the maximum does not
			// get their cash silently cut down to it when the panel opens and the thumb is placed.
			const Money *money = trainerMoney();
			if (!money) {
				return 0;
			}
			return clamp<Int>(s_rows[row].minValue, (Int)money->countMoney(), s_rows[row].maxValue);
		}
	}
	return s_rowDefaults[row];
}

//-------------------------------------------------------------------------------------------------
static Int getLiveMoreRowValue( Int row )
{
	switch (row)
	{
		case MORE_ROW_DRAW_DISTANCE:
			return TheGlobalData
				? REAL_TO_INT_FLOOR(TheGlobalData->m_terrainDrawDistanceScale * 100.0f + 0.5f)
				: s_moreRowDefaults[row];

		case MORE_ROW_HEALTH_BARS:
			return TheGlobalData
				? clamp<Int>(s_moreRows[row].minValue,
					TheGlobalData->m_healthBarDisplayMode, s_moreRows[row].maxValue)
				: s_moreRowDefaults[row];
	}
	return s_moreRowDefaults[row];
}

//-------------------------------------------------------------------------------------------------
/** Push one row's value into the engine.
	* Nothing here touches simulation state. The render cap is a presentation setting; the logic time
	* scale is explicitly ignored in network games by FramePacer, so a replay or a multiplayer match
	* cannot be desynced from this panel. */
//-------------------------------------------------------------------------------------------------
static void applyRowValue( Int row, Int value )
{
	value = clamp<Int>(s_rows[row].minValue, value, s_rows[row].maxValue);

	switch (row)
	{
		case ROW_RENDER_FPS:
		{
			if (TheFramePacer) {
				TheFramePacer->setFramesPerSecondLimit(value);
				TheFramePacer->enableFramesPerSecondLimit(TRUE);
			}
			if (TheWritableGlobalData) {
				TheWritableGlobalData->m_useFpsLimit = TRUE;
				TheWritableGlobalData->m_framesPerSecondLimit = value;
			}
			break;
		}

		case ROW_GAME_SPEED:
		{
			// GeneralsX @bugfix 26/07/2026 The render cap and the logic cadence are independent.
			// The old code raised the render cap to match the logic rate whenever the speed slider
			// went past it, which silently overwrote the user's frame rate choice and made the two
			// sliders fight each other. The fixed-step accumulator in the main loop already runs as
			// many logic steps per render frame as the ratio needs, so a 6.0x simulation at 60
			// render frames is 3 logic steps per frame and perfectly legal.
			if (TheFramePacer) {
				const Int logicFps = (value * LOGICFRAMES_PER_SECOND + 5) / 10;
				TheFramePacer->setLogicTimeScaleFps(logicFps);
				TheFramePacer->enableLogicTimeScale(TRUE);
			}
			break;
		}

		case ROW_CAMERA_PITCH:
		{
			if (TheWritableGlobalData) {
				TheWritableGlobalData->m_cameraPitch = (Real)value;
			}
			if (TheTacticalView)
			{
				// GeneralsX @feature Codex 21/08/2026 Make the existing pitch preference a live
				// tactical-camera control. Always update the cached default so later camera resets
				// and new matches use it, but do not disturb a locked, scripted, or cinematic view.
				TheTacticalView->setDefaultPitch(DEG_TO_RADF((Real)value));
				if (TheGameLogic &&
					GameLogic::isInInteractiveGame(TheGameLogic->getGameMode()) &&
					TheTacticalView->isUserControlled() &&
					!TheTacticalView->isUserControlLocked() &&
					!TheTacticalView->isDoingScriptedCamera())
				{
					TheTacticalView->setPitchToDefault();
				}
			}
			break;
		}

		case ROW_SCROLL_SPEED:
		{
			if (TheWritableGlobalData) {
				TheWritableGlobalData->m_keyboardScrollFactor = (Real)value / 100.0f;
			}
			break;
		}

		case ROW_MAX_ZOOM:
		{
			if (TheWritableGlobalData) {
				TheWritableGlobalData->m_maxCameraHeightScale = (Real)value / 100.0f;
			}
			if (TheTacticalView) {
				// Recalculate the limit live, but leave the current height alone. The player can continue
				// outward with the normal zoom control, and a shell/cinematic camera is not interrupted.
				TheTacticalView->setCameraHeightAboveGroundLimitsToDefault();
			}
			break;
		}

		case ROW_MONEY:
		{
			Money *money = trainerMoney();
			if (!money) {
				break;		// not a legitimate place to edit cash; trainerMoney explains which places are
			}

			// Move to the target with deposit/withdraw rather than setStartingCash. setStartingCash
			// zeroes the 60-second income ring buffer, which would blank the "cash per minute" readout
			// the sidebar shows. Both calls are made silently and without income tracking, so the
			// trainer edit does not announce itself with the money chime or show up as earnings.
			const UnsignedInt current = money->countMoney();
			const UnsignedInt target = (UnsignedInt)value;
			if (target > current) {
				money->deposit(target - current, FALSE, FALSE);
			} else if (target < current) {
				money->withdraw(current - target, FALSE);
			}
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
static void applyMoreRowValue( Int row, Int value )
{
	value = clamp<Int>(s_moreRows[row].minValue, value, s_moreRows[row].maxValue);

	switch (row)
	{
		case MORE_ROW_DRAW_DISTANCE:
		{
			if (TheWritableGlobalData) {
				TheWritableGlobalData->m_terrainDrawDistanceScale = (Real)value / 100.0f;
			}
			break;
		}

		case MORE_ROW_HEALTH_BARS:
		{
			// GeneralsX @feature Codex 21/08/2026 This is presentation-only state. Drawable
			// reads it while drawing visible objects; no Object or lockstep simulation data changes.
			if (TheWritableGlobalData) {
				TheWritableGlobalData->m_healthBarDisplayMode = value;
			}
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Refresh one row's caption from its slider position. */
//-------------------------------------------------------------------------------------------------
static void refreshRowLabel( Int row )
{
	if (!s_rowLabel[row] || !s_rowSlider[row] || !TheGameText) {
		return;
	}

	const Int value = GadgetSliderGetPosition(s_rowSlider[row]);
	const UnicodeString name = panelText(s_rows[row].textLabel, s_rows[row].fallback, s_rows[row].fallbackZh);

	UnicodeString text;
	switch (row)
	{
		case ROW_GAME_SPEED:
		{
			const Int logicFps = (value * LOGICFRAMES_PER_SECOND + 5) / 10;
			text.format(L"%s:  %d.%dx  (%d Hz)", name.str(), value / 10, value % 10, logicFps);
			break;
		}

		case ROW_RENDER_FPS:
		{
			text.format(L"%s:  %d FPS", name.str(), value);
			break;
		}

		case ROW_CAMERA_PITCH:
		{
			text.format(L"%s:  %d", name.str(), value);
			break;
		}

		case ROW_MONEY:
		{
			// Say why the row is inert rather than showing a plausible looking $0 that does nothing.
			if (trainerMoney() == nullptr) {
				const UnicodeString unavailable = panelText("GUI:GXMoneyUnavailable",
					L"single player only", L"仅单人游戏可用");
				text.format(L"%s:  %s", name.str(), unavailable.str());
			} else {
				text.format(L"%s:  $%d", name.str(), value);
			}
			break;
		}

		default:
		{
			text.format(L"%s:  %d%%", name.str(), value);
			break;
		}
	}

	GadgetStaticTextSetText(s_rowLabel[row], text);
}

//-------------------------------------------------------------------------------------------------
static void refreshAllRowLabels()
{
	for (Int row = 0; row < ROW_COUNT; ++row) {
		refreshRowLabel(row);
	}
}

//-------------------------------------------------------------------------------------------------
static void refreshMoreRowLabel( Int row )
{
	if (!s_moreRowLabel[row] || !TheGameText) {
		return;
	}

	const UnicodeString name = panelText(s_moreRows[row].textLabel,
		s_moreRows[row].fallback, s_moreRows[row].fallbackZh);
	if (row == MORE_ROW_HEALTH_BARS)
	{
		static const char *modeLabels[HEALTH_MODE_COUNT] =
		{
			"GUI:GXHealthBarsOriginal",
			"GUI:GXHealthBarsDamaged",
			"GUI:GXHealthBarsAlways",
		};
		static const WideChar *modeFallbacks[HEALTH_MODE_COUNT] =
		{
			L"Original",
			L"Damaged",
			L"Always",
		};
		static const WideChar *modeFallbacksZh[HEALTH_MODE_COUNT] =
		{
			L"原版",
			L"受伤时",
			L"始终显示",
		};
		const Int selectedMode = clamp<Int>(0, getLiveMoreRowValue(row), HEALTH_MODE_COUNT - 1);
		const UnicodeString selectedModeText = panelText(modeLabels[selectedMode],
			modeFallbacks[selectedMode], modeFallbacksZh[selectedMode]);
		UnicodeString labelText;
		labelText.format(L"%s:  %s", name.str(), selectedModeText.str());
		GadgetStaticTextSetText(s_moreRowLabel[row], labelText);

		for (Int mode = 0; mode < HEALTH_MODE_COUNT; ++mode)
		{
			if (!s_healthModeButton[mode]) {
				continue;
			}
			const UnicodeString modeText = panelText(modeLabels[mode], modeFallbacks[mode], modeFallbacksZh[mode]);
			if (mode == selectedMode)
			{
				UnicodeString selectedText;
				selectedText.format(L"[%s]", modeText.str());
				GadgetButtonSetText(s_healthModeButton[mode], selectedText);
			}
			else
			{
				GadgetButtonSetText(s_healthModeButton[mode], modeText);
			}
		}
		return;
	}

	if (!s_moreRowSlider[row]) {
		return;
	}
	const Int value = GadgetSliderGetPosition(s_moreRowSlider[row]);
	UnicodeString text;
	text.format(L"%s:  %d%%", name.str(), value);
	GadgetStaticTextSetText(s_moreRowLabel[row], text);
}

//-------------------------------------------------------------------------------------------------
static void refreshAllMoreRowLabels()
{
	for (Int row = 0; row < MORE_ROW_COUNT; ++row) {
		refreshMoreRowLabel(row);
	}
}

//-------------------------------------------------------------------------------------------------
/** Switch between the ordinary controls and the expandable specialist page without rebuilding the
	* panel. Hiding existing children is input-safe and leaves room for more advanced rows later. */
//-------------------------------------------------------------------------------------------------
static void showPage( ExtrasPage page )
{
	s_activePage = page;
	const Bool showMain = page == EXTRAS_PAGE_MAIN;

	for (Int row = 0; row < ROW_COUNT; ++row)
	{
		if (s_rowLabel[row]) {
			s_rowLabel[row]->winHide(!showMain);
		}
		if (s_rowSlider[row]) {
			s_rowSlider[row]->winHide(!showMain);
		}
	}

	for (Int row = 0; row < MORE_ROW_COUNT; ++row)
	{
		if (s_moreRowLabel[row]) {
			s_moreRowLabel[row]->winHide(showMain);
		}
		if (s_moreRowSlider[row]) {
			s_moreRowSlider[row]->winHide(showMain);
		}
	}
	for (Int mode = 0; mode < HEALTH_MODE_COUNT; ++mode) {
		if (s_healthModeButton[mode]) {
			s_healthModeButton[mode]->winHide(showMain);
		}
	}

	if (s_modeLabel) {
		s_modeLabel->winHide(!showMain);
	}
	if (s_buttonMode) {
		s_buttonMode->winHide(!showMain);
	}
	if (s_buttonMore) {
		s_buttonMore->winHide(!showMain);
	}
	if (s_buttonBack) {
		s_buttonBack->winHide(showMain);
	}

	if (s_titleLabel)
	{
		GadgetStaticTextSetText(s_titleLabel, showMain
			? panelText("GUI:GXCadenceTitle", L"Display and Game Speed", L"画面与速度设置")
			: panelText("GUI:GXMoreSettingsTitle", L"More Settings", L"更多设置"));
	}
}

//-------------------------------------------------------------------------------------------------
/** Describe the active scaling mode and the resolution it actually produces.
	* The resolution is the useful half: "native" and "point" mean nothing without the numbers, and
	* seeing 4096x2304 versus 2048x1152 is what makes the tradeoff legible. */
//-------------------------------------------------------------------------------------------------
static void refreshModeLabel()
{
#ifdef GX_HAS_SCALE_MODE
	if (!s_modeLabel) {
		return;
	}

	const Int mode = getScaleMode();
	const UnicodeString name = panelText("GUI:GXScaleMode", L"Clarity", L"清晰度");

	UnicodeString modeName;
	if (mode == SCALE_MODE_POINT) {
		modeName = panelText("GUI:GXScaleModePoint", L"point for point", L"像素点对点");
	} else {
		modeName = panelText("GUI:GXScaleModeNative", L"native HiDPI", L"原生 HiDPI");
	}

	// Report what is on screen now, not what the preference says, so a saved-but-not-applied switch
	// cannot look like it already took effect.
	const Int liveW = TheDisplay ? (Int)TheDisplay->getWidth() : 0;
	const Int liveH = TheDisplay ? (Int)TheDisplay->getHeight() : 0;

	// GeneralsX @tweak 27/07/2026 Name the window mode and, when windowed, the window's own point
	// size as well as the render size. Reporting one bare resolution was actively misleading: in a
	// window that number is the configured Resolution in render pixels, which is neither the screen's
	// resolution nor the size of the window on screen, so it read like the game had picked a display
	// mode the monitor does not have.
	const Bool windowed = TheDisplay ? TheDisplay->getWindowed() : FALSE;
	const UnicodeString windowMode = windowed
		? panelText("GUI:GXWindowed", L"windowed", L"窗口")
		: panelText("GUI:GXFullscreen", L"fullscreen", L"全屏");

	UnicodeString text;
	if (windowed)
	{
		// GeneralsX @bugfix 27/07/2026 Ask for the window's point size instead of recomputing it.
		// The old arithmetic here inverted the same broken rule that made 50% draw the game at double
		// size, so the label agreed with the bug rather than reporting it. The window's size is now
		// stored state; reading it back cannot disagree with what is on screen.
		Int winPtW = 0, winPtH = 0;
		if (GeneralsX_GetWindowPointSize(winPtW, winPtH))
		{
			// L"\x2192" is a right arrow: wchar_t is 4-byte UTF-32 here, so a codepoint escape is
			// correct and a UTF-8 byte sequence would come out as three garbage characters.
			text.format(L"%s:  %s  [%s]  %dx%d \x2192 %dx%d",
				name.str(), modeName.str(), windowMode.str(), winPtW, winPtH, liveW, liveH);
		}
		else
		{
			text.format(L"%s:  %s  [%s]  (%dx%d)",
				name.str(), modeName.str(), windowMode.str(), liveW, liveH);
		}
	}
	else
	{
		text.format(L"%s:  %s  [%s]  (%dx%d)",
			name.str(), modeName.str(), windowMode.str(), liveW, liveH);
	}
	GadgetStaticTextSetText(s_modeLabel, text);

	if (s_buttonMode) {
		// The button names the mode it will switch TO, so the click's outcome is stated before it
		// happens rather than having to be inferred from the current mode.
		UnicodeString buttonText;
		if (mode == SCALE_MODE_POINT) {
			buttonText = panelText("GUI:GXScaleModeToNative", L"Use native HiDPI", L"切换为原生 HiDPI");
		} else {
			buttonText = panelText("GUI:GXScaleModeToPoint", L"Use point for point", L"切换为像素点对点");
		}
		GadgetButtonSetText(s_buttonMode, buttonText);
	}
#endif
}

//-------------------------------------------------------------------------------------------------
/** Dismiss the transient footer immediately, for example when the player starts another edit. */
//-------------------------------------------------------------------------------------------------
static void clearFooterNotice()
{
	s_footerNotice = FOOTER_NOTICE_NONE;
	s_footerNoticeUntil = 0;
	if (s_footerLabel) {
		GadgetStaticTextSetText(s_footerLabel, UnicodeString());
	}
}

//-------------------------------------------------------------------------------------------------
/** Show a short save/error confirmation, or clear it once its deadline has passed.
	* There is deliberately no default live telemetry: rapidly changing FPS and logic-step text made
	* the otherwise static settings panel flicker without helping normal play. */
//-------------------------------------------------------------------------------------------------
static void refreshFooterLabel()
{
	if (!s_footerLabel || !TheGameText) {
		return;
	}
	if (s_footerNotice == FOOTER_NOTICE_NONE) {
		return;
	}

	UnicodeString text;
	const UnsignedInt now = timeGetTime();
	if (now < s_footerNoticeUntil)
	{
		if (s_footerNotice == FOOTER_NOTICE_SAVED) {
			text = panelText("GUI:GXSettingsRemembered",
				L"Remembered for future launches.", L"设置已保存，今后启动时继续使用。");
		} else if (s_footerNotice == FOOTER_NOTICE_SAVE_FAILED) {
			text = panelText("GUI:GXSettingsSaveFailed",
				L"Could not save Options.ini.", L"无法保存 Options.ini。");
		} else {
			text = panelText("GUI:GXScaleModeRejected",
				L"Could not switch mode right now.", L"当前无法切换模式。");
		}
		GadgetStaticTextSetText(s_footerLabel, text);
		return;
	}
	clearFooterNotice();
}

// PERSISTENCE ////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
/** Write the current live settings to Options.ini.
	*
	* GeneralsX @bugfix 14/08/2026 Do not read the controls back here. A point/HiDPI reset must close
	* this panel before rebuilding the display, and the current presentation state also changes through
	* native macOS window controls outside this menu. Reading the engine's live state makes Remember
	* one coherent snapshot instead of a partial copy of whichever gadgets still happen to exist. */
//-------------------------------------------------------------------------------------------------
static Bool savePreferences()
{
	OptionPreferences pref;
	AsciiString value;

	value.format("%d", getLiveRowValue(ROW_RENDER_FPS));
	pref["GXRenderFPS"] = value;

	value.format("%d", getLiveRowValue(ROW_GAME_SPEED));
	pref["GXGameSpeedTenths"] = value;

	value.format("%d", getLiveRowValue(ROW_CAMERA_PITCH));
	pref["CameraPitch"] = value;

	value.format("%d", getLiveRowValue(ROW_SCROLL_SPEED));
	pref["ScrollFactor"] = value;

	value.format("%.2f", (Real)getLiveRowValue(ROW_MAX_ZOOM) / 100.0f);
	pref["MaxCameraHeightScale"] = value;

	// Store the engine scale, not the slider's percentage. Earlier candidates wrote 105 for 1.05.
	value.format("%.2f", (Real)getLiveMoreRowValue(MORE_ROW_DRAW_DISTANCE) / 100.0f);
	pref["TerrainDrawDistanceScale"] = value;

	value.format("%d", getLiveMoreRowValue(MORE_ROW_HEALTH_BARS));
	pref["HealthBarDisplayMode"] = value;

	// ROW_MONEY is deliberately absent. It is live game state, not a setting: persisting it would
	// mean a saved "preference" that silently hands the player cash at the start of every session.

#ifdef GX_HAS_SCALE_MODE
	value.format("%d", GeneralsX_GetRenderScalePercent());
	pref["GXRenderScalePercent"] = value;

	// GeneralsX @feature 14/08/2026 Remember the live macOS presentation mode for the launcher.
	// The launcher used to pass -fullscreen unconditionally, so a native Ctrl+Cmd+F/green-button
	// transition could never outlive this process even after the player explicitly chose Remember.
	pref["GXWindowed"] = TheDisplay && TheDisplay->getWindowed() ? "yes" : "no";

	// Save a manually resized window as the next windowed resolution. In fullscreen m_xResolution can
	// be the panel-sized render target injected at launch; writing that would destroy the last useful
	// window size, so a fullscreen snapshot deliberately leaves Resolution alone.
	if (TheDisplay && TheDisplay->getWindowed() && TheGlobalData &&
		TheGlobalData->m_xResolution > 0 && TheGlobalData->m_yResolution > 0)
	{
		value.format("%d %d", TheGlobalData->m_xResolution, TheGlobalData->m_yResolution);
		pref["Resolution"] = value;

		// GeneralsX @feature 14/08/2026 Remember the content window's position in macOS points.
		// Size alone made every launch begin wherever Cocoa placed the bootstrap window, which could
		// overlap the Dock even when the player had carefully fitted and positioned it beforehand.
		Int pointX = 0, pointY = 0;
		if (GeneralsX_GetWindowPositionInPoints(pointX, pointY)) {
			value.format("%d %d", pointX, pointY);
			pref["GXWindowPosition"] = value;
		}
	}
#endif

	return pref.write();
}

#ifdef GX_HAS_SCALE_MODE
//-------------------------------------------------------------------------------------------------
/** Persist only a successfully applied clarity mode.
	*
	* The display reset intentionally closes Extras, so this one setting cannot reasonably wait for a
	* later Remember click. Other live previews remain session-only until Remember is selected. */
//-------------------------------------------------------------------------------------------------
static Bool saveScaleModePreference()
{
	OptionPreferences pref;
	AsciiString value;
	value.format("%d", GeneralsX_GetRenderScalePercent());
	pref["GXRenderScalePercent"] = value;
	return pref.write();
}
#endif

// PANEL CONSTRUCTION /////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
static GameWindow *createLabel( GameWindow *parent, Int x, Int y, Int width, Int height,
																GameFont *font, Bool centered )
{
	WinInstanceData instData;
	instData.init();
	instData.m_style = GWS_STATIC_TEXT;
	instData.m_status = WIN_STATUS_ENABLED | WIN_STATUS_NO_INPUT | WIN_STATUS_NO_FOCUS;
	instData.m_owner = parent;
	instData.m_font = font;

	TextData textData;
	textData.text = nullptr;
	textData.centered = centered;
	textData.centeredVertically = TRUE;
	textData.leftMargin = 0;
	textData.topMargin = 0;

	GameWindow *label = TheWindowManager->gogoGadgetStaticText(parent, instData.m_status,
		x, y, width, height, &instData, &textData, font, FALSE);

	if (label) {
		// No background: the panel behind it already provides one.
		label->winSetEnabledTextColors(
			TheWindowManager->winMakeColor(255, 255, 255, 255),
			TheWindowManager->winMakeColor(0, 0, 0, 255));
		if (font) {
			label->winSetFont(font);
		}
	}

	return label;
}

//-------------------------------------------------------------------------------------------------
/** Build one slider, with a thumb that is actually visible at this display scale.
	*
	* Two things have to be done by hand here.
	*
	* The track and thumb are drawn with colours, not images. assignDefaultGadgetLook asks for
	* "HSliderEnabledLeftEnd", "HSliderThumbEnabled" and friends, and none of those names exist in the
	* retail mapped images: every horizontal slider in the shipped game supplies its own artwork
	* through its .wnd layout instead. With WIN_STATUS_IMAGE set, the image renderer would find null
	* images, fall through to the colour renderer for the track, and draw nothing at all for the thumb
	* -- a slider with no visible handle. Asking for the colour renderer up front and naming the
	* colours is honest about what is actually being drawn.
	*
	* The thumb is then resized. gogoGadgetSlider does create it, but at the fixed 13x16 from
	* Gadget.h, which is never scaled by display size. GSM_SET_MIN_MAX afterwards recomputes the tick
	* spacing against the new width, which is what keeps the thumb's travel matched to the value
	* range. */
//-------------------------------------------------------------------------------------------------
static GameWindow *createSlider( GameWindow *parent, Int x, Int y, Int width, Int height,
																 GameFont *font, Int minValue, Int maxValue,
																 Int thumbWidth )
{
	WinInstanceData instData;
	instData.init();
	instData.m_style = GWS_HORZ_SLIDER | GWS_MOUSE_TRACK;
	instData.m_status = WIN_STATUS_ENABLED;
	instData.m_owner = parent;
	instData.m_font = font;

	SliderData sliderData;
	sliderData.minVal = minValue;
	sliderData.maxVal = maxValue;
	sliderData.numTicks = 0.0f;		// filled in by gogoGadgetSlider
	sliderData.position = minValue;

	GameWindow *slider = TheWindowManager->gogoGadgetSlider(parent, instData.m_status,
		x, y, width, height, &instData, &sliderData, font, TRUE);

	if (!slider) {
		return nullptr;
	}

	// The track: a sunken dark channel. All three states are the same because a slider that changes
	// colour under the cursor reads as a button, and the default look would make it red.
	const Color trackFill = TheWindowManager->winMakeColor(8, 11, 17, 255);
	const Color trackBorder = TheWindowManager->winMakeColor(96, 104, 118, 255);
	GadgetSliderSetEnabledColor(slider, trackFill);
	GadgetSliderSetEnabledBorderColor(slider, trackBorder);
	GadgetSliderSetDisabledColor(slider, trackFill);
	GadgetSliderSetDisabledBorderColor(slider, trackBorder);
	GadgetSliderSetHiliteColor(slider, trackFill);
	GadgetSliderSetHiliteBorderColor(slider, trackBorder);

	GameWindow *thumb = GadgetSliderGetThumb(slider);
	if (thumb)
	{
		// Fill the track's full height and sit flush inside it. A thumb that overhangs the track would
		// not survive the first drag: the window manager clips a dragged child to its parent's box.
		thumb->winSetSize(thumbWidth, height);
		thumb->winSetPosition(0, 0);

		// Light handle, bright border, brighter still while held.
		const Color thumbFill = TheWindowManager->winMakeColor(168, 178, 196, 255);
		const Color thumbBorder = TheWindowManager->winMakeColor(236, 240, 248, 255);
		const Color thumbHeld = TheWindowManager->winMakeColor(214, 224, 240, 255);
		GadgetButtonSetEnabledColor(thumb, thumbFill);
		GadgetButtonSetEnabledBorderColor(thumb, thumbBorder);
		GadgetButtonSetEnabledSelectedColor(thumb, thumbHeld);
		GadgetButtonSetEnabledSelectedBorderColor(thumb, thumbBorder);
		GadgetButtonSetHiliteColor(thumb, thumbHeld);
		GadgetButtonSetHiliteBorderColor(thumb, thumbBorder);
		GadgetButtonSetHiliteSelectedColor(thumb, thumbHeld);
		GadgetButtonSetHiliteSelectedBorderColor(thumb, thumbBorder);

		// Re-derive numTicks from the thumb's real width. gogoGadgetSlider computed it against the
		// stock 13, which would leave the thumb's travel and the reported value out of step.
		TheWindowManager->winSendSystemMsg(slider, GSM_SET_MIN_MAX, minValue, maxValue);
	}

	return slider;
}

//-------------------------------------------------------------------------------------------------
/** Dress a push button in the stock three piece button artwork.
	*
	* assignDefaultGadgetLook assigns "PushButtonEnabled", "PushButtonHilite" and so on. Those names
	* appear in the retail .wnd layouts but are not defined as mapped images anywhere in the shipped
	* data, so winFindImage returns null for all of them. The image renderer then has nothing to draw
	* and the button comes out as bare floating text with no plate behind it.
	*
	* What the retail menus actually use is a left cap, a tiling middle and a right cap -- the
	* Buttons-* set, which does exist. Naming them here gives the panel the same button look as the
	* rest of the game, and the tiling middle means it stretches to any width without distortion. */
//-------------------------------------------------------------------------------------------------
static void applyStockButtonLook( GameWindow *button )
{
	if (!button || !TheWindowManager) {
		return;
	}

	// Slot 0 is the left cap, 5 the tiling middle, 6 the right cap; the *Selected variants (1, 3, 4)
	// are the pressed state. GadgetButtonGetMiddleEnabledImage being non-null is what selects the
	// three piece renderer over the single image one.
	GadgetButtonSetLeftEnabledImage(button, TheWindowManager->winFindImage("Buttons-Left"));
	GadgetButtonSetMiddleEnabledImage(button, TheWindowManager->winFindImage("Buttons-Middle"));
	GadgetButtonSetRightEnabledImage(button, TheWindowManager->winFindImage("Buttons-Right"));

	GadgetButtonSetLeftEnabledSelectedImage(button, TheWindowManager->winFindImage("Buttons-Pushed-Left"));
	GadgetButtonSetMiddleEnabledSelectedImage(button, TheWindowManager->winFindImage("Buttons-Pushed-Middle"));
	GadgetButtonSetRightEnabledSelectedImage(button, TheWindowManager->winFindImage("Buttons-Pushed-Right"));

	GadgetButtonSetLeftHiliteImage(button, TheWindowManager->winFindImage("Buttons-HiLite-Left"));
	GadgetButtonSetMiddleHiliteImage(button, TheWindowManager->winFindImage("Buttons-HiLite-Middle"));
	GadgetButtonSetRightHiliteImage(button, TheWindowManager->winFindImage("Buttons-HiLite-Right"));

	GadgetButtonSetLeftHiliteSelectedImage(button, TheWindowManager->winFindImage("Buttons-Pushed-Left"));
	GadgetButtonSetMiddleHiliteSelectedImage(button, TheWindowManager->winFindImage("Buttons-Pushed-Middle"));
	GadgetButtonSetRightHiliteSelectedImage(button, TheWindowManager->winFindImage("Buttons-Pushed-Right"));

	GadgetButtonSetLeftDisabledImage(button, TheWindowManager->winFindImage("Buttons-Disabled-Left"));
	GadgetButtonSetMiddleDisabledImage(button, TheWindowManager->winFindImage("Buttons-Disabled-Middle"));
	GadgetButtonSetRightDisabledImage(button, TheWindowManager->winFindImage("Buttons-Disabled-Right"));

	GadgetButtonSetLeftDisabledSelectedImage(button, TheWindowManager->winFindImage("Buttons-Disabled-Left"));
	GadgetButtonSetMiddleDisabledSelectedImage(button, TheWindowManager->winFindImage("Buttons-Disabled-Middle"));
	GadgetButtonSetRightDisabledSelectedImage(button, TheWindowManager->winFindImage("Buttons-Disabled-Right"));

	// The stock plate is dark, so the retail menus use a bright caption with a black outline and turn
	// it lime while hovered. Matching that keeps the hover state legible on the hilite artwork.
	button->winSetEnabledTextColors(
		TheWindowManager->winMakeColor(255, 255, 255, 255),
		TheWindowManager->winMakeColor(0, 0, 0, 255));
	button->winSetHiliteTextColors(
		TheWindowManager->winMakeColor(186, 255, 12, 255),
		TheWindowManager->winMakeColor(0, 2, 0, 255));
	button->winSetDisabledTextColors(
		TheWindowManager->winMakeColor(62, 64, 92, 255),
		TheWindowManager->winMakeColor(31, 32, 47, 255));
}

//-------------------------------------------------------------------------------------------------
static GameWindow *createButton( GameWindow *parent, Int x, Int y, Int width, Int height,
																 GameFont *font, const char *textLabel, const WideChar *fallback,
																 const WideChar *fallbackZh )
{
	WinInstanceData instData;
	instData.init();
	instData.m_style = GWS_PUSH_BUTTON | GWS_MOUSE_TRACK;
	instData.m_status = WIN_STATUS_ENABLED | WIN_STATUS_IMAGE;
	instData.m_owner = parent;
	instData.m_font = font;

	GameWindow *button = TheWindowManager->gogoGadgetPushButton(parent, instData.m_status,
		x, y, width, height, &instData, font, TRUE);

	if (button) {
		if (font) {
			button->winSetFont(font);
		}
		applyStockButtonLook(button);
		GadgetButtonSetText(button, panelText(textLabel, fallback, fallbackZh));
	}

	return button;
}

//-------------------------------------------------------------------------------------------------
/** Build the panel. Returns FALSE and leaves nothing behind if the window system is not up. */
//-------------------------------------------------------------------------------------------------
static Bool buildPanel()
{
	if (!TheWindowManager || !TheDisplay) {
		return FALSE;
	}

	const Real scale = getPanelScale();

	// Fonts first: the footer's height is measured from labelFont, and the panel's height is measured
	// from the footer, so neither dimension can be computed until the font exists.
	GameFont *titleFont = resolvePanelFont("Title", 14, scale);
	GameFont *labelFont = resolvePanelFont("LabelRegular", 10, scale);
	GameFont *buttonFont = resolvePanelFont("Button", 10, scale);

	// GeneralsX @bugfix 10/08/2026 The last line is sized from the font, not from a design constant.
	//
	// FOOTER_HEIGHT applies the H >= 1.79 * P rule, but 1.79 is derived from one CJK face's
	// ascent+descent and every face differs: Songti and PingFang both report 1060/-340 per 1000 em
	// (1.40 of em), while Noto Serif SC reports 1151/-286 (1.44). The public build ships Noto, so a
	// constant tuned against the host's Songti left it about 3% short there -- clipping again, on the
	// one machine nobody testing this can see.
	//
	// GameFont::height is W3DGameFont's Get_Char_Height, which render2dsentence computes as
	// CharAscent + descent after expanding both to the face's own FT bbox. That is the real cell the
	// glyphs are rasterized into, so taking it directly makes this correct for any face, including
	// one a user drops in themselves. The design constant stays as the floor so a face reporting an
	// unusually tight cell cannot make the row smaller than the layout expects.
	const Int footerH = max<Int>(scaleValue(FOOTER_HEIGHT, scale), labelFont ? labelFont->height : 0);

	const Int panelW = scaleValue(PANEL_WIDTH, scale);
	const Int panelH = scaleValue(FOOTER_TOP, scale) + footerH + scaleValue(PANEL_MARGIN, scale);
	const Int panelX = (TheDisplay->getWidth() - panelW) / 2;
	const Int panelY = (TheDisplay->getHeight() - panelH) / 2;

	// The panel body. No WIN_STATUS_IMAGE, so the default draw fills it with the enabled colour:
	// a near opaque dark plate that stays readable over both the shell art and the battlefield.
	WinInstanceData panelInstData;
	panelInstData.init();
	panelInstData.m_style = GWS_USER_WINDOW;
	panelInstData.m_status = WIN_STATUS_ENABLED | WIN_STATUS_BORDER | WIN_STATUS_NO_FOCUS;
	panelInstData.m_font = labelFont;

	s_panel = TheWindowManager->winCreate(nullptr, panelInstData.m_status,
		panelX, panelY, panelW, panelH, ExtrasMenuSystem, &panelInstData);

	if (!s_panel) {
		return FALSE;
	}

	s_panel->winSetInputFunc(ExtrasMenuInput);
	s_panel->winSetEnabledColor(0, TheWindowManager->winMakeColor(12, 16, 24, 235));
	s_panel->winSetEnabledBorderColor(0, TheWindowManager->winMakeColor(180, 190, 205, 255));

	const Int contentX = scaleValue(PANEL_MARGIN, scale);
	const Int contentW = panelW - 2 * contentX;

	s_titleLabel = createLabel(s_panel, contentX, scaleValue(TITLE_TOP, scale),
		contentW, scaleValue(TITLE_HEIGHT, scale), titleFont, TRUE);
	if (s_titleLabel) {
		GadgetStaticTextSetText(s_titleLabel,
			panelText("GUI:GXCadenceTitle", L"Display and Game Speed", L"画面与速度设置"));
	}

	for (Int row = 0; row < ROW_COUNT; ++row)
	{
		const Int rowY = scaleValue(ROW_TOP + row * ROW_STRIDE, scale);

		s_rowLabel[row] = createLabel(s_panel, contentX, rowY,
			contentW, scaleValue(ROW_LABEL_HEIGHT, scale), labelFont, FALSE);

		s_rowSlider[row] = createSlider(s_panel, contentX, rowY + scaleValue(ROW_SLIDER_TOP, scale),
			contentW, scaleValue(ROW_SLIDER_HEIGHT, scale), labelFont,
			s_rows[row].minValue, s_rows[row].maxValue, scaleValue(ROW_THUMB_WIDTH, scale));

		if (s_rowSlider[row]) {
			GadgetSliderSetPosition(s_rowSlider[row],
				clamp<Int>(s_rows[row].minValue, getLiveRowValue(row), s_rows[row].maxValue));

			// Grey the money slider out wherever editing cash is not legitimate, so it cannot be
			// dragged in a network game, during a replay, or in the shell. The label says why.
			if (row == ROW_MONEY && trainerMoney() == nullptr) {
				s_rowSlider[row]->winEnable(FALSE);
			}
		}
	}

	// The second page is created once and hidden, rather than destroying and rebuilding child windows
	// from a button callback. Its empty rows intentionally leave room for future specialist settings.
	for (Int row = 0; row < MORE_ROW_COUNT; ++row)
	{
		const Int rowY = scaleValue(ROW_TOP + row * ROW_STRIDE, scale);
		s_moreRowLabel[row] = createLabel(s_panel, contentX, rowY,
			contentW, scaleValue(ROW_LABEL_HEIGHT, scale), labelFont, FALSE);

		if (row == MORE_ROW_HEALTH_BARS)
		{
			const Int buttonGap = scaleValue(6, scale);
			const Int buttonW = (contentW - 2 * buttonGap) / HEALTH_MODE_COUNT;
			const Int buttonY = rowY + scaleValue(ROW_SLIDER_TOP, scale);
			for (Int mode = 0; mode < HEALTH_MODE_COUNT; ++mode)
			{
				s_healthModeButton[mode] = createButton(s_panel,
					contentX + mode * (buttonW + buttonGap), buttonY,
					buttonW, scaleValue(MODE_BUTTON_HEIGHT, scale), buttonFont,
					"GUI:GXHealthBarsOriginal", L"Original", L"原版");
			}
			continue;
		}

		s_moreRowSlider[row] = createSlider(s_panel, contentX, rowY + scaleValue(ROW_SLIDER_TOP, scale),
			contentW, scaleValue(ROW_SLIDER_HEIGHT, scale), labelFont,
			s_moreRows[row].minValue, s_moreRows[row].maxValue, scaleValue(ROW_THUMB_WIDTH, scale));

		if (s_moreRowSlider[row]) {
			GadgetSliderSetPosition(s_moreRowSlider[row],
				clamp<Int>(s_moreRows[row].minValue, getLiveMoreRowValue(row), s_moreRows[row].maxValue));
		}
	}

	// The scaling mode row. Only built where there is a point/pixel distinction to switch between:
	// on a non HiDPI display the two modes render identically, so offering the choice would be a
	// control that visibly does nothing.
#ifdef GX_HAS_SCALE_MODE
	if (displayHasHiDpi())
	{
		s_modeLabel = createLabel(s_panel, contentX, scaleValue(MODE_TOP, scale),
			contentW, scaleValue(MODE_LABEL_HEIGHT, scale), labelFont, FALSE);

		const Int modeButtonW = scaleValue(MODE_BUTTON_WIDTH, scale);
		s_buttonMode = createButton(s_panel, contentX, scaleValue(MODE_BUTTON_TOP, scale),
			modeButtonW, scaleValue(MODE_BUTTON_HEIGHT, scale), buttonFont,
			"GUI:GXScaleModeSwitch", L"Switch", L"切换");

		refreshModeLabel();
	}
#endif

	// GeneralsX @feature Codex 21/08/2026 The main page links to an expandable specialist page.
	// On HiDPI displays it shares the row with the clarity switch; elsewhere it is centred by itself.
	const Int modeButtonW = scaleValue(MODE_BUTTON_WIDTH, scale);
	const Int modeButtonH = scaleValue(MODE_BUTTON_HEIGHT, scale);
	Int moreButtonX = (panelW - modeButtonW) / 2;
	if (s_buttonMode) {
		moreButtonX = contentX + contentW - modeButtonW;
	}
	s_buttonMore = createButton(s_panel, moreButtonX, scaleValue(MODE_BUTTON_TOP, scale),
		modeButtonW, modeButtonH, buttonFont,
		"GUI:GXMoreSettings", L"More settings", L"更多设置");
	s_buttonBack = createButton(s_panel, (panelW - modeButtonW) / 2, scaleValue(MODE_BUTTON_TOP, scale),
		modeButtonW, modeButtonH, buttonFont,
		"GUI:GXBack", L"Back", L"返回");

	// GeneralsX @tweak 14/08/2026 The middle action is deliberately called Remember rather
	// than Save: every control is a live preview already, and this button's distinct job is to write
	// those live values to Options.ini for later launches. Keep three buttons centred as a group.
	const Int buttonW = scaleValue(BUTTON_WIDTH, scale);
	const Int buttonH = scaleValue(BUTTON_HEIGHT, scale);
	const Int buttonGap = scaleValue(BUTTON_GAP, scale);
	const Int buttonRowW = 3 * buttonW + 2 * buttonGap;
	const Int buttonY = scaleValue(BUTTON_TOP, scale);
	Int buttonX = (panelW - buttonRowW) / 2;

	s_buttonDefaults = createButton(s_panel, buttonX, buttonY, buttonW, buttonH, buttonFont,
		"GUI:GXDefaults", L"Defaults", L"默认值");
	buttonX += buttonW + buttonGap;
	s_buttonSave = createButton(s_panel, buttonX, buttonY, buttonW, buttonH, buttonFont,
		"GUI:GXRemember", L"Remember", L"记住设置");
	buttonX += buttonW + buttonGap;
	s_buttonClose = createButton(s_panel, buttonX, buttonY, buttonW, buttonH, buttonFont,
		"GUI:GXClose", L"Close", L"关闭");

	// Status line under the buttons, inside the bottom margin. Positioned from FOOTER_TOP rather
	// than from the button row's runtime pixels, and given the measured footerH computed above --
	// the same value panelH was derived from, so the bottom margin is PANEL_MARGIN by construction
	// whatever the font turns out to be.
	s_footerLabel = createLabel(s_panel, contentX, scaleValue(FOOTER_TOP, scale),
		contentW, footerH, labelFont, TRUE);

	refreshAllRowLabels();
	refreshAllMoreRowLabels();
	refreshFooterLabel();
	showPage(EXTRAS_PAGE_MAIN);

	// Worth logging: this is the one place that proves the panel scaled itself to the actual display
	// rather than to the 800x600 reference, which is the whole point of building it in code.
	DEBUG_LOG(("ExtrasMenu: panel %dx%d at (%d,%d), scale %.2f, display %dx%d",
		panelW, panelH, panelX, panelY, scale, TheDisplay->getWidth(), TheDisplay->getHeight()));

	return TRUE;
}

// LIFECYCLE //////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
Bool IsExtrasMenuVisible( void )
{
	return s_panel != nullptr;
}


//-------------------------------------------------------------------------------------------------
/** Tear the panel down completely.
	* Destroying the windows and dropping the layout is what keeps this panel from leaking into
	* gameplay the way the old shell stack version did. There is no hidden-but-alive state. */
//-------------------------------------------------------------------------------------------------
void CloseExtrasMenu( void )
{
	if (!s_panel && !s_layout) {
		return;
	}

	if (s_registeredWithInGameUI && TheInGameUI && s_layout) {
		TheInGameUI->unregisterWindowLayout(s_layout);
	}
	s_registeredWithInGameUI = FALSE;

	if (s_layout) {
		// destroyWindows walks the layout's window list, which owns the panel and all its children.
		s_layout->destroyWindows();
		deleteInstance(s_layout);
		s_layout = nullptr;
	}
	else if (s_panel) {
		TheWindowManager->winDestroy(s_panel);
	}

	s_panel = nullptr;
	s_titleLabel = nullptr;
	s_footerLabel = nullptr;
	s_buttonDefaults = nullptr;
	s_buttonSave = nullptr;
	s_buttonClose = nullptr;
	s_modeLabel = nullptr;
	s_buttonMode = nullptr;
	s_buttonMore = nullptr;
	s_buttonBack = nullptr;
	s_activePage = EXTRAS_PAGE_MAIN;
	s_footerNotice = FOOTER_NOTICE_NONE;
	s_footerNoticeUntil = 0;
	s_pendingResolutionRebuild = FALSE;
	for (Int row = 0; row < ROW_COUNT; ++row) {
		s_rowLabel[row] = nullptr;
		s_rowSlider[row] = nullptr;
	}
	for (Int row = 0; row < MORE_ROW_COUNT; ++row) {
		s_moreRowLabel[row] = nullptr;
		s_moreRowSlider[row] = nullptr;
	}
	for (Int mode = 0; mode < HEALTH_MODE_COUNT; ++mode) {
		s_healthModeButton[mode] = nullptr;
	}
}

//-------------------------------------------------------------------------------------------------
/** Build the panel and make it live. Shared by the hotkey toggle and a rejected mode switch. */
//-------------------------------------------------------------------------------------------------
static void openPanel( void )
{
	if (!TheWindowManager || !TheDisplay) {
		return;
	}

	if (!buildPanel()) {
		CloseExtrasMenu();
		return;
	}

	// Wrap the panel in a layout so teardown is a single destroyWindows call and the in-game UI can
	// keep it in the same lifecycle as its other temporary layouts.
	s_layout = newInstance(WindowLayout);
	if (s_layout) {
		s_layout->addWindow(s_panel);
		s_layout->setUpdate(ExtrasMenuUpdate);

		// isInGame() is also true for the shell's 3D background map, and InGameUI::update only runs
		// registered layouts during real play, so gate on the interactive modes only. Transient footer
		// expiry is handled by the engine's outer update and therefore works in both shell and gameplay.
		if (TheInGameUI && TheGameLogic && GameLogic::isInInteractiveGame(TheGameLogic->getGameMode())) {
			TheInGameUI->registerWindowLayout(s_layout);
			s_registeredWithInGameUI = TRUE;
		}
	}

	s_panel->winBringToTop();

	if (TheMouse) {
		TheMouse->setCursor(Mouse::ARROW);
	}
}

//-------------------------------------------------------------------------------------------------
/** Open or close the panel.
	* Deliberately does not pause the simulation so live slider effects remain visible. */
//-------------------------------------------------------------------------------------------------
void ToggleExtrasMenu( void )
{
	if (IsExtrasMenuVisible()) {
		CloseExtrasMenu();
		return;
	}

	openPanel();
}

//-------------------------------------------------------------------------------------------------
/** A panel built in code has no immutable .wnd geometry for the generic reflow pass. Do not destroy
	* it from a Cocoa/SDL resize callback; remember its page and rebuild on the next outer engine tick. */
//-------------------------------------------------------------------------------------------------
void NotifyExtrasMenuResolutionChanged( void )
{
	if (IsExtrasMenuVisible()) {
		s_pendingResolutionPage = s_activePage;
		s_pendingResolutionRebuild = TRUE;
	}
}

//-------------------------------------------------------------------------------------------------
/** Finish work which must not run from inside a gadget's input callback.
	* SDL3GameEngine calls this at the beginning of a later outer tick. A render-scale change rebuilds
	* the display coordinate system, so the
	* old panel is closed before that work and deliberately stays closed afterwards. Keeping a window
	* created for the old coordinate system alive across the reset is what produced the stale z-order
	* and hit-test state seen on macOS. */
//-------------------------------------------------------------------------------------------------
void ProcessExtrasMenuDeferredActions( void )
{
	// This outer update also runs while the shell owns the screen, unlike a self-owned layout update.
	// It only clears an expired notice once; no footer text is regenerated on ordinary frames.
	if (s_footerNotice != FOOTER_NOTICE_NONE && timeGetTime() >= s_footerNoticeUntil) {
		clearFooterNotice();
	}

	if (s_pendingResolutionRebuild) {
		const ExtrasPage page = s_pendingResolutionPage;
		s_pendingResolutionRebuild = FALSE;
		CloseExtrasMenu();
		openPanel();
		if (IsExtrasMenuVisible()) {
			showPage(page);
		}
	}

#ifdef GX_HAS_SCALE_MODE
	if (s_pendingScaleMode < 0) {
		return;
	}

	const Int requestedMode = s_pendingScaleMode;
	s_pendingScaleMode = -1;
	const Bool reopenOnFailure = IsExtrasMenuVisible();
	CloseExtrasMenu();
	if (applyScaleMode(requestedMode))
	{
		// GeneralsX @bugfix 14/08/2026 Do not resurrect UI from the old render coordinate system.
		// Ctrl+G opens a fresh panel later if the player wants to make another adjustment.
		// GeneralsX @feature 14/08/2026 The scale choice is the one exception to the explicit Remember
		// action: its required display rebuild just closed that action along with the old panel.
		if (!saveScaleModePreference()) {
			fprintf(stderr, "WARNING: ExtrasMenu: applied clarity mode but could not save Options.ini\n");
		}
	}
	else
	{
		if (reopenOnFailure) {
			openPanel();
			s_footerNotice = FOOTER_NOTICE_SCALE_FAILED;
			s_footerNoticeUntil = timeGetTime() + 2500;
			refreshFooterLabel();
		}
	}
#endif
}

// LAYOUT CALLBACKS ///////////////////////////////////////////////////////////////////////////////
// These stay in place because FunctionLexicon maps them by name, so a .wnd layout could still drive
// this panel if one is ever authored for it.

//-------------------------------------------------------------------------------------------------
void ExtrasMenuInit( WindowLayout *layout, void *userData )
{
	refreshAllRowLabels();
	refreshAllMoreRowLabels();
	refreshFooterLabel();
}

//-------------------------------------------------------------------------------------------------
void ExtrasMenuUpdate( WindowLayout *layout, void *userData )
{
	// Deliberately empty. Slider changes are event-driven, and transient notices are expired from the
	// engine's outer update so the same behaviour also works in the shell.
}

//-------------------------------------------------------------------------------------------------
void ExtrasMenuShutdown( WindowLayout *layout, void *userData )
{
	// Nothing to release here: CloseExtrasMenu owns the teardown.
}


//-------------------------------------------------------------------------------------------------
/** Raw input handler for the panel.
	* Two jobs: close on ESC, and swallow every mouse event that lands on the panel so a click on a
	* slider does not also start a drag select box in the tactical view underneath. */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType ExtrasMenuInput( GameWindow *window, UnsignedInt msg,
																			WindowMsgData mData1, WindowMsgData mData2 )
{
	switch (msg)
	{
		case GWM_CHAR:
		{
			const UnsignedByte key = (UnsignedByte)mData1;
			const UnsignedByte state = (UnsignedByte)mData2;
			if (key == KEY_ESC && BitIsSet(state, KEY_STATE_UP))
			{
				CloseExtrasMenu();
				return MSG_HANDLED;
			}
			// Let everything else through: the panel is not a modal dialog and the player may still
			// want their normal hotkeys while it is open.
			return MSG_IGNORED;
		}

		case GWM_LEFT_DOWN:
		case GWM_LEFT_UP:
		case GWM_LEFT_DRAG:
		case GWM_MIDDLE_DOWN:
		case GWM_MIDDLE_UP:
		case GWM_MIDDLE_DRAG:
		case GWM_RIGHT_DOWN:
		case GWM_RIGHT_UP:
		case GWM_RIGHT_DRAG:
		case GWM_MOUSE_ENTERING:
		case GWM_MOUSE_LEAVING:
		case GWM_WHEEL_UP:
		case GWM_WHEEL_DOWN:
			// Consumed on purpose. This is the wall that stops clicks reaching the battlefield.
			return MSG_HANDLED;
	}

	return MSG_IGNORED;
}

//-------------------------------------------------------------------------------------------------
/** Gadget message handler.
	* Slider values are applied on GSM_SLIDER_TRACK rather than polled from the update callback,
	* because a self owned layout opened from the shell never gets its update callback driven: the
	* shell only updates layouts on its own stack, and the in game UI only updates layouts that were
	* registered with it. Tracking covers both cases with no polling at all. */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType ExtrasMenuSystem( GameWindow *window, UnsignedInt msg,
																			 WindowMsgData mData1, WindowMsgData mData2 )
{
	switch (msg)
	{
		case GWM_CREATE:
		case GWM_DESTROY:
			return MSG_HANDLED;

		case GSM_SLIDER_TRACK:
		{
			GameWindow *control = (GameWindow *)mData1;
			const Int sliderPos = (Int)mData2;

			for (Int row = 0; row < ROW_COUNT; ++row)
			{
				if (s_rowSlider[row] == control)
				{
					clearFooterNotice();
					applyRowValue(row, sliderPos);
					refreshRowLabel(row);
					return MSG_HANDLED;
				}
			}
			for (Int row = 0; row < MORE_ROW_COUNT; ++row)
			{
				if (s_moreRowSlider[row] == control)
				{
					clearFooterNotice();
					applyMoreRowValue(row, sliderPos);
					refreshMoreRowLabel(row);
					return MSG_HANDLED;
				}
			}
			return MSG_IGNORED;
		}

		case GBM_SELECTED:
		{
			GameWindow *control = (GameWindow *)mData1;

			for (Int mode = 0; mode < HEALTH_MODE_COUNT; ++mode)
			{
				if (control == s_healthModeButton[mode])
				{
					clearFooterNotice();
					applyMoreRowValue(MORE_ROW_HEALTH_BARS, mode);
					refreshMoreRowLabel(MORE_ROW_HEALTH_BARS);
					return MSG_HANDLED;
				}
			}

			if (control == s_buttonClose)
			{
				CloseExtrasMenu();
				return MSG_HANDLED;
			}

			if (control == s_buttonSave)
			{
				s_footerNotice = savePreferences() ? FOOTER_NOTICE_SAVED : FOOTER_NOTICE_SAVE_FAILED;
				s_footerNoticeUntil = timeGetTime() + 2500;
				refreshFooterLabel();
				return MSG_HANDLED;
			}

			if (control == s_buttonDefaults)
			{
				clearFooterNotice();
				if (s_activePage == EXTRAS_PAGE_MORE)
				{
					for (Int row = 0; row < MORE_ROW_COUNT; ++row)
					{
						applyMoreRowValue(row, s_moreRowDefaults[row]);
						if (s_moreRowSlider[row]) {
							GadgetSliderSetPosition(s_moreRowSlider[row], s_moreRowDefaults[row]);
						}
						refreshMoreRowLabel(row);
					}
				}
				else
				{
					for (Int row = 0; row < ROW_COUNT; ++row)
					{
						// Money is skipped on purpose: taking a player's cash away is not "restoring a
						// default", and a misclick on this button must not be able to lose a game.
						if (row == ROW_MONEY) {
							continue;
						}
						applyRowValue(row, s_rowDefaults[row]);
						if (s_rowSlider[row]) {
							GadgetSliderSetPosition(s_rowSlider[row], s_rowDefaults[row]);
						}
						refreshRowLabel(row);
					}
				}
				return MSG_HANDLED;
			}

			if (control == s_buttonMore)
			{
				clearFooterNotice();
				showPage(EXTRAS_PAGE_MORE);
				return MSG_HANDLED;
			}

			if (control == s_buttonBack)
			{
				clearFooterNotice();
				showPage(EXTRAS_PAGE_MAIN);
				return MSG_HANDLED;
			}

#ifdef GX_HAS_SCALE_MODE
			if (control == s_buttonMode)
			{
				const Int next = (getScaleMode() == SCALE_MODE_POINT) ? SCALE_MODE_NATIVE : SCALE_MODE_POINT;
				// The previous candidate applied the scale here and rebuilt later in this same
				// SDL3GameEngine::update call. That was not a later tick: the replacement panel
				// inherited stale hover/capture coordinates and both rendering and clicks shifted.
				s_pendingScaleMode = next;
				return MSG_HANDLED;
			}
#endif

			return MSG_IGNORED;
		}
	}

	return MSG_IGNORED;
}
