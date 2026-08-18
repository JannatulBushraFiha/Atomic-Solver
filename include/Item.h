#ifndef ITEM_H
#define ITEM_H

#include <string>
#include "Dimensions.h"

class Item {
public:
    std::string itemCode;
    std::string itemReference;
    Dimensions d;
    double weight;
    std::string boxGroup;

    Item(std::string code, std::string ref, Dimensions dim, double w, std::string group = "");
};

#endif
