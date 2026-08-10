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

// FILE: W3DDisplay.cpp ///////////////////////////////////////////////////////
//
// W3D Implementation for the Game Display which is responsible for creating
// and maintaining the entire visual display
//
// Author: Colin Day, April 2001
//
///////////////////////////////////////////////////////////////////////////////

static void drawFramerateBar();

// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
#include <numeric>
#include <stdlib.h>
#include <windows.h>
// GeneralsX @bugfix BenderAI 13/02/2026 - io.h is Windows-specific, use unistd.h on Linux
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h> // access() for file existence checks
#include <SDL3/SDL.h> // For SDL_ShowWindow() on Linux
#endif
#include <time.h>
#include <vector>

// USER INCLUDES //////////////////////////////////////////////////////////////
#include "Common/FramePacer.h"
#include "Common/ThingFactory.h"
#include "Common/GlobalData.h"
#include "Common/PerfTimer.h"
#include "Common/FileSystem.h"
#include "Common/LocalFileSystem.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingTemplate.h"
#include "Common/GameLOD.h"
#include "Common/DrawModule.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/PhysicsUpdate.h"

#include "GameClient/Drawable.h"
#include "GameClient/GameText.h"
#include "GameClient/GraphDraw.h"
#include "GameClient/Line2D.h"
#include "GameClient/Mouse.h"
#include "GameClient/GlobalLanguage.h"
// GeneralsX @tweak 27/07/2026 For GeneralsX_NotifyResolutionChanged, which forwards to the header
// template manager the way the stock options screen does after a resolution change.
#include "GameClient/HeaderTemplate.h"
// GeneralsX @bugfix 27/07/2026 For GeneralsX_NotifyResolutionChanged, which rebuilds the shell
// layouts and the control bar the way the stock options screen does after a resolution change.
#include "GameClient/Shell.h"
#include "GameClient/InGameUI.h"
#include "GameClient/View.h"
#include "GameClient/Water.h"

#include "GameNetwork/NetworkInterface.h"
#include "Common/ModelState.h"
#include "Lib/BaseType.h"
#include "W3DDevice/Common/W3DConvert.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "W3DDevice/GameClient/W3DGameClient.h"
#include "W3DDevice/GameClient/W3DFileSystem.h"
#include "W3DDevice/GameClient/W3DDynamicLight.h"
#include "W3DDevice/GameClient/W3DProfilerFrameCapture.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "W3DDevice/GameClient/W3DTerrainTracks.h"
#include "W3DDevice/GameClient/W3DWater.h"
#include "W3DDevice/GameClient/W3DVideoBuffer.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DDebugDisplay.h"
#include "W3DDevice/GameClient/W3DProjectedShadow.h"
#include "W3DDevice/GameClient/W3DShroud.h"
#include "WWMath/wwmath.h"
#include "WWLib/registry.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/predlod.h"
#include "WW3D2/part_emt.h"
#include "WW3D2/part_ldr.h"
#include "WW3D2/dx8caps.h"
#include "WW3D2/ww3dformat.h"
#include "WW3D2/agg_def.h"
#include "WW3D2/render2dsentence.h"
#include "WW3D2/sortingrenderer.h"
#include "WW3D2/textureloader.h"
#include "WW3D2/dx8webbrowser.h"
#include "WW3D2/mesh.h"
#include "WW3D2/hlod.h"
#include "WW3D2/meshmatdesc.h"
#include "WW3D2/meshmdl.h"
#include "WW3D2/rddesc.h"
#include "TARGA.h"

#include "GameLogic/ScriptEngine.h"		// For TheScriptEngine - jkmcd
#include "GameLogic/GameLogic.h"
#ifdef DUMP_PERF_STATS
#include "GameLogic/PartitionManager.h"
#endif

#include "WinMain.h"


// DEFINE AND ENUMS ///////////////////////////////////////////////////////////

#define no_SAMPLE_DYNAMIC_LIGHT	1
#ifdef SAMPLE_DYNAMIC_LIGHT
static W3DDynamicLight * theDynamicLight = nullptr;
static Real theLightXOffset = 0.1f;
static Real theLightYOffset = 0.07f;
static Int theFlashCount = 0;
#endif

//*****************************************************************************************
//*****************************************************************************************
//**** Start Statistical Dump *************************************************************
//*****************************************************************************************

#ifdef DUMP_PERF_STATS

#include <cstdarg>

class StatDumpClass
{
public:
	StatDumpClass( const char *fname );
	~StatDumpClass();
	void dumpStats( Bool brief = FALSE, Bool flagSpikes = FALSE );

protected:
	FILE *m_fp;
};

//=============================================================================
//Open the file once at the beginning of the game -- everything appends to it.
//=============================================================================
StatDumpClass::StatDumpClass( const char *fname )
{
	char buffer[ _MAX_PATH ];
	GetModuleFileName( nullptr, buffer, sizeof( buffer ) );
	if (char *pEnd = strrchr(buffer, '\\'))
	{
		*pEnd = 0;
	}
	// TheSuperHackers @fix Caball009 03/06/2025 Don't use AsciiString here anymore because its memory allocator may not have been initialized yet.
	const std::string fullPath = std::string(buffer) + "\\" + fname;
	m_fp = fopen(fullPath.c_str(), "wt");
}

//=============================================================================
//Close the file at the end of the application
//=============================================================================
StatDumpClass::~StatDumpClass()
{
	if( m_fp )
	{
		fclose( m_fp );
	}
}

static const char *getCurrentTimeString()
{
	time_t aclock;
	time(&aclock);
	struct tm *newtime = localtime(&aclock);
	return asctime(newtime);
}

//=============================================================================
//Dump the stats
//=============================================================================


static Bool s_notFirstDump = FALSE;

void StatDumpClass::dumpStats( Bool brief, Bool flagSpikes )
{
	if( !m_fp )
	{
		return;
	}


  Bool beBrief = brief & s_notFirstDump;
  s_notFirstDump = TRUE;

	fprintf( m_fp, "----------------------------------------------------------------\n" );
	fprintf( m_fp, "Performance Statistical Dump -- Frame %d\n", TheGameLogic->getFrame() );
  if ( ! beBrief )
  {
	  //static char buf[1024];
	  fprintf( m_fp, "Time:\t%s", getCurrentTimeString() );
	  fprintf( m_fp, "Map:\t%s\n", TheGlobalData->m_mapName.str());
	  fprintf( m_fp, "Side:\t%s\n", ThePlayerList->getLocalPlayer()->getSide().str());
	  fprintf( m_fp, "----------------------------------------------------------------\n" );
  }

	//FPS
	Real fps = TheDisplay->getAverageFPS();
	fprintf( m_fp, "Average FPS: %.1f (%.5f msec)\n", fps, 1000.0f / fps );
  if ( flagSpikes && fps<20.0f )
  	fprintf( m_fp, "                                                                      FPS OUT OF TOLERANCE\n" );


	//Rendering stats
	fprintf( m_fp, "Draws: %d \nSkins: %d \nSortedPolys: %d \nSkinPolys: %d\n",(Int)Debug_Statistics::Get_Draw_Calls(),
		(Int)Debug_Statistics::Get_DX8_Skin_Renders(),
		(Int)Debug_Statistics::Get_Sorting_Polygons(), (Int)Debug_Statistics::Get_DX8_Skin_Polygons());

	Int onScreenParticleCount = TheParticleSystemManager->getOnScreenParticleCount();

  if ( flagSpikes )
  {
    if ( Debug_Statistics::Get_Draw_Calls()>2000 )
  	  fprintf( m_fp, "                                                                      DRAWS OUT OF TOLERANCE(2000)\n" );
    if ( Debug_Statistics::Get_Sorting_Polygons() > (onScreenParticleCount*2) + 300 )
  	  fprintf( m_fp, "                                                                      NON-PARTICLE-SORTS OUT OF TOLERANCE(300)\n" );
    if ( Debug_Statistics::Get_DX8_Skin_Renders()>100 )
  	  fprintf( m_fp, "                                                                      SKINS OUT OF TOLERANCE(100)\n" );
  }


	//Object stats
	UnsignedInt objCount = TheGameLogic->getObjectCount();
	UnsignedInt objScreenCount = TheGameClient->getRenderedObjectCount();
	fprintf( m_fp, "Objects: %d in world (%d onscreen)\n", objCount, objScreenCount );
  if ( flagSpikes && objCount > 800 )
  	fprintf( m_fp, "                                                                      OBJS OUT OF TOLERANCE(800)\n" );

	//AI stats
	UnsignedInt numAI, numMoving, numAttacking, numWaitingForPath, overallFailedPathfinds;
	TheGameLogic->getAIMetricsStatistics( &numAI, &numMoving, &numAttacking, &numWaitingForPath, &overallFailedPathfinds );
	fprintf( m_fp, "\n" );
	fprintf( m_fp, "AI Statistics:\n" );
	fprintf( m_fp, "  Total AI Objects: %d\n", numAI );
	fprintf( m_fp, "    -moving: %d\n", numMoving );
	fprintf( m_fp, "    -attacking: %d\n", numAttacking );
	fprintf( m_fp, "    -waiting for path: %d\n", numWaitingForPath );
	fprintf( m_fp, "  Total failed pathfinds: %d\n", overallFailedPathfinds );
  if ( flagSpikes && overallFailedPathfinds > 0 )
  	fprintf( m_fp, "                                                                      FAILEDPATHFINDS OUT OF TOLERANCE(0)\n" );
	fprintf( m_fp, "\n" );

	// Script stats
	Real timeLastFrame, slowScript1, slowScript2;
	AsciiString slowScripts = TheScriptEngine->getStats(&timeLastFrame, &slowScript1, &slowScript2);
	fprintf( m_fp, "\n" );
	fprintf( m_fp, "Script Engine Statistics:\n" );
	fprintf( m_fp, "  Total time last frame: %.5f msec\n", timeLastFrame*1000 );
	fprintf( m_fp, "    -Slowest 2 scripts      %s\n", slowScripts.str() );
	fprintf( m_fp, "    -Slowest 2 script times %.5f msec, %.5f msec \n", slowScript1*1000, slowScript2*1000 );
  if ( flagSpikes && slowScript1*1000 > 0.2f || slowScript2*1000 > 0.2f )
  	fprintf( m_fp, "                                                                      SLOW SCRIPT OUT OF TOLERANCE(0.2)\n" );
	fprintf( m_fp, "\n" );



	//PartitionMgr stats
	double gcoTimeThisFrameTotal, gcoTimeThisFrameAvg;
	ThePartitionManager->getPMStats(gcoTimeThisFrameTotal, gcoTimeThisFrameAvg);
	fprintf(m_fp, "Partition Manager Statistics:\n");
	fprintf(m_fp, "  Total time for object scans this frame is %.5f msec\n", gcoTimeThisFrameTotal);
	fprintf(m_fp, "  Avg time per object scan this frame is %.5f msec\n", gcoTimeThisFrameAvg);
	fprintf( m_fp, "\n" );

	// setup texture stats
	Debug_Statistics::Record_Texture_Mode(Debug_Statistics::RECORD_TEXTURE_SIMPLE/*RECORD_TEXTURE_NONE*/);

	fprintf( m_fp, "Video Statistics:\n" );
	//Particle system stats
	fprintf( m_fp, "  Particle Systems: %d\n", TheParticleSystemManager->getParticleSystemCount() );
	Int totalParticles = TheParticleSystemManager->getParticleCount();
	fprintf( m_fp, "  Particles: %d in world (%d onscreen)\n", totalParticles, onScreenParticleCount );

  if ( flagSpikes && totalParticles > TheGlobalData->m_maxParticleCount - 10 )
  	fprintf( m_fp, "                                                                      PARTICLES OUT OF TOLERANCE(CAP-10)\n" );
  if ( flagSpikes && onScreenParticleCount > TheGlobalData->m_maxParticleCount - 10 )
  	fprintf( m_fp, "                                                                      ON_SCREEN_PARTICLES OUT OF TOLERANCE(CAP-10)\n" );


	// polygons this frame
	Int polyPerFrame = Debug_Statistics::Get_DX8_Polygons();
	Int polyPerSecond = (Int)(polyPerFrame * fps);
	fprintf( m_fp, "  Polygons: %d per frame (%d per second)\n", polyPerFrame, polyPerSecond );

	// vertices this frame
	fprintf( m_fp, "  Vertices: %d\n", Debug_Statistics::Get_DX8_Vertices() );

	//
	// I'm adjusting the texture memory usage counter by subtracting
	// out the terrain alpha texture (since it's really == terrain texture).
	//
	fprintf( m_fp, "  Video RAM: %d\n", Debug_Statistics::Get_Record_Texture_Size() - 1376256 );

	// terrain stats
	fprintf( m_fp, "  3-Way Blends: %d/%d, \n Shoreline Blends: %d/%d\n", TheTerrainRenderObject->getNumExtraBlendTiles(TRUE),TheTerrainRenderObject->getNumExtraBlendTiles(FALSE), TheTerrainRenderObject->getNumShoreLineTiles(TRUE),TheTerrainRenderObject->getNumShoreLineTiles(FALSE));
  if ( flagSpikes && TheTerrainRenderObject->getNumExtraBlendTiles(TRUE) > 2000 )
  	fprintf( m_fp, "                                                                      3-WAYS OUT OF TOLERANCE(2000)\n" );
  if ( flagSpikes && TheTerrainRenderObject->getNumShoreLineTiles(TRUE) > 2000 )
  	fprintf( m_fp, "                                                                      SHORELINES OUT OF TOLERANCE(2000)\n" );

	fprintf( m_fp, "\n" );

#if defined(RTS_DEBUG)
  if ( ! beBrief )
  {
    TheAudio->audioDebugDisplay( nullptr, nullptr, m_fp );
	  fprintf( m_fp, "\n" );
  }
#endif

#ifdef MEMORYPOOL_DEBUG
	//Report memory usage.
	TheMemoryPoolFactory->debugMemoryReport( REPORT_FACTORYINFO | REPORT_POOLINFO, 0, 0, m_fp );
#else
	fprintf( m_fp, "Memory Report -- unavailable \n(build doesn't have MEMORYPOOL_DEBUG defined)\n" );
#endif
	fprintf( m_fp, "\n" );

	fprintf( m_fp, "%s", TheSubsystemList->dumpTimesForAll().str());

  if ( ! beBrief )
  {
	  fprintf( m_fp, "----------------------------------------------------------------\n" );
	  fprintf( m_fp, "END -- Frame %d\n", TheGameLogic->getFrame() );
	  fprintf( m_fp, "----------------------------------------------------------------\n" );
  }
	fprintf( m_fp, "\n\n" );
	fflush(m_fp);
}

StatDumpClass TheStatDump("StatisticsDump.txt");

#endif //DUMP_PERF_STATS

//*****************************************************************************************
//**** End Statistical Dump ***************************************************************
//*****************************************************************************************
//*****************************************************************************************



///////////////////////////////////////////////////////////////////////////////
// DEFINITIONS ////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//=============================================================================
RTS3DScene *W3DDisplay::m_3DScene = nullptr;
RTS2DScene *W3DDisplay::m_2DScene = nullptr;
RTS3DInterfaceScene *W3DDisplay::m_3DInterfaceScene = nullptr;
W3DAssetManager *W3DDisplay::m_assetManager = nullptr;

//=============================================================================
	// note, can't use the ones from PerfTimer.h 'cuz they are currently
	// only valid when "-vtune" is used... (srj)
// GeneralsX @build BenderAI 13/02/2026 Add platform-specific performance counter wrappers
inline Int64 getPerformanceCounter()
{
	Int64 tmp;
#ifdef _WIN32
	QueryPerformanceCounter((LARGE_INTEGER*)&tmp);
#else
	// Linux: Use time_compat.h wrapper which calls clock_gettime(CLOCK_MONOTONIC)
	QueryPerformanceCounter(&tmp);
#endif
	return tmp;
}

inline Int64 getPerformanceCounterFrequency()
{
	Int64 tmp;
#ifdef _WIN32
	QueryPerformanceFrequency((LARGE_INTEGER*)&tmp);
#else
	// Linux: Use time_compat.h wrapper which returns 10 million (100-nanosecond intervals)
	QueryPerformanceFrequency(&tmp);
#endif
	return tmp;
}

// W3DDisplay::W3DDisplay =====================================================
/** */
//=============================================================================
W3DDisplay::W3DDisplay()
{
	Int i;

	m_initialized = false;
	m_assetManager = nullptr;
	m_3DScene = nullptr;
	m_2DScene = nullptr;
	m_3DInterfaceScene = nullptr;
	m_averageFPS = TheGlobalData->m_framesPerSecondLimit;
#if defined(RTS_DEBUG)
	m_timerAtCumuFPSStart = 0;
#endif
	for (i=0; i<LightEnvironmentClass::MAX_LIGHTS; i++)
		m_myLight[i] = nullptr;
	m_2DRender = nullptr;
	m_isClippedEnabled = FALSE;
	m_clipRegion.lo.x = 0;
	m_clipRegion.lo.y = 0;
	m_clipRegion.hi.x = 0;
	m_clipRegion.hi.y = 0;

	for (i = 0; i < DisplayStringCount; i++)
		m_displayStrings[i] = nullptr;

	m_batchTexture = nullptr;
	m_batchMode = DRAW_IMAGE_ALPHA;
	m_batchGrayscale = FALSE;
	m_batchNeedsInit = FALSE;

#ifdef PROFILER_ENABLED
	m_profilerFrameCapture = NEW W3DProfilerFrameCapture();
#endif
}

// W3DDisplay::~W3DDisplay ====================================================
/** */
//=============================================================================
W3DDisplay::~W3DDisplay()
{
#ifdef PROFILER_ENABLED
	delete m_profilerFrameCapture;
	m_profilerFrameCapture = nullptr;
#endif

	// get rid of the debug display
	delete m_debugDisplay;
	m_debugDisplay = nullptr;
	m_nativeDebugDisplay = nullptr;

	// delete the display strings
	for (int i = 0; i < DisplayStringCount; i++)
		TheDisplayStringManager->freeDisplayString(m_displayStrings[i]);

	// TheSuperHackers @fix Mauller/Tomsons26 28/04/2025 Free benchmark display string
	if( m_benchmarkDisplayString ) {
		TheDisplayStringManager->freeDisplayString(m_benchmarkDisplayString);
	}

	// delete 2D renderer
	if( m_2DRender )
	{

		m_2DRender->Reset();
		delete m_2DRender;
		m_2DRender = nullptr;

	}

	//
	// delete all our views now since they are W3D views and we need to
	// free them BEFORE we shutdown W3D
	//
	Display::deleteViews();

	REF_PTR_RELEASE( m_3DScene );
	REF_PTR_RELEASE( m_2DScene );
	REF_PTR_RELEASE( m_3DInterfaceScene );
	for (Int j=0; j<LightEnvironmentClass::MAX_LIGHTS; j++)
		REF_PTR_RELEASE( m_myLight[j] );

	PredictiveLODOptimizerClass::Free();

	// shutdown
	Debug_Statistics::Shutdown_Statistics();
	if (!TheGlobalData->m_headless)
		W3DShaderManager::shutdown();
	m_assetManager->Free_Assets();
	delete m_assetManager;
	if (!TheGlobalData->m_headless)
		WW3D::Shutdown();
	WWMath::Shutdown();
	if (!TheGlobalData->m_headless)
		DX8WebBrowser::Shutdown();
	delete TheW3DFileSystem;
	TheW3DFileSystem = nullptr;

}

// TheSuperHackers @tweak valeronm 20/03/2025 No longer filters resolutions by a 4:3 aspect ratio.
inline Bool isResolutionSupported(const ResolutionDescClass &res)
{
	static const Int minBitDepth = 24;

	return res.Width >= DEFAULT_DISPLAY_WIDTH && res.BitDepth >= minBitDepth;
}

// SDL3 display size providers for DX8Wrapper pillarbox (registered at init)
#ifdef SAGE_USE_SDL3

#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
// GeneralsX @feature 26/07/2026 Single source of truth for macOS HiDPI unit conversion.
//
// The port previously had two providers disagreeing about units: SDL3_GetNativeDisplaySize
// returned logical points while SDL3_GetWindowSizeInPixels returned physical pixels, and each
// caller multiplied (or forgot to multiply) by density on its own. That split is what produced
// both the "oversized resolution list / oversized fonts" regression and the halved mouse
// coordinates. Everything macOS-specific about points-vs-pixels now derives from this one call.
//
// Convention established here and relied on by the rest of the HiDPI path:
//   - The engine's internal render resolution (TheDisplay, .wnd layout scaling, font scaling,
//     Render2D coordinate range, mouse output) is in PHYSICAL PIXELS.
//   - SDL window geometry APIs (SDL_SetWindowSize, mouse event coordinates) are in POINTS.
//   - Conversion between the two goes through outDensity, never a hardcoded 2.0f.
bool GeneralsX_GetMacDisplayMetrics(int& outPointsW, int& outPointsH,
                                   int& outPixelsW, int& outPixelsH, float& outDensity)
{
	extern SDL_Window* TheSDL3Window;
	if (!TheSDL3Window) return false;

	const SDL_DisplayID displayId = SDL_GetDisplayForWindow(TheSDL3Window);
	const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayId);
	if (!mode || mode->w <= 0 || mode->h <= 0) return false;

	outDensity = mode->pixel_density > 0.0f ? mode->pixel_density : 1.0f;

	// Cocoa can transiently misreport window size mid-fullscreen-transition, so in fullscreen
	// trust the display mode; windowed reads the live window instead.
	if ((SDL_GetWindowFlags(TheSDL3Window) & SDL_WINDOW_FULLSCREEN) != 0)
	{
		outPointsW = mode->w;
		outPointsH = mode->h;
	}
	else
	{
		int winW = 0, winH = 0;
		SDL_GetWindowSize(TheSDL3Window, &winW, &winH);
		if (winW <= 0 || winH <= 0) { winW = mode->w; winH = mode->h; }
		outPointsW = winW;
		outPointsH = winH;

		// Prefer the window's real backing ratio when SDL can report it; a window can sit on a
		// different display than the one the mode came from.
		int physW = 0, physH = 0;
		SDL_GetWindowSizeInPixels(TheSDL3Window, &physW, &physH);
		if (physW > 0 && winW > 0)
			outDensity = (float)physW / (float)winW;
	}

	outPixelsW = (int)(outPointsW * outDensity);
	outPixelsH = (int)(outPointsH * outDensity);

	{
		static int lastLoggedW = -1, lastLoggedH = -1;
		if (outPixelsW != lastLoggedW || outPixelsH != lastLoggedH) {
			lastLoggedW = outPixelsW;
			lastLoggedH = outPixelsH;
			int wpW = 0, wpH = 0, wxW = 0, wxH = 0;
			SDL_GetWindowSize(TheSDL3Window, &wpW, &wpH);
			SDL_GetWindowSizeInPixels(TheSDL3Window, &wxW, &wxH);
			fprintf(stderr, "INFO: GX-HiDPI metrics: mode=%dx%d modeDensity=%.2f "
				"winPoints=%dx%d winPixels=%dx%d -> points=%dx%d pixels=%dx%d density=%.2f fs=%d\n",
				mode->w, mode->h, mode->pixel_density, wpW, wpH, wxW, wxH,
				outPointsW, outPointsH, outPixelsW, outPixelsH, outDensity,
				(SDL_GetWindowFlags(TheSDL3Window) & SDL_WINDOW_FULLSCREEN) != 0 ? 1 : 0);
		}
	}
	return true;
}

