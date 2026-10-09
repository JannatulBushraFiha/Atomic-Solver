#pragma once

#include <vector>
#include <string>
#include "Placement.h"
#include "Violation.h"

struct UsedBox {
    std::string boxReference;
    int boxInstance;
    double totalWeight = 0.0;
};

// An item shipped as-is in its own packaging. It occupies no carton and is its own parcel.
struct OwnPackagedItem {
    std::string itemCode;
    Dimension dimension;
    double weight = 0.0;
};

struct PackingSolution {
    std::vector<Placement> placements;
    std::vector<std::string> unplacedItems;
    std::vector<UsedBox> usedBoxes;
    std::vector<OwnPackagedItem> ownPackagedItems;
    std::vector<Violation> violations;
};