#pragma once
#include <cmath>

namespace Pakal::TopDown
{
    // The follow camera sits at a fixed offset in level coords: side = west (-x), back =
    // south (-y). It looks toward the player, i.e. to the northeast when both are positive.
    // Screen "up" is away from the camera (northeast); screen "right" is southeast. The
    // rotation that maps screen input (right=+x, up=+y) to level axes is atan2(side, back):
    // for the original camera (-12.5, +12.5) it gives -45°, so W goes northeast and D goes
    // southeast. The angle depends only on the offset, not on where the player stands.
    inline float camera_input_angle(float sideOffsetMeters, float backOffsetMeters)
    {
        return std::atan2(sideOffsetMeters, backOffsetMeters);
    }

    // Maps a screen-space input (right = +x, up = +y) to level axes.
    inline void camera_relative(float inX, float inY, float angle, float& outX, float& outY)
    {
        const float c = std::cos(angle), s = std::sin(angle);
        outX = inX * c - inY * s;
        outY = inX * s + inY * c;
    }
}
