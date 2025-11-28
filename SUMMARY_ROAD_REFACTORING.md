# Summary: Road Management Refactoring

## Current Road Implementation

### Road Class (building.h:69-86)
```cpp
class Road : public Building {
    Position m_toPosition;  // Second endpoint (first is inherited m_position)
    // Methods: getFromPosition(), getToPosition(), setToPosition(), paint()
};
```

### How Roads Are Currently Managed

#### 1. Storage
- Roads are stored in `Player::m_roads` as `QList<Road*>`
- Each player owns their roads independently

#### 2. Generation (`MapWidget::updateRoads()` - lines 2053-2160)
Roads are auto-generated when:
- Two cities are owned by the same player
- The cities are in **adjacent territories** (horizontal/vertical neighbors only)
- Both territories are land (not sea)
- Both territories are still owned by the player

The function:
1. First removes invalid roads (lost territory, lost city)
2. Then creates new roads between newly-adjacent city pairs
3. Called at: game start, end of turn, after territory changes

#### 3. Usage - Movement (playerinfowidget.cpp)

**BFS through road network** (lines 1120-1161, 4923-4955):
```cpp
// Iterate through player's roads
for (Road *road : player->getRoads()) {
    QString territory1 = road->getTerritoryName();
    Position toPos = road->getToPosition();
    QString territory2 = m_mapWidget->getTerritoryNameAt(toPos.row, toPos.col);
    // ... BFS to find all road-connected territories
}
```

Roads allow leaders to move through multiple territories for **1 movement point** (the entire road network costs 1 move).

#### 4. Usage - Rendering

**Grid MapWidget** (mapwidget.cpp:324-330):
```cpp
for (Road *road : player->getRoads()) {
    Position from = road->getFromPosition();
    Position to = road->getToPosition();
    // Draw line between tile centers
}
```

**OpenGL GameMapWidget**: Does NOT render roads (no road-related code found).

#### 5. Road Destruction

Roads are destroyed when:
- A city is destroyed (playerinfowidget.cpp:4313-4330)
- A territory is conquered (playerinfowidget.cpp:3615-3651, combatdialog.cpp:1631-1644)
- Player is eliminated (combatdialog.cpp:1437-1441 transfers roads)

---

## Problems with Current Implementation

1. **Redundant State**: Roads are derivable from city positions + ownership - storing them duplicates state
2. **Row/Col Dependencies**: Road endpoints use grid positions, not territory names
3. **Sync Issues**: Must call `updateRoads()` after every territory/city change
4. **Memory Management**: Road objects must be created/deleted, potential for leaks
5. **OpenGL Incompatibility**: OpenGL map doesn't use Position-based rendering

---

## Proposed Solution: On-the-Fly Road Computation

### Core Insight
A road exists between two territories if and only if:
1. Same player owns both territories
2. Same player has cities in both territories
3. Territories are adjacent (neighbors in MapGraph)
4. Both territories are land

This can be computed on-demand using only:
- `Player::getCities()` - list of cities
- `Player::ownsTerritory(name)` - ownership check
- `MapGraph::areAdjacent(t1, t2)` - adjacency check
- `MapGraph::isLandTerritory(name)` - land check

### New MapGraph Method

```cpp
// Returns all territories reachable via roads from startTerritory for given player
QStringList MapGraph::getRoadConnectedTerritories(
    const QString &startTerritory,
    const Player *player) const
{
    QStringList result;
    QSet<QString> visited;
    QList<QString> toVisit;

    visited.insert(startTerritory);
    toVisit.append(startTerritory);

    // Get all city territory names for this player
    QSet<QString> cityTerritories;
    for (City *city : player->getCities()) {
        cityTerritories.insert(city->getTerritoryName());
    }

    // BFS through road network
    while (!toVisit.isEmpty()) {
        QString current = toVisit.takeFirst();

        // Skip if no city here
        if (!cityTerritories.contains(current)) {
            continue;
        }

        // Check all neighbors
        for (const QString &neighbor : getNeighbors(current)) {
            if (visited.contains(neighbor)) continue;
            if (!isLandTerritory(neighbor)) continue;
            if (!player->ownsTerritory(neighbor)) continue;
            if (!cityTerritories.contains(neighbor)) continue;

            // Valid road connection found
            visited.insert(neighbor);
            toVisit.append(neighbor);
            result.append(neighbor);
        }
    }

    return result;
}
```

