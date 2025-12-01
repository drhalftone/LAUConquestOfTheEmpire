#ifndef AIDECISIONMAKER_H
#define AIDECISIONMAKER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QSet>
#include "reachabilitycalculator.h"

// Forward declarations
class GamePiece;
class GeneralPiece;
class Player;
class MapGraph;
class City;

/**
 * @brief Represents a scored move option for the AI
 */
struct ScoredMove {
    GamePiece *leader = nullptr;
    QString destination;
    int score = 0;
    int troopsCanBring = 0;
    QString reason;  // Human-readable explanation for debugging

    bool isValid() const { return leader != nullptr && !destination.isEmpty(); }
};

/**
 * @brief Represents a planned assignment for a single general
 */
struct GeneralAssignment {
    GamePiece *general = nullptr;
    QString targetTerritory;
    int troopsToTake = 0;       // How many troops this general should bring
    QList<int> troopIds;        // Specific troop IDs to assign
    int priority = 0;           // Higher = more important mission
    QString missionType;        // "Expand", "Attack", "Defend", "StayHome"
    QString reason;

    bool isValid() const { return general != nullptr && !targetTerritory.isEmpty(); }
};

/**
 * @brief Represents a complete movement plan for the turn
 * This plans the END STATE - where each general should be at end of turn
 */
struct MovementPlan {
    QList<GeneralAssignment> assignments;
    QString summary;
    int totalTroopsDeployed = 0;
    int generalsUsed = 0;
    int territoriesTargeted = 0;

    bool isEmpty() const { return assignments.isEmpty(); }
};

/**
 * @brief Represents the AI's purchase decision
 */
struct AIPurchaseDecision {
    // Troops (placed at home province)
    int infantry = 0;
    int cavalry = 0;
    int catapults = 0;

    // Cities to build (territory name -> fortified?)
    QMap<QString, bool> cities;  // territory -> true if fortified

    // Existing cities to fortify
    QStringList fortifications;

    // Cities to DESTROY (high risk, can't defend - deny enemy the prize)
    QStringList citiesToDestroy;

    // Galleys to build (sea territory -> count)
    QMap<QString, int> galleys;

    // Total cost of all purchases
    int totalCost = 0;

    // Explanation for debugging
    QString reason;

    bool isEmpty() const {
        return infantry == 0 && cavalry == 0 && catapults == 0 &&
               cities.isEmpty() && fortifications.isEmpty() && galleys.isEmpty() &&
               citiesToDestroy.isEmpty();
    }
};

/**
 * @brief AI decision-making engine that uses risk assessment to choose moves
 *
 * This class analyzes the game state using ReachabilityCalculator and
 * scores possible moves to determine the best action for an AI player.
 */
class AIDecisionMaker
{
public:
    AIDecisionMaker();

    /**
     * @brief Get the best move for a player
     * @param player The AI player to find a move for
     * @param allPlayers All players in the game (for threat assessment)
     * @param graph The map graph
     * @return The highest-scoring move, or invalid ScoredMove if no good moves
     */
    ScoredMove getBestMove(Player *player, const QList<Player*> &allPlayers, MapGraph *graph);

    /**
     * @brief Get all scored moves for a player, sorted by score (highest first)
     * @param player The AI player
     * @param allPlayers All players in the game
     * @param graph The map graph
     * @return List of all possible moves with scores
     */
    QList<ScoredMove> getAllScoredMoves(Player *player, const QList<Player*> &allPlayers, MapGraph *graph);

    /**
     * @brief Generate a text report of AI decision analysis
     * @param player The AI player
     * @param allPlayers All players in the game
     * @param graph The map graph
     * @return Human-readable analysis report
     */
    QString generateDecisionReport(Player *player, const QList<Player*> &allPlayers, MapGraph *graph);

    // === Movement Planning Methods ===

    /**
     * @brief Plan the entire turn's movement at once
     *
     * This is the NEW planning-based approach that:
     * 1. Identifies target territories (unowned, enemy, at-risk own)
     * 2. Assigns generals to targets based on priority and reachability
     * 3. Allocates troops efficiently (concentrate force, don't scatter)
     * 4. Returns a complete plan for execution
     *
     * @param player The AI player
     * @param allPlayers All players (for threat assessment)
     * @param graph The map graph
     * @return Complete movement plan for the turn
     */
    MovementPlan planMovement(Player *player, const QList<Player*> &allPlayers, MapGraph *graph);

    /**
     * @brief Get the next move to execute from a plan
     * Call this repeatedly until it returns invalid move
     * @param plan The movement plan
     * @param player The player (to check current general positions)
     * @return Next ScoredMove to execute, or invalid if plan complete
     */
    ScoredMove getNextMoveFromPlan(const MovementPlan &plan, Player *player, const QList<Player*> &allPlayers, MapGraph *graph);

    // === Purchase Decision Methods ===

