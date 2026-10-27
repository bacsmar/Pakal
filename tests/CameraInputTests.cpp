#include "topdown/CameraInput.h"
#include <cmath>
#include <cstdio>

int main()
{
    int failures = 0;
    // Camera sits west and south of the player (side -12.5, back +12.5) looking northeast.
    // Screen "up" = away from camera = northeast in level coords (+x,+y); "right" = southeast (+x,-y).
    const float angle = Pakal::TopDown::camera_input_angle(-12.5f, 12.5f);

    struct Case { int inX, inY; float wantX, wantY; const char* name; };
    const Case cases[] = {
        { 0, 1,  1.0f / std::sqrt(2.0f),  1.0f / std::sqrt(2.0f), "up -> northeast (away from camera)" },
        { 0, -1, -1.0f / std::sqrt(2.0f), -1.0f / std::sqrt(2.0f), "down -> southwest (toward camera)" },
        { 1, 0,  1.0f / std::sqrt(2.0f), -1.0f / std::sqrt(2.0f), "right -> southeast" },
        { -1, 0, -1.0f / std::sqrt(2.0f),  1.0f / std::sqrt(2.0f), "left -> northwest" },
    };

    for (const Case& c : cases)
    {
        float x = 0.0f, y = 0.0f;
        Pakal::TopDown::camera_relative(c.inX, c.inY, angle, x, y);
        const bool ok = std::fabs(x - c.wantX) < 1e-5f && std::fabs(y - c.wantY) < 1e-5f;
        std::printf("[%s] %s: (%d,%d) -> (%.4f, %.4f)\n", ok ? "PASS" : "FAIL", c.name, c.inX, c.inY, x, y);
        if (!ok) ++failures;
    }

    // Diagonals stay unit length (no speed boost on diagonals).
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            if (x == 0 && y == 0) continue;
            float ox = 0.0f, oy = 0.0f;
            Pakal::TopDown::camera_relative(x, y, angle, ox, oy);
            const float len = std::sqrt(ox * ox + oy * oy);
            const bool ok = std::fabs(len - std::sqrt(float(x * x + y * y))) < 1e-5f;
            if (!ok)
            {
                std::printf("[FAIL] diagonal (%d,%d) length %.6f\n", x, y, len);
                ++failures;
            }
        }
    }

    // A different camera offset must give a different angle (the mapping is derived, not hardcoded).
    const float other = Pakal::TopDown::camera_input_angle(-12.5f, 20.0f);
    if (std::fabs(other - angle) < 1e-6f)
    {
        std::printf("[FAIL] camera angle does not depend on the offset\n");
        ++failures;
    }

    return failures == 0 ? 0 : 1;
}
