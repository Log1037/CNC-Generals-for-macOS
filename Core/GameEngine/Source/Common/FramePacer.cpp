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
#include "PreRTS.h"

#include "Common/FramePacer.h"
#include "time_compat.h"

#include "GameClient/View.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/ScriptEngine.h"

#include "GameNetwork/NetworkDefs.h"
#include "GameNetwork/NetworkInterface.h"


FramePacer* TheFramePacer = nullptr;

FramePacer::FramePacer()
{
	// GeneralsX @bugfix BenderAI 11/05/2026 Protect Windows-specific timer API calls
#ifdef _WIN32
	// Set the time slice size to 1 ms.
	timeBeginPeriod(1);
#endif

	m_maxFPS = BaseFps;
	m_logicTimeScaleFPS = LOGICFRAMES_PER_SECOND;
	m_updateTime = 1.0f / (Real)BaseFps; // initialized to something to avoid division by zero on first use
	m_logicStepsThisFrame = 1;
	m_enableFpsLimit = FALSE;
	m_enableLogicTimeScale = FALSE;
	m_isTimeFrozen = FALSE;
	m_isGameHalted = FALSE;
}

FramePacer::~FramePacer()
{
	// GeneralsX @bugfix BenderAI 11/05/2026 Protect Windows-specific timer API calls
#ifdef _WIN32
	// Restore the previous time slice for Windows.
	timeEndPeriod(1);
#endif
}

void FramePacer::update()
{
	// TheSuperHackers @bugfix xezon 05/08/2025 Re-implements the frame rate limiter
	// with higher resolution counters to cap the frame rate more accurately to the desired limit.
	const UnsignedInt maxFps = getActualFramesPerSecondLimit();// allowFpsLimit ? getFramesPerSecondLimit() : RenderFpsPreset::UncappedFpsValue;
	m_updateTime = m_frameRateLimit.wait(maxFps);
}

void FramePacer::setFramesPerSecondLimit( Int fps )
{
	DEBUG_LOG(("FramePacer::setFramesPerSecondLimit() - setting max fps to %d (TheGlobalData->m_useFpsLimit == %d)", fps, TheGlobalData->m_useFpsLimit));
	m_maxFPS = fps;
}

Int FramePacer::getFramesPerSecondLimit()  const
{
	return m_maxFPS;
}

void FramePacer::enableFramesPerSecondLimit( Bool enable )
{
	m_enableFpsLimit = enable;
}

Bool FramePacer::isFramesPerSecondLimitEnabled() const
{
	return m_enableFpsLimit;
}

Bool FramePacer::isActualFramesPerSecondLimitEnabled() const
{
	Bool allowFpsLimit = true;

	if (TheScriptEngine != nullptr)
	{
		// GeneralsX @bugfix 26/07/2026 A scripted time multiplier no longer uncaps the render rate.
		// It is applied to the logic cadence instead (see getTimeMultiplier), so the render limit
		// can stay where the player put it and the fast-forwarded section stays smooth. The debug
		// fast forward still runs the render loop flat out, because that one exists specifically to
		// get through a mission as fast as the machine allows.
		allowFpsLimit &= !TheScriptEngine->isTimeFast();
	}

	if (TheGameLogic != nullptr)
	{
#if defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
		allowFpsLimit &= !(!TheGameLogic->isGamePaused() && TheGlobalData->m_TiVOFastMode);
#else	//always allow this cheat key if we're in a replay game.
		allowFpsLimit &= !(!TheGameLogic->isGamePaused() && TheGlobalData->m_TiVOFastMode && TheGameLogic->isInReplayGame());
#endif
	}

	allowFpsLimit &= TheGlobalData->m_useFpsLimit;
	allowFpsLimit &= isFramesPerSecondLimitEnabled();

	return allowFpsLimit;
}

Int FramePacer::getActualFramesPerSecondLimit() const
{
	return isActualFramesPerSecondLimitEnabled() ? getFramesPerSecondLimit() : RenderFpsPreset::UncappedFpsValue;
}

Real FramePacer::getUpdateTime()  const
{
	return m_updateTime;
}

Real FramePacer::getUpdateFps()  const
{
	return 1.0f / m_updateTime;
}

Real FramePacer::getBaseOverUpdateFpsRatio(Real minUpdateFps)
{
	// Update fps is floored to default 5 fps, 200 ms.
	// Useful to prevent insane ratios on frame spikes/stalls.
	return (Real)BaseFps / std::max(getUpdateFps(), minUpdateFps);
}

void FramePacer::setTimeFrozen(Bool frozen)
{
	m_isTimeFrozen = frozen;
}

void FramePacer::setGameHalted(Bool halted)
{
	m_isGameHalted = halted;
}

