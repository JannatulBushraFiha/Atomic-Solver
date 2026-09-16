
#include "Validator.hpp"
#include "PackingSolver.hpp"

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// =============================================================================
// Minimal test harness (no external dependency)
// =============================================================================
namespace testkit {

struct TestFailure {
    std::string expr;
};

inline std::vector<std::pair<std::string, std::function<void()>>>& registry() {
    static std::vector<std::pair<std::string, std::function<void()>>> tests;
    return tests;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

inline int& currentFailures() {
    static int failures = 0;
    return failures;
}

inline void reportFailure(const std::string& expr, const char* file, int line) {
    currentFailures()++;
    std::cerr << "      FAIL " << file << ":" << line << "  CHECK(" << expr << ")\n";
}

} // namespace testkit

#define TEST_CASE(name)                                                        \
    static void name();                                                        \
    static ::testkit::Registrar registrar_##name(#name, name);                  \
    static void name()

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) { ::testkit::reportFailure(#cond, __FILE__, __LINE__); }  \
    } while (0)

#define REQUIRE(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ::testkit::reportFailure(#cond, __FILE__, __LINE__);               \
            throw ::testkit::TestFailure{#cond};                               \
        }                                                                      \
    } while (0)

// =============================================================================
// Fixture builders — ADJUST ME if your real field/constructor names differ
// =============================================================================
namespace fixtures {

Dimension dim(int width, int length, int depth) {
    Dimension d;
    d.width = width;
    d.length = length;
    d.depth = depth;
    return d;
}

Position pos(int x, int y, int z) {
    Position p;
    p.x = x;
    p.y = y;
    p.z = z;
    return p;
}

Item makeItem(std::string itemCode, Dimension itemDimension, double weight, std::string boxGroup = "") {
    Item item;
    item.itemCode = std::move(itemCode);
    item.itemDimension = itemDimension;
    item.weight = weight;
    item.boxGroup = std::move(boxGroup);
    return item;
}

BoxType makeBox(std::string reference, Dimension boxDimension, double maxWeight,
                int maximumBoxes = -1, bool active = true) {
    BoxType box;
    box.reference = std::move(reference);
    box.boxDimension = boxDimension;
    box.maxWeight = maxWeight;
    box.maximumBoxes = maximumBoxes;
    box.active = active;
    return box;
}

Placement makePlacement(std::string itemCode, std::string boxReference, int boxInstance,
                         Position position, Dimension placedDimension) {
    Placement p;
    p.itemCode = std::move(itemCode);
    p.boxReference = std::move(boxReference);
    p.boxInstance = boxInstance;
    p.position = position;
    p.placedDimension = placedDimension;
    return p;
}

// TODO(constraints): this is the one piece I genuinely can't infer from the
// two .cpp files alone. Wire up whatever your real Constraints setup needs
// (e.g. rotation rules, per-item checkItem rejection rules). For now this
// just default-constructs one and sets the single field we know about.
Constraints makeConstraints(bool enforceBoxGroups = false) {
    Constraints c;
    c.enforceBoxGroups = enforceBoxGroups;
    return c;
}

bool hasViolation(const ValidationResult& r, const std::string& code) {
    for (const auto& v : r.violations) {
        if (v.code == code) return true;
    }
    return false;
}

bool hasViolation(const ValidationResult& r, const std::string& code, const std::string& itemCode) {
    for (const auto& v : r.violations) {
        if (v.code == code && v.itemCode == itemCode) return true;
    }
    return false;
}

int countViolations(const ValidationResult& r, const std::string& code) {
    int n = 0;
    for (const auto& v : r.violations) {
        if (v.code == code) n++;
    }
    return n;
}

// Prints every violation — call this from a REQUIRE/CHECK failure path when
// debugging a test, e.g. `if (!result.valid) fixtures::dump(result);`
void dump(const ValidationResult& r) {
    std::cerr << "    violations (" << r.violations.size() << "):\n";
    for (const auto& v : r.violations) {
        std::cerr << "      [" << v.code << "] item=" << v.itemCode << " : " << v.message << "\n";
    }
}

} // namespace fixtures