// GeneralsX @feature 27/07/2026 The active render scale, as a percentage of the display's physical
// pixel size, shared between the launch path and the in-game clarity switch.
//
// The percentage never takes part in sizing the window -- that is the whole point of it. The window is
// always pickedResolution/density points, at any percentage; what the percentage decides is how many
// pixels get drawn into that window, which is windowPixels * percent / 100. An earlier version folded
// the percentage into the density divisor instead, which at 50% asked for a window twice as wide in
// pixels as the render target it held, so the game drew at double size with the window framing only
// its top-left corner.
static int s_gxRenderScalePercent = 100;

void GeneralsX_SetRenderScalePercent(int percent)
{
	if (percent < 25) percent = 25;
	if (percent > 100) percent = 100;
	s_gxRenderScalePercent = percent;
}

int GeneralsX_GetRenderScalePercent(void)
{
	return s_gxRenderScalePercent;
}

// GeneralsX @bugfix 27/07/2026 The window's size in POINTS, as explicit state.
//
// This is the fix for a design error that made point-for-point mode unusable in a window. The window
// size used to be re-derived from the render resolution while the render resolution was re-derived
// from the window size, so the two chased each other. At 100% the arithmetic happened to round-trip
// and it looked fine; at 50% it did not. Dividing the render pixels by the *effective* density asked
// Cocoa for a window of renderWidth POINTS, which on a 2x display is a window twice as wide in pixels
// as the render target it holds -- so the game drew at double size and the window showed only its
// top-left corner. Requests past the screen edge were then clamped by macOS, the resize handler fed
// the clamped size back in, and the pair ran away to 5140x2004.
//
// So the window's point size is now stored, not computed. Exactly three things set it: a resolution
// pick, the user dragging the window, and launch. The clarity mode is deliberately not one of them --
// it changes how many pixels are rendered into the window, never the window itself, which is what
// makes the switch feel like a sharpness control instead of a window resizer.
//
// The invariant everything else derives from:
//     windowPoints = pickedResolution / density        (clamped to what fits on screen)
//     renderPixels = windowPoints * density * percent / 100
// At 100% renderPixels equals the window's pixel extent, the pillarbox is a no-op, and output is
// genuine 1:1 HiDPI. At 50% the render target is half-size in each axis and the swapchain upscales it
// by exactly 2 -- soft, but correctly sized and filling the window, which is what a Windows game gets
// when it opts out of DPI virtualization.
static int s_gxWindowPointW = 0;
static int s_gxWindowPointH = 0;

// Re-entrancy guard. setDisplayMode calls the apply function below, which resizes the window, which
// makes Cocoa post a resize event, which SDL3GameEngine::handleWindowEvent answers by calling
// setDisplayMode again. Without this the first resolution pick recurses.
static bool s_gxApplyingWindowMode = false;

// GeneralsX @bugfix 27/07/2026 Do not re-apply a window mode macOS has already applied.
//
// When the user toggles fullscreen with Ctrl+Cmd+F or the zoom button, the window reaches its final
// state before the engine hears about it, so the only thing left to do is match the render resolution.
// But that goes through setDisplayMode, which calls SDL3_ApplyWindowModeForRenderConfig, which sets
// the SDL fullscreen state again -- and SDL3_EnsureNativeFullscreen pumps events while doing it, so
// the transition re-entered itself. Measured: one keypress produced four alternating transitions, and
// leaving fullscreen fought Cocoa's frame restore down from 1802x1002 to 2048x1034 points.
//
// While this is set the apply function does nothing at all. The window is right; only the render
// target is not.
static bool s_gxFollowingNativeFullscreen = false;

Bool GeneralsX_IsApplyingWindowMode(void)
{
	return s_gxApplyingWindowMode ? TRUE : FALSE;
}

void GeneralsX_SetWindowPointSize(int pointW, int pointH)
{
	if (pointW > 0 && pointH > 0) {
		s_gxWindowPointW = pointW;
		s_gxWindowPointH = pointH;
	}
}

Bool GeneralsX_GetWindowPointSize(int& pointW, int& pointH)
{
	if (s_gxWindowPointW <= 0 || s_gxWindowPointH <= 0) return FALSE;
	pointW = s_gxWindowPointW;
	pointH = s_gxWindowPointH;
	return TRUE;
}

// The render resolution the current window should be drawing at, in physical pixels.
Bool GeneralsX_GetWindowedRenderSize(int& outW, int& outH)
{
	extern SDL_Window* TheSDL3Window;
	if (!TheSDL3Window) return FALSE;

	int pxW = 0, pxH = 0;
	if (!SDL_GetWindowSizeInPixels(TheSDL3Window, &pxW, &pxH) || pxW <= 0 || pxH <= 0) return FALSE;

	outW = (pxW * s_gxRenderScalePercent) / 100;
	outH = (pxH * s_gxRenderScalePercent) / 100;
	outW &= ~1;		// even, as every other path that sets a resolution requires
	outH &= ~1;
	return (outW > 0 && outH > 0) ? TRUE : FALSE;
}

// Defined in Main/MacDisplayKick.cpp, which cannot be included here: <CoreGraphics/CoreGraphics.h>
// pulls in <MacTypes.h>, whose "typedef UInt8 Byte" is a hard conflict with the engine's own
// "typedef char Byte" in BaseTypeCore.h. Declaring the one symbol keeps the two apart.
extern "C" bool GXGetPanelNativePixelSize(int* outWidth, int* outHeight);

// GeneralsX @bugfix 10/08/2026 In fullscreen, point-for-point targets the PANEL, not the point grid.
//
// The two clarity modes exist for two different reasons, and only one of them is about sharpness:
//
//   HIDPI  follows the system's scaled framebuffer. The point of it is UI SIZE -- the game's text and
//          panels end up the size the rest of the desktop taught the user to expect, and rendering at
//          the full framebuffer is what keeps that from costing any sharpness.
//
//   POINT  is one rendered pixel per PHYSICAL PANEL PIXEL, which is the sharpest image the display can
//          physically show. Its known cost is that the UI gets small, because the engine scales fonts
//          at 0.7x the resolution ratio (GlobalLanguage's ResolutionFontAdjustment) while the layout
//          boxes scale proportionally. That tradeoff is the mode's purpose, not a defect in it.
//
// The clarity percentage cannot express POINT on a scaled display. On the development monitor macOS
// reports 2048x1152 points over a 4096x2304 framebuffer for a panel that is physically 2560x1440, so
// 50% of the framebuffer lands on 2048x1152 -- point-for-point by the letter of the old definition,
// and a fifth of the panel's detail discarded in each axis before WindowServer even resamples the
// frame. The panel size is not reachable as an integer percentage either: it is 62.5% of 4096x2304,
// and both 62% and 63% miss 2560 outright. So POINT asks CoreGraphics for the panel directly.
//
// Deliberately NOT conditional on the display supersampling. Where the framebuffer already IS the
// panel -- a stock Retina Mac at its default scaling -- the two modes resolve to the same number, and
// that is the honest answer rather than a collapsed setting: on such a display, rendering at the
// system framebuffer already is 1:1 with the panel, so there is nothing for POINT to do differently.
// The percentage is used only as the fallback for displays where CoreGraphics flags no native mode at
// all (some external and virtual displays), which is also the path every non-Apple platform takes.
Bool GeneralsX_GetFullscreenRenderSize(int& outW, int& outH)
{
	int ptW = 0, ptH = 0, pxW = 0, pxH = 0;
	float density = 1.0f;
	if (!GeneralsX_GetMacDisplayMetrics(ptW, ptH, pxW, pxH, density) || pxW <= 0 || pxH <= 0) {
		return FALSE;
	}

	int targetW = (pxW * s_gxRenderScalePercent) / 100;
	int targetH = (pxH * s_gxRenderScalePercent) / 100;

	if (s_gxRenderScalePercent < 100) {
		int panelW = 0, panelH = 0;
		if (GXGetPanelNativePixelSize(&panelW, &panelH) && panelW > 0 && panelH > 0) {
			static bool logged = false;
			if (!logged) {
				logged = true;
				fprintf(stderr, "INFO: GX-HiDPI point mode targets panel native %dx%d "
					"(points %dx%d, framebuffer %dx%d, %d%% would have given %dx%d)\n",
					panelW, panelH, ptW, ptH, pxW, pxH, s_gxRenderScalePercent, targetW, targetH);
			}
			targetW = panelW;
			targetH = panelH;
		}
	}

	targetW &= ~1;
	targetH &= ~1;
	if (targetW <= 0 || targetH <= 0) {
		return FALSE;
	}
	outW = targetW;
	outH = targetH;
	return TRUE;
}
#endif

static bool SDL3_GetNativeDisplaySize(int& outW, int& outH, float& outDensity)
{
	extern SDL_Window* TheSDL3Window;
	if (!TheSDL3Window) return false;
	SDL_DisplayID displayId = SDL_GetDisplayForWindow(TheSDL3Window);
	const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayId);
	if (!mode || mode->w <= 0 || mode->h <= 0) return false;
#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
	// GeneralsX @bugfix 26/07/2026 Report PHYSICAL PIXELS here, same as every other platform.
	//
	// This used to return logical points to keep buildFilteredResolutions() and GlobalLanguage's
	// font scaling from inflating. That was treating a symptom: because the engine rendered at
	// point dimensions while DXVK's swapchain was sized in pixels, DX8Wrapper::Pillarbox_Setup
	// upscaled the whole frame (3D, UI and text alike) by the backing scale factor -- the exact
	// softness this port was trying to eliminate. The resolution list and font scaling are now
	// pixel-based too, so a consistent pixel convention is what makes them correct.
	{
		int ptW = 0, ptH = 0, pxW = 0, pxH = 0;
		float density = 1.0f;
		if (GeneralsX_GetMacDisplayMetrics(ptW, ptH, pxW, pxH, density))
		{
			// Screen extent, not window extent: the resolution list must not shrink when the
			// game happens to be running in a small window.
			outDensity = density;
			outW = (int)(mode->w * density);
			outH = (int)(mode->h * density);
			return true;
		}
	}
	outDensity = mode->pixel_density > 0 ? mode->pixel_density : 1.0f;
	outW = (int)(mode->w * outDensity);
	outH = (int)(mode->h * outDensity);
#else
	outDensity = mode->pixel_density > 0 ? mode->pixel_density : 1.0f;
	outW = (int)(mode->w * outDensity);
	outH = (int)(mode->h * outDensity);
#endif
	return true;
}

static bool SDL3_GetWindowSizeInPixels(int& outW, int& outH, float& outDensity)
{
	extern SDL_Window* TheSDL3Window;
	if (!TheSDL3Window) return false;
#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
	// GeneralsX @refactor 26/07/2026 Contract: physical/backbuffer pixels, with outDensity set to
	// the backing scale so DX8Wrapper::Pillarbox_Get_Rect can convert its fit rect back to the
	// point space SDL3 reports mouse coordinates in.
	{
		int ptW = 0, ptH = 0, pxW = 0, pxH = 0;
		float density = 1.0f;
		if (GeneralsX_GetMacDisplayMetrics(ptW, ptH, pxW, pxH, density))
		{
			outW = pxW;
			outH = pxH;
			outDensity = density;
			return true;
		}
	}
#endif
	int logW = 0, logH = 0, physW = 0, physH = 0;
	SDL_GetWindowSize(TheSDL3Window, &logW, &logH);
	SDL_GetWindowSizeInPixels(TheSDL3Window, &physW, &physH);
	if (physW <= 0 || physH <= 0) return false;
	outW = physW;
	outH = physH;
	outDensity = (logW > 0) ? (float)physW / (float)logW : 1.0f;
	return true;
}

// GeneralsX @bugfix GitHub Copilot 28/04/2026 Ensure SDL3 fullscreen transition actually lands in native fullscreen and foreground.
static void SDL3_EnsureNativeFullscreen(SDL_Window* window)
{
	if (!window) return;

	for (int attempt = 0; attempt < 3; ++attempt) {
		SDL_PumpEvents();
		if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0) {
			break;
		}
		if (!SDL_SetWindowFullscreen(window, true)) {
			fprintf(stderr, "WARNING: SDL_SetWindowFullscreen(retry) failed: %s\n", SDL_GetError());
			break;
		}
	}

	SDL_RaiseWindow(window);
}

#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
// GeneralsX @bugfix 27/07/2026 Measure the title bar; SDL will not report it.
//
// SDL_GetWindowBordersSize is not implemented in the Cocoa backend: it reports zero on all four sides even
// for a window that is on screen and plainly has a bar. Everything here that has to fit a frame inside the
// screen needs that height, because SDL sizes and positions the CONTENT area while the frame has to hold
// the bar as well -- so a request that exactly fills the usable height overflows by the bar, and Cocoa
// answers the overflow by shrinking the height or sliding the window upward. Measured on the reporting
// machine after leaving fullscreen: content 1579x1066 with the frame at y = -62, so the bar was above the
// top of the screen and there was nothing left to grab.
//
// Seeded high enough for current macOS and only ever raised from what Cocoa actually does. Reserving a few
// points too many costs a few points of window; reserving too few costs the title bar.
static int s_gxTitleBarPoints = 32;

// Cocoa clamps by fitting the whole frame inside the visible screen, so a height it refused says what
// the bar took: the content it granted plus the bar is the entire usable height.
static void GeneralsX_LearnTitleBarFromClamp(int usableHeight, int grantedH)
{
	const int implied = usableHeight - grantedH;
	if (implied > s_gxTitleBarPoints && implied <= 64) {
		s_gxTitleBarPoints = implied;
	}
}

// GeneralsX @bugfix 27/07/2026 Put the title bar back on screen if something moved it off.
//
// A window whose bar is above the top of the screen cannot be moved, resized or closed by hand, and the
// only way out is a hotkey the player may not know. macOS will not let a drag produce this, so a
// position this high is always something the app or Cocoa did -- a restored frame, or a programmatic
// move -- and correcting it cannot be fighting the user.
//
// SDL positions the content area, so the bar sits in the points above it and the content has to start at
// least that far into the usable area.
void GeneralsX_EnsureWindowTitleBarOnScreen(void)
{
	extern SDL_Window* TheSDL3Window;
	if (!TheSDL3Window) return;
	if ((SDL_GetWindowFlags(TheSDL3Window) & SDL_WINDOW_FULLSCREEN) != 0) return;

	SDL_Rect usable;
	if (!SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(TheSDL3Window), &usable)) return;
	if (usable.h <= 0) return;

	int posX = 0, posY = 0;
	if (!SDL_GetWindowPosition(TheSDL3Window, &posX, &posY)) return;

	const int minContentY = usable.y + s_gxTitleBarPoints;
	if (posY >= minContentY) return;

	SDL_SetWindowPosition(TheSDL3Window, posX, minContentY);
	fprintf(stderr, "INFO: window title bar was off-screen at y=%d, moved to y=%d\n", posY, minContentY);
}
#endif

// GeneralsX @bugfix GitHub Copilot 27/04/2026 Apply SDL3 window sizing/fullscreen only after the final render resolution is known.
static void SDL3_ApplyWindowModeForRenderConfig(Bool windowed, Int renderWidth, Int renderHeight)
{
	extern SDL_Window* TheSDL3Window;
	if (!TheSDL3Window) return;

#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
	// macOS already put the window where it belongs; touching it again re-enters the transition.
	if (s_gxFollowingNativeFullscreen) return;

	// Hold the guard for the whole call. Resizing the window makes Cocoa post an event that
	// SDL3GameEngine::handleWindowEvent answers with another setDisplayMode, which lands back here;
	// without this the first resolution pick recurses.
	struct ApplyGuard {
		ApplyGuard()  { s_gxApplyingWindowMode = true;  }
		~ApplyGuard() { s_gxApplyingWindowMode = false; }
	} applyGuard;
#endif

	if (!windowed) {
		const bool alreadyFullscreen =
			(SDL_GetWindowFlags(TheSDL3Window) & SDL_WINDOW_FULLSCREEN) != 0;
#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
		// GeneralsX @bugfix 27/07/2026 Drop the window ceiling before going fullscreen.
		//
		// A maximum size reaches Cocoa as -setContentMaxSize:, which caps fullscreen content as well as the
		// window's, so a ceiling below the display size letterboxes the fullscreen picture. The windowed
		// ceiling is the display bounds precisely so that it cannot, but a window that has moved to a smaller
		// display carries the smaller display's ceiling with it, and this path can be reached before any
		// resize has updated it. Clearing costs nothing and closes that case.
		//
		// It does NOT close the native-toggle case, which is where the bars actually came from: Ctrl+Cmd+F and
		// the green button never reach this function, and by the time ENTER_FULLSCREEN is posted Cocoa has
		// already sized the frame. That one is fixed by the ceiling's value, not by clearing it here.
		SDL_SetWindowMaximumSize(TheSDL3Window, 0, 0);
#endif
#if !(defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE))
		if (alreadyFullscreen && !SDL_SetWindowFullscreen(TheSDL3Window, false)) {
			fprintf(stderr, "WARNING: SDL_SetWindowFullscreen(false) failed: %s\n", SDL_GetError());
		}
#endif

#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
		// GeneralsX @bugfix 27/07/2026 Ask for a borderless fullscreen, not an exclusive one.
		//
		// Naming a display mode is what makes SDL mark the window fullscreen_exclusive, and AppKit answers
		// that from -willUseFullScreenPresentationOptions with NSApplicationPresentationHideMenuBar. That
		// is a hard hide, not an auto-hide: the menu bar cannot be reached by moving to the top of the
		// screen no matter what, so releasing the cursor left nowhere to release it to.
		//
		// Nothing is given up by dropping it. The mode being named was the current mode, and the desktop
		// fullscreen path lands on the same geometry -- measured 2048x1152 points / 4096x2304 pixels either
		// way, on the same panel. macOS has no real exclusive mode-setting behind this anyway; both are a
		// window filling a Space.
		if (!SDL_SetWindowFullscreenMode(TheSDL3Window, NULL)) {
			fprintf(stderr, "WARNING: SDL_SetWindowFullscreenMode(desktop) failed: %s\n", SDL_GetError());
		}
#else
		SDL_DisplayID displayId = SDL_GetDisplayForWindow(TheSDL3Window);
		const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayId);
		if (mode) {
			if (!SDL_SetWindowFullscreenMode(TheSDL3Window, mode)) {
				fprintf(stderr, "WARNING: SDL_SetWindowFullscreenMode(native) failed: %s\n", SDL_GetError());
			}
		}
		else {
			fprintf(stderr, "WARNING: SDL_GetCurrentDisplayMode failed for fullscreen transition\n");
		}
#endif
	}
	else {
		int requestW = renderWidth;
		int requestH = renderHeight;
		bool skipResize = false;
#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
		// GeneralsX @bugfix 26/07/2026 renderWidth/renderHeight arrive in engine pixels, but
		// SDL_SetWindowSize takes points. Passing pixels straight through asked Cocoa for a window
		// twice the intended size on a 2x display, which it then clamped to the screen -- leaving
		// the window and the engine's idea of its own resolution permanently out of step.
		{
			int ptW = 0, ptH = 0, pxW = 0, pxH = 0;
			float density = 1.0f;
			if (GeneralsX_GetMacDisplayMetrics(ptW, ptH, pxW, pxH, density) && density > 0.0f)
			{
				// GeneralsX @bugfix 27/07/2026 Never derive the window size from the render size.
				//
				// Recovering a point size from a render resolution means undoing both the density and
				// the clarity percentage, and that round trip does not survive integer rounding and
				// the screen clamp: measured, it crept 1654 -> 1685 -> 1860 points, one resize event
				// at a time. An earlier form of the same mistake divided by the percentage-scaled
				// density, which at 50% asked for a window of renderWidth POINTS -- twice as many
				// pixels of window as the render target held, so the game drew at double size and the
				// window framed only its top-left corner.
				//
				// Two kinds of caller reach here and they want opposite things. A resize the user
				// performed arrives with a render size the current window already implies, and wants
				// the window left alone. A resolution pick arrives with a render size the window does
				// not imply, and wants the window changed to suit. Comparing the incoming size against
				// what the current window would produce tells them apart without either caller having
				// to announce itself.
				const int percent = GeneralsX_GetRenderScalePercent();
				int impliedW = 0, impliedH = 0;
				int storedW = 0, storedH = 0;
				const bool consistent =
					GeneralsX_GetWindowedRenderSize(impliedW, impliedH) &&
					abs(impliedW - renderWidth) <= 2 && abs(impliedH - renderHeight) <= 2 &&
					GeneralsX_GetWindowPointSize(storedW, storedH);

				// GeneralsX @bugfix 27/07/2026 Let AppKit enforce the ceiling instead of policing it.
				//
				// The window was observed growing on its own, a few hundred points at a time, until it was
				// wider than the display and the render target derived from it clipped the picture.
				// Instrumentation cleared every entry point the engine could be using: breakpoints on
				// SDL_SetWindowSize, -[NSWindow setFrame:display:], setFrame:display:animate: and
				// setContentSize: were never hit, yet the growth still arrived as ordinary Cocoa resize
				// events. Whatever the source, answering it from the resize handler means resizing the window
				// in response to a resize, which is how the earlier runaway to 5140x2004 started.
				//
				// A maximum size is the fix that does not need to know the cause: AppKit refuses the oversized
				// frame itself, before any event is posted, so there is nothing to react to.
				//
				// The value is the FULL display bounds, not the usable area less the title bar, and the
				// difference is a bug that took a while to place. SDL hands the maximum to Cocoa as
				// -setContentMaxSize:, which caps the fullscreen content as well as the window's -- and
				// fullscreen content is the whole display. A ceiling of usable-minus-bar (1002 points here)
				// therefore left a fullscreen drawable of 4096x2004 inside a 4096x2304 panel, with the 300
				// spare rows split as black bars above and below. Clearing the ceiling when fullscreen is
				// entered does not help: the ENTER_FULLSCREEN event arrives after Cocoa has already sized the
				// frame, and a native toggle gives no earlier hook at all.
				//
				// Display bounds still catch what this was for -- the growth ran past the display width -- and
				// they cannot clamp a fullscreen that is exactly that size. Keeping a window inside the usable
				// area is a separate job, done by the fit-to-usable rescale below and by
				// GeneralsX_EnsureWindowTitleBarOnScreen.
				SDL_Rect maxBounds;
				if (SDL_GetDisplayBounds(SDL_GetDisplayForWindow(TheSDL3Window), &maxBounds) &&
					maxBounds.w > 0 && maxBounds.h > 0)
				{
					SDL_SetWindowMaximumSize(TheSDL3Window, maxBounds.w, maxBounds.h);

					// GeneralsX @diagnostic 27/07/2026 Kept after the fullscreen letterbox hunt.
					//
					// The bars were a window ceiling clamping the fullscreen content, and the one thing that
					// would have identified it in an hour instead of a day is the ceiling's value at the
					// moment it is set. Cheap -- once per resolution change, never per frame -- and the
					// matching half is in the "entered fullscreen" line, which reports the ceiling actually
					// in force. If a letterbox is ever reported again, compare the two.
					fprintf(stderr, "INFO: window ceiling set to %dx%d points (display bounds)\n",
						maxBounds.w, maxBounds.h);

					// The ceiling stops a window from outgrowing the screen, but one that is already too
					// tall is still sitting wrong, and the symptom is an unreachable title bar.
					GeneralsX_EnsureWindowTitleBarOnScreen();
				}

				if (consistent)
				{
					// The window is already the size that produces this render size. Skipping the
					// resize avoids a redundant resize event, and with it the drift that used to
					// accumulate one event at a time.
					skipResize = true;
				}
				else
				{
					// A genuine resolution change: this is the resolution the user picked, in physical
					// pixels, so the window that shows it point-for-point is that over the density.
					//
					// The clarity percentage deliberately does not appear here. Callers that pass an
					// already-scaled render size are caught by the consistency check above and never
					// reach this branch, so trying to undo the percentage only corrupted the one
					// caller that passes the raw preference -- the windowed launch, where undoing 50%
					// asked for a 5760-point window and the screen clamp cut it to 1654.
					(void)percent;
					requestW = REAL_TO_INT_FLOOR((Real)renderWidth / density + 0.5f);
					requestH = REAL_TO_INT_FLOOR((Real)renderHeight / density + 0.5f);

					// Never ask for a window larger than the screen can show. Cocoa answers an
					// oversized request by clamping one axis and not the other, which breaks the
					// aspect ratio and used to leave the resize handler and this function trading
					// larger and larger numbers. Scaling both axes by the tighter ratio keeps the
					// picture correct and the request satisfiable, so the size Cocoa grants is the
					// size that was asked for.
					SDL_Rect usable;
					if (SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(TheSDL3Window), &usable) &&
						usable.w > 0 && usable.h > 0 && requestW > 0 && requestH > 0)
					{
						// The height available to CONTENT is the usable height less the title bar, since the
						// frame has to hold both. Fitting to the full usable height instead asks for a frame
						// that is a bar too tall, which Cocoa answers by shrinking the height alone -- and the
						// aspect-restore pass below then has to undo it. Reserving it here means the first
						// request is already satisfiable.
						const int fitH_avail = (usable.h > s_gxTitleBarPoints)
							? (usable.h - s_gxTitleBarPoints) : usable.h;
						const Real fitW = (Real)usable.w / (Real)requestW;
						const Real fitH = (Real)fitH_avail / (Real)requestH;
						const Real fit = (fitW < fitH) ? fitW : fitH;
						if (fit < 1.0f)
						{
							requestW = REAL_TO_INT_FLOOR((Real)requestW * fit);
							requestH = REAL_TO_INT_FLOOR((Real)requestH * fit);
						}
					}

					if (requestW < 320) requestW = 320;
					if (requestH < 240) requestH = 240;
					GeneralsX_SetWindowPointSize(requestW, requestH);
				}
			}
		}
