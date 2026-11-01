#pragma once
#include <algorithm>
#include <array>
#include <vector>

namespace Pakal::TopDown
{
    // A scripted camera move: a straight one from one eye and target to another, or (with 'keys') a path through
    // timed shots, eased as a whole and smooth through each of them (Catmull-Rom), each with its field of view
    struct CameraPan
    {
        using Point = std::array<float, 3>;
        Point fromEye{}, toEye{}, fromTarget{}, toTarget{};
        double startedMs = 0.0;
        double durationMs = 0.0;
        bool active = false;
        // How far the camera sees meanwhile (meters; 0: as in play). A shot low over the floor looks across the whole level:
        // a near far plane leaves the distance out instead of drawing all of it (a phone's frame rate at a mission's end)
        float farMeters = 0.0f;

        struct Key
        {
            Point eye{}, target{};
            float fovY = 0.0f;   // degrees; 0: the rig's pan field of view
            float at = 0.0f;     // when, as a share of the duration (the first 0, the last 1)
        };
        std::vector<Key> keys;

        void sample(double nowMs, Point& eye, Point& target) const
        {
            float fovY = 0.0f;
            sample(nowMs, eye, target, fovY);
        }

        void sample(double nowMs, Point& eye, Point& target, float& fovY) const
        {
            float t = durationMs > 0.0
                ? static_cast<float>(std::clamp((nowMs - startedMs) / durationMs, 0.0, 1.0)) : 1.0f;
            fovY = 0.0f;
            if (keys.size() < 2) {
                for (int i = 0; i < 3; ++i) {
                    eye[i] = fromEye[i] + (toEye[i] - fromEye[i]) * t;
                    target[i] = fromTarget[i] + (toTarget[i] - fromTarget[i]) * t;
                }
                return;
            }
            // eased in and out as a whole: it starts and arrives gently, and does not stop at the shots between
            t = t * t * (3.0f - 2.0f * t);
            size_t k = 0;
            while (k + 2 < keys.size() && t > keys[k + 1].at) ++k;
            const Key& a = keys[k];
            const Key& b = keys[k + 1];
            const Key& before = keys[k > 0 ? k - 1 : k];
            const Key& after = keys[k + 2 < keys.size() ? k + 2 : k + 1];
            const float span = std::max(1e-4f, b.at - a.at);
            const float u = std::clamp((t - a.at) / span, 0.0f, 1.0f);
            auto spline = [u](float p0, float p1, float p2, float p3)
            {
                return 0.5f * (2.0f * p1 + (p2 - p0) * u + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * u * u
                    + (3.0f * p1 - p0 - 3.0f * p2 + p3) * u * u * u);
            };
            for (int i = 0; i < 3; ++i) {
                eye[i] = spline(before.eye[i], a.eye[i], b.eye[i], after.eye[i]);
                target[i] = spline(before.target[i], a.target[i], b.target[i], after.target[i]);
            }
            fovY = a.fovY + (b.fovY - a.fovY) * u;
        }
    };
}
