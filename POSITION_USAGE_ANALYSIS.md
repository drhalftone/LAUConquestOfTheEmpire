# Position Usage Analysis: Grid to Territory Name Migration

## Overview

This document catalogs all uses of `Position{row, col}` in the codebase and provides a migration strategy to replace grid-based indexing with territory name-based lookups.

---

## Current Position Structure

```cpp
// In common.h
struct Position {
    int row;  // 0-7
    int col;  // 0-11
};
```

**Grid Size:** 8 rows × 12 columns = 96 territories

**Current Territory Names:** Format "T_row_col" (e.g., "T_3_5")

---

## Position Usage Categories

### Category 1: GamePiece Storage (✓ Already Dual)

**Status:** ✅ Already has both Position AND territoryName!

```cpp
// In gamepiece.h
class GamePiece {
    Position m_position;           // Grid position - DEPRECATED
    QString m_territoryName;       // Territory name - NEW SYSTEM

    Position getPosition() const;
    void setPosition(const Position &pos);

    QString getTerritoryName() const;
    void setTerritoryName(const QString &name);
};
```

**Migration Strategy:**
- GamePiece ALREADY supports both systems
- Just need to update code to use `getTerritoryName()` instead of `getPosition()`
- Eventually remove Position completely

**Usage Count:** ~50 occurrences across all piece types

---

### Category 2: Building Storage

**Current:**
```cpp
// In building.h
class Building {
    Position m_position;  // Grid position
    QString m_territoryName;  // Territory name
};
```

**Status:** ✅ Already has both Position AND territoryName!

**Migration Strategy:** Same as GamePiece

---

### Category 3: MapWidget Grid Arrays (🔴 Core Migration)

**Current Grid-Based Storage:**

```cpp
// In mapwidget.h - ALL need migration
QVector<QVector<TileType>> m_tiles;              // m_tiles[row][col]
QVector<QVector<TerritoryInfo>> m_territories;   // m_territories[row][col]
QVector<QVector<QChar>> m_ownership;             // DEPRECATED
QVector<QVector<bool>> m_hasCity;                // m_hasCity[row][col]
QVector<QVector<bool>> m_hasFortification;       // m_hasFortification[row][col]

// Troop counts per tile
QMap<QChar, QVector<QVector<TroopCounts>>> m_playerTroops;  // [player][row][col]
```

**Usage Patterns:**

1. **Tile Type Lookups** (~40 occurrences)
   ```cpp
   if (m_tiles[row][col] == TileType::Land) { ... }
   ```

2. **Territory Info Lookups** (~15 occurrences)
   ```cpp
   QString name = m_territories[row][col].name;
   int value = m_territories[row][col].value;
   ```

3. **Ownership Lookups** (~8 occurrences - DEPRECATED)
   ```cpp
   QChar owner = m_ownership[row][col];  // Don't use - query Player instead
   ```

4. **Building Existence** (~12 occurrences)
   ```cpp
   if (m_hasCity[row][col]) { ... }
   if (m_hasFortification[row][col]) { ... }
   ```

5. **Troop Counts** (~6 occurrences)
   ```cpp
   m_playerTroops[player][row][col].infantry = 5;
   ```

---

## Detailed Usage Breakdown

### 1. Player Queries (Already Territory-Based!)

✅ **ALREADY MIGRATED** - Uses territory names!

```cpp
// In player.h - Already uses QString territoryName!
QList<GamePiece*> getPiecesAtTerritory(const QString &territoryName) const;
QList<Building*> getBuildingsAtTerritory(const QString &territoryName) const;
bool ownsTerritory(const QString &territoryName) const;
```

**Also has Position versions for backward compatibility:**
```cpp
QList<GamePiece*> getPiecesAtPosition(const Position &pos) const;
```

**Migration:** These Position versions should eventually be removed.

---

### 2. MapWidget Territory Lookups

**Current (grid-based):**
```cpp
QString MapWidget::getTerritoryNameAt(int row, int col) const
{
    return m_territories[row][col].name;
}

int MapWidget::getTerritoryValueAt(int row, int col) const
{
    return m_territories[row][col].value;
}

bool MapWidget::isSeaTerritory(int row, int col) const
{
    return m_tiles[row][col] == TileType::Sea;
}
```

**Target (name-based):**
```cpp
// Use graph instead
QString MapWidget::getTerritoryValue(const QString &territoryName) const
{
    // Look up in graph or territory map
}

bool MapWidget::isSeaTerritory(const QString &territoryName) const
{
    return m_graph->getType(territoryName) == TerritoryType::Sea;
}
```

---

### 3. Rendering / Pixel Coordinate Conversion

