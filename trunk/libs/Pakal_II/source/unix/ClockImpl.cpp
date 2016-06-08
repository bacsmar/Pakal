////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2015 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "ClockImpl.h"
#if defined(PAKAL_APPLE_PLATFORM) || defined(PAKAL_IPHONE_PLATFORM)
    #include <mach/mach_time.h>
#else
    #include <time.h>
#endif
#include <pthread.h>
#include <chrono>


namespace Pakal
{
namespace priv
{
////////////////////////////////////////////////////////////
Time ClockImpl::getCurrentTime()
{
#if defined(PAKAL_APPLE_PLATFORM) || defined(PAKAL_IPHONE_PLATFORM)

    // Mac OS X specific implementation (it doesn't support clock_gettime)
    static mach_timebase_info_data_t frequency = {0, 0};
    if (frequency.denom == 0)
        mach_timebase_info(&frequency);
    unsigned long long nanoseconds = mach_absolute_time() * frequency.numer / frequency.denom;
    return Pakal::microseconds(nanoseconds / 1000);

#else

    // POSIX implementation
	
	//clockid_t cid;
	//pthread_getcpuclockid(pthread_self(), &cid);
	
	timespec time;
	//clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time);
	//clock_gettime(CLOCK_MONOTONIC, &time);
	//clock_gettime(CLOCK_REALTIME, &time);
	clock_gettime(CLOCK_MONOTONIC_COARSE, &time);
	//clock_gettime( cid, &time);
	return Pakal::microseconds(static_cast<unsigned long long>(time.tv_sec) * 1000000 + time.tv_nsec * 0.001);
	
	//auto currenttime = std::chrono::high_resolution_clock::now();
	//auto currenttime = std::chrono::steady_clock::now();
	//auto currenttime = std::chrono::system_clock::now();
	//return Pakal::microseconds(std::chrono::duration_cast< std::chrono::microseconds >(currenttime.time_since_epoch()).count());
#endif
}

} // namespace priv

} // namespace sf
