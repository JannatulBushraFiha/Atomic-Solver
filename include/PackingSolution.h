#ifndef PACKINGSOLUTION_H
#define PACKINGSOLUTION_H

#include <vector>
#include <string>
#include "Placement.h"
#include "Violation.h"

struct UsedBox {
    std::string boxReference;
    int boxInstance;
    double totalWeight = 0.0;
};

class PackingSolution {
public:
    std::vector<Placement> solution;
    std::vector<std::string> unplacedItems;
<<<<<<< HEAD
};

#endif
=======
    std::vector<UsedBox> usedBoxes;
    std::vector<Violation> violations;
};
>>>>>>> origin/jannatul-final-branch