using namespace fixtures;

// =============================================================================
// SECTION 1 — Validator unit tests (hand-built solutions, solver not involved)
// =============================================================================

TEST_CASE(Validator_ValidSingleItemSolution_PassesCleanly) {
    Item item = makeItem("ITEM-1", dim(10, 10, 10), 2.0);
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0, -1, true);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-1", "BOX-S", 1, pos(0, 0, 0), item.itemDimension));
    solution.usedBoxes.push_back({"BOX-S", 1, 2.0});

    ValidationResult result = Validator::validate(solution, {item}, {box}, constraints);
    if (!result.valid) dump(result);
    CHECK(result.valid);
    CHECK(result.violations.empty());
}

TEST_CASE(Validator_DuplicateItemCodeInInput) {
    Item a = makeItem("ITEM-1", dim(1, 1, 1), 1.0);
    Item b = makeItem("ITEM-1", dim(2, 2, 2), 1.0); // same code, different item
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution; // deliberately empty placements — we're only testing input validation
    solution.unplacedItems.push_back("ITEM-1");
    solution.violations.push_back({"NO_SPACE_AVAILABLE", "ITEM-1", "not tested here"});

    ValidationResult result = Validator::validate(solution, {a, b}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "DUPLICATE_ITEM_CODE", "ITEM-1"));
}

TEST_CASE(Validator_UnknownItemInPlacement) {
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("GHOST-ITEM", "BOX-S", 1, pos(0, 0, 0), dim(5, 5, 5)));
    solution.usedBoxes.push_back({"BOX-S", 1, 0.0});

    ValidationResult result = Validator::validate(solution, /*items=*/{}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "UNKNOWN_ITEM", "GHOST-ITEM"));
}

TEST_CASE(Validator_InvalidRotation) {
    // Native item dimension is 4x6x8. We place it as 6x4x9 — not just a swap,
    // 9 doesn't appear anywhere in the original dims, so no reasonable
    // permittedRotations() implementation should accept this.
    Item item = makeItem("ITEM-1", dim(4, 6, 8), 1.0);
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-1", "BOX-S", 1, pos(0, 0, 0), dim(6, 4, 9)));
    solution.usedBoxes.push_back({"BOX-S", 1, 1.0});

    ValidationResult result = Validator::validate(solution, {item}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "INVALID_ROTATION", "ITEM-1"));
}

TEST_CASE(Validator_NegativePosition) {
    Item item = makeItem("ITEM-1", dim(4, 4, 4), 1.0);
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-1", "BOX-S", 1, pos(-1, 0, 0), item.itemDimension));
    solution.usedBoxes.push_back({"BOX-S", 1, 1.0});

    ValidationResult result = Validator::validate(solution, {item}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "NEGATIVE_POSITION", "ITEM-1"));
}

TEST_CASE(Validator_InvalidBoxInstanceZero) {
    Item item = makeItem("ITEM-1", dim(4, 4, 4), 1.0);
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-1", "BOX-S", 0, pos(0, 0, 0), item.itemDimension));
    solution.usedBoxes.push_back({"BOX-S", 0, 1.0});

    ValidationResult result = Validator::validate(solution, {item}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "INVALID_BOX_INSTANCE", "ITEM-1"));
}

TEST_CASE(Validator_UnknownBoxReference) {
    Item item = makeItem("ITEM-1", dim(4, 4, 4), 1.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-1", "NO-SUCH-BOX", 1, pos(0, 0, 0), item.itemDimension));
    solution.usedBoxes.push_back({"NO-SUCH-BOX", 1, 1.0});

    ValidationResult result = Validator::validate(solution, {item}, /*boxes=*/{}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "UNKNOWN_BOX_REFERENCE", "ITEM-1"));
}

