# Graph Neighbor Verification Test

## Purpose

This test verifies that the graph-based map system correctly represents the grid-based map's neighbor relationships, territory types, and other properties.

## What It Tests

For each of the 96 territories (8 rows × 12 columns):
1. **Territory identification** - Gets the territory name from grid position
2. **Territory type** - Verifies Land/Sea classification matches between grid and graph
3. **Territory value** - Shows the tax value (5 or 10)
4. **Ownership** - Reports which player (if any) owns the territory
5. **Neighbor relationships** - Compares graph neighbors vs expected grid neighbors
6. **Neighbor details** - For each neighbor, shows position and type (Land/Sea)
7. **Validation** - Reports any mismatches or inconsistencies

## Expected Results

- **Total territories tested**: 96
- **Passed**: 96
- **Failed**: 0
- **Success rate**: 100.0%

Each territory should have:
- **Corner territories** (4 total): 2 neighbors each
- **Edge territories** (non-corner): 3 neighbors each
- **Interior territories**: 4 neighbors each

## How to Run

### Option 1: Using Qt Creator (Normal Mode)

1. Open `test_graph_neighbors.pro` in Qt Creator
2. Build the project (Ctrl+B)
3. Run the test (Ctrl+R)
4. Check the "Application Output" pane for results

**Normal mode** shows only failures and summary statistics.

### Option 2: Using Qt Creator (Verbose Mode)

To see detailed output for ALL territories (not just failures):

1. In Qt Creator, go to Projects → Run Settings
2. Under "Command line arguments", add: `-v`
3. Run the test (Ctrl+R)

**Verbose mode** shows every territory with full details.

### Option 3: Command Line (if qmake is in PATH)

```bash
cd C:\Users\dllau\Developer\LAUConquestOfTheEmpire
qmake test_graph_neighbors.pro
make   # or nmake on Windows with MSVC

# Normal mode (failures only)
./test_graph_neighbors

# Verbose mode (all territories)
./test_graph_neighbors -v
```

## Output Format

### Success Output (Normal Mode)
```
========================================
  Graph Neighbor Verification Test
========================================

Map Configuration:
  Grid size: 8 rows x 12 columns
  Total territories: 96
  Land territories: 58
  Sea territories: 38

========================================
  Testing Territory Properties
========================================

(no failures shown in normal mode)

========================================
  Test Results Summary
========================================

Territory Statistics:
  Total territories: 96
  Land territories: 58
  Sea territories: 38
  Owned territories: 6
  Territories with pieces: 6

Test Results:
  Passed: 96
  Failed: 0
  Success rate: 100.0%

✓ ✓ ✓ ALL TESTS PASSED ✓ ✓ ✓
Graph neighbors match grid neighbors perfectly!
Territory types match between grid and graph!

========================================
```

### Verbose Output (showing individual territory)
```
[PASS] T_3_5 at (3,5)
  Grid Type: Land
  Graph Type: Land
  Territory Value: 10
  Owner: A
  Neighbor Count: 4 (expected: 4)
  Neighbors:
    - T_2_5 (2,5) Land
    - T_4_5 (4,5) Sea
    - T_3_4 (3,4) Land
    - T_3_6 (3,6) Land
```

### Failure Output (if there are issues)
```
[FAIL] T_3_5 at (3,5)
  Grid Type: Land
  Graph Type: Sea
  Territory Value: 10
  Owner: None
  Neighbor Count: 3 (expected: 4)
  Neighbors:
    - T_4_5 (4,5) Sea
    - T_3_4 (3,4) Land
    - T_3_6 (3,6) Land
    - [MISSING] T_2_5 (2,5)
  Issues:
    • Type mismatch: Grid=Land, Graph=Sea
    • Neighbor count: Expected=4, Got=3
    • Missing neighbor: T_2_5

========================================
  Test Results Summary
========================================

Territory Statistics:
  Total territories: 96
  Land territories: 58
  Sea territories: 38
  Owned territories: 6
  Territories with pieces: 6

Test Results:
  Passed: 95
  Failed: 1
  Success rate: 99.0%

Issues Found:
  Type mismatches (Grid vs Graph): 1

✗ ✗ ✗ TESTS FAILED ✗ ✗ ✗
Issues detected in graph/grid synchronization.

========================================
```

## What Success Means

If all tests pass, it confirms:
- ✓ **Grid-to-graph conversion** is working correctly
- ✓ **All 96 territories** were created with unique names
- ✓ **Territory naming** is consistent (T_row_col format)
- ✓ **Territory types** (Land/Sea) match between grid and graph
- ✓ **4-directional adjacency** is properly established
- ✓ **Edge and corner cases** are handled correctly
- ✓ **The dual system** (grid + graph) is perfectly synchronized
- ✓ **Neighbor types** are correctly identified as Land or Sea
- ✓ **Territory properties** (value, ownership) are accessible

## Next Steps After Success

Once this test passes, Phase 2 is verified and we can proceed to Phase 3:
- Migrate features to use graph instead of grid
- Add polygon-based click detection
- Implement graph-based pathfinding
- Update movement validation to use graph adjacency
