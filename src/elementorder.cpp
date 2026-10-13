#include "elementorder.h"

namespace elementorder {

int placed(int count, int from, int landing)
{
    if (count < 1 || from < 0 || from >= count || landing < 0 || landing > count)
        return -1;
    // Its row in the list now, the front one being row 0
    const int row = count - 1 - from;
    // Its row once it has been taken out and put in again: the rows under where it was
    // have each moved up one by then, so a landing under it is one row less.
    const int target = landing > row ? landing - 1 : landing;
    if (target == row)
        return -1;
    return count - 1 - target;
}

}
