# Atomic-Solver

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

### Constraints and out of scope for MVP

18. No role based access control
19. No picking logic, packing only
20. No box stacking or top heavy considerations
21. No item orientation rules (e.g. fragile side up)
22. No box mode vs fluid/volume mode distinction
23. No margin or air gap calculations between items
24. Test data and format will be provided by Thomax

> **Status notes**
> - Requirement 5: rules are held in one Constraints object with adjustable settings (item weight, volume and size limits, group enforcement, rotation, prohibited dangerous goods classes). A full configurable rule language is not built yet, and settings are not yet read from the JSON input.
> - Requirement 7: items in different box groups are never packed together. Items in the same group are allowed to share a box, but are not forced into one.

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
