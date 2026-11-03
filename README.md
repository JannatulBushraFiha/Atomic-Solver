# Atomic-Solver
## Introduction

**Atomic-Solver** is a 3D bin-packing engine written in C++20 that works out how to pack a set of items into shipping boxes (cartons). You give it the available box types and the items to ship. It returns a packing plan that says which box each item goes in, its exact (x, y, z) position, and the orientation it is placed in. The goal is to use as few boxes, and as little total box volume, as possible.

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

## Project Description

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

It also checks that items are not rotated when they must not be, no inactive box type is used, no type exceeds `MaximumBoxes`, box groups are not mixed, dangerous goods classes and fragile items are not mixed with other goods, and every input item appears exactly once, either placed or unplaced.

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

## 3. Testing

The tests live in the **tests** folder. CMake builds them into two programs, **unitTests** and **integrationTest**, and ctest runs both (see "To run the tests" above). No test library is needed: both programs use a small framework written for this project.

There are **202 tests** in total: 139 unit tests and 63 integration tests. All of them pass on the current code.

### How the test framework works

**test_helpers.h** holds the framework and the shared helpers.

| Piece | What it does |
|---|---|
| **TEST(name)** | Defines a test and adds it to a global list when the program starts, so tests never have to be registered by hand. |
| **CHECK(x)** | If x is false, prints the file and line and marks the test as failed, then carries on so one run shows every problem. |
| **CHECK_EQ(a, b)** | Same as CHECK, but also prints both values. |
| **REQUIRE(x)** | Same as CHECK, but stops the test straight away. Used before reading something that may not exist, such as the first placement. |

**test_main.cpp** loops through the list, runs each test and prints **[ PASS ]** or **[ FAIL ]** next to its name. If a test throws an exception it is caught and counted as a failure, so one crash does not stop the run. At the end it prints "X of Y tests passed" and exits with 0 if everything passed or 1 if not, which is what ctest reads.

The helper file also has builders (**makeItem**, **makeFragile**, **makeDangerous**, **makeGrouped** and **makeBox**) so a test can set up an item or box in one line, and four checks that are reused across the solver tests:

