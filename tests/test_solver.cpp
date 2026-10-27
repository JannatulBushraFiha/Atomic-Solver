#include "test_helpers.h"

#include <chrono>
#include <random>

#include "PackingSolver.h"

static PackingSolution solveJob(const std::vector<Item>& items, const std::vector<BoxType>& boxes) {
    PackingSolver solver;
    return solver.solve(items, boxes);
}

// Solve and also make sure the answer passes the validator and accounts for every item.
// Most tests start with this so a broken packing can never sneak through.
static PackingSolution solveAndCheck(const std::vector<Item>& items, const std::vector<BoxType>& boxes,
                                     const Constraints& constraints = Constraints{}) {
    PackingSolver solver;
    solver.constraints = constraints;
    PackingSolution solution = solver.solve(items, boxes);
    ValidationResult result = Validator::validate(solution, items, boxes, solver.constraints);
    if (!result.valid) {
        for (const auto& v : result.violations) {
            std::cout << "    validator: " << v.code << " " << v.itemCode << " " << v.message << "\n";
        }
    }
    CHECK(result.valid);
    CHECK(everyItemAccountedFor(solution, items));
    CHECK(stackingIsLegal(solution, items));
    CHECK(boxesNumberedFromOne(solution));
    return solution;
}

// ---------- basics ----------

TEST(solver_empty_job_gives_empty_result) {
    PackingSolution s = solveJob({}, { makeBox("M", 100, 100, 100, 10.0) });
    CHECK(s.placements.empty());
    CHECK(s.unplacedItems.empty());
    CHECK(s.usedBoxes.empty());
}

TEST(solver_single_item_goes_in_one_box_at_the_corner) {
    PackingSolution s = solveAndCheck({ makeItem("A", 10, 10, 10, 1.0) }, { makeBox("M", 100, 100, 100, 10.0) });
    REQUIRE(s.placements.size() == 1);
    CHECK_EQ(s.usedBoxes.size(), static_cast<size_t>(1));
    CHECK_EQ(s.placements[0].position.x, 0);
    CHECK_EQ(s.placements[0].position.y, 0);
    CHECK_EQ(s.placements[0].position.z, 0);
}

TEST(solver_item_exactly_the_size_of_the_box_fits) {
    PackingSolution s = solveAndCheck({ makeItem("A", 100, 100, 100, 1.0) }, { makeBox("M", 100, 100, 100, 10.0) });
    CHECK_EQ(s.placements.size(), static_cast<size_t>(1));
}

TEST(solver_turns_an_item_to_make_it_fit) {
    PackingSolution s = solveAndCheck({ makeItem("A", 20, 100, 50, 1.0) }, { makeBox("M", 100, 50, 20, 10.0) });
    CHECK_EQ(s.placements.size(), static_cast<size_t>(1));
    CHECK(s.unplacedItems.empty());
}

TEST(solver_packs_many_small_items_into_few_boxes) {
    std::vector<Item> items;
    for (int i = 0; i < 27; i++) items.push_back(makeItem("I" + std::to_string(i), 10, 10, 10, 1.0));
    PackingSolution s = solveAndCheck(items, { makeBox("M", 30, 30, 30, 100.0) });
    CHECK_EQ(s.usedBoxes.size(), static_cast<size_t>(1));   // 27 cubes of 10 fill a 30 cube exactly
    CHECK(s.unplacedItems.empty());
}

// ---------- weight ----------

TEST(solver_splits_into_more_boxes_when_weight_limit_is_hit) {
    std::vector<Item> items = { makeItem("A", 10, 10, 10, 4.0), makeItem("B", 10, 10, 10, 4.0), makeItem("C", 10, 10, 10, 4.0) };
    PackingSolution s = solveAndCheck(items, { makeBox("M", 100, 100, 100, 10.0) });
    CHECK_EQ(s.usedBoxes.size(), static_cast<size_t>(2));
    for (const auto& u : s.usedBoxes) CHECK(u.totalWeight <= 10.0);
}