#endif
		if (!skipResize && !SDL_SetWindowSize(TheSDL3Window, requestW, requestH)) {
			fprintf(stderr, "WARNING: SDL_SetWindowSize(%d,%d) failed: %s\n", requestW, requestH, SDL_GetError());
		}

#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
		// GeneralsX @bugfix 27/07/2026 Restore the aspect ratio after Cocoa clamps one axis.
		//
		// SDL_SetWindowSize takes the CONTENT size, but the screen has to hold content plus title bar.
		// A request that fills the usable height therefore overflows by the bar's height, and Cocoa
		// answers by shrinking the height alone -- 1838x1034 came back as 1838x1002, turning a 16:9
		// window into 1.83:1. SDL_GetWindowBordersSize cannot be used to reserve the bar in advance:
		// asked before the window is on screen it reports zero on all four sides.
		//
		// So the frame height is measured instead of predicted. Reading back what was actually granted
		// and rescaling the untouched axis by the same ratio restores the aspect in one correction. It
		// is a single pass, not a loop: the second request is strictly smaller than the first, so there
		// is nothing left for Cocoa to clamp and no way for the two to trade sizes.
		if (!skipResize)
		{
			int gotW = 0, gotH = 0;
			if (SDL_GetWindowSize(TheSDL3Window, &gotW, &gotH) && gotW > 0 && gotH > 0 &&
				(gotW != requestW || gotH != requestH))
			{
				// A refused height is the one honest measurement of the title bar available here, so take
				// it while it is in hand -- everywhere else has to work from the seeded guess.
				//
				// The test is on the FRAME, not the content: a request only tall enough to be clamped is one
				// whose content plus the assumed bar reaches the usable height. Testing the content against
				// the usable height instead would never fire now that the bar is reserved before the request.
				if (gotH < requestH)
				{
					SDL_Rect learnBounds;
					if (SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(TheSDL3Window), &learnBounds) &&
						learnBounds.h > 0 && (requestH + s_gxTitleBarPoints) >= learnBounds.h)
					{
						GeneralsX_LearnTitleBarFromClamp(learnBounds.h, gotH);
					}
				}

				int fixW = gotW;
				int fixH = gotH;
				if (gotH < requestH && requestH > 0) {
					fixW = REAL_TO_INT_FLOOR((Real)requestW * (Real)gotH / (Real)requestH + 0.5f);
				}
				if (gotW < requestW && requestW > 0) {
					fixH = REAL_TO_INT_FLOOR((Real)requestH * (Real)gotW / (Real)requestW + 0.5f);
				}
				if (fixW < 320) fixW = 320;
				if (fixH < 240) fixH = 240;

				if (fixW != gotW || fixH != gotH) {
					SDL_SetWindowSize(TheSDL3Window, fixW, fixH);
					SDL_GetWindowSize(TheSDL3Window, &gotW, &gotH);
					fprintf(stderr, "INFO: window clamped to %dx%d, aspect restored to %dx%d points\n",
						requestW, requestH, gotW, gotH);
				}
			}
			if (gotW > 0 && gotH > 0) {
				GeneralsX_SetWindowPointSize(gotW, gotH);
			}
		}
#endif
	}

	if (!windowed) {
		if ((SDL_GetWindowFlags(TheSDL3Window) & SDL_WINDOW_FULLSCREEN) == 0 &&
			!SDL_SetWindowFullscreen(TheSDL3Window, true)) {
			fprintf(stderr, "WARNING: SDL_SetWindowFullscreen(true) failed: %s\n", SDL_GetError());
		}
		SDL3_EnsureNativeFullscreen(TheSDL3Window);
	}
}

#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
// GeneralsX @bugfix 27/07/2026 Everything a resolution change owes the rest of the engine.
//
// This used to send only the header-template and mouse notifications, which is why a live clarity
// switch left the UI wrong: every .wnd layout and the control bar are laid out in render pixels and
// cached at the resolution they were built for, so going 50% -> 100% doubled the render target while
// the shell was still sized for the old one. The stock options screen (OptionsMenu.cpp) recreates
// them, and any other path that changes the render resolution owes the same work.
//
// Null checks throughout because a windowed launch reaches the resize path before GameClient::init
// has created the shell, the in-game UI, or the tactical view.
void GeneralsX_NotifyResolutionChanged(void)
{
	if (TheHeaderTemplateManager) {
		TheHeaderTemplateManager->onResolutionChanged();
	}
	if (TheMouse) {
		TheMouse->onResolutionChanged();
	}

	// Rebuild what is sized in render pixels. Order follows the stock options screen.
	if (TheShell) {
		TheShell->recreateWindowLayouts();
	}
	if (TheInGameUI) {
		TheInGameUI->recreateControlBar();
		TheInGameUI->refreshCustomUiResources();
	}
	if (TheTacticalView) {
		// Matches the stock path: keep the camera limits and zoom consistent with the new resolution
		// without disturbing a scripted camera, which gets reset at game start anyway.
		TheTacticalView->setCameraHeightAboveGroundLimitsToDefault();
		TheTacticalView->setZoomToMax();
	}
}

// GeneralsX @bugfix 27/07/2026 Apply a clarity change by re-rendering, not by resizing.
//
// The window keeps the size the user gave it. What changes is how many pixels are drawn into it: the
// window's full pixel extent at 100%, half of it in each axis at 50%, with the swapchain upscaling
// the difference. That is the entire difference between the two modes, and it is why switching them
// no longer moves or resizes anything on screen.
//
// The previous version did the opposite -- it held the render resolution fixed and resized the window
// to suit -- which is what made 50% draw the game at double size with the window framing only its
// top-left corner, and what let an oversized request get clamped by macOS into a runaway.
//
// Returns TRUE when it did the work, FALSE in fullscreen where the caller owns the transition.
Bool GeneralsX_ApplyRenderScaleToWindow(void)
{
	extern SDL_Window* TheSDL3Window;
	if (!TheSDL3Window || !TheDisplay) return FALSE;
	if ((SDL_GetWindowFlags(TheSDL3Window) & SDL_WINDOW_FULLSCREEN) != 0) return FALSE;

	int targetW = 0, targetH = 0;
	if (!GeneralsX_GetWindowedRenderSize(targetW, targetH)) return FALSE;

	if ((Int)TheDisplay->getWidth() == targetW && (Int)TheDisplay->getHeight() == targetH) {
		return TRUE;		// already there; a device reset for nothing would just cost a stutter
	}

	// The guard matters here: this is a resolution change with no window change, so the resize event
	// the device reset provokes must not be answered by resizing the window back.
	s_gxApplyingWindowMode = true;
	const Bool ok = TheDisplay->setDisplayMode(targetW, targetH, TheDisplay->getBitDepth(), TRUE);
	s_gxApplyingWindowMode = false;

	if (!ok) {
		fprintf(stderr, "WARNING: clarity change to %dx%d rejected, staying at %dx%d\n",
			targetW, targetH, (Int)TheDisplay->getWidth(), (Int)TheDisplay->getHeight());
		return FALSE;
	}

	extern void GeneralsX_NotifyResolutionChanged(void);
	GeneralsX_NotifyResolutionChanged();

	fprintf(stderr, "INFO: clarity change: render resolution now %dx%d (%d%%), window unchanged\n",
		targetW, targetH, GeneralsX_GetRenderScalePercent());
	return TRUE;
}

// GeneralsX @feature 27/07/2026 Follow macOS's own fullscreen toggle instead of owning the transition.
//
// Ctrl+Cmd+F and the green zoom button already worked -- the window is created SDL_WINDOW_RESIZABLE
// and SDL3 uses native fullscreen Spaces on macOS -- but the engine never noticed. It kept the render
// resolution it had in the window while DXVK's swapchain grew to the panel, so DX8Wrapper's pillarbox
// stretched an 1802x1002 frame across 4096x2304: measured, a 2.27x upscale, blurrier than the 50%
// clarity mode. TheDisplay also still reported windowed, which is wrong for two things that ask --
// Mouse::canCapture picks its capture rule by window mode, and the pillarbox logs.
//
// So this is the reaction, not the transition: macOS owns the animation and the Space, and this only
// has to say what render resolution belongs to the result.
//
// The two directions are not symmetrical, which cost a round of debugging. Entering, the target comes
// from the display and not from the window, so it can be computed straight away. Leaving, the target IS
// the window, and the window is not back yet -- so leaving only records the mode and lets the RESIZED
// event that follows the restore do the sizing.
//
// Deliberately not persisted. The window mode still comes from the launcher's -fullscreen or
// Options.ini at startup; this is a live toggle, and a toggle that silently rewrote the launch mode
// would be the same trap the clarity switch used to be.
void GeneralsX_OnFullscreenChanged(Bool nowFullscreen)
{
	extern SDL_Window* TheSDL3Window;
	if (!TheSDL3Window || !TheDisplay || !TheWritableGlobalData) return;

	// Trust the window's own flag over the event. SDL posts a LEAVE_FULLSCREEN while a windowed window
	// is being created, before there is a render target worth resizing, and acting on that would drag
	// the launch resolution around before the window has settled.
	const Bool flagSaysFullscreen =
		(SDL_GetWindowFlags(TheSDL3Window) & SDL_WINDOW_FULLSCREEN) != 0 ? TRUE : FALSE;
	if (flagSaysFullscreen != nowFullscreen) return;

	// A transition the engine itself started (a resolution pick, or the fullscreen launch path) is
	// already handled by the code that started it.
	if (s_gxApplyingWindowMode) return;

	int targetW = 0, targetH = 0;
	if (nowFullscreen) {
		// Clear the window ceiling so a later reset cannot be clamped by it. This does NOT undo the
		// letterboxing that a too-low ceiling causes: the frame is already sized by the time this event is
		// posted, and clearing it here was measured to leave the drawable at 4096x2004 regardless. The bars
		// are prevented by the ceiling's value -- display bounds, never usable-minus-bar -- where it is set.
		SDL_SetWindowMaximumSize(TheSDL3Window, 0, 0);

		// The fullscreen render target -- the same helper the launch path and the clarity switch use,
		// so all three agree on both the percentage and the panel-native point mode.
		if (!GeneralsX_GetFullscreenRenderSize(targetW, targetH)) {
			fprintf(stderr, "WARNING: fullscreen transition: display metrics unavailable\n");
			return;
		}
	}
	else {
		// GeneralsX @bugfix 27/07/2026 Do not size anything from a window Cocoa is still restoring.
		//
		// LEAVE_FULLSCREEN arrives BEFORE the window frame comes back -- measured, SDL still reported the
		// fullscreen size at this point, and the handler duly logged "left fullscreen: render resolution
		// now 2048x1152" for a window about to become 1304 points wide. That fullscreen-sized render
		// target was then applied to a window that did not match it, and the resize events chasing the
		// mismatch walked the window off the top of the screen.
		//
		// Nothing needs deriving here. The RESIZED event that follows the restore carries the settled
		// size, and handleWindowEvent already turns exactly that into a render resolution -- it was only
		// held off before because the fullscreen flag was still set. Setting the mode and standing aside
		// is the whole job.
		TheDisplay->setWindowed(TRUE);
		TheWritableGlobalData->m_windowed = TRUE;

		if (TheMouse) {
			TheMouse->setMouseLimits();
			TheMouse->refreshCursorCapture();
		}

		fprintf(stderr, "INFO: left fullscreen: awaiting restored window size\n");
		return;
	}

	// Only the entering direction reaches here; leaving returned above.
	if (targetW <= 0 || targetH <= 0) return;

	// Tell the engine its window mode before the device reset: setDisplayMode is handed this same flag,
	// and getWindowed() is read during the reset by the pillarbox path.
	TheDisplay->setWindowed(FALSE);
	TheWritableGlobalData->m_windowed = FALSE;

	const Bool alreadyThere =
		(Int)TheDisplay->getWidth() == targetW && (Int)TheDisplay->getHeight() == targetH;

	if (!alreadyThere) {
		// Both guards, for two different problems. s_gxApplyingWindowMode stops handleWindowEvent from
		// answering the resize the device reset provokes; s_gxFollowingNativeFullscreen stops
		// setDisplayMode from re-applying the window mode macOS has already applied, which is what made
		// one keypress produce four transitions.
		s_gxApplyingWindowMode = true;
		s_gxFollowingNativeFullscreen = true;
		const Bool ok = TheDisplay->setDisplayMode(
			targetW, targetH, TheDisplay->getBitDepth(), FALSE);
		s_gxFollowingNativeFullscreen = false;
		s_gxApplyingWindowMode = false;

		if (!ok) {
			fprintf(stderr, "WARNING: fullscreen transition to %dx%d rejected, staying at %dx%d\n",
				targetW, targetH,
				(Int)TheDisplay->getWidth(), (Int)TheDisplay->getHeight());
			return;
		}

		extern void GeneralsX_NotifyResolutionChanged(void);
		GeneralsX_NotifyResolutionChanged();
	}

	// The capture rule differs between windowed and fullscreen, so re-evaluate it against the mode the
	// engine is now in. A user-requested release survives this: it is a block reason, not a state.
	if (TheMouse) {
		TheMouse->setMouseLimits();
		TheMouse->refreshCursorCapture();
	}

	// GeneralsX @diagnostic 27/07/2026 Kept after the fullscreen letterbox hunt.
	//
	// The drawable is logged next to the render size because a mismatch between them IS the letterbox: a
	// window ceiling still in force in fullscreen left 300 pixel rows of the panel with nothing in them,
	// while the render resolution and the swapchain both reported the full 4096x2304 and looked correct.
	// The ceiling and the point size come with it, because a short drawable says nothing about the cause.
	//
	// Reading it: drawable == panel pixels means the picture fills the screen. Drawable short of the panel
	// with a non-zero max is the ceiling clamping, and "window ceiling set to" says what put it there.
	// One line per transition, so it is not a hot path.
	{
		int drawW = 0, drawH = 0, ptW = 0, ptH = 0, maxW = 0, maxH = 0;
		SDL_GetWindowSizeInPixels(TheSDL3Window, &drawW, &drawH);
		SDL_GetWindowSize(TheSDL3Window, &ptW, &ptH);
		SDL_GetWindowMaximumSize(TheSDL3Window, &maxW, &maxH);
		fprintf(stderr, "INFO: entered fullscreen: render resolution now %dx%d (%d%%), drawable %dx%d, "
			"window %dx%d points, max %dx%d\n",
			targetW, targetH, s_gxRenderScalePercent, drawW, drawH, ptW, ptH, maxW, maxH);
	}
}
#endif
#endif

// Filtered resolution cache — built once, clamps widths to 4:3..16:9 and deduplicates.
struct FilteredRes { Int w, h, bits; };
static std::vector<FilteredRes> s_filteredResolutions;
static bool s_filteredDirty = true;

static void buildFilteredResolutions()
{
	s_filteredResolutions.clear();
	const RenderDeviceDescClass &devDesc = WW3D::Get_Render_Device_Desc(0);
	const DynamicVectorClass<ResolutionDescClass> &resolutions = devDesc.Enumerate_Resolutions();

	int nativeW = 0, nativeH = 0;
	float density = 1.0f;
	DX8Wrapper::GetNativeDisplaySize(nativeW, nativeH, density);

	for (int i = 0; i < resolutions.Count(); i++) {
		if (!isResolutionSupported(resolutions[i])) continue;
		Int w = resolutions[i].Width;
		Int h = resolutions[i].Height;
		Int bits = resolutions[i].BitDepth;
		if (nativeH > 0 && h > nativeH) continue;
		Int minW = h * 4 / 3;
		Int maxW = h * 16 / 9;
		if (w < minW) w = minW;
		if (w > maxW) w = maxW;
		bool duplicate = false;
		for (const auto& e : s_filteredResolutions) {
			if (e.w == w && e.h == h && e.bits == bits) { duplicate = true; break; }
		}
		if (!duplicate) s_filteredResolutions.push_back({w, h, bits});
	}

#if defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
	// GeneralsX @bugfix 26/07/2026 Guarantee the display's native pixel resolution is offered.
	// The list above comes from DXVK's EnumAdapterModes, which has no obligation to report the
	// Retina-scaled mode the game now boots into. Without this entry the options menu cannot
	// re-select native HiDPI after the user has switched away from it once, and the resolution
	// combo box would show the running resolution as absent from its own list.
	if (nativeW > 0 && nativeH > 0) {
		bool haveNative = false;
		for (const auto& e : s_filteredResolutions) {
			if (e.w == nativeW && e.h == nativeH) { haveNative = true; break; }
		}
		if (!haveNative) s_filteredResolutions.push_back({nativeW, nativeH, 32});
	}
#endif

	s_filteredDirty = false;
}

Int W3DDisplay::getDisplayModeCount()
{
	if (s_filteredDirty) buildFilteredResolutions();
	return (Int)s_filteredResolutions.size();
}

void W3DDisplay::getDisplayModeDescription(Int modeIndex, Int *xres, Int *yres, Int *bitDepth)
{
	if (s_filteredDirty) buildFilteredResolutions();
	if (modeIndex >= 0 && modeIndex < (Int)s_filteredResolutions.size()) {
		*xres = s_filteredResolutions[modeIndex].w;
		*yres = s_filteredResolutions[modeIndex].h;
		*bitDepth = s_filteredResolutions[modeIndex].bits;
	}
}

void W3DDisplay::setGamma(Real gamma, Real bright, Real contrast, Bool calibrate)
{
	if (m_windowed)
		return;	//we don't allow gamma to change in window because it would affect desktop.

	DX8Wrapper::Set_Gamma(gamma,bright,contrast,calibrate, false);
}

/** Set resolution of display */
//=============================================================================
Bool W3DDisplay::setDisplayMode( UnsignedInt xres, UnsignedInt yres, UnsignedInt bitdepth, Bool windowed )
{
	const UnsignedInt oldWidth = getWidth();
	const UnsignedInt oldHeight = getHeight();
	const UnsignedInt oldBitDepth = getBitDepth();
	const Bool oldWindowed = getWindowed();

	if (WW3D_ERROR_OK == WW3D::Set_Device_Resolution(xres,yres,bitdepth,windowed,true))
	{
		#ifdef SAGE_USE_SDL3
		SDL3_ApplyWindowModeForRenderConfig(windowed, xres, yres);
		#endif
		Render2DClass::Set_Screen_Resolution(RectClass(0, 0, xres, yres));
		Display::setDisplayMode(xres, yres, bitdepth, windowed);
		return TRUE;
	}

	//set back to the original mode.
	WW3D::Set_Device_Resolution(oldWidth, oldHeight, oldBitDepth, oldWindowed, true);
	Render2DClass::Set_Screen_Resolution(RectClass(0, 0, oldWidth, oldHeight));
	Display::setDisplayMode(oldWidth, oldHeight, oldBitDepth, oldWindowed);
	return FALSE;	//did not change to a new mode.
}

Bool W3DDisplay::getViewportRect( Int& x, Int& y, Int& width, Int& height ) const
{
	return DX8Wrapper::Pillarbox_Get_Rect(x, y, width, height) ? TRUE : FALSE;
}

/** Set width of display */
//=============================================================================
void W3DDisplay::setWidth( UnsignedInt width )
{

	// extending functionality
	Display::setWidth( width );

	// our 2D renderer will use mapping coords to make (0,0) the upper left
	// of the screen with (width,height) at the lower right
	m_2DRender->Set_Coordinate_Range( RectClass( 0, 0, getWidth(), getHeight() ) );

}

// W3DDisplay::setHeight ======================================================
/** Set height of display */
//=============================================================================
void W3DDisplay::setHeight( UnsignedInt height )
{

	// extending functionality
	Display::setHeight( height );

	// our 2D renderer will use mapping coords to make (0,0) the upper left
	// of the screen with (width,height) at the lower right
	m_2DRender->Set_Coordinate_Range( RectClass( 0, 0, getWidth(), getHeight() ) );

}