| Helper | What it checks |
|---|---|
| **everyItemAccountedFor** | Every input item appears exactly once in the output, either placed or unplaced. |
| **stackingIsLegal** | No item rests on a lighter item, and the weight on a fragile item is at most half of its own weight. The validator does not check stacking, so the tests do. |
| **boxesNumberedFromOne** | Boxes of each type are numbered 1, 2, 3 and so on with no gaps. |
| **boxOf** | Returns the box an item ended up in (for example M#1), so a test can check two items were kept apart. |

Most solver tests go through **solveAndCheck**. It runs the solver, passes the result to the validator, then runs the first three checks above. This means every solver test also proves the packing is valid, even when the test itself is about something else such as box choice.

### Unit tests

| File | Tests | What it covers |
|---|---|---|
| **test_constraints.cpp** | 40 | The rotation code and the Constraints checks, called directly |
| **test_solver.cpp** | 43 | The full solver on small jobs with a known answer, plus random jobs |
| **test_validator.cpp** | 35 | The validator on packings built by hand |
| **test_edge_cases.cpp** | 21 | The solver program run from the command line with real JSON |

#### test_constraints.cpp

These tests call the Orientation and Constraints code on its own, without the solver.

- **Rotations:** an item with three different sides gives exactly six rotations, all different, all with the same volume, and one of them is the original. Ordinary items get all six. Fragile items and dangerous goods get only one, the way they were given. Setting allowRotation to false gives one rotation for every item.
- **Item check (checkItem):** each rejection reason has a test with an item built to trigger it. A zero or negative side gives INVALID_DIMENSIONS, a negative weight gives INVALID_WEIGHT, class 1 gives PROHIBITED_GOODS, going over a configured item limit gives ITEM_TOO_HEAVY or ITEM_TOO_LARGE, no active boxes gives NO_BOX_AVAILABLE, too big for every box gives NO_BOX_FITS and too heavy for every box gives NO_BOX_SUPPORTS_WEIGHT.
- **Edges of the item check:** a 0 kg item is accepted, an item exactly at the weight limit is accepted, inactive boxes are ignored when looking for a fit, and an item that only fits when turned is accepted unless it is fragile, dangerous goods, or rotation is switched off. A box that fits but is too weak, next to a box that is strong enough but too small, gives NO_BOX_FITS with a message that mentions the weight.
- **Placement (allowsPlacement):** these tests build the state of a box by hand (its current weight, item count, group and flags) and ask whether an item may join it. They cover the weight limit (exactly at the limit is allowed, over is refused), mixed box groups (refused unless the rule is switched off), items with no group (allowed anywhere), dangerous goods and ordinary items in both directions, two dangerous goods classes, and fragile items only joining other fragile items.
- **The 0 kg bug:** a box holding only 0 kg items has a weight of zero but is not empty. These tests set the item count to 1 and the weight to 0, then check that a dangerous goods item or a fragile item is still refused. The rule now looks at how many items are in the box instead of its weight.

#### test_solver.cpp

These tests run the full PackingSolver on small jobs where the right answer is known, so the test can check the exact result and not just that it is valid.

| Group | What is tested |
|---|---|
| **Basics** | An empty job gives an empty result. One item goes in one box at (0, 0, 0). An item exactly the size of the box fits. An item that only fits on its side is turned to fit. 27 cubes of 10 fill a 30 cube in exactly one box. |
| **Weight** | Three 4 kg items with a 10 kg limit need 2 boxes. The reported box weight matches its items. An item heavier than every box is unplaced with NO_BOX_SUPPORTS_WEIGHT. A MaxWeight of 0 means no limit. |
| **Unplaceable items** | Items that are too big, prohibited, have a bad size or weight, have no boxes to go in, or are fragile or dangerous goods that only fit when turned. Each one must be in unplacedItems with the right violation code, and the other items in the same job must still be packed. |
| **Box types** | Inactive types are never used. MaximumBoxes is respected and the extra item is unplaced. The smaller box is chosen when everything fits in it. A plan can mix a large and a small box. When one type runs out, the solver moves on to another. |
| **Settings** | With rotation off, an item that only fits when turned is unplaced. With the group rule off, two groups share one box. |
| **Separation rules** | Dangerous goods get their own box. Different classes never share, the same class can. Fragile items get their own box. Different box groups never share, the same group can, and items with no group can join any group. |
| **0 kg items** | Four tests pack a 0 kg item with a dangerous goods or fragile item (and the other way round). The two always end up in different boxes. Ten 0 kg items all pack into one box. |
| **Stacking** | A 5 kg and a 1 kg cube go in a box only tall enough to stack them, and the 5 kg cube must be at the bottom. A second test feeds stacks built by hand into stackingIsLegal to prove the helper really catches a bad stack. |

**Random jobs.** The **randomItems** function makes items with random sizes and weights from a fixed seed (std::mt19937), so the same seed gives the same items on every machine. Roughly 1 in 10 items are fragile, 1 in 10 are dangerous goods (class 3 or 4), 1 in 10 belong to a box group, and about 1 in 10 weigh 0 kg. Five tests use it to run **74 random jobs** of 40 to 60 items each:

- plain jobs (25 seeds)
- items with several flags at once, such as fragile dangerous goods in a box group (16 seeds)
- boxes with their own weight set (16 seeds)
- rotation switched off, checking every item keeps its original size on each axis (6 seeds)
- a limit of 2 small and 3 large boxes, checking the limits hold (11 seeds)

Every random job goes through solveAndCheck. Every item is small enough to fit the small box on its own, so apart from the box limit test, nothing may be left unplaced.

**Same answer every time.** The same 50 random items are solved twice, and every field of every placement (item, box, instance, position and size) must match.

**Speed.** 150 random items must solve in under 5 seconds. The limit is generous so it does not fail on a slow laptop or an unoptimised build, so it is a smoke test for the 1 to 2 second requirement rather than a benchmark. On a development laptop it takes about 0.55 seconds with the default build and about 0.1 seconds in a Release build.

#### test_validator.cpp

The validator checks a finished packing, so these tests build packings by hand without the solver. Most start from a correct packing (two 10 × 10 × 10 items side by side in a 100 × 100 × 100 box), break one thing, and check the validator reports the matching issue code.

| Area | What is broken | Issue code expected |
|---|---|---|
| **Position** | Items overlap, an item sticks out of the box, a negative position | OVERLAP, OUT_OF_BOUNDS, NEGATIVE_POSITION |
| **Weight** | Box over its limit, reported weight wrong | BOX_OVERWEIGHT, WEIGHT_MISMATCH |
| **Items** | Item missing, placed twice, both placed and unplaced, unplaced with no reason, unknown item, duplicate code in the input, listed as unplaced twice | ITEM_MISSING, ITEM_PLACED_TWICE, ITEM_PLACED_AND_UNPLACED, MISSING_REASON, UNKNOWN_ITEM, DUPLICATE_ITEM_CODE, DUPLICATE_UNPLACED_ITEM |
| **Boxes** | Unknown box type, inactive box used, too many of one type, box opened but empty, box missing from the used list, instance number 0, same box listed twice | UNKNOWN_BOX_REFERENCE, INACTIVE_BOX, BOX_LIMIT_EXCEEDED, EMPTY_BOX, MISSING_USED_BOX, INVALID_BOX_INSTANCE, DUPLICATE_BOX_INSTANCE |
| **Rules** | Fragile item turned, dangerous goods mixed with ordinary items or another class, fragile mixed with ordinary, two box groups in one box | INVALID_ROTATION, DANGEROUS_GOODS_MIXED, FRAGILE_NOT_SEPARATE, MIXED_BOX_GROUPS |

Overlap and bounds are tested on every axis. The items are 10 × 20 × 30 so that mixing up width, length and depth anywhere in the code would show up. Moving an item one unit into its neighbour must give OVERLAP, and moving it so they only touch must not. An item touching the far wall is fine, and one unit further must give OUT_OF_BOUNDS. Two items in the same spot but in different boxes are not an overlap.

Other tests check the validator does not raise false alarms. Three 0.1 kg items in a box with a 0.3 kg limit pass, even though 0.1 + 0.1 + 0.1 is stored as slightly more than 0.3. Mixed groups pass when the group rule is off, and fragile items or dangerous goods of the same class may share a box.

#### test_edge_cases.cpp

Most of these tests run the real **solver** program, the same way a user or the Flask API would. The **runSolver** helper writes the input JSON to a temporary file, runs the solver with that file as standard input, then reads back the output and exit code. CMake passes the path of the built solver into the test program and makes sure the solver is built first.

| Area | What is tested |
|---|---|
| **Bad input** | Broken JSON, missing boxTypes, a width of "wide", a missing or text weight, boxTypes or items sent as an object instead of a list, a Quantity of 0, negative or 2.5, weights of NaN, Infinity or 1e309, and a side larger than the integer range (2147483648). Each must exit with a code other than 0 and an error starting with "Invalid JSON:" or "Bad input:". |
| **Quantity** | A missing Quantity means 1. A Quantity of 3 gives three placements, all reported under the original code. 3.0 is accepted as 3. Unplaced units are also reported under the original code. |
| **Code clashes** | BOOK with a Quantity of 2 is packed next to a real item called BOOK-1. The test tells them apart by size and checks there are exactly two BOOK placements and one BOOK-1. |
| **Duplicate codes** | Two items with the same code are both packed, and the validation output flags DUPLICATE_ITEM_CODE. |
| **Rounding sizes** | A 10.9 box is rounded down to 10, so a 10 cube fits and an 11 long item does not. A 9.9999999999 box still takes a 10.0000000001 item, so tiny floating point noise is not rounded the wrong way. |

The last three tests call the code directly. One checks that a huge item (a million units on each side) is rejected as ITEM_TOO_LARGE against a volume limit without the volume overflowing. The other two check the fragile load limit on a 10 kg fragile base: a 4.9 kg or 5.0 kg item is stacked on top of it in the same box, and a 5.1 kg item is put in a second box.

### Integration tests

**integrationTest.cpp** is a separate program with its own copy of the same idea (TEST_CASE, CHECK and REQUIRE). It prints **[ RUN ]** before each test and **[ OK ]** or **[ FAIL ]** after, then a passed and failed count. It is split into four sections:

| Section | Tests | What it does |
|---|---|---|
| **1. Validator** | 26 | Solutions built by hand and fed to the validator, one broken rule per test, the same idea as test_validator.cpp. |
| **2. Solver** | 7 | The solver alone: smallest box that fits, items sharing a box, a second box opened when the first is full, weight limit, MaximumBoxes, an item too large for every box, inactive box types. |
| **3. Integration** | 20 | The solver output is passed straight into the validator with the same constraints, and the result must be valid. |
| **4. Constraints** | 10 | checkItem and allowsPlacement called directly, including 0 kg dangerous goods of the same and different classes. |

Section 3 is the important one. The solver and the validator are written separately, so this section proves they agree once wired together. If the solver ever makes a placement the validator thinks is illegal, the test fails. It covers a small three item order, mixed box types and instances, a weight limit that forces at least 3 boxes, an item too big to pack (still a valid result as long as it is reported), box groups, an empty order, no boxes at all, a 50 item seeded random order, an item exactly the size of the box and one a single unit too long, items exactly at the box weight limit, a MaximumBoxes of 0, and eight jobs mixing 0 kg and normal weight items with dangerous goods and fragile items.

### Results

Running ctest on the current code:

```
1/2 Test #1: integrationTest ..................   Passed    0.06 sec
2/2 Test #2: unitTests ........................   Passed    1.35 sec

100% tests passed out of 2
```

All 139 unit tests and all 63 integration tests pass.

### What the tests do not cover

- The Flask API (app.py) and the Docker deployment have no automated tests.
- The speed test checks a single 150 item order. It does not test the peak load of 1000 orders per second from the requirements.

## Conclusion

Atomic-Solver takes a list of items and box types as JSON and returns a packing plan: which boxes to open, which box each item goes in, and exactly where and which way round it sits. It builds several plans with a best fit heuristic that fills the floor first, keeps the one with the most items placed, then the fewest boxes, then the least box volume, and improves it with a consolidation pass and a seeded ruin and recreate search. The same input always gives the same output.

The shipping rules (weight limits, box stock, box groups, fragile items, dangerous goods and stacking) are enforced twice: by **Constraints** while packing, and again by an independent **Validator** on the finished plan. Any item that cannot be packed is reported in unplacedItems with a reason, and validation.valid tells the caller (Portal) whether the result can be trusted.

The solver runs from the command line, or as a web service through the Flask API on Render. It is covered by **202 automated tests** across the constraints, the solver, the validator, the command line, and the solver and validator working together, and all of them pass.
