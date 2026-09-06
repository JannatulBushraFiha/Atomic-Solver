#pragma once

#include <string>
#include <vector>

#include "BoxType.hpp"
#include "Dimension.hpp"
#include "Item.hpp"
#include "Violation.hpp"

struct BoxState {
    double maxWeight = 0.0;
    double currentWeight = 0.0;
    std::string group;
};

// The rule set the solver packs under. A zero or empty limit is not applied, so a
// default constructed Constraints lets everything through.
struct Constraints {
    double maxItemWeight = 0.0;
    long long maxItemVolume = 0;
    Dimension maxItemDimension {0, 0, 0};

    bool enforceBoxGroups = true;
    bool allowRotation = true;

    bool checkItem(const Item& item, const std::vector<BoxType>& boxes, Violation& out) const;
    bool allowsPlacement(const BoxState& box, const Item& item) const;
    std::vector<Dimension> permittedRotations(const Item& item) const;
};
