# Graph-Based Map Migration Plan

## Overview

This document outlines the plan to migrate from the current grid-based map system to a flexible graph-based representation. This will enable support for irregularly-shaped territories (like the actual Roman Empire) while maintaining backward compatibility during the transition.

## Goals

- Replace grid-based territory representation with graph-based system
- Support arbitrary territory shapes and adjacency relationships
- Enable realistic historical maps with proper geography
- Maintain backward compatibility during migration
- Improve pathfinding and movement mechanics

## Current System (Grid-Based)

- **Representation**: 8x12 grid of tiles (`m_tiles[row][col]`)
- **Territory Identification**: `Position{row, col}` struct
- **Adjacency**: 4-directional (up, down, left, right)
- **Limitations**:
  - All territories are rectangular
  - Fixed 4 neighbors maximum
  - Unrealistic geography
  - Click detection based on grid cells

## Target System (Graph-Based)

- **Representation**: Graph of named territories with polygon boundaries
- **Territory Identification**: String names (e.g., "Rome", "Egypt")
- **Adjacency**: Arbitrary neighbor lists stored in graph
- **Benefits**:
  - Realistic territory shapes (Italy, Greece, etc.)
  - Variable neighbor counts (2-8+ neighbors)
  - Proper pathfinding algorithms
  - Polygon-based click detection
  - Historical accuracy

---

## Phase 1: Foundation - Create Graph Data Structure

**Goal**: Implement the core graph structure without breaking existing code.

### Tasks

- [ ] Create `mapgraph.h` and `mapgraph.cpp` files
- [ ] Define `Territory` struct with:
  - [ ] `QString name` - Unique territory identifier
  - [ ] `QPointF centroid` - Center point for rendering pieces
  - [ ] `QPolygonF boundary` - Territory shape for click detection
  - [ ] `QList<QString> neighbors` - Adjacent territory names
  - [ ] `TerritoryType type` - Enum (Land, Sea, Mountain, etc.)
  - [ ] `QColor color` - Optional visual distinction
  - [ ] `QPointF labelPosition` - Where to render territory name
- [ ] Implement `MapGraph` class with:
  - [ ] `QMap<QString, Territory> m_territories` - Territory storage
  - [ ] `void addTerritory(const Territory &territory)` - Add new territory
  - [ ] `Territory getTerritory(const QString &name) const` - Lookup by name
  - [ ] `QList<QString> getTerritoryNames() const` - Get all territory names
  - [ ] `QList<QString> getNeighbors(const QString &name) const` - Get adjacent territories
  - [ ] `bool areAdjacent(const QString &t1, const QString &t2) const` - Check adjacency
  - [ ] `QString getTerritoryAt(const QPointF &point) const` - Click detection via polygon
  - [ ] `QPointF getCentroid(const QString &name) const` - Get territory center
  - [ ] `bool isSeaTerritory(const QString &name) const` - Check if sea
  - [ ] `bool exists(const QString &name) const` - Check if territory exists
- [ ] Add graph to build system (`.pro` file)
- [ ] Write unit tests for basic graph operations

### Acceptance Criteria

- MapGraph can store and retrieve territories
- Neighbor relationships work correctly
- Point-in-polygon detection works for click handling
- No changes to existing MapWidget code yet

---

## Phase 2: Dual System - Grid + Graph Coexistence

**Goal**: Add graph alongside existing grid, create converter from grid to graph.

### Tasks

- [ ] Add `MapGraph *m_graph` member to MapWidget
- [ ] Implement `void MapWidget::buildGraphFromGrid()` to:
  - [ ] Create territory for each grid cell
  - [ ] Generate simple rectangular boundaries for each cell
  - [ ] Set centroid to cell center
  - [ ] Auto-detect neighbors (4-directional from grid)
  - [ ] Classify as land/sea based on existing tile type
  - [ ] Generate territory names from row/col (e.g., "T_2_5")
- [ ] Add `QString MapWidget::positionToTerritoryName(const Position &pos)` converter
- [ ] Add `Position MapWidget::territoryNameToPosition(const QString &name)` converter
- [ ] Update MapWidget constructor to:
  - [ ] Initialize grid (existing code)
  - [ ] Call `buildGraphFromGrid()` to populate graph
- [ ] Add debug visualization option to show:
  - [ ] Territory boundaries (polygons)
  - [ ] Neighbor connections (lines between centroids)
  - [ ] Territory names at centroids

### Acceptance Criteria

