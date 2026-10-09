#pragma once

#include <string>
#include "Dimension.h"


struct Item {
    std::string itemCode;
    std::string itemReference;
    Dimension itemDimension;
    std::string boxGroup;
    bool isFragile = false;
    bool isDangerousGoods = false;
    std::string dangerousGoodsClass;

    // Ships as-is in its own packaging (never put in a carton); dimensions/weight are then the outer packaging.
    bool shipInOwnPackaging = false;

    double weight =0.0;
};