#pragma once

#include <string>
#include <vector>

#include "BoxType.h"
#include "Dimension.h"
#include "Item.h"
#include "Violation.h"

struct BoxState {
    double maxWeight = 0.0;
    double currentWeight = 0.0;
    std::string group;
    bool hasDangerousGoods = false;
    std::string dangerousGoodsClass;
    bool hasFragile = false;
};

struct Constraints {
    double maxItemWeight = 0.0;
    long long maxItemVolume = 0;
    Dimension maxItemDimension {0, 0, 0};

    bool enforceBoxGroups = true;
    bool allowRotation = true;

    std::vector<std::string> prohibitedDangerousGoodsClasses = {"1"};

    // Item-only checks (dimensions, weight, prohibited goods, per-item limits); ignores box types.
    bool checkItemLimits(const Item& item, Violation& out) const;

    // checkItemLimits plus "at least one active box type can take this item".
    bool checkItem(const Item& item, const std::vector<BoxType>& boxes, Violation& out) const;
    bool allowsPlacement(const BoxState& box, const Item& item, Violation& out) const;
    std::vector<Dimension> permittedRotations(const Item& item) const;
};