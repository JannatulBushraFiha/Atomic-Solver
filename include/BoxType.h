#pragma once

#include <string>
#include <optional>

struct BoxType {
    std::string reference;

    int width;
    int length;
    int depth;

    double maxWeight =0;
    double boxWeight =0;

    bool active = true;

   int maximumBoxes = -1;
};