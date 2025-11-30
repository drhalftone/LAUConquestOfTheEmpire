#ifndef REACHABILITYCALCULATOR_H
#define REACHABILITYCALCULATOR_H

#include <QString>
#include <QList>
#include <QMap>
#include <QSet>

// Forward declarations
class GamePiece;
class Player;
class MapGraph;
class GalleyPiece;

/**
 * @brief Information about how a territory can be reached
 */
struct ReachInfo {
    QString territoryName;
    QList<GamePiece*> leadersWhoCanReach;  // Leaders that can reach this territory
    int maxTroopStrength = 0;               // Total troops those leaders could bring
    double bestMovesRemaining = 0.0;        // Most moves remaining after arrival (best case)
    bool viaRoad = false;                   // Can be reached via road network
    bool viaGalley = false;                 // Can be reached via galley transport
};

/**
 * @brief Information about multi-turn reachability for a leader
 */
struct MultiTurnReachInfo {
    QString territoryName;
    GamePiece *leader = nullptr;
    int turnsToReach = 0;                   // 1, 2, or 3 turns to reach
    int troopsCanBring = 0;                 // Troops that can make the journey
    bool hasCity = false;                   // Does destination have a city?
    bool hasFortifiedCity = false;          // Is the city fortified?
    QString pathDescription;                // Human-readable path for debugging
};

/**
 * @brief Risk level for a territory
 */
enum class RiskLevel {
    Safe,                   // We can reach, enemy cannot
    Low,                    // We have force advantage
    Medium,                 // Forces are roughly equal
    High,                   // Enemy has force advantage
    Unreachable             // We cannot reach this turn
};

/**
 * @brief Risk assessment for a territory
 */
struct TerritoryRisk {
    QString territoryName;
    int ourMaxForce = 0;
    int enemyMaxForce = 0;
    RiskLevel risk = RiskLevel::Unreachable;
    QList<GamePiece*> ourLeaders;           // Our leaders that can reach
    QList<GamePiece*> enemyLeaders;         // Enemy leaders that can reach
};

/**
 * @brief Calculates which territories a player can reach in a single turn
 *
 * This class analyzes leader positions, movement points, road networks,
 * and galley transport to determine all reachable territories.
 */
class ReachabilityCalculator
{
public:
    ReachabilityCalculator();

    /**
     * @brief Get all territories reachable by a single leader this turn
     * @param leader The leader (Caesar, General, or Galley) to analyze
     * @param graph The map graph for neighbor/road queries
     * @param player The player who owns this leader
     * @param turnMultiplier Multiplier for movement range (default 1). Use 2 for 2-turn projection.
     * @param useActualMoves If true, use actual current moves. If false, assume full moves (for threat assessment).
     * @return Map of territory name -> ReachInfo
     */
    QMap<QString, ReachInfo> getReachableFrom(GamePiece *leader, MapGraph *graph, Player *player, int turnMultiplier = 1, bool useActualMoves = false);

    /**
     * @brief Get all territories reachable by any of a player's leaders
     * @param player The player to analyze
     * @param graph The map graph for neighbor/road queries
     * @param turnMultiplier Multiplier for movement range (default 1). Use 2 for 2-turn projection.
     * @param useActualMoves If true, use actual current moves. If false, assume full moves (for threat assessment).
     * @return Map of territory name -> ReachInfo (aggregated across all leaders)
     */
    QMap<QString, ReachInfo> getAllReachable(Player *player, MapGraph *graph, int turnMultiplier = 1, bool useActualMoves = false);

    /**
     * @brief Generate a text report of reachability for a player
     * @param player The player to analyze
     * @param graph The map graph
     * @return Human-readable report string
     */
    QString generateReport(Player *player, MapGraph *graph);

    // === Risk Assessment Methods ===

    /**
     * @brief Assess risk for all territories by comparing our reach vs enemy reach
     * @param us The player we're analyzing for
     * @param allPlayers List of all players in the game
     * @param graph The map graph
     * @return Map of territory name -> TerritoryRisk
     */
    QMap<QString, TerritoryRisk> assessAllTerritories(Player *us, const QList<Player*> &allPlayers, MapGraph *graph);

    /**
     * @brief Generate defensive risk report - which of our territories are threatened
     * @param us The player we're analyzing for
     * @param allPlayers List of all players in the game
     * @param graph The map graph
     * @return Human-readable report string
     */
    QString generateDefensiveReport(Player *us, const QList<Player*> &allPlayers, MapGraph *graph);

    /**
     * @brief Generate offensive opportunity report - good targets to attack
     * @param us The player we're analyzing for
     * @param allPlayers List of all players in the game
     * @param graph The map graph
     * @return Human-readable report string
     */
    QString generateOffensiveReport(Player *us, const QList<Player*> &allPlayers, MapGraph *graph);

    /**
     * @brief Generate combined risk dashboard
     * @param us The player we're analyzing for
     * @param allPlayers List of all players in the game
     * @param graph The map graph
     * @return Human-readable report string
     */
    QString generateRiskDashboard(Player *us, const QList<Player*> &allPlayers, MapGraph *graph);

