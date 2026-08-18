#ifndef PACKINGSOLUTION_H
#define PACKINGSOLUTION_H

#include <vector>
#include <string>
#include "Placement.h"

class PackingSolution {
public:
    std::vector<Placement> solution;
    std::vector<std::string> unplacedItems;
};

#endif