    /**
     * @brief Decide what to purchase given budget and game state
     * @param player The AI player making purchases
     * @param allPlayers All players (for threat assessment)
     * @param graph The map graph
     * @param budget Available money to spend
     * @param inflationMultiplier Current inflation (1, 2, or 3)
     * @param territoriesForCities Territories where cities can be built
     * @param territoriesForFortification Territories with cities that can be fortified
     * @param seaTerritoriesForGalleys Sea territories where galleys can be placed
     * @param currentGalleyCount How many galleys player already has
     * @param maxGalleys Maximum galleys allowed (usually 6)
     * @return Purchase decision with items to buy
     */
    AIPurchaseDecision decidePurchases(
        Player *player,
        const QList<Player*> &allPlayers,
        MapGraph *graph,
        int budget,
        int inflationMultiplier,
        const QStringList &territoriesForCities,
        const QStringList &territoriesForFortification,
        const QStringList &seaTerritoriesForGalleys,
        int currentGalleyCount,
        int maxGalleys = 6
    );

    /**
     * @brief Generate a text report explaining purchase decision
     */
    QString generatePurchaseReport(const AIPurchaseDecision &decision, int budget);

    // === Scoring Weight Configuration ===
    // These can be adjusted to tune AI behavior

    void setTerritoryValueWeight(int weight) { m_territoryValueWeight = weight; }
    void setSafetyBonus(int bonus) { m_safetyBonus = bonus; }
    void setDefenseBonus(int bonus) { m_defenseBonus = bonus; }
    void setForceAdvantageWeight(int weight) { m_forceAdvantageWeight = weight; }

private:
    /**
     * @brief Score a single move option
     * @param leader The leader who would move
     * @param destination Target territory
     * @param reachInfo Reachability info for this destination
     * @param risk Risk assessment for this destination
     * @param player The player making the move
     * @param allPlayers All players (for checking cities and defenders)
     * @param graph The map graph
     * @return Scored move with explanation
     */
    ScoredMove scoreMove(GamePiece *leader,
                         const QString &destination,
                         const ReachInfo &reachInfo,
                         const TerritoryRisk &risk,
                         Player *player,
                         const QList<Player*> &allPlayers,
                         MapGraph *graph,
                         const QMap<QString, TerritoryRisk> &riskMap);

    /**
     * @brief Count enemy troops currently stationed at a territory (not including reinforcements)
     */
    int countEnemyTroopsAt(const QString &territory, Player *us, const QList<Player*> &allPlayers);

    /**
     * @brief Check if any player has a city at a territory
     * @return Player who owns the city, or nullptr
     */
    Player* findCityOwnerAt(const QString &territory, const QList<Player*> &allPlayers);

    /**
     * @brief Check if player owns a territory
     */
    bool playerOwnsTerritory(Player *player, const QString &territory);

    /**
     * @brief Check if any enemy pieces (including leaders) are at a territory
     * This checks for ANY enemy presence which would trigger combat or block entry
     */
    bool hasEnemyPresenceAt(const QString &territory, Player *us, const QList<Player*> &allPlayers);

    // === Movement Planning Helpers ===

    /**
     * @brief Identify all target territories worth moving to
     * Returns a list of territories with scores, categorized by type
     */
    struct TargetTerritory {
        QString name;
        int score = 0;
        QString type;  // "Expand", "Attack", "Defend"
        int enemyTroops = 0;
        int troopsNeeded = 0;  // Minimum troops to take this territory
        bool requiresTroops = false;  // True if enemy presence (can't capture with lone general)
    };
    QList<TargetTerritory> identifyTargets(Player *player, const QList<Player*> &allPlayers,
                                            MapGraph *graph, const QMap<QString, TerritoryRisk> &riskMap);

    /**
     * @brief Find which generals can reach a territory this turn
     */
    QList<GamePiece*> getGeneralsWhoCanReach(const QString &territory, Player *player,
                                              MapGraph *graph, const QMap<QString, ReachInfo> &allReach);

    /**
     * @brief Count available troops at a territory (unassigned + in legion of generals there)
     */
    int countAvailableTroopsAt(const QString &territory, Player *player);

    /**
     * @brief Assign specific troop IDs to a general's assignment
     */
    void assignTroopsToGeneral(GeneralAssignment &assignment, Player *player, int maxTroops, bool requiresMultiHop = false);

    // Scoring weights (can be tuned)
    int m_territoryValueWeight = 10;   // Points per territory value (5 or 10)
    int m_safetyBonus = 50;            // Bonus for SAFE moves
    int m_lowRiskBonus = 30;           // Bonus for LOW risk moves
    int m_defenseBonus = 100;          // Bonus for defending HIGH-risk own territory
    int m_forceAdvantageWeight = 10;   // Points per troop advantage
    int m_highRiskPenalty = 50;        // Penalty for HIGH risk attacks
    int m_ownTerritoryPenalty = 30;    // Penalty for moving to own safe territory (waste of move)

    // Purchase costs (base prices before inflation)
    static const int INFANTRY_COST = 10;
    static const int CAVALRY_COST = 20;
    static const int CATAPULT_COST = 30;
    static const int CITY_COST = 30;
    static const int FORTIFICATION_COST = 20;
    static const int GALLEY_COST = 20;
};

#endif // AIDECISIONMAKER_H