**Current (converts Position to pixels):**
```cpp
// In paintEvent() - lots of this pattern
int x = pos.col * m_tileWidth;
int y = pos.row * m_tileHeight;

// Draw piece at pixel location
painter.drawEllipse(x, y, width, height);
```

**Target (territory name to pixel):**
```cpp
// Get centroid from graph
QPointF centroid = m_graph->getCentroid(territoryName);
int x = centroid.x();
int y = centroid.y();

// Draw piece at centroid
painter.drawEllipse(x, y, width, height);
```

**Count:** ~30 occurrences in paintEvent, drag handlers, etc.

---

### 4. Mouse Click to Territory

**Current (pixel to grid position):**
```cpp
void MapWidget::mousePressEvent(QMouseEvent *event)
{
    int col = event->x() / m_tileWidth;
    int row = (event->y() - menuBarHeight) / m_tileHeight;

    Position clickedPos = {row, col};
    QString territoryName = m_territories[row][col].name;
}
```

**Target (pixel to territory via polygon):**
```cpp
void MapWidget::mousePressEvent(QMouseEvent *event)
{
    QPointF clickPoint(event->x(), event->y());

    // Use graph's polygon detection
    QString territoryName = m_graph->getTerritoryAt(clickPoint);
}
```

**Count:** ~8 occurrences in mouse handlers

---

### 5. Movement Validation

**Current (Position-based):**
```cpp
bool MapWidget::isValidMove(const Position &from, const Position &to) const
{
    // Check if adjacent
    int rowDiff = abs(to.row - from.row);
    int colDiff = abs(to.col - from.col);

    // Check tile type
    if (m_tiles[to.row][to.col] != TileType::Land) {
        return false;
    }
}
```

**Target (Territory-based):**
```cpp
bool MapWidget::isValidMove(const QString &fromTerritory, const QString &toTerritory) const
{
    // Check if adjacent using graph
    if (!m_graph->areAdjacent(fromTerritory, toTerritory)) {
        return false;
    }

    // Check territory type
    if (m_graph->getType(toTerritory) == TerritoryType::Sea) {
        return false;
    }
}
```

**Count:** ~15 occurrences in movement logic

---

### 6. Adjacency / Neighbor Checks

**Current (Grid-based 4-directional):**
```cpp
// Check if territories are adjacent
bool adjacent = (abs(pos1.row - pos2.row) + abs(pos1.col - pos2.col)) == 1;

// Get neighbors
QList<Position> neighbors;
if (row > 0) neighbors.append({row-1, col});
if (row < ROWS-1) neighbors.append({row+1, col});
if (col > 0) neighbors.append({row, col-1});
if (col < COLUMNS-1) neighbors.append({row, col+1});
```

**Target (Graph-based, any number of neighbors):**
```cpp
// Check if territories are adjacent
bool adjacent = m_graph->areAdjacent(territory1, territory2);

// Get neighbors
QList<QString> neighbors = m_graph->getNeighbors(territoryName);
```

**Count:** ~20 occurrences

---

### 7. Building Placement

**Current:**
```cpp
void MapWidget::placeCity(int row, int col)
{
    m_hasCity[row][col] = true;
}

bool MapWidget::hasCityAt(int row, int col)
{
    return m_hasCity[row][col];
}
```

**Target:**
```cpp
void MapWidget::placeCity(const QString &territoryName)
{
    // Store in map: territoryName → has city
    m_cityLocations.insert(territoryName);
}

bool MapWidget::hasCityAt(const QString &territoryName)
{
    return m_cityLocations.contains(territoryName);
}
```

**Alternative (use Player/Building system):**
Query Player objects for buildings instead of MapWidget storing this!

**Count:** ~12 occurrences

---

## Migration Roadmap

### Phase 1: Deprecate Position in GamePiece/Building ✅
**Status:** DONE - both already have territoryName field

### Phase 2: Replace Grid Arrays with Maps (Current Phase 3)

**Replace:**
```cpp
QVector<QVector<X>> m_gridData;  // m_gridData[row][col]
```

**With:**
```cpp
QMap<QString, X> m_territoryData;  // m_territoryData[territoryName]
```

**Specific Changes:**

| Old Grid Array | New Map-Based Storage |
|----------------|----------------------|
| `m_tiles[row][col]` | `m_graph->getType(name)` (already exists!) |
| `m_territories[row][col].name` | territoryName itself |
| `m_territories[row][col].value` | `m_territoryValues[name]` |
| `m_hasCity[row][col]` | `m_cityLocations.contains(name)` or query Players |
| `m_hasFortification[row][col]` | `m_fortificationLocations.contains(name)` or query Players |
| `m_playerTroops[player][row][col]` | `m_troopCounts[player][name]` |

### Phase 3: Update All Function Signatures