TEST_CASE(Validator_InactiveBoxUsed) {
    Item item = makeItem("ITEM-1", dim(4, 4, 4), 1.0);
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0, -1, /*active=*/false);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-1", "BOX-S", 1, pos(0, 0, 0), item.itemDimension));
    solution.usedBoxes.push_back({"BOX-S", 1, 1.0});

    ValidationResult result = Validator::validate(solution, {item}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "INACTIVE_BOX", "ITEM-1"));
}

TEST_CASE(Validator_OutOfBounds) {
    Item item = makeItem("ITEM-1", dim(10, 10, 10), 1.0);
    BoxType box = makeBox("BOX-S", dim(8, 8, 8), 50.0); // box smaller than item
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-1", "BOX-S", 1, pos(0, 0, 0), item.itemDimension));
    solution.usedBoxes.push_back({"BOX-S", 1, 1.0});

    ValidationResult result = Validator::validate(solution, {item}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "OUT_OF_BOUNDS", "ITEM-1"));
}

TEST_CASE(Validator_OverlappingItems) {
    Item a = makeItem("ITEM-A", dim(10, 10, 10), 1.0);
    Item b = makeItem("ITEM-B", dim(10, 10, 10), 1.0);
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-A", "BOX-S", 1, pos(0, 0, 0), a.itemDimension));
    solution.placements.push_back(makePlacement("ITEM-B", "BOX-S", 1, pos(5, 5, 5), b.itemDimension)); // overlaps A
    solution.usedBoxes.push_back({"BOX-S", 1, 2.0});

    ValidationResult result = Validator::validate(solution, {a, b}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "OVERLAP"));
}

TEST_CASE(Validator_FlushAdjacentItems_NoFalseOverlap) {
    // Items sitting flush against each other (sharing a face) must NOT be
    // flagged — this guards the strict-inequality logic in overlaps().
    Item a = makeItem("ITEM-A", dim(10, 10, 10), 1.0);
    Item b = makeItem("ITEM-B", dim(10, 10, 10), 1.0);
    BoxType box = makeBox("BOX-S", dim(20, 10, 10), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-A", "BOX-S", 1, pos(0, 0, 0), a.itemDimension));
    solution.placements.push_back(makePlacement("ITEM-B", "BOX-S", 1, pos(10, 0, 0), b.itemDimension)); // touches at x=10
    solution.usedBoxes.push_back({"BOX-S", 1, 2.0});

    ValidationResult result = Validator::validate(solution, {a, b}, {box}, constraints);
    if (!result.valid) dump(result);
    CHECK(!hasViolation(result, "OVERLAP"));
}

TEST_CASE(Validator_MixedBoxGroupsWhenEnforced) {
    Item a = makeItem("ITEM-A", dim(5, 5, 5), 1.0, "FOOD");
    Item b = makeItem("ITEM-B", dim(5, 5, 5), 1.0, "CHEMICAL");
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0);
    Constraints constraints = makeConstraints(/*enforceBoxGroups=*/true);

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-A", "BOX-S", 1, pos(0, 0, 0), a.itemDimension));
    solution.placements.push_back(makePlacement("ITEM-B", "BOX-S", 1, pos(10, 10, 10), b.itemDimension));
    solution.usedBoxes.push_back({"BOX-S", 1, 2.0});

    ValidationResult result = Validator::validate(solution, {a, b}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "MIXED_BOX_GROUPS"));
}

TEST_CASE(Validator_MixedBoxGroupsAllowedWhenNotEnforced) {
    Item a = makeItem("ITEM-A", dim(5, 5, 5), 1.0, "FOOD");
    Item b = makeItem("ITEM-B", dim(5, 5, 5), 1.0, "CHEMICAL");
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0);
    Constraints constraints = makeConstraints(/*enforceBoxGroups=*/false);

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-A", "BOX-S", 1, pos(0, 0, 0), a.itemDimension));
    solution.placements.push_back(makePlacement("ITEM-B", "BOX-S", 1, pos(10, 10, 10), b.itemDimension));
    solution.usedBoxes.push_back({"BOX-S", 1, 2.0});

    ValidationResult result = Validator::validate(solution, {a, b}, {box}, constraints);
    if (!result.valid) dump(result);
    CHECK(!hasViolation(result, "MIXED_BOX_GROUPS"));
}