TEST(solver_reports_a_box_weight_that_matches_its_items) {
    PackingSolution s = solveAndCheck({ makeItem("A", 10, 10, 10, 1.5), makeItem("B", 10, 10, 10, 2.5) }, { makeBox("M", 100, 100, 100, 10.0) });
    REQUIRE(s.usedBoxes.size() == 1);
    CHECK(s.usedBoxes[0].totalWeight > 3.99 && s.usedBoxes[0].totalWeight < 4.01);
}

TEST(solver_reports_a_box_weight_that_includes_the_empty_box) {
    BoxType box = makeBox("M", 100, 100, 100, 10.0);
    box.boxWeight = 0.5;
    PackingSolution s = solveAndCheck({ makeItem("A", 10, 10, 10, 1.5), makeItem("B", 10, 10, 10, 2.5) }, { box });
    REQUIRE(s.usedBoxes.size() == 1);
    CHECK(s.usedBoxes[0].totalWeight > 4.49 && s.usedBoxes[0].totalWeight < 4.51);
}

TEST(solver_counts_the_box_weight_towards_its_limit) {
    BoxType box = makeBox("M", 100, 100, 100, 10.0);
    box.boxWeight = 2.0;
    std::vector<Item> items = { makeItem("A", 10, 10, 10, 5.0), makeItem("B", 10, 10, 10, 4.0) };   // 11 kg with the box
    PackingSolution s = solveAndCheck(items, { box });
    CHECK_EQ(s.usedBoxes.size(), static_cast<size_t>(2));
    for (const auto& u : s.usedBoxes) CHECK(u.totalWeight <= 10.0);
}

TEST(solver_item_too_heavy_once_the_box_weight_is_added_is_reported_unplaced) {
    BoxType box = makeBox("M", 100, 100, 100, 10.0);
    box.boxWeight = 2.0;
    PackingSolution s = solveAndCheck({ makeItem("A", 10, 10, 10, 9.0) }, { box });
    CHECK_EQ(s.unplacedItems.size(), static_cast<size_t>(1));
    CHECK(hasViolation(s.violations, "NO_BOX_SUPPORTS_WEIGHT"));
    CHECK(s.usedBoxes.empty());
}

TEST(solver_weight_limit_allows_for_floating_point_rounding) {
    std::vector<Item> items = { makeItem("A", 10, 10, 10, 0.1), makeItem("B", 10, 10, 10, 0.1), makeItem("C", 10, 10, 10, 0.1) };
    PackingSolution s = solveAndCheck(items, { makeBox("M", 100, 100, 100, 0.3) });
    CHECK_EQ(s.usedBoxes.size(), static_cast<size_t>(1));   // 0.1 + 0.1 + 0.1 is 0.30000000000000004
}

TEST(solver_item_heavier_than_every_box_is_reported_unplaced) {
    PackingSolution s = solveAndCheck({ makeItem("A", 10, 10, 10, 50.0) }, { makeBox("M", 100, 100, 100, 10.0) });
    CHECK_EQ(s.unplacedItems.size(), static_cast<size_t>(1));
    CHECK(hasViolation(s.violations, "NO_BOX_SUPPORTS_WEIGHT"));
}

TEST(solver_box_with_max_weight_zero_has_no_weight_limit) {
    PackingSolution s = solveAndCheck({ makeItem("A", 10, 10, 10, 500.0) }, { makeBox("M", 100, 100, 100, 0.0) });
    CHECK_EQ(s.placements.size(), static_cast<size_t>(1));
}

// ---------- items that cannot be packed ----------

TEST(solver_item_bigger_than_every_box_is_reported_unplaced) {
    PackingSolution s = solveAndCheck({ makeItem("A", 500, 10, 10, 1.0), makeItem("B", 10, 10, 10, 1.0) }, { makeBox("M", 100, 100, 100, 10.0) });
    REQUIRE(s.unplacedItems.size() == 1);
    CHECK_EQ(s.unplacedItems[0], std::string("A"));
    CHECK(hasViolation(s.violations, "NO_BOX_FITS"));
    CHECK_EQ(s.placements.size(), static_cast<size_t>(1));   // the other item still gets packed
}

TEST(solver_prohibited_dangerous_goods_are_reported_unplaced) {
    PackingSolution s = solveAndCheck({ makeDangerous("D", 10, 10, 10, 1.0, "1") }, { makeBox("M", 100, 100, 100, 10.0) });
    CHECK_EQ(s.unplacedItems.size(), static_cast<size_t>(1));
    CHECK(hasViolation(s.violations, "PROHIBITED_GOODS"));
}

