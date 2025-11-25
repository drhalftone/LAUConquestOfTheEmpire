# Map Integration Plan: Grid to Graph Migration

## Overview

This plan outlines the integration of the new graph-based map system with OpenGL rendering into the main Conquest of the Empire game application.

## Current State

### Existing Grid System (mapwidget.h/cpp)
- Uses 2D arrays: `m_tiles[rows][cols]`, `m_territories[rows][cols]`
- Fixed 7x5 grid with Land/Sea tile types
- Territory info stored per tile: name, value
- Ownership tracked in `m_ownership[rows][cols]`
- Click detection via grid cell calculation

### New Graph System
- `MapGraph` class with `Territory` struct (mapgraph.h/cpp)
- 60 territories defined in CSV: ID, centroid, name, value, neighbors
- Supports pathfinding (BFS), adjacency queries, spatial queries
- JSON serialization for save/load

### New Map Data (images/territories.csv)
```
ID,Area,Color,Centroid X,Centroid Y,Name,Points,Neighbors
1,67094,Red,697.1,214.3,Britannia,10,39
...
```
- 60 territories total
- Land territories: have Points value (5, 10, or 20)
- Sea territories: empty Points field, names start with "Mare" or "Oceanus"
- Neighbors: semicolon-separated territory IDs

### MapViewer Widget (MapViewer/)
- OpenGL-based map rendering
- Uses index image for click detection (pixel color → territory ID)
- Loads territories.csv and territories_index.png

---

## Phase 1: CSV Loader for MapGraph

**Goal:** Load territory data from CSV into the existing MapGraph class.

### Tasks
1. Add `loadFromCSV(const QString &filePath)` method to MapGraph
2. Parse CSV format: ID, Area, Color, Centroid X/Y, Name, Points, Neighbors
3. Determine TerritoryType from Points field (empty = Sea, otherwise Land)
4. Convert neighbor IDs to territory names for adjacency list
5. Store centroid as territory position

### Files to Modify
- `mapgraph.h` - Add loadFromCSV declaration
- `mapgraph.cpp` - Implement CSV parsing

---

## Phase 2: Index Image Integration

**Goal:** Enable click detection using the territory index image.

### Tasks
1. Load `territories_index.png` into MapGraph or MapWidget
2. Add method: `QString getTerritoryAtPixel(int x, int y)`
3. Map pixel color/value to territory ID, then to territory name
4. Handle coordinate scaling between widget size and image size

### Files to Modify
- `mapgraph.h/cpp` or new helper class for index image

---

## Phase 3: OpenGL Map Widget

**Goal:** Port MapViewerWidget rendering to main application.

### Tasks
1. Create `GraphMapWidget` class (or modify MapWidget)
2. Port OpenGL initialization from MapViewer
3. Render map texture with territory overlay
4. Render game pieces at territory centroids
5. Handle click events using index image

### New Files
- `graphmapwidget.h`
- `graphmapwidget.cpp`

### Resources to Add
- `images/Map.jpg` (main map texture)
- `images/territories_index.png` (click detection)
- `images/territories.csv` (territory data)

---

## Phase 4: Game Logic Migration

**Goal:** Replace grid-based game logic with graph-based queries.

### Key Migrations

| Grid Function | Graph Replacement |
|---------------|-------------------|
| `isSeaTerritory(row, col)` | `m_graph->isSeaTerritory(name)` |
| `getTerritoryNameAt(row, col)` | Direct territory name |
| `getAdjacentSeaTerritories(pos)` | `m_graph->getNeighbors(name)` filtered |
| `isValidMove(from, to)` | `m_graph->areAdjacent(from, to)` |
| `m_ownership[row][col]` | `Player::getOwnedTerritories()` |

### Tasks
1. Update Player class to track territories by name instead of Position
2. Migrate piece positions from grid Position to territory name
3. Update movement validation to use graph adjacency
4. Migrate road network logic to use graph pathfinding
5. Update save/load to use territory names

### Files to Modify
- `player.h/cpp` - Territory ownership by name
- `mapwidget.cpp` - Replace grid queries with graph queries
- `purchasedialog.cpp` - Territory selection
- `playerinfowidget.cpp` - Display updates

---

## Phase 5: Remove Grid System

**Goal:** Clean up deprecated grid code.

### Tasks
1. Remove `m_tiles`, `m_territories`, `m_ownership` 2D arrays
2. Remove grid-based helper methods
3. Update all references to use graph system
4. Clean up Position-based code where replaced by territory names

---

## Data Flow

```
territories.csv
      │
      ▼
┌─────────────┐     ┌─────────────────┐
│  MapGraph   │◄────│territories_index│
│ (adjacency, │     │    .png         │
│  centroids) │     │ (click detect)  │
└─────────────┘     └─────────────────┘
      │                     │
      ▼                     ▼
┌─────────────────────────────────────┐
│         GraphMapWidget              │
│  - OpenGL rendering (Map.jpg)       │
│  - Piece rendering at centroids     │
│  - Click → pixel → territory ID     │
└─────────────────────────────────────┘
```

---

## Territory Type Identification

From CSV analysis:
- **Sea territories** (15 total): Empty Points field
  - Names: Mare Pontus Euxinus, Mare Adriatico, Mare Alexandria, Mare Sabrata, Mare Tyrrhenum, Mare Rhodus, Mare Hispalis, Oceanus Britannicus, Mare Amisus, Mare Epirus, Mare Numidia, Mare Pelusium, Oceanus Atlanticus, Mare Aegaeum, Mare Baliaricum, Mare Cantabricum, Mare Narbo, Mare Libycum

- **Land territories** (42 total): Have Points value (5, 10, or 20)
  - Value 5: Smaller provinces
  - Value 10: Medium provinces
  - Value 20: Syria (capital region)

---

## Testing Strategy

1. **Unit tests:** MapGraph CSV loading, adjacency validation
2. **Visual tests:** Compare MapViewer rendering with game integration
3. **Click tests:** Verify all 60 territories respond to clicks
4. **Game tests:** Full game with movement, combat, scoring

---

## Migration Order

1. Add CSV loader to MapGraph (non-breaking)
2. Add GraphMapWidget alongside MapWidget (parallel development)
3. Add feature flag to switch between widgets
4. Migrate game logic incrementally
5. Remove old grid system when stable

---

## Estimated Scope

- Phase 1: ~100 lines (CSV loader)
- Phase 2: ~50 lines (index image)
- Phase 3: ~500 lines (OpenGL widget port)
- Phase 4: ~300 lines (game logic updates)
- Phase 5: ~-200 lines (cleanup/removal)