TEST_CASE(Validator_DuplicateBoxInstanceInUsedBoxes) {
    Item item = makeItem("ITEM-1", dim(4, 4, 4), 1.0);
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-1", "BOX-S", 1, pos(0, 0, 0), item.itemDimension));
    solution.usedBoxes.push_back({"BOX-S", 1, 1.0});
    solution.usedBoxes.push_back({"BOX-S", 1, 1.0}); // duplicate

    ValidationResult result = Validator::validate(solution, {item}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "DUPLICATE_BOX_INSTANCE"));
}

TEST_CASE(Validator_EmptyBoxListedInUsedBoxes) {
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution; // no placements at all
    solution.usedBoxes.push_back({"BOX-S", 1, 0.0}); // but box is "opened"

    ValidationResult result = Validator::validate(solution, /*items=*/{}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "EMPTY_BOX"));
}

TEST_CASE(Validator_WeightMismatchBetweenReportedAndActual) {
    Item item = makeItem("ITEM-1", dim(4, 4, 4), 3.0);
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-1", "BOX-S", 1, pos(0, 0, 0), item.itemDimension));
    solution.usedBoxes.push_back({"BOX-S", 1, 999.0}); // wrong — actual is 3.0

    ValidationResult result = Validator::validate(solution, {item}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "WEIGHT_MISMATCH"));
}

TEST_CASE(Validator_BoxOverweight) {
    Item item = makeItem("ITEM-1", dim(4, 4, 4), 999.0);
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), /*maxWeight=*/10.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-1", "BOX-S", 1, pos(0, 0, 0), item.itemDimension));
    solution.usedBoxes.push_back({"BOX-S", 1, 999.0});

    ValidationResult result = Validator::validate(solution, {item}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "BOX_OVERWEIGHT"));
}

TEST_CASE(Validator_MissingUsedBoxForOccupiedBox) {
    Item item = makeItem("ITEM-1", dim(4, 4, 4), 1.0);
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-1", "BOX-S", 1, pos(0, 0, 0), item.itemDimension));
    // usedBoxes deliberately left empty

    ValidationResult result = Validator::validate(solution, {item}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "MISSING_USED_BOX"));
}

TEST_CASE(Validator_BoxLimitExceeded) {
    Item a = makeItem("ITEM-A", dim(4, 4, 4), 1.0);
    Item b = makeItem("ITEM-B", dim(4, 4, 4), 1.0);
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0, /*maximumBoxes=*/1);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-A", "BOX-S", 1, pos(0, 0, 0), a.itemDimension));
    solution.placements.push_back(makePlacement("ITEM-B", "BOX-S", 2, pos(0, 0, 0), b.itemDimension));
    solution.usedBoxes.push_back({"BOX-S", 1, 1.0});
    solution.usedBoxes.push_back({"BOX-S", 2, 1.0}); // 2 instances used, limit is 1

    ValidationResult result = Validator::validate(solution, {a, b}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "BOX_LIMIT_EXCEEDED"));
}

TEST_CASE(Validator_DuplicateUnplacedItem) {
    Item item = makeItem("ITEM-1", dim(4, 4, 4), 1.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.unplacedItems.push_back("ITEM-1");
    solution.unplacedItems.push_back("ITEM-1"); // duplicate
    solution.violations.push_back({"NO_SPACE_AVAILABLE", "ITEM-1", "no room"});

    ValidationResult result = Validator::validate(solution, {item}, /*boxes=*/{}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "DUPLICATE_UNPLACED_ITEM", "ITEM-1"));
}

