# Atomic-Solver
## Introduction

**Atomic-Solver** is a 3D bin-packing engine written in C++17 that works out how to pack a set of items into shipping boxes (cartons). You give it the available box types and the items to ship. It returns a packing plan that says which box each item goes in, its exact (x, y, z) position, and the orientation it is placed in. The goal is to use as few boxes, and as little total box volume, as possible.

The solver goes beyond simply fitting items by size. It also enforces real-world shipping rules:

- **Weight limits:** each box's maximum load, including the weight of the box itself
- **Box availability:** inactive box types and a maximum number of boxes per type
- **Grouping:** items that share a `BoxGroup` are packed together
- **Fragile items and dangerous goods:** these are packed alone, and limits apply to what can be stacked on fragile items
- **Lid clearance:** a small headroom is left at the top of each box so the lid can close

To find a good plan, the solver tries several packing orders (largest footprint first, heaviest first, largest volume first). It places each item using a best-fit heuristic that fills the floor first. It then improves the result with a consolidation pass and a deterministic "ruin and recreate" search, so the same input always produces the same output. Before a plan is returned, an independent **Validator** checks that it is physically valid: no overlaps, no item outside its box, no broken constraints, and every item accounted for.

The solver reads JSON on standard input and writes JSON on standard output. This makes it easy to use from the command line or from other programs. The repo also includes a small **Flask API** (`POST /api/solve`) and a **Dockerfile**, which deploys the solver as a web service on Render. Link to live deployment: https://atomic-solver.onrender.com.
GET / returns {"status":"alive"}; 
POST /api/solve takes the input JSON.

## **In order to build the program:**
cmake -S . -B build && cmake --build build

## **To run the tests**
ctest --test-dir build --output-on-failure

## **To run the program**
echo '{
  "boxTypes": [
    {"Reference": "SML", "Width": 150, "Length": 150, "Depth": 150, "MaxWeight": 8.5, "BoxWeight": 0.5, "Active": true, "MaximumBoxes": 100},
    {"Reference": "MED", "Width": 400, "Length": 400, "Depth": 400, "MaxWeight": 15.2, "BoxWeight": 0.75, "Active": true}
  ],
  "items": [
    {"ItemCode": "ITM-001", "ItemReference": "Widget A", "Width": 100, "Length": 200, "Depth": 50, "Weight": 1, "BoxGroup": "GROUP-A"},
    {"ItemCode": "ITM-002", "ItemReference": "Widget B", "Width": 300, "Length": 150, "Depth": 75, "Weight": 2.8}
  ]
}' | ./build/solver

## Client Requirements

### Functional

1. Accept a full set of items and a full set of available box types as input (not streamed one at a time)
2. Determine the optimal selection and arrangement of boxes for the given items
3. Support items and boxes as rectangular objects only, container size can range from small box to pallet
4. Apply weight constraints, each box type has a maximum weight it can carry
5. Support a configurable packing constraint language, so admins can define rules without changing the engine's code
6. Support grouping constraints for dangerous goods, so incompatible classes cannot be packed together
7. Support generic grouping constraints, so specific items can be forced into the same box on request
8. Validate the final packing arrangement against all configured constraints before returning it
9. Report any constraint violations found in an arrangement
10. Report items that could not be packed at all, instead of silently dropping them
11. Reject the order if the engine cannot produce a valid packing solution

### Non-functional (Performance)

12. Return a packing result within 1 to 2 seconds maximum
13. Support up to 1000 orders per second at peak load
14. Prioritise packing efficiency first (time taken, space utilisation), extensibility second
15. Code should be modular enough to extend later without a rewrite or loss of efficiency

### Interfacing

16. Expose the engine through the Portal, taking a Problem object (items and boxes) as input and returning a PackingSolution
17. Provide handheld scanner support as an input source, this is on the visualisation side rather than the solver itself

## User Stories

### As an admin, so the engine follows the client's rules

