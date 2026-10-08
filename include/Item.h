#ifndef ITEM_H
#define ITEM_H

#include <string>
<<<<<<< HEAD
#include "Dimensions.h"
=======
#include "Dimension.h"

>>>>>>> origin/jannatul-final-branch

class Item {
public:
    std::string itemCode;
    std::string itemReference;
<<<<<<< HEAD
    Dimensions d;
    double weight;
    std::string boxGroup;

    Item(std::string code, std::string ref, Dimensions dim, double w, std::string group = "");
};

#endif
=======
    Dimension itemDimension;
    std::string boxGroup;
    bool isFragile = false;
    bool isDangerousGoods = false;
    std::string dangerousGoodsClass;
    
    double weight =0.0;
};
>>>>>>> origin/jannatul-final-branch
