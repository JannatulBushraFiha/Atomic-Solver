#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <map>

#include "json.hpp"
#include "BoxType.h"
#include "Item.h"
#include "PackingSolver.h"
#include "Validator.h"

using json = nlohmann::json;

BoxType parseBoxType(const json& j) {
    BoxType b;
    b.reference = j.at("Reference").get<std::string>();
    b.boxDimension.width  = j.at("Width").get<int>();
    b.boxDimension.length = j.at("Length").get<int>();
    b.boxDimension.depth  = j.at("Depth").get<int>();
    b.maxWeight    = j.value("MaxWeight", 0.0);
    b.boxWeight    = j.value("BoxWeight", 0.0);
    b.active       = j.value("Active", true);
    b.maximumBoxes = j.value("MaximumBoxes", -1);
    return b;
}

Item parseItem(const json& j) {
    Item item;
    item.itemCode = j.at("ItemCode").get<std::string>();
    item.itemReference = j.at("ItemReference").get<std::string>();
    item.itemDimension.width  = j.at("Width").get<int>();
    item.itemDimension.length = j.at("Length").get<int>();
    item.itemDimension.depth  = j.at("Depth").get<int>();
    item.weight = j.at("Weight").get<double>();
    item.boxGroup = j.value("BoxGroup", std::string(""));
    item.isFragile = j.value("IsFragile", false);
    item.isDangerousGoods = j.value("IsDangerousGoods", false);
    item.dangerousGoodsClass = j.value("DangerousGoodsClass", std::string(""));
    item.shipInOwnPackaging = j.value("ShipInOwnPackaging", false);
    return item;
}

// One input line can describe several identical units ("Quantity": N).
// Each unit becomes its own Item with a unique INTERNAL code (BOOK-1, BOOK-2, ...)
// because the solver and validator need unique codes. The output maps every unit back
// to the original code (BOOK), so callers see exactly the codes they sent.
// Quantity defaults to 1.
// `originalCodes` remembers which input code each unit came from, so the output can
// show the code exactly as the caller sent it.
void parseItemWithQuantity(const json& j, std::vector<Item>& out,
                           std::map<std::string, std::string>& originalCodes) {
    const int quantity = j.value("Quantity", 1);
    if (quantity < 1) {
        throw std::runtime_error("Quantity must be at least 1 for item " +
                                 j.value("ItemCode", std::string("?")));
    }

    Item base = parseItem(j);
    if (quantity == 1) {
        originalCodes[base.itemCode] = base.itemCode;
        out.push_back(base);
        return;
    }
    for (int n = 1; n <= quantity; n++) {
        Item unit = base;
        unit.itemCode = base.itemCode + "-" + std::to_string(n); // internal unique id only
        originalCodes[unit.itemCode] = base.itemCode;
        out.push_back(unit);
    }
}

int main() {
    std::stringstream buffer;
    buffer << std::cin.rdbuf();

    json input;
    try {
        input = json::parse(buffer.str());
    } catch (const std::exception& e) {
        std::cout << json{{"error", std::string("Invalid JSON: ") + e.what()}}.dump();
        return 1;
    }

    std::vector<BoxType> boxes;
    std::vector<Item> items;
    std::map<std::string, std::string> originalCodes;
    auto outCode = [&](const std::string& internal) {
        auto it = originalCodes.find(internal);
        return it != originalCodes.end() ? it->second : internal;
    };

    try {
        for (const auto& b : input.at("boxTypes")) boxes.push_back(parseBoxType(b));
        for (const auto& i : input.at("items"))    parseItemWithQuantity(i, items, originalCodes);
    } catch (const std::exception& e) {
        std::cout << json{{"error", std::string("Bad input: ") + e.what()}}.dump();
        return 1;
    }

    PackingSolver solver;
    auto startTime = std::chrono::high_resolution_clock::now();
    PackingSolution solution = solver.solve(items, boxes);
    auto endTime = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    ValidationResult validation = Validator::validate(solution, items, boxes, solver.constraints);

    json output;
    output["placements"] = json::array();
    for (const auto& p : solution.placements) {
        output["placements"].push_back({
            {"itemCode", outCode(p.itemCode)},
            {"boxReference", p.boxReference},
            {"boxInstance", p.boxInstance},
            {"position", {{"x", p.position.x}, {"y", p.position.y}, {"z", p.position.z}}},
            {"placedDimension", {{"width", p.placedDimension.width}, {"length", p.placedDimension.length}, {"depth", p.placedDimension.depth}}}
        });
    }

    output["unplacedItems"] = json::array();
    for (const auto& code : solution.unplacedItems) output["unplacedItems"].push_back(outCode(code));

    output["ownPackagedItems"] = json::array();
    for (const auto& o : solution.ownPackagedItems) {
        output["ownPackagedItems"].push_back({
            {"itemCode", outCode(o.itemCode)},
            {"dimension", {{"width", o.dimension.width}, {"length", o.dimension.length}, {"depth", o.dimension.depth}}},
            {"weight", o.weight}
        });
    }

    output["violations"] = json::array();
    for (const auto& v : solution.violations) {
        output["violations"].push_back({
            {"code", v.code},
            {"itemCode", outCode(v.itemCode)},
            {"message", v.message}
        });
    }

    output["usedBoxes"] = json::array();
    for (const auto& u : solution.usedBoxes) {
        output["usedBoxes"].push_back({
            {"boxReference", u.boxReference},
            {"boxInstance", u.boxInstance},
            {"totalWeight", u.totalWeight}
        });
    }

    output["validation"] = json::object();
    output["validation"]["valid"] = validation.valid;
    output["validation"]["issues"] = json::array();
    for (const auto& v : validation.violations) {
        output["validation"]["issues"].push_back({
            {"code", v.code},
            {"itemCode", outCode(v.itemCode)},
            {"message", v.message}
        });
    }

    output["timeMs"] = elapsedMs;

    std::cout << output.dump(2);
    return 0;
}