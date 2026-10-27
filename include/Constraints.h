#pragma once

#include <string>
#include <vector>

#include "BoxType.h"
#include "Dimension.h"
#include "Item.h"
#include "Violation.h"

const double WEIGHT_TOLERANCE = 1e-6;

struct BoxState {
    double maxWeight = 0.0;
    double boxWeight = 0.0;
    double currentWeight = 0.0;
    int itemCount = 0; // how many items are in the box (weight can't tell, an item may weigh 0)
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

    bool checkItem(const Item& item, const std::vector<BoxType>& boxes, Violation& out) const;
    bool allowsPlacement(const BoxState& box, const Item& item, Violation& out) const;
    bool canRotate(const Item& item) const;
    std::vector<Dimension> permittedRotations(const Item& item) const;
};