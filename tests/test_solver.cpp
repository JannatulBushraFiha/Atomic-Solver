#include "test_helpers.h"

#include <chrono>
#include <cstdlib>

#include "PackingSolver.h"

static PackingSolution solveJob(const std::vector<Item>& items, const std::vector<BoxType>& boxes) {
    PackingSolver solver;
    return solver.solve(items, boxes);
}

// Solve and also make sure the answer passes the validator and accounts for every item.
// Most tests start with this so a broken packing can never sneak through.
static PackingSolution solveAndCheck(const std::vector<Item>& items, const std::vector<BoxType>& boxes) {
    PackingSolver solver;
    PackingSolution solution = solver.solve(items, boxes);
    ValidationResult result = Validator::validate(solution, items, boxes, solver.constraints);
    if (!result.valid) {
        for (const auto& v : result.violations) {
            std::cout << "    validator: " << v.code << " " << v.itemCode << " " << v.message << "\n";
        }
    }
    CHECK(result.valid);
    CHECK(everyItemAccountedFor(solution, items));
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
    CHECK_EQ(s.placements.size(), static_cast<size_t>(1));
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
    CHECK_EQ(s.usedBoxes.size(), static_cast<size_t>(1));
    CHECK(s.usedBoxes[0].totalWeight > 3.99 && s.usedBoxes[0].totalWeight < 4.01);
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
    CHECK_EQ(s.unplacedItems.size(), static_cast<size_t>(1));
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

// ---------- box types ----------

TEST(solver_never_uses_an_inactive_box_type) {
    BoxType small = makeBox("SMALL", 50, 50, 50, 10.0);
    small.active = false;
    PackingSolution s = solveAndCheck({ makeItem("A", 10, 10, 10, 1.0) }, { small, makeBox("BIG", 200, 200, 200, 10.0) });
    CHECK_EQ(s.placements.size(), static_cast<size_t>(1));
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
    CHECK_EQ(s.usedBoxes.size(), static_cast<size_t>(1));
    CHECK_EQ(s.usedBoxes[0].boxReference, std::string("SMALL"));
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
    for (const auto& p : s.placements) {
        if (p.itemCode == "HEAVY") CHECK_EQ(p.position.z, 0);
    }
}

// ---------- whole-run properties ----------

static std::vector<Item> randomItems(unsigned seed, int count) {
    srand(seed);
    std::vector<Item> items;
    for (int i = 0; i < count; i++) {
        int w = 30 + rand() % 250, l = 30 + rand() % 250, d = 20 + rand() % 180;
        double weight = (rand() % 10 == 0) ? 0.0 : 0.1 + (rand() % 500) / 100.0;
        std::string code = "I" + std::to_string(i);
        int kind = rand() % 10;
        if (kind == 0) items.push_back(makeFragile(code, w, l, d, weight));
        else if (kind == 1) items.push_back(makeDangerous(code, w, l, d, weight, rand() % 2 ? "3" : "4"));
        else if (kind == 2) items.push_back(makeGrouped(code, w, l, d, weight, rand() % 2 ? "A" : "B"));
        else items.push_back(makeItem(code, w, l, d, weight));
    }
    return items;
}

TEST(solver_random_jobs_always_give_valid_packings) {
    std::vector<BoxType> boxes = { makeBox("S", 300, 300, 300, 20.0), makeBox("L", 800, 600, 500, 40.0) };
    for (unsigned seed = 1; seed <= 25; seed++) {
        solveAndCheck(randomItems(seed, 40), boxes);
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
        CHECK_EQ(first.placements[i].position.x, second.placements[i].position.x);
        CHECK_EQ(first.placements[i].position.y, second.placements[i].position.y);
        CHECK_EQ(first.placements[i].position.z, second.placements[i].position.z);
    }
}

TEST(solver_does_not_change_the_input_order_of_results_unpredictably_for_duplicates) {
    // Items with identical sizes should all still be placed exactly once.
    std::vector<Item> items;
    for (int i = 0; i < 12; i++) items.push_back(makeItem("SAME" + std::to_string(i), 50, 50, 50, 1.0));
    solveAndCheck(items, { makeBox("M", 200, 200, 200, 100.0) });
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