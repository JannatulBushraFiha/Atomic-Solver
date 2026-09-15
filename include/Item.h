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
    
    double weight =0.0;
};