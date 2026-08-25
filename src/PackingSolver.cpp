#include "PackingSolver.h"
#include "Orientation.h"

#include <algorithm>
#include <map>

namespace {

struct FreeSpace {
    int x, y, z;
    int width, length, depth;
};

struct BoxInstance {
    std::string reference;
    int instanceNumber;
    double maxWeight;
    double currentWeight = 0.0;
    std::string group; // empty = not yet assigned to a group
    std::vector<FreeSpace> freeSpaces;
};

long long volume(const Dimension& d) {
    return static_cast<long long>(d.width) * d.length * d.depth;
}

bool fitsInSpace(const Dimension& dim, const FreeSpace& space) {
    return dim.width <= space.width &&
           dim.length <= space.length &&
           dim.depth <= space.depth;
}

bool groupCompatible(const BoxInstance& box, const Item& item) {
    if (box.group.empty()) return true;
    if (item.boxGroup.empty()) return true;
    return box.group == item.boxGroup;
}

void splitFreeSpace(std::vector<FreeSpace>& spaces, size_t usedIndex, const Dimension& placedDim) {
    FreeSpace used = spaces[usedIndex];
    spaces.erase(spaces.begin() + usedIndex);

    if (used.width - placedDim.width > 0) {
        spaces.push_back({
            used.x + placedDim.width, used.y, used.z,
            used.width - placedDim.width, used.length, used.depth
        });
    }
    if (used.length - placedDim.length > 0) {
        spaces.push_back({
            used.x, used.y + placedDim.length, used.z,
            placedDim.width, used.length - placedDim.length, used.depth
        });
    }
    if (used.depth - placedDim.depth > 0) {
        spaces.push_back({
            used.x, used.y, used.z + placedDim.depth,
            placedDim.width, placedDim.length, used.depth - placedDim.depth
        });
    }
}

bool tryPlaceInInstance(BoxInstance& box, const Item& item, Placement& outPlacement) {
    if (!groupCompatible(box, item)) return false;
    if (box.maxWeight > 0 && box.currentWeight + item.weight > box.maxWeight) return false;

    auto rotations = Orientation::allRotations(item.itemDimension);

    for (size_t spaceIdx = 0; spaceIdx < box.freeSpaces.size(); spaceIdx++) {
        for (const Dimension& rot : rotations) {
            if (fitsInSpace(rot, box.freeSpaces[spaceIdx])) {
                FreeSpace chosen = box.freeSpaces[spaceIdx];

                outPlacement.itemCode = item.itemCode;
                outPlacement.boxReference = box.reference;
                outPlacement.boxInstance = box.instanceNumber;
                outPlacement.position = { chosen.x, chosen.y, chosen.z };
                outPlacement.placedDimension = rot;

                splitFreeSpace(box.freeSpaces, spaceIdx, rot);
                box.currentWeight += item.weight;
                if (!item.boxGroup.empty()) box.group = item.boxGroup;

                return true;
            }
        }
    }
    return false;
}

} // namespace

PackingSolution PackingSolver::solve(
    const std::vector<Item>& items,
    const std::vector<BoxType>& boxes
) {
    PackingSolution solution;

    std::vector<BoxType> activeBoxes;
    for (const auto& b : boxes) {
        if (b.active) activeBoxes.push_back(b);
    }
    std::sort(activeBoxes.begin(), activeBoxes.end(), [](const BoxType& a, const BoxType& b) {
        return volume(a.boxDimension) < volume(b.boxDimension);
    });

    std::vector<Item> sortedItems = items;
    std::sort(sortedItems.begin(), sortedItems.end(), [](const Item& a, const Item& b) {
        return volume(a.itemDimension) > volume(b.itemDimension);
    });

    std::vector<BoxInstance> openBoxes;
    std::map<std::string, int> boxTypeCount;

    for (const auto& item : sortedItems) {
        bool placed = false;
        Placement placement;

        for (auto& box : openBoxes) {
            if (tryPlaceInInstance(box, item, placement)) {
                solution.placements.push_back(placement);
                placed = true;
                break;
            }
        }

        if (!placed) {
            for (const auto& boxType : activeBoxes) {
                int usedCount = boxTypeCount[boxType.reference];
                if (boxType.maximumBoxes != -1 && usedCount >= boxType.maximumBoxes) continue;
                if (boxType.maxWeight > 0 && item.weight > boxType.maxWeight) continue;

                auto rotations = Orientation::allRotations(item.itemDimension);
                bool fitsAny = false;
                for (const Dimension& rot : rotations) {
                    if (rot.width <= boxType.boxDimension.width &&
                        rot.length <= boxType.boxDimension.length &&
                        rot.depth <= boxType.boxDimension.depth) {
                        fitsAny = true;
                        break;
                    }
                }
                if (!fitsAny) continue;

                BoxInstance newBox;
                newBox.reference = boxType.reference;
                newBox.instanceNumber = usedCount + 1;
                newBox.maxWeight = boxType.maxWeight;
                newBox.freeSpaces.push_back({0, 0, 0, boxType.boxDimension.width,
                                              boxType.boxDimension.length, boxType.boxDimension.depth});

                boxTypeCount[boxType.reference] = usedCount + 1;
                openBoxes.push_back(newBox);

                if (tryPlaceInInstance(openBoxes.back(), item, placement)) {
                    solution.placements.push_back(placement);
                    placed = true;
                }
                break;
            }
        }

        if (!placed) {
            solution.unplacedItems.push_back(item.itemCode);
        }
    }

    for (const auto& box : openBoxes) {
        solution.usedBoxes.push_back({ box.reference, box.instanceNumber, box.currentWeight });
    }

    return solution;
}