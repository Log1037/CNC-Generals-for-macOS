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

// FILE: Shell.cpp ////////////////////////////////////////////////////////////////////////////////
// Author: Colin Day, September 2001
// Description: Shell menu representations
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/RandomValue.h"
#include "GameClient/Shell.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/View.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GameWindowTransitions.h"
#include "GameClient/IMEManager.h"
#include "GameClient/AnimateWindowManager.h"
#include "GameClient/ShellMenuScheme.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/GameSpyOverlay.h"
#include "GameNetwork/GameSpy/PeerDefsImplementation.h"

// PUBLIC DATA ////////////////////////////////////////////////////////////////////////////////////
Shell *TheShell = nullptr;  ///< the shell singleton definition

#if defined(__APPLE__)
static UnsignedInt s_gxLastShellResolutionChangeTime = 0;
#endif


// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Shell::Shell()
{
	construct();

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Shell::~Shell()
{
	deconstruct();

}

//-------------------------------------------------------------------------------------------------
void Shell::construct()
{
	Int i;

	m_screenCount = 0;
	for( i = 0; i < MAX_SHELL_STACK; i++ )
		m_screenStack[ i ] = nullptr;

	m_pendingPush = FALSE;
	m_pendingPop = FALSE;
	m_pendingPushName.set( "" );
	m_isShellActive = TRUE;
	m_shellMapOn = FALSE;
	m_shellMapRefreshTime = 0;
	m_background = nullptr;
	m_clearBackground = FALSE;
	m_animateWindowManager = NEW AnimateWindowManager;
	m_schemeManager = NEW ShellMenuSchemeManager;
	m_saveLoadMenuLayout = nullptr;
	m_popupReplayLayout = nullptr;
	m_optionsLayout = nullptr;
	m_screenCount = 0;
}

//-------------------------------------------------------------------------------------------------
void Shell::deconstruct()
{
	WindowLayout *newTop = top();
	while(newTop)
	{
		popImmediate();
		newTop = top();
	}

	if(m_background)
	{
		m_background->destroyWindows();
		deleteInstance(m_background);
		m_background = nullptr;
	}

	delete m_animateWindowManager;
	m_animateWindowManager = nullptr;

	delete m_schemeManager;
	m_schemeManager = nullptr;

	// delete the save/load menu if present
	if( m_saveLoadMenuLayout )
	{

		m_saveLoadMenuLayout->destroyWindows();
		deleteInstance(m_saveLoadMenuLayout);
		m_saveLoadMenuLayout = nullptr;

	}

	// delete the replay save menu if present
	if( m_popupReplayLayout )
	{

		m_popupReplayLayout->destroyWindows();
		deleteInstance(m_popupReplayLayout);
		m_popupReplayLayout = nullptr;

	}

	// delete the options menu if present.
	if (m_optionsLayout != nullptr) {
		m_optionsLayout->destroyWindows();
		deleteInstance(m_optionsLayout);
		m_optionsLayout = nullptr;
	}
}

//-------------------------------------------------------------------------------------------------
/** Initialize the shell system */
//-------------------------------------------------------------------------------------------------
void Shell::init()
{
	INI ini;
	// Read from INI all the ShellMenuScheme
	ini.loadFileDirectory( "Data\\INI\\Default\\ShellMenuScheme", INI_LOAD_OVERWRITE, nullptr );
	ini.loadFileDirectory( "Data\\INI\\ShellMenuScheme", INI_LOAD_OVERWRITE, nullptr );

	if( m_schemeManager )
		m_schemeManager->init();

}

//-------------------------------------------------------------------------------------------------
/** Reset the shell system to a clean state just as though init had
	* just been called and ready to re-use */
//-------------------------------------------------------------------------------------------------
void Shell::reset()
{

	if (TheIMEManager)
		TheIMEManager->detach();

	// pop all screens
	while( m_screenCount )
		popImmediate();

	m_animateWindowManager->reset();

}

//-------------------------------------------------------------------------------------------------
/** Update shell system cycle.  All windows are updated that are on the stack, starting
	* with the top layout and progressing to the bottom one */
//-------------------------------------------------------------------------------------------------
void Shell::update()
{
	static Int lastUpdate = timeGetTime();
	static Int lastShellMapHealthLog = 0;
	static const Int shellUpdateDelay = 30;  // try to update 30 frames a second
	Int now = timeGetTime();

	//
	// we keep the shell updates fixed in time so that we can write consistent animation
	// speeds during the screen update functions
	//
	if( now - lastUpdate >= ((1000.0f / shellUpdateDelay ) - 1) )
	{
		// MoltenVK can leave the 3D shell presenting a stale black image after
		// a focus-driven swapchain rebuild even though the scene still renders.
		// Recreate the shell game after the swapchain has had a short grace period.
		if (m_shellMapRefreshTime != 0 && (Int)(now - m_shellMapRefreshTime) >= 0)
		{
			m_shellMapRefreshTime = 0;
			if (m_shellMapOn && m_isShellActive && TheGameLogic &&
				TheGameLogic->isInGame() && TheGameLogic->getGameMode() == GAME_SHELL)
			{
				fprintf(stderr, "INFO: GX refreshing shell map after window focus restore\n");
				showShellMap(FALSE);
				showShellMap(TRUE);
			}
		}

		// run the updates for every window layout on the stack
		for( Int i = m_screenCount - 1; i >= 0; i-- )
		{

			DEBUG_ASSERTCRASH( m_screenStack[ i ], ("Top of shell stack is null!") );
			m_screenStack[ i ]->runUpdate( nullptr );

		}
		if(TheGlobalData->m_shellMapOn && m_shellMapOn &&m_background)
		{

			m_background->destroyWindows();
			deleteInstance(m_background);
			m_background = nullptr;

		}

		// Update the animate window manager
		m_animateWindowManager->update();

		m_schemeManager->update();

#if defined(__APPLE__)
		// The shell map can finish loading while the menu still presents a black
		// background.  Record the actual scene and camera state so we can
		// distinguish an empty map, a hidden fallback window, and a bad camera.
		if (m_shellMapOn && TheGameLogic && now - lastShellMapHealthLog >= 2000)
		{
			UnsignedInt drawableCount = 0;
			if (TheGameClient)
			{
				for (Drawable *draw = TheGameClient->getDrawableList(); draw; draw = draw->getNextDrawable())
				{
					++drawableCount;
				}
			}

			Coord3D cameraTarget = { 0.0f, 0.0f, 0.0f };
			Coord3D cameraPosition = { 0.0f, 0.0f, 0.0f };
			Coord3D cameraDirection = { 0.0f, 0.0f, 0.0f };
			Real cameraAngle = 0.0f;
			Real cameraPitch = 0.0f;
			Real cameraZoom = 0.0f;
			if (TheTacticalView)
			{
				cameraTarget = TheTacticalView->getPosition();
				cameraPosition = TheTacticalView->get3DCameraPosition();
				cameraDirection = TheTacticalView->get3DCameraDirection();
				cameraAngle = TheTacticalView->getAngle();
				cameraPitch = TheTacticalView->getPitch();
				cameraZoom = TheTacticalView->getZoom();
			}

			fprintf(stderr,
				"INFO: GX shell health mode=%d loading=%d objects=%u drawables=%u rendered=%u fallback_bg=%d "
				"target=(%.2f,%.2f,%.2f) camera=(%.2f,%.2f,%.2f) dir=(%.3f,%.3f,%.3f) "
				"angle=%.3f pitch=%.3f zoom=%.3f\n",
				(int)TheGameLogic->getGameMode(),
				TheGameLogic->isLoadingMap() ? 1 : 0,
				TheGameLogic->getObjectCount(),
				drawableCount,
				TheGameClient ? TheGameClient->getRenderedObjectCount() : 0,
				m_background ? 1 : 0,
				cameraTarget.x, cameraTarget.y, cameraTarget.z,
				cameraPosition.x, cameraPosition.y, cameraPosition.z,
				cameraDirection.x, cameraDirection.y, cameraDirection.z,
				cameraAngle, cameraPitch, cameraZoom);
			lastShellMapHealthLog = now;
		}
#endif

		// mark last time we ran the updates
		lastUpdate = now;

	}

}

//-------------------------------------------------------------------------------------------------
void Shell::queueShellMapRefresh()
{
#if defined(__APPLE__)
	const UnsignedInt now = timeGetTime();
	// Display transitions synthesize a focus regain while their device reset is
	// settling. Restarting ShellMap here races UI reflow and menu animations.
	if (s_gxLastShellResolutionChangeTime != 0 &&
		(Int)(now - s_gxLastShellResolutionChangeTime) >= 0 &&
		now - s_gxLastShellResolutionChangeTime < 1000)
	{
		fprintf(stderr, "INFO: GX ignored shell map focus refresh during display transition\n");
		return;
	}
	if (m_shellMapOn && m_isShellActive)
	{
		m_shellMapRefreshTime = now + 250;
		fprintf(stderr, "INFO: GX queued shell map refresh after focus restore\n");
	}
#endif
}

//-------------------------------------------------------------------------------------------------
void Shell::onResolutionChanged()
{
#if defined(__APPLE__)
	s_gxLastShellResolutionChangeTime = timeGetTime();
	// A focus event can precede the resize notification. It belongs to the same
	// display transition and must not tear down/restart the live shell map.
	m_shellMapRefreshTime = 0;
#endif
	// Shell animations cache absolute rest positions. Restore and forget them
	// before the existing scripted windows are reflowed at the new resolution.
	if (m_animateWindowManager)
		m_animateWindowManager->reset();
}

//-------------------------------------------------------------------------------------------------
namespace
{
	struct ScreenInfo
	{
		ScreenInfo() : isHidden(false) {}
		AsciiString filename;
		bool isHidden;
	};
}

//-------------------------------------------------------------------------------------------------
void Shell::recreateWindowLayouts()
{
		// collect state of the current shell
	const Int screenCount = getScreenCount();
	std::vector<ScreenInfo> screenStackInfos;

	{
		screenStackInfos.resize(screenCount);
		Int screenIndex = 0;
		for (; screenIndex < screenCount; ++screenIndex)
		{
			const WindowLayout* layout = getScreenLayout(screenIndex);
			ScreenInfo& screenInfo = screenStackInfos[screenIndex];
			screenInfo.filename = layout->getFilename();
			screenInfo.isHidden = layout->isHidden();
		}
	}

	// reconstruct the shell now
	deconstruct();
	construct();
	init();

	// restore the screen stack
	Int screenIndex = 0;
	for (; screenIndex < screenCount; ++screenIndex)
	{
		const ScreenInfo& screenInfo = screenStackInfos[screenIndex];
		push(screenInfo.filename);

		WindowLayout* layout = getScreenLayout(screenIndex);
		layout->hide(screenInfo.isHidden);
	}
}

//-------------------------------------------------------------------------------------------------
/** Find a screen via the .wnd script filename loaded */
//-------------------------------------------------------------------------------------------------
WindowLayout *Shell::findScreenByFilename( AsciiString filename )
{

	if (filename.isEmpty())
		return nullptr;

	// search screen list
	WindowLayout *screen;
	Int i;
	for( i = 0; i < MAX_SHELL_STACK; i++ )
	{

		screen = m_screenStack[ i ];
		if( screen && filename.compareNoCase(screen->getFilename()) == 0 )
			return screen;

	}

	return nullptr;

}

//-------------------------------------------------------------------------------------------------
WindowLayout *Shell::getScreenLayout( Int index ) const
{
	if (index >= 0 && index < m_screenCount)
		return m_screenStack[index];

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Hide or unhide all window layouts loaded */
//-------------------------------------------------------------------------------------------------
void Shell::hide( Bool hide )
{
	Int i;

	for( i = 0; i < MAX_SHELL_STACK; i++ )
		if( m_screenStack[ i ] )
			m_screenStack[ i ]->hide( hide );

	if (TheIMEManager)
		TheIMEManager->detach();

}

//-------------------------------------------------------------------------------------------------
/** Push layout onto shell */
//-------------------------------------------------------------------------------------------------
void Shell::push( AsciiString filename, Bool shutdownImmediate )
{
	// GeneralsX @feature BenderAI 18/02/2026 Debug logging for shell push
	fprintf(stderr, "DEBUG: Shell::push() called with filename='%s'\n", filename.str());
	fflush(stderr);

	// sanity
	if( filename.isEmpty() )
		return;
	if(TheGameSpyInfo)
			GameSpyCloseAllOverlays();


#ifdef DEBUG_LOGGING
	DEBUG_LOG(("Shell:push(%s) - stack was", filename.str()));
	for (Int i=0; i<m_screenCount; ++i)
	{
		DEBUG_LOG(("\t\t%s", m_screenStack[i]->getFilename().str()));
	}
#endif

	// make sure we have an available spot for another screen
	if( m_screenCount >= MAX_SHELL_STACK )
	{

		DEBUG_LOG(( "Unable to load screen '%s', max '%d' reached",
								filename.str(), MAX_SHELL_STACK ));
		fprintf(stderr, "DEBUG: Shell::push() failed - max stack reached\n");
		fflush(stderr);
		return;

	}

	// set a push as pending with the layout name passed in
	m_pendingPush = TRUE;
	m_pendingPushName = filename;
	fprintf(stderr, "DEBUG: Shell::push() marked as pending, will load '%s' next frame\n", filename.str());
	fflush(stderr);

	// get the top of the current stack
	WindowLayout *currentTop = top();

	//
	// if we have something on the top of the stack we won't do the push
	// right now, we will instead shutdown the top, and when the top tells
	// us it's done shutting down (via the shutdownComplete() method) we do
	// the push then
	//
	if( currentTop && !currentTop->isHidden() )
	{

		// run the shutdown
		currentTop->runShutdown( &shutdownImmediate );

	}
	else
	{

		// just call shutdownComplete() which will immediately cause the push to happen
		shutdownComplete( nullptr );

	}

//	if (TheIMEManager)
//		TheIMEManager->detach();

}

//-------------------------------------------------------------------------------------------------
/** Pop top layout of the stack.  Note that we don't actually do the pop right here,
	* we instead run the layout shutdown.  That shutdown() in turn notifies the
	* shell when the shutdown is complete and at that point we do the actual pop */
//-------------------------------------------------------------------------------------------------
void Shell::pop()
{
	WindowLayout *screen = top();
	if(TheGameSpyInfo)
			GameSpyCloseAllOverlays();


	// sanity
	if( screen == nullptr )
		return;

#ifdef DEBUG_LOGGING
	DEBUG_LOG(("Shell:pop() - stack was"));
	for (Int i=0; i<m_screenCount; ++i)
	{
		DEBUG_LOG(("\t\t%s", m_screenStack[i]->getFilename().str()));
	}
#endif

	// set a pop as pending
	m_pendingPop = TRUE;

	//
	// run the shutdown function for the screen, when it's actually shutdown it
	// will call Shell::shutdownComplete(), where the pending pop will be seen
	// and the actual pop will occur
	//
	Bool immediatePop = FALSE;
	screen->runShutdown( &immediatePop );

	if (TheIMEManager)
		TheIMEManager->detach();

}

//-------------------------------------------------------------------------------------------------
/** When you need to immediately pop a screen off the stack use this method.  It
	* gives the screen the opportunity to shutdown and tells the shutdown
	* method that an immediate pop is going to take place.  When control returns
	* from the shutdown() for the screen, it will be immediately popped off
	* the stack */
//-------------------------------------------------------------------------------------------------
void Shell::popImmediate()
{
	WindowLayout *screen = top();

	// sanity
	if( screen == nullptr )
		return;

#ifdef DEBUG_LOGGING
	DEBUG_LOG(("Shell:popImmediate() - stack was"));
	for (Int i=0; i<m_screenCount; ++i)
	{
		DEBUG_LOG(("\t\t%s", m_screenStack[i]->getFilename().str()));
	}
#endif

	// do NOT set pending pop, we are going to force a pop after the shutdown is run
	m_pendingPop = FALSE;

	// run the shutdown
	Bool immediatePop = TRUE;
	screen->runShutdown( &immediatePop );

	// pop the screen of the stack
	doPop( FALSE );

	if (TheIMEManager)
		TheIMEManager->detach();

}

//-------------------------------------------------------------------------------------------------
/** Run the initialize function for the top of the stack just as though it was pushed
	* on the stack.  We want this behavior when we want to act like the top was just
	* pushed on the stack, but it's already there (ie going from in game back to the
	* pre-game shell menus */
//-------------------------------------------------------------------------------------------------
void Shell::showShell( Bool runInit )
{
	DEBUG_LOG(("Shell:showShell() - %s (%s)", TheGlobalData->m_initialFile.str(), (top())?top()->getFilename().str():"no top screen"));

	if(!TheGlobalData->m_initialFile.isEmpty() || !TheGlobalData->m_simulateReplays.empty())
	{
		return;
	}

	// runInit is used if we want show shell to run
	if(runInit)
	{
		WindowLayout *layout = top();

		if( layout )
		{
			layout->runInit( nullptr );
		//	layout->bringForward();
		}
	}
	// @todo remove this hack
//	TheGlobalData->m_inGame = FALSE;
	// add in the background stuff

//	if(TheGlobalData->m_shellMapOn)
	//	{
	//		if( top() )
	//			top()->hide(TRUE);
	//		m_background = TheWindowManager->winCreateLayout("Menus/BlankWindow.wnd");
	//		DEBUG_ASSERTCRASH(m_background,("We Couldn't Load Menus/BlankWindow.wnd"));
	//		m_background->hide(FALSE);
	//		m_background->bringForward();
	//		if (TheGameLogic->isInGame())
	//				TheMessageStream->appendMessage( GameMessage::MSG_CLEAR_GAME_DATA );
	//
	//		TheGlobalData->m_pendingFile = TheGlobalData->m_shellMapName;
	//		GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_NEW_GAME );
	//		msg->appendIntegerArgument(GAME_SHELL);
	//	}
	//	else
	//	{
	//
	//		m_background = TheWindowManager->winCreateLayout("Menus/BlankWindow.wnd");
	//
	//		DEBUG_ASSERTCRASH(m_background,("We Couldn't Load Menus/BlankWindow.wnd"));
	//		m_background->hide(FALSE);
	//		if (top())
	//			top()->bringForward();
	//
	//	}


	if (!TheGlobalData->m_shellMapOn && m_screenCount == 0)
  {
#ifdef RTS_PROFILE_LEGACY
    Profile::StopRange("init");
#endif
	//else
		push( "Menus/MainMenu.wnd" );
  }
	m_isShellActive = TRUE;
}

void Shell::showShellMap(Bool useShellMap )
{
	// we don't want any of this to show if we're loading straight into a file
	if (TheGlobalData->m_initialFile.isNotEmpty() || !TheGameLogic || !TheGlobalData->m_simulateReplays.empty())
	{
		return;
	}
	if(useShellMap && TheGlobalData->m_shellMapOn)
	{
		fprintf(stderr,
			"INFO: GX shell map request use=1 local_on=%d in_game=%d mode=%d shell_active=%d\n",
			m_shellMapOn ? 1 : 0,
			TheGameLogic->isInGame() ? 1 : 0,
			(int)TheGameLogic->getGameMode(),
			m_isShellActive ? 1 : 0);

		// Only trust GAME_SHELL when the Shell itself still owns a live shell-map
		// session. Entering a real match explicitly clears m_shellMapOn below; if
		// the game mode later remains stale, tear it down and rebuild the map.
		if(TheGameLogic->isInGame() && TheGameLogic->getGameMode() == GAME_SHELL && m_shellMapOn)
		{
			return;
		}
		// we're in some other kind of game, clear it out foo!
		if(TheGameLogic->isInGame())
		{
			TheGameLogic->exitGame();
		}
		TheWritableGlobalData->m_pendingFile = TheGlobalData->m_shellMapName;
		InitRandom(0);
		GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_NEW_GAME );
		msg->appendIntegerArgument(GAME_SHELL);
		m_shellMapOn = TRUE;
		fprintf(stderr, "INFO: GX shell map rebuild queued map=%s\n", TheGlobalData->m_shellMapName.str());
	}
	else
	{
		// we're in a shell game, stop it!
		if(TheGameLogic->isInGame() && TheGameLogic->getGameMode() == GAME_SHELL)
		{
			TheGameLogic->exitGame();
		}

		// if the shell is active,we need a background
		if(!m_isShellActive)
		{
			return;
		}
		if(!m_background)
		{
			m_background = TheWindowManager->winCreateLayout("Menus/BlankWindow.wnd");
		}

		DEBUG_ASSERTCRASH(m_background,("We Couldn't Load Menus/BlankWindow.wnd"));
		m_background->getFirstWindow()->winSetStatus(WIN_STATUS_IMAGE);
		m_background->hide(FALSE);
		if (top())
			top()->bringForward();
		m_shellMapOn = FALSE;
		m_clearBackground = FALSE;
	}
}

//-------------------------------------------------------------------------------------------------
/** Run the shutdown() function for the top of the stack just like we're going to pop
	* it off but DO NOT pop it off the stack.  We want this behavior when leaving the
	* pre-game menus and entering the game and want the shell to still exist and contain
	* the stack information but don't want it to go away */
//-------------------------------------------------------------------------------------------------
void Shell::hideShell()
{
	// If we have the 3d background running, mark it to close
	m_clearBackground = TRUE;
	// A non-shell game is about to take ownership of GameLogic. Do not let a
	// later stale GAME_SHELL value convince showShellMap() that the old 3D
	// background survived the reset.
	m_shellMapOn = FALSE;

	DEBUG_LOG(("Shell:hideShell() - %s", (top())?top()->getFilename().str():"no top screen"));

	WindowLayout *layout = top();

	if( layout )
	{
		Bool immediatePop = TRUE;

		layout->runShutdown( &immediatePop );

	}

	if (TheIMEManager)
		TheIMEManager->detach();

	// Mark that the shell is no longer up.
	m_isShellActive = FALSE;

}

//-------------------------------------------------------------------------------------------------
/** Return the top layout on the stack */
//-------------------------------------------------------------------------------------------------
WindowLayout *Shell::top()
{

	// empty stack
	if( m_screenCount == 0 )
		return nullptr;

	// top layout is at count index
	return m_screenStack[ m_screenCount - 1 ];

}

// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------------------------
/** Add screen to our list */
//-------------------------------------------------------------------------------------------------
void Shell::linkScreen( WindowLayout *screen )
{

	// sanity
	if( screen == nullptr )
		return;

	// check to see if at top already
	if( m_screenCount == MAX_SHELL_STACK )
	{

		DEBUG_CRASH(( "No room in shell stack for screen" ));
		return;

	}

	// add to array at top index
	m_screenStack[ m_screenCount++ ] = screen;

}

//-------------------------------------------------------------------------------------------------
/** Remove screen from our list */
//-------------------------------------------------------------------------------------------------
void Shell::unlinkScreen( WindowLayout *screen )
{

	// sanity
	if( screen == nullptr )
		return;

	DEBUG_ASSERTCRASH( m_screenStack[ m_screenCount - 1 ] == screen,
										 ("Screen not on top of stack") );

	// remove reference to screen and decrease count
	if( m_screenStack[ m_screenCount - 1 ] == screen )
		m_screenStack[ --m_screenCount ] = nullptr;

}

//-------------------------------------------------------------------------------------------------
/** Actually do the work for a push */
//-------------------------------------------------------------------------------------------------
void Shell::doPush( AsciiString layoutFile )
{
	// GeneralsX @feature BenderAI 18/02/2026 Debug logging - actually pushing layout
	fprintf(stderr, "DEBUG: Shell::doPush() called with layoutFile='%s'\n", layoutFile.str());
	fflush(stderr);

	if(TheGameSpyInfo)
			GameSpyCloseAllOverlays();
	WindowLayout *newScreen;

	// create new layout and load from window manager
	fprintf(stderr, "DEBUG: About to call TheWindowManager->winCreateLayout('%s')\n", layoutFile.str());
	fflush(stderr);
	newScreen = TheWindowManager->winCreateLayout( layoutFile );
	fprintf(stderr, "DEBUG: winCreateLayout returned: %p\n", newScreen);
	fflush(stderr);
	
	DEBUG_ASSERTCRASH( newScreen != nullptr, ("Shell unable to load pending push layout") );

	// link screen to the top
	linkScreen( newScreen );

	if (TheIMEManager)
		TheIMEManager->detach();

	// run the init function automatically
	newScreen->runInit( nullptr );
	newScreen->bringForward();

	fprintf(stderr, "DEBUG: Shell::doPush() completed successfully\n");
	fflush(stderr);
}

//-------------------------------------------------------------------------------------------------
/** Actually do the work for a pop */
//-------------------------------------------------------------------------------------------------
void Shell::doPop( Bool impendingPush )
{
	WindowLayout *currentTop = top();

	// there better be a top of the stack since we're popping
	DEBUG_ASSERTCRASH( currentTop, ("Shell: No top of stack and we want to pop!") );

	if (currentTop)
	{
		// remove this screen from our list
		unlinkScreen(currentTop);

		// delete all the windows in the screen
		currentTop->destroyWindows();

		// release the screen object back to the memory pool
		deleteInstance(currentTop);
	}

	// run the init for the new top of the stack if present
	WindowLayout *newTop = top();
	if( newTop && !impendingPush )
	{
		newTop->runInit( nullptr );
		//newTop->bringForward();
	}

	if (TheIMEManager)
		TheIMEManager->detach();

}

//-------------------------------------------------------------------------------------------------
/** This is called when a layout has finished its shutdown process.  Layouts are
	* shutdown when a new screen is being pushed on the stack, or when we are
	* popping the current screen off the top of the stack.  It is here that we
	* can look for any pending push or pop operations and actually do them
	*
	* NOTE: It is possible for the screen parameter to be nullptr when we are
	*       short circuiting the shutdown logic because there is no layout
	*				to actually shutdown (ie, the stack is empty and we push) */
//-------------------------------------------------------------------------------------------------
void Shell::shutdownComplete( WindowLayout *screen, Bool impendingPush )
{

	// there should never be a pending push AND pop operation
	DEBUG_ASSERTCRASH( m_pendingPush == FALSE || m_pendingPop == FALSE,
										 ("There is a pending push AND pop in the shell.  Not allowed!") );

	// Reset the AnimateWindowManager
	m_animateWindowManager->reset();

	// check for pending push or pop
	if( m_pendingPush )
	{
		// GeneralsX @feature BenderAI 18/02/2026 Debug logging - pending push being processed
		fprintf(stderr, "DEBUG: Shell::update() - Processing pending push: '%s'\n", m_pendingPushName.str());
		fflush(stderr);

		// do the push
		doPush( m_pendingPushName );

		// no more pending pushy for you!
		m_pendingPush = FALSE;
		m_pendingPushName.set( "" );
		fprintf(stderr, "DEBUG: Shell::update() - Push completed\n");
		fflush(stderr);

	}
	else if( m_pendingPop )
	{

		// do the pop
		doPop( impendingPush );

		// no more pending pop for you!
		m_pendingPop = FALSE;

	}

	if(m_clearBackground)
	{
		if(m_background)
		{
			m_background->destroyWindows();
			deleteInstance(m_background);
			m_background = nullptr;
			m_clearBackground = FALSE;
		}

	}

}


void Shell::registerWithAnimateManager( GameWindow *win, AnimTypes animType, Bool needsToFinish, UnsignedInt delayMS)
{
	if(!m_animateWindowManager)
	{
		DEBUG_CRASH(("We called registerWithAnimateManager and we don't have an Animate Manager created"));
		return;
	}
	if (TheGlobalData->m_animateWindows)
		m_animateWindowManager->registerGameWindow(win,animType,needsToFinish, 500,delayMS);
}

Bool Shell::isAnimFinished()
{
	// check the new way also.
	if (!TheTransitionHandler->isFinished())
		return FALSE;

	if(!m_animateWindowManager)
	{
		DEBUG_CRASH(("We called registerWithAnimateManager and we don't have an Animate Manager created"));
		return TRUE;
	}
	if (TheGlobalData->m_animateWindows)
		return m_animateWindowManager->isFinished();
	else
		return TRUE;
}

void Shell::reverseAnimatewindow()
{
	if(!m_animateWindowManager)
	{
		DEBUG_CRASH(("We called registerWithAnimateManager and we don't have an Animate Manager created"));
		return;
	}
	if (TheGlobalData->m_animateWindows)
		m_animateWindowManager->reverseAnimateWindow();
}

Bool Shell::isAnimReversed()
{
	if(!m_animateWindowManager)
	{
		DEBUG_CRASH(("We called registerWithAnimateManager and we don't have an Animate Manager created"));
		return TRUE;
	}
	if (TheGlobalData->m_animateWindows)
		return m_animateWindowManager->isReversed();
	else
		return TRUE;
}


void Shell::loadScheme( AsciiString name )
{
	if(!m_schemeManager)
		return;

	m_schemeManager->setShellMenuScheme( name );
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
WindowLayout *Shell::getSaveLoadMenuLayout()
{

	// if layout has not been created, create it now
	if( m_saveLoadMenuLayout == nullptr )
   m_saveLoadMenuLayout = TheWindowManager->winCreateLayout( "Menus/PopupSaveLoad.wnd" );

	// sanity
	DEBUG_ASSERTCRASH( m_saveLoadMenuLayout, ("Unable to create save/load menu layout") );

	// return the layout
	return m_saveLoadMenuLayout;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
WindowLayout *Shell::getPopupReplayLayout()
{

	// if layout has not been created, create it now
	if( m_popupReplayLayout == nullptr )
   m_popupReplayLayout = TheWindowManager->winCreateLayout( "Menus/PopupReplay.wnd" );

	// sanity
	DEBUG_ASSERTCRASH( m_popupReplayLayout, ("Unable to create replay save menu layout") );

	// return the layout
	return m_popupReplayLayout;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
WindowLayout *Shell::getOptionsLayout( Bool create )
{
	// if layout has not been created, create it now
	if ((m_optionsLayout == nullptr) && (create == TRUE))
	{
		m_optionsLayout = TheWindowManager->winCreateLayout( "Menus/OptionsMenu.wnd" );

		// sanity
		DEBUG_ASSERTCRASH( m_optionsLayout, ("Unable to create options menu layout") );
	}

	// return the layout
	return m_optionsLayout;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Shell::destroyOptionsLayout() {
	if (m_optionsLayout != nullptr) {
		m_optionsLayout->destroyWindows();
		deleteInstance(m_optionsLayout);
		m_optionsLayout = nullptr;
	}
}
