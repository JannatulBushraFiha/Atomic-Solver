#ifndef PLACEMENT_H
#define PLACEMENT_H

#include <string>
#include "Position.h"
<<<<<<< HEAD

class Placement {
public:
    std::string boxRef;
    std::string itemRef;
    Position position;

    Placement(std::string box = "", std::string item = "", Position pos = Position());
};

#endif
=======
#include "Dimension.h"

struct Placement {
    std::string itemCode;

    std::string boxReference;   // matches BoxType::reference
    int boxInstance;            // 1st box of this type, 2nd box, etc.

    Position position;          // corner of item inside the box
    Dimension placedDimension;
};
>>>>>>> origin/jannatul-final-branch
