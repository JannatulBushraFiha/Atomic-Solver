#include "Orientation.h"

namespace Orientation {

std::array<Dimension, 6> allRotations(const Dimension& dim) {
    int w = dim.width;
    int l = dim.length;
    int d = dim.depth;

    return {
        Dimension{w, l, d},
        Dimension{w, d, l},
        Dimension{l, w, d},
        Dimension{l, d, w},
        Dimension{d, w, l},
        Dimension{d, l, w}
    };
}

}