#include "test_helpers.h"

// The validator checks a finished packing, so here we build packings by hand
// (some good, some deliberately broken) and see if it spots the problems.

static Placement place(const std::string& code, const std::string& box, int instance,
                       int x, int y, int z, int w, int l, int d) {
    Placement p;
    p.itemCode = code;
    p.boxReference = box;
    p.boxInstance = instance;
    p.position = { x, y, z };
    p.placedDimension = { w, l, d };
    return p;
}

static UsedBox used(const std::string& box, int instance, double weight) {
    UsedBox u;
    u.boxReference = box;
    u.boxInstance = instance;
    u.totalWeight = weight;
    return u;
}

// Two 10x10x10 items side by side in one 100x100x100 box. This one is fully correct.
static void goodSetup(std::vector<Item>& items, std::vector<BoxType>& boxes, PackingSolution& s) {
    items = { makeItem("A", 10, 10, 10, 1.0), makeItem("B", 10, 10, 10, 2.0) };
    boxes = { makeBox("M", 100, 100, 100, 50.0) };
    s.placements = { place("A", "M", 1, 0, 0, 0, 10, 10, 10), place("B", "M", 1, 10, 0, 0, 10, 10, 10) };
    s.usedBoxes = { used("M", 1, 3.0) };
}

TEST(validator_accepts_a_correct_packing) {
    std::vector<Item> items; std::vector<BoxType> boxes; PackingSolution s;
    goodSetup(items, boxes, s);
    auto result = Validator::validate(s, items, boxes);
    CHECK(result.valid);
    CHECK(result.violations.empty());
}

TEST(validator_accepts_an_empty_job) {
    auto result = Validator::validate(PackingSolution{}, {}, {});
    CHECK(result.valid);
}

TEST(validator_catches_overlapping_items) {
    std::vector<Item> items; std::vector<BoxType> boxes; PackingSolution s;
    goodSetup(items, boxes, s);
    s.placements[1].position = { 5, 0, 0 };
    CHECK(hasIssue(Validator::validate(s, items, boxes), "OVERLAP"));
}

TEST(validator_items_that_only_touch_are_not_overlapping) {
    std::vector<Item> items; std::vector<BoxType> boxes; PackingSolution s;
    goodSetup(items, boxes, s);   // B starts exactly where A ends
    CHECK(!hasIssue(Validator::validate(s, items, boxes), "OVERLAP"));
}

TEST(validator_catches_items_sticking_out_of_the_box) {
    std::vector<Item> items; std::vector<BoxType> boxes; PackingSolution s;
    goodSetup(items, boxes, s);
    s.placements[1].position = { 95, 0, 0 };
    CHECK(hasIssue(Validator::validate(s, items, boxes), "OUT_OF_BOUNDS"));
}

TEST(validator_catches_negative_positions) {
    std::vector<Item> items; std::vector<BoxType> boxes; PackingSolution s;
    goodSetup(items, boxes, s);
    s.placements[0].position = { -1, 0, 0 };
    CHECK(hasIssue(Validator::validate(s, items, boxes), "NEGATIVE_POSITION"));
}

TEST(validator_catches_a_box_over_its_weight_limit) {
    std::vector<Item> items; std::vector<BoxType> boxes; PackingSolution s;
    goodSetup(items, boxes, s);
    boxes[0].maxWeight = 2.0;
    CHECK(hasIssue(Validator::validate(s, items, boxes), "BOX_OVERWEIGHT"));
}

TEST(validator_catches_wrong_reported_box_weight) {
    std::vector<Item> items; std::vector<BoxType> boxes; PackingSolution s;
    goodSetup(items, boxes, s);
    s.usedBoxes[0].totalWeight = 99.0;
    CHECK(hasIssue(Validator::validate(s, items, boxes), "WEIGHT_MISMATCH"));
}

TEST(validator_catches_an_item_that_vanished) {
    std::vector<Item> items; std::vector<BoxType> boxes; PackingSolution s;
    goodSetup(items, boxes, s);
    s.placements.pop_back();
    s.usedBoxes[0].totalWeight = 1.0;
    CHECK(hasIssue(Validator::validate(s, items, boxes), "ITEM_MISSING"));
}

TEST(validator_catches_an_item_placed_twice) {
    std::vector<Item> items; std::vector<BoxType> boxes; PackingSolution s;
    goodSetup(items, boxes, s);
    s.placements.push_back(place("A", "M", 1, 50, 50, 0, 10, 10, 10));
    s.usedBoxes[0].totalWeight = 4.0;
    CHECK(hasIssue(Validator::validate(s, items, boxes), "ITEM_PLACED_TWICE"));
}

TEST(validator_catches_an_item_both_placed_and_unplaced) {
    std::vector<Item> items; std::vector<BoxType> boxes; PackingSolution s;
    goodSetup(items, boxes, s);
    s.unplacedItems = { "A" };
    s.violations = { { "NO_VALID_PLACEMENT", "A", "test" } };
    CHECK(hasIssue(Validator::validate(s, items, boxes), "ITEM_PLACED_AND_UNPLACED"));
}

TEST(validator_unplaced_item_needs_a_reason) {
    std::vector<Item> items = { makeItem("A", 10, 10, 10, 1.0) };
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 50.0) };
    PackingSolution s;
    s.unplacedItems = { "A" };
    CHECK(hasIssue(Validator::validate(s, items, boxes), "MISSING_REASON"));
    s.violations = { { "NO_BOX_FITS", "A", "too big" } };
    CHECK(Validator::validate(s, items, boxes).valid);
}

