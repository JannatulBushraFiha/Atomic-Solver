#include <iostream>
#include <sstream>
#include <string>

//#include "json.hpp"
#include "BoxType.hpp"
#include "Item.hpp"
#include "PackingSolver.hpp"
#include "server.hpp"

/*
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
    return item;
}
*/

int main() {
    // Start the server on port 8080
    startServer(8080);

    return 0;
}