void W3DDisplay::onBeginBatch()
{
	m_batchTexture = nullptr;
	m_batchMode = DRAW_IMAGE_ALPHA;
	m_batchGrayscale = FALSE;
	m_batchNeedsInit = TRUE;

	if (m_2DRender)
	{
		m_2DRender->Reset();
	}
}

void W3DDisplay::onEndBatch()
{
	REF_PTR_RELEASE(m_batchTexture);
}

void W3DDisplay::onFlush()
{
	if (m_2DRender && !m_batchNeedsInit)
	{
		m_2DRender->Render();
		m_2DRender->Reset();
		m_batchNeedsInit = TRUE;
	}
}

void W3DDisplay::setup2DRenderState(TextureClass *tex, DrawImageMode mode, Bool grayscale)
{
	if (m_isBatching)
	{
		if (!m_batchNeedsInit && m_batchTexture == tex && m_batchMode == mode && m_batchGrayscale == grayscale)
		{
			return;
		}

		onFlush();

		if (tex != m_batchTexture)
		{
			if (tex)
			{
				tex->Add_Ref();
			}
			if (m_batchTexture)
			{
				m_batchTexture->Release_Ref();
			}

			m_batchTexture = tex;
		}

		m_batchMode = mode;
		m_batchGrayscale = grayscale;
		m_batchNeedsInit = FALSE;
	}
	else if (m_2DRender)
	{
		m_2DRender->Reset();
	}

	if (m_2DRender)
	{
		if (tex)
		{
			m_2DRender->Enable_Texturing(TRUE);
			m_2DRender->Set_Texture(tex);
		}
		else
		{
			m_2DRender->Enable_Texturing(FALSE);
		}

		switch (mode)
		{
			default:
			case DRAW_IMAGE_ALPHA:
				m_2DRender->Enable_Additive(FALSE);
				m_2DRender->Enable_Alpha(TRUE);
				m_2DRender->Enable_Grayscale(grayscale);
				break;
			case DRAW_IMAGE_GRAYSCALE:
				m_2DRender->Enable_Additive(FALSE);
				m_2DRender->Enable_Alpha(TRUE);
				m_2DRender->Enable_Grayscale(TRUE);
				break;
			case DRAW_IMAGE_ADDITIVE:
				m_2DRender->Enable_Additive(TRUE);
				m_2DRender->Enable_Alpha(FALSE);
				m_2DRender->Enable_Grayscale(grayscale);
				break;
			case DRAW_IMAGE_SOLID:
				m_2DRender->Enable_Additive(FALSE);
				m_2DRender->Enable_Alpha(FALSE);
				m_2DRender->Enable_Grayscale(grayscale);
				break;
		}
	}
}

// W3DDisplay::initAssets =====================================================
/** */
//=============================================================================
void W3DDisplay::initAssets()
{

}

// W3DDisplay::init3DScene ====================================================
/** */
//=============================================================================
void W3DDisplay::init3DScene()
{

}

// W3DDisplay::init2DScene ====================================================
/** This is the 2D scene, you can use it to draw on a 2D plane over the
	* 3D background */
//=============================================================================
void W3DDisplay::init2DScene()
{

}

// W3DDisplay::init ===========================================================
/** Initialize or re-initialize the W3D display system.  Here we need to
  * create our window, and get our 3D hardware setup and online */
//=============================================================================
void W3DDisplay::init()
{

	//
	// call our base class init, this method should be able to handle re-entry
	// with its own logic
	//
	Display::init();

	// handle re-entry for ourselves
	if( m_initialized )
	{

		/// @todo W3DDisplay needs RE-init logic!
		return;

	}
	// Override the W3D File system
	TheW3DFileSystem = NEW W3DFileSystem;

	// init the Westwood math library
	WWMath::Init();

	if (!TheGlobalData->m_headless)
	{

		// create our 3D interface scene
		m_3DInterfaceScene = NEW_REF( RTS3DInterfaceScene, () );
		m_3DInterfaceScene->Set_Ambient_Light( Vector3( 1, 1, 1 ) );

		// create our 2D scene
		m_2DScene = NEW_REF( RTS2DScene, () );
		m_2DScene->Set_Ambient_Light( Vector3( 1, 1, 1 ) );

		// create our 3D scene
		m_3DScene =NEW_REF( RTS3DScene, () );
	#if defined(RTS_DEBUG)
		if( TheGlobalData->m_wireframe )
			m_3DScene->Set_Polygon_Mode( SceneClass::LINE );
	#endif
	//============================================================================
		// m_myLight = NEW_REF
	//============================================================================
		Int lindex;
		for (lindex=0; lindex<TheGlobalData->m_numGlobalLights; lindex++)
		{	m_myLight[lindex] = NEW_REF( LightClass, (LightClass::DIRECTIONAL) );
		}

		setTimeOfDay( TheGlobalData->m_timeOfDay );	//set each light to correct values for given time

		for (lindex=0; lindex<TheGlobalData->m_numGlobalLights; lindex++)
		{	m_3DScene->setGlobalLight( m_myLight[lindex], lindex );
		}

	#ifdef SAMPLE_DYNAMIC_LIGHT
		theDynamicLight = NEW_REF(W3DDynamicLight, ());
		Real red = 1;
		Real green = 1;
		Real blue = 0;
		if(red==0 && blue==0 && green==0) {
			red = green = blue = 1;
		}
		theDynamicLight->Set_Ambient( Vector3( red, green, blue ) );
		theDynamicLight->Set_Diffuse( Vector3( red, green, blue) );
		theDynamicLight->Set_Position(Vector3(0, 0, 4));
		theDynamicLight->Set_Far_Attenuation_Range(1, 8);
		// Note: Don't Add_Render_Object dynamic lights.
		m_3DScene->addDynamicLight( theDynamicLight );
	#endif

	}

	// create a new asset manager
	m_assetManager = NEW W3DAssetManager;
	m_assetManager->Register_Prototype_Loader(&_ParticleEmitterLoader );
	m_assetManager->Register_Prototype_Loader(&_AggregateLoader);
	m_assetManager->Set_WW3D_Load_On_Demand( true );

	if (!TheGlobalData->m_headless)
	{

		if (TheGlobalData->m_incrementalAGPBuf)
		{
			SortingRendererClass::SetMinVertexBufferSize(1);
		}

		// Register SDL3 display size providers for pillarbox before device init
#ifdef SAGE_USE_SDL3
		DX8Wrapper::Set_Display_Size_Provider(SDL3_GetNativeDisplaySize, SDL3_GetWindowSizeInPixels);
#endif

		// GeneralsX @bugfix felipebraz 16/02/2026 Add detailed WW3D init logging
		fprintf(stderr, "DEBUG: About to call WW3D::Init() with ApplicationHWnd=%p\n", ApplicationHWnd);
		WW3DErrorType ww3d_result = WW3D::Init( ApplicationHWnd );

		// Decode error type
		const char* error_names[] = {
			"WW3D_ERROR_OK",
			"WW3D_ERROR_GENERIC",
			"WW3D_ERROR_LOAD_FAILED",
			"WW3D_ERROR_SAVE_FAILED",
			"WW3D_ERROR_WINDOW_NOT_OPEN",
			"WW3D_ERROR_INITIALIZATION_FAILED"
		};
		const char* error_name = (ww3d_result >= 0 && ww3d_result <= 5) ? error_names[ww3d_result] : "UNKNOWN";
		fprintf(stderr, "DEBUG: WW3D::Init() returned: %d (%s)\n", ww3d_result, error_name);

		if (ww3d_result != WW3D_ERROR_OK) {
			fprintf(stderr, "ERROR: WW3D::Init() failed with %s - DirectX8/DXVK initialization error\n", error_name);
			throw ERROR_INVALID_D3D;	//failed to initialize.  User probably doesn't have DX 8.1
		}

		// GeneralsX @bugfix felipebraz 16/02/2026 Show window after DirectX8/DXVK initialized
		#ifndef _WIN32
		extern SDL_Window* TheSDL3Window;
		if (TheSDL3Window) {
			fprintf(stderr, "DEBUG: Showing SDL3 window after WW3D init...\n");
			SDL_ShowWindow(TheSDL3Window);
		}
		#endif

		WW3D::Set_Prelit_Mode( WW3D::PRELIT_MODE_LIGHTMAP_MULTI_PASS );
		WW3D::Set_Collision_Box_Display_Mask(0x00);	///<set to 0xff to make collision boxes visible
		WW3D::Enable_Static_Sort_Lists(true);
		WW3D::Set_Thumbnail_Enabled(false);
		WW3D::Set_Screen_UV_Bias( TRUE );  ///< this makes text look good :)
		WW3D::Set_Texture_Bitdepth(32);

		fprintf(stderr, "[DEBUG-WIN] W3DDisplay::init() - TheGlobalData->m_windowed=%d\n", (int)TheGlobalData->m_windowed);
		setWindowed( TheGlobalData->m_windowed );

		// create a 2D renderer helper
		m_2DRender = NEW Render2DClass;
		DEBUG_ASSERTCRASH( m_2DRender, ("Cannot create Render2DClass") );

		WW3DErrorType renderDeviceError;
		Int attempt = 0;
		do
		{
			switch (attempt)
			{
			case 0:
			{
				// set our default width and height and bit depth
				setWidth( TheGlobalData->m_xResolution );
				setHeight( TheGlobalData->m_yResolution );
				setBitDepth( DEFAULT_DISPLAY_BIT_DEPTH );
				break;
			}
			case 1:
			{
				// Getting the device at the default bit depth (32) didn't work, so try
				// getting a 16 bit display.  (Voodoo 1-3 only supported 16 bit.) jba.
				setBitDepth( MIN_DISPLAY_BIT_DEPTH );
				break;
			}
			case 2:
			{
				// TheSuperHackers @bugfix xezon 11/06/2025 Now tries a safe default resolution
				// if the custom resolution did not succeed. This is unlikely to happen but is possible
				// if the user writes an unsupported resolution in the Option Preferences or if the
				// graphics adapter does not support the minimum display resolution to begin with.
				Int xres = DEFAULT_DISPLAY_WIDTH;
				Int yres = DEFAULT_DISPLAY_HEIGHT;
				Int bitDepth = DEFAULT_DISPLAY_BIT_DEPTH;
				Int displayModeCount = getDisplayModeCount();
				Int displayModeIndex = 0;
				for (; displayModeIndex < displayModeCount; ++displayModeIndex)
				{
					getDisplayModeDescription(displayModeIndex, &xres, &yres, &bitDepth);
					if (xres * yres >= DEFAULT_DISPLAY_WIDTH * DEFAULT_DISPLAY_HEIGHT)
						break; // Is good enough. Use it.
				}
				TheWritableGlobalData->m_xResolution = xres;
				TheWritableGlobalData->m_yResolution = yres;
				setWidth( xres );
				setHeight( yres );
				setBitDepth( bitDepth );
				break;
			}
			}

#if defined(SAGE_USE_SDL3) && defined(__APPLE__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
			// GeneralsX @bugfix 27/07/2026 Establish a windowed launch's clarity mode before the device
			// exists, not by resetting it afterwards.
			//
			// Fullscreen gets this for free: SDL3Main injects -xres/-yres already scaled by the
			// percentage. Windowed mode keeps the Resolution preference verbatim, so a saved 50% used to
			// be ignored until something resized the window. Applying it with a setDisplayMode after
			// Set_Render_Device is not an option -- the base Display::setDisplayMode dereferences
			// TheTacticalView, which GameClient::init does not create until after this function returns,
			// so it segfaulted on every windowed launch at 50%.
			//
			// Doing it here costs nothing and skips the device reset entirely. Sizing the window from the
			// unscaled preference first is what makes the reduced render size derivable, which is the
			// same invariant every other path uses: renderPixels = windowPixels * percent / 100.
			if (attempt == 0 && getWindowed() && GeneralsX_GetRenderScalePercent() < 100)
			{
				SDL3_ApplyWindowModeForRenderConfig(TRUE, getWidth(), getHeight());

				int scaledW = 0, scaledH = 0;
				if (GeneralsX_GetWindowedRenderSize(scaledW, scaledH))
				{
					int winPtW = 0, winPtH = 0;
					GeneralsX_GetWindowPointSize(winPtW, winPtH);
					fprintf(stderr, "INFO: windowed launch at %d%%: rendering %dx%d into a %dx%d point window\n",
						GeneralsX_GetRenderScalePercent(), scaledW, scaledH, winPtW, winPtH);
					setWidth(scaledW);
					setHeight(scaledH);
				}
			}
#endif

			// TheSuperHackers @feature Mauller 13/03/2026 Add native MSAA support, must be set before creating render device
			WW3D::Set_MSAA_Mode((WW3D::MultiSampleModeEnum)TheWritableGlobalData->m_antiAliasLevel);

			renderDeviceError = WW3D::Set_Render_Device(
				0,
				getWidth(),
				getHeight(),
				getBitDepth(),
				getWindowed(),
				true );

			// TheSuperHackers @info Update the MSAA mode that was set as some GPU's may not support certain levels
			// Texture filtering must also be updated after render device initialization
			if (renderDeviceError == WW3D_ERROR_OK) {
				TheWritableGlobalData->m_antiAliasLevel = (UnsignedInt)WW3D::Get_MSAA_Mode();
				WW3D::Set_Texture_Filter(TheWritableGlobalData->m_textureFilteringMode);
				TheWritableGlobalData->m_textureFilteringMode = WW3D::Get_Texture_Filter();
				WW3D::Set_Anisotropy_Level(TheWritableGlobalData->m_textureAnisotropyLevel);
				TheWritableGlobalData->m_textureAnisotropyLevel = WW3D::Get_Anisotropy_Level();
			}

			++attempt;
		}
		while (attempt < 3 && renderDeviceError != WW3D_ERROR_OK);

		if (renderDeviceError != WW3D_ERROR_OK)
		{
			WW3D::Shutdown();
			WWMath::Shutdown();
			throw ERROR_INVALID_D3D;	//failed to initialize.  User probably doesn't have DX 8.1
			DEBUG_CRASH( ("Unable to set render device") );
			return;
		}

		#ifdef SAGE_USE_SDL3
		// The windowed clarity mode was already applied before the device was created, above; this is
		// the stock call that reconciles the window with whatever mode the device ended up in.
		SDL3_ApplyWindowModeForRenderConfig(getWindowed(), getWidth(), getHeight());
		#endif

		//Check if level was never set and default to setting most suitable for system.
		if (TheGameLODManager->getStaticLODLevel() == STATIC_GAME_LOD_UNKNOWN)
		{
			TheGameLODManager->setStaticLODLevel(TheGameLODManager->getRecommendedStaticLODLevel());
		}
		else
		{
			//Static LOD level was applied during GameLOD manager init except for texture reduction
			//which needs to be applied here.
			TheGameClient->setTextureLOD(TheWritableGlobalData->m_textureReductionFactor);
		}

		if (TheGlobalData->m_displayGamma != 1.0f)
			setGamma(TheGlobalData->m_displayGamma,0.0f,1.0f,FALSE);
	}

	initAssets();

	if (!TheGlobalData->m_headless)
	{
		init2DScene();
		init3DScene();
		W3DShaderManager::init();

		// Create and initialize the debug display
		m_nativeDebugDisplay = NEW W3DDebugDisplay();
		m_debugDisplay = m_nativeDebugDisplay;
		if ( m_nativeDebugDisplay )
		{
			m_nativeDebugDisplay->init();
			GameFont *font;

			if (TheGlobalLanguageData && TheGlobalLanguageData->m_nativeDebugDisplay.name.isNotEmpty())
			{
				font=TheFontLibrary->getFont(
					TheGlobalLanguageData->m_nativeDebugDisplay.name,
					TheGlobalLanguageData->m_nativeDebugDisplay.size,
					TheGlobalLanguageData->m_nativeDebugDisplay.bold);
			}
			else
				font=TheFontLibrary->getFont( "FixedSys", 8, FALSE );

			m_nativeDebugDisplay->setFont( font );
			m_nativeDebugDisplay->setFontHeight( 13 );
			m_nativeDebugDisplay->setFontWidth( 9 );
		}

		DX8WebBrowser::Initialize();
	}

	// we're now online
	m_initialized = true;
	if( TheGlobalData->m_displayDebug )
	{
		m_debugDisplayCallback = StatDebugDisplay;
	}
}

// W3DDisplay::reset ===========================================================
/** Reset the W3D display system.  Here we need to
  * remove the objects from the previous map. */
//=============================================================================
void W3DDisplay::reset()
{

	Display::reset();

	// Remove all render objects.

	if (m_3DScene != nullptr)
	{
		SceneIterator *sceneIter = m_3DScene->Create_Iterator();
		sceneIter->First();
		while(!sceneIter->Is_Done()) {
			RenderObjClass * robj = sceneIter->Current_Item();
			robj->Add_Ref();
			m_3DScene->Remove_Render_Object(robj);
			robj->Release_Ref();
			sceneIter->Next();
		}
		m_3DScene->Destroy_Iterator(sceneIter);
	}

	m_isClippedEnabled = FALSE;

	// release any unused assets from W3D
	/// @todo really need that "scene abstraction", having this stuff in the display is icky
	m_assetManager->Release_Unused_Assets();

	if (TheWritableGlobalData)
		TheWritableGlobalData->m_drawSkyBox =0;
}

const UnsignedInt START_CUMU_FRAME = LOGICFRAMES_PER_SECOND / 2;	// skip first half-sec

void W3DDisplay::updateAverageFPS()
{
	constexpr const Int FPS_HISTORY_SIZE = 30;

	static Int64 lastUpdateTime64 = 0;
	static Int historyOffset = 0;
	static Real fpsHistory[FPS_HISTORY_SIZE] = {0};

	const Int64 freq64 = getPerformanceCounterFrequency();
	const Int64 time64 = getPerformanceCounter();

#if defined(RTS_DEBUG)
	if (TheGameLogic->getFrame() == START_CUMU_FRAME)
	{
		m_timerAtCumuFPSStart = time64;
	}
#endif

	const Int64 timeDiff = time64 - lastUpdateTime64;

	// convert elapsed time to seconds
	Real elapsedSeconds = (Real)timeDiff/(Real)freq64;

	// append new sample to fps history.
	if (historyOffset >= FPS_HISTORY_SIZE)
		historyOffset = 0;

	m_currentFPS = 1.0f/elapsedSeconds;
	fpsHistory[historyOffset++] = m_currentFPS;

	// determine average frame rate over our past history.
	const Real sum = std::accumulate(fpsHistory, fpsHistory + FPS_HISTORY_SIZE, 0.0f);
	m_averageFPS = sum / FPS_HISTORY_SIZE;

	lastUpdateTime64 = time64;
}

#if defined(RTS_DEBUG)	//debug hack to view object under mouse stats
ICoord2D TheMousePos;
#endif

