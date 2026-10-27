#include "topdown/ActorOrientation.h"
#include <bx/math.h>
#include <cmath>
#include <cstdio>

int main()
{
    int failures = 0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            if (x == 0 && y == 0) continue;
            const float length = std::sqrt(float(x*x + y*y));
            // Level (x,y) becomes render (x,-y).
            const float dx = x / length, dz = -y / length;
            float matrix[16];
            bx::mtxSRT(matrix, 1, 1, 1, 0, Pakal::TopDown::actor_yaw(dx, dz), 0, 0, 0, 0);
            const bool matches = std::fabs(matrix[8] - dx) < 1e-5f && std::fabs(matrix[10] - dz) < 1e-5f;
            std::printf("[%s] model +Z faces movement (%d,%d) using bx transform\n", matches ? "PASS" : "FAIL", x, y);
            if (!matches) ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