TEST(solver_invalid_items_are_reported_and_do_not_stop_the_others) {
    std::vector<Item> items = { makeItem("BAD1", 0, 10, 10, 1.0), makeItem("BAD2", 10, 10, 10, -3.0), makeItem("OK", 10, 10, 10, 1.0) };
    PackingSolution s = solveAndCheck(items, { makeBox("M", 100, 100, 100, 10.0) });
    CHECK_EQ(s.unplacedItems.size(), static_cast<size_t>(2));
    CHECK_EQ(s.placements.size(), static_cast<size_t>(1));
    CHECK(hasViolation(s.violations, "INVALID_DIMENSIONS"));
    CHECK(hasViolation(s.violations, "INVALID_WEIGHT"));
}

TEST(solver_no_boxes_at_all_means_everything_is_unplaced) {
    PackingSolution s = solveAndCheck({ makeItem("A", 10, 10, 10, 1.0) }, {});
    CHECK_EQ(s.unplacedItems.size(), static_cast<size_t>(1));
    CHECK(hasViolation(s.violations, "NO_BOX_AVAILABLE"));
}

TEST(solver_fragile_item_that_only_fits_when_turned_is_reported_as_not_fitting) {
    PackingSolution s = solveAndCheck({ makeFragile("F", 20, 100, 50, 1.0) }, { makeBox("M", 100, 50, 20, 10.0) });
    CHECK_EQ(s.unplacedItems.size(), static_cast<size_t>(1));
    CHECK(hasViolation(s.violations, "NO_BOX_FITS"));
}

TEST(solver_dangerous_goods_that_only_fit_when_turned_are_reported_as_not_fitting) {
    PackingSolution s = solveAndCheck({ makeDangerous("D", 20, 100, 50, 1.0, "3") }, { makeBox("M", 100, 50, 20, 10.0) });
    CHECK_EQ(s.unplacedItems.size(), static_cast<size_t>(1));
    CHECK(hasViolation(s.violations, "NO_BOX_FITS"));
}

// ---------- box types ----------

TEST(solver_never_uses_an_inactive_box_type) {
    BoxType small = makeBox("SMALL", 50, 50, 50, 10.0);
    small.active = false;
    PackingSolution s = solveAndCheck({ makeItem("A", 10, 10, 10, 1.0) }, { small, makeBox("BIG", 200, 200, 200, 10.0) });
    REQUIRE(s.placements.size() == 1);
    CHECK_EQ(s.placements[0].boxReference, std::string("BIG"));
}

TEST(solver_respects_the_maximum_number_of_boxes) {
    BoxType box = makeBox("M", 100, 100, 100, 10.0);
    box.maximumBoxes = 1;
    std::vector<Item> items = { makeItem("A", 10, 10, 10, 8.0), makeItem("B", 10, 10, 10, 8.0) };   // can't share a box (weight)
    PackingSolution s = solveAndCheck(items, { box });
    CHECK_EQ(s.usedBoxes.size(), static_cast<size_t>(1));
    CHECK_EQ(s.unplacedItems.size(), static_cast<size_t>(1));
}

TEST(solver_uses_the_smaller_box_when_everything_fits_in_it) {
    PackingSolution s = solveAndCheck({ makeItem("A", 10, 10, 10, 1.0) }, { makeBox("BIG", 500, 500, 500, 100.0), makeBox("SMALL", 20, 20, 20, 100.0) });
    REQUIRE(s.usedBoxes.size() == 1);
    CHECK_EQ(s.usedBoxes[0].boxReference, std::string("SMALL"));
}

TEST(solver_mixes_box_types_when_the_last_box_can_be_smaller) {
    // X needs the large box, Y can't join it (weight) but fits the small one.
    std::vector<Item> items = { makeItem("X", 60, 60, 60, 6.0), makeItem("Y", 10, 10, 10, 6.0) };
    PackingSolution s = solveAndCheck(items, { makeBox("L", 100, 100, 100, 10.0), makeBox("S", 20, 20, 20, 10.0) });
    CHECK_EQ(boxOf(s, "X"), std::string("L#1"));
    CHECK_EQ(boxOf(s, "Y"), std::string("S#1"));
}