// W3DDisplay::gatherDebugStats ===================================================
/** Compute and display debug stats on screen */
//=============================================================================
void W3DDisplay::gatherDebugStats()
{
	static UnsignedInt s_framesRenderedSinceLastUpdate = 0;
	static Int64 s_lastUpdateTime64 = 0;
	static double s_timeSinceLastUpdateInSecs = 0.0;
	static Int s_drawCallsSinceLastUpdate = 0;
	static Int s_sortedPolysSinceLastUpdate = 0;

	// allocate the display strings if needed
	if( m_displayStrings[0] == nullptr )
	{
		GameFont *font;
		if (TheGlobalLanguageData && TheGlobalLanguageData->m_nativeDebugDisplay.name.isNotEmpty())
		{
			font=TheFontLibrary->getFont(
				TheGlobalLanguageData->m_nativeDebugDisplay.name,
				TheGlobalLanguageData->m_nativeDebugDisplay.size,
				TheGlobalLanguageData->m_nativeDebugDisplay.bold);
		}
		else
			font = TheFontLibrary->getFont( "FixedSys", 8, FALSE );

		for (int i = 0; i < DisplayStringCount; i++)
		{
			if (m_displayStrings[i] == nullptr)
			{
				m_displayStrings[i] = TheDisplayStringManager->newDisplayString();
				DEBUG_ASSERTCRASH( m_displayStrings[i], ("Failed to create DisplayString") );
				m_displayStrings[i]->setFont( font );
			}
		}

	}

	if (m_benchmarkDisplayString == nullptr)
	{
		GameFont *thisFont = TheFontLibrary->getFont( "FixedSys", 8, FALSE );
		m_benchmarkDisplayString = TheDisplayStringManager->newDisplayString();
		DEBUG_ASSERTCRASH( m_benchmarkDisplayString, ("Failed to create DisplayString") );
		m_benchmarkDisplayString->setFont( thisFont );
	}

	++s_framesRenderedSinceLastUpdate;
  s_drawCallsSinceLastUpdate += Debug_Statistics::Get_Draw_Calls();
	s_sortedPolysSinceLastUpdate += Debug_Statistics::Get_Sorting_Polygons();

	Int64 freq64 = getPerformanceCounterFrequency();
	Int64 time64 = getPerformanceCounter();

	s_timeSinceLastUpdateInSecs = ((double)(time64 - s_lastUpdateTime64) / (double)(freq64));

#ifdef EXTENDED_STATS
		static FILE *pListFile = nullptr;
		static Int64 lastFrameTime=0;
		static samples = 0;
		if (pListFile == nullptr) {
			pListFile = fopen("FrameRateLog.txt", "w");
		}
		samples++;
		if (pListFile && lastFrameTime && samples<100) {
			float timeSinceLastFrame = (float)((double)(time64-lastFrameTime) / (double)(freq64));
			fprintf(pListFile, "%d ", (int)(1/timeSinceLastFrame));
		}
		lastFrameTime = time64;
#endif

	// we update stats on a delay
	const Real UPDATE_RATE_SECS = 2.0;
	if( s_timeSinceLastUpdateInSecs >= UPDATE_RATE_SECS || TheGlobalData->m_constantDebugUpdate )
	{
		UnicodeString unibuffer, unibuffer2;
		UnicodeString fpsString;

		// setup texture stats
		Debug_Statistics::Record_Texture_Mode(Debug_Statistics::RECORD_TEXTURE_SIMPLE/*RECORD_TEXTURE_NONE*/);

		// frames per second
		double fps = (Real)s_framesRenderedSinceLastUpdate / s_timeSinceLastUpdateInSecs;
		double drawsPerFrame = Debug_Statistics::Get_Draw_Calls(); //(Real)s_drawCallsSinceLastUpdate / (Real)s_framesRenderedSinceLastUpdate;
		double sortPolysPerFrame = Debug_Statistics::Get_Sorting_Polygons();  //(Real)s_sortedPolysSinceLastUpdate / (Real)s_framesRenderedSinceLastUpdate;
		double skinDrawsPerFrame = Debug_Statistics::Get_DX8_Skin_Renders();

		if (fps<0.1) fps = 0.1;

		double ms = 1000.0f/fps;


#if defined(RTS_DEBUG)
		double cumuTime = ((double)(time64 - m_timerAtCumuFPSStart) / (double)(freq64));
		if (cumuTime < 0.0) cumuTime = 0.0;
		Int numFrames = (Int)TheGameLogic->getFrame() - (Int)START_CUMU_FRAME;
		double cumuFPS = (numFrames > 0 && cumuTime > 0.0) ? (numFrames / cumuTime) : 0.0;
		double skinPolysPerFrame = Debug_Statistics::Get_DX8_Skin_Polygons();

		Int LOD = TheGlobalData->m_terrainLOD;
		//unibuffer.format( L"FPS: %.2f, %.2fms mapLOD=%d [cumu FPS=%.2f] draws: %.2f sort: %.2f", fps, ms, LOD, cumuFPS, drawsPerFrame,sortPolysPerFrame);
		if (TheGlobalData->m_useFpsLimit)
				unibuffer.format( L"%.2f/%d FPS, ", fps, TheFramePacer->getFramesPerSecondLimit());
		else
				unibuffer.format( L"%.2f FPS, ", fps);

		unibuffer2.format( L"%.2fms [cumuFPS=%.2f] draws: %d skins: %d sortP: %d skinP: %d LOD %d", ms, cumuFPS, (Int)drawsPerFrame,(Int)skinDrawsPerFrame,(Int)sortPolysPerFrame, (Int)skinPolysPerFrame, LOD);
		unibuffer.concat(unibuffer2);
#else
		//Int LOD = TheGlobalData->m_terrainLOD;
		//unibuffer.format( L"FPS: %.2f, %.2fms mapLOD=%d draws: %.2f sort %.2f", fps, ms, LOD, drawsPerFrame,sortPolysPerFrame);
		unibuffer.format( L"FPS: %.2f, %.2fms draws: %.2f skins: %.2f sort %.2f", fps, ms, drawsPerFrame,skinDrawsPerFrame,sortPolysPerFrame);
		if (TheGlobalData->m_useFpsLimit)
		{
			unibuffer2.format(L", FPSLock %d",TheGlobalData->m_framesPerSecondLimit);
			unibuffer.concat(unibuffer2);
		}
#endif

		fpsString.format( L"FPS: %.2f", fps);
		m_benchmarkDisplayString->setText( fpsString );

		Int polyPerFrame = Debug_Statistics::Get_DX8_Polygons();

#ifdef EXTENDED_STATS
		static float gameOverheadMS = 0.0f;
		static float consoleMS = 0.0f;
		static float threeDOverheadMS = 0.0f;
		static float terrainMS = 0.0f;
		static float objectMS = 0.0f;
		static float overlapMS = 0.0f;
		static int  extendedStats = 0;
		const int SHOW_STATS_TIME=12; // show extended stats for 5 cycles == 10 seconds.
		static enum {disabled, sync, gameOverhead, console, threeDOverhead, terrain, objects, overlap, normal} statMode = disabled;

		if (statMode == sync) {
			extendedStats = SHOW_STATS_TIME;
			statMode = gameOverhead;
		} else if (statMode == gameOverhead) {
			gameOverheadMS = ms;
			statMode = console;
			DX8Wrapper::stats.m_disableTerrain = true;
			DX8Wrapper::stats.m_disableOverhead = true;
			DX8Wrapper::stats.m_disableWater = true;
			DX8Wrapper::stats.m_disableObjects = true;
			DX8Wrapper::stats.m_disableConsole = false;
			DX8Wrapper::stats.m_debugLinesToShow = 1;
		} else if (statMode == console) {
			consoleMS = ms;
			statMode = threeDOverhead;
			DX8Wrapper::stats.m_disableTerrain = true;
			DX8Wrapper::stats.m_disableOverhead = true;
			DX8Wrapper::stats.m_disableWater = true;
			DX8Wrapper::stats.m_disableObjects = true;
			DX8Wrapper::stats.m_disableConsole = true;
			DX8Wrapper::stats.m_debugLinesToShow = 1;
		} else if (statMode == threeDOverhead) {
			threeDOverheadMS = ms;
			statMode = terrain;
			DX8Wrapper::stats.m_disableTerrain = false;
			DX8Wrapper::stats.m_disableOverhead = true;
			DX8Wrapper::stats.m_disableWater = true;
			DX8Wrapper::stats.m_disableObjects = true;
			DX8Wrapper::stats.m_disableConsole = true;
			DX8Wrapper::stats.m_debugLinesToShow = 1;
		} else if (statMode == terrain) {
			terrainMS = ms;
			statMode = objects;
			DX8Wrapper::stats.m_disableOverhead = true;
			DX8Wrapper::stats.m_disableTerrain = true;
			DX8Wrapper::stats.m_disableWater = true;
			DX8Wrapper::stats.m_disableObjects = false;
			DX8Wrapper::stats.m_disableConsole = true;
			DX8Wrapper::stats.m_debugLinesToShow = 1;
		} else if (statMode == objects) {
			objectMS = ms;
			statMode = overlap;
			DX8Wrapper::stats.m_disableOverhead = false;
			DX8Wrapper::stats.m_disableTerrain = false;
			DX8Wrapper::stats.m_disableWater = false;
			DX8Wrapper::stats.m_disableObjects = false;
			DX8Wrapper::stats.m_disableConsole = true;
			DX8Wrapper::stats.m_sleepTime = (int)(terrainMS);
			DX8Wrapper::stats.m_debugLinesToShow = 1;
		} else if (statMode == overlap) {
			overlapMS = ms;
			statMode = normal;
			DX8Wrapper::stats.m_disableOverhead = false;
			DX8Wrapper::stats.m_disableTerrain = false;
			DX8Wrapper::stats.m_disableWater = false;
			DX8Wrapper::stats.m_disableObjects = false;
			DX8Wrapper::stats.m_disableConsole = true;
			DX8Wrapper::stats.m_sleepTime = 0;
			DX8Wrapper::stats.m_debugLinesToShow = 1;
		} else if (statMode == normal) {
			overlapMS = (ms + ((int)terrainMS) - overlapMS );
			statMode = disabled;
			extendedStats = SHOW_STATS_TIME;

			// Done collecting stats. Re-enable stuff
			DX8Wrapper::stats.m_disableConsole = false;
			DX8Wrapper::stats.m_debugLinesToShow = -1;
		} else if (!DX8Wrapper::stats.m_showingStats) {
			// start collecting extended info.
			DX8Wrapper::stats.m_showingStats = true;
			DX8Wrapper::stats.m_disableOverhead = false;
			DX8Wrapper::stats.m_disableTerrain = true;
			DX8Wrapper::stats.m_disableWater = true;
			DX8Wrapper::stats.m_disableObjects = true;
			DX8Wrapper::stats.m_disableConsole = true;
			DX8Wrapper::stats.m_debugLinesToShow = 1;
			statMode = sync;
			gameOverheadMS = 0.0f;
			threeDOverheadMS = 0.0f;
			terrainMS = 0.0f;
			objectMS = 0.0f;
		}
		if (statMode != disabled) {
			unibuffer.format(L"FPS: %.2f, %.2fms - Collecting extended stats.", fps, ms);
		} else if (extendedStats>0) {
			extendedStats--;
			unibuffer.format( L"FPS: %.2f, %.2fms - OH %.2fms, Console %.2fms, 3D OH %.2fms, Terrain %.2fms, Obs %.2fms, CPU %.2fms",
				fps, ms, gameOverheadMS, consoleMS, threeDOverheadMS, terrainMS, objectMS, overlapMS);
			if (extendedStats==SHOW_STATS_TIME-2) {
				char bufferA[ 256 ];
				sprintf( bufferA, "FPS: %.2f, %.2fms - OH %.2fms, Console %.2fms, 3D OH %.2fms, Terrain %.2fms, Obs %.2fms, CPU %.2fms\n",
					fps, ms, gameOverheadMS, consoleMS, threeDOverheadMS, terrainMS, objectMS, overlapMS);
				::OutputDebugString(bufferA);
				if (pListFile) {
					fprintf(pListFile, "\n%s", bufferA);
				}
				sprintf( bufferA, "Polygons: per frame %d, per second %d\n", polyPerFrame,
						(Int)(polyPerFrame*fps));
				::OutputDebugString(bufferA);
				if (pListFile) {
					fprintf(pListFile, "%s", bufferA);
					fflush(pListFile);
				}
			}
		}
 		if (pListFile) {
			fprintf(pListFile, "\nFPS: %.2f, %.2fms\n", fps, ms);
			fflush(pListFile);
		}
		if (pListFile) {
			samples = 0;
			if (statMode != disabled) {
				fprintf(pListFile, "Stat%d-", statMode);
			}
		}

#endif
		// check for debug D3D
		Bool debugD3D=false;
		RegistryClass registry ("Software\\Microsoft\\Direct3d");
		if (registry.Is_Valid ()) {
			if (registry.Get_Int ("LoadDebugRuntime", 0) == 1) {
				debugD3D = true;
			}
		}
		if (debugD3D) {
			unibuffer.concat(L", DEBUG D3D");
		}
#ifdef RTS_DEBUG
		unibuffer.concat(L", DEBUG app");
#endif

		m_displayStrings[FPS]->setText( unibuffer );

		// Actual GameLogic frame number
		unibuffer.format(L"Frame: %d", TheGameLogic->getFrame());
		m_displayStrings[Frame]->setText( unibuffer );

		// polygons this frame
		unibuffer.format( L"Polygons: per frame %d, per second %d", polyPerFrame,
				(Int)(polyPerFrame*fps));
		m_displayStrings[Polygons]->setText( unibuffer );

		// vertices this frame
		unibuffer.format( L"Vertices: %d", Debug_Statistics::Get_DX8_Vertices() );
		m_displayStrings[Vertices]->setText( unibuffer );

		//
		// I'm adjusting the texture memory usage counter by subtracting
		// out the terrain alpha texture (since it's really == terrain texture).
		//
		unibuffer.format( L"Video RAM: %d", Debug_Statistics::Get_Record_Texture_Size() - 1376256 );
		m_displayStrings[VideoRam]->setText( unibuffer );

		s_lastUpdateTime64 = time64;
		s_timeSinceLastUpdateInSecs = 0.0f;
		s_framesRenderedSinceLastUpdate = 0;
		s_drawCallsSinceLastUpdate = 0;
		s_sortedPolysSinceLastUpdate = 0;

		// terrain stats
		unibuffer.format( L"3-Way Blends: %d/%d, Shoreline Blends: %d/%d", TheTerrainRenderObject->getNumExtraBlendTiles(TRUE),
			TheTerrainRenderObject->getNumExtraBlendTiles(FALSE),
			TheTerrainRenderObject->getNumShoreLineTiles(TRUE),
			TheTerrainRenderObject->getNumShoreLineTiles(FALSE));
		m_displayStrings[TerrainStats]->setText( unibuffer );

		// misc debug info
		Coord3D camPos = TheTacticalView->getPosition();
		Real zoom = TheTacticalView->getZoom();
		Real pitch = TheTacticalView->getPitch();
		Real FXPitch = TheTacticalView->getFXPitch();
		Real angle = TheTacticalView->getAngle();
		Real FOV = TheTacticalView->getFieldOfView();
		Real terrainHeight = TheTacticalView->getTerrainHeightAtPivot();
		Real actualHeightAboveGround = TheTacticalView->getCurrentHeightAboveGround();

		unibuffer.format(
			L"Camera zoom: %.3f, pitch: %.2f, FXpitch: %.2f, yaw: %.2f, pos: (%.2f, %.2f, %.2f), FOV: %.2f\n"
			L"Height above ground: %.2f, Terrain height at camera pivot: %.2f",
			zoom,
			RAD_TO_DEGF(pitch),
			RAD_TO_DEGF(FXPitch),
			RAD_TO_DEGF(angle),
			camPos.x, camPos.y, camPos.z,
			RAD_TO_DEGF(FOV),
			actualHeightAboveGround, terrainHeight );

		m_displayStrings[DebugInfo]->setText( unibuffer );

		// display the keyboard modifier and mouse states.
		unibuffer.format( L"States: " );
		if( TheKeyboard->isShift() )
		{
			unibuffer.concat( L"Shift(" );
			if( TheKeyboard->getModifierFlags() & KEY_STATE_LSHIFT )
			{
				unibuffer.concat( L"L" );
			}
			if( TheKeyboard->getModifierFlags() & KEY_STATE_RSHIFT )
			{
				unibuffer.concat( L"R" );
			}
			unibuffer.concat( L") " );
		}
		if( TheKeyboard->isCtrl() )
		{
			unibuffer.concat( L"Ctrl(" );
			if( TheKeyboard->getModifierFlags() & KEY_STATE_LCONTROL )
			{
				unibuffer.concat( L"L" );
			}
			if( TheKeyboard->getModifierFlags() & KEY_STATE_RCONTROL )
			{
				unibuffer.concat( L"R" );
			}
			unibuffer.concat( L") " );
		}
		if( TheKeyboard->isAlt() )
		{
			unibuffer.concat( L"Alt(" );
			if( TheKeyboard->getModifierFlags() & KEY_STATE_LALT )
			{
				unibuffer.concat( L"L" );
			}
			if( TheKeyboard->getModifierFlags() & KEY_STATE_RALT )
			{
				unibuffer.concat( L"R" );
			}
			unibuffer.concat( L") " );
		}

		const MouseIO *mouseStatus = TheMouse->getMouseStatus();

		if( mouseStatus->leftState )
		{
			unibuffer.concat( L"LMB " );
		}
		if( mouseStatus->middleState )
		{
			unibuffer.concat( L"MMB " );
		}
		if( mouseStatus->rightState )
		{
			unibuffer.concat( L"RMB " );
		}

		Object *object = nullptr;
#if defined(RTS_DEBUG)	//debug hack to view object under mouse stats
		Drawable *draw = 	TheTacticalView->pickDrawable(&TheMousePos, FALSE, (PickType)0xffffffff );
#else
		Drawable *draw = TheGameClient->findDrawableByID( TheInGameUI->getMousedOverDrawableID() );
#endif
		if( draw  )
			object = draw->getObject();
		if( object )
		{
			unibuffer2.format( L"Moused over object: %S (%d) ", object->getTemplate()->getName().str(), object->getID() );
			unibuffer.concat( unibuffer2 );
		}
		else
		{
			unibuffer.concat( L"Moused over object: TERRAIN " );
		}

		m_displayStrings[ KEY_MOUSE_STATES ]->setText( unibuffer );

		//display the x and y mouse coordinates
		const MouseIO *mouseIO = TheMouse->getMouseStatus();
		Coord3D worldPos;
		TheTacticalView->screenToTerrain(&mouseIO->pos, &worldPos);
		unibuffer.format( L"Mouse position: screen: (%d, %d), world: (%g, %g, %g)", mouseIO->pos.x, mouseIO->pos.y,
			worldPos.x, worldPos.y, worldPos.z);
		m_displayStrings[MousePosition]->setText( unibuffer );

		//display the number of particles in the world and being displayed on screen
		Int totalParticles = TheParticleSystemManager->getParticleCount();
		Int onScreenParticleCount = TheParticleSystemManager->getOnScreenParticleCount();
		unibuffer.format( L"Particles: %d in world, %d being displayed", totalParticles, onScreenParticleCount );
		m_displayStrings[Particles]->setText( unibuffer );

		//display the number of objects in the world
		UnsignedInt objCount = TheGameLogic->getObjectCount();
		UnsignedInt objScreenCount = TheGameClient->getRenderedObjectCount();

		unibuffer.format(L"Objects: %d in world, %d being displayed", objCount, objScreenCount );
		m_displayStrings[Objects]->setText( unibuffer );

		// Network incoming bandwidth stats
		if (TheNetwork != nullptr) {
			unibuffer.format(L"IN: %.2f bytes/sec, %.2f packets/sec",
				TheNetwork->getIncomingBytesPerSecond(), TheNetwork->getIncomingPacketsPerSecond());
			m_displayStrings[NetIncoming]->setText( unibuffer );

			// Network outgoing bandwidth stats
			unibuffer.format(L"OUT: %.2f bytes/sec, %.2f packets/sec",
				TheNetwork->getOutgoingBytesPerSecond(), TheNetwork->getOutgoingPacketsPerSecond());
			m_displayStrings[NetOutgoing]->setText( unibuffer );

			// Network performance stats
			unibuffer.format(L"Run Ahead: %d, Net FPS: %d, Packet arrival cushion: %d",
				TheNetwork->getRunAhead(), TheNetwork->getFrameRate(), TheNetwork->getPacketArrivalCushion());
			m_displayStrings[NetStats]->setText( unibuffer );

			// Client frame rate averages for all players in the game.  This only works right for the packet router.
			unibuffer.clear();
			Int numPlayers = TheNetwork->getNumPlayers();
			for (Int i = 0; i < numPlayers; ++i) {
				UnicodeString tempstr;
				tempstr.format(L"%s: %d ", TheNetwork->getPlayerName(i).str(), TheNetwork->getSlotAverageFPS(i));
				unibuffer.concat(tempstr);
			}
			m_displayStrings[NetFPSAverages]->setText( unibuffer );
		} else {
//			unibuffer.format(L"IN: 0.0 bytes/sec, 0.0 packets/sec");
//			m_displayStrings[NetIncoming]->setText( unibuffer );

			// Network outgoing bandwidth stats
//			unibuffer.format(L"OUT: 0.0 bytes/sec, 0.0 packets/sec");
//			m_displayStrings[NetOutgoing]->setText( unibuffer );
			unibuffer.clear();
//			unibuffer.format(L"Network not present");
			m_displayStrings[NetOutgoing]->setText(unibuffer);
			m_displayStrings[NetIncoming]->setText(unibuffer);
			m_displayStrings[NetStats]->setText(unibuffer);
			m_displayStrings[NetFPSAverages]->setText( unibuffer );
		}

		// selected object info stats
		unibuffer.format( L"Select Info: '%d' drawables selected", TheInGameUI->getSelectCount() );



		//Sorry, guys. I need a special kluge here to get constantdebug results for angry mob.
		//Do no be cross with me.
		//if there is not exactly one drawable selected it will report on the moused-over drawable
		if (TheInGameUI->getSelectCount() == 1)
			draw = TheInGameUI->getFirstSelectedDrawable();


		if( draw )
		{
			Object *obj = draw->getObject();
			AsciiString objectName;

			objectName.set( "No-Name" );
			if( obj && obj->getName().isEmpty() == FALSE )
				objectName = obj->getName();

			unibuffer.format( L"Select Info: '%S'(%S) at (%.3f,%.3f,%.3f)",
												draw->getTemplate()->getName().str(),
												objectName.str(),
												draw->getPosition()->x,
												draw->getPosition()->y,
												draw->getPosition()->z
											);

			const PhysicsBehavior *physics = obj->getPhysics();
			PhysicsTurningType turnType = physics ? physics->getTurning() : TURN_NONE;

			const DrawableLocoInfo *locoInfo = draw->getLocoInfo();
			if( locoInfo )
			{
				unibuffer2.format( L"\nPhysics Info -- Turn: %d, Pitch(accel): %.3f(%.3f), Roll(accel): %.3f(%.3f)",
													 turnType,
													 locoInfo->m_accelerationPitch, locoInfo->m_accelerationPitchRate,
													 locoInfo->m_accelerationRoll, locoInfo->m_accelerationRollRate );
				unibuffer.concat( unibuffer2 );
			}






			// (gth) compute some stats about the rendering cost of this drawable
#if defined(RTS_DEBUG)
			RenderCost rcost;
			for (DrawModule** dm = draw->getDrawModules(); *dm; ++dm)
			{
				(*dm)->getRenderCost(rcost);
			}
			if (rcost.getDrawCallCount() > 0)
			{
				unibuffer2.format( L"\ndraw calls: %d(+%d) sort meshes: %d skins: %d  bones: %d",rcost.getDrawCallCount(),rcost.getShadowDrawCount(),rcost.getSortedMeshCount(),rcost.getSkinMeshCount(),rcost.getBoneCount());
				unibuffer.concat( unibuffer2 );
			}
#endif

			unibuffer.concat( L"\nModelStates: " );
			ModelConditionFlags mcFlags = draw->getModelConditionFlags();
			const int numEntriesPerLine = 4;
			int lineCount = 0;

			for( int i = 0; i < MODELCONDITION_COUNT; i++ )
			{
				if( mcFlags.test( i ) )
				{
					unibuffer2.format( L"%S ", ModelConditionFlags::getBitNames()[ i ] );
					unibuffer.concat( unibuffer2 );
					lineCount++;
					if( lineCount == numEntriesPerLine )
					{
						lineCount = 0;
						unibuffer.concat( L"\n" );
					}
				}
			}

			//Render ALL modelcondition statii

		}
		m_displayStrings[ SelectedInfo ]->setText( unibuffer );

	}

}

// W3DDisplay::drawDebugStats =================================================
/** Draw debug statistics */
//=============================================================================
void W3DDisplay::drawDebugStats()
{
	Int	x = 3;
	Int	y = 30;
	Color textColor = GameMakeColor( 255, 255, 255, 255 );
	Color dropColor = GameMakeColor( 0, 0, 0, 255 );

	int linesOfStrings = DisplayStringCount;
#ifdef EXTENDED_STATS
	if (DX8Wrapper::stats.m_debugLinesToShow > -1)
	{
		linesOfStrings = DX8Wrapper::stats.m_debugLinesToShow;
	}

#endif


	Int w, h;
	for (int i = 0; i < linesOfStrings; i++)
	{
		m_displayStrings[i]->draw( x, y, textColor, dropColor );
		m_displayStrings[i]->getSize(&w, &h);
		y += h;
	}

}

// W3DDisplay::drawFPSStats =================================================
/** Draw the FPS on the screen */
//=============================================================================
void W3DDisplay::drawFPSStats()
{
	Int	x = 3;
	Int	y = 20;
	Color textColor = GameMakeColor( 255, 255, 255, 255 );
	Color dropColor = GameMakeColor( 0, 0, 0, 255 );

	int linesOfStrings = 1;

	for (int i = 0; i < linesOfStrings; i++)
	{
		m_benchmarkDisplayString->draw( x, y, textColor, dropColor );
	}
}


//=============================================================================
void StatDebugDisplay( DebugDisplayInterface *, void *, FILE *fp )
{
	DEBUG_CRASH(("This should never be called directly, but is just a placeholder for drawDebugStats()"));
}

// W3DDisplay::drawCurrentDebugDisplay =================================================
/** Draw current debug display */
//=============================================================================
void W3DDisplay::drawCurrentDebugDisplay()
{
	if (m_debugDisplayCallback == StatDebugDisplay)
	{
		drawDebugStats();
	}
	else
	{
		if ( m_debugDisplay && m_debugDisplayCallback )
		{
			m_debugDisplay->reset();
			m_debugDisplayCallback( m_debugDisplay, m_debugDisplayUserData, nullptr );
		}
	}
}

