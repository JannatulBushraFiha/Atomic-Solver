# Atomic-Solver

## **In order to build the program:**
cmake -S . -B build && cmake --build build

## **To run the program**
echo '{
  "boxTypes": [
    {"Reference": "SML", "Width": 150, "Length": 150, "Depth": 150, "MaxWeight": 8.5, "BoxWeight": 0.5, "Active": true, "MaximumBoxes": 100},
    {"Reference": "MED", "Width": 400, "Length": 400, "Depth": 400, "MaxWeight": 15.2, "BoxWeight": 0.75, "Active": true}
  ],
  "items": [
    {"ItemCode": "ITM-001", "ItemReference": "Widget A", "Width": 100, "Length": 200, "Depth": 50, "Weight": 1, "BoxGroup": "GROUP-A"},
    {"ItemCode": "ITM-002", "ItemReference": "Widget B", "Width": 300, "Length": 150, "Depth": 75, "Weight": 2.8},
    {"ItemCode": "ITM-003", "ItemReference": "Kettle", "Width": 300, "Length": 200, "Depth": 100, "Weight": 4.2, "ShipInOwnPackaging": true}
  ]
}' | ./build/solver

## **Ship in own packaging**
Set `"ShipInOwnPackaging": true` on an item to ship it as-is in its manufacturer packaging.
The item's Width/Length/Depth/Weight must then describe the outer packaging. Such items are
never packed into a carton: they do not appear in `placements` or `usedBoxes`, do not count
toward a box type's `MaximumBoxes`, and are returned in a separate `ownPackagedItems` array:

```json
"ownPackagedItems": [
  {"itemCode": "ITM-003", "dimension": {"width": 300, "length": 200, "depth": 100}, "weight": 4.2}
]
```

Item-level checks still apply (dimensions above zero, non-negative weight, prohibited
dangerous goods classes, and any configured per-item limits). A flagged item that fails one
is reported in `unplacedItems` with a violation, as usual.

See `docs/ship-in-own-packaging.md` for the integration notes shared with FitPortal and FitVisualizer.