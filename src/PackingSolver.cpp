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
    bool hasDangerousGoods = false;
    std::string dangerousGoodsClass;
    bool hasFragile = false;
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

BoxState toBoxState(const BoxInstance& box) {
    BoxState state;
    state.maxWeight = box.maxWeight;
    state.currentWeight = box.currentWeight;
    state.group = box.group;
    state.hasDangerousGoods = box.hasDangerousGoods;
    state.dangerousGoodsClass = box.dangerousGoodsClass;
    state.hasFragile = box.hasFragile;
    return state;
}

bool tryPlaceInInstance(BoxInstance& box, const Item& item, Placement& outPlacement, const Constraints& constraints) {
    Violation ignored;
    if (!constraints.allowsPlacement(toBoxState(box), item, ignored)) return false;

    // Sort free spaces: prioritize lowest z (build up), then y, then x
    std::sort(box.freeSpaces.begin(), box.freeSpaces.end(), [](const FreeSpace& a, const FreeSpace& b) {
        if (a.z != b.z) return a.z < b.z;
        if (a.y != b.y) return a.y < b.y;
        return a.x < b.x;
    });

    auto rotations = constraints.permittedRotations(item);

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
                if (item.isDangerousGoods) {
                    box.hasDangerousGoods = true;
                    box.dangerousGoodsClass = item.dangerousGoodsClass;
                }
                if (item.isFragile) box.hasFragile = true;

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

    // Own-packaged items skip packing entirely; they only need to pass the item-level limits.
    std::vector<Item> sortedItems;
    for (const auto& item : items) {
        if (!item.shipInOwnPackaging) {
            sortedItems.push_back(item);
            continue;
        }
        Violation limitViolation;
        if (!constraints.checkItemLimits(item, limitViolation)) {
            solution.unplacedItems.push_back(item.itemCode);
            solution.violations.push_back(limitViolation);
            continue;
        }
        solution.ownPackagedItems.push_back({ item.itemCode, item.itemDimension, item.weight });
    }

    std::sort(sortedItems.begin(), sortedItems.end(), [](const Item& a, const Item& b) {
        long long areaA = static_cast<long long>(a.itemDimension.width) * a.itemDimension.length;
        long long areaB = static_cast<long long>(b.itemDimension.width) * b.itemDimension.length;
        if (areaA != areaB) return areaA > areaB;
        return volume(a.itemDimension) > volume(b.itemDimension);
    });

    std::vector<BoxInstance> openBoxes;
    std::map<std::string, int> boxTypeCount;

    for (const auto& item : sortedItems) {
        // Pre-screen: is this item even placeable given the constraints and available boxes?
        Violation preCheckViolation;
        if (!constraints.checkItem(item, boxes, preCheckViolation)) {
            solution.unplacedItems.push_back(item.itemCode);
            solution.violations.push_back(preCheckViolation);
            continue;
        }

        bool placed = false;
        Placement placement;

        for (auto& box : openBoxes) {
            if (tryPlaceInInstance(box, item, placement, constraints)) {
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

                auto rotations = constraints.permittedRotations(item);
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

                if (tryPlaceInInstance(openBoxes.back(), item, placement, constraints)) {
                    solution.placements.push_back(placement);
                    placed = true;
                }
                break;
            }
        }

        if (!placed) {
            solution.unplacedItems.push_back(item.itemCode);
            solution.violations.push_back({
                "NO_VALID_PLACEMENT",
                item.itemCode,
                "No open or new box could accept this item under the current constraints"
            });
        }
    }

    for (const auto& box : openBoxes) {
        solution.usedBoxes.push_back({ box.reference, box.instanceNumber, box.currentWeight });
    }

    return solution;
}