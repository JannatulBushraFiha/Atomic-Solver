#include "test_helpers.h"

#include "Constraints.h"
#include "Orientation.h"

// ---------- Orientation ----------

TEST(orientation_gives_six_rotations_including_the_original) {
    Dimension d = { 1, 2, 3 };
    auto rotations = Orientation::allRotations(d);
    CHECK_EQ(rotations.size(), static_cast<size_t>(6));

    bool foundOriginal = false;
    for (const auto& r : rotations) {
        if (r.width == 1 && r.length == 2 && r.depth == 3) foundOriginal = true;
    }
    CHECK(foundOriginal);
}

TEST(orientation_every_rotation_has_the_same_three_sides) {
    Dimension d = { 4, 5, 6 };
    for (const auto& r : Orientation::allRotations(d)) {
        CHECK_EQ(r.width * r.length * r.depth, 120);
        CHECK_EQ(r.width + r.length + r.depth, 15);
    }
}

TEST(orientation_gives_all_six_different_orders_for_distinct_sides) {
    std::set<std::string> seen;
    for (const auto& r : Orientation::allRotations({ 1, 2, 3 })) {
        seen.insert(std::to_string(r.width) + "," + std::to_string(r.length) + "," + std::to_string(r.depth));
    }
    CHECK_EQ(seen.size(), static_cast<size_t>(6));
}

// ---------- permittedRotations ----------

TEST(rotations_normal_item_can_rotate_all_ways) {
    Constraints c;
    CHECK_EQ(c.permittedRotations(makeItem("A", 1, 2, 3, 1.0)).size(), static_cast<size_t>(6));
}

TEST(rotations_fragile_item_stays_as_it_is) {
    Constraints c;
    auto rotations = c.permittedRotations(makeFragile("F", 1, 2, 3, 1.0));
    REQUIRE(rotations.size() == 1);
    CHECK_EQ(rotations[0].width, 1);
    CHECK_EQ(rotations[0].length, 2);
    CHECK_EQ(rotations[0].depth, 3);
}

TEST(rotations_dangerous_goods_stay_as_they_are) {
    Constraints c;
    CHECK_EQ(c.permittedRotations(makeDangerous("D", 1, 2, 3, 1.0, "3")).size(), static_cast<size_t>(1));
}

TEST(rotations_switched_off_means_one_orientation) {
    Constraints c;
    c.allowRotation = false;
    CHECK_EQ(c.permittedRotations(makeItem("A", 1, 2, 3, 1.0)).size(), static_cast<size_t>(1));
}

// ---------- checkItem ----------

TEST(checkItem_accepts_a_normal_item) {
    Constraints c;
    Violation v;
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 10.0) };
    CHECK(c.checkItem(makeItem("A", 10, 10, 10, 1.0), boxes, v));
}

TEST(checkItem_rejects_zero_or_negative_dimensions) {
    Constraints c;
    Violation v;
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 10.0) };
    CHECK(!c.checkItem(makeItem("A", 0, 10, 10, 1.0), boxes, v));
    CHECK_EQ(v.code, std::string("INVALID_DIMENSIONS"));
    CHECK(!c.checkItem(makeItem("B", 10, -5, 10, 1.0), boxes, v));
    CHECK_EQ(v.code, std::string("INVALID_DIMENSIONS"));
    CHECK(!c.checkItem(makeItem("C", 10, 10, 0, 1.0), boxes, v));
    CHECK_EQ(v.code, std::string("INVALID_DIMENSIONS"));
}

TEST(checkItem_rejects_negative_weight) {
    Constraints c;
    Violation v;
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 10.0) };
    CHECK(!c.checkItem(makeItem("A", 10, 10, 10, -1.0), boxes, v));
    CHECK_EQ(v.code, std::string("INVALID_WEIGHT"));
}

TEST(checkItem_accepts_zero_weight) {
    Constraints c;
    Violation v;
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 10.0) };
    CHECK(c.checkItem(makeItem("A", 10, 10, 10, 0.0), boxes, v));
}

TEST(checkItem_rejects_prohibited_dangerous_goods_class) {
    Constraints c;
    Violation v;
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 10.0) };
    CHECK(!c.checkItem(makeDangerous("D", 10, 10, 10, 1.0, "1"), boxes, v));
    CHECK_EQ(v.code, std::string("PROHIBITED_GOODS"));
    CHECK(c.checkItem(makeDangerous("D2", 10, 10, 10, 1.0, "3"), boxes, v));
}

