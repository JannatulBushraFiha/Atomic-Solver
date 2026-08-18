#include "BoxType.h"

BoxType::BoxType(std::string ref, Dimensions dim, double maxW, double boxW, bool isActive, int maxBoxes) {
    reference = ref;
    d = dim;
    maxWeight = maxW;
    boxWeight = boxW;
    active = isActive;
    maximumBoxes = maxBoxes;
}
