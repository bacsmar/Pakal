#pragma once

#include "Config.h"
#include "PakalTime.h"
#include <chrono>
#include <windows.h>
#include <time.h>


namespace Pakal
{
namespace priv
{
	struct ClockInitializer
	{
		LARGE_INTEGER HighPerformanceFreq;
		BOOL HighPerformanceTimerSupport = true;
		BOOL MultiCore = false;
		bool usePerformanceTimer = true;

		ClockInitializer()
		{
			SYSTEM_INFO sysinfo;
			GetSystemInfo(&sysinfo);
			MultiCore = (sysinfo.dwNumberOfProcessors > 1);
			if (usePerformanceTimer)
				HighPerformanceTimerSupport = QueryPerformanceFrequency(&HighPerformanceFreq);
			else
				HighPerformanceTimerSupport = FALSE;			
		}
	};
class ClockImpl
{	
public:
    static Time getCurrentTime()
    {
		static ClockInitializer clock;		

		// based on irrlicht timer.
		if (clock.HighPerformanceTimerSupport)
		{
			// Avoid potential timing inaccuracies across multiple cores by
			// temporarily setting the affinity of this process to one core.
			DWORD_PTR affinityMask = 0;
			if (clock.MultiCore)
				affinityMask = SetThreadAffinityMask(GetCurrentThread(), 1);
			LARGE_INTEGER nTime;
			BOOL queriedOK = QueryPerformanceCounter(&nTime);


			// Restore the true affinity.
			if (clock.MultiCore)
				(void)SetThreadAffinityMask(GetCurrentThread(), affinityMask);
			if (queriedOK)
				return Pakal::microseconds(unsigned((nTime.QuadPart) * 1000 / clock.HighPerformanceFreq.QuadPart) * 1000);

		}

		return Pakal::microseconds(GetTickCount()*1000);
    }
};

} // namespace priv

} // namespace Pakal