TEST(solver_box_weight_follows_a_box_swapped_for_a_smaller_one) {
    BoxType large = makeBox("L", 100, 100, 100, 10.0);
    large.boxWeight = 1.0;
    BoxType small = makeBox("S", 20, 20, 20, 10.0);
    small.boxWeight = 0.25;
    std::vector<Item> items = { makeItem("X", 60, 60, 60, 6.0), makeItem("Y", 10, 10, 10, 6.0) };
    PackingSolution s = solveAndCheck(items, { large, small });
    REQUIRE(s.usedBoxes.size() == 2);
    for (const auto& u : s.usedBoxes) {
        if (u.boxReference == "L") CHECK(u.totalWeight > 6.99 && u.totalWeight < 7.01);
        if (u.boxReference == "S") CHECK(u.totalWeight > 6.24 && u.totalWeight < 6.26);
    }
}

TEST(solver_falls_back_to_another_box_type_when_one_runs_out) {
    BoxType small = makeBox("S", 20, 20, 20, 10.0);
    small.maximumBoxes = 1;
    std::vector<Item> items = { makeItem("A", 10, 10, 10, 6.0), makeItem("B", 10, 10, 10, 6.0) };   // can't share a box (weight)
    PackingSolution s = solveAndCheck(items, { small, makeBox("L", 100, 100, 100, 10.0) });
    CHECK(s.unplacedItems.empty());
    CHECK_EQ(s.usedBoxes.size(), static_cast<size_t>(2));
    CHECK(boxOf(s, "A") == "S#1" || boxOf(s, "B") == "S#1");
}

// ---------- constraint settings ----------

TEST(solver_rotation_switched_off_leaves_an_item_that_only_fits_when_turned_unplaced) {
    Constraints c;
    c.allowRotation = false;
    PackingSolution s = solveAndCheck({ makeItem("A", 20, 100, 50, 1.0) }, { makeBox("M", 100, 50, 20, 10.0) }, c);
    CHECK_EQ(s.unplacedItems.size(), static_cast<size_t>(1));
    CHECK(hasViolation(s.violations, "NO_BOX_FITS"));
}

TEST(solver_group_rule_switched_off_lets_groups_share_a_box) {
    Constraints c;
    c.enforceBoxGroups = false;
    std::vector<Item> items = { makeGrouped("A1", 10, 10, 10, 1.0, "A"), makeGrouped("B1", 10, 10, 10, 1.0, "B") };
    PackingSolution s = solveAndCheck(items, { makeBox("M", 100, 100, 100, 50.0) }, c);
    CHECK_EQ(s.usedBoxes.size(), static_cast<size_t>(1));
}

// ---------- the separation rules ----------

TEST(solver_dangerous_goods_go_in_their_own_box) {
    std::vector<Item> items = { makeDangerous("D", 10, 10, 10, 1.0, "3"), makeItem("A", 10, 10, 10, 1.0), makeItem("B", 10, 10, 10, 1.0) };
    PackingSolution s = solveAndCheck(items, { makeBox("M", 100, 100, 100, 50.0) });
    CHECK(boxOf(s, "D") != boxOf(s, "A"));
    CHECK(boxOf(s, "D") != boxOf(s, "B"));
}

TEST(solver_different_dangerous_goods_classes_never_share_a_box) {
    std::vector<Item> items = { makeDangerous("D3", 10, 10, 10, 1.0, "3"), makeDangerous("D4", 10, 10, 10, 1.0, "4") };
    PackingSolution s = solveAndCheck(items, { makeBox("M", 100, 100, 100, 50.0) });
    CHECK(boxOf(s, "D3") != boxOf(s, "D4"));
}

TEST(solver_same_dangerous_goods_class_may_share_a_box) {
    std::vector<Item> items = { makeDangerous("D1", 10, 10, 10, 1.0, "3"), makeDangerous("D2", 10, 10, 10, 1.0, "3") };
    PackingSolution s = solveAndCheck(items, { makeBox("M", 100, 100, 100, 50.0) });
    CHECK_EQ(s.usedBoxes.size(), static_cast<size_t>(1));
}