TEST(checkItem_respects_max_item_weight) {
    Constraints c;
    c.maxItemWeight = 5.0;
    Violation v;
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 50.0) };
    CHECK(!c.checkItem(makeItem("A", 10, 10, 10, 6.0), boxes, v));
    CHECK_EQ(v.code, std::string("ITEM_TOO_HEAVY"));
    CHECK(c.checkItem(makeItem("B", 10, 10, 10, 5.0), boxes, v));
}

TEST(checkItem_respects_max_item_volume) {
    Constraints c;
    c.maxItemVolume = 1000;
    Violation v;
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 50.0) };
    CHECK(!c.checkItem(makeItem("A", 11, 10, 10, 1.0), boxes, v));
    CHECK_EQ(v.code, std::string("ITEM_TOO_LARGE"));
    CHECK(c.checkItem(makeItem("B", 10, 10, 10, 1.0), boxes, v));
}

TEST(checkItem_respects_max_item_dimension_even_when_rotated) {
    Constraints c;
    c.maxItemDimension = { 50, 20, 10 };
    Violation v;
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 50.0) };
    CHECK(c.checkItem(makeItem("A", 10, 20, 50, 1.0), boxes, v));   // fits when rotated
    CHECK(!c.checkItem(makeItem("B", 60, 5, 5, 1.0), boxes, v));
    CHECK_EQ(v.code, std::string("ITEM_TOO_LARGE"));
}

TEST(checkItem_max_item_dimension_without_rotation_is_strict) {
    Constraints c;
    c.maxItemDimension = { 50, 20, 10 };
    c.allowRotation = false;
    Violation v;
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 50.0) };
    CHECK(!c.checkItem(makeItem("A", 10, 20, 50, 1.0), boxes, v));
    CHECK_EQ(v.code, std::string("ITEM_TOO_LARGE"));
}

TEST(checkItem_max_item_dimension_is_strict_for_items_that_cannot_be_turned) {
    Constraints c;
    c.maxItemDimension = { 50, 20, 10 };
    Violation v;
    std::vector<BoxType> boxes = { makeBox("M", 100, 100, 100, 50.0) };
    CHECK(!c.checkItem(makeFragile("F", 10, 20, 50, 1.0), boxes, v));
    CHECK_EQ(v.code, std::string("ITEM_TOO_LARGE"));
    CHECK(!c.checkItem(makeDangerous("D", 10, 20, 50, 1.0, "3"), boxes, v));
    CHECK_EQ(v.code, std::string("ITEM_TOO_LARGE"));
}

TEST(checkItem_fails_when_there_are_no_active_boxes) {
    Constraints c;
    Violation v;
    BoxType inactive = makeBox("M", 100, 100, 100, 10.0);
    inactive.active = false;
    CHECK(!c.checkItem(makeItem("A", 10, 10, 10, 1.0), { inactive }, v));
    CHECK_EQ(v.code, std::string("NO_BOX_AVAILABLE"));
    CHECK(!c.checkItem(makeItem("A", 10, 10, 10, 1.0), {}, v));
    CHECK_EQ(v.code, std::string("NO_BOX_AVAILABLE"));
}

TEST(checkItem_ignores_inactive_boxes_when_looking_for_a_fit) {
    Constraints c;
    Violation v;
    BoxType big = makeBox("BIG", 500, 500, 500, 100.0);
    big.active = false;
    BoxType small = makeBox("SMALL", 20, 20, 20, 100.0);
    CHECK(!c.checkItem(makeItem("A", 100, 100, 100, 1.0), { big, small }, v));
    CHECK_EQ(v.code, std::string("NO_BOX_FITS"));
}

TEST(checkItem_item_too_big_for_every_box) {
    Constraints c;
    Violation v;
    CHECK(!c.checkItem(makeItem("A", 200, 10, 10, 1.0), { makeBox("M", 100, 100, 100, 10.0) }, v));
    CHECK_EQ(v.code, std::string("NO_BOX_FITS"));
}

TEST(checkItem_item_too_heavy_for_every_box) {
    Constraints c;
    Violation v;
    CHECK(!c.checkItem(makeItem("A", 10, 10, 10, 50.0), { makeBox("M", 100, 100, 100, 10.0) }, v));
    CHECK_EQ(v.code, std::string("NO_BOX_SUPPORTS_WEIGHT"));
}

TEST(checkItem_item_that_only_fits_when_turned_is_ok_if_rotation_allowed) {
    Constraints c;
    Violation v;
    std::vector<BoxType> boxes = { makeBox("M", 100, 50, 20, 10.0) };
    CHECK(c.checkItem(makeItem("A", 20, 100, 50, 1.0), boxes, v));
    c.allowRotation = false;
    CHECK(!c.checkItem(makeItem("A", 20, 100, 50, 1.0), boxes, v));
}

