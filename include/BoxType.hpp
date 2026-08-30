#pragma once
#include <string>
#include "Dimension.hpp"

struct BoxType {
    std::string reference;

    Dimension boxDimension;

    double maxWeight =0.0;
    double boxWeight =0.0;

    bool active = true;

   int maximumBoxes = -1;
};