### Alternative: Get Road Segments for Rendering

```cpp
// Returns list of territory pairs connected by roads for rendering
QList<QPair<QString, QString>> MapGraph::getRoadSegments(const Player *player) const
{
    QList<QPair<QString, QString>> segments;
    QSet<QString> processedPairs;

    // Get city territories
    QSet<QString> cityTerritories;
    for (City *city : player->getCities()) {
        cityTerritories.insert(city->getTerritoryName());
    }

    // For each city, check neighbors for road connections
    for (const QString &cityTerritory : cityTerritories) {
        for (const QString &neighbor : getNeighbors(cityTerritory)) {
            // Skip if not a city territory owned by player
            if (!cityTerritories.contains(neighbor)) continue;
            if (!player->ownsTerritory(neighbor)) continue;

            // Create sorted pair key to avoid duplicates
            QString key = (cityTerritory < neighbor)
                ? cityTerritory + "|" + neighbor
                : neighbor + "|" + cityTerritory;

            if (!processedPairs.contains(key)) {
                processedPairs.insert(key);
                segments.append({cityTerritory, neighbor});
            }
        }
    }

    return segments;
}
```

---

## Migration Steps

### Step 1: Add New MapGraph Methods
- `getRoadConnectedTerritories(startTerritory, player)`
- `getRoadSegments(player)` (for rendering)

### Step 2: Update PlayerInfoWidget Movement Code
Replace:
```cpp
for (Road *road : player->getRoads()) {
    QString territory1 = road->getTerritoryName();
    Position toPos = road->getToPosition();
    QString territory2 = m_mapWidget->getTerritoryNameAt(toPos.row, toPos.col);
    // ... BFS
}
```

With:
```cpp
QStringList roadConnected = m_mapWidget->getGraph()->getRoadConnectedTerritories(
    currentTerritory, player);
```

### Step 3: Update MapWidget Rendering (if keeping grid map)
Replace road iteration with:
```cpp
auto segments = m_graph->getRoadSegments(player);
for (const auto &[from, to] : segments) {
    QPointF fromPos = m_graph->getCentroid(from);
    QPointF toPos = m_graph->getCentroid(to);
    // Draw line between centroids
}
```

### Step 4: Remove Road Class and Related Code
- Delete `Road` class from building.h/cpp
- Remove `m_roads` from Player class
- Remove `addRoad()`, `removeRoad()`, `getRoads()`, `getRoadsAtTerritory()` from Player
- Remove `updateRoads()` from MapWidget
- Remove road destruction code from conquestTerritory, combat resolution, city destruction

### Step 5: Update Territory Display (playerinfowidget.cpp:290-292)
Replace road count display with computed value or remove entirely.

---

## Benefits

| Aspect | Before | After |
|--------|--------|-------|
| State | Duplicated (roads + cities) | Single source (cities only) |
| Sync | Manual `updateRoads()` calls | Automatic (computed on demand) |
| Memory | Road objects allocated/freed | No allocations |
| Position deps | Uses row/col positions | Uses territory names |
| OpenGL compat | Requires position conversion | Works with centroids directly |

## Estimated Scope

- **New code**: ~50 lines (2 MapGraph methods)
- **Removed code**: ~200+ lines (Road class, Player road methods, updateRoads, destruction code)
- **Modified code**: ~30 lines (movement BFS, rendering)

**Net reduction**: ~150+ lines of code