// W3DDisplay::calculateTerrainLOD =================================================
/** Calculates an adequately speedy terrain Level Of Detail. */
//=============================================================================
void W3DDisplay::calculateTerrainLOD()
{
	const Int NUM_SAMPLES=20;
	const Int NUM_TO_DISCARD=5;

	Int64 freq64 = getPerformanceCounterFrequency();

	char buf[_MAX_PATH];
	float frameTime = 0;
	float maxTimeLimit = TheGlobalData->m_terrainLODTargetTimeMS/1000.0f;
	TerrainLOD goodLOD = TERRAIN_LOD_MIN;
	TerrainLOD curLOD = TERRAIN_LOD_AUTOMATIC;
	Int count = 0;
#ifdef RTS_DEBUG
	// just go to TERRAIN_LOD_NO_WATER, mirror off.
	TheWritableGlobalData->m_terrainLOD = TERRAIN_LOD_NO_WATER;
	m_3DScene->drawTerrainOnly(false);
	TheTerrainRenderObject->adjustTerrainLOD(0);
	return;
#endif
	do {
		Int i;
		float timeForFrame=0;
		frameTime = 0;
		switch(curLOD) {
			default: curLOD = TERRAIN_LOD_DISABLE; break;
			case TERRAIN_LOD_AUTOMATIC: curLOD = TERRAIN_LOD_MAX; break;
			case TERRAIN_LOD_MAX: curLOD = TERRAIN_LOD_NO_WATER; break;
			case TERRAIN_LOD_HALF_CLOUDS: curLOD = TERRAIN_LOD_DISABLE; break;
			case TERRAIN_LOD_NO_WATER: curLOD = TERRAIN_LOD_HALF_CLOUDS; break;
		}
		if (curLOD == TERRAIN_LOD_DISABLE) {
			break;
		}
		TheWritableGlobalData->m_terrainLOD = curLOD;
		m_3DScene->drawTerrainOnly(true);
		TheTerrainRenderObject->adjustTerrainLOD(0);
		for (i=0; i<NUM_SAMPLES; i++) {
			Int64 startTime64 = getPerformanceCounter();
			// start render block
			updateViews();
			if (WW3D::Begin_Render( true, true, Vector3( 0.0f, 0.0f, 0.0f ) ) == WW3D_ERROR_OK)
			{	// draw all views of the world
				drawViews();
				// render is all done!
				WW3D::End_Render();
			}
			Int64 time64 = getPerformanceCounter();
			timeForFrame = (float)((double)(time64-startTime64) / (double)(freq64));
			sprintf(buf, "%.2fms ", timeForFrame*1000.0f);
			::OutputDebugString(buf);
			if (i>=NUM_TO_DISCARD) {
				frameTime += timeForFrame;
				if (i>NUM_TO_DISCARD+1 &&
					(timeForFrame / ((i+1)-NUM_TO_DISCARD)) > 2*maxTimeLimit) {
					i++;
					break;
				}
			}
		}
		frameTime /= ((i)-NUM_TO_DISCARD);
		count++;
		sprintf(buf, "\n LOD %d, time %.2fms\n", curLOD, frameTime*1000.0f);
		::OutputDebugString(buf);
		if (frameTime<maxTimeLimit && goodLOD<curLOD) {
			goodLOD = curLOD;
		}
		if (frameTime < maxTimeLimit) break;
	} while (count<10);

	TheWritableGlobalData->m_terrainLOD = goodLOD;
	m_3DScene->drawTerrainOnly(false);
	TheTerrainRenderObject->adjustTerrainLOD(0);
#ifdef RTS_DEBUG
	DEBUG_ASSERTCRASH(count<10, ("calculateTerrainLOD") );
#endif

}


Real W3DDisplay::getAverageFPS()
{
	return m_averageFPS;
}

Real W3DDisplay::getCurrentFPS()
{
	return m_currentFPS;
}

Int W3DDisplay::getLastFrameDrawCalls()
{
	return Debug_Statistics::Get_Draw_Calls();
}

//=============================================================================
void W3DDisplay::step()
{
	stepViews();
}

//DECLARE_PERF_TIMER(BigAssRenderLoop)

// W3DDisplay::draw ===========================================================
/** Draw the entire W3D Display */
//=============================================================================
//DECLARE_PERF_TIMER(W3DDisplay_draw)
void W3DDisplay::draw()
{
	//USE_PERF_TIMER(W3DDisplay_draw)

	// GeneralsX @feature xxorza 15/04/2026 Process deferred window resize for pillarbox
	DX8Wrapper::Pillarbox_Process_Resize();

	extern HWND ApplicationHWnd;
	if (ApplicationHWnd && ::IsIconic(ApplicationHWnd)) {
		return;
	}

	if (TheGlobalData->m_headless)
		return;

	updateAverageFPS();
	if (TheGlobalData->m_enableDynamicLOD && TheGameLogic->getShowDynamicLOD())
	{
		DynamicGameLODLevel lod=TheGameLODManager->findDynamicLODLevel(m_averageFPS);
		TheGameLODManager->setDynamicLODLevel(lod);
	}
	else
	{	//if dynamic LOD is turned off, force highest LOD
		TheGameLODManager->setDynamicLODLevel(DYNAMIC_GAME_LOD_VERY_HIGH);
	}

	if (TheGlobalData->m_terrainLOD == TERRAIN_LOD_AUTOMATIC && TheTerrainRenderObject)
	{
		calculateTerrainLOD();
	}
#ifdef EXTENDED_STATS
AGAIN:
#endif

#ifdef DUMP_PERF_STATS
	if( TheGlobalData->m_dumpPerformanceStatistics )
	{
		TheStatDump.dumpStats( FALSE, TRUE );
		TheWritableGlobalData->m_dumpPerformanceStatistics = FALSE;
	}
  //The <= GAME_REPLAY essentially means, GAME_SINGLE_PLAYER || GAME_LAN || GAME_SKIRMISH || GAME_REPLAY
  else if ( TheGlobalData->m_dumpStatsAtInterval && TheGameLogic->getGameMode() <= GAME_REPLAY )
  {
    Int interval = TheGlobalData->m_statsInterval;
    if ( TheGameLogic->getFrame() > 0 && (TheGameLogic->getFrame() % interval) == 0 )
    {
  	  TheStatDump.dumpStats( TRUE, TRUE );
    	TheInGameUI->message( L"-stats is running, at interval: %d.", TheGlobalData->m_statsInterval );
    }
  }
#endif

	// compute debug statistics for display later
	if ( m_debugDisplayCallback == StatDebugDisplay
#if defined(RTS_DEBUG)
				|| TheGlobalData->m_benchmarkTimer > 0
#endif
			)
	{
		gatherDebugStats();
	}
#ifdef EXTENDED_STATS
	else
	{
		DX8Wrapper::stats.m_showingStats = false;
	}
#endif

#ifdef SAMPLE_DYNAMIC_LIGHT
	Vector3 loc;
	loc = theDynamicLight->Get_Position();
	loc.X += theLightXOffset;
	if(loc.X>128) theLightXOffset = -theLightXOffset;
	if(loc.X<0) theLightXOffset = -theLightXOffset;
	loc.Y += theLightYOffset;
	if(loc.Y>128) theLightYOffset = -theLightYOffset;
	if(loc.Y<0) theLightYOffset = -theLightYOffset;
	theDynamicLight->Set_Position(loc);
#endif


	/// @todo Make more explicit drawing layers(ground, ground UI, objects, object UI, overlay UI)

	///@todo: Ask Vegas why the LOD optimizer hangs particle system.
 	//
  	// Predictive LOD optimizer optimizes the mesh LOD levels to match
  	// the given polygon budget
  	//
	//PredictiveLODOptimizerClass::Optimize_LODs( 5000 );

	Bool freezeTime = TheFramePacer->isTimeFrozen() || TheFramePacer->isGameHalted();

	/// @todo: I'm assuming the first view is our main 3D view.
	W3DView *primaryW3DView=(W3DView *)getFirstView();

	if (!freezeTime && TheScriptEngine->isTimeFast())
	{
		primaryW3DView->updateCameraMovements();  // Update camera motion effects.
		return;
	}

	Debug_Statistics::Begin_Statistics();	//reset all counters (polygons, vertices, etc) before drawing

	//update state of all the terrain tracks (fade, remove, etc.)
	/// @todo: Is there a better place to put per-frame updates like this?

	if(TheGlobalData->m_loadScreenRender != TRUE)
	{

		if (TheTerrainTracksRenderObjClassSystem)
			TheTerrainTracksRenderObjClassSystem->update();

		//Shroud data is needed to render all other views, so handle this first.
		if (TheTerrainRenderObject)
		{
			//update the shroud surface here since it may be needed by reflections
			if (TheTerrainRenderObject->getMap())	//make sure a valid map is loaded into terrain.
			{
				if (TheTerrainRenderObject->getShroud())
				{
					TheTerrainRenderObject->getShroud()->render(primaryW3DView->get3DCamera());
				}
			}
		}
	}

	WW3D::Update_Logic_Frame_Time(TheFramePacer->getLogicTimeStepMilliseconds());

	// TheSuperHackers @info This binds the WW3D update to the logic update.
	WW3D::Sync(TheGameLogic->hasUpdated());

	static Int now;
	now=timeGetTime();

	// GeneralsX @bugfix 26/07/2026 The scripted time multiplier no longer skips render frames.
	//
	// This block used to draw 1 of every N frames while a script had asked for an N-times time
	// multiplier, because SET_TIME_MULTIPLIER was implemented by uncapping the render rate and
	// leaning on "one logic step per rendered frame" to speed the simulation up. The simulation now
	// runs a fixed step accumulator, so the multiplier is applied to the logic cadence instead (see
	// FramePacer::getTimeMultiplier) and skipping frames here would only make a fast-forwarded
	// cutscene stutter at a third of the display rate while running at normal speed.

	// GeneralsX @bugfix 26/07/2026 Pace the freeze-time render loop.
	//
	// While a script freezes time for a camera move, this loop renders and advances the camera
	// without returning to the main engine loop, so nothing called TheFramePacer->update() and the
	// loop ran as fast as the machine could go. The camera advances by a fixed slice of time per
	// iteration, so on modern hardware a 3-second scripted camera move was over in a fraction of a
	// second -- cinematic camera work played back at many times its authored speed. Waiting on the
	// frame limiter here makes each iteration one real rendered frame, which is what the camera code
	// assumes, and also means the loop cannot spin the CPU flat out.
	const Bool freezeLoopWasActive = freezeTime;
	Bool freezeLoopFirstIteration = TRUE;

	do {
		if (freezeLoopWasActive)
		{
			if (freezeLoopFirstIteration)
			{
				freezeLoopFirstIteration = FALSE;
			}
			else
			{
				TheFramePacer->update();
			}
		}


		// update all views of the world - recomputes data which will affect drawing
		if (DX8Wrapper::_Get_D3D_Device8() && (DX8Wrapper::_Get_D3D_Device8()->TestCooperativeLevel()) == D3D_OK)
		{	//Checking if we have the device before updating views because the heightmap crashes otherwise while
			//trying to refresh the visible terrain geometry.
//			if(TheGlobalData->m_loadScreenRender != TRUE)
				updateViews();
     		TheParticleSystemManager->update();//LORENZEN AND WILCZYNSKI MOVED THIS FROM ITS NATIVE POSITION, ABOVE
                                           //FOR THE PURPOSE OF LETTING THE PARTICLE SYSTEM LOOK UP THE RENDER OBJECT"S
                                           //TRANSFORM MATRIX, WHILE IT IS STILL VALID (HAVING DONE ITS CLIENT TRANSFORMS
                                           //BUT NOT YET RESETTING TOT HE LOGICAL TRANSFORM)
                                           //THE RESULT IS THAT PARTICLESYSTEMS LINKED TO BONES IN DRAWABLES.OBJECTS
                                           //MOVE WITH THE CLIENT TRANSFORMS, NOW.
                                           //REVOLUTIONARY!
                                           //-LORENZEN


			if (TheWaterRenderObj && TheGlobalData->m_waterType == 2)
				TheWaterRenderObj->updateRenderTargetTextures(primaryW3DView->get3DCamera());	//do a render into each texture

			//Can't render into textures while rendering to screen so these textures need to be updated
			//before we enter main rendering loop.
			if (TheW3DProjectedShadowManager)
				TheW3DProjectedShadowManager->updateRenderTargetTextures();
		}

		// Switch to offscreen RT AFTER pre-render (shadows/water) completes, BEFORE main render.
		DX8Wrapper::Pillarbox_Begin();

		Debug_Statistics::End_Statistics();	//record number of polygons rendered in RenderTargetTextures.

		//Store number of polygons rendered in renderTargetTextures.
		Int numRenderTargetPolygons=Debug_Statistics::Get_DX8_Polygons();
		Int numRenderTargetVertices=Debug_Statistics::Get_DX8_Vertices();

		// start render block
		#if defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
    if ( (TheGameLogic->getFrame() % 30 == 1) || ( ! ( !TheGameLogic->isGamePaused() && TheGlobalData->m_TiVOFastMode) ) )
		#else
	    if ( (TheGameLogic->getFrame() % 30 == 1) || ( ! (!TheGameLogic->isGamePaused() && TheGlobalData->m_TiVOFastMode && TheGameLogic->isInReplayGame())) )
    #endif
		{
			//USE_PERF_TIMER(BigAssRenderLoop)
			static Bool couldRender = true;
			if ((TheGlobalData->m_breakTheMovie == FALSE) && (TheGlobalData->m_disableRender == false) && WW3D::Begin_Render( true, true, Vector3( 0.0f, 0.0f, 0.0f ), TheWaterTransparency->m_minWaterOpacity ) == WW3D_ERROR_OK)
			{

				if(TheGlobalData->m_loadScreenRender == TRUE)
				{
					TheInGameUI->draw();
					if( TheMouse )
						TheMouse->draw();	//keep applying the current cursor style so it remains hidden if needed.
					WW3D::End_Render();
					continue;
				}
				couldRender = true;
				// add the number of verts/polygons drawn before the main scene
				if (numRenderTargetPolygons || numRenderTargetVertices)
					Debug_Statistics::Record_DX8_Polys_And_Vertices(numRenderTargetPolygons,numRenderTargetVertices,ShaderClass::_PresetOpaqueShader);

				// draw all views of the world
				drawViews();

				// draw the user interface
				TheInGameUI->DRAW();

				// end of video example code

				// draw the mouse
				if( TheMouse )
					TheMouse->DRAW();

				if ( m_videoStream && m_videoBuffer )
				{
					// TheSuperHackers @bugfix Mauller 20/07/2025 scale videos based on screen size so they are shown in their original aspect
					drawScaledVideoBuffer( m_videoBuffer, m_videoStream );
				}
				if( m_copyrightDisplayString )
				{
					Int x, y, dX, dY;
					m_copyrightDisplayString->getSize(&dX, &dY);
					x = (getWidth() / 2) - (dX /2);
					y = getHeight()  - dY - 20 ;
					m_copyrightDisplayString->draw(x, y, GameMakeColor(0,0,0,255), GameMakeColor(0,0,0,0),0,0);
				}
				// render letter box before debug display so debug info isn't hidden
				renderLetterBox(now);

				// display cinematicText over the black
				if( m_cinematicText != AsciiString::TheEmptyString && m_cinematicTextFrames != 0)
				{
					DisplayString *displayString = TheDisplayStringManager->newDisplayString();

					// set word wrap if necessary

					Int wordWrapWidth = TheDisplay->getWidth() - 20;
					displayString->setWordWrap( wordWrapWidth );
					displayString->setWordWrapCentered( TRUE );

					UnicodeString text;
					text.translate( m_cinematicText );
					displayString->setText( text );
					Color color = GameMakeColor( 255, 255, 255, 255 );  // white
					Color backColor = GameMakeColor( 0, 0, 0, 0 );      // black
					displayString->setFont( m_cinematicFont );
					Int height = TheDisplay->getHeight() * .9;

					Int width;
					if( displayString->getWidth() > TheDisplay->getWidth() )
						width = 20;
					else
						width = ( TheDisplay->getWidth() - displayString->getWidth() ) / 2;
					displayString->draw( width, height, color, backColor );

					m_cinematicTextFrames--;
				}

				if ( m_debugDisplayCallback )
				{
					// draw the current debug display
					drawCurrentDebugDisplay();
				}

#if defined(RTS_DEBUG)
				if (TheGlobalData->m_benchmarkTimer > 0)
				{
					drawFPSStats();
				}
#endif


#if defined(RTS_DEBUG)
				if (TheGlobalData->m_debugShowGraphicalFramerate)
				{
					drawFramerateBar();
				}
#endif

#ifdef PERF_TIMERS
				TheGraphDraw->render();
				TheGraphDraw->clear();
#endif

#ifdef PROFILER_ENABLED
				if (m_profilerFrameCapture && !TheGlobalData->m_headless)
				{
					m_profilerFrameCapture->Capture(getWidth(), getHeight());
				}
#endif
				// render is all done!
				WW3D::End_Render();
			}
			else
			{
				if (couldRender)
				{
					couldRender = false;
					DEBUG_LOG(("Could not do WW3D::Begin_Render()!  Are we ALT-Tabbed out?"));
				}
			}
		}

		if (TheScriptEngine->isTimeFrozenDebug() || TheScriptEngine->isTimeFrozenScript() || TheGameLogic->isGamePaused())
		{
			freezeTime = false; // We're frozen for debug or for pause, and need to continue out of the loop.
		}

	} while (freezeTime && !TheTacticalView->isCameraMovementFinished());

#ifdef EXTENDED_STATS
	if (DX8Wrapper::stats.m_disableOverhead) {
		goto AGAIN;
	}
#endif
}

#define LETTER_BOX_FADE_TIME	1000.0f		///1000 ms.

/** Render letter-box border at top/bottom of display
*/
void W3DDisplay::renderLetterBox(UnsignedInt currentTime)
{
		if (m_letterBoxEnabled)
		{	if (m_letterBoxFadeLevel != 1.0f)
			{
				m_letterBoxFadeLevel = (currentTime - m_letterBoxFadeStartTime)/LETTER_BOX_FADE_TIME;
				if (m_letterBoxFadeLevel > 1.0f)
					m_letterBoxFadeLevel = 1.0f;
			}

			UnsignedInt lbcolor = (Int)(m_letterBoxFadeLevel * 255.0f) << 24;

#ifdef SLIDE_LETTERBOX
			Int height = (Int)(getHeight() * 0.12f * m_letterBoxFadeLevel);
			TheTacticalView->setOrigin(0, height);
#else
			drawFillRect( 0, 0, m_width, (m_height-(9.0f/16.0f * m_width))*0.5f, lbcolor );
			drawFillRect( 0, m_height-(m_height-(9.0f/16.0f * m_width))*0.5f, m_width, m_height, lbcolor );
#endif
		}
		else
		{	//letter box is disabled, but may still be fading out
			if (m_letterBoxFadeLevel != 0.0f)
			{
				m_letterBoxFadeLevel = 1.0f - (currentTime - m_letterBoxFadeStartTime)/LETTER_BOX_FADE_TIME;
				if (m_letterBoxFadeLevel < 0.0f)
					m_letterBoxFadeLevel = 0.0f;

				UnsignedInt lbcolor = (Int)(m_letterBoxFadeLevel * 255.0f) << 24;

#ifdef SLIDE_LETTERBOX
				Int height = (Int)(getHeight() * 0.12f * m_letterBoxFadeLevel);
				TheTacticalView->setOrigin(0, height);
#else
				drawFillRect( 0, 0, m_width, (m_height-(9.0f/16.0f * m_width))*0.5f, lbcolor );
				//drawFillRect( 0, m_height-(m_height-(9.0f/16.0f * m_width))*0.5f, m_width, m_height, lbcolor );
#endif
			}
			else
			{	//box has finished fading out
#ifdef SLIDE_LETTERBOX
				TheTacticalView->setOrigin(0, 0);
#else
				m_letterBoxEnabled = FALSE;
#endif
			}
		}
}

Bool W3DDisplay::isLetterBoxFading()
{
	if (m_letterBoxEnabled && m_letterBoxFadeLevel != 1.0f)
		return TRUE;
	if (!m_letterBoxEnabled && m_letterBoxFadeLevel != 0.0f)
		return TRUE;
	return FALSE;
}

//WST 10/2/2002 added query function.  JSC Integrated 5/20/03
Bool W3DDisplay::isLetterBoxed()
{
	return (m_letterBoxEnabled);
}

// W3DDisplay::createLightPulse ===============================================
/** Create a "light pulse" which is a dynamic light that grows, decays
	* and vanishes over several frames */
//=============================================================================
void W3DDisplay::createLightPulse( const Coord3D *pos, const RGBColor *color,
																	 Real innerRadius, Real attenuationWidth,
																	 UnsignedInt increaseFrameTime,
																	 UnsignedInt decayFrameTime//, Bool donut
																	 )
{
	if (m_3DScene == nullptr)
		return;
	if (innerRadius+attenuationWidth<2.0*PATHFIND_CELL_SIZE_F + 1.0f) {
		return; // it basically won't make any visual difference.  jba.
	}
	W3DDynamicLight * theDynamicLight = m_3DScene->getADynamicLight();
	// turn it on.
	theDynamicLight->setEnabled(true);

	theDynamicLight->Set_Ambient( Vector3( color->red, color->green, color->blue ) );
	theDynamicLight->Set_Diffuse( Vector3( color->red, color->green, color->blue) );
	theDynamicLight->Set_Position(Vector3(pos->x, pos->y, pos->z));
	theDynamicLight->Set_Far_Attenuation_Range(innerRadius, innerRadius + attenuationWidth);
	theDynamicLight->setFrameFade(increaseFrameTime, decayFrameTime);
	theDynamicLight->setDecayRange();
	theDynamicLight->setDecayColor();
	//theDynamicLight->setDonut(donut);
	// (gth) CNC3 enable far attenuation.  C&C3 defaults to disabled.  Must enable to match Generals. MW 8-06-03
	theDynamicLight->Set_Flag(LightClass::FAR_ATTENUATION,true);
}

void W3DDisplay::toggleLetterBox()
{
	m_letterBoxEnabled = !m_letterBoxEnabled;
	m_letterBoxFadeStartTime = timeGetTime();

	//WST  9/18/2002 This is not a script api to prevent cheat. JSC Integrated 5/20/03
	if( TheTacticalView )
	{
		TheTacticalView->setZoomLimited( !m_letterBoxEnabled );
	}
}

void W3DDisplay::enableLetterBox(Bool enable)
{
	if (enable)
	{
		if (!m_letterBoxEnabled)
		{	//letterbox mode not previously enabled
			m_letterBoxEnabled = TRUE;
			m_letterBoxFadeStartTime = timeGetTime();

			//WST  9/18/2002 - This is not a script api to prevent cheat.  JSC Integrated 5/20/03
			if( TheTacticalView )
			{
				TheTacticalView->setZoomLimited( 0 );
			}
		}
	}
	else
	{
		if (m_letterBoxEnabled)
		{	//letterbox mode no previously disabled
			m_letterBoxEnabled = FALSE;
			m_letterBoxFadeStartTime = timeGetTime();

			//WST  9/18/2002. JSC Integrated 5/20/03
			if( TheTacticalView )
			{
				TheTacticalView->setZoomLimited( 1 );
			}
		}
	}
}

