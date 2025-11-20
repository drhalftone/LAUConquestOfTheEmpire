# Graph Neighbor Verification Test

## Purpose

This test verifies that the graph-based map system correctly represents the grid-based map's neighbor relationships.

## What It Tests

For each of the 96 territories (8 rows × 12 columns):
1. Gets the territory name from the grid position
2. Queries the graph for that territory's neighbors
3. Calculates the expected neighbors from the grid (4-directional: up, down, left, right)
4. Compares the graph neighbors with expected grid neighbors
5. Reports any mismatches

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

### Option 1: Using Qt Creator

1. Open `test_graph_neighbors.pro` in Qt Creator
2. Build the project (Ctrl+B)
3. Run the test (Ctrl+R)
4. Check the "Application Output" pane for results

### Option 2: Command Line (if qmake is in PATH)

```bash
cd C:\Users\dllau\Developer\LAUConquestOfTheEmpire
qmake test_graph_neighbors.pro
make   # or nmake on Windows with MSVC
./test_graph_neighbors   # or test_graph_neighbors.exe on Windows
```

## Output Format

### Success Output
```
=== Starting Graph Neighbor Verification Test ===

Map generated with 96 territories
Grid size: 8 rows x 12 columns
Expected territories: 96

=== Test Summary ===
Total territories tested: 96
Passed: 96
Failed: 0
Success rate: 100.0%

✓ ALL TESTS PASSED - Graph neighbors match grid neighbors perfectly!
```

### Failure Output (if there are issues)
```
FAIL: Territory T_3_5 at (3,5)
  Missing expected neighbor: T_2_5
  Expected neighbors: ["T_2_5", "T_4_5", "T_3_4", "T_3_6"]
  Graph neighbors:    ["T_4_5", "T_3_4", "T_3_6"]

=== Test Summary ===
Total territories tested: 96
Passed: 95
Failed: 1
Success rate: 99.0%

✗ SOME TESTS FAILED - Graph neighbors do not match grid neighbors
```

## What Success Means

If all tests pass, it confirms:
- ✓ Grid-to-graph conversion is working correctly
- ✓ All 96 territories were created
- ✓ Territory naming is consistent
- ✓ 4-directional adjacency is properly established
- ✓ Edge and corner cases are handled correctly
- ✓ The dual system (grid + graph) is synchronized

## Next Steps After Success

Once this test passes, Phase 2 is verified and we can proceed to Phase 3:
- Migrate features to use graph instead of grid
- Add polygon-based click detection
- Implement graph-based pathfinding
- Update movement validation to use graph adjacency
