#include "PackingSolver.h"

// pretty basic approach for now, just trying to get something working
// sort items biggest first so the big stuff gets priority, then put
// each item in the first box that actually fits it

std::vector<Item> sortBySize(std::vector<Item> items) {
    // simple bubble sort, not the fastest but easy to reason about at 1am
    for (size_t i = 0; i < items.size(); i++) {
        for (size_t j = 0; j < items.size() - i - 1; j++) {
            if (items[j].d.volume() < items[j + 1].d.volume()) {
                Item temp = items[j];
                items[j] = items[j + 1];
                items[j + 1] = temp;
            }
        }
    }
    return items;
}

PackingSolution PackingSolver::solve(const Problem& problem) {
    PackingSolution result;
    std::vector<Item> items = sortBySize(problem.items);

    for (size_t i = 0; i < items.size(); i++) {
        Item item = items[i];
        bool placed = false;

        for (size_t j = 0; j < problem.boxes.size(); j++) {
            BoxType box = problem.boxes[j];

            if (!box.active) {
                continue;
            }

            if (item.d.compare(box.d) && item.weight <= box.maxWeight) {
                result.solution.push_back(Placement(box.reference, item.itemCode, Position(0, 0, 0)));
                placed = true;
                break; // just take the first one that fits, good enough for now
            }
        }

        if (!placed) {
            result.unplacedItems.push_back(item.itemCode);
        }
    }

    return result;
}
