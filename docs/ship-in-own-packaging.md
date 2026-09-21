# Ship in Own Packaging: solver contract change

**For:** FitPortal and FitVisualizer teams
**From:** FitSolver (Atomic Fit)

## What it is

An item can now be flagged to ship in its own manufacturer / product packaging.
The solver will not pack a flagged item into a carton. It comes back as its own parcel.
This is a hard rule: a flagged item is never consolidated, even if it would fit.

## Input (sent by FitPortal)

One new optional boolean per item: `ShipInOwnPackaging`. Defaults to `false` when absent.

```json
{
  "ItemCode": "ITM-007",
  "ItemReference": "Kettle",
  "Width": 300, "Length": 200, "Depth": 100,
  "Weight": 4.2,
  "ShipInOwnPackaging": true
}
```

Rules for flagged items:
- `Width` / `Length` / `Depth` / `Weight` must be the **outer dimensions and gross weight of the product packaging**, not the bare product. The solver passes them through unchanged.
- One entry is one physical unit. Ten units means ten entries (same as today).
- `BoxGroup` is ignored for flagged items.
- `IsFragile` and `IsDangerousGoods` are accepted. A prohibited dangerous goods class (currently class `"1"`) is still rejected.
- The solver's per-item limits (max item weight, volume, dimensions) still apply.

## Output (read by both teams)

One new top-level array, `ownPackagedItems`. It is always present, empty when unused.

```json
{
  "placements":   [ ...carton items only... ],
  "usedBoxes":    [ ...cartons only... ],
  "ownPackagedItems": [
    {
      "itemCode": "ITM-007",
      "dimension": { "width": 300, "length": 200, "depth": 100 },
      "weight": 4.2
    }
  ],
  "unplacedItems": [],
  "violations": [],
  "validation": { "valid": true, "issues": [] },
  "timeMs": 0.4
}
```

- Flagged items **never** appear in `placements` or `usedBoxes`.
- A flagged item that fails an item check (zero dimension, negative weight, prohibited DG class, over the item limits) appears in `unplacedItems` with a matching `violations` entry, exactly as today.
- Every input item appears in exactly one of `placements`, `unplacedItems`, `ownPackagedItems`.
- All other fields are unchanged.

## FitPortal: what to be aware of

- **Parcel count** is now `usedBoxes.length + ownPackagedItems.length`.
- **Shipment weight** is the sum of `usedBoxes[].totalWeight` plus the sum of `ownPackagedItems[].weight`.
- Flagged items do not count toward a box type's `MaximumBoxes`.
- When the checkbox is ticked, the portal should require packaged dimensions and weight to be entered, since the solver cannot tell the difference.

## FitVisualizer: what to be aware of

- There is nothing to draw inside a carton for these items. Show each one as its own single-item parcel using `dimension` and `weight`, so staff do not think the item is missing.
- Entries have no `boxReference`. Do not look them up in `boxTypes`.
- Rendering of `placements` and `usedBoxes` is unchanged.

## Open points

- The field name `ShipInOwnPackaging` is negotiable. Tell us if you want something else.
- We can add `isDangerousGoods` and `dangerousGoodsClass` to each `ownPackagedItems` entry if either team needs to know a standalone parcel is DG. Say so and we will include it.