TEST(solver_fragile_items_go_in_their_own_box) {
    std::vector<Item> items = { makeFragile("F", 10, 10, 10, 1.0), makeItem("A", 10, 10, 10, 1.0) };
    PackingSolution s = solveAndCheck(items, { makeBox("M", 100, 100, 100, 50.0) });
    CHECK(boxOf(s, "F") != boxOf(s, "A"));
}

TEST(solver_different_box_groups_never_share_a_box) {
    std::vector<Item> items = { makeGrouped("A1", 10, 10, 10, 1.0, "A"), makeGrouped("B1", 10, 10, 10, 1.0, "B"), makeGrouped("A2", 10, 10, 10, 1.0, "A") };
    PackingSolution s = solveAndCheck(items, { makeBox("M", 100, 100, 100, 50.0) });
    CHECK(boxOf(s, "A1") != boxOf(s, "B1"));
    CHECK_EQ(boxOf(s, "A1"), boxOf(s, "A2"));   // the same group can share
}

TEST(solver_items_without_a_group_can_join_any_group) {
    std::vector<Item> items = { makeGrouped("A1", 10, 10, 10, 1.0, "A"), makeItem("N1", 10, 10, 10, 1.0) };
    PackingSolution s = solveAndCheck(items, { makeBox("M", 100, 100, 100, 50.0) });
    CHECK_EQ(s.usedBoxes.size(), static_cast<size_t>(1));
}

// The 0kg bug: these used to come back invalid.
TEST(solver_zero_weight_item_does_not_let_dangerous_goods_into_its_box) {
    std::vector<Item> items = { makeItem("Z0", 300, 300, 100, 0.0), makeDangerous("D", 100, 100, 50, 2.0, "3") };
    PackingSolution s = solveAndCheck(items, { makeBox("M", 500, 400, 400, 30.0) });
    CHECK(boxOf(s, "Z0") != boxOf(s, "D"));
}

TEST(solver_zero_weight_item_does_not_let_fragile_items_into_its_box) {
    std::vector<Item> items = { makeItem("Z0", 300, 300, 100, 0.0), makeFragile("F", 90, 90, 50, 2.0) };
    PackingSolution s = solveAndCheck(items, { makeBox("M", 500, 400, 400, 30.0) });
    CHECK(boxOf(s, "Z0") != boxOf(s, "F"));
}

TEST(solver_zero_weight_dangerous_goods_box_stays_dangerous_goods_only) {
    std::vector<Item> items = { makeDangerous("D0", 300, 300, 100, 0.0, "3"), makeItem("A", 100, 100, 50, 2.0) };
    PackingSolution s = solveAndCheck(items, { makeBox("M", 500, 400, 400, 30.0) });
    CHECK(boxOf(s, "D0") != boxOf(s, "A"));
}

TEST(solver_zero_weight_fragile_box_stays_fragile_only) {
    std::vector<Item> items = { makeFragile("F0", 300, 300, 100, 0.0), makeItem("A", 100, 100, 50, 2.0) };
    PackingSolution s = solveAndCheck(items, { makeBox("M", 500, 400, 400, 30.0) });
    CHECK(boxOf(s, "F0") != boxOf(s, "A"));
}

TEST(solver_many_zero_weight_items_pack_together) {
    std::vector<Item> items;
    for (int i = 0; i < 10; i++) items.push_back(makeItem("Z" + std::to_string(i), 10, 10, 10, 0.0));
    PackingSolution s = solveAndCheck(items, { makeBox("M", 100, 100, 100, 10.0) });
    CHECK_EQ(s.usedBoxes.size(), static_cast<size_t>(1));
}

// ---------- stacking ----------

TEST(solver_heavy_item_is_never_stacked_on_a_lighter_one) {
    // Box fits two cubes on top of each other and nothing else, so one has to sit on the other.
    std::vector<Item> items = { makeItem("LIGHT", 100, 100, 100, 1.0), makeItem("HEAVY", 100, 100, 100, 5.0) };
    PackingSolution s = solveAndCheck(items, { makeBox("M", 100, 100, 200, 50.0) });
    REQUIRE(s.usedBoxes.size() == 1);
    for (const auto& p : s.placements) {
        if (p.itemCode == "HEAVY") CHECK_EQ(p.position.z, 0);
    }
}

