#include "framepacing.h"

#include <algorithm>
#include <vector>

namespace framepacing {

double videoInterval(const QList<double> &arrivals)
{
    if (arrivals.size() < 2)
        return 0;
    std::vector<double> gaps;
    for (qsizetype i = 1; i < arrivals.size(); ++i)
        gaps.push_back(arrivals[i] - arrivals[i - 1]);
    std::nth_element(gaps.begin(), gaps.begin() + gaps.size() / 2, gaps.end());
    return gaps[gaps.size() / 2];
}

QList<double> framesBetween(double videoInterval, double refresh)
{
    QList<double> moments;
    if (videoInterval <= 0 || refresh <= 0)
        return moments;
    // (A millisecond to spare: a video of thirty frames a second on a 60 Hz screen has
    // exactly one refresh's room after the one between, and its frames come a little
    // early or late.)
    for (int k = 1; k * refresh <= videoInterval - refresh + 1.0 && k < 16; ++k)
        moments.append(k * refresh);
    return moments;
}

bool videoStopped(double sinceLastFrame, double videoInterval)
{
    return videoInterval <= 0 || sinceLastFrame > videoInterval * 1.5;
}

} // namespace framepacing
