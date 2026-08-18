#include "Item.h"

Item::Item(std::string code, std::string ref, Dimensions dim, double w, std::string group) {
    itemCode = code;
    itemReference = ref;
    d = dim;
    weight = w;
    boxGroup = group;
}
