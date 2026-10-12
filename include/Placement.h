#pragma once

#include <string>
#include "Position.h"
#include "Dimension.h"

struct Placement {
    std::string itemCode;

    std::string boxReference;   // matches BoxType::reference
    int boxInstance;            // 1st box of this type, 2nd box, etc.

    Position position;          // corner of item inside the box
    Dimension placedDimension;
};