#ifndef ORIENTATION_H
#define ORIENTATION_H

<<<<<<< HEAD
// not doing orientation for MVP, out of scope for now
enum class Orientation {
    NONE,
    UPRIGHT
};

#endif
=======
#include <array>
#include "Dimension.h"

// All 6 ways a box/item can be rotated. Duplicates for non-cube items are fine.
namespace Orientation {
    std::array<Dimension, 6> allRotations(const Dimension& dim);
}
>>>>>>> origin/jannatul-final-branch
