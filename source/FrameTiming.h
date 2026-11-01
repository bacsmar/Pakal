#pragma once
#include <algorithm>
#include <cmath>
#include <deque>
#include <mutex>
#include <vector>

namespace Pakal
{
    // A rolling five-second view of presented frame intervals, independent of profiling recordings.
    class FrameTiming
    {
    public:
        struct Snapshot { double fps = 0.0, p95Ms = 0.0; size_t frames = 0; double currentFps = 0.0; };
        void add(double milliseconds)
        {
            if (!std::isfinite(milliseconds) || milliseconds <= 0.0) return;
            std::lock_guard<std::mutex> lock(m_mutex);
            m_frames.push_back(milliseconds);
            m_total += milliseconds;
            while (m_frames.size() > 1 && m_total - m_frames.front() >= 5000.0)
            {
                m_total -= m_frames.front();
                m_frames.pop_front();
            }
        }
        Snapshot snapshot() const
        {
            std::vector<double> frames;
            double total;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                frames.assign(m_frames.begin(), m_frames.end());
                total = m_total;
            }
            if (frames.empty()) return {};
            double recentMs = 0.0;
            size_t recentFrames = 0;
            for (auto it = frames.rbegin(); it != frames.rend() && recentMs < 1000.0; ++it)
            { recentMs += *it; ++recentFrames; }
            const double currentFps = 1000.0 * recentFrames / recentMs;
            const size_t rank = static_cast<size_t>(std::ceil(frames.size() * 0.95)) - 1;
            std::nth_element(frames.begin(), frames.begin() + rank, frames.end());
            return {1000.0 * frames.size() / total, frames[rank], frames.size(), currentFps};
        }
    private:
        mutable std::mutex m_mutex;
        std::deque<double> m_frames;
        double m_total = 0.0;
    };
}