// Makes sure the stackingIsLegal helper used by solveAndCheck really spots bad stacks.
TEST(stacking_check_spots_items_resting_on_something_they_should_not) {
    std::vector<Item> items = { makeItem("LIGHT", 10, 10, 10, 1.0), makeItem("HEAVY", 10, 10, 10, 5.0),
                                makeItem("MID", 10, 10, 10, 3.0), makeFragile("F", 10, 10, 10, 4.0) };
    PackingSolution s;
    s.placements = { { "LIGHT", "M", 1, { 0, 0, 0 }, { 10, 10, 10 } }, { "HEAVY", "M", 1, { 0, 0, 10 }, { 10, 10, 10 } } };
    CHECK(!stackingIsLegal(s, items));

    s.placements[1].position = { 10, 0, 0 };   // side by side is fine
    CHECK(stackingIsLegal(s, items));

    s.placements = { { "F", "M", 1, { 0, 0, 0 }, { 10, 10, 10 } }, { "LIGHT", "M", 1, { 0, 0, 10 }, { 10, 10, 10 } } };
    CHECK(stackingIsLegal(s, items));    // 1 kg on a 4 kg fragile item is within half
    s.placements[1].itemCode = "MID";
    CHECK(!stackingIsLegal(s, items));   // 3 kg on a 4 kg fragile item is too much
}

// ---------- whole-run properties ----------

static std::vector<Item> randomItems(unsigned seed, int count, bool mixFlags = false) {
    std::mt19937 rng(seed);   // unlike rand(), gives the same numbers on every compiler
    std::vector<Item> items;
    for (int i = 0; i < count; i++) {
        int w = 30 + rng() % 250, l = 30 + rng() % 250, d = 20 + rng() % 180;
        double weight = (rng() % 10 == 0) ? 0.0 : 0.1 + (rng() % 500) / 100.0;
        std::string code = "I" + std::to_string(i);
        int kind = rng() % 10;
        Item item;
        if (kind == 0) item = makeFragile(code, w, l, d, weight);
        else if (kind == 1) item = makeDangerous(code, w, l, d, weight, rng() % 2 ? "3" : "4");
        else if (kind == 2) item = makeGrouped(code, w, l, d, weight, rng() % 2 ? "A" : "B");
        else item = makeItem(code, w, l, d, weight);
        if (mixFlags && rng() % 3 == 0) {   // e.g. fragile dangerous goods, or a grouped fragile item
            item.boxGroup = rng() % 2 ? "A" : "B";
            if (rng() % 2) item.isFragile = true;
            if (rng() % 2) {
                item.isDangerousGoods = true;
                item.dangerousGoodsClass = rng() % 2 ? "3" : "4";
            }
        }
        items.push_back(item);
    }
    return items;
}

// Every random item fits box S on its own, so nothing should ever be left unplaced.
TEST(solver_random_jobs_always_give_valid_packings) {
    std::vector<BoxType> boxes = { makeBox("S", 300, 300, 300, 20.0), makeBox("L", 800, 600, 500, 40.0) };
    for (unsigned seed = 1; seed <= 25; seed++) {
        PackingSolution s = solveAndCheck(randomItems(seed, 40), boxes);
        CHECK(s.unplacedItems.empty());
    }
}

TEST(solver_random_jobs_with_combined_flags_stay_valid) {
    std::vector<BoxType> boxes = { makeBox("S", 300, 300, 300, 20.0), makeBox("L", 800, 600, 500, 40.0) };
    for (unsigned seed = 200; seed <= 215; seed++) {
        PackingSolution s = solveAndCheck(randomItems(seed, 40, true), boxes);
        CHECK(s.unplacedItems.empty());
    }
}

TEST(solver_random_jobs_with_box_weights_stay_valid) {
    BoxType small = makeBox("S", 300, 300, 300, 20.0);
    small.boxWeight = 0.5;
    BoxType large = makeBox("L", 800, 600, 500, 40.0);
    large.boxWeight = 1.5;
    for (unsigned seed = 300; seed <= 315; seed++) {
        PackingSolution s = solveAndCheck(randomItems(seed, 40), { small, large });
        CHECK(s.unplacedItems.empty());
    }
}