    /**
     * @brief Calculate the troop strength a leader can bring to a destination
     * @param leader The leader to check
     * @param player The player who owns the leader
     * @param movesUsed How many moves to reach the destination (affects which troops can follow)
     *        Infantry/Catapults can only move 1, Cavalry can move 2
     * @param turnMultiplier Multiplier for movement range (default 1). Use 2 for 2-turn projection.
     *        This multiplies the base movement for each unit type.
     * @return Total troop count that can actually reach the destination
     */
    int calculateTroopStrength(GamePiece *leader, Player *player, double movesUsed = 0.0, int turnMultiplier = 1);

    /**
     * @brief Get territories reachable within 1-3 turns by a leader WITH troops
     * @param leader The leader to analyze (must have troops to be included)
     * @param player The player who owns the leader
     * @param graph The map graph
     * @param maxTurns Maximum turns to look ahead (1-3)
     * @return List of MultiTurnReachInfo for all reachable territories
     *
     * NOTE: Only considers leaders that have troops with them.
     * Generals without troops are excluded (they can't defend).
     */
    QList<MultiTurnReachInfo> getMultiTurnReachability(GamePiece *leader, Player *player, MapGraph *graph, int maxTurns = 3);

    /**
     * @brief Get all territories reachable within 1-3 turns by ANY leader with troops
     * @param player The player to analyze
     * @param allPlayers All players (for city ownership checks)
     * @param graph The map graph
     * @param maxTurns Maximum turns to look ahead (1-3)
     * @return Map of territory name -> best MultiTurnReachInfo (shortest path with most troops)
     */
    QMap<QString, MultiTurnReachInfo> getAllMultiTurnReachability(Player *player, const QList<Player*> &allPlayers, MapGraph *graph, int maxTurns = 3);

    // === Threat Map Methods (for AI decision-making) ===

    /**
     * @brief Get enemy threat map - max enemy force that can reach each territory
     * @param us The player we're analyzing for (enemies are everyone else)
     * @param allPlayers All players in the game
     * @param graph The map graph
     * @param turnMultiplier 1 for 1-turn threat, 2 for 2-turn threat
     * @return Map of territory name -> max enemy troop strength that can reach it
     */
    QMap<QString, int> getEnemyThreatMap(Player *us, const QList<Player*> &allPlayers, MapGraph *graph, int turnMultiplier = 1);

    /**
     * @brief Get our force projection map - max force we can project to each territory
     * @param player The player to analyze
     * @param graph The map graph
     * @param turnMultiplier 1 for 1-turn projection, 2 for 2-turn projection
     * @return Map of territory name -> max troop strength we can project there
     */
    QMap<QString, int> getForceProjectionMap(Player *player, MapGraph *graph, int turnMultiplier = 1);

    /**
     * @brief Check if a territory is safe to expand to (we can reach, enemy cannot in 1 turn)
     * @param territory Territory name to check
     * @param player The player considering expansion
     * @param allPlayers All players in the game
     * @param graph The map graph
     * @return true if territory is safe for expansion (enemy 1-turn threat is 0)
     */
    bool isSafeForExpansion(const QString &territory, Player *player, const QList<Player*> &allPlayers, MapGraph *graph);

    /**
     * @brief Get territories that will be threatened in 2 turns but not in 1 turn
     * These are territories where we should preemptively move troops
     * @param us The player we're analyzing for
     * @param allPlayers All players in the game
     * @param graph The map graph
     * @return Map of territory name -> enemy force that can reach in 2 turns
     */
    QMap<QString, int> getEmergingThreats(Player *us, const QList<Player*> &allPlayers, MapGraph *graph);

private:
    /**
     * @brief Get territories reachable by land movement
     * @param startTerritory Starting territory name
     * @param movesRemaining Movement points available
     * @param graph The map graph
     * @param visited Set of already visited territories (modified)
     * @param results Map to store results (modified)
     * @param originalMoves Original movement points (for calculating remaining)
     */
    void getReachableByLand(const QString &startTerritory,
                           double movesRemaining,
                           MapGraph *graph,
                           QSet<QString> &visited,
                           QMap<QString, double> &results,
                           double originalMoves);

    /**
     * @brief Get territories reachable via road network
     * @param startTerritory Starting territory (must have a city)
     * @param player The player (for checking city ownership)
     * @param graph The map graph
     * @return List of territories reachable via roads
     */
    QStringList getReachableByRoad(const QString &startTerritory, Player *player, MapGraph *graph);

    /**
     * @brief Get territories reachable via galley transport
     * @param leader The leader to transport
     * @param player The player
     * @param graph The map graph
     * @return Map of territory -> moves remaining after arrival
     */
    QMap<QString, double> getReachableByGalley(GamePiece *leader, Player *player, MapGraph *graph);
};

#endif // REACHABILITYCALCULATOR_H
