#include "FrameTiming.h"
#include <cmath>
#include <limits>
int main()
{
    Pakal::FrameTiming timing;
    if (timing.snapshot().frames != 0) return 1;
    for (int i = 0; i < 19; ++i) timing.add(10);
    timing.add(100);
    auto stats = timing.snapshot();
    if (stats.p95Ms != 10 || std::abs(stats.fps - 20000.0 / 290.0) > 0.001) return 2;
    timing.add(100);
    if (timing.snapshot().p95Ms != 100) return 3;
    for (int i = 0; i < 300; ++i) timing.add(20);
    stats = timing.snapshot();
    if (stats.frames != 250 || stats.p95Ms != 20 || stats.fps != 50) return 4;
    timing.add(0); timing.add(-1); timing.add(std::numeric_limits<double>::quiet_NaN());
    if (timing.snapshot().frames != 250) return 5;
    Pakal::FrameTiming changing;
    for (int i = 0; i < 100; ++i) changing.add(20);
    for (int i = 0; i < 50; ++i) changing.add(10);
    const auto changed = changing.snapshot();
    if (changed.fps != 60 || changed.currentFps != 75) return 6;
}