1. I want to define the maximum weight limit for each package type, so the engine never packs a box over its limit
2. I want to define packing rules for items, so items are packed according to safety and handling requirements
3. I want dangerous goods kept in their own boxes, so they are never packed with ordinary items or with a different dangerous goods class
4. I want to define packing constraints for fragile items, so fragile items are protected from inappropriate placement
5. I want the packing rules and limits held in one Constraints object, so they can be changed in one place without touching the packing algorithm
6. I want to mark a box type as active or inactive, so retired box types are never used in a new packing run
7. I want a maximum available box count per box type, so packing does not assume unlimited stock

### As a user, so I can trust and use the packing output

8. I want the system to generate an efficient packing arrangement, so packing space is used effectively
9. I want the packing engine to apply all relevant constraints from the constraint module, so the arrangement always complies with configured rules
10. I want the engine to validate the final packing arrangement against all constraints, so I can be sure no rules were broken
11. I want the engine to identify any constraint violations in an arrangement, so I know exactly what was breached and why
12. I want unplaced items reported clearly, so I know what could not be packed and can deal with it manually
13. I want a result within 1 to 2 seconds, so my workflow is not delayed

### As a system, so failure is handled gracefully

14. If the engine cannot produce a valid arrangement, the order should be rejected rather than returning an incomplete or invalid result

### As a user, so large orders are easy to submit

15. I want to send several identical items as one line with a quantity, so I do not have to repeat the same item many times

### As a developer, so changes do not silently break the rules

16. I want automated tests for the constraints, validator and solver, so a change that breaks a rule (like items weighing 0 kg slipping past the dangerous goods, fragile and group rules) is caught before it is merged

## Project Descriptiom

### 1. Algorithm (What it solves)

Given a list of items (with size, weight and flags) and a list of box types (with size, weight limit and stock), the solver decides:

1. which boxes to open,
2. which box each item goes into, and
3. exactly where (x, y, z) and in which orientation it sits.

The goal is to use as few boxes as possible, and when two plans use the same number of boxes, the one with the least total box volume, while obeying every rule in the next section. Finding the perfect packing is an NP hard problem, so the solver is a heuristic: it builds several plans quickly, tries out each, keeps the best, and then tries to improve it.

### Step by step

| Step | What happens |
|---|---|
| **1. Pre-check** | Every item is checked on its own: positive size, non-negative weight, not a prohibited class, within any item limits, and able to fit (and be carried by) at least one active box type. Items that fail are put straight into `unplacedItems` with a reason. |
| **2. Best-fit placement** | Items are placed one at a time. Each open box keeps a list of **free spaces**. For an item, the solver tries every free space and every allowed rotation, and picks the placement that **wastes the least volume**. A position on the floor of the box beats a stacked one. |
| **3. Free-space splitting** | After an item is placed, the space it used is split into three smaller free spaces: **right**, **front** and **top**. These become the candidates for the next item. |
| **4. Several plans** | The whole packing is repeated with three item orders (largest footprint first, heaviest first, largest volume first). Each order is tried with "smallest box that fits" and with each box type preferred in turn. |
| **5. Consolidate** | The emptiest box is emptied into the spare room of the other boxes. If every item finds a new home, that box disappears. |
| **6. Shrink** | Each box's items are repacked into a smaller box type if everything still fits, so a plan can mix types (for example 3 large boxes and 1 small one). |
| **7. Pick the winner** | The plan with the most items placed wins; ties go to the fewest boxes, then the least total box volume. |
| **8. Improve** | A **ruin-and-recreate** search empties one to three weak boxes and re-places their items. A change is kept only if it uses fewer boxes or packs the same number of boxes more tightly. The search uses a **fixed random seed**, so the same input always gives the same output. It stops early once it reaches the theoretical lower bound (by weight or volume). The number of rounds shrinks for very large jobs (between 100 and 1500). |
| **9. Validate** | The final arrangement goes through the validator (see section 2). |

### Worked example: free-space splitting

A box is 300 × 300 × 300. The first item is 100 × 100 × 100 and goes in the corner (0, 0, 0). The rest of the box is then described by three free spaces:

