#include "Validator.h"

#include <cmath>
#include <map>
#include <set>
#include <string>

namespace {

const double WEIGHT_TOLERANCE = 1e-6;

struct BoxContents {
    std::string reference;
    int instance = 0;
    std::vector<const Placement*> placements;
    double itemWeight = 0.0;
};

std::string boxKey(const std::string& reference, int instance) {
    return reference + "#" + std::to_string(instance);
}

std::string boxLabel(const std::string& reference, int instance) {
    return reference + " instance " + std::to_string(instance);
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

void add(ValidationResult& result, const std::string& code, const std::string& itemCode, const std::string& message) {
    result.valid = false;
    result.violations.push_back({ code, itemCode, message });
}

bool overlaps(const Placement& first, const Placement& second) {
    return first.position.x < second.position.x + second.placedDimension.width &&
           second.position.x < first.position.x + first.placedDimension.width &&
           first.position.y < second.position.y + second.placedDimension.length &&
           second.position.y < first.position.y + first.placedDimension.length &&
           first.position.z < second.position.z + second.placedDimension.depth &&
           second.position.z < first.position.z + first.placedDimension.depth;
}

bool isPermittedRotation(const Dimension& placed, const Item& item, const Constraints& constraints) {
    for (const Dimension& rotation : constraints.permittedRotations(item)) {
        if (rotation.width == placed.width &&
            rotation.length == placed.length &&
            rotation.depth == placed.depth) {
            return true;
        }
    }
    return false;
}

} // namespace

ValidationResult Validator::validate(
    const PackingSolution& solution,
    const std::vector<Item>& items,
    const std::vector<BoxType>& boxes,
    const Constraints& constraints
) {
    ValidationResult result;

    std::map<std::string, const Item*> itemsByCode;
    for (const auto& item : items) {
        if (!itemsByCode.insert({ item.itemCode, &item }).second) {
            add(result, "DUPLICATE_ITEM_CODE", item.itemCode,
                "Item code appears more than once in the input");
        }
    }

    std::map<std::string, const BoxType*> boxesByReference;
    for (const auto& box : boxes) {
        boxesByReference[box.reference] = &box;
    }

    std::map<std::string, BoxContents> contents;
    std::map<std::string, int> placementCount;

    for (const auto& placement : solution.placements) {
        placementCount[placement.itemCode]++;

        auto itemEntry = itemsByCode.find(placement.itemCode);
        if (itemEntry == itemsByCode.end()) {
            add(result, "UNKNOWN_ITEM", placement.itemCode, "Placed item is not in the input");
            continue;
        }
        const Item& item = *itemEntry->second;

        if (item.shipInOwnPackaging) {
            add(result, "FLAGGED_ITEM_PACKED", item.itemCode,
                "Marked to ship in its own packaging but was packed into box " +
                boxLabel(placement.boxReference, placement.boxInstance));
        }

        if (!isPermittedRotation(placement.placedDimension, item, constraints)) {
            add(result, "INVALID_ROTATION", item.itemCode,
                "Placed as " + dimensionText(placement.placedDimension) +
                " which is not an allowed rotation of " + dimensionText(item.itemDimension));
        }

        if (placement.position.x < 0 || placement.position.y < 0 || placement.position.z < 0) {
            add(result, "NEGATIVE_POSITION", item.itemCode, "Sits at a negative position");
        }

        if (placement.boxInstance < 1) {
            add(result, "INVALID_BOX_INSTANCE", item.itemCode, "Box instance numbers start at 1");
        }

        auto boxEntry = boxesByReference.find(placement.boxReference);
        if (boxEntry == boxesByReference.end()) {
            add(result, "UNKNOWN_BOX_REFERENCE", item.itemCode,
                "Placed in box type " + placement.boxReference + " which is not in the input");
            continue;
        }
        const BoxType& boxType = *boxEntry->second;

        if (!boxType.active) {
            add(result, "INACTIVE_BOX", item.itemCode,
                "Placed in box type " + boxType.reference + " which is not active");
        }

        const Dimension& placed = placement.placedDimension;
        const Dimension& outer = boxType.boxDimension;
        if (placement.position.x + placed.width > outer.width ||
            placement.position.y + placed.length > outer.length ||
            placement.position.z + placed.depth > outer.depth) {
            add(result, "OUT_OF_BOUNDS", item.itemCode,
                "Sticks out of box " + boxLabel(placement.boxReference, placement.boxInstance));
        }

        BoxContents& box = contents[boxKey(placement.boxReference, placement.boxInstance)];
        box.reference = placement.boxReference;
        box.instance = placement.boxInstance;
        box.placements.push_back(&placement);
        box.itemWeight += item.weight;
    }

    for (const auto& entry : contents) {
        const BoxContents& box = entry.second;

        for (size_t first = 0; first < box.placements.size(); first++) {
            for (size_t second = first + 1; second < box.placements.size(); second++) {
                if (overlaps(*box.placements[first], *box.placements[second])) {
                    add(result, "OVERLAP", box.placements[first]->itemCode,
                        "Overlaps " + box.placements[second]->itemCode +
                        " in box " + boxLabel(box.reference, box.instance));
                }
            }
        }

        if (constraints.enforceBoxGroups) {
            std::string boxGroup;
            for (const Placement* placement : box.placements) {
                auto itemEntry = itemsByCode.find(placement->itemCode);
                if (itemEntry == itemsByCode.end()) {
                    continue;
                }

                const std::string& itemGroup = itemEntry->second->boxGroup;
                if (itemGroup.empty()) {
                    continue;
                }
                if (boxGroup.empty()) {
                    boxGroup = itemGroup;
                    continue;
                }
                if (boxGroup != itemGroup) {
                    add(result, "MIXED_BOX_GROUPS", placement->itemCode,
                        "Box " + boxLabel(box.reference, box.instance) +
                        " holds groups " + boxGroup + " and " + itemGroup);
                }
            }
        }

        bool hasDangerousGoods = false;
        bool hasOrdinary = false;
        std::string dgClass;
        bool mixedDgClasses = false;
        for (const Placement* placement : box.placements) {
            auto itemEntry = itemsByCode.find(placement->itemCode);
            if (itemEntry == itemsByCode.end()) {
                continue;
            }
            const Item& item = *itemEntry->second;
            if (item.isDangerousGoods) {
                if (!hasDangerousGoods) {
                    hasDangerousGoods = true;
                    dgClass = item.dangerousGoodsClass;
                } else if (dgClass != item.dangerousGoodsClass) {
                    mixedDgClasses = true;
                }
            } else {
                hasOrdinary = true;
            }
        }
        if (hasDangerousGoods && hasOrdinary) {
            add(result, "DANGEROUS_GOODS_MIXED", "",
                "Box " + boxLabel(box.reference, box.instance) +
                " mixes dangerous goods with ordinary items");
        }
        if (mixedDgClasses) {
            add(result, "DANGEROUS_GOODS_MIXED", "",
                "Box " + boxLabel(box.reference, box.instance) +
                " mixes more than one dangerous goods class");
        }

        // Fragile items may share a box with other fragile items, but never with ordinary items
        int fragileCount = 0;
        int totalCount = static_cast<int>(box.placements.size());
        for (const Placement* placement : box.placements) {
        auto itemEntry = itemsByCode.find(placement->itemCode);
        if (itemEntry != itemsByCode.end() && itemEntry->second->isFragile) {
             fragileCount++;
        }
        }
        if (fragileCount > 0 && fragileCount < totalCount) {
             add(result, "FRAGILE_NOT_SEPARATE", "",
             "Box " + boxLabel(box.reference, box.instance) +
             " mixes fragile items with ordinary items");
        }
    }

    std::map<std::string, int> instancesPerType;
    std::set<std::string> usedKeys;

    for (const auto& used : solution.usedBoxes) {
        const std::string key = boxKey(used.boxReference, used.boxInstance);
        const std::string label = boxLabel(used.boxReference, used.boxInstance);

        if (!usedKeys.insert(key).second) {
            add(result, "DUPLICATE_BOX_INSTANCE", "", "Box " + label + " is listed more than once");
        }
        instancesPerType[used.boxReference]++;

        auto contentEntry = contents.find(key);
        const double packedWeight = contentEntry == contents.end() ? 0.0 : contentEntry->second.itemWeight;

        if (contentEntry == contents.end()) {
            add(result, "EMPTY_BOX", "", "Box " + label + " was opened but holds nothing");
        }

        if (std::fabs(packedWeight - used.totalWeight) > WEIGHT_TOLERANCE) {
            add(result, "WEIGHT_MISMATCH", "",
                "Box " + label + " reports " + weightText(used.totalWeight) +
                " but its items weigh " + weightText(packedWeight));
        }

        auto boxEntry = boxesByReference.find(used.boxReference);
        if (boxEntry == boxesByReference.end()) {
            add(result, "UNKNOWN_BOX_REFERENCE", "",
                "Box type " + used.boxReference + " is not in the input");
            continue;
        }
        const BoxType& boxType = *boxEntry->second;

        if (boxType.maxWeight > 0.0 && packedWeight > boxType.maxWeight + WEIGHT_TOLERANCE) {
            add(result, "BOX_OVERWEIGHT", "",
                "Box " + label + " holds " + weightText(packedWeight) +
                " which is over its limit of " + weightText(boxType.maxWeight));
        }
    }

    for (const auto& entry : contents) {
        if (usedKeys.count(entry.first) == 0) {
            add(result, "MISSING_USED_BOX", "",
                "Box " + boxLabel(entry.second.reference, entry.second.instance) +
                " holds items but is not listed in usedBoxes");
        }
    }

    for (const auto& entry : instancesPerType) {
        auto boxEntry = boxesByReference.find(entry.first);
        if (boxEntry == boxesByReference.end()) {
            continue;
        }

        const BoxType& boxType = *boxEntry->second;
        if (boxType.maximumBoxes != -1 && entry.second > boxType.maximumBoxes) {
            add(result, "BOX_LIMIT_EXCEEDED", "",
                "Used " + std::to_string(entry.second) + " boxes of type " + entry.first +
                " but the limit is " + std::to_string(boxType.maximumBoxes));
        }
    }

    std::set<std::string> unplaced;
    for (const auto& code : solution.unplacedItems) {
        if (!unplaced.insert(code).second) {
            add(result, "DUPLICATE_UNPLACED_ITEM", code, "Listed as unplaced more than once");
        }
        if (itemsByCode.find(code) == itemsByCode.end()) {
            add(result, "UNKNOWN_ITEM", code, "Unplaced item is not in the input");
        }
    }

    std::set<std::string> ownPackaged;
    for (const auto& own : solution.ownPackagedItems) {
        if (!ownPackaged.insert(own.itemCode).second) {
            add(result, "DUPLICATE_OWN_PACKAGED_ITEM", own.itemCode,
                "Listed as shipped in own packaging more than once");
        }
        auto itemEntry = itemsByCode.find(own.itemCode);
        if (itemEntry == itemsByCode.end()) {
            add(result, "UNKNOWN_ITEM", own.itemCode, "Own-packaged item is not in the input");
            continue;
        }
        const Item& item = *itemEntry->second;
        if (!item.shipInOwnPackaging) {
            add(result, "OWN_PACKAGING_NOT_FLAGGED", own.itemCode,
                "Shipped in own packaging but the input did not ask for it");
        }
        const Dimension& in = item.itemDimension;
        if (own.dimension.width != in.width ||
            own.dimension.length != in.length ||
            own.dimension.depth != in.depth ||
            std::fabs(own.weight - item.weight) > WEIGHT_TOLERANCE) {
            add(result, "OWN_PACKAGING_MISMATCH", own.itemCode,
                "Reported as " + dimensionText(own.dimension) + " weighing " + weightText(own.weight) +
                " but the input is " + dimensionText(in) + " weighing " + weightText(item.weight));
        }
    }

    std::set<std::string> explained;
    for (const auto& violation : solution.violations) {
        explained.insert(violation.itemCode);
    }

    for (const auto& item : items) {
        const int placedTimes = placementCount[item.itemCode];
        const bool isUnplaced = unplaced.count(item.itemCode) > 0;
        const bool isOwnPackaged = ownPackaged.count(item.itemCode) > 0;

        if (placedTimes > 1) {
            add(result, "ITEM_PLACED_TWICE", item.itemCode,
                "Placed " + std::to_string(placedTimes) + " times");
        }
        if (placedTimes > 0 && isUnplaced) {
            add(result, "ITEM_PLACED_AND_UNPLACED", item.itemCode,
                "Listed as both placed and unplaced");
        }
        if (placedTimes > 0 && isOwnPackaged) {
            add(result, "ITEM_PLACED_AND_OWN_PACKAGED", item.itemCode,
                "Listed as both placed and shipped in own packaging");
        }
        if (isUnplaced && isOwnPackaged) {
            add(result, "ITEM_UNPLACED_AND_OWN_PACKAGED", item.itemCode,
                "Listed as both unplaced and shipped in own packaging");
        }
        if (placedTimes == 0 && !isUnplaced && !isOwnPackaged) {
            add(result, "ITEM_MISSING", item.itemCode,
                "Does not appear as placed, unplaced, or shipped in own packaging");
        }
        if (isUnplaced && explained.count(item.itemCode) == 0) {
            add(result, "MISSING_REASON", item.itemCode, "Unplaced with nothing explaining why");
        }
    }

    return result;
}