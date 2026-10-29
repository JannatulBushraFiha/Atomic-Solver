#pragma once

// Tiny test helpers so we don't need any extra library.
// Each TEST(name) { ... } registers itself and the runner in test_main.cpp runs them all.
// CHECK(x) marks the test as failed (and prints where) if x is false, but keeps going.
// REQUIRE(x) does the same but stops the test, e.g. before reading s.placements[0].

#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "BoxType.h"
#include "Item.h"
#include "PackingSolution.h"
#include "Validator.h"

struct TestCase {
    std::string name;
    std::function<void()> run;
};

inline std::vector<TestCase>& allTests() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failuresInCurrentTest() {
    static int failures = 0;
    return failures;
}

struct TestRegistrar {
    TestRegistrar(const std::string& name, std::function<void()> run) {
        allTests().push_back({ name, run });
    }
};

#define TEST(name) \
    static void name(); \
    static TestRegistrar registrar_##name(#name, name); \
    static void name()

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::cout << "    FAILED " << __FILE__ << ":" << __LINE__ << "  CHECK(" #condition ")\n"; \
            failuresInCurrentTest()++; \
        } \
    } while (0)

#define CHECK_EQ(actual, expected) \
    do { \
        auto a_ = (actual); \
        auto b_ = (expected); \
        if (!(a_ == b_)) { \
            std::cout << "    FAILED " << __FILE__ << ":" << __LINE__ << "  " #actual " == " #expected \
                      << "  (got " << a_ << ", expected " << b_ << ")\n"; \
            failuresInCurrentTest()++; \
        } \
    } while (0)

#define REQUIRE(condition) \
    do { \
        if (!(condition)) { \
            std::cout << "    FAILED " << __FILE__ << ":" << __LINE__ << "  REQUIRE(" #condition ")\n"; \
            failuresInCurrentTest()++; \
            return; \
        } \
    } while (0)

// ---- helpers to build test data quickly ----

inline Item makeItem(const std::string& code, int w, int l, int d, double weight) {
    Item item;
    item.itemCode = code;
    item.itemReference = code;
    item.itemDimension = { w, l, d };
    item.weight = weight;
    return item;
}

inline Item makeFragile(const std::string& code, int w, int l, int d, double weight) {
    Item item = makeItem(code, w, l, d, weight);
    item.isFragile = true;
    return item;
}

inline Item makeDangerous(const std::string& code, int w, int l, int d, double weight, const std::string& dgClass) {
    Item item = makeItem(code, w, l, d, weight);
    item.isDangerousGoods = true;
    item.dangerousGoodsClass = dgClass;
    return item;
}

inline Item makeGrouped(const std::string& code, int w, int l, int d, double weight, const std::string& group) {
    Item item = makeItem(code, w, l, d, weight);
    item.boxGroup = group;
    return item;
}

inline BoxType makeBox(const std::string& ref, int w, int l, int d, double maxWeight) {
    BoxType box;
    box.reference = ref;
    box.boxDimension = { w, l, d };
    box.maxWeight = maxWeight;
    return box;
}

inline bool hasViolation(const std::vector<Violation>& list, const std::string& code) {
    for (const auto& v : list) {
        if (v.code == code) return true;
    }
    return false;
}

inline bool hasIssue(const ValidationResult& result, const std::string& code) {
    return hasViolation(result.violations, code);
}

// Every input item must show up exactly once: either placed or unplaced.
inline bool everyItemAccountedFor(const PackingSolution& solution, const std::vector<Item>& items) {
    std::multiset<std::string> seen;
    for (const auto& p : solution.placements) seen.insert(p.itemCode);
    for (const auto& code : solution.unplacedItems) seen.insert(code);
    if (seen.size() != items.size()) return false;
    for (const auto& item : items) {
        if (seen.count(item.itemCode) != 1) return false;
    }
    return true;
}

// Which box (reference#instance) an item ended up in, or "" if it was not placed.
inline std::string boxOf(const PackingSolution& solution, const std::string& itemCode) {
    for (const auto& p : solution.placements) {
        if (p.itemCode == itemCode) return p.boxReference + "#" + std::to_string(p.boxInstance);
    }
    return "";
}

// The validator does not check stacking, so tests do. An item resting on another may not be
// heavier than it, and on a fragile item may weigh at most half of it (FRAGILE_MAX_LOAD_RATIO).
inline bool stackingIsLegal(const PackingSolution& solution, const std::vector<Item>& items) {
    std::map<std::string, const Item*> itemByCode;
    for (const auto& item : items) itemByCode[item.itemCode] = &item;
    for (const auto& below : solution.placements) {
        for (const auto& above : solution.placements) {
            if (below.boxReference != above.boxReference || below.boxInstance != above.boxInstance) continue;
            const bool restsOn = above.position.z == below.position.z + below.placedDimension.depth &&
                                 above.position.x < below.position.x + below.placedDimension.width &&
                                 below.position.x < above.position.x + above.placedDimension.width &&
                                 above.position.y < below.position.y + below.placedDimension.length &&
                                 below.position.y < above.position.y + above.placedDimension.length;
            if (!restsOn) continue;
            const Item* lower = itemByCode[below.itemCode];
            const Item* upper = itemByCode[above.itemCode];
            if (!lower || !upper) continue;
            if (upper->weight > lower->weight) return false;
            if (lower->isFragile && upper->weight > 0.5 * lower->weight) return false;
        }
    }
    return true;
}

// Boxes of each type must be numbered 1, 2, 3... with no gaps.
inline bool boxesNumberedFromOne(const PackingSolution& solution) {
    std::map<std::string, std::set<int>> instancesByType;
    for (const auto& u : solution.usedBoxes) instancesByType[u.boxReference].insert(u.boxInstance);
    for (const auto& entry : instancesByType) {
        int expected = 1;
        for (int instance : entry.second) {
            if (instance != expected++) return false;
        }
    }
    return true;
}