// W3DDisplay::setTimeOfDay ===================================================
/** */
//=============================================================================
void W3DDisplay::setTimeOfDay( TimeOfDay tod )
{
	const GlobalData::TerrainLighting *ol=&TheGlobalData->m_terrainObjectsLighting[tod][0];

	if( m_3DScene )
	{
		m_3DScene->Set_Ambient_Light( Vector3(ol->ambient.red, ol->ambient.green, ol->ambient.blue) );
	}

	for (Int i=0; i<LightEnvironmentClass::MAX_LIGHTS; i++)
	{
		if( m_myLight[i] )
		{
			ol=&TheGlobalData->m_terrainObjectsLighting[tod][i];

			m_myLight[i]->Set_Ambient( Vector3( 0.0f, 0.0f, 0.0f ) );
			m_myLight[i]->Set_Diffuse( Vector3(ol->diffuse.red, ol->diffuse.green, ol->diffuse.blue ) );
			m_myLight[i]->Set_Specular( Vector3(0,0,0) );
			Matrix3D mtx;
			mtx.Set(Vector3(1,0,0), Vector3(0,1,0), Vector3(ol->lightPos.x, ol->lightPos.y, ol->lightPos.z), Vector3(0,0,0));
			m_myLight[i]->Set_Transform(mtx);
		}
	}
	if(TheTerrainRenderObject) {
		TheTerrainRenderObject->setTimeOfDay(tod);
		TheTacticalView->forceRedraw();
	}
}

// W3DDisplay::drawLine =======================================================
/** draw a line on the display in pixel coordinates with the specified color */
//=============================================================================
void W3DDisplay::drawLine( Int startX, Int startY,
													 Int endX, Int endY,
													 Real lineWidth,
													 UnsignedInt lineColor )
{
	setup2DRenderState(nullptr, DRAW_IMAGE_ALPHA, FALSE);

	m_2DRender->Add_Line( Vector2( startX, startY ), Vector2( endX, endY ),
												lineWidth, lineColor );

	if (!m_isBatching)
	{
		m_2DRender->Render();
	}
}

// W3DDisplay::drawLine =======================================================
/** draw a line on the display in pixel coordinates with the specified color */
//=============================================================================
void W3DDisplay::drawLine( Int startX, Int startY,
													 Int endX, Int endY,
													 Real lineWidth,
													 UnsignedInt lineColor1,UnsignedInt lineColor2 )
{
	setup2DRenderState(nullptr, DRAW_IMAGE_ALPHA, FALSE);

	m_2DRender->Add_Line( Vector2( startX, startY ), Vector2( endX, endY ),
												lineWidth, lineColor1, lineColor2 );

	if (!m_isBatching)
	{
		m_2DRender->Render();
	}

}


// W3DDisplay::drawOpenRect ===================================================
//=============================================================================
void W3DDisplay::drawOpenRect( Int startX, Int startY, Int width, Int height,
															 Real lineWidth, UnsignedInt lineColor )
{

	if (m_isClippedEnabled)
	{
		ICoord2D start, end, returnStart, returnEnd;
		start.x = startX;
		start.y = startY;

		end.x = start.x;
		end.y = start.y + height;
		if(ClipLine2D(&start, &end, &returnStart, &returnEnd, &m_clipRegion ))
			drawLine( returnStart.x, returnStart.y, returnEnd.x, returnEnd.y, lineWidth, lineColor);

		end.x = start.x + width;
		end.y = start.y;
		if(ClipLine2D(&start, &end, &returnStart, &returnEnd, &m_clipRegion ))
			drawLine( returnStart.x, returnStart.y, returnEnd.x, returnEnd.y, lineWidth, lineColor);

		start.x = startX + width;
		start.y = startY;
		end.x = start.x;
		end.y = start.y + height;
		if(ClipLine2D(&start, &end, &returnStart, &returnEnd, &m_clipRegion ))
			drawLine( returnStart.x, returnStart.y, returnEnd.x, returnEnd.y, lineWidth, lineColor);

		start.x = startX;
		start.y = startY + height;
		end.x = start.x + width;
		end.y = start.y;
		if(ClipLine2D(&start, &end, &returnStart, &returnEnd, &m_clipRegion ))
			drawLine( returnStart.x, returnStart.y, returnEnd.x, returnEnd.y, lineWidth, lineColor);
	}
	else
	{
		setup2DRenderState(nullptr, DRAW_IMAGE_ALPHA, FALSE);

		m_2DRender->Add_Outline( RectClass( startX, startY,
																				startX + width, startY + height ),
														 lineWidth, lineColor );

		if (!m_isBatching)
		{
			m_2DRender->Render();
		}
	}

}

// W3DDisplay::drawFillRect ===================================================
//=============================================================================
void W3DDisplay::drawFillRect( Int startX, Int startY, Int width, Int height,
															 UnsignedInt color )
{
	setup2DRenderState(nullptr, DRAW_IMAGE_ALPHA, FALSE);

	m_2DRender->Add_Rect( RectClass( startX, startY,
																	 startX + width, startY + height ),
												0, 0, color );

	if (!m_isBatching)
	{
		m_2DRender->Render();
	}
}

void W3DDisplay::drawRectClock(Int startX, Int startY, Int width, Int height, Int percent, UnsignedInt color)
{
	// sanity
	if(percent < 1 || percent > 100)
		return;

	setup2DRenderState(nullptr, DRAW_IMAGE_ALPHA, FALSE);

// The rectangles are numberd as follows
//(x,y)	|---------|
//			| 4  | 1  |
//			|----+----|
//			| 3  | 2  |
//			|---------| (x + width, y + width)
//
	// we're done, lets just draw one rectangle for it all.
	if(percent == 100)
	{
		m_2DRender->Add_Rect(RectClass( startX, startY,
																		startX + width, startY + height), 0,0, color);
	}
	else if( percent> 75)
	{
		//rectangle #1 & 2
		m_2DRender->Add_Rect(RectClass( startX + width/2, startY,
																		startX + width, startY + height), 0,0, color);
		// rectangle #3
		m_2DRender->Add_Rect(RectClass( startX, startY + height/2,
																		startX + width/2, startY + height), 0,0, color);
		// draw the part of rectangle 4
		Real remain = percent - 75;
		if(remain > 12)
		{
			//draw the full triangle
			m_2DRender->Add_Tri(Vector2(startX, startY),
													Vector2(startX, startY + height/2),
													Vector2(startX + width/2, startY + height/2),
													Vector2(0,0),Vector2(0,0),Vector2(0,0),color);

			// draw the part of triangle
			Real percentDraw = (Real)(remain - 12)/ 13;
			m_2DRender->Add_Tri(Vector2(startX, startY),
													Vector2(startX + width/2, startY + height/2),
													Vector2(startX + (width/2 * percentDraw), startY),
													Vector2(0,0),Vector2(0,0),Vector2(0,0),color);
		}
		else
		{
			// draw the part of triangle
			Real percentDraw = (Real)(remain)/ 12;
			m_2DRender->Add_Tri(Vector2(startX, startY + height/2 - (height/2 * percentDraw)),
													Vector2(startX, startY + height/2),
													Vector2(startX + width/2, startY + height/2),
													Vector2(0,0),Vector2(0,0),Vector2(0,0),color);
		}

	}
	else if( percent > 50)
	{
		//rectangle #1 & 2
		m_2DRender->Add_Rect(RectClass( startX + width/2, startY,
																		startX + width, startY + height), 0,0, color);
		// draw the part of rectangle 3
		Real remain = percent - 50;
		if(remain > 12)
		{
			//draw the full triangle
			m_2DRender->Add_Tri(Vector2(startX + width/2, startY + height/2),
													Vector2(startX, startY + height),
													Vector2(startX + width/2, startY + height),
													Vector2(0,0),Vector2(0,0),Vector2(0,0),color);

			// draw the part of triangle
			Real percentDraw = (Real)(remain - 12)/ 13;
			m_2DRender->Add_Tri(Vector2(startX, startY + height - (height/2 * percentDraw)),
													Vector2(startX, startY + height),
													Vector2(startX + width/2, startY + height/2),
													Vector2(0,0),Vector2(0,0),Vector2(0,0),color);
		}
		else
		{
			// draw the part of triangle
			Real percentDraw = (Real)(remain)/ 12;
			m_2DRender->Add_Tri(Vector2(startX + width/2, startY + height),
													Vector2(startX + width/2, startY + height/2),
													Vector2(startX + width/2 - ( width/2 * percentDraw), startY + height),
													Vector2(0,0),Vector2(0,0),Vector2(0,0),color);
		}
	}
	else if(percent > 25)
	{
		// rectangle #1
		m_2DRender->Add_Rect(RectClass( startX + width/2, startY,
																		startX + width, startY + height/2), 0,0, color);
		// draw the part of rectangle 2
		Real remain = percent - 25;
		if(remain > 12)
		{
			//draw the full triangle
			m_2DRender->Add_Tri(Vector2(startX + width/2, startY + height/2),
													Vector2(startX + width, startY + height),
													Vector2(startX + width, startY + height/2),
													Vector2(0,0),Vector2(0,0),Vector2(0,0),color);

			// draw the part of triangle
			Real percentDraw = (Real)(remain - 12)/ 13;
			m_2DRender->Add_Tri(Vector2(startX + width/2, startY + height/2),
													Vector2(startX + width - (width/2 * percentDraw), startY + height),
													Vector2(startX + width, startY + height),
													Vector2(0,0),Vector2(0,0),Vector2(0,0),color);
		}
		else
		{
			// draw the part of triangle
			Real percentDraw = (Real)(remain)/ 12;
			m_2DRender->Add_Tri(Vector2(startX + width, startY + height/2),
													Vector2(startX + width/2, startY + height/2),
													Vector2(startX + width, startY + height/2 + ( height/2 * percentDraw)),
													Vector2(0,0),Vector2(0,0),Vector2(0,0),color);
		}
	}
	else
	{
				// draw the part of rectangle 1

		if(percent > 12)
		{
			//draw the full triangle
			m_2DRender->Add_Tri(Vector2(startX + width/2, startY),
													Vector2(startX + width/2, startY + height/2),
													Vector2(startX + width, startY),
													Vector2(0,0),Vector2(0,0),Vector2(0,0),color);

			// draw the part of triangle
			Real percentDraw = (Real)(percent - 12)/ 13;
			m_2DRender->Add_Tri(Vector2(startX + width, startY),
													Vector2(startX + width/2, startY + height/2),
													Vector2(startX + width, startY + (height/2 * percentDraw)),
													Vector2(0,0),Vector2(0,0),Vector2(0,0),color);
		}
		else
		{
			// draw the part of triangle
			Real percentDraw = (Real)(percent)/ 12;
			m_2DRender->Add_Tri(Vector2(startX + width/2, startY),
													Vector2(startX + width/2, startY + height/2),
													Vector2(startX + width/2 + (width/2 * percentDraw), startY ),
													Vector2(0,0),Vector2(0,0),Vector2(0,0),color);
		}
	}

	if (!m_isBatching)
	{
		m_2DRender->Render();
	}
}


//--------------------------------------------------------------------------------------------------------------------
// W3DDisplay::drawRemainingRectClock
// Variation added by Kris -- October 2002
// This version will overlay a clock progress from the specified percentage to 100%. Essentially, this function will
// "reveal" an icon as it progresses towards completion.
//--------------------------------------------------------------------------------------------------------------------
void W3DDisplay::drawRemainingRectClock(Int startX, Int startY, Int width, Int height, Int percent, UnsignedInt color)
{
	// sanity
	if( percent < 0 || percent > 99 )
		return;

	setup2DRenderState(nullptr, DRAW_IMAGE_ALPHA, FALSE);

// The rectangles are numbered as follows
//(x,y)	|---------|
//			| 4  | 1  |
//			|----+----|
//			| 3  | 2  |
//			|---------| (x + width, y + width)
//

	Int midX = startX + width/2;
	Int midY = startY + height/2;
	Int endX = startX + width;
	Int endY = startY + height;
	Int halfWidth = width/2;
	Int halfHeight = height/2;

	if( percent == 0 )
	{
		// We just started, so draw the entire remaining rectangle.
		// #1, #2, #3, and #4
		m_2DRender->Add_Rect( RectClass( startX, startY, endX, endY ), 0, 0, color );
	}
	else if( percent < 25 )
	{
		//1-25%
		//-----

		//Rectangle #3 & 4
		m_2DRender->Add_Rect( RectClass( startX, startY, midX, endY ), 0, 0, color );

		//Rectangle #2
		m_2DRender->Add_Rect( RectClass( midX, midY, endX, endY ), 0, 0, color );

		//Handle rectangle #1 than needs partial rendering.
		if( percent < 13 )
		{
			//1-12%
  		//-----

			//Draw the 2nd half of rectangle #1
			m_2DRender->Add_Tri( Vector2( midX, midY ), Vector2( endX, midY ), Vector2( endX, startY ),
													 Vector2( 0, 0 ), Vector2( 0, 0 ), Vector2( 0, 0 ), color );

			//Draw the last part of the 1st portion of rectangle #1
			Real percentDraw = (Real)( 13 - percent ) / 13;
			m_2DRender->Add_Tri( Vector2( midX, midY ), Vector2( endX, startY ), Vector2( endX - halfWidth * percentDraw, startY ),
													 Vector2( 0, 0 ), Vector2( 0, 0 ), Vector2( 0, 0 ), color );
		}
		else
		{
			//13-24%
			//------

			//Draw the last part of the 2nd half of rectangle #1
			Real percentDraw = (Real)( percent - 13 ) / 12;
			m_2DRender->Add_Tri( Vector2( midX, midY ), Vector2( endX, midY ), Vector2( endX, startY + halfHeight * percentDraw ),
													 Vector2( 0, 0 ), Vector2( 0, 0 ), Vector2( 0, 0 ), color );
		}
	}
	else if( percent < 50 )
	{
		//25-49%
		//------

		//rectangle #3 & 4
		m_2DRender->Add_Rect( RectClass( startX, startY, midX, endY ), 0, 0, color );

		//Handle rectangle #2 that needs partial rendering.
		if( percent < 38 )
		{
			//25-37%
  		//-----

			//Draw the 2nd half of rectangle #2
			m_2DRender->Add_Tri( Vector2( midX, midY ), Vector2( midX, endY ), Vector2( endX, endY ),
													 Vector2( 0, 0 ), Vector2( 0, 0 ), Vector2( 0, 0 ), color );

			//Draw the last part of the 1st portion of rectangle #2
			Real percentDraw = (Real)( percent - 25 ) / 13;
			m_2DRender->Add_Tri( Vector2( midX, midY ), Vector2( endX, endY ), Vector2( endX, midY + halfHeight * percentDraw ),
													 Vector2( 0, 0 ), Vector2( 0, 0 ), Vector2( 0, 0 ), color );
		}
		else
		{
			//38-49%
			//------

			//Draw the last part of the 2nd half of rectangle #1
			Real percentDraw = (Real)( percent - 38 ) / 12;
			m_2DRender->Add_Tri( Vector2( midX, midY ), Vector2( midX, endY ), Vector2( endX - halfWidth * percentDraw, endY ),
													 Vector2( 0, 0 ), Vector2( 0, 0 ), Vector2( 0, 0 ), color );
		}
	}
	else if( percent < 75 )
	{
		//50-74%
		//------

		//Rectangle #4
		m_2DRender->Add_Rect( RectClass( startX, startY, midX, midY ), 0, 0, color );

		//Handle rectangle #3 that needs partial rendering.
		if( percent < 63 )
		{
			//50-62%
  		//-----

			//Draw the 2nd half of rectangle #3
			m_2DRender->Add_Tri( Vector2( midX, midY ), Vector2( startX, midY ), Vector2( startX, endY ),
													 Vector2( 0, 0 ), Vector2( 0, 0 ), Vector2( 0, 0 ), color );

			//Draw the last part of the 1st portion of rectangle #3
			Real percentDraw = (Real)( percent - 50 ) / 13;
			m_2DRender->Add_Tri( Vector2( midX, midY ), Vector2( startX, endY ), Vector2( midX - halfWidth * percentDraw, endY ),
													 Vector2( 0, 0 ), Vector2( 0, 0 ), Vector2( 0, 0 ), color );
		}
		else
		{
			//62-74%
			//------

			//Draw the last part of the 2nd half of rectangle #3
			Real percentDraw = (Real)( percent - 62 ) / 12;
			m_2DRender->Add_Tri( Vector2( midX, midY ), Vector2( startX, midY ), Vector2( startX, endY - halfHeight * percentDraw ),
													 Vector2( 0, 0 ), Vector2( 0, 0 ), Vector2( 0, 0 ), color );
		}
	}
	else
	{
		//75-99%
		//------

		//Handle rectangle #4 that needs partial rendering.
		if( percent < 87 )
		{
			//75-87%
  		//-----

			//Draw the 2nd half of rectangle #4
			m_2DRender->Add_Tri( Vector2( midX, midY ), Vector2( midX, startY ), Vector2( startX, startY ),
													 Vector2( 0, 0 ), Vector2( 0, 0 ), Vector2( 0, 0 ), color );

			//Draw the last part of the 1st portion of rectangle #4
			Real percentDraw = (Real)( percent - 75 ) / 13;
			m_2DRender->Add_Tri( Vector2( midX, midY ), Vector2( startX, startY ), Vector2( startX, midY - halfHeight * percentDraw ),
													 Vector2( 0, 0 ), Vector2( 0, 0 ), Vector2( 0, 0 ), color );
		}
		else
		{
			//88-99%
			//------

			//Draw the last part of the 2nd half of rectangle #4
			Real percentDraw = (Real)( percent - 88 ) / 12;
			m_2DRender->Add_Tri( Vector2( midX, midY ), Vector2( midX, startY ), Vector2( startX + halfWidth * percentDraw, startY ),
													 Vector2( 0, 0 ), Vector2( 0, 0 ), Vector2( 0, 0 ), color );
		}
	}

	if (!m_isBatching)
	{
		m_2DRender->Render();
	}
}


// W3DDisplay::drawImage ======================================================
/** Draws an images at the screen coordinates and keeps it within the end
	* screen coords specified */
//=============================================================================
void W3DDisplay::drawImage( const Image *image, Int startX, Int startY,
														Int endX, Int endY, Color color, DrawImageMode mode)
{

	// sanity
	if( image == nullptr )
		return;

	if (m_isClippedEnabled)
	{
		if (endX <= m_clipRegion.lo.x ||
			endY <= m_clipRegion.lo.y ||
			startX >= m_clipRegion.hi.x ||
			startY >= m_clipRegion.hi.y)
		{
			return;	//nothing to render
		}
	}

	// !!
	// Remember to update the GUIEditDisplay::drawImage when you make
	// changes to this, it technically uses W3D code to render itself,
	// but it not derived on the W3DDisplay
	// !!

	const Region2D *uv = image->getUV();

	TextureClass *tex = nullptr;
	if (BitIsSet(image->getStatus(), IMAGE_STATUS_RAW_TEXTURE))
		tex = (TextureClass *)(image->getRawTextureData());
	else
		tex = WW3DAssetManager::Get_Instance()->Get_Texture(image->getFilename().str(), MIP_LEVELS_1);

	Bool grayscale = (mode == DRAW_IMAGE_GRAYSCALE);
	setup2DRenderState(tex, mode, grayscale);

	RectClass screen_rect(startX,startY,endX,endY);
	RectClass uv_rect(uv->lo.x,uv->lo.y,uv->hi.x,uv->hi.y);

	if (m_isClippedEnabled)
	{	//need to clip this quad to clip rectangle
		if (screen_rect.Left < m_clipRegion.lo.x || screen_rect.Right > m_clipRegion.hi.x || screen_rect.Top < m_clipRegion.lo.y || screen_rect.Bottom > m_clipRegion.hi.y)
		{
			RectClass clipped_rect;
			RectClass clipped_uv_rect;

			if( BitIsSet( image->getStatus(), IMAGE_STATUS_ROTATED_90_CLOCKWISE ) )
			{


				//
				//	Clip the polygons to the specified area
				//

				// GeneralsX @bugfix BenderAI 13/02/2026 Use MAX/MIN macros (cross-platform, defined in BaseTypeCore.h)
				clipped_rect.Left		= MAX (screen_rect.Left, m_clipRegion.lo.x);
				clipped_rect.Right	= MIN (screen_rect.Right, m_clipRegion.hi.x);
				clipped_rect.Top		= MAX (screen_rect.Top, m_clipRegion.lo.y);
				clipped_rect.Bottom	= MIN (screen_rect.Bottom, m_clipRegion.hi.y);

				//
				//	Clip the texture to the specified area
				//

				float percent				= ((clipped_rect.Left - screen_rect.Left) / screen_rect.Width ());
				clipped_uv_rect.Top		= uv_rect.Top + (uv_rect.Height () * percent);

				percent						= ((clipped_rect.Right - screen_rect.Left) / screen_rect.Width ());
				clipped_uv_rect.Bottom	= uv_rect.Top + (uv_rect.Height () * percent);

				percent						= ((clipped_rect.Top - screen_rect.Top) / screen_rect.Height ());
				clipped_uv_rect.Right	= uv_rect.Right - (uv_rect.Width () * percent);

				percent						= ((clipped_rect.Bottom - screen_rect.Top) / screen_rect.Height ());
				clipped_uv_rect.Left		= uv_rect.Right - (uv_rect.Width () * percent);
			}
			else

			{

				//
				//	Clip the polygons to the specified area
				//

				// GeneralsX @bugfix BenderAI 13/02/2026 Use MAX/MIN macros (cross-platform, defined in BaseTypeCore.h)
				clipped_rect.Left		= MAX (screen_rect.Left, m_clipRegion.lo.x);
				clipped_rect.Right	= MIN (screen_rect.Right, m_clipRegion.hi.x);
				clipped_rect.Top		= MAX (screen_rect.Top, m_clipRegion.lo.y);
				clipped_rect.Bottom	= MIN (screen_rect.Bottom, m_clipRegion.hi.y);

				//
				//	Clip the texture to the specified area
				//

				float percent				= ((clipped_rect.Left - screen_rect.Left) / screen_rect.Width ());
				clipped_uv_rect.Left		= uv_rect.Left + (uv_rect.Width () * percent);

				percent						= ((clipped_rect.Right - screen_rect.Left) / screen_rect.Width ());
				clipped_uv_rect.Right	= uv_rect.Left + (uv_rect.Width () * percent);

				percent						= ((clipped_rect.Top - screen_rect.Top) / screen_rect.Height ());
				clipped_uv_rect.Top		= uv_rect.Top + (uv_rect.Height () * percent);

				percent						= ((clipped_rect.Bottom - screen_rect.Top) / screen_rect.Height ());
				clipped_uv_rect.Bottom	= uv_rect.Top + (uv_rect.Height () * percent);
			}

			//
			//	Use the clipped rectangles to render
			//
			screen_rect = clipped_rect;
			uv_rect		= clipped_uv_rect;
		}
	}

	// if rotated 90 degrees clockwise we have to adjust the uv coords
	if( BitIsSet( image->getStatus(), IMAGE_STATUS_ROTATED_90_CLOCKWISE ) )
	{

		m_2DRender->Add_Tri( Vector2( screen_rect.Left, screen_rect.Top ),
												 Vector2( screen_rect.Left, screen_rect.Bottom ),
												 Vector2( screen_rect.Right, screen_rect.Top ),
												 Vector2( uv_rect.Right, uv_rect.Top),
												 Vector2( uv_rect.Left, uv_rect.Top),
												 Vector2( uv_rect.Right, uv_rect.Bottom ),
												 color );

		m_2DRender->Add_Tri( Vector2( screen_rect.Right, screen_rect.Bottom ),
												 Vector2( screen_rect.Right, screen_rect.Top ),
												 Vector2( screen_rect.Left, screen_rect.Bottom ),
												 Vector2( uv_rect.Left, uv_rect.Bottom ),
												 Vector2( uv_rect.Right, uv_rect.Bottom ),
												 Vector2( uv_rect.Left, uv_rect.Top ),
												 color );

	}
	else
	{

		// just draw as normal
		m_2DRender->Add_Quad( screen_rect, uv_rect, color );

	}

	if (!m_isBatching)
	{
		m_2DRender->Render();
		m_2DRender->Enable_Grayscale(false);
		if (mode == DRAW_IMAGE_ADDITIVE || mode == DRAW_IMAGE_SOLID)
		{
			m_2DRender->Enable_Alpha(true);
		}
	}

	if (tex != nullptr && !BitIsSet(image->getStatus(), IMAGE_STATUS_RAW_TEXTURE))
	{
		tex->Release_Ref();
	}

}