- Graph is populated from grid on initialization
- Both systems contain identical information
- Can convert between Position and territory name
- Debug visualization confirms graph structure is correct
- Game still runs exactly as before (grid is still primary)

---

## Phase 3: Migrate Core Features to Graph

**Goal**: Replace grid-based lookups with graph-based equivalents, one feature at a time.

### Phase 3.1: Territory Name Lookups

- [ ] Update `getTerritoryNameAt(row, col)` to use graph
- [ ] Add overload `getTerritoryNameAt(const QPointF &point)` using polygon hit detection
- [ ] Update click handling to use point-based lookup instead of grid
- [ ] Test: Clicking territories still works correctly

### Phase 3.2: Adjacency and Neighbors

- [ ] Create `QList<QString> MapWidget::getAdjacentTerritoryNames(const QString &territory)`
- [ ] Update `getAdjacentSeaTerritories()` to:
  - [ ] Accept territory name instead of Position
  - [ ] Use `m_graph->getNeighbors()`
  - [ ] Filter for sea territories
- [ ] Update movement validation to use graph adjacency
- [ ] Test: Movement to adjacent territories only

### Phase 3.3: Pathfinding

- [ ] Implement `QList<QString> MapGraph::findPath(const QString &from, const QString &to)` using BFS
- [ ] Implement `int MapGraph::getDistance(const QString &from, const QString &to)`
- [ ] Add `bool MapGraph::isReachable(const QString &from, const QString &to)` for connectivity checks
- [ ] Update movement points calculation to use graph distance
- [ ] Test: Pathfinding matches grid-based distance

### Phase 3.4: Territory Type Queries

- [ ] Update `isSeaTerritory()` to accept territory name
- [ ] Add `TerritoryType getType(const QString &territory)` to graph
- [ ] Replace all `m_tiles[row][col]` type checks with graph queries
- [ ] Test: Sea/land distinction still works

### Phase 3.5: Piece Placement

- [ ] Update `Position` references to use territory names where possible
- [ ] Keep Position for backward compatibility but derive from territory centroid
- [ ] Update piece storage to reference territory names
- [ ] Test: Pieces appear in correct territories

### Acceptance Criteria

- All territory lookups use graph instead of grid
- Movement uses graph pathfinding
- Click detection uses polygon boundaries
- Game plays identically to before (but uses graph internally)
- Grid still exists but is rarely accessed

---

## Phase 4: External Map Definition

**Goal**: Load territory definitions from external file instead of generating from grid.

### Tasks

- [ ] Design map definition file format (JSON recommended):
  ```json
  {
    "name": "Roman Empire 270 AD",
    "territories": [
      {
        "name": "Rome",
        "type": "land",
        "centroid": [400, 300],
        "boundary": [[380,280], [420,280], [430,310], [390,320]],
        "neighbors": ["Ravenna", "Naples", "Corsica"],
        "color": "#8B4513",
        "labelPosition": [400, 295]
      }
    ]
  }
  ```
- [ ] Implement `bool MapGraph::loadFromJson(const QString &filePath)`
- [ ] Implement `bool MapGraph::saveToJson(const QString &filePath)` for map editor
- [ ] Create initial map definition file with realistic Roman territories:
  - [ ] Italy (Rome, Ravenna, Naples, Sicily)
  - [ ] Gaul (several territories)
  - [ ] Hispania (several territories)
  - [ ] Britannia
  - [ ] Germania territories
  - [ ] Greece and Balkans
  - [ ] Asia Minor
  - [ ] Egypt
  - [ ] North Africa
  - [ ] Mediterranean Sea (multiple regions)
  - [ ] Black Sea
  - [ ] Atlantic Ocean
- [ ] Update MapWidget to load from JSON instead of hardcoded grid
- [ ] Add map selection option to load different maps

### Acceptance Criteria

- Map loads from external JSON file
- Territory shapes are realistic and based on historical geography
- Game works with new realistic map
- Multiple maps can be loaded

---

## Phase 5: Visual Improvements

**Goal**: Enhance rendering to work with irregular territories.

### Tasks

- [ ] Implement polygon rendering for territory boundaries
- [ ] Add territory fill colors (land vs sea, different regions)
- [ ] Improve territory name rendering (curved text, better positioning)
- [ ] Add visual borders between territories
- [ ] Implement hover highlighting for territories
- [ ] Add minimap showing full world
- [ ] Support zoom and pan for larger maps
- [ ] Add background texture/image support

### Acceptance Criteria