TEST(checkItem_box_with_max_weight_zero_means_no_weight_limit) {
    // This is how the code works today: maxWeight 0 = unlimited.
    Constraints c;
    Violation v;
    CHECK(c.checkItem(makeItem("A", 10, 10, 10, 9999.0), { makeBox("M", 100, 100, 100, 0.0) }, v));
}

TEST(checkItem_box_that_fits_is_too_weak_and_box_that_is_strong_enough_is_too_small) {
    Constraints c;
    Violation v;
    std::vector<BoxType> boxes = { makeBox("SMALL", 20, 20, 20, 100.0), makeBox("BIG", 500, 500, 500, 5.0) };
    CHECK(!c.checkItem(makeItem("A", 100, 100, 100, 10.0), boxes, v));
    CHECK_EQ(v.code, std::string("NO_BOX_FITS"));
    CHECK(v.message.find("carries") != std::string::npos);
}

TEST(checkItem_fragile_item_that_only_fits_when_turned_does_not_fit) {
    Constraints c;
    Violation v;
    std::vector<BoxType> boxes = { makeBox("M", 100, 50, 20, 10.0) };
    CHECK(!c.checkItem(makeFragile("F", 20, 100, 50, 1.0), boxes, v));
    CHECK_EQ(v.code, std::string("NO_BOX_FITS"));
    CHECK(c.checkItem(makeFragile("F2", 100, 50, 20, 1.0), boxes, v));   // already the right way up
}

TEST(checkItem_dangerous_goods_that_only_fit_when_turned_do_not_fit) {
    Constraints c;
    Violation v;
    std::vector<BoxType> boxes = { makeBox("M", 100, 50, 20, 10.0) };
    CHECK(!c.checkItem(makeDangerous("D", 20, 100, 50, 1.0, "3"), boxes, v));
    CHECK_EQ(v.code, std::string("NO_BOX_FITS"));
}

TEST(checkItem_box_weight_with_no_weight_limit_is_still_unlimited) {
    Constraints c;
    Violation v;
    BoxType box = makeBox("M", 100, 100, 100, 0.0);
    box.boxWeight = 5.0;
    CHECK(c.checkItem(makeItem("A", 10, 10, 10, 9999.0), { box }, v));
}

TEST(checkItem_weight_limit_allows_for_floating_point_rounding) {
    Constraints c;
    Violation v;
    BoxType box = makeBox("M", 100, 100, 100, 0.3);
    box.boxWeight = 0.1;
    CHECK(c.checkItem(makeItem("A", 10, 10, 10, 0.2), { box }, v));   // 0.1 + 0.2 is 0.30000000000000004
}

// ---------- allowsPlacement ----------

TEST(placement_rejects_going_over_the_box_weight_limit) {
    Constraints c;
    BoxState box;
    box.maxWeight = 10.0;
    box.currentWeight = 8.0;
    box.itemCount = 1;
    Violation v;
    CHECK(!c.allowsPlacement(box, makeItem("A", 1, 1, 1, 3.0), v));
    CHECK_EQ(v.code, std::string("BOX_OVERWEIGHT"));
    CHECK(c.allowsPlacement(box, makeItem("B", 1, 1, 1, 2.0), v));   // exactly at the limit is fine
}

TEST(placement_box_with_no_weight_limit_takes_anything) {
    Constraints c;
    BoxState box;
    box.maxWeight = 0.0;
    box.currentWeight = 1000.0;
    box.itemCount = 5;
    Violation v;
    CHECK(c.allowsPlacement(box, makeItem("A", 1, 1, 1, 500.0), v));
}

TEST(placement_rejects_mixing_box_groups) {
    Constraints c;
    BoxState box;
    box.maxWeight = 100.0;
    box.group = "A";
    box.itemCount = 1;
    Violation v;
    CHECK(!c.allowsPlacement(box, makeGrouped("X", 1, 1, 1, 1.0, "B"), v));
    CHECK_EQ(v.code, std::string("MIXED_BOX_GROUPS"));
    CHECK(c.allowsPlacement(box, makeGrouped("Y", 1, 1, 1, 1.0, "A"), v));
    CHECK(c.allowsPlacement(box, makeItem("Z", 1, 1, 1, 1.0), v));   // no group = can go anywhere
}