- **right** of the item,
- **in front** of the item,
- **on top** of the item (from z = 100 upwards).

The next item is tested in all three, and goes wherever it leaves the least wasted volume. Floor positions are preferred, which keeps the packing stable and leaves the upper space free.

### Why it is fast

- Free-space splitting keeps the search small: an item only has to be tested against a handful of rectangles, not every point in the box.
- The improvement search stops as soon as it reaches the lower bound, and its length scales down as jobs get bigger.
- The 300-unit chaotic test set (54 boxes, nothing unplaced) solves in about 2 seconds on a development laptop.

## 2. Constraints and validators

Rules are enforced **twice**:

1. **Constraints** (`Constraints.cpp`) decide, while packing, whether an item is *allowed* in a particular box and position. A placement that breaks a rule is never made.
2. **Validator** (`Validator.cpp`) checks the *finished* packing from scratch, independently of the packing code, and reports any breach. This protects against bugs in the packing logic.

### Rules the solver enforces

| Rule | What it means | Example |
|---|---|---|
| **Weight limit** | The items in a box never weigh more than its `MaxWeight`. The empty box's own `BoxWeight` is **not** counted towards `MaxWeight`. A box with no `MaxWeight` field mentioned has no limit. | Box `MaxWeight` 5; three items of 2 kg each → 2 boxes (4 kg + 2 kg). |
| **Heavier never on lighter** | An item is never placed on top of a lighter one. | A 5 kg and a 1 kg item in one tall box → the 5 kg item sits at z = 0 and the 1 kg item on top of it. |
| **Fragile stacking rule** | The weight resting on a fragile item is at most 50% of that item's weight. | A fragile 1 kg item can carry at most 0.5 kg on top. |
| **Fragile items** | Share a box only with other fragile items, and are never rotated. | One fragile item and one ordinary item → 2 boxes. |
| **Dangerous goods** | Only items of the same class share a box, dangerous goods are never mixed with ordinary items, and they are never rotated. | Class 8 and class 2 items → 2 boxes. |
| **Prohibited goods** | Items in a prohibited class are rejected (default: class `"1"`). | A class 1 item is returned as unplaced with `PROHIBITED_GOODS`. |
| **Box groups** | Items with different `BoxGroup` values never share a box. | A `FOOD` item and a `CORROSIVE` item → 2 boxes. |
| **Box types** | Inactive types are never used, and `MaximumBoxes` caps the stock of each type. | `Active: false` means that type is skipped. |
| **Item limits** | Optional maximum weight, volume and dimension per item. | An item over the limit is rejected before packing. |
| **Rotation** | Ordinary items may rotate; fragile and dangerous goods may not. | A fragile 20 × 100 × 50 item does not fit a 100 × 50 × 20 box, even though a rotated one would. |

### Input checks

Before packing, `main.cpp` rejects malformed input with a clear error. Example:

```json
{"error": "Bad input: Quantity must be a whole number of at least 1 for item A"}
{"error": "Bad input: boxTypes and items must be lists"}
```

Repeated item codes are handled safely: `Quantity` is expanded into units with unique internal codes (`BOOK-1`, `BOOK-2`, ...), which can never clash with a real item called `BOOK-1`, and the output reports them under the original code.

### Violations: why an item was not packed

An item that cannot be packed is never dropped silently. It goes in `unplacedItems`, with a matching entry in `violations`:

| Code | Meaning |
|---|---|
| `INVALID_DIMENSIONS` | A size is zero or negative |
| `ITEM_TOO_LARGE` | Over the configured item size limit |
| `NO_BOX_AVAILABLE` | There are no active box types |
| `NO_BOX_FITS` | Too big for every active box type |
| `NO_BOX_SUPPORTS_WEIGHT` | Too heavy for every active box type |
| `PROHIBITED_GOODS` | Dangerous goods class is on the prohibited list |
| `BOX_OVERWEIGHT` | Adding it would exceed a box's weight limit |
| `DANGEROUS_GOODS_MUST_BE_SEPARATE` | Needs a dedicated box |
| `FRAGILE_MUST_BE_SEPARATE` | Needs a dedicated box |

