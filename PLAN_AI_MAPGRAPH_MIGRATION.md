# Plan: Migrate AIPlayer to Use MapGraph Instead of MapWidget

## Goal
Update AIPlayer to rely solely on MapGraph for map topology queries, making it compatible with both OpenGL map (GameMapWidget) and grid-based map (MapWidget).

## Current State Analysis

### AIPlayer's Current MapWidget Dependencies (aiplayer.cpp)

| Line | Usage | Purpose |
|------|-------|---------|
| 366-376 | `m_mapWidget->rows()`, `cols()`, `getTerritoryOwnerAt()`, `getTerritoryNameAt()` | Find enemy territories via grid iteration |
| 692-696 | `m_mapWidget->territoryNameToPosition()`, `getTerritoryValueAt()` | Get territory value in `scoreExpandMove()` |

### What MapGraph Already Provides

| Method | Replaces |
|--------|----------|
| `getTerritoryNames()` | Grid iteration (`rows()`/`cols()`) |
| `getValue(name)` | `getTerritoryValueAt(row, col)` |
| `isSeaTerritory(name)` | `isSeaTerritory(row, col)` |
| `getTerritory(name)` | Multiple lookups combined |
| `getNeighbors(name)` | Already used via PlayerInfoWidget |

### Territory Ownership (Not in MapGraph - Game State)

Territory ownership is tracked in `Player` objects:
- `Player::ownsTerritory(name)` - check if player owns territory
- `Player::getOwnedTerritories()` - get list of owned territories

This is correct - ownership is game state, not map topology.

---

## Implementation Plan

### Step 1: Add MapGraph Pointer to AIPlayer

**File: aiplayer.h**

Add a `MapGraph*` member alongside (or replacing) `MapWidget*`:

```cpp
// Option A: Add alongside (for transition)
MapGraph *m_mapGraph;

// Option B: Replace entirely (cleaner)
// Remove: MapWidget *m_mapWidget;
// Add: MapGraph *m_mapGraph;
```

**Recommendation**: Option A initially for safer transition, then remove MapWidget reference once verified working.

Update constructor signature:
```cpp
explicit AIPlayer(Player *player,
                  PlayerInfoWidget *infoWidget,
                  MapGraph *mapGraph,  // NEW: Direct graph reference
                  QObject *parent = nullptr);
```

### Step 2: Update Enemy Territory Detection (lines 364-377)

**Current code** (grid-based iteration):
```cpp
if (m_mapWidget) {
    for (int row = 0; row < m_mapWidget->rows(); ++row) {
        for (int col = 0; col < m_mapWidget->cols(); ++col) {
            QChar owner = m_mapWidget->getTerritoryOwnerAt(row, col);
            if (owner != '\0' && owner != playerId) {
                QString territoryName = m_mapWidget->getTerritoryNameAt(row, col);
                if (!state.enemyTerritories.contains(territoryName)) {
                    state.enemyTerritories.append(territoryName);
                }
            }
        }
    }
}
```

**New code** (graph + player-based):
```cpp
if (m_mapGraph) {
    QList<QString> allTerritories = m_mapGraph->getTerritoryNames();
    for (const QString &territoryName : allTerritories) {
        // Skip sea territories (no ownership)
        if (m_mapGraph->isSeaTerritory(territoryName)) {
            continue;
        }
        // Check each player for ownership
        for (Player *p : m_infoWidget->getPlayers()) {
            if (p->getId() != playerId && p->ownsTerritory(territoryName)) {
                state.enemyTerritories.append(territoryName);
                break;
            }
        }
    }
}
```

### Step 3: Update scoreExpandMove() (lines 691-700)

**Current code**:
```cpp
int AIPlayer::scoreExpandMove(const QString &target, const GameState &state)
{
    Q_UNUSED(state)
    int score = 100;

    if (m_mapWidget) {
        Position pos = m_mapWidget->territoryNameToPosition(target);
        if (pos.row >= 0) {
            int value = m_mapWidget->getTerritoryValueAt(pos.row, pos.col);
            score += value;
        }
    }
    return score;
}
```

**New code**:
```cpp
int AIPlayer::scoreExpandMove(const QString &target, const GameState &state)
{
    Q_UNUSED(state)
    int score = 100;

    if (m_mapGraph) {
        int value = m_mapGraph->getValue(target);
        score += value;  // 0 for sea, 5/10/20 for land
    }
    return score;
}
```

### Step 4: Update Constructor and Initialization

**File: aiplayer.cpp**

```cpp
AIPlayer::AIPlayer(Player *player,
                   PlayerInfoWidget *infoWidget,
                   MapGraph *mapGraph,
                   QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_infoWidget(infoWidget)
    , m_mapGraph(mapGraph)
    , m_actionTimer(new QTimer(this))
{
    // ... existing code ...
}
```

### Step 5: Update Call Sites in main.cpp

**Current** (main.cpp):
```cpp
AIPlayer *aiPlayer = new AIPlayer(player, infoWidget, mapWidget, ...);
```

**New**:
```cpp
AIPlayer *aiPlayer = new AIPlayer(player, infoWidget, mapWidget->getGraph(), ...);
```

Both `GameMapWidget` and `MapWidget` already have `getGraph()` methods, so this works with either.

### Step 6: Remove MapWidget Include from aiplayer.cpp

**Current**:
```cpp
#ifdef USE_OPENGL_MAP
#include "gamemapwidget.h"
#else
#include "mapwidget.h"
#endif
```

**New**:
```cpp
#include "mapgraph.h"
```

### Step 7: Update aiplayer.h

Remove conditional MapWidget type alias dependency:
```cpp
// Remove: #include "common.h"  // For MapWidget type alias
// Add:
class MapGraph;  // Forward declaration
```

---

## Additional Considerations

### PlayerInfoWidget::getMovesForLeader()

This method (used by AI at line 549) already uses MapGraph for most operations but still has some grid-based code:
- Line 4938: `m_mapWidget->getTerritoryNameAt(toPos.row, toPos.col)` for road handling
- Line 4987-4988: `territoryNameToPosition()` and `getTroopInfoAt()` for troop info

**Recommendation**: These can be addressed in a separate refactoring pass. The Road class stores `toPosition` as row/col, which would need migration to territory names. This is outside the scope of AIPlayer changes but noted for completeness.

### Backward Compatibility

The changes are backward compatible because:
1. `MapGraph` is the same object regardless of which map widget is used
2. Both `GameMapWidget::getGraph()` and `MapWidget::getGraph()` return the same `MapGraph*`
3. Territory ownership queries go through `Player` objects, not the map widget

### Testing Checklist

- [ ] AI can detect enemy territories correctly
- [ ] AI scores expand moves correctly (value-based)
- [ ] AI movement decisions work with both map types
- [ ] No runtime errors when switching between map types
- [ ] AI performance is not degraded (graph queries are O(1) hash lookups)

---

## Summary of Changes

| File | Changes |
|------|---------|
| aiplayer.h | Add `MapGraph*` member, update constructor signature, remove MapWidget dependency |
| aiplayer.cpp | Replace grid iteration with graph queries, use `Player::ownsTerritory()` for ownership |
| main.cpp | Pass `mapWidget->getGraph()` instead of `mapWidget` to AIPlayer constructor |

**Estimated scope**: ~30-40 lines of code changes across 3 files.
