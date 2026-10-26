#include "delayline.h"

#include <cmath>

namespace delayline {

int shown(const QList<double> &drawnAt, double now, double delay, double frame)
{
    if (drawnAt.isEmpty())
        return -1;
    int newest = 0;
    for (int index = 0; index < drawnAt.size(); ++index) {
        if (drawnAt[index] <= now - delay + frame / 2)
            newest = index;
    }
    return newest;
}

double nextDue(const QList<double> &drawnAt, int shown, double delay, double frame)
{
    if (shown < 0 || shown + 1 >= drawnAt.size())
        return -1;
    return drawnAt[shown + 1] + delay - frame / 2;
}

int most(double delay, double frame)
{
    if (frame <= 0)
        return 2;
    return int(std::ceil(delay / frame)) + 2;
}

} // namespace delayline