**Change function parameters from Position to QString:**

```cpp
// Old signatures
QString getTerritoryNameAt(int row, int col);
int getTerritoryValueAt(int row, int col);
bool isSeaTerritory(int row, int col);
QChar getTerritoryOwnerAt(int row, int col);

// New signatures
int getTerritoryValue(const QString &territoryName);
bool isSeaTerritory(const QString &territoryName);
QChar getTerritoryOwner(const QString &territoryName);
```

**Keep old signatures temporarily for backward compatibility:**
```cpp
// Deprecated - calls new version
int getTerritoryValueAt(int row, int col) const
{
    QString name = positionToTerritoryName({row, col});
    return getTerritoryValue(name);
}
```

### Phase 4: Update Rendering Logic

Replace all:
```cpp
int x = pos.col * m_tileWidth;
int y = pos.row * m_tileHeight;
```

With:
```cpp
QPointF centroid = m_graph->getCentroid(territoryName);
int x = centroid.x();
int y = centroid.y();
```

### Phase 5: Update Movement/Combat Logic

Replace Position-based adjacency with graph queries:
```cpp
// Old
bool adjacent = (abs(p1.row - p2.row) + abs(p1.col - p2.col)) == 1;

// New
bool adjacent = m_graph->areAdjacent(territory1, territory2);
```

### Phase 6: Remove Position Entirely

Once all code uses territory names:
1. Remove `Position m_position` from GamePiece
2. Remove `Position` from Building
3. Remove `positionToTerritoryName()` converter
4. Remove grid arrays from MapWidget
5. Update all remaining Position references

---

## Usage Statistics Summary

| Category | Occurrences | Status |
|----------|-------------|--------|
| GamePiece position storage | ~50 | ✅ Already has territoryName |
| Building position storage | ~10 | ✅ Already has territoryName |
| Grid array indexing (`[row][col]`) | ~150 | 🔴 Needs migration |
| Position field access (`.row`, `.col`) | ~80 | 🔴 Needs migration |
| Pixel coordinate conversion | ~30 | 🔴 Needs migration |
| Adjacency calculations | ~20 | 🔴 Needs migration |
| Mouse click handling | ~8 | 🔴 Needs migration |
| **TOTAL** | **~350** | **~60 done, ~290 to migrate** |

---

## Key Insights

1. **GamePiece and Building are ready!** They already store both Position and territoryName.

2. **Player class already uses territory names** for ownership and piece queries.

3. **Main work is in MapWidget** - replacing grid arrays with maps/graph queries.

4. **Graph already has most needed functionality:**
   - `getType()` replaces `m_tiles[row][col]`
   - `getNeighbors()` replaces manual adjacency checks
   - `areAdjacent()` replaces distance calculations
   - `getCentroid()` replaces pixel coordinate math

5. **Biggest changes:**
   - Function signatures (Position → QString)
   - Rendering logic (grid math → centroid lookup)
   - Storage (2D arrays → QMap<QString, X>)

6. **Backward compatibility approach:**
   - Keep old signatures calling new ones via converters
   - Gradually migrate callers
   - Remove deprecated code in final phase

---

## Example Migration: One Function

**Before (Position-based):**
```cpp
int MapWidget::getTerritoryValueAt(int row, int col) const
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLUMNS) {
        return 0;
    }
    return m_territories[row][col].value;
}
```

**After (Territory-based):**
```cpp
int MapWidget::getTerritoryValue(const QString &territoryName) const
{
    if (!m_territoryValues.contains(territoryName)) {
        return 0;
    }
    return m_territoryValues[territoryName];
}

// Deprecated wrapper for backward compatibility
int MapWidget::getTerritoryValueAt(int row, int col) const
{
    QString name = positionToTerritoryName({row, col});
    return getTerritoryValue(name);
}
```

---

## Timeline Estimate

- **Phase 2** (Replace grid arrays): 2-3 days
- **Phase 3** (Update signatures): 3-4 days
- **Phase 4** (Update rendering): 2-3 days
- **Phase 5** (Update movement): 2-3 days
- **Phase 6** (Remove Position): 1-2 days

**Total:** ~2-3 weeks (matches Phase 3 estimate in migration plan)

---

## Risk Mitigation

1. **Keep converters active** during migration
2. **Test at each step** - run test suite after each function migrated
3. **Use verbose test mode** to verify all territories work
4. **Commit frequently** - one function category at a time
5. **Maintain backward compatibility** until all code migrated

This migration will enable:
- ✅ Irregularly-shaped territories
- ✅ Variable neighbor counts (not just 4)
- ✅ Realistic geography
- ✅ Polygon-based click detection
- ✅ Proper pathfinding
- ✅ Cleaner, more maintainable code
