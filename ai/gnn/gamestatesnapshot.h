#ifndef GAMESTATESNAPSHOT_H
#define GAMESTATESNAPSHOT_H

#include <QString>
#include <QList>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>

// Forward declarations
class Player;
class MapGraph;
class ReachabilityCalculator;

/**
 * @brief Per-territory snapshot data for GNN training
 *
 * Contains all features needed for a single territory node in the graph.
 */
struct TerritorySnapshot {
    // Territory identity
    QString name;
    int id = 0;
    bool isSea = false;
    int value = 0;  // Tax value (0 for sea)

    // Ownership (mutually exclusive)
    QChar owner = '\0';       // '\0' = neutral, 'A'-'F' = player
    bool isMine = false;
    bool isEnemy = false;
    bool isNeutral = true;

    // My forces (current player)
    int myInfantry = 0;
    int myCavalry = 0;
    int myCatapults = 0;
    int myGenerals = 0;
    int myGalleys = 0;
    bool myCaesarHere = false;

    // Enemy forces (summed across all enemies)
    int enemyInfantry = 0;
    int enemyCavalry = 0;
    int enemyCatapults = 0;
    int enemyGenerals = 0;
    int enemyGalleys = 0;
    bool enemyCaesarHere = false;

    // Infrastructure
    bool hasCity = false;
    bool hasFortification = false;
    bool onMyRoadNetwork = false;

    // Strategic features
    bool isHomeProvince = false;     // Starting province for any player
    int distToMyCaesar = -1;         // Graph distance (-1 = unreachable/no Caesar)
    int distToNearestEnemy = -1;     // Graph distance to nearest enemy unit

    // Heat map features (from ReachabilityCalculator)
    int myForce1Turn = 0;            // Max force I can project here in 1 turn
    int myForce2Turn = 0;            // Max force I can project here in 2 turns
    int enemyThreat1Turn = 0;        // Max enemy force that can reach here in 1 turn
    int enemyThreat2Turn = 0;        // Max enemy force that can reach here in 2 turns

    // Risk label (from heuristic - training target)
    int riskLevel = 0;               // 0=Safe, 1=Low, 2=Medium, 3=High, 4=Contested
    double riskScore = 0.0;          // Continuous version (0.0 to 1.0)

    // Adjacency (filled separately)
    QList<QString> neighbors;

    // Serialize to JSON
    QJsonObject toJson() const;

    // Deserialize from JSON
    static TerritorySnapshot fromJson(const QJsonObject &obj);
};

/**
 * @brief Complete game state snapshot for GNN training
 *
 * Captures the full state of the game at a single point in time,
 * from the perspective of a specific player (for risk assessment).
 */
class GameStateSnapshot
{
public:
    GameStateSnapshot();

    /**
     * @brief Capture current game state
     * @param currentPlayer The player whose perspective we're capturing (for "my" vs "enemy")
     * @param allPlayers All players in the game
     * @param graph The map graph
     * @param reachCalc Reachability calculator for heat map features (optional)
     */
    void capture(Player *currentPlayer,
                 const QList<Player*> &allPlayers,
                 MapGraph *graph,
                 ReachabilityCalculator *reachCalc = nullptr);

    // === Accessors ===

    // Get snapshot for a specific territory
    TerritorySnapshot getTerritory(const QString &name) const;

    // Get all territory snapshots
    const QMap<QString, TerritorySnapshot>& getTerritories() const { return m_territories; }

    // Get territory names in order
    QList<QString> getTerritoryNames() const { return m_territoryNames; }

    // Get adjacency list for a territory
    QList<QString> getNeighbors(const QString &territory) const;

    // Get the current player ID
    QChar getCurrentPlayerId() const { return m_currentPlayerId; }

    // Get turn number
    int getTurnNumber() const { return m_turnNumber; }

    // Get timestamp
    QString getTimestamp() const { return m_timestamp; }

    // === Serialization ===

    // Save to JSON object
    QJsonObject toJson() const;

    // Load from JSON object
    bool fromJson(const QJsonObject &obj);

    // Save to JSON file
    bool saveToFile(const QString &filePath) const;

    // Load from JSON file
    bool loadFromFile(const QString &filePath);

    // === Statistics ===

    int territoryCount() const { return m_territories.size(); }
    int landTerritoryCount() const;
    int seaTerritoryCount() const;

private:
    // Captured territory data
    QMap<QString, TerritorySnapshot> m_territories;

    // Territory names in consistent order (for indexing)
    QList<QString> m_territoryNames;

    // Game metadata
    QChar m_currentPlayerId = '\0';
    int m_turnNumber = 0;
    QString m_timestamp;

    // Helper: Calculate distance from territory to Caesar
    int calculateDistanceToCaesar(const QString &territory, Player *player, MapGraph *graph);

    // Helper: Calculate distance to nearest enemy
    int calculateDistanceToNearestEnemy(const QString &territory, Player *currentPlayer,
                                        const QList<Player*> &allPlayers, MapGraph *graph);

    // Helper: Check if territory is on player's road network
    bool isOnRoadNetwork(const QString &territory, Player *player, MapGraph *graph);
};

#endif // GAMESTATESNAPSHOT_H
