#pragma once

#include <string>
#include "Position.h"
#include "Orientation.h"

struct Placement {
    std::string itemCode;
    Position position;
    Orientation orientation;
};