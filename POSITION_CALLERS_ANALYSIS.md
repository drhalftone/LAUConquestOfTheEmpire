# External Callers of Position-Based Player Queries

## Summary

**Total external calls to Position-based queries: 25**

### Breakdown by File:

| File | Function | Count |
|------|----------|-------|
| **combatdialog.cpp** | `getPiecesAtPosition()` | 3 |
| | `getCityAtPosition()` | 2 |
| **mapwidget.cpp** | `getPiecesAtPosition()` | 4 |
| | `getCityAtPosition()` | 2 |
| | `getPiecesAtPosition()` (own method) | 1 |
| **playerinfowidget.cpp** | `getPiecesAtPosition()` | 14 |
| **TOTAL** | | **25** |

---

## Detailed Analysis

### 1. combatdialog.cpp (5 occurrences)

**Lines 25-26: Combat initialization**
```cpp
m_attackingPieces = m_attackingPlayer->getPiecesAtPosition(combatPosition);
m_defendingPieces = m_defendingPlayer->getPiecesAtPosition(combatPosition);
```

**Line 343: Get all pieces at combat location**
```cpp
QList<GamePiece*> allPieces = owningPlayer->getPiecesAtPosition(m_combatPosition);
```

**Lines 661, 998: Check for city at combat location**
```cpp
City *city = m_defendingPlayer->getCityAtPosition(m_combatPosition);
```

**Migration:**
- CombatDialog has `Position m_combatPosition`
- Need to also store `QString m_combatTerritoryName`
- Or convert Position → territory name at dialog creation

---

### 2. mapwidget.cpp (7 occurrences)

**Line 179: Check for pieces in paintEvent**
```cpp
for (Player *player : m_players) {
    Position pos = {row, col};
    QList<GamePiece*> pieces = player->getPiecesAtPosition(pos);
}
```

**Line 237: Check for city in paintEvent**
```cpp
City *city = player->getCityAtPosition(pos);
```

**Line 768: DEPRECATED method call**
```cpp
QVector<Piece*> piecesAtPos = getPiecesAtPosition(piece.position, player);
```
Note: This calls MapWidget's own deprecated method!

**Line 802: MapWidget's own method**
```cpp
QVector<MapWidget::Piece*> MapWidget::getPiecesAtPosition(const Position &pos, QChar player)
```
This is MapWidget's OLD method (uses deprecated Piece struct)

**Lines 872, 886, 1178: Various lookups**
```cpp
City *city = player->getCityAtPosition(pos);
QList<GamePiece*> piecesHere = player->getPiecesAtPosition(pos);
QList<GamePiece*> pieces = player->getPiecesAtPosition(pos);
```

**Migration:**
- MapWidget iterates through grid positions
- Need to iterate through territory names instead
- Can use `m_graph->getTerritoryNames()` for iteration
- Convert grid position → territory name for lookups

---

### 3. playerinfowidget.cpp (14 occurrences!)

**Lines 1085, 1167, 1269, 1503: Enemy piece checks during movement**
```cpp
QList<GamePiece*> enemyPieces = otherPlayer->getPiecesAtPosition(newPos);
QList<GamePiece*> enemyPieces = otherPlayer->getPiecesAtPosition(newPos);
QList<GamePiece*> enemyPieces = player->getPiecesAtPosition(destPos);
QList<GamePiece*> enemyPieces = player->getPiecesAtPosition(destPos);
```

**Lines 1228, 1464: Get all pieces at current position**
```cpp
QList<GamePiece*> allPiecesAtPosition = owningPlayer->getPiecesAtPosition(currentPos);
QList<GamePiece*> allPiecesAtPosition = owningPlayer->getPiecesAtPosition(currentPos);
```

**Lines 1719: Check pieces during action**
```cpp
QList<GamePiece*> piecesHere = player->getPiecesAtPosition(pos);
```

**Lines 1868, 1877, 1906, 1913, 1947: Various combat/movement checks**
```cpp
QList<GamePiece*> currentPlayerPieces = currentPlayer->getPiecesAtPosition(pos);
QList<GamePiece*> enemyPieces = player->getPiecesAtPosition(pos);
// ... (multiple similar calls)
```

**Migration:**
- PlayerInfoWidget has Position variables for current/destination
- Need to track territory names instead of Positions
- Most calls are checking pieces at destination for combat/movement

---

## Migration Strategy for Each File

### CombatDialog Migration

**Current State:**
```cpp
class CombatDialog {
    Position m_combatPosition;

    // Constructor
    CombatDialog(..., const Position &combatPosition, ...)
        : m_combatPosition(combatPosition)
    {
        m_attackingPieces = m_attackingPlayer->getPiecesAtPosition(combatPosition);
    }
};
```

**Migrated State:**
```cpp
class CombatDialog {
    QString m_combatTerritoryName;

    // Constructor
    CombatDialog(..., const QString &territoryName, ...)
        : m_combatTerritoryName(territoryName)
    {
        m_attackingPieces = m_attackingPlayer->getPiecesAtTerritory(territoryName);
    }
};
```

**Changes needed:**
- Change constructor parameter: `Position` → `QString`
- Change member variable: `m_combatPosition` → `m_combatTerritoryName`
- Change all 5 calls: `getPiecesAtPosition()` → `getPiecesAtTerritory()`
- Update whoever creates CombatDialog to pass territory name

---

### MapWidget Migration