//============================================================================
// W3DDisplay::createVideoBuffer
//============================================================================

VideoBuffer*	W3DDisplay::createVideoBuffer()
{
	VideoBuffer::Type format = VideoBuffer::TYPE_UNKNOWN;

	/// @todo query video player for supported formats - we assume bink formats here

	// first try to use the native format

	WW3DFormat displayFormat = DX8Wrapper::getBackBufferFormat();

#if defined(__APPLE__)
	// DXVK/MoltenVK can report every legacy D3D8 texture format as unsupported
	// through the old capability table even though X8R8G8B8 managed textures are
	// valid. Do this before the legacy selection block: that block otherwise
	// returns nullptr before W3DVideoBuffer gets a chance to try its fallbacks.
	format = VideoBuffer::TYPE_X8R8G8B8;
#else
	if ( DX8Wrapper::Get_Current_Caps()->Support_Texture_Format( displayFormat ))
	{
		format = W3DVideoBuffer::W3DFormatToType( displayFormat );
	}

	if ( format == VideoBuffer::TYPE_UNKNOWN )
	{
		if ( DX8Wrapper::Get_Current_Caps()->Support_Texture_Format( WW3D_FORMAT_X8R8G8B8 ))
		{
			format = VideoBuffer::TYPE_X8R8G8B8;
		}
		else if ( DX8Wrapper::Get_Current_Caps()->Support_Texture_Format( WW3D_FORMAT_R8G8B8 ))
		{
			format = VideoBuffer::TYPE_R8G8B8;
		}
		else if ( DX8Wrapper::Get_Current_Caps()->Support_Texture_Format( WW3D_FORMAT_R5G6B5 ))
		{
			format = VideoBuffer::TYPE_R5G6B5;
		}
		else if ( DX8Wrapper::Get_Current_Caps()->Support_Texture_Format( WW3D_FORMAT_X1R5G5B5 ))
		{
			format = VideoBuffer::TYPE_X1R5G5B5;
		}
		else
		{
			// card does not support any of the formats we need
			return nullptr;
		}
	}
	// on low mem machines, render every video in 16bit
	if (TheGameLODManager && (!TheGameLODManager->didMemPass() || W3DShaderManager::getChipset() == DC_GEFORCE2))
		format = VideoBuffer::TYPE_R5G6B5;
#endif
	fprintf(stderr,
		"INFO: GX create video buffer display_format=%d selected_type=%d lod_mem_pass=%d\n",
		(int)displayFormat,
		(int)format,
		TheGameLODManager == nullptr || TheGameLODManager->didMemPass() ? 1 : 0);

	W3DVideoBuffer *buffer = NEW W3DVideoBuffer( format );

	return buffer;
}

//============================================================================
// W3DDisplay::drawScaledVideoBuffer
//============================================================================

void W3DDisplay::drawScaledVideoBuffer( VideoBuffer *buffer, VideoStreamInterface *stream )
{
	// TheSuperHackers @bugfix Mauller 20/07/2025 scale videos based on screen size so they are shown in their original aspect
	Real videoAspect = (Real)stream->width() / (Real)stream->height();
	Real displayAspect = (Real)getWidth() / (Real)getHeight();
	Bool wideAspect = displayAspect >= videoAspect;

	Int startX = 0;
	Int endX = 0;
	Int startY = 0;
	Int endY = 0;

	if (wideAspect)
	{
		// TheSuperHackers @info if we are in a wide aspect, we scale the videos width and fill the height
		Real heightScale = (Real)getHeight() / (Real)stream->height();
		startX = (getWidth() / 2.0f) - (stream->width() * heightScale / 2.0f);
		endX = (getWidth() / 2.0f) + (stream->width() * heightScale / 2.0f);

		endY = getHeight();
	}
	else
	{
		// TheSuperHackers @info if we are in a narrow aspect, we scale the videos height and fill the width
		Real widthScale = (Real)getWidth() / (Real)stream->width();
		startY = (getHeight() / 2.0f) - (stream->height() * widthScale / 2.0f);
		endY = (getHeight() / 2.0f) + (stream->height() * widthScale / 2.0f);

		endX = getWidth();
	}

	drawVideoBuffer( buffer, startX, startY, endX, endY );
}

//============================================================================
// W3DDisplay::drawVideoBuffer
//============================================================================

void W3DDisplay::drawVideoBuffer( VideoBuffer *buffer, Int startX, Int startY, Int endX, Int endY )
{
	W3DVideoBuffer *vbuffer = (W3DVideoBuffer*) buffer;

	setup2DRenderState(vbuffer->texture(), DRAW_IMAGE_ALPHA, FALSE);

	m_2DRender->Add_Quad( RectClass( startX, startY, endX, endY ),
												vbuffer->Rect( 0, 0, 1, 1) );
	
	if (!m_isBatching)
	{
		m_2DRender->Render();
	}

}

// W3DDisplay::setClipRegion ============================================
/** Set the clipping region for images.
  @todo: Make this work for all primitives, not just drawImage. */
//=============================================================================
void W3DDisplay::setClipRegion( IRegion2D *region )
{
		// assign new region
		m_clipRegion = *region;
		m_isClippedEnabled = TRUE;

}

//=============================================================================
/* we don't really need to override this call, since we will soon be called to
	update every shroud cell explicitly...
*/
void W3DDisplay::clearShroud()
{
	// nothing
}

//=============================================================================
void W3DDisplay::setBorderShroudLevel(UnsignedByte level)
{
	if (TheTerrainRenderObject && TheTerrainRenderObject->getShroud())
	{
		TheTerrainRenderObject->getShroud()->setBorderShroudLevel((W3DShroudLevel)level);
	}
}

//=============================================================================
void W3DDisplay::setShroudLevel( Int x, Int y, CellShroudStatus setting )
{
	if (TheTerrainRenderObject && TheTerrainRenderObject->getShroud())
	{
		#ifdef INTENSE_DEBUG
		TheTerrainRenderObject->getShroud()->setShroudFilter(false);
		#endif
		if( setting == CELLSHROUD_SHROUDED )
			TheTerrainRenderObject->getShroud()->setShroudLevel(x, y, (W3DShroudLevel)TheGlobalData->m_shroudAlpha );
		else if( setting == CELLSHROUD_FOGGED )
			TheTerrainRenderObject->getShroud()->setShroudLevel(x, y, (W3DShroudLevel)TheGlobalData->m_fogAlpha );///< @todo placeholder to get feedback on logic work while graphic side being decided
		else
			TheTerrainRenderObject->getShroud()->setShroudLevel(x, y, (W3DShroudLevel)TheGlobalData->m_clearAlpha );
		//Logic is saying shroud.  We can add alpha levels here in client if needed.
		// W3DShroud is a 0-255 alpha byte.  Logic shroud is a double reference count.

		TheTerrainRenderObject->notifyShroudChanged();

	}
}

//=============================================================================
///Utility function to dump data into a .BMP file
// GeneralsX @build BenderAI 13/02/2026 Screenshot is Windows-specific functionality
#ifdef _WIN32
static void CreateBMPFile(LPTSTR pszFile, char *image, Int width, Int height)
{
	HANDLE hf;                  // file handle
	BITMAPFILEHEADER hdr;       // bitmap file-header
	BITMAPINFOHEADER* pbih;     // bitmap info-header
	unsigned char* lpBits;      // memory pointer
	DWORD dwTotal;              // total count of bytes
	DWORD cb;                   // incremental count of bytes
	BYTE *hp;                   // byte pointer
	DWORD dwTmp;

	BITMAPINFO* pbmi;

	pbmi = (BITMAPINFO*) LocalAlloc(LPTR,sizeof(BITMAPINFOHEADER));
	if (pbmi == nullptr)
		return;

	pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pbmi->bmiHeader.biWidth = width;
	pbmi->bmiHeader.biHeight = height;
	pbmi->bmiHeader.biPlanes = 1;
	pbmi->bmiHeader.biBitCount = 24;
	pbmi->bmiHeader.biCompression = BI_RGB;
	pbmi->bmiHeader.biSizeImage = (pbmi->bmiHeader.biWidth + 7) /8 * pbmi->bmiHeader.biHeight * 24;
	pbmi->bmiHeader.biClrImportant = 0;

	pbih = (BITMAPINFOHEADER*) pbmi;
	lpBits = (unsigned char*) image;

	// Create the .BMP file.
	hf = CreateFile(pszFile,
		GENERIC_READ | GENERIC_WRITE,
		(DWORD) 0,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		(HANDLE) nullptr);

	if (hf != INVALID_HANDLE_VALUE)
	{
		hdr.bfType = 0x4d42;        // 0x42 = "B" 0x4d = "M"
		// Compute the size of the entire file.
		hdr.bfSize = (DWORD) (sizeof(BITMAPFILEHEADER) +
									pbih->biSize + pbih->biClrUsed
									* sizeof(RGBQUAD) + pbih->biSizeImage);
		hdr.bfReserved1 = 0;
		hdr.bfReserved2 = 0;

		// Compute the offset to the array of color indices.
		hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) +
									pbih->biSize + pbih->biClrUsed
									* sizeof (RGBQUAD);

		// Copy the BITMAPFILEHEADER into the .BMP file.
		if (WriteFile(hf, (LPVOID) &hdr, sizeof(BITMAPFILEHEADER),
				(LPDWORD) &dwTmp,  nullptr))
		{
			// Copy the BITMAPINFOHEADER and RGBQUAD array into the file.
			if (WriteFile(hf, (LPVOID) pbih, sizeof(BITMAPINFOHEADER) + pbih->biClrUsed * sizeof (RGBQUAD),(LPDWORD) &dwTmp, nullptr))
			{
				// Copy the array of color indices into the .BMP file.
				dwTotal = cb = pbih->biSizeImage;
				hp = lpBits;
				WriteFile(hf, (LPSTR) hp, (int) cb, (LPDWORD) &dwTmp, nullptr);
			}
		}

		// Close the .BMP file.
		CloseHandle(hf);
	}

	// Free memory.
	LocalFree( (HLOCAL) pbmi);
}
#else
// Linux stub: Screenshot not implemented (would require SDL3 surface capture)
static void CreateBMPFile(const char* pszFile, char *image, Int width, Int height)
{
	// TODO (Phase 3): Implement SDL3-based screenshot capture
}
#endif
// GeneralsX @build BenderAI 13/02/2026 Screenshot is Windows-specific functionality
#ifdef _WIN32
void W3DDisplay::takeScreenShot()
{
	char leafname[256];
	char pathname[1024];

	static int frame_number = 1;

	Bool done = false;
	while (!done) {
#ifdef CAPTURE_TO_TARGA
		sprintf( leafname, "%s%.3d.tga", "sshot", frame_number++);
#else
		sprintf( leafname, "%s%.3d.bmp", "sshot", frame_number++);
#endif
		strlcpy(pathname, TheGlobalData->getPath_UserData().str(), ARRAY_SIZE(pathname));
		strlcat(pathname, leafname, ARRAY_SIZE(pathname));
		if (_access( pathname, 0 ) == -1)
			done = true;
	}

	// TheSuperHackers @bugfix xezon 21/05/2025 Get the back buffer and create a copy of the surface.
	// Originally this code took the front buffer and tried to lock it. This does not work when the
	// render view clips outside the desktop boundaries. It crashed the game.
	SurfaceClass* surface = DX8Wrapper::_Get_DX8_Back_Buffer();

	SurfaceClass::SurfaceDescription surfaceDesc;
	surface->Get_Description(surfaceDesc);

	SurfaceClass* surfaceCopy = NEW_REF(SurfaceClass, (DX8Wrapper::_Create_DX8_Surface(surfaceDesc.Width, surfaceDesc.Height, surfaceDesc.Format)));
	DX8Wrapper::_Copy_DX8_Rects(surface->Peek_D3D_Surface(), nullptr, 0, surfaceCopy->Peek_D3D_Surface(), nullptr);

	surface->Release_Ref();
	surface = nullptr;

	struct Rect
	{
		int Pitch;
		void* pBits;
	} lrect;

	lrect.pBits = surfaceCopy->Lock(&lrect.Pitch);
	if (lrect.pBits == nullptr)
	{
		surfaceCopy->Release_Ref();
		return;
	}

	unsigned int x,y,index,index2,width,height;

	width = surfaceDesc.Width;
	height = surfaceDesc.Height;

	char *image=NEW char[3*width*height];
#ifdef CAPTURE_TO_TARGA
	//bytes are mixed in targa files, not rgb order.
	for (y=0; y<height; y++)
	{
		for (x=0; x<width; x++)
		{
			// index for image
			index=3*(x+y*width);
			// index for fb
			index2=y*lrect.Pitch+4*x;

			image[index]=*((char *) lrect.pBits + index2+2);
			image[index+1]=*((char *) lrect.pBits + index2+1);
			image[index+2]=*((char *) lrect.pBits + index2+0);
		}
	}

	surfaceCopy->Unlock();
	surfaceCopy->Release_Ref();
	surfaceCopy = nullptr;

	Targa targ;
	memset(&targ.Header,0,sizeof(targ.Header));
	targ.Header.Width=width;
	targ.Header.Height=height;
	targ.Header.PixelDepth=24;
	targ.Header.ImageType=TGA_TRUECOLOR;
	targ.SetImage(image);
	targ.YFlip();

	targ.Save(pathname,TGAF_IMAGE,false);
#else	//capturing to bmp file
	//bmp is same byte order
	for (y=0; y<height; y++)
	{
		for (x=0; x<width; x++)
		{
			// index for image
			index=3*(x+y*width);
			// index for fb
			index2=y*lrect.Pitch+4*x;

			image[index]=*((char *) lrect.pBits + index2+0);
			image[index+1]=*((char *) lrect.pBits + index2+1);
			image[index+2]=*((char *) lrect.pBits + index2+2);
		}
	}

	surfaceCopy->Unlock();
	surfaceCopy->Release_Ref();
	surfaceCopy = nullptr;

	//Flip the image
	char *ptr,*ptr1;
	char  v,v1;

	for (y = 0; y < (height >> 1); y++)
	{
		/* Compute address of lines to exchange. */
		ptr = (image + ((width * y) * 3));
		ptr1 = (image + ((width * (height - 1)) * 3));
		ptr1 -= ((width * y) * 3);

		/* Exchange all the pixels on this scan line. */
		for (x = 0; x < (width * 3); x++)
			{
			v = *ptr;
			v1 = *ptr1;
			*ptr = v1;
			*ptr1 = v;
			ptr++;
			ptr1++;
			}
	}
	CreateBMPFile(pathname, image, width, height);
#endif

	delete [] image;

	UnicodeString ufileName;
	ufileName.translate(leafname);
	TheInGameUI->message(TheGameText->fetch("GUI:ScreenCapture"), ufileName.str());
}
#else
// Linux stub: Screenshot capture not implemented
void W3DDisplay::takeScreenShot(void)
{
	// TODO (Phase 3): Implement SDL3-based screenshot capture
}
#endif

/** Start/Stop capturing an AVI movie*/
void W3DDisplay::toggleMovieCapture()
{
	WW3D::Toggle_Movie_Capture("Movie",30);
}


#if defined(RTS_DEBUG)

static FILE *AssetDumpFile=nullptr;

void dumpMeshAssets(MeshClass *mesh)
{
	if (mesh)
	{
		TextureClass *texture;
		//MaterialInfoClass	*material = mesh->Get_Material_Info();
		MeshModelClass *model=mesh->Get_Model();
		for (int stage=0;stage<MeshMatDescClass::MAX_TEX_STAGES;++stage)
		{
			for (int pass=0;pass<model->Get_Pass_Count();++pass)
			{
				if (model->Has_Texture_Array(pass,stage))
				{
					for (int i=0;i<model->Get_Polygon_Count();++i)
					{
						if ((texture=model->Peek_Texture(i,pass,stage)) != nullptr)
						{
							fprintf(AssetDumpFile,"\t%s\n",texture->Get_Texture_Name().str());
						}
					}
				}
				else
				{
					if ((texture=model->Peek_Single_Texture(pass,stage)) != nullptr)
					{
						fprintf(AssetDumpFile,"\t%s\n",texture->Get_Texture_Name().str());
					}
				}
			}
		}
	}
}

void dumpHLODAssets(HLodClass *hlod)
{
	if (hlod)
	{
		//model composed of multiple meshes.
		for (Int i=0; i<hlod->Get_Num_Sub_Objects(); i++)
		{
			RenderObjClass *subObj=hlod->Get_Sub_Object(i);
			if (subObj->Class_ID() == RenderObjClass::CLASSID_HLOD)
				dumpHLODAssets((HLodClass *)subObj);
			else
			if (subObj->Class_ID() == RenderObjClass::CLASSID_MESH)
				dumpMeshAssets((MeshClass *)subObj);
		}
	}
}

//-------------------------------------------------------------------------------------------------
/**  dump all used models/textures to a file.*/
//-------------------------------------------------------------------------------------------------
void W3DDisplay::dumpModelAssets(const char *path)
{
	if (m_3DScene)
	{
		AssetDumpFile=fopen(path,"w");
		if (AssetDumpFile)
		{
			fprintf(AssetDumpFile,"Models and Textures used on %s:\n\n",TheGlobalData->m_mapName.str());
			SceneIterator *sceneIter = m_3DScene->Create_Iterator();
			sceneIter->First();
			while(!sceneIter->Is_Done())
			{
				RenderObjClass * robj = sceneIter->Current_Item();
				if (robj->Class_ID() == RenderObjClass::CLASSID_HLOD)
				{	fprintf(AssetDumpFile,"%s.W3D:\n",robj->Get_Name());
					dumpHLODAssets((HLodClass *)robj);
				}
				else
				if (robj->Class_ID() == RenderObjClass::CLASSID_MESH)
				{	fprintf(AssetDumpFile,"%s.W3D:\n",robj->Get_Name());
					dumpMeshAssets((MeshClass *)robj);
				}
				sceneIter->Next();
			}
			m_3DScene->Destroy_Iterator(sceneIter);
			fclose(AssetDumpFile);
		}
	}
}
#endif	//only include above code in debug and internal
//-------------------------------------------------------------------------------------------------
/** Preload using the W3D asset manager the model referenced by the string parameter */
//-------------------------------------------------------------------------------------------------
void W3DDisplay::preloadModelAssets( AsciiString model )
{

	if( m_assetManager )
	{
		AsciiString nameWithExtension;

		nameWithExtension.format( "%s.w3d", model.str() );
		m_assetManager->Load_3D_Assets( nameWithExtension.str() );

	}

}

//-------------------------------------------------------------------------------------------------
/** Preload using the W3D asset manager the texture referenced by the string parameter */
//-------------------------------------------------------------------------------------------------
void W3DDisplay::preloadTextureAssets( AsciiString texture )
{

	if( m_assetManager )
	{
		TextureClass *theTexture = m_assetManager->Get_Texture( texture.str() );
		theTexture->Release_Ref();//release reference
	}

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void W3DDisplay::doSmartAssetPurgeAndPreload(const char* usageFileName)
{
	if (!m_assetManager || !usageFileName || !*usageFileName)
		return;

	DynamicVectorClass<StringClass> names(8000);

	// use TheFileSystem here so we can bigify these files
	File* f = TheFileSystem->openFile(usageFileName, File::READ | File::TEXT);
	if (f)
	{
		for (;;)
		{
			AsciiString tmp;
			if (f->scanString(tmp) == FALSE)
				break;

			// allow for comments in the file. Note that this doesn't allow for comments
			// with spaces! doh. oh well. better than nothing.
			if (tmp.str()[0] == ';')
				continue;

			names.Add(StringClass(tmp.str()));
		}
		f->close();
	}

	// just free everything if there's no exclusion list file (send in an empty list)
	m_assetManager->Free_Assets_With_Exclusion_List(names);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
#if defined(RTS_DEBUG)
void W3DDisplay::dumpAssetUsage(const char* mapname)
{
	if (!m_assetManager || !mapname || !*mapname)
		return;

	DynamicVectorClass<StringClass> names(8000);
	m_assetManager->Create_Asset_List(names);

	const char* leafname = strrchr(mapname, '\\');
	if (leafname)
		++leafname;					// point to first character after the last backslash
	else
		leafname = mapname;		// point to the start of the filename

	char buf[256];
	int idx = 1;
	while (true)
	{
		sprintf(buf, "AssetUsage_%s_%04d.txt",leafname,idx);
		if (_access(buf, 0) != 0)
			break;	// it exists, we're good
		++idx;
	}

	FILE *fp = fopen(buf, "w");
	if (fp)
	{
		for (int i=0; i<names.Count(); i++)
		{
			const char* n = names[i];
			fprintf(fp, "%s\n", n);
		}
		fclose(fp);
	}
}
#endif

//-------------------------------------------------------------------------------------------------
static void drawFramerateBar()
{
	static DWORD prevTime = timeGetTime();
	DWORD now = timeGetTime();
	Real percTime = (1000.0f / (now - prevTime) ) / (1000.0f / TheGlobalData->m_framesPerSecondLimit);

	if (percTime > 1.0f)
		percTime = 1.0f;
	else if (percTime < 0.0f)
		percTime = 0.0f;
	Int width = REAL_TO_INT(percTime * TheDisplay->getWidth());
	UnsignedInt colorToUse = GameMakeColor( REAL_TO_UNSIGNEDBYTE((1.0f - percTime) * 255),
																					REAL_TO_UNSIGNEDBYTE(percTime * 255),
																					0,
																					0x7F);

	TheDisplay->drawFillRect(1, 1, width, 15, colorToUse);
	prevTime = now;
}