- Map looks professional with clear territory boundaries
- Easy to see which territories are adjacent
- Territory names are readable
- Hover effects make interaction clear

---

## Phase 6: Cleanup and Grid Removal

**Goal**: Remove grid-based code entirely, complete migration.

### Tasks

- [ ] Remove `TileType m_tiles[ROWS][COLUMNS]` array
- [ ] Remove `ROWS` and `COLUMNS` constants
- [ ] Remove `Position` struct (or repurpose as pixel coordinates only)
- [ ] Update all remaining Position references to use territory names
- [ ] Remove `positionToTerritoryName()` and `territoryNameToPosition()` converters
- [ ] Update Player class to store territory names instead of Positions
- [ ] Update GamePiece to use territory names instead of Positions
- [ ] Remove `buildGraphFromGrid()` function
- [ ] Clean up any grid-related helper functions
- [ ] Update documentation to reflect graph-based system

### Acceptance Criteria

- No grid-related code remains
- All features work with graph-based territories
- Code is cleaner and more maintainable
- Game runs smoothly with realistic map

---

## Phase 7: Advanced Features (Future Enhancements)

**Goal**: Leverage graph structure for new gameplay features.

### Potential Enhancements

- [ ] Map editor tool for creating custom maps
- [ ] Multiple map support (different time periods, fantasy maps)
- [ ] Strategic resources tied to specific territories
- [ ] Variable movement costs (mountains, roads, rivers)
- [ ] Supply lines based on graph connectivity
- [ ] Siege mechanics for cities
- [ ] Naval routes separate from land routes
- [ ] Fog of war / exploration mechanics
- [ ] Dynamic territory control visualization
- [ ] Historical event triggers based on territories

---

## Testing Strategy

### Unit Tests

- MapGraph basic operations (add, lookup, neighbors)
- Pathfinding algorithms (BFS, shortest path)
- Point-in-polygon detection
- Territory type classification
- JSON serialization/deserialization

### Integration Tests

- Grid-to-graph conversion accuracy
- Click detection matches visual territories
- Movement respects graph adjacency
- Piece placement in correct territories
- Territory ownership tracking

### Regression Tests

- All existing gameplay works during migration
- Purchase phase with new territories
- Combat in graph-based territories
- City placement and fortification
- Turn progression and scoring

---

## Risk Mitigation

### Risk: Breaking Existing Gameplay

**Mitigation**:
- Maintain dual system (grid + graph) during transition
- Extensive testing at each phase
- Keep backward compatibility until Phase 6

### Risk: Performance Issues with Polygon Detection

**Mitigation**:
- Spatial indexing (quadtree) for fast click detection
- Cache polygon bounding boxes for quick rejection tests
- Profile and optimize if needed

### Risk: Complex Territory Definitions

**Mitigation**:
- Start with simplified map (fewer territories)
- Build map editor tool to make creation easier
- Use historical maps as reference

### Risk: Save Game Compatibility

**Mitigation**:
- Version save game format
- Add migration code for old Position-based saves
- Test loading old games with new code

---

## Success Metrics

- [ ] Graph-based map fully implemented and tested
- [ ] Realistic Roman Empire map loaded from file
- [ ] All game features work with new map
- [ ] No grid-based code remains
- [ ] Performance is acceptable (no lag on clicks or rendering)
- [ ] Code is cleaner and more maintainable
- [ ] Game is more enjoyable with realistic geography

---

## Timeline Estimate

- **Phase 1**: 2-3 days (foundation)
- **Phase 2**: 2-3 days (dual system)
- **Phase 3**: 5-7 days (feature migration)
- **Phase 4**: 3-4 days (external maps)
- **Phase 5**: 3-4 days (visual improvements)
- **Phase 6**: 2-3 days (cleanup)
- **Phase 7**: Ongoing (enhancements)

**Total Core Migration**: ~3-4 weeks

---

## Notes

- This is an incremental migration - the game should remain playable throughout
- Each phase should be committed separately for easy rollback if needed
- Testing is critical at each phase to catch issues early
- The graph structure opens up many future possibilities beyond just the map
- Consider creating a map editor tool to make territory definition easier

---

## Related Files

- `mapwidget.h/cpp` - Current grid-based map implementation
- `common.h` - Position struct and shared definitions
- `gamepiece.h/cpp` - Piece placement and movement
- `player.h/cpp` - Territory ownership tracking
- Future: `mapgraph.h/cpp` - New graph-based map system
- Future: `maps/roman_empire.json` - Territory definitions
