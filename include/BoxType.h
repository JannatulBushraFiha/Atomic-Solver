#ifndef BOXTYPE_H
#define BOXTYPE_H

#include <string>
#include "Dimensions.h"

class BoxType {
public:
    std::string reference;
    Dimensions d;
    double maxWeight;
    double boxWeight;
    bool active;
    int maximumBoxes;

    BoxType(std::string ref, Dimensions dim, double maxW, double boxW, bool isActive = true, int maxBoxes = -1);
};

#endif