Bool FramePacer::isTimeFrozen() const
{
	return m_isTimeFrozen;
}

Bool FramePacer::isGameHalted() const
{
	return m_isGameHalted;
}

void FramePacer::setLogicTimeScaleFps( Int fps )
{
	m_logicTimeScaleFPS = fps;
}

Int FramePacer::getLogicTimeScaleFps() const
{
	return m_logicTimeScaleFPS;
}

void FramePacer::enableLogicTimeScale( Bool enable )
{
	m_enableLogicTimeScale = enable;
}

Bool FramePacer::isLogicTimeScaleEnabled() const
{
	return m_enableLogicTimeScale;
}

void FramePacer::setLogicStepsThisFrame( Int steps )
{
	m_logicStepsThisFrame = max<Int>(0, steps);
}

Int FramePacer::getLogicStepsThisFrame() const
{
	return m_logicStepsThisFrame;
}

Int FramePacer::getActualLogicTimeScaleFps(LogicTimeQueryFlags flags) const
{
	if (m_isTimeFrozen && (flags & IgnoreFrozenTime) == 0)
	{
		return 0;
	}

	if (m_isGameHalted && (flags & IgnoreHaltedGame) == 0)
	{
		return 0;
	}

	if (TheNetwork != nullptr)
	{
		return TheNetwork->getFrameRate();
	}

	if (isLogicTimeScaleEnabled())
	{
		return getLogicTimeScaleFps() * getTimeMultiplier();
	}

	// Returns uncapped value to align with the render update as per the original game behavior.
	return RenderFpsPreset::UncappedFpsValue;
}

Int FramePacer::getTimeMultiplier() const
{
	// GeneralsX @bugfix 26/07/2026 The scripted time multiplier now multiplies the logic cadence.
	//
	// SET_TIME_MULTIPLIER used to be implemented in W3DDisplay::draw: it uncapped the render frame
	// rate and then drew only 1 of every N frames, relying on "logic runs once per rendered frame"
	// to make the simulation N times faster while the screen still refreshed at a normal rate. The
	// simulation is driven by a fixed step accumulator now, so uncapping the render rate no longer
	// speeds anything up -- that implementation would just throw away 2 of every 3 frames and leave
	// the game running at 1x. Multiplying the logic cadence here does what the script asked for and
	// leaves the render rate alone, which also means the fast-forwarded section stays smooth
	// instead of dropping to a third of the display rate.
	if (TheNetwork != nullptr)
	{
		// Never desync a network match over a client side script.
		return 1;
	}

	if (TheTacticalView == nullptr)
	{
		return 1;
	}

	return max<Int>(1, TheTacticalView->getTimeMultiplier());
}

Real FramePacer::getActualLogicTimeScaleRatio(LogicTimeQueryFlags flags) const
{
	return (Real)getActualLogicTimeScaleFps(flags) / LOGICFRAMES_PER_SECONDS_REAL;
}

Real FramePacer::getActualLogicTimeScaleOverFpsRatio(LogicTimeQueryFlags flags) const
{
	// GeneralsX @bugfix 26/07/2026 No longer clamped to 1.
	//
	// This ratio answers "how much logic time passes during one render frame", and every client
	// side animation multiplies its per-frame step by it. Upstream clamps it to 1 because the main
	// loop used to run at most one logic step per render frame, so logic could never outpace the
	// render loop. The macOS port runs a proper fixed step accumulator (see
	// FramePacer::getLogicStepsThisFrame and GameEngine::canUpdateRegularGameLogic), so at a 20 fps
	// render cap with a 30 Hz simulation the correct answer is 1.5, not 1. Clamping made client
	// animations run slower than the units they are attached to during scripted cinematics.
	//
	// Still bounded by MaxRatio so a render stall cannot hand animations an enormous time step;
	// the logic step loop applies the same bound to its iteration count.
	const Real MaxRatio = 8.0f;

	const Int logicFps = getActualLogicTimeScaleFps(flags);
	if (logicFps <= 0)
	{
		return 0.0f;
	}

	if (!isLogicTimeScaleEnabled() && TheNetwork == nullptr)
	{
		// Logic is bound to the render update, exactly one logic step per render frame.
		return 1.0f;
	}

	return min(MaxRatio, (Real)logicFps / getUpdateFps());
}

Real FramePacer::getLogicTimeStepSeconds(LogicTimeQueryFlags flags) const
{
	return SECONDS_PER_LOGICFRAME_REAL * getActualLogicTimeScaleOverFpsRatio(flags);
}

Real FramePacer::getLogicTimeStepMilliseconds(LogicTimeQueryFlags flags) const
{
	return MSEC_PER_LOGICFRAME_REAL * getActualLogicTimeScaleOverFpsRatio(flags);
}
