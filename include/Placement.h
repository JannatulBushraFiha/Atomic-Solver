#ifndef PLACEMENT_H
#define PLACEMENT_H

#include <string>
#include "Position.h"

class Placement {
public:
    std::string boxRef;
    std::string itemRef;
    Position position;

    Placement(std::string box = "", std::string item = "", Position pos = Position());
};

#endif
