#include "Constraints.hpp"
#include "Orientation.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace {

long long volume(const Dimension& dim) {
    return static_cast<long long>(dim.width) * dim.length * dim.depth;
}

bool fitsWithin(const Dimension& dim, const Dimension& outer) {
    return dim.width <= outer.width &&
           dim.length <= outer.length &&
           dim.depth <= outer.depth;
}

// Compares the two shapes side sorted longest to shortest, which answers "could this
// fit inside that in some rotation" without walking every rotation.
bool fitsWhenRotated(const Dimension& dim, const Dimension& outer) {
    std::array<int, 3> innerSides { dim.width, dim.length, dim.depth };
    std::array<int, 3> outerSides { outer.width, outer.length, outer.depth };
    std::sort(innerSides.begin(), innerSides.end());
    std::sort(outerSides.begin(), outerSides.end());
    return innerSides[0] <= outerSides[0] &&
           innerSides[1] <= outerSides[1] &&
           innerSides[2] <= outerSides[2];
}

std::string dimensionText(const Dimension& dim) {
    return std::to_string(dim.width) + "x" +
           std::to_string(dim.length) + "x" +
           std::to_string(dim.depth);
}

std::string weightText(double weight) {
    std::string text = std::to_string(weight);
    text.erase(text.find_last_not_of('0') + 1);
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text;
}

} // namespace

// Screens an item against the box catalogue before the packer sees it. Returns false with
// out describing the first rule it broke, checked cheapest first so the reported reason is
// the most useful one.
bool Constraints::checkItem(const Item& item, const std::vector<BoxType>& boxes, Violation& out) const {
    out.itemCode = item.itemCode;

    const Dimension& dim = item.itemDimension;
    if (dim.width <= 0 || dim.length <= 0 || dim.depth <= 0) {
        out.code = "INVALID_DIMENSIONS";
        out.message = "Dimensions must all be greater than zero, got " + dimensionText(dim);
        return false;
    }

    if (item.weight < 0.0) {
        out.code = "INVALID_WEIGHT";
        out.message = "Weight must not be negative";
        return false;
    }

    if (maxItemWeight > 0.0 && item.weight > maxItemWeight) {
        out.code = "ITEM_TOO_HEAVY";
        out.message = "Weight " + weightText(item.weight) +
                      " exceeds the limit of " + weightText(maxItemWeight);
        return false;
    }

    if (maxItemVolume > 0 && volume(dim) > maxItemVolume) {
        out.code = "ITEM_TOO_LARGE";
        out.message = "Volume " + std::to_string(volume(dim)) +
                      " exceeds the limit of " + std::to_string(maxItemVolume);
        return false;
    }

    const bool limitBySize = maxItemDimension.width > 0 &&
                             maxItemDimension.length > 0 &&
                             maxItemDimension.depth > 0;
    if (limitBySize) {
        const bool withinLimit = allowRotation ? fitsWhenRotated(dim, maxItemDimension)
                                               : fitsWithin(dim, maxItemDimension);
        if (!withinLimit) {
            out.code = "ITEM_TOO_LARGE";
            out.message = "Size " + dimensionText(dim) +
                          " exceeds the limit of " + dimensionText(maxItemDimension);
            return false;
        }
    }

    // An item is only worth handing to the packer if some active box could take it, both
    // by shape and by weight. Tracking the two separately tells the user which one to fix.
    bool anyBoxActive = false;
    bool anyBoxFits = false;
    bool anyBoxHoldsWeight = false;

    for (const auto& box : boxes) {
        if (!box.active) {
            continue;
        }
        anyBoxActive = true;

        const bool fits = allowRotation ? fitsWhenRotated(dim, box.boxDimension)
                                        : fitsWithin(dim, box.boxDimension);
        const bool holdsWeight = box.maxWeight <= 0.0 || item.weight <= box.maxWeight;

        if (fits) {
            anyBoxFits = true;
        }
        if (holdsWeight) {
            anyBoxHoldsWeight = true;
        }
        if (fits && holdsWeight) {
            return true;
        }
    }

    if (!anyBoxActive) {
        out.code = "NO_BOX_AVAILABLE";
        out.message = "There are no active box types to pack into";
        return false;
    }

    if (!anyBoxFits) {
        out.code = "NO_BOX_FITS";
        out.message = "Size " + dimensionText(dim) + " does not fit any active box type";
        return false;
    }

    if (!anyBoxHoldsWeight) {
        out.code = "NO_BOX_SUPPORTS_WEIGHT";
        out.message = "Weight " + weightText(item.weight) +
                      " is over the limit of every active box type";
        return false;
    }

    // Every box either fits the shape or takes the weight, but no single one does both.
    out.code = "NO_BOX_FITS";
    out.message = "No active box type fits " + dimensionText(dim) +
                  " and carries " + weightText(item.weight);
    return false;
}

bool Constraints::allowsPlacement(const BoxState& box, const Item& item) const {
    if (box.maxWeight > 0.0 && box.currentWeight + item.weight > box.maxWeight) {
        return false;
    }

    if (enforceBoxGroups && !box.group.empty() && !item.boxGroup.empty()) {
        return box.group == item.boxGroup;
    }

    return true;
}

std::vector<Dimension> Constraints::permittedRotations(const Item& item) const {
    if (!allowRotation) {
        return { item.itemDimension };
    }

    auto rotations = Orientation::allRotations(item.itemDimension);
    return std::vector<Dimension>(rotations.begin(), rotations.end());
}
