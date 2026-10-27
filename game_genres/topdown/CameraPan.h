#pragma once
#include <algorithm>
#include <array>

namespace Pakal::TopDown
{
    struct CameraPan
    {
        using Point = std::array<float, 3>;
        Point fromEye{}, toEye{}, fromTarget{}, toTarget{};
        double startedMs = 0.0;
        double durationMs = 0.0;
        bool active = false;

        void sample(double nowMs, Point& eye, Point& target) const
        {
            const float t = durationMs > 0.0
                ? static_cast<float>(std::clamp((nowMs - startedMs) / durationMs, 0.0, 1.0)) : 1.0f;
            for (int i = 0; i < 3; ++i) {
                eye[i] = fromEye[i] + (toEye[i] - fromEye[i]) * t;
                target[i] = fromTarget[i] + (toTarget[i] - fromTarget[i]) * t;
            }
        }
    };
}
