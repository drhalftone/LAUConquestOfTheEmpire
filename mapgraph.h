#ifndef MAPGRAPH_H
#define MAPGRAPH_H

#include <QString>
#include <QPointF>
#include <QList>
#include <QMap>
#include <QJsonObject>

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

    // Constructor with defaults
    Territory()
        : id(0)
        , type(TerritoryType::Land)
        , value(0)
    {}

    Territory(int i, const QString &n, const QPointF &c, TerritoryType t, int v)
        : id(i)
        , name(n)
        , centroid(c)
        , type(t)
        , value(v)
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

    // Internal storage: map from territory name to Territory data
    QMap<QString, Territory> m_territories;

    // Map from territory ID to name (for neighbor resolution)
    QMap<int, QString> m_idToName;

    // Helper function for BFS pathfinding
    QList<QString> breadthFirstSearch(const QString &from, const QString &to) const;
};

#endif // MAPGRAPH_H