TEST_CASE(Validator_ItemPlacedTwice) {
    Item item = makeItem("ITEM-1", dim(4, 4, 4), 1.0);
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-1", "BOX-S", 1, pos(0, 0, 0), item.itemDimension));
    solution.placements.push_back(makePlacement("ITEM-1", "BOX-S", 1, pos(10, 10, 10), item.itemDimension));
    solution.usedBoxes.push_back({"BOX-S", 1, 2.0});

    ValidationResult result = Validator::validate(solution, {item}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "ITEM_PLACED_TWICE", "ITEM-1"));
}

TEST_CASE(Validator_ItemPlacedAndUnplaced) {
    Item item = makeItem("ITEM-1", dim(4, 4, 4), 1.0);
    BoxType box = makeBox("BOX-S", dim(20, 20, 20), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.placements.push_back(makePlacement("ITEM-1", "BOX-S", 1, pos(0, 0, 0), item.itemDimension));
    solution.usedBoxes.push_back({"BOX-S", 1, 1.0});
    solution.unplacedItems.push_back("ITEM-1");
    solution.violations.push_back({"NO_SPACE_AVAILABLE", "ITEM-1", "contradiction"});

    ValidationResult result = Validator::validate(solution, {item}, {box}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "ITEM_PLACED_AND_UNPLACED", "ITEM-1"));
}

TEST_CASE(Validator_ItemMissingEntirely) {
    Item item = makeItem("ITEM-1", dim(4, 4, 4), 1.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution; // ITEM-1 appears nowhere

    ValidationResult result = Validator::validate(solution, {item}, /*boxes=*/{}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "ITEM_MISSING", "ITEM-1"));
}

