<<<<<<< HEAD
#ifndef BOXTYPE_H
#define BOXTYPE_H

#include <string>
#include "Dimensions.h"
=======
#pragma once
#include <string>
#include "Dimension.h"
>>>>>>> origin/jannatul-final-branch

class BoxType {
public:
    std::string reference;
    Dimensions d;
    double maxWeight;
    double boxWeight;
    bool active;
    int maximumBoxes;

<<<<<<< HEAD
    BoxType(std::string ref, Dimensions dim, double maxW, double boxW, bool isActive = true, int maxBoxes = -1);
};

#endif
=======
    Dimension boxDimension;

    double maxWeight =0.0;
    double boxWeight =0.0;

    bool active = true;

   int maximumBoxes = -1;
};
>>>>>>> origin/jannatul-final-branch
