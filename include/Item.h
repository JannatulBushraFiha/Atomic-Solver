#pragma once

#include <string>
#include <optional>

struct Item {
    std::string itemCode;
    std::string itemReference;

    int width;
    int length;
    int depth;

    double weight;

  std::string boxGroup;
};