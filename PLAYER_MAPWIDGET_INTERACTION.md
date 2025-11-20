# Player-MapWidget Interaction Summary

## Overview

The **Player** class and **MapWidget** class have a **bidirectional relationship** where MapWidget maintains a reference to all Player objects and queries them for game state, while Players own the actual game pieces and territory ownership data.

## Architecture: Separation of Concerns

### MapWidget Responsibilities:
- **Map structure** (grid/graph of territories)
- **Territory properties** (name, type Land/Sea, tax value)
- **Rendering** (drawing map, pieces, buildings)
- **User interaction** (mouse clicks, drag-and-drop)
- **Game flow coordination** (turn progression, combat triggers)

### Player Responsibilities:
- **Piece ownership** (Caesar, Generals, Infantry, Cavalry, Catapults, Galleys)
- **Building ownership** (Cities, Roads, Fortifications)
- **Territory ownership** (list of claimed territories by name)
- **Economic data** (wallet/money)
- **Turn state** (is it my turn?)

---

## How MapWidget References Players

### 1. Player List Storage

```cpp
// In mapwidget.h
QList<Player*> m_players;  // Reference to all 6 players (A-F)
int m_currentPlayerIndex;  // Which player's turn it is
```

### 2. Setting Players (from MainWindow)

```cpp
// MainWindow creates players and injects them into MapWidget
void MapWidget::setPlayers(const QList<Player*> &players)
{
    m_players = players;
}
```

**When:** Called during game initialization after MapWidget is created.

---

## Key Interaction Patterns

### Pattern 1: Territory Ownership Query

**Question:** Who owns territory at position (row, col)?

```cpp
QChar MapWidget::getTerritoryOwnerAt(int row, int col) const
{
    QString territoryName = m_territories[row][col].name;

    // Ask each player if they own this territory
    for (Player *player : m_players) {
        if (player->ownsTerritory(territoryName)) {
            return player->getId();  // 'A', 'B', 'C', 'D', 'E', or 'F'
        }
    }

    return '\0';  // Unowned
}
```

**Flow:**
1. MapWidget converts grid position → territory name
2. MapWidget iterates through all players
3. Calls `player->ownsTerritory(territoryName)` on each
4. Player checks its internal list: `m_ownedTerritories`
5. Returns first match or '\0' if unowned

**Used for:**
- Drawing ownership borders (thick colored borders around owned territories)
- Determining who collects taxes
- Checking control for victory conditions

---

### Pattern 2: Piece Location Query

**Question:** Are there enemy pieces at position (row, col)?

```cpp
bool MapWidget::hasEnemyPiecesAt(int row, int col, QChar currentPlayer) const
{
    Position pos = {row, col};

    // Check all players except current player
    for (Player *player : m_players) {
        if (player->getId() == currentPlayer) {
            continue;  // Skip current player
        }

        // Ask player for pieces at this position
        QList<GamePiece*> pieces = player->getPiecesAtPosition(pos);
        if (!pieces.isEmpty()) {
            return true;  // Found enemy pieces
        }
    }

    return false;
}
```

**Flow:**
1. MapWidget creates Position from row/col
2. Iterates through all players except current player
3. Calls `player->getPiecesAtPosition(pos)` on each
4. Player searches its piece inventories (Caesars, Generals, Infantry, etc.)
5. Returns true if any enemy has pieces there

**Used for:**
- Combat detection (disputed territories)
- Movement restrictions
- Drawing combat indicators (red X on map)

---

### Pattern 3: Rendering Pieces (Indirect)

**Question:** What pieces should be drawn on the map?

```cpp
// In paintEvent()
for (Player *player : m_players) {
    // Get all pieces from player
    QList<GamePiece*> allPieces = player->getAllPieces();

    for (GamePiece *piece : allPieces) {
        Position pos = piece->getPosition();
        QColor color = player->getColor();

        // Draw piece at position with player color
        drawPieceOnMap(piece, pos, color);
    }
}
```

**Flow:**
1. MapWidget iterates through all players
2. Calls `player->getAllPieces()` to get complete inventory
3. For each piece, gets position and draws it
4. Uses player color for rendering

**Note:** This is **indirect** - MapWidget asks "what pieces do you have?" and draws them, rather than maintaining its own piece positions.

---

### Pattern 4: Tax Collection

**Question:** How much tax does each player collect?

```cpp
// In Player::collectTaxes(MapWidget *mapWidget)
int Player::collectTaxes(MapWidget *mapWidget)
{
    int totalTax = 0;

    for (const QString &territoryName : m_ownedTerritories) {
        // Convert territory name to position
        Position pos = mapWidget->territoryNameToPosition(territoryName);

        // Ask MapWidget for tax value
        int taxValue = mapWidget->getTerritoryValueAt(pos.row, pos.col);

        totalTax += taxValue;
    }

    // Add to player's wallet
    addMoney(totalTax);

    return totalTax;
}
```

**Flow:**
1. **Player** initiates: "I want to collect taxes"
2. Player has list of owned territories (by name)
3. Player asks MapWidget: "What's the tax value of territory X?"
4. MapWidget looks up value from `m_territories[row][col].value`
5. Player adds money to wallet

**This is REVERSE of typical pattern:** Player queries MapWidget!

---

### Pattern 5: Movement Validation

**Question:** Can a piece move from Position A to Position B?