**Current State:**
```cpp
// In paintEvent() - iterate through grid
for (int row = 0; row < ROWS; ++row) {
    for (int col = 0; col < COLUMNS; ++col) {
        Position pos = {row, col};
        for (Player *player : m_players) {
            QList<GamePiece*> pieces = player->getPiecesAtPosition(pos);
        }
    }
}
```

**Migrated State:**
```cpp
// Iterate through territories from graph
QList<QString> territoryNames = m_graph->getTerritoryNames();
for (const QString &territoryName : territoryNames) {
    for (Player *player : m_players) {
        QList<GamePiece*> pieces = player->getPiecesAtTerritory(territoryName);
    }
}
```

**Changes needed:**
- Replace grid iteration with territory name iteration
- Replace `getPiecesAtPosition()` → `getPiecesAtTerritory()`
- Replace `getCityAtPosition()` → `getCityAtTerritory()`
- Remove MapWidget's own deprecated `getPiecesAtPosition()` method

---

### PlayerInfoWidget Migration

**Current State:**
```cpp
// Movement validation
Position newPos = getDestinationPosition();
QList<GamePiece*> enemyPieces = otherPlayer->getPiecesAtPosition(newPos);
```

**Migrated State:**
```cpp
// Movement validation
QString destTerritory = getDestinationTerritory();
QList<GamePiece*> enemyPieces = otherPlayer->getPiecesAtTerritory(destTerritory);
```

**Changes needed:**
- Identify all Position variables used for lookups
- Replace with territory name variables
- Change all 14 calls: `getPiecesAtPosition()` → `getPiecesAtTerritory()`
- Update movement logic to work with territory names

---

## Function Call Replacement Pattern

### Search and Replace Guide

**Pattern 1: Simple replacement**
```cpp
// FIND
player->getPiecesAtPosition(pos)

// REPLACE WITH
player->getPiecesAtTerritory(territoryName)
```

**Pattern 2: With Position variable**
```cpp
// FIND
Position pos = {row, col};
QList<GamePiece*> pieces = player->getPiecesAtPosition(pos);

// REPLACE WITH
QString territoryName = mapWidget->positionToTerritoryName({row, col});
QList<GamePiece*> pieces = player->getPiecesAtTerritory(territoryName);
```

**Pattern 3: City lookup**
```cpp
// FIND
City *city = player->getCityAtPosition(pos);

// REPLACE WITH
City *city = player->getCityAtTerritory(territoryName);
```

---

## Migration Order Recommendation

### Phase 1: Add Territory Name Alongside Position
For each file, add territory name storage alongside Position (dual system):

1. **CombatDialog**: Add `QString m_combatTerritoryName`
2. **PlayerInfoWidget**: Track territory names for source/destination
3. **MapWidget**: Already has converters available

### Phase 2: Use Territory-Based Queries
Replace all Position-based calls with territory-based calls:

1. Change function calls one by one
2. Test after each change
3. Use existing territory name if available

### Phase 3: Remove Position Variables
Once all calls are migrated:

1. Remove Position member variables
2. Remove Position parameters from constructors
3. Remove Position-based query methods from Player

---

## Dependency Chain

**To remove Position from Player, we need:**

1. ✅ Player has territory-based query methods (already exists!)
2. 🔴 CombatDialog uses territory names instead of Position
3. 🔴 MapWidget uses territory names for iteration
4. 🔴 PlayerInfoWidget uses territory names for movement
5. 🔴 GamePiece stores only territory name (remove Position)
6. 🔴 Building stores only territory name (remove Position)

**Can't remove Position from Player until all callers are updated.**

---

## Testing Strategy

### For Each File:

1. **Before migration:**
   - Run game and test all features
   - Document current behavior

2. **During migration:**
   - Change one call at a time
   - Compile and test after each change
   - Use verbose test mode

3. **After migration:**
   - Test combat (CombatDialog)
   - Test piece movement (PlayerInfoWidget)
   - Test map rendering (MapWidget)
   - Run comprehensive test suite

---

## Estimated Effort

| File | Calls | Complexity | Time |
|------|-------|------------|------|
| combatdialog.cpp | 5 | Medium | 1-2 hours |
| mapwidget.cpp | 7 | High | 2-3 hours |
| playerinfowidget.cpp | 14 | High | 3-4 hours |
| **TOTAL** | **25** | | **6-9 hours** |

**Complexity factors:**
- MapWidget: Need to change iteration strategy (grid → territories)
- PlayerInfoWidget: Many calls scattered throughout movement logic
- CombatDialog: Need to change constructor signature

---

## Risk Assessment

### Low Risk: ✅
- Territory-based methods already exist and tested
- Dual system possible (keep Position temporarily)

### Medium Risk: 🟡
- Many call sites (25 total)
- Need to track territory names through code paths
- Testing required for each change

### High Risk: 🔴
- MapWidget iteration change (grid → graph) is fundamental
- PlayerInfoWidget has complex movement logic
- Any missed call will cause runtime errors

**Mitigation:**
- Make changes incrementally
- Test frequently
- Keep Position methods until all callers migrated
- Use compiler errors to find missed calls

---

## Next Steps

1. **Review this analysis** with team
2. **Choose migration approach:**
   - Option A: All at once (risky but fast)
   - Option B: Incrementally (safer, slower)
3. **Start with CombatDialog** (smallest, most isolated)
4. **Then PlayerInfoWidget** (most calls, need careful testing)
5. **Finally MapWidget** (most fundamental change)
6. **Remove Position from Player** (last step)
