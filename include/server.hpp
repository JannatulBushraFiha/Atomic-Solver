#pragma once

#include <vector>
#include <string>
#include "json.hpp"
#include "BoxType.hpp"
#include "Item.hpp"
#include "PackingSolver.hpp"

using json = nlohmann::json;

// Parse BoxType from JSON
BoxType parseBoxType(const json& j);

// Parse Item from JSON
Item parseItem(const json& j);
