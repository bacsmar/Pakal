#pragma once

#include "Config.h"
#include "PakalTime.h"
#include <chrono>


namespace Pakal
{
namespace priv
{
class ClockImpl
{
public:
    static Time getCurrentTime()
    {
		auto currenttime = std::chrono::system_clock::now();
		return Pakal::microseconds(std::chrono::duration_cast< std::chrono::microseconds >(currenttime.time_since_epoch()).count());
    }
};

} // namespace priv

} // namespace Pakal