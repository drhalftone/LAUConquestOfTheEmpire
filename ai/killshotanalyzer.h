#ifndef KILLSHOTANALYZER_H
#define KILLSHOTANALYZER_H

#include <QString>
#include <QList>
#include <QMap>
#include "reachabilitycalculator.h"
#include "moveenumerator.h"
#include "combatsimulator.h"

// Forward declarations
class Player;
class GamePiece;
class MapGraph;

/**
 * @brief Type of kill shot opportunity
 */
enum class KillShotType {
    CaesarKill,      // Attack Caesar directly (eliminates player)
    HomeCityCapture  // Capture home city (prevents troop purchases)
};

/**
 * @brief Result of analyzing a potential kill shot opportunity
 */
struct KillShotOpportunity {
    KillShotType type = KillShotType::CaesarKill;  // Type of opportunity
    Player *targetPlayer = nullptr;          // The player we're attacking
    QString targetTerritory;                 // Territory to attack (Caesar location or home city)
    QString caesarLocation;                  // Where Caesar actually is
    QString homeCity;                        // Where home city is
    int ourMaxForce = 0;                     // Maximum troops we can concentrate
    int enemyDefenders = 0;                  // Enemy troops defending
    bool enemyHasFortifiedCity = false;      // Does defender have walls?
    bool caesarAtTarget = false;             // Is Caesar at the target territory?
    double winProbability = 0.0;             // Estimated win chance (0.0 - 1.0)
    double expectedCasualties = 0.0;         // Expected troop losses
    int turnsToReach = 1;                    // How many turns to reach (1 = this turn)
    QList<GamePiece*> generalsToUse;         // Which generals should attack
    QString reason;                          // Explanation for debugging

    // Unit breakdown from heat map
    int infantryCount = 0;
    int cavalryCount = 0;
    int catapultCount = 0;

    // Enemy unit breakdown
    int enemyInfantryCount = 0;
    int enemyCavalryCount = 0;
    int enemyCatapultCount = 0;

    bool isViable() const { return targetPlayer != nullptr && winProbability > 0.5; }
    bool isHighConfidence() const { return winProbability >= 0.7; }
    bool isOverwhelming() const { return winProbability >= 0.85; }
    bool isCaesarKill() const { return type == KillShotType::CaesarKill; }
    bool isHomeCityCapture() const { return type == KillShotType::HomeCityCapture; }
};

/**
 * @brief Result of analyzing defensive kill shot threats
 */
struct KillShotThreat {
    Player *threateningPlayer = nullptr;     // The player who threatens us
    int enemyMaxForce = 0;                   // Maximum enemy force that can reach us
    int ourDefenders = 0;                    // Our troops at home
    bool weHaveFortifiedCity = false;        // Do we have walls?
    bool caesarAtHome = false;               // Is our Caesar at home?
    double enemyWinProbability = 0.0;        // Enemy's chance of winning
    double expectedEnemyCasualties = 0.0;    // Expected casualties for enemy
    int turnsUntilThreat = 1;                // How soon can they attack (1 = this turn)
    QString reason;                          // Explanation for debugging

    // Enemy unit breakdown
    int enemyInfantryCount = 0;
    int enemyCavalryCount = 0;
    int enemyCatapultCount = 0;

    // Our unit breakdown
    int ourInfantryCount = 0;
    int ourCavalryCount = 0;
    int ourCatapultCount = 0;

    bool isCritical() const { return threateningPlayer != nullptr && enemyWinProbability > 0.5; }
    bool isUrgent() const { return enemyWinProbability >= 0.7 && turnsUntilThreat <= 1; }
};

/**
 * @brief Recommended action from kill shot analysis
 */
struct KillShotAction {
    enum class ActionType {
        None,                   // No kill shot situation - proceed normally
        ExecuteKillShot,        // Attack enemy home with full force
        DefendHome,             // Reinforce our home against threat
        PreemptiveStrike,       // Attack threatening player before they attack us
        Retreat                 // Pull back to defend
    };

    ActionType type = ActionType::None;
    KillShotOpportunity opportunity;         // If executing kill shot
    KillShotThreat threat;                   // If defending
    int priority = 0;                        // Higher = more urgent
    QString summary;                         // Human-readable summary

    bool shouldOverrideNormalAI() const {
        return type != ActionType::None && priority >= 80;
    }
};

