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
    {"ItemCode": "ITM-002", "ItemReference": "Widget B", "Width": 300, "Length": 150, "Depth": 75, "Weight": 2.8}
  ]
}' | ./build/solver

## **Constraints**
An optional `constraints` block sets the rules the packer works under. Leave it out and nothing
is limited, which is how the solver behaved before constraints existed.

| Field | Default | Meaning |
| --- | --- | --- |
| `MaxItemWeight` | 0 | Heaviest single item accepted. 0 means no limit. |
| `MaxItemVolume` | 0 | Largest item volume accepted. 0 means no limit. |
| `MaxItemDimension` | none | Largest item size accepted, as `Width` / `Length` / `Depth`. |
| `EnforceBoxGroups` | true | Keep items with different `BoxGroup` values in separate boxes. |
| `AllowRotation` | true | Let items be turned to fit. When false they are packed as given. |

echo '{
  "boxTypes": [
    {"Reference": "MED", "Width": 400, "Length": 400, "Depth": 400, "MaxWeight": 15.2, "Active": true}
  ],
  "items": [
    {"ItemCode": "ITM-001", "ItemReference": "Widget A", "Width": 100, "Length": 200, "Depth": 50, "Weight": 9}
  ],
  "constraints": {
    "MaxItemWeight": 5.0,
    "MaxItemDimension": {"Width": 300, "Length": 300, "Depth": 300},
    "EnforceBoxGroups": true,
    "AllowRotation": true
  }
}' | ./build/solver

An item that breaks a rule does not fail the request. It goes into `unplacedItems` as before, and
a matching entry appears in the new `violations` list saying why:

{
  "placements": [],
  "unplacedItems": ["ITM-001"],
  "usedBoxes": [],
  "violations": [
    {"code": "ITEM_TOO_HEAVY", "itemCode": "ITM-001", "message": "Weight 9 exceeds the limit of 5"}
  ]
}

Violation codes: `INVALID_DIMENSIONS`, `INVALID_WEIGHT`, `ITEM_TOO_HEAVY`, `ITEM_TOO_LARGE`,
`NO_BOX_AVAILABLE`, `NO_BOX_FITS`, `NO_BOX_SUPPORTS_WEIGHT`, `NO_SPACE_AVAILABLE`.