TEST(solver_random_jobs_with_rotation_switched_off_keep_every_item_as_it_is) {
    Constraints c;
    c.allowRotation = false;
    std::vector<BoxType> boxes = { makeBox("S", 300, 300, 300, 20.0), makeBox("L", 800, 600, 500, 40.0) };
    for (unsigned seed = 400; seed <= 405; seed++) {
        std::vector<Item> items = randomItems(seed, 40);
        PackingSolution s = solveAndCheck(items, boxes, c);
        CHECK(s.unplacedItems.empty());
        for (const auto& p : s.placements) {
            for (const auto& item : items) {
                if (item.itemCode != p.itemCode) continue;
                CHECK_EQ(p.placedDimension.width, item.itemDimension.width);
                CHECK_EQ(p.placedDimension.length, item.itemDimension.length);
                CHECK_EQ(p.placedDimension.depth, item.itemDimension.depth);
            }
        }
    }
}

TEST(solver_random_jobs_with_a_box_limit_stay_valid) {
    BoxType small = makeBox("S", 300, 300, 300, 20.0);
    small.maximumBoxes = 2;
    BoxType large = makeBox("L", 800, 600, 500, 40.0);
    large.maximumBoxes = 3;
    for (unsigned seed = 100; seed <= 110; seed++) {
        PackingSolution s = solveAndCheck(randomItems(seed, 60), { small, large });
        int smallCount = 0, largeCount = 0;
        for (const auto& u : s.usedBoxes) {
            if (u.boxReference == "S") smallCount++;
            if (u.boxReference == "L") largeCount++;
        }
        CHECK(smallCount <= 2);
        CHECK(largeCount <= 3);
    }
}

TEST(solver_gives_the_same_answer_every_time) {
    std::vector<BoxType> boxes = { makeBox("S", 300, 300, 300, 20.0), makeBox("L", 800, 600, 500, 40.0) };
    std::vector<Item> items = randomItems(7, 50);
    PackingSolution first = solveJob(items, boxes);
    PackingSolution second = solveJob(items, boxes);
    CHECK_EQ(first.placements.size(), second.placements.size());
    CHECK_EQ(first.usedBoxes.size(), second.usedBoxes.size());
    for (size_t i = 0; i < first.placements.size() && i < second.placements.size(); i++) {
        CHECK_EQ(first.placements[i].itemCode, second.placements[i].itemCode);
        CHECK_EQ(first.placements[i].boxReference, second.placements[i].boxReference);
        CHECK_EQ(first.placements[i].boxInstance, second.placements[i].boxInstance);
        CHECK_EQ(first.placements[i].position.x, second.placements[i].position.x);
        CHECK_EQ(first.placements[i].position.y, second.placements[i].position.y);
        CHECK_EQ(first.placements[i].position.z, second.placements[i].position.z);
        CHECK_EQ(first.placements[i].placedDimension.width, second.placements[i].placedDimension.width);
        CHECK_EQ(first.placements[i].placedDimension.length, second.placements[i].placedDimension.length);
        CHECK_EQ(first.placements[i].placedDimension.depth, second.placements[i].placedDimension.depth);
    }
}

TEST(solver_identical_items_are_each_placed_once) {
    std::vector<Item> items;
    for (int i = 0; i < 12; i++) items.push_back(makeItem("SAME" + std::to_string(i), 50, 50, 50, 1.0));
    PackingSolution s = solveAndCheck(items, { makeBox("M", 200, 200, 200, 100.0) });
    CHECK_EQ(s.usedBoxes.size(), static_cast<size_t>(1));
}

// The client wants an answer in 1 to 2 seconds. This is only a rough smoke test: it uses a
// generous limit so it does not fail on a slow laptop or an unoptimised (Debug) build.
TEST(solver_handles_a_bigger_order_in_reasonable_time) {
    std::vector<BoxType> boxes = { makeBox("S", 300, 300, 300, 20.0), makeBox("L", 800, 600, 500, 40.0) };
    std::vector<Item> items = randomItems(42, 150);
    auto start = std::chrono::steady_clock::now();
    PackingSolution s = solveAndCheck(items, boxes);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::cout << "    (150 items took " << ms << " ms)\n";
    CHECK(ms < 5000.0);
}