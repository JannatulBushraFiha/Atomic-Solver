#include "PackingSolver.h"

// pretty basic approach for now, just trying to get something working
// sort items biggest first so the big stuff gets priority, then put
// each item in the first box that actually fits it

<<<<<<< Updated upstream
std::vector<Item> sortBySize(std::vector<Item> items) {
    // simple bubble sort, not the fastest but easy to reason about at 1am
    for (size_t i = 0; i < items.size(); i++) {
        for (size_t j = 0; j < items.size() - i - 1; j++) {
            if (items[j].d.volume() < items[j + 1].d.volume()) {
                Item temp = items[j];
                items[j] = items[j + 1];
                items[j + 1] = temp;
            }
=======
namespace {

// A fragile item may only carry items weighing at most this fraction of its own weight.
// (e.g. 0.5 -> a 10 kg fragile item can support up to 5 kg on top.)
const double FRAGILE_MAX_LOAD_RATIO = 0.5;

// Spare room left at the top of every box so the lid can close, as a FRACTION of the box
// depth, so it works in any unit (mm, cm, ...). 0.02 = keep the top 2% free (always at
// least 1 unit). Items are never stacked higher than the box depth minus this gap.
// Set to 0.0 for a perfect fit.
const double BOX_TOP_HEADROOM_RATIO = 0.0;

// Smallest single dimension of any item in the current job. A free space with a side
// shorter than this can never hold anything, so it is dropped instead of being searched
// again and again. Set at the start of solve().
thread_local int g_minItemDim = 0;

struct FreeSpace {
    int x, y, z;
    int width, length, depth;
    double supportWeight = 1e18;   // weight of the item underneath (box floor = unlimited)
    bool supportIsFragile = false; // is the item underneath fragile?
};

struct BoxInstance {
    std::string reference;
    int instanceNumber;
    double maxWeight;
    double currentWeight = 0.0;
    std::string group; // empty = not yet assigned to a group
    bool hasDangerousGoods = false;
    std::string dangerousGoodsClass;
    bool hasFragile = false;
    std::vector<FreeSpace> freeSpaces;
};

// Depth that items may actually use: box depth minus the top headroom (rounded up to a
// whole unit so any ratio above 0 keeps at least 1 unit free).
int usableDepth(int boxDepth) {
    if (BOX_TOP_HEADROOM_RATIO <= 0.0) return boxDepth;
    int headroom = static_cast<int>(std::ceil(boxDepth * BOX_TOP_HEADROOM_RATIO - 1e-9));
    return std::max(0, boxDepth - headroom);
}

long long volume(const Dimension& d) {
    return static_cast<long long>(d.width) * d.length * d.depth;
}

long long spaceVolume(const FreeSpace& s) {
    return static_cast<long long>(s.width) * s.length * s.depth;
}

bool fitsInSpace(const Dimension& dim, const FreeSpace& space) {
    return dim.width <= space.width &&
           dim.length <= space.length &&
           dim.depth <= space.depth;
}

void splitFreeSpace(std::vector<FreeSpace>& spaces, size_t usedIndex, const Dimension& placedDim, double placedWeight, bool placedFragile) {
    FreeSpace used = spaces[usedIndex];
    spaces.erase(spaces.begin() + usedIndex);

    // Only keep leftover spaces that something could still fit into.
    auto addSpace = [&](const FreeSpace& f) {
        if (f.width < g_minItemDim || f.length < g_minItemDim || f.depth < g_minItemDim) return;
        spaces.push_back(f);
    };

    if (used.width - placedDim.width > 0) {
        addSpace({
            used.x + placedDim.width, used.y, used.z,
            used.width - placedDim.width, used.length, used.depth,
            used.supportWeight, used.supportIsFragile
        });
    }
    if (used.length - placedDim.length > 0) {
        addSpace({
            used.x, used.y + placedDim.length, used.z,
            placedDim.width, used.length - placedDim.length, used.depth,
            used.supportWeight, used.supportIsFragile
        });
    }
    if (used.depth - placedDim.depth > 0) {
        addSpace({
            used.x, used.y, used.z + placedDim.depth,
            placedDim.width, placedDim.length, used.depth - placedDim.depth,
            placedWeight, placedFragile
        });
    }
}

BoxState toBoxState(const BoxInstance& box) {
    BoxState state;
    state.maxWeight = box.maxWeight;
    state.currentWeight = box.currentWeight;
    state.group = box.group;
    state.hasDangerousGoods = box.hasDangerousGoods;
    state.dangerousGoodsClass = box.dangerousGoodsClass;
    state.hasFragile = box.hasFragile;
    return state;
}

// The best spot found for an item inside one box.
struct FitCandidate {
    bool found = false;
    size_t spaceIdx = 0;
    Dimension rotation{};
    long long waste = std::numeric_limits<long long>::max();
    int z = 0, y = 0, x = 0;
};

// Is candidate `a` a better spot than `b`?
// 1) FLOOR FIRST: a spot on the box floor (z == 0) beats any stacked spot.
// 2) least wasted volume (best fit).
// 3) lie flat: lower item height (bigger footprint = steadier).
// 4) tidy corner: lowest z, then y, then x.
bool isBetterFit(const FitCandidate& a, const FitCandidate& b) {
    if (!a.found) return false;
    if (!b.found) return true;
    bool aFloor = (a.z == 0), bFloor = (b.z == 0);
    if (aFloor != bFloor) return aFloor;
    if (a.waste != b.waste) return a.waste < b.waste;
    if (a.rotation.depth != b.rotation.depth) return a.rotation.depth < b.rotation.depth;
    if (a.z != b.z) return a.z < b.z;
    if (a.y != b.y) return a.y < b.y;
    return a.x < b.x;
}

// BEST FIT inside one box: look at every free space and every allowed rotation,
// keep the one that leaves the least wasted volume. If two spots waste the same,
// take the lowest corner (z, then y, then x) so the packing stays tidy.
FitCandidate evaluateBestFit(const BoxInstance& box, const Item& item, const Constraints& constraints) {
    FitCandidate best;

    Violation ignored;
    if (!constraints.allowsPlacement(toBoxState(box), item, ignored)) return best;

    auto rotations = constraints.permittedRotations(item);

    for (size_t spaceIdx = 0; spaceIdx < box.freeSpaces.size(); spaceIdx++) {
        const FreeSpace& space = box.freeSpaces[spaceIdx];

        // STACKING RULE 1: never put a heavier item on top of a lighter one.
        if (item.weight > space.supportWeight) continue;
        // STACKING RULE 2: on top of a fragile item, only a much lighter load is allowed.
        if (space.supportIsFragile && item.weight > FRAGILE_MAX_LOAD_RATIO * space.supportWeight) continue;

        for (const Dimension& rot : rotations) {
            if (!fitsInSpace(rot, space)) continue;

            FitCandidate cand;
            cand.found = true;
            cand.waste = spaceVolume(space) - volume(rot);
            cand.spaceIdx = spaceIdx;
            cand.rotation = rot;
            cand.z = space.z;
            cand.y = space.y;
            cand.x = space.x;
            if (isBetterFit(cand, best)) best = cand;
>>>>>>> Stashed changes
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

<<<<<<< Updated upstream
            if (!box.active) {
=======
// A finished packing run plus the live state of its boxes (free spaces, weights).
struct PlanState {
    PackingSolution solution;
    std::vector<BoxInstance> boxes;
};

} // namespace

PackingSolution PackingSolver::solve(
    const std::vector<Item>& allItems,
    const std::vector<BoxType>& boxes
) {
    // Items that ship in their own packaging skip packing entirely: they only have to pass
    // the per-item limits, and are then reported as their own parcels.
    PackingSolution shipping;
    std::vector<Item> items;
    items.reserve(allItems.size());
    for (const auto& item : allItems) {
        if (!item.shipInOwnPackaging) { items.push_back(item); continue; }
        Violation limitViolation;
        if (!constraints.checkItemLimits(item, limitViolation)) {
            shipping.unplacedItems.push_back(item.itemCode);
            shipping.violations.push_back(limitViolation);
            continue;
        }
        shipping.ownPackagedItems.push_back({ item.itemCode, item.itemDimension, item.weight });
    }

    // Smallest side of any item we will pack (see g_minItemDim).
    g_minItemDim = 0;
    for (const auto& it : items) {
        int m = std::min({it.itemDimension.width, it.itemDimension.length, it.itemDimension.depth});
        if (g_minItemDim == 0 || m < g_minItemDim) g_minItemDim = m;
    }

    std::vector<BoxType> activeBoxes;
    for (const auto& b : boxes) {
        if (b.active) activeBoxes.push_back(b);
    }
    std::sort(activeBoxes.begin(), activeBoxes.end(), [](const BoxType& a, const BoxType& b) {
        return volume(a.boxDimension) < volume(b.boxDimension);
    });

    // Largest footprint first, then largest volume.
    std::vector<Item> sortedItems = items;
    std::sort(sortedItems.begin(), sortedItems.end(), [](const Item& a, const Item& b) {
        long long areaA = static_cast<long long>(a.itemDimension.width) * a.itemDimension.length;
        long long areaB = static_cast<long long>(b.itemDimension.width) * b.itemDimension.length;
        if (areaA != areaB) return areaA > areaB;
        return volume(a.itemDimension) > volume(b.itemDimension);
    });

    // One full packing run. `preferredRef` is the box type we try to open first
    // whenever a new box is needed (empty = just take the smallest that fits).
    auto runPlan = [&](const std::string& preferredRef, const std::vector<Item>& order) {
        PackingSolution solution;

        // Order in which new box types are tried: preferred first, then smallest to largest.
        std::vector<BoxType> openOrder;
        for (const auto& b : activeBoxes) if (b.reference == preferredRef) openOrder.push_back(b);
        for (const auto& b : activeBoxes) if (b.reference != preferredRef) openOrder.push_back(b);

        std::vector<BoxInstance> openBoxes;
        std::map<std::string, int> boxTypeCount;

        for (const auto& item : order) {
            // Pre-screen: is this item even placeable given the constraints and available boxes?
            Violation preCheckViolation;
            if (!constraints.checkItem(item, boxes, preCheckViolation)) {
                solution.unplacedItems.push_back(item.itemCode);
                solution.violations.push_back(preCheckViolation);
>>>>>>> Stashed changes
                continue;
            }

            if (item.d.compare(box.d) && item.weight <= box.maxWeight) {
                result.solution.push_back(Placement(box.reference, item.itemCode, Position(0, 0, 0)));
                placed = true;
                break; // just take the first one that fits, good enough for now
            }
        }

<<<<<<< Updated upstream
        if (!placed) {
            result.unplacedItems.push_back(item.itemCode);
        }
    }

    return result;
}
=======
        for (const auto& box : openBoxes) {
            solution.usedBoxes.push_back({ box.reference, box.instanceNumber, box.currentWeight });
        }
        return PlanState{ solution, openBoxes };
    };

    // Total volume of the boxes a plan uses (tie-break: smaller total = less cardboard).
    auto planVolume = [&](const PackingSolution& s) {
        long long total = 0;
        for (const auto& ub : s.usedBoxes)
            for (const auto& bt : activeBoxes)
                if (bt.reference == ub.boxReference) total += volume(bt.boxDimension);
        return total;
    };

    std::map<std::string, const Item*> itemByCode;
    for (const auto& it : sortedItems) itemByCode[it.itemCode] = &it;

    // CONSOLIDATION PASS: the one-item-at-a-time packing can leave many half-empty boxes.
    // Take the emptiest box and try to move ALL of its items into the spare room of the
    // other boxes (every rule is still checked: weight, stacking, groups, fragile, DG).
    // If every item finds a home, that box disappears. Repeat until nothing more can go.
    auto consolidate = [&](PlanState st) -> PlanState {
        std::vector<BoxInstance> boxes = st.boxes;
        std::vector<std::vector<Placement>> plc(boxes.size());
        std::map<std::pair<std::string, int>, size_t> idxOf;
        for (size_t i = 0; i < boxes.size(); i++) idxOf[{boxes[i].reference, boxes[i].instanceNumber}] = i;
        for (const auto& p : st.solution.placements) plc[idxOf[{p.boxReference, p.boxInstance}]].push_back(p);

        bool improved = true;
        while (improved && boxes.size() > 1) {
            improved = false;

            std::vector<long long> usedVol(boxes.size(), 0);
            for (size_t i = 0; i < boxes.size(); i++)
                for (const auto& p : plc[i]) usedVol[i] += volume(itemByCode[p.itemCode]->itemDimension);
            std::vector<size_t> order(boxes.size());
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return usedVol[a] < usedVol[b]; });

            for (size_t victim : order) {
                std::vector<const Item*> moving;
                for (const auto& p : plc[victim]) moving.push_back(itemByCode[p.itemCode]);

                // Two move orders: big footprint first, then heavy first.
                for (int pass = 0; pass < 2 && !improved; pass++) {
                    std::sort(moving.begin(), moving.end(), [pass](const Item* a, const Item* b) {
                        if (pass == 1 && a->weight != b->weight) return a->weight > b->weight;
                        long long areaA = static_cast<long long>(a->itemDimension.width) * a->itemDimension.length;
                        long long areaB = static_cast<long long>(b->itemDimension.width) * b->itemDimension.length;
                        if (areaA != areaB) return areaA > areaB;
                        return volume(a->itemDimension) > volume(b->itemDimension);
                    });

                    std::vector<BoxInstance> trial = boxes;
                    std::vector<std::vector<Placement>> trialPlc = plc;
                    bool allMoved = true;
                    for (const Item* it : moving) {
                        int bestJ = -1;
                        FitCandidate bestFit;
                        for (size_t j = 0; j < trial.size(); j++) {
                            if (j == victim) continue;
                            FitCandidate c = evaluateBestFit(trial[j], *it, constraints);
                            if (c.found && (!bestFit.found || c.waste < bestFit.waste)) {
                                bestFit = c;
                                bestJ = static_cast<int>(j);
                            }
                        }
                        if (bestJ < 0) { allMoved = false; break; }
                        Placement p;
                        commitFit(trial[bestJ], *it, bestFit, p);
                        trialPlc[bestJ].push_back(p);
                    }
                    if (allMoved) {
                        trial.erase(trial.begin() + victim);
                        trialPlc.erase(trialPlc.begin() + victim);
                        boxes = std::move(trial);
                        plc = std::move(trialPlc);
                        improved = true;
                    }
                }
                if (improved) break;
            }
        }

        // Renumber boxes 1, 2, 3... within each box type and rebuild the output lists.
        PackingSolution out;
        out.unplacedItems = st.solution.unplacedItems;
        out.violations = st.solution.violations;
        std::map<std::string, int> nextInstance;
        for (size_t i = 0; i < boxes.size(); i++) {
            int number = ++nextInstance[boxes[i].reference];
            for (auto p : plc[i]) {
                p.boxInstance = number;
                out.placements.push_back(p);
            }
            out.usedBoxes.push_back({ boxes[i].reference, number, boxes[i].currentWeight });
            boxes[i].instanceNumber = number;
        }
        return PlanState{ out, boxes };
    };

    // IMPROVEMENT SEARCH (ruin and recreate): repeatedly empty 1-3 weak boxes, then re-place
    // their items into the other boxes (opening a new box only when nothing else works).
    // Keep the change only if it uses fewer boxes, or the same number packed more tightly.
    // Fixed seed, fixed iteration count, so the result is the same every run.
    auto improve = [&](PlanState st) -> PlanState {
        std::vector<BoxInstance> boxes = st.boxes;
        std::vector<std::vector<Placement>> plc(boxes.size());
        std::map<std::pair<std::string, int>, size_t> idxOf;
        for (size_t i = 0; i < boxes.size(); i++) idxOf[{boxes[i].reference, boxes[i].instanceNumber}] = i;
        for (const auto& p : st.solution.placements) plc[idxOf[{p.boxReference, p.boxInstance}]].push_back(p);

        unsigned long long rng = 88172645463325252ULL;
        auto next = [&]() { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; };

        auto typeOf = [&](const std::string& ref) -> const BoxType* {
            for (const auto& bt : activeBoxes) if (bt.reference == ref) return &bt;
            return nullptr;
        };
        auto fillOf = [&](const BoxInstance& b, const std::vector<Placement>& ps) {
            const BoxType* bt = typeOf(b.reference);
            double wf = (bt && bt->maxWeight > 0) ? b.currentWeight / bt->maxWeight : 0.0;
            long long v = 0;
            for (const auto& p : ps) v += volume(itemByCode[p.itemCode]->itemDimension);
            double vf = bt ? static_cast<double>(v) / static_cast<double>(volume(bt->boxDimension)) : 0.0;
            return std::max(wf, vf);
        };
        auto score = [&](const std::vector<BoxInstance>& bx, const std::vector<std::vector<Placement>>& pl) {
            double total = 0.0;
            for (size_t i = 0; i < bx.size(); i++) { double f = fillOf(bx[i], pl[i]); total += f * f; }
            return total;
        };

        int nextId = 5000000;
        // Work budget: more boxes means each try costs more, so large jobs get fewer tries.
        const int iterations = std::max<int>(100, std::min<int>(1500, 120000 / static_cast<int>(boxes.size() + 1)));
        double curScore = score(boxes, plc);

        // Fewest boxes that could ever hold these items (by weight and by volume). Once we
        // are there, fewer boxes is impossible and the search can stop early.
        size_t lowerBound = 1;
        {
            double totalWeight = 0.0;
            long long totalVolume = 0;
            for (const auto& list : plc) {
                for (const auto& p : list) {
                    const Item* it = itemByCode[p.itemCode];
                    totalWeight += it->weight;
                    totalVolume += volume(it->itemDimension);
                }
            }
            double bigWeight = 0.0;
            long long bigVolume = 0;
            bool weightLimited = !activeBoxes.empty();
            for (const auto& bt : activeBoxes) {
                if (bt.maxWeight <= 0) weightLimited = false;
                bigWeight = std::max(bigWeight, bt.maxWeight);
                bigVolume = std::max(bigVolume, volume(bt.boxDimension));
            }
            if (weightLimited && bigWeight > 0) {
                lowerBound = std::max<size_t>(lowerBound, static_cast<size_t>(std::ceil(totalWeight / bigWeight - 1e-9)));
            }
            if (bigVolume > 0) {
                lowerBound = std::max<size_t>(lowerBound, static_cast<size_t>((totalVolume + bigVolume - 1) / bigVolume));
            }
        }

        for (int iter = 0; iter < iterations && boxes.size() > 1; iter++) {
            if (boxes.size() <= lowerBound) break;
            // pick 1-3 victims, biased towards weak boxes
            int k = 1 + static_cast<int>(next() % 3);
            if (k > static_cast<int>(boxes.size())) k = static_cast<int>(boxes.size());
            std::vector<size_t> victims;
            while (static_cast<int>(victims.size()) < k) {
                size_t a = next() % boxes.size(), b = next() % boxes.size();
                size_t pick = fillOf(boxes[a], plc[a]) <= fillOf(boxes[b], plc[b]) ? a : b;
                if (std::find(victims.begin(), victims.end(), pick) == victims.end()) victims.push_back(pick);
            }
            std::sort(victims.begin(), victims.end(), std::greater<size_t>());

            std::vector<BoxInstance> trial = boxes;
            std::vector<std::vector<Placement>> trialPlc = plc;
            std::vector<const Item*> moving;
            for (size_t v : victims) {
                for (const auto& p : trialPlc[v]) moving.push_back(itemByCode[p.itemCode]);
                trial.erase(trial.begin() + v);
                trialPlc.erase(trialPlc.begin() + v);
            }

            int mode = static_cast<int>(next() % 4);
            if (mode == 3) {
                for (size_t i = moving.size(); i > 1; i--) std::swap(moving[i - 1], moving[next() % i]);
            } else {
                std::sort(moving.begin(), moving.end(), [mode](const Item* a, const Item* b) {
                    if (mode == 1 && a->weight != b->weight) return a->weight > b->weight;
                    if (mode == 2) {
                        long long va = volume(a->itemDimension), vb = volume(b->itemDimension);
                        if (va != vb) return va > vb;
                    }
                    long long areaA = static_cast<long long>(a->itemDimension.width) * a->itemDimension.length;
                    long long areaB = static_cast<long long>(b->itemDimension.width) * b->itemDimension.length;
                    if (areaA != areaB) return areaA > areaB;
                    return volume(a->itemDimension) > volume(b->itemDimension);
                });
            }

            bool ok = true;
            for (size_t m = 0; m < moving.size() && ok; m++) {
                const Item* it = moving[m];
                int bestJ = -1;
                FitCandidate bestFit;
                for (size_t j = 0; j < trial.size(); j++) {
                    FitCandidate c = evaluateBestFit(trial[j], *it, constraints);
                    if (c.found && (!bestFit.found || c.waste < bestFit.waste)) { bestFit = c; bestJ = static_cast<int>(j); }
                }
                if (bestJ >= 0) {
                    Placement p;
                    commitFit(trial[bestJ], *it, bestFit, p);
                    trialPlc[bestJ].push_back(p);
                    continue;
                }

                // Open a new box: choose the type that should hold the most of what is left.
                double sumW = 0.0, sumV = 0.0;
                for (size_t r = m; r < moving.size(); r++) { sumW += moving[r]->weight; sumV += static_cast<double>(volume(moving[r]->itemDimension)); }
                double cnt = static_cast<double>(moving.size() - m);
                double avgW = std::max(1e-9, sumW / cnt), avgV = std::max(1.0, sumV / cnt);

                std::map<std::string, int> typeCount;
                for (const auto& b : trial) typeCount[b.reference]++;
                std::vector<std::pair<double, const BoxType*>> options;
                for (const auto& bt : activeBoxes) {
                    if (bt.maximumBoxes != -1 && typeCount[bt.reference] >= bt.maximumBoxes) continue;
                    if (bt.maxWeight > 0 && it->weight > bt.maxWeight) continue;
                    double capW = bt.maxWeight > 0 ? bt.maxWeight / avgW : 1e18;
                    double capV = 0.7 * static_cast<double>(volume(bt.boxDimension)) / avgV;
                    options.push_back({ std::min(capW, capV), &bt });
                }
                std::sort(options.begin(), options.end(), [](const auto& a, const auto& b) {
                    if (a.first != b.first) return a.first > b.first;
                    return volume(a.second->boxDimension) < volume(b.second->boxDimension);
                });
                bool opened = false;
                for (const auto& opt : options) {
                    const BoxType& bt = *opt.second;
                    BoxInstance nb;
                    nb.reference = bt.reference;
                    nb.instanceNumber = nextId++;
                    nb.maxWeight = bt.maxWeight;
                    nb.freeSpaces.push_back({0, 0, 0, bt.boxDimension.width, bt.boxDimension.length,
                                              usableDepth(bt.boxDimension.depth)});
                    FitCandidate f = evaluateBestFit(nb, *it, constraints);
                    if (!f.found) continue;
                    Placement p;
                    commitFit(nb, *it, f, p);
                    trial.push_back(nb);
                    trialPlc.push_back({ p });
                    opened = true;
                    break;
                }
                if (!opened) ok = false;
            }
            if (!ok) continue;

            double trialScore = score(trial, trialPlc);
            bool accept = trial.size() < boxes.size() ||
                          (trial.size() == boxes.size() && trialScore > curScore + 1e-9);
            if (accept) {
                boxes = std::move(trial);
                plc = std::move(trialPlc);
                curScore = trialScore;
            }
        }

        PackingSolution out;
        out.unplacedItems = st.solution.unplacedItems;
        out.violations = st.solution.violations;
        std::map<std::string, int> nextInstance;
        for (size_t i = 0; i < boxes.size(); i++) {
            int number = ++nextInstance[boxes[i].reference];
            for (auto p : plc[i]) {
                p.boxInstance = number;
                out.placements.push_back(p);
            }
            out.usedBoxes.push_back({ boxes[i].reference, number, boxes[i].currentWeight });
            boxes[i].instanceNumber = number;
        }
        return PlanState{ out, boxes };
    };

    auto shrinkBoxes = [&](PackingSolution plan) {
        // SHRINK PASS (per plan): a box may be mostly empty (e.g. the last one). For every used box,
        // try to repack just its items into a smaller box type. If they all fit (all rules
        // still checked), swap to the smaller box. This is how we end up mixing box types,
        // e.g. 3 big boxes + 1 smaller one, instead of 4 big ones.

        std::map<std::pair<std::string, int>, std::vector<Placement>> groups;
        std::vector<std::pair<std::string, int>> groupOrder;
        for (const auto& p : plan.placements) {
            auto key = std::make_pair(p.boxReference, p.boxInstance);
            if (!groups.count(key)) groupOrder.push_back(key);
            groups[key].push_back(p);
        }

        std::map<std::string, int> typeCount; // how many boxes of each type the plan uses
        for (const auto& key : groupOrder) typeCount[key.first]++;

        bool changed = false;
        std::vector<std::pair<std::string, int>> finalOrder;
        std::map<std::pair<std::string, int>, std::vector<Placement>> finalGroups;
        for (const auto& key : groupOrder) {
            std::vector<Placement>& oldPlacements = groups[key];
            const BoxType* currentType = nullptr;
            for (const auto& bt : activeBoxes) if (bt.reference == key.first) currentType = &bt;

            std::vector<const Item*> boxItems;
            for (const auto& p : oldPlacements) boxItems.push_back(itemByCode[p.itemCode]);
            std::sort(boxItems.begin(), boxItems.end(), [](const Item* a, const Item* b) {
                long long areaA = static_cast<long long>(a->itemDimension.width) * a->itemDimension.length;
                long long areaB = static_cast<long long>(b->itemDimension.width) * b->itemDimension.length;
                if (areaA != areaB) return areaA > areaB;
                return volume(a->itemDimension) > volume(b->itemDimension);
            });

            bool swapped = false;
            for (const auto& target : activeBoxes) { // smallest volume first
                if (!currentType || volume(target.boxDimension) >= volume(currentType->boxDimension)) break;
                if (target.maximumBoxes != -1 && typeCount[target.reference] >= target.maximumBoxes) continue;

                BoxInstance trial;
                trial.reference = target.reference;
                trial.instanceNumber = 0;
                trial.maxWeight = target.maxWeight;
                trial.freeSpaces.push_back({0, 0, 0, target.boxDimension.width, target.boxDimension.length,
                                             usableDepth(target.boxDimension.depth)});
                std::vector<Placement> newPlacements;
                bool allFit = true;
                for (const Item* it : boxItems) {
                    FitCandidate fit = evaluateBestFit(trial, *it, constraints);
                    if (!fit.found) { allFit = false; break; }
                    Placement p;
                    commitFit(trial, *it, fit, p);
                    newPlacements.push_back(p);
                }
                if (allFit) {
                    typeCount[key.first]--;
                    typeCount[target.reference]++;
                    auto newKey = std::make_pair(target.reference, 1000000 + static_cast<int>(finalOrder.size()));
                    for (auto& p : newPlacements) p.boxInstance = newKey.second;
                    finalOrder.push_back(newKey);
                    finalGroups[newKey] = newPlacements;
                    swapped = true;
                    changed = true;
                    break;
                }
            }
            if (!swapped) {
                finalOrder.push_back(key);
                finalGroups[key] = oldPlacements;
            }
        }

        if (changed) {
            // Renumber boxes 1, 2, 3... within each box type and rebuild the output lists.
            std::map<std::string, int> nextInstance;
            PackingSolution out;
            out.unplacedItems = plan.unplacedItems;
            out.violations = plan.violations;
            for (const auto& key : finalOrder) {
                int number = ++nextInstance[key.first];
                double weight = 0.0;
                for (auto p : finalGroups[key]) {
                    weight += itemByCode[p.itemCode]->weight;
                    p.boxInstance = number;
                    out.placements.push_back(p);
                }
                out.usedBoxes.push_back({ key.first, number, weight });
            }
            plan = std::move(out);
        }
        return plan;
    };

    // Different packing orders. Footprint-first suits flat stuff; heavy-first lets heavy
    // items sit on the floor (they may not go on top of lighter ones); volume-first is a
    // third opinion.
    std::vector<std::vector<Item>> orders;
    orders.push_back(sortedItems);
    {
        std::vector<Item> o = items;
        std::sort(o.begin(), o.end(), [](const Item& a, const Item& b) {
            if (a.weight != b.weight) return a.weight > b.weight;
            long long areaA = static_cast<long long>(a.itemDimension.width) * a.itemDimension.length;
            long long areaB = static_cast<long long>(b.itemDimension.width) * b.itemDimension.length;
            if (areaA != areaB) return areaA > areaB;
            return volume(a.itemDimension) > volume(b.itemDimension);
        });
        orders.push_back(o);
    }
    {
        std::vector<Item> o = items;
        std::sort(o.begin(), o.end(), [](const Item& a, const Item& b) {
            return volume(a.itemDimension) > volume(b.itemDimension);
        });
        orders.push_back(o);
    }

    // Try every order with "smallest box that fits", then with each box type tried first.
    // Each plan gets the shrink pass (above) before it is scored. Keep the plan that
    // places the most items, then uses the fewest boxes, then the least total box volume.
    PackingSolution best;
    PlanState bestState;
    long long bestVol = 0;
    bool haveBest = false;
    auto isBetter = [&](const PackingSolution& cand, long long candVol) {
        if (!haveBest) return true;
        if (cand.unplacedItems.size() != best.unplacedItems.size())
            return cand.unplacedItems.size() < best.unplacedItems.size();
        if (cand.usedBoxes.size() != best.usedBoxes.size())
            return cand.usedBoxes.size() < best.usedBoxes.size();
        return candVol < bestVol;
    };
    for (const auto& order : orders) {
        std::vector<std::string> prefs = {""};
        for (const auto& bt : activeBoxes) prefs.push_back(bt.reference);
        for (const auto& pref : prefs) {
            PlanState state = consolidate(runPlan(pref, order));
            PackingSolution cand = shrinkBoxes(state.solution);
            long long candVol = planVolume(cand);
            if (isBetter(cand, candVol)) { best = std::move(cand); bestState = std::move(state); bestVol = candVol; haveBest = true; }
        }
    }

    // Polish the winner with the improvement search, then shrink boxes again.
    if (haveBest && bestState.boxes.size() > 1) {
        PlanState polished = improve(bestState);
        PackingSolution cand = shrinkBoxes(polished.solution);
        long long candVol = planVolume(cand);
        if (cand.unplacedItems.size() == best.unplacedItems.size() && isBetter(cand, candVol)) {
            best = std::move(cand);
        }
    }

    best.ownPackagedItems = std::move(shipping.ownPackagedItems);
    best.unplacedItems.insert(best.unplacedItems.end(), shipping.unplacedItems.begin(), shipping.unplacedItems.end());
    best.violations.insert(best.violations.end(), shipping.violations.begin(), shipping.violations.end());
    return best;
}
>>>>>>> Stashed changes