TEST_CASE(Validator_UnplacedItemMissingReason) {
    Item item = makeItem("ITEM-1", dim(4, 4, 4), 1.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    solution.unplacedItems.push_back("ITEM-1");
    // no matching entry in solution.violations

    ValidationResult result = Validator::validate(solution, {item}, /*boxes=*/{}, constraints);
    CHECK(!result.valid);
    CHECK(hasViolation(result, "MISSING_REASON", "ITEM-1"));
}

// =============================================================================
// SECTION 2 — PackingSolver unit tests (solver only, output not yet validated)
// =============================================================================

TEST_CASE(Solver_SingleItemFitsInSmallestSufficientBox) {
    Item item = makeItem("ITEM-1", dim(5, 5, 5), 1.0);
    BoxType small = makeBox("BOX-S", dim(10, 10, 10), 50.0);
    BoxType large = makeBox("BOX-L", dim(100, 100, 100), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution = PackingSolver::solve({item}, {small, large}, constraints);

    REQUIRE(solution.placements.size() == 1);
    CHECK(solution.placements[0].itemCode == "ITEM-1");
    CHECK(solution.placements[0].boxReference == "BOX-S"); // smaller box preferred
    CHECK(solution.unplacedItems.empty());
}

TEST_CASE(Solver_MultipleSmallItemsShareOneBoxWhenTheyFit) {
    Item a = makeItem("ITEM-A", dim(5, 5, 5), 1.0);
    Item b = makeItem("ITEM-B", dim(5, 5, 5), 1.0);
    BoxType box = makeBox("BOX-S", dim(10, 10, 10), 50.0, -1); // room for both
    Constraints constraints = makeConstraints();

    PackingSolution solution = PackingSolver::solve({a, b}, {box}, constraints);

    CHECK(solution.unplacedItems.empty());
    REQUIRE(solution.placements.size() == 2);
    // Both should land in the same single box instance rather than opening a second one.
    CHECK(solution.placements[0].boxInstance == solution.placements[1].boxInstance);
    CHECK(solution.usedBoxes.size() == 1);
}

TEST_CASE(Solver_OpensSecondBoxInstanceWhenFirstIsFull) {
    // Box only has room for one item at a time.
    Item a = makeItem("ITEM-A", dim(10, 10, 10), 1.0);
    Item b = makeItem("ITEM-B", dim(10, 10, 10), 1.0);
    BoxType box = makeBox("BOX-S", dim(10, 10, 10), 50.0, -1);
    Constraints constraints = makeConstraints();

    PackingSolution solution = PackingSolver::solve({a, b}, {box}, constraints);

    CHECK(solution.unplacedItems.empty());
    REQUIRE(solution.usedBoxes.size() == 2);
}

TEST_CASE(Solver_RespectsBoxWeightLimit) {
    Item a = makeItem("ITEM-A", dim(2, 2, 2), 8.0);
    Item b = makeItem("ITEM-B", dim(2, 2, 2), 8.0);
    BoxType box = makeBox("BOX-S", dim(50, 50, 50), /*maxWeight=*/10.0, -1); // both fit spatially, not by weight
    Constraints constraints = makeConstraints();

    PackingSolution solution = PackingSolver::solve({a, b}, {box}, constraints);

    CHECK(solution.unplacedItems.empty());
    for (const auto& used : solution.usedBoxes) {
        CHECK(used.totalWeight <= box.maxWeight + 1e-9);
    }
    REQUIRE(solution.usedBoxes.size() == 2); // forced into separate boxes by weight
}

TEST_CASE(Solver_RespectsMaximumBoxesLimit_ExtraItemBecomesUnplaced) {
    Item a = makeItem("ITEM-A", dim(10, 10, 10), 1.0);
    Item b = makeItem("ITEM-B", dim(10, 10, 10), 1.0);
    BoxType box = makeBox("BOX-S", dim(10, 10, 10), 50.0, /*maximumBoxes=*/1);
    Constraints constraints = makeConstraints();

    PackingSolution solution = PackingSolver::solve({a, b}, {box}, constraints);

    CHECK(solution.placements.size() == 1);
    REQUIRE(solution.unplacedItems.size() == 1);
    CHECK(!solution.violations.empty());
}

TEST_CASE(Solver_ItemTooLargeForAnyBox_BecomesUnplacedWithReason) {
    Item giant = makeItem("ITEM-GIANT", dim(1000, 1000, 1000), 1.0);
    BoxType box = makeBox("BOX-S", dim(10, 10, 10), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution = PackingSolver::solve({giant}, {box}, constraints);

    CHECK(solution.placements.empty());
    REQUIRE(solution.unplacedItems.size() == 1);
    CHECK(solution.unplacedItems[0] == "ITEM-GIANT");
    REQUIRE(!solution.violations.empty());
    CHECK(solution.violations[0].itemCode == "ITEM-GIANT");
}

TEST_CASE(Solver_InactiveBoxTypeNeverUsed) {
    Item item = makeItem("ITEM-1", dim(5, 5, 5), 1.0);
    BoxType inactive = makeBox("BOX-INACTIVE", dim(100, 100, 100), 50.0, -1, /*active=*/false);
    BoxType active = makeBox("BOX-ACTIVE", dim(100, 100, 100), 50.0);
    Constraints constraints = makeConstraints();

    PackingSolution solution = PackingSolver::solve({item}, {inactive, active}, constraints);

    REQUIRE(solution.placements.size() == 1);
    CHECK(solution.placements[0].boxReference == "BOX-ACTIVE");
}

// =============================================================================
// SECTION 3 — Integration tests: solve() output fed straight into validate()
// This is the core of what was asked: proving the two modules still agree
// with each other once wired together end-to-end.
// =============================================================================

// Small helper shared by the integration tests below.
static ValidationResult solveAndValidate(const std::vector<Item>& items,
                                          const std::vector<BoxType>& boxes,
                                          const Constraints& constraints,
                                          PackingSolution* outSolution = nullptr) {
    PackingSolution solution = PackingSolver::solve(items, boxes, constraints);
    ValidationResult result = Validator::validate(solution, items, boxes, constraints);
    if (outSolution) *outSolution = solution;
    return result;
}

TEST_CASE(Integration_SmallOrder_SolverOutputPassesValidator) {
    std::vector<Item> items = {
        makeItem("ITEM-1", dim(5, 5, 5), 1.0),
        makeItem("ITEM-2", dim(3, 3, 3), 0.5),
        makeItem("ITEM-3", dim(8, 4, 2), 2.0),
    };
    std::vector<BoxType> boxes = {
        makeBox("BOX-S", dim(10, 10, 10), 20.0),
        makeBox("BOX-L", dim(30, 30, 30), 50.0),
    };
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    ValidationResult result = solveAndValidate(items, boxes, constraints, &solution);

    if (!result.valid) dump(result);
    CHECK(result.valid);
    CHECK(solution.unplacedItems.empty());
}

TEST_CASE(Integration_MultipleBoxTypesAndInstances_SolverOutputPassesValidator) {
    std::vector<Item> items;
    for (int i = 0; i < 12; i++) {
        items.push_back(makeItem("ITEM-" + std::to_string(i), dim(4, 4, 4), 1.5));
    }
    std::vector<BoxType> boxes = {
        makeBox("BOX-S", dim(8, 8, 8), 10.0, -1),
        makeBox("BOX-M", dim(16, 16, 16), 30.0, -1),
    };
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    ValidationResult result = solveAndValidate(items, boxes, constraints, &solution);

    if (!result.valid) dump(result);
    CHECK(result.valid);
}

TEST_CASE(Integration_WeightConstrainedOrder_ForcesMultipleBoxes_StillValid) {
    std::vector<Item> items;
    for (int i = 0; i < 6; i++) {
        items.push_back(makeItem("ITEM-" + std::to_string(i), dim(2, 2, 2), 4.0));
    }
    std::vector<BoxType> boxes = {
        makeBox("BOX-S", dim(50, 50, 50), /*maxWeight=*/9.0, -1), // spatially huge, weight-limited
    };
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    ValidationResult result = solveAndValidate(items, boxes, constraints, &solution);

    if (!result.valid) dump(result);
    CHECK(result.valid);
    CHECK(solution.usedBoxes.size() >= 3); // 6 items * 4.0 weight over a 9.0 limit needs >=3 boxes
}

TEST_CASE(Integration_UnplaceableItem_StillProducesValidSolution) {
    std::vector<Item> items = {
        makeItem("ITEM-FITS", dim(4, 4, 4), 1.0),
        makeItem("ITEM-TOO-BIG", dim(999, 999, 999), 1.0),
    };
    std::vector<BoxType> boxes = {
        makeBox("BOX-S", dim(10, 10, 10), 20.0),
    };
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    ValidationResult result = solveAndValidate(items, boxes, constraints, &solution);

    if (!result.valid) dump(result);
    CHECK(result.valid); // unplaced-with-reason is a legitimate valid outcome
    CHECK(solution.placements.size() == 1);
    REQUIRE(solution.unplacedItems.size() == 1);
    CHECK(solution.unplacedItems[0] == "ITEM-TOO-BIG");
}

TEST_CASE(Integration_BoxGroupEnforcement_SolverAndValidatorAgree) {
    std::vector<Item> items = {
        makeItem("FOOD-1", dim(4, 4, 4), 1.0, "FOOD"),
        makeItem("FOOD-2", dim(4, 4, 4), 1.0, "FOOD"),
        makeItem("CHEM-1", dim(4, 4, 4), 1.0, "CHEMICAL"),
    };
    std::vector<BoxType> boxes = {
        makeBox("BOX-S", dim(20, 20, 20), 50.0, -1), // big enough that FOOD+CHEM could share if groups weren't enforced
    };
    Constraints constraints = makeConstraints(/*enforceBoxGroups=*/true);

    PackingSolution solution;
    ValidationResult result = solveAndValidate(items, boxes, constraints, &solution);

    if (!result.valid) dump(result);
    CHECK(result.valid);
    // NOTE: this only holds if PackingSolver actually reads Constraints.enforceBoxGroups
    // when deciding placements. If this fails, it's telling you the solver and
    // validator disagree on box-group rules — exactly the kind of integration bug
    // this test file exists to catch.
}

TEST_CASE(Integration_EmptyOrder_ProducesEmptyValidSolution) {
    std::vector<Item> items;
    std::vector<BoxType> boxes = { makeBox("BOX-S", dim(10, 10, 10), 20.0) };
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    ValidationResult result = solveAndValidate(items, boxes, constraints, &solution);

    CHECK(result.valid);
    CHECK(solution.placements.empty());
    CHECK(solution.usedBoxes.empty());
    CHECK(solution.unplacedItems.empty());
}

TEST_CASE(Integration_NoBoxesAvailable_EveryItemUnplacedWithReason) {
    std::vector<Item> items = { makeItem("ITEM-1", dim(4, 4, 4), 1.0) };
    std::vector<BoxType> boxes; // none at all
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    ValidationResult result = solveAndValidate(items, boxes, constraints, &solution);

    if (!result.valid) dump(result);
    CHECK(result.valid);
    REQUIRE(solution.unplacedItems.size() == 1);
}

TEST_CASE(Integration_LargerRandomizedOrder_SolverOutputPassesValidator) {
    // Deterministic pseudo-random spread of item sizes/weights — a broad
    // regression net for integration bugs that only show up with volume.
    std::vector<Item> items;
    unsigned seed = 12345u;
    auto nextRand = [&seed]() {
        seed = seed * 1103515245u + 12345u;
        return (seed / 65536u) % 32768u;
    };
    for (int i = 0; i < 50; i++) {
        int w = 1 + static_cast<int>(nextRand() % 6);
        int l = 1 + static_cast<int>(nextRand() % 6);
        int d = 1 + static_cast<int>(nextRand() % 6);
        double weight = 0.1 + (nextRand() % 50) / 10.0;
        items.push_back(makeItem("ITEM-" + std::to_string(i), dim(w, l, d), weight));
    }
    std::vector<BoxType> boxes = {
        makeBox("BOX-S", dim(8, 8, 8), 15.0, -1),
        makeBox("BOX-M", dim(15, 15, 15), 40.0, -1),
        makeBox("BOX-L", dim(30, 30, 30), 100.0, -1),
    };
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    ValidationResult result = solveAndValidate(items, boxes, constraints, &solution);

    if (!result.valid) dump(result);
    CHECK(result.valid);
}

TEST_CASE(Integration_ZeroWeightItems_StillValid) {
    std::vector<Item> items = {
        makeItem("ITEM-1", dim(4, 4, 4), 0.0),
        makeItem("ITEM-2", dim(4, 4, 4), 0.0),
    };
    std::vector<BoxType> boxes = { makeBox("BOX-S", dim(10, 10, 10), 0.0, -1) }; // 0 = "no limit" in some schemes; adjust if not
    Constraints constraints = makeConstraints();

    PackingSolution solution;
    ValidationResult result = solveAndValidate(items, boxes, constraints, &solution);

    if (!result.valid) dump(result);
    CHECK(result.valid);
}

// =============================================================================
// Runner
// =============================================================================
int main() {
    int passed = 0;
    int failed = 0;

    for (auto& [name, fn] : testkit::registry()) {
        testkit::currentFailures() = 0;
        std::cout << "[ RUN  ] " << name << "\n";
        try {
            fn();
        } catch (const testkit::TestFailure&) {
            // already reported by REQUIRE
        } catch (const std::exception& e) {
            testkit::currentFailures()++;
            std::cerr << "      threw std::exception: " << e.what() << "\n";
        } catch (...) {
            testkit::currentFailures()++;
            std::cerr << "      threw unknown exception\n";
        }

        if (testkit::currentFailures() == 0) {
            std::cout << "[  OK  ] " << name << "\n";
            passed++;
        } else {
            std::cout << "[ FAIL ] " << name << "\n";
            failed++;
        }
    }

    std::cout << "\n---------------------------------------------\n";
    std::cout << passed << " passed, " << failed << " failed, "
              << (passed + failed) << " total\n";

    return failed == 0 ? 0 : 1;
}
