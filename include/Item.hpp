#pragma once

#include <string>
#include "Dimension.hpp"


struct Item {
    std::string itemCode;
    std::string itemReference;

    Dimension itemDimension;
    double weight =0.0;

  std::string boxGroup;
};