#pragma once
#include <cmath>

namespace Pakal::TopDown
{
    // Kit characters face +Z. bx::mtxSRT rotates that axis to (-sin(yaw), cos(yaw)).
    inline float actor_yaw(float renderX, float renderZ)
    {
        return std::atan2(-renderX, renderZ);
    }
}
