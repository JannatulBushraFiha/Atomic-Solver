#pragma once

#include <array>
#include "Dimension.h"

// All 6 ways a box/item can be rotated. Duplicates for non-cube items are fine.
namespace Orientation {
    std::array<Dimension, 6> allRotations(const Dimension& dim);
}