```cpp
bool MapWidget::isValidMove(const Position &from, const Position &to) const
{
    // 1. Check adjacency (grid or graph)
    // 2. Check movement range
    // 3. Check terrain type (land pieces can't enter sea without galleys)
    // 4. Check if destination is blocked

    // Query current player
    Player *currentPlayer = m_players[m_currentPlayerIndex];

    // Check if player has pieces at destination
    QList<GamePiece*> destinationPieces =
        currentPlayer->getPiecesAtPosition(to);

    // (validation logic)
}
```

---

## Deprecated Patterns (Old System)

These are **no longer used** but still exist in the code:

### Old: Direct Ownership Grid
```cpp
// DEPRECATED - don't use
QVector<QVector<QChar>> m_ownership;  // m_ownership[row][col] = 'A'
```

**Problem:** Duplicates data - both MapWidget and Player tracked ownership.

### Old: Direct Piece Storage
```cpp
// DEPRECATED - don't use
QMap<QChar, QVector<Piece>> m_playerPieces;  // Only Caesar + Generals
```

**Problem:** Limited to 6 pieces per player, didn't include troops.

**Current approach:** MapWidget queries Player objects as "source of truth".

---

## Data Flow Examples

### Example 1: Clicking on a Territory

```
User clicks at pixel (500, 300)
    ↓
MapWidget::mousePressEvent()
    ↓
Calculate grid position: row=3, col=5
    ↓
Get territory name: "T_3_5"
    ↓
Query all players: who owns "T_3_5"?
    ↓
Player A: ownsTerritory("T_3_5") → false
Player B: ownsTerritory("T_3_5") → true ✓
    ↓
Return 'B'
    ↓
Show territory info: "Owned by Player B"
```

### Example 2: End of Turn Tax Collection

```
User clicks "End Turn" button
    ↓
MainWindow::endCurrentPlayerTurn()
    ↓
Player A (current player)
    ↓
Player A: "I want to collect taxes"
    ↓
Player A: "I own: ['T_2_3', 'T_2_4', 'T_3_3']"
    ↓
For each territory:
    Player A asks MapWidget: "What's the value of T_2_3?"
    MapWidget: "Tax value = 10"
    Player A asks MapWidget: "What's the value of T_2_4?"
    MapWidget: "Tax value = 5"
    Player A asks MapWidget: "What's the value of T_3_3?"
    MapWidget: "Tax value = 10"
    ↓
Player A: totalTax = 25
Player A: addMoney(25)
Player A: m_wallet = 75
    ↓
Emit signal: walletChanged(75)
    ↓
MainWindow updates wallet display
```

### Example 3: Combat Detection

```
MapWidget::paintEvent() - Drawing the map
    ↓
For position (4, 7):
    ↓
Check: Are there pieces from multiple players?
    ↓
Ask Player A: getPiecesAtPosition({4,7}) → [Infantry, Infantry]
Ask Player B: getPiecesAtPosition({4,7}) → []
Ask Player C: getPiecesAtPosition({4,7}) → [Cavalry, General]
Ask Player D: getPiecesAtPosition({4,7}) → []
Ask Player E: getPiecesAtPosition({4,7}) → []
Ask Player F: getPiecesAtPosition({4,7}) → []
    ↓
Result: Players A and C both have pieces here
    ↓
Territory is DISPUTED!
    ↓
Draw red X pattern to indicate combat
```

---

## Summary: Who Owns What?

| Data Type | Owned By | Queried By |
|-----------|----------|------------|
| **Territory structure** (grid, names, types) | MapWidget | Player (for tax collection) |
| **Territory values** (tax amounts) | MapWidget | Player (for tax collection) |
| **Territory ownership** (who controls what) | Player | MapWidget (for rendering borders) |
| **Pieces** (units on map) | Player | MapWidget (for rendering, combat) |
| **Buildings** (cities, roads) | Player | MapWidget (for rendering) |
| **Money/Wallet** | Player | MainWindow (for display) |
| **Turn state** | Player | MapWidget (for move validation) |

---

## Key Takeaways

1. **Players are the source of truth** for ownership, pieces, and money
2. **MapWidget is the source of truth** for map structure and territory properties
3. **Queries flow both ways:**
   - MapWidget asks Player: "Do you own this? Do you have pieces here?"
   - Player asks MapWidget: "What's the tax value of this territory?"
4. **No data duplication** - each piece of data has one owner
5. **Late binding** - MapWidget doesn't create players, they're injected via `setPlayers()`
6. **Iteration pattern** - Most queries loop through `m_players` list

This design allows:
- ✓ Clean separation of responsibilities
- ✓ Easy to add new players
- ✓ Players can exist independently of map
- ✓ Easy to save/load game state (serialize Players separately)
- ✓ Testable (can test Player logic without MapWidget)

---

## Future: Graph Migration Impact

When we migrate to graph-based territories in Phase 3+:

**What changes:**
- Territory identification: Position{row,col} → QString territoryName
- Piece positions: `piece->getPosition()` → `piece->getTerritory()`
- Ownership queries: `ownsTerritory(name)` already uses QString ✓

**What stays the same:**
- Player still owns pieces and territories
- MapWidget still queries Player for ownership
- Data flow patterns remain identical
- Just the coordinate system changes (grid → graph)

The architecture is already graph-ready! 🎉
