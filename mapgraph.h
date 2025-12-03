#ifndef MAPGRAPH_H
#define MAPGRAPH_H

#include <QString>
#include <QPointF>
#include <QList>
#include <QMap>
#include <QPair>
#include <QJsonObject>

// Forward declarations
class Player;

// Territory type classification (Land or Sea only)
enum class TerritoryType {
    Land,
    Sea
};

// Represents a single territory on the map
struct Territory {
    int id;                         // Territory ID (1-60)
    QString name;                   // Territory name (e.g., "Roma", "Aegyptus")
    QPointF centroid;               // Center point for rendering pieces/labels
    QList<QString> neighbors;       // List of adjacent territory names
    TerritoryType type;             // Land or Sea
    int value;                      // Tax value (0 for sea, 5/10/20 for land)
    int area;                       // Area in pixels (from CSV)

    // Constructor with defaults
    Territory()
        : id(0)
        , type(TerritoryType::Land)
        , value(0)
        , area(0)
    {}

    Territory(int i, const QString &n, const QPointF &c, TerritoryType t, int v, int a = 0)
        : id(i)
        , name(n)
        , centroid(c)
        , type(t)
        , value(v)
        , area(a)
    {}
};

// Graph-based map representation
class MapGraph
{
public:
    MapGraph();
    ~MapGraph() = default;

    // === Territory Management ===

    // Add a new territory to the graph
    void addTerritory(const Territory &territory);

    // Get territory by name (returns empty Territory if not found)
    Territory getTerritory(const QString &name) const;

    // Get all territory names in the graph
    QList<QString> getTerritoryNames() const;

    // Check if a territory exists
    bool exists(const QString &name) const;

    // Remove a territory (rarely used, but useful for map editing)
    void removeTerritory(const QString &name);

    // Clear all territories
    void clear();

    // === Adjacency and Navigation ===

    // Get list of adjacent territory names
    QList<QString> getNeighbors(const QString &name) const;

    // Check if two territories are adjacent
    bool areAdjacent(const QString &territory1, const QString &territory2) const;

    // Add a bidirectional connection between two territories
    void addEdge(const QString &territory1, const QString &territory2);

    // Remove a bidirectional connection between two territories
    void removeEdge(const QString &territory1, const QString &territory2);

    // === Spatial Queries ===

    // Get the centroid of a territory
    QPointF getCentroid(const QString &name) const;

    // Get territory by ID (1-60)
    Territory getTerritoryById(int id) const;

    // Get territory name by ID
    QString getTerritoryNameById(int id) const;

    // === Type Queries ===

    // Check if a territory is a sea territory
    bool isSeaTerritory(const QString &name) const;

    // Check if a territory is a land territory
    bool isLandTerritory(const QString &name) const;

    // Get the type of a territory
    TerritoryType getType(const QString &name) const;

    // Get the tax value of a territory (0 for sea)
    int getValue(const QString &name) const;

    // Get list of adjacent sea territory names for a land territory
    // Returns empty list if territory doesn't exist or has no adjacent seas
    QList<QString> getAdjacentSeaTerritories(const QString &landTerritoryName) const;

    // Get sea zones that share a beach with the given sea zone at a land territory
    // Neighbors are stored in clockwise order, so consecutive sea zones share a beach
    // A beached galley from seaZone can launch into any sea zone returned by this function
    // Returns list including the original seaZone plus any adjacent seas in the neighbor list
    QList<QString> getConnectedBeachSeaZones(const QString &landTerritory, const QString &seaZone) const;

    // === Beach Position Queries (for galley movement) ===

    // Get beach position for galley moving between land and sea territory
    // Returns the position where a galley should be displayed when beached
    // Returns QPointF(0,0) if no beach position exists for this pair
    QPointF getBeachPosition(const QString &landTerritory, const QString &seaTerritory) const;

    // Check if a beach position exists for a land/sea pair
    bool hasBeachPosition(const QString &landTerritory, const QString &seaTerritory) const;

    // Get all sea zones accessible from a specific beach position on a land territory
    // This is useful for determining which seas a beached galley can launch into
    QList<QString> getSeaZonesAtBeach(const QString &landTerritory, const QPointF &beachPos) const;

    // === Road Queries (computed on-the-fly from city positions) ===

    // Get all territories reachable via roads from startTerritory for given player
    // Roads exist between adjacent territories where the same player owns both
    // territories AND has cities in both territories.
    // Returns list of territory names (excludes startTerritory itself)
    QStringList getRoadConnectedTerritories(const QString &startTerritory, const Player *player) const;

    // Get all road segments for a player (for rendering)
    // Returns list of territory name pairs representing road connections
    // Each pair appears only once (no duplicates for bidirectional roads)
    QList<QPair<QString, QString>> getRoadSegments(const Player *player) const;

    // === Pathfinding ===

    // Find shortest path between two territories (BFS)
    // Returns empty list if no path exists
    QList<QString> findPath(const QString &from, const QString &to) const;

    // Get distance between two territories (number of edges)
    // Returns -1 if no path exists
    int getDistance(const QString &from, const QString &to) const;

    // Check if two territories are connected (path exists)
    bool isReachable(const QString &from, const QString &to) const;

    // Get all territories reachable from a starting territory within N steps
    QList<QString> getTerritoriesWithinDistance(const QString &from, int maxDistance) const;

    // === Statistics ===

    // Get total number of territories
    int territoryCount() const { return m_territories.size(); }

    // Get count of territories by type
    int countByType(TerritoryType type) const;

    // === Serialization (for future use) ===

    // Load graph from JSON file
    bool loadFromJson(const QString &filePath);

    // Save graph to JSON file
    bool saveToJson(const QString &filePath) const;

    // Load graph from JSON object (for embedding in game saves)
    bool loadFromJsonObject(const QJsonObject &graphObj);

    // Save graph to JSON object (for embedding in game saves)
    QJsonObject saveToJsonObject() const;

private:
    // Load territories from CSV resource file
    void loadFromCSV();

    // Load beach positions from CSV resource file
    void loadBeachPositions();

    // Internal storage: map from territory name to Territory data
    QMap<QString, Territory> m_territories;

    // Map from territory ID to name (for neighbor resolution)
    QMap<int, QString> m_idToName;

    // Beach positions: (landName, seaName) -> beach position
    // Key is "landName|seaName" for efficient lookup
    QMap<QString, QPointF> m_beachPositions;

    // Helper function for BFS pathfinding
    QList<QString> breadthFirstSearch(const QString &from, const QString &to) const;
};

#endif // MAPGRAPH_H
