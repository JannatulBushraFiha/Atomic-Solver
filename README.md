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

## **Validation**
After packing, the solution is checked against the input and reported under `validation`. This is a
safety net over the packer, so a failure here means the solver has a bug rather than the caller
sending bad data. An item rejected by a constraint is not a validation failure: the packer handled
it correctly, so `violations` fills in while `validation.valid` stays true.

{
  "validation": {"valid": true, "violations": []}
}

It checks that nothing pokes out of its box, that no two items in the same box share space, that
every placed size is a rotation the constraints allow, that every item comes back exactly once as
either packed or unplaced with a reason, that box weights add up and stay under their limits, that
box groups are not mixed, and that no box type is used more often than `MaximumBoxes` allows.

Validation runs by default. Set `"validate": false` at the top level of the input to skip it.

Validation codes: `OVERLAP`, `OUT_OF_BOUNDS`, `NEGATIVE_POSITION`, `INVALID_ROTATION`,
`INVALID_BOX_INSTANCE`, `UNKNOWN_ITEM`, `UNKNOWN_BOX_REFERENCE`, `INACTIVE_BOX`,
`DUPLICATE_ITEM_CODE`, `DUPLICATE_UNPLACED_ITEM`, `DUPLICATE_BOX_INSTANCE`, `ITEM_MISSING`,
`ITEM_PLACED_TWICE`, `ITEM_PLACED_AND_UNPLACED`, `MISSING_REASON`, `MISSING_USED_BOX`,
`EMPTY_BOX`, `WEIGHT_MISMATCH`, `BOX_OVERWEIGHT`, `MIXED_BOX_GROUPS`, `BOX_LIMIT_EXCEEDED`.