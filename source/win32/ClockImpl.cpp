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
#include <windows.h>
#include <chrono>
#include <thread>


namespace
{
    LARGE_INTEGER getFrequency()
    {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        return frequency;
    }
}

namespace Pakal
{
namespace priv
{
////////////////////////////////////////////////////////////
//Time ClockImpl::getCurrentTime()
//{
//	/*
//    // Force the following code to run on first core
//    // (see http://msdn.microsoft.com/en-us/library/windows/desktop/ms644904(v=vs.85).aspx)
//    HANDLE currentThread = GetCurrentThread();
//    DWORD_PTR previousMask = SetThreadAffinityMask(currentThread, 1);
//
//    // Get the frequency of the performance counter
//    // (it is constant across the program lifetime)
//    static LARGE_INTEGER frequency = getFrequency();
//
//    // Get the current time
//    LARGE_INTEGER time;
//    QueryPerformanceCounter(&time);
//
//    // Restore the thread affinity
//    SetThreadAffinityMask(currentThread, previousMask);
//
//    // Return the current time as microseconds
//    return Pakal::microseconds(1000000 * time.QuadPart / frequency.QuadPart);
//	*/
//	auto currenttime = std::chrono::system_clock::now();
//	return Pakal::microseconds(std::chrono::duration_cast< std::chrono::microseconds >( currenttime.time_since_epoch() ).count());
//}

} // namespace priv

} // namespace Pakal