TEST(placement_group_rule_can_be_switched_off) {
    Constraints c;
    c.enforceBoxGroups = false;
    BoxState box;
    box.maxWeight = 100.0;
    box.group = "A";
    box.itemCount = 1;
    Violation v;
    CHECK(c.allowsPlacement(box, makeGrouped("X", 1, 1, 1, 1.0, "B"), v));
}

TEST(placement_dangerous_goods_cannot_join_ordinary_items) {
    Constraints c;
    BoxState box;
    box.maxWeight = 100.0;
    box.currentWeight = 2.0;
    box.itemCount = 1;
    Violation v;
    CHECK(!c.allowsPlacement(box, makeDangerous("D", 1, 1, 1, 1.0, "3"), v));
    CHECK_EQ(v.code, std::string("DANGEROUS_GOODS_MUST_BE_SEPARATE"));
}

TEST(placement_ordinary_items_cannot_join_dangerous_goods) {
    Constraints c;
    BoxState box;
    box.maxWeight = 100.0;
    box.currentWeight = 2.0;
    box.itemCount = 1;
    box.hasDangerousGoods = true;
    box.dangerousGoodsClass = "3";
    Violation v;
    CHECK(!c.allowsPlacement(box, makeItem("A", 1, 1, 1, 1.0), v));
    CHECK_EQ(v.code, std::string("DANGEROUS_GOODS_MUST_BE_SEPARATE"));
}

TEST(placement_same_dangerous_goods_class_can_share_but_different_class_cannot) {
    Constraints c;
    BoxState box;
    box.maxWeight = 100.0;
    box.currentWeight = 2.0;
    box.itemCount = 1;
    box.hasDangerousGoods = true;
    box.dangerousGoodsClass = "3";
    Violation v;
    CHECK(c.allowsPlacement(box, makeDangerous("D", 1, 1, 1, 1.0, "3"), v));
    CHECK(!c.allowsPlacement(box, makeDangerous("E", 1, 1, 1, 1.0, "4"), v));
}

TEST(placement_dangerous_goods_can_go_into_an_empty_box) {
    Constraints c;
    BoxState box;
    box.maxWeight = 100.0;
    Violation v;
    CHECK(c.allowsPlacement(box, makeDangerous("D", 1, 1, 1, 1.0, "3"), v));
}

TEST(placement_fragile_items_cannot_mix_with_ordinary_items) {
    Constraints c;
    BoxState ordinaryBox;
    ordinaryBox.maxWeight = 100.0;
    ordinaryBox.currentWeight = 2.0;
    ordinaryBox.itemCount = 1;
    Violation v;
    CHECK(!c.allowsPlacement(ordinaryBox, makeFragile("F", 1, 1, 1, 1.0), v));
    CHECK_EQ(v.code, std::string("FRAGILE_MUST_BE_SEPARATE"));

    BoxState fragileBox = ordinaryBox;
    fragileBox.hasFragile = true;
    CHECK(!c.allowsPlacement(fragileBox, makeItem("A", 1, 1, 1, 1.0), v));
    CHECK(c.allowsPlacement(fragileBox, makeFragile("F2", 1, 1, 1, 1.0), v));   // fragile with fragile is fine
}

// This is the 0kg bug. A box holding only a 0kg item has weight 0 but it is NOT empty.
TEST(placement_zero_weight_item_still_counts_as_something_in_the_box) {
    Constraints c;
    BoxState box;
    box.maxWeight = 100.0;
    box.currentWeight = 0.0;
    box.itemCount = 1;
    Violation v;
    CHECK(!c.allowsPlacement(box, makeDangerous("D", 1, 1, 1, 1.0, "3"), v));
    CHECK(!c.allowsPlacement(box, makeFragile("F", 1, 1, 1, 1.0), v));
}

TEST(placement_zero_weight_dangerous_goods_box_rejects_ordinary_items) {
    Constraints c;
    BoxState box;
    box.maxWeight = 100.0;
    box.itemCount = 1;
    box.hasDangerousGoods = true;
    box.dangerousGoodsClass = "3";
    Violation v;
    CHECK(!c.allowsPlacement(box, makeItem("A", 1, 1, 1, 1.0), v));
}



TEST(placement_box_weight_with_no_weight_limit_is_still_unlimited) {
    Constraints c;
    BoxState box;
    box.maxWeight = 0.0;
    box.boxWeight = 5.0;
    box.currentWeight = 1000.0;
    box.itemCount = 5;
    Violation v;
    CHECK(c.allowsPlacement(box, makeItem("A", 1, 1, 1, 500.0), v));
}