/**
 * @brief Analyzes the game state for kill shot opportunities and threats
 *
 * A "kill shot" is an attack on a player's home city that would eliminate
 * them from the game. This analyzer detects both offensive opportunities
 * and defensive threats.
 */
class KillShotAnalyzer
{
public:
    KillShotAnalyzer();

    /**
     * @brief Perform full kill shot analysis for a player
     * @param player The AI player to analyze for
     * @param allPlayers All players in the game
     * @param graph The map graph
     * @return Recommended action (attack, defend, or none)
     */
    KillShotAction analyze(Player *player, const QList<Player*> &allPlayers, MapGraph *graph);

    /**
     * @brief Generate a human-readable report of all kill shot scenarios
     * This is intended for display in a text edit window for debugging/visualization
     * @param player The AI player to analyze for
     * @param allPlayers All players in the game
     * @param graph The map graph
     * @return Formatted text report of all scenarios
     */
    QString generateReport(Player *player, const QList<Player*> &allPlayers, MapGraph *graph);

    /**
     * @brief Find all offensive kill shot opportunities
     * @return List of potential targets sorted by viability
     */
    QList<KillShotOpportunity> findOffensiveOpportunities(
        Player *player,
        const QList<Player*> &allPlayers,
        MapGraph *graph);

    /**
     * @brief Find all defensive kill shot threats
     * @return List of threats sorted by urgency
     */
    QList<KillShotThreat> findDefensiveThreats(
        Player *player,
        const QList<Player*> &allPlayers,
        MapGraph *graph);

    /**
     * @brief Calculate maximum force we can bring to a territory this turn
     * Uses MoveEnumerator heat map for accurate force projection
     * @param targetTerritory The territory to attack
     * @param player Our player
     * @param allPlayers All players in game
     * @param graph The map graph
     * @param outGenerals Output: which generals can participate
     * @param outBreakdown Output: breakdown by unit type
     * @return Total troop strength that can reach target
     */
    int calculateMaxConcentration(
        const QString &targetTerritory,
        Player *player,
        const QList<Player*> &allPlayers,
        MapGraph *graph,
        QList<GamePiece*> &outGenerals,
        ReachabilityBreakdown *outBreakdown = nullptr);

    /**
     * @brief Calculate maximum enemy force that can reach a territory
     * Uses MoveEnumerator heat map for accurate force projection
     */
    int calculateEnemyMaxForce(
        const QString &territory,
        Player *us,
        const QList<Player*> &allPlayers,
        MapGraph *graph,
        Player **outThreateningPlayer = nullptr,
        ReachabilityBreakdown *outBreakdown = nullptr);

    // Configuration
    void setKillShotThreshold(double threshold) { m_killShotThreshold = threshold; }
    void setDefenseThreshold(double threshold) { m_defenseThreshold = threshold; }

private:
    /**
     * @brief Run Monte Carlo simulation to get combat probability
     *
     * Uses CombatSimulator with proper unit breakdowns for accurate
     * win probability and expected casualty calculations.
     *
     * @param attackerBreakdown Attacker's unit composition
     * @param defenderBreakdown Defender's unit composition
     * @param defenderHasFortifiedCity Does defender have fortified city?
     * @return CombatProbability with win chances and expected casualties
     */
    CombatProbability simulateCombat(
        const ReachabilityBreakdown &attackerBreakdown,
        const ReachabilityBreakdown &defenderBreakdown,
        bool defenderHasFortifiedCity);

    /**
     * @brief Count defender's unit breakdown at a territory
     */
    ReachabilityBreakdown countDefenderBreakdown(const QString &territory, Player *player);

    /**
     * @brief Count troops at a territory for a player
     */
    int countTroopsAt(const QString &territory, Player *player);

    /**
     * @brief Check if player has a fortified city at territory
     */
    bool hasFortifiedCityAt(const QString &territory, Player *player);

    /**
     * @brief Check if player's Caesar is at territory
     */
    bool hasCaesarAt(const QString &territory, Player *player);

    // Configuration thresholds
    double m_killShotThreshold = 0.70;   // Min win probability to attempt kill shot
    double m_defenseThreshold = 0.50;    // Enemy win probability that triggers defense
};

#endif // KILLSHOTANALYZER_H
