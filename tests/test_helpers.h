#pragma once

// Tiny test helpers so we don't need any extra library.
// Each TEST(name) { ... } registers itself and the runner in test_main.cpp runs them all.
// CHECK(x) marks the test as failed (and prints where) if x is false, but keeps going.

#include <functional>
#include <iostream>
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