**Example 1: item too big**

Input: a 300 × 300 × 300 box and a 500 × 100 × 100 item (`SOFA`).

```json
"unplacedItems": ["SOFA"],
"violations": [{ "code": "NO_BOX_FITS", "itemCode": "SOFA", "message": "..." }]
```

**Example 2: item too heavy**

Input: a box with `MaxWeight` 5 and a 9 kg item (`ANVIL`).

```json
"unplacedItems": ["ANVIL"],
"violations": [{ "code": "NO_BOX_SUPPORTS_WEIGHT", "itemCode": "ANVIL", "message": "..." }]
```

**Example 3: prohibited class**

Input: an item with `"IsDangerousGoods": true, "DangerousGoodsClass": "1"`.

```json
"unplacedItems": ["BOMB"],
"violations": [{ "code": "PROHIBITED_GOODS", "itemCode": "BOMB", "message": "..." }]
```

### What the validator checks

After packing, the validator re-checks the result against the original input. It reports the following as `issues`:

| Check | Issue code (where used) |
|---|---|
| Two items overlap in the same box | `OVERLAP` |
| An item sticks out of its box | `OUT_OF_BOUNDS` |
| A box is over its weight limit | `BOX_OVERWEIGHT` |
| The reported box weight does not match its items | `WEIGHT_MISMATCH` |
| Two input items share the same code | `DUPLICATE_ITEM_CODE` |
| An item is placed more than once | `ITEM_PLACED_TWICE` |
| An item in the output does not exist in the input | `UNKNOWN_ITEM` |
| A box type in the output does not exist in the input | `UNKNOWN_BOX_REFERENCE` |
| Bad or repeated box instance numbers | `INVALID_BOX_INSTANCE`, `DUPLICATE_BOX_INSTANCE` |
| A box was opened but holds nothing | `EMPTY_BOX` |
| An item is listed as unplaced twice | `DUPLICATE_UNPLACED_ITEM` |

t also checks that items are not rotated when they must not be, no inactive box type is used, no type exceeds `MaximumBoxes`, box groups are not mixed, dangerous goods classes and fragile items are not mixed with other goods, and every input item appears exactly once, either placed or unplaced.

**Example: valid result**

```json
"validation": { "valid": true, "issues": [] }
```

**Example: caught problem**

If the input contains two items with the same `ItemCode` (`A`), the solver still packs both, but the validator flags it:

```json
"validation": {
  "valid": false,
  "issues": [
    { "code": "DUPLICATE_ITEM_CODE", "itemCode": "A", "message": "..." },
    { "code": "ITEM_PLACED_TWICE",   "itemCode": "A", "message": "..." }
  ]
}
```

The caller (Portal) can use `validation.valid` to decide whether to trust a result.

### Tolerance for rounding

Weights are decimals, and computers store decimals with tiny rounding errors (for example 0.1 + 0.2 is stored as 0.30000000000000004). The validator allows a tolerance of `0.000001` when comparing weights, so a correct packing is not flagged because of that rounding error.

### Configuration

| Setting | Where | Meaning |
|---|---|---|
| `BOX_TOP_HEADROOM_RATIO` | `src/PackingSolver.cpp` | Fraction of box depth kept free at the top so a lid can close (currently set to `0.0`; `0.02` would keep 2%) |
| `FRAGILE_MAX_LOAD_RATIO` | `src/PackingSolver.cpp` | Maximum load on a fragile item, as a fraction of its weight (default `0.5`) |
| `prohibitedDangerousGoodsClasses` | `Constraints` | Dangerous goods classes that are rejected |
| Item limits | `Constraints` | Optional maximum item weight, volume and dimensions |
| Rotation and box groups | `Constraints` | Switches for allowing rotation and enforcing box groups |