TEST(validator_catches_unknown_items_and_boxes) {
    std::vector<Item> items; std::vector<BoxType> boxes; PackingSolution s;
    goodSetup(items, boxes, s);
    s.placements.push_back(place("GHOST", "M", 1, 50, 50, 0, 10, 10, 10));
    CHECK(hasIssue(Validator::validate(s, items, boxes), "UNKNOWN_ITEM"));

    goodSetup(items, boxes, s);
    s.placements[0].boxReference = "NOPE";
    CHECK(hasIssue(Validator::validate(s, items, boxes), "UNKNOWN_BOX_REFERENCE"));
}

TEST(validator_catches_items_in_an_inactive_box) {
    std::vector<Item> items; std::vector<BoxType> boxes; PackingSolution s;
    goodSetup(items, boxes, s);
    boxes[0].active = false;
    CHECK(hasIssue(Validator::validate(s, items, boxes), "INACTIVE_BOX"));
}

TEST(validator_catches_too_many_boxes_of_one_type) {
    std::vector<Item> items = { makeItem("A", 10, 10, 10, 1.0), makeItem("B", 10, 10, 10, 1.0) };
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 50.0) };
    boxes[0].maximumBoxes = 1;
    PackingSolution s;
    s.placements = { place("A", "M", 1, 0, 0, 0, 10, 10, 10), place("B", "M", 2, 0, 0, 0, 10, 10, 10) };
    s.usedBoxes = { used("M", 1, 1.0), used("M", 2, 1.0) };
    CHECK(hasIssue(Validator::validate(s, items, boxes), "BOX_LIMIT_EXCEEDED"));
}

TEST(validator_catches_an_opened_but_empty_box) {
    std::vector<Item> items; std::vector<BoxType> boxes; PackingSolution s;
    goodSetup(items, boxes, s);
    s.usedBoxes.push_back(used("M", 2, 0.0));
    CHECK(hasIssue(Validator::validate(s, items, boxes), "EMPTY_BOX"));
}

TEST(validator_catches_a_box_missing_from_the_used_list) {
    std::vector<Item> items; std::vector<BoxType> boxes; PackingSolution s;
    goodSetup(items, boxes, s);
    s.usedBoxes.clear();
    CHECK(hasIssue(Validator::validate(s, items, boxes), "MISSING_USED_BOX"));
}

TEST(validator_catches_a_rotation_that_is_not_allowed) {
    std::vector<Item> items = { makeFragile("F", 10, 20, 30, 1.0) };   // fragile items may not be turned
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 50.0) };
    PackingSolution s;
    s.placements = { place("F", "M", 1, 0, 0, 0, 30, 20, 10) };
    s.usedBoxes = { used("M", 1, 1.0) };
    CHECK(hasIssue(Validator::validate(s, items, boxes), "INVALID_ROTATION"));
}

TEST(validator_catches_dangerous_goods_mixed_with_ordinary_items) {
    std::vector<Item> items = { makeDangerous("D", 10, 10, 10, 1.0, "3"), makeItem("A", 10, 10, 10, 1.0) };
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 50.0) };
    PackingSolution s;
    s.placements = { place("D", "M", 1, 0, 0, 0, 10, 10, 10), place("A", "M", 1, 10, 0, 0, 10, 10, 10) };
    s.usedBoxes = { used("M", 1, 2.0) };
    CHECK(hasIssue(Validator::validate(s, items, boxes), "DANGEROUS_GOODS_MIXED"));
}

TEST(validator_catches_two_dangerous_goods_classes_in_one_box) {
    std::vector<Item> items = { makeDangerous("D", 10, 10, 10, 1.0, "3"), makeDangerous("E", 10, 10, 10, 1.0, "4") };
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 50.0) };
    PackingSolution s;
    s.placements = { place("D", "M", 1, 0, 0, 0, 10, 10, 10), place("E", "M", 1, 10, 0, 0, 10, 10, 10) };
    s.usedBoxes = { used("M", 1, 2.0) };
    CHECK(hasIssue(Validator::validate(s, items, boxes), "DANGEROUS_GOODS_MIXED"));
}

TEST(validator_catches_fragile_mixed_with_ordinary_items) {
    std::vector<Item> items = { makeFragile("F", 10, 10, 10, 1.0), makeItem("A", 10, 10, 10, 1.0) };
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 50.0) };
    PackingSolution s;
    s.placements = { place("F", "M", 1, 0, 0, 0, 10, 10, 10), place("A", "M", 1, 10, 0, 0, 10, 10, 10) };
    s.usedBoxes = { used("M", 1, 2.0) };
    CHECK(hasIssue(Validator::validate(s, items, boxes), "FRAGILE_NOT_SEPARATE"));
}

TEST(validator_catches_mixed_box_groups) {
    std::vector<Item> items = { makeGrouped("A", 10, 10, 10, 1.0, "X"), makeGrouped("B", 10, 10, 10, 1.0, "Y") };
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 50.0) };
    PackingSolution s;
    s.placements = { place("A", "M", 1, 0, 0, 0, 10, 10, 10), place("B", "M", 1, 10, 0, 0, 10, 10, 10) };
    s.usedBoxes = { used("M", 1, 2.0) };
    CHECK(hasIssue(Validator::validate(s, items, boxes), "MIXED_BOX_GROUPS"));
}

TEST(validator_catches_duplicate_item_codes_in_the_input) {
    std::vector<Item> items = { makeItem("A", 10, 10, 10, 1.0), makeItem("A", 10, 10, 10, 1.0) };
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 50.0) };
    CHECK(hasIssue(Validator::validate(PackingSolution{}, items, boxes), "DUPLICATE_ITEM_CODE"));
}