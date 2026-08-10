/* SPDX-License-Identifier: MIT OR GPL-2.0-or-later */
#include "types_compat.h"
#include "time_compat.h"

#include <time.h>

// GeneralsX @feature BenderAI 24/02/2026 Phase 5 macOS timer support (CLOCK_BOOTTIME -> CLOCK_UPTIME)
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

namespace
{
uint64_t getRealTimeNanoseconds()
{
#ifdef __APPLE__
  static mach_timebase_info_data_t timebase = { 0, 0 };
  if (timebase.denom == 0)
    mach_timebase_info(&timebase);
  const uint64_t elapsed = mach_absolute_time();
  return elapsed * timebase.numer / timebase.denom;
#elif defined(__linux__)
  struct timespec ts;
  clock_gettime(CLOCK_BOOTTIME, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + ts.tv_nsec;
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + ts.tv_nsec;
#endif
}

}

uint64_t GeneralsXGetRealTimeMilliseconds(void)
{
  return getRealTimeNanoseconds() / 1000000ULL;
}

// GeneralsX @bugfix 26/07/2026 timeGetTime() is real time again.
//
// This used to return a scaled "game clock" so that a speed control could stretch the legacy Win32
// millisecond timers. That was the wrong lever. timeGetTime() is read by hundreds of unrelated call
// sites -- FFmpeg video presentation, menu fades, network timeouts, profiling -- and every one of
// them means wall-clock milliseconds. Scaling it made in-engine video play at the wrong rate and
// desync from its audio, and silently skewed every real-time gate in the engine. Simulation speed
// belongs to the logic step cadence (FramePacer::setLogicTimeScaleFps), which changes how many
// fixed-size logic steps run per second without lying to anybody about what time it is.
DWORD timeGetTime(void)
{
  return static_cast<DWORD>(GeneralsXGetRealTimeMilliseconds());
}

DWORD GetTickCount(void)
{
  return timeGetTime();
}

void Sleep(DWORD ms)
{
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000;
  nanosleep(&ts, NULL);
}

void GetLocalTime(SYSTEMTIME* st)
{
	time_t t = time(NULL);
	struct tm* tm = localtime(&t);
	st->wYear = tm->tm_year + 1900;
	st->wMonth = tm->tm_mon + 1;
	st->wDayOfWeek = tm->tm_wday;
	st->wDay = tm->tm_mday;
	st->wHour = tm->tm_hour;
	st->wMinute = tm->tm_min;
	st->wSecond = tm->tm_sec;
	st->wMilliseconds = 0;
}
