#ifndef COMBATSIMULATOR_H
#define COMBATSIMULATOR_H

#include <QString>
#include <QList>

// Forward declarations
class Player;
class GamePiece;
class MapGraph;

/**
 * @brief Represents an army's composition for combat simulation
 */
struct ArmyComposition {
    int infantry = 0;
    int cavalry = 0;
    int catapults = 0;
    int generals = 0;          // Number of generals (not Caesar)
    bool hasCaesar = false;

    int totalTroops() const { return infantry + cavalry + catapults; }
    int totalPieces() const { return totalTroops() + generals + (hasCaesar ? 1 : 0); }
    bool hasTroops() const { return totalTroops() > 0; }
    bool hasLeaders() const { return generals > 0 || hasCaesar; }
};

/**
 * @brief Terrain and fortification modifiers for combat
 */
struct CombatTerrain {
    bool defenderHasFortifiedCity = false;   // Walls give defender advantage
    bool defenderHasCity = false;            // Unfortified city
    bool isSeaCombat = false;                // Naval battle rules
    QString territoryName;
};

/**
 * @brief Current state of an ongoing battle
 */
struct BattleState {
    ArmyComposition attacker;
    ArmyComposition defender;
    CombatTerrain terrain;

    bool isAttackersTurn = true;
    int roundNumber = 0;

    // Casualties this battle
    int attackerCasualties = 0;
    int defenderCasualties = 0;

    // Leaders lost
    int attackerGeneralsLost = 0;
    int defenderGeneralsLost = 0;
    bool attackerLostCaesar = false;
    bool defenderLostCaesar = false;
};

/**
 * @brief Result of a combat probability analysis
 */
struct CombatProbability {
    double attackerWinChance = 0.0;          // 0.0 - 1.0
    double defenderWinChance = 0.0;          // 0.0 - 1.0
    double drawChance = 0.0;                 // Mutual destruction

    // Expected outcomes
    double expectedAttackerCasualties = 0.0;
    double expectedDefenderCasualties = 0.0;
    double expectedAttackerSurvivors = 0.0;
    double expectedDefenderSurvivors = 0.0;

    // Risk assessment
    double attackerCaesarRisk = 0.0;         // Chance of losing Caesar
    double defenderCaesarRisk = 0.0;

    QString analysis;                         // Human-readable explanation
};

/**
 * @brief Recommendation for whether to continue fighting or retreat
 */
struct RetreatRecommendation {
    bool shouldRetreat = false;
    double confidenceLevel = 0.0;            // How confident in this recommendation
    QString reason;

    // Factors considered
    double currentWinChance = 0.0;
    double expectedFurtherLosses = 0.0;
    bool caesarAtRisk = false;
    bool wouldLoseAllTroops = false;
};

/**
 * @brief Result of a single combat round simulation
 */
struct RoundResult {
    bool attackerHit = false;                // Did attacker kill a defender?
    bool defenderHit = false;                // Did defender kill an attacker?
    int attackerRoll = 0;                    // Die roll + advantage
    int defenderRoll = 0;
    int attackerAdvantage = 0;
    int defenderAdvantage = 0;

    // What was lost (if any)
    QString attackerLostType;                // "Infantry", "Cavalry", "Catapult", "General", "Caesar"
    QString defenderLostType;
};

/**
 * @brief Simulates combat without UI for AI decision making
 *
 * This class mirrors the logic of CombatDialog but without any UI components.
 * It can:
 * - Calculate combat advantages based on army composition and terrain
 * - Estimate win probabilities using Monte Carlo simulation
 * - Track real-time battle state as casualties occur
 * - Recommend when to retreat
 * - Be used by KillShotAnalyzer for accurate win probability estimation
 */
class CombatSimulator
{
public:
    CombatSimulator();

    // ========================================================================
    // Setup Methods
    // ========================================================================

    /**
     * @brief Initialize a new battle simulation
     * @param attacker Attacking army composition
     * @param defender Defending army composition
     * @param terrain Terrain and fortification modifiers
     */
    void initializeBattle(const ArmyComposition &attacker,
                          const ArmyComposition &defender,
                          const CombatTerrain &terrain);

    /**
     * @brief Initialize from actual game pieces at a territory
     * @param attackingPlayer The attacking player
     * @param defendingPlayer The defending player
     * @param territoryName Where the battle takes place
     * @param graph Map graph for terrain info
     */
    void initializeFromGame(Player *attackingPlayer,
                            Player *defendingPlayer,
                            const QString &territoryName,
                            MapGraph *graph);

    // ========================================================================
    // Advantage Calculation (mirrors CombatDialog logic)
    // ========================================================================

    /**
     * @brief Calculate attacker's combat advantage
     * Considers: Caesar (+2), multiple generals, cavalry, catapults vs walls
     */
    int calculateAttackerAdvantage() const;

    /**
     * @brief Calculate defender's combat advantage
     * Considers: Caesar (+2), multiple generals, cavalry, fortified city (+2)
     */
    int calculateDefenderAdvantage() const;

    /**
     * @brief Get net advantage for a side (their advantage - opponent's)
     */
    int getNetAdvantage(bool forAttacker) const;

    // ========================================================================
    // Probability Estimation
    // ========================================================================

    /**
     * @brief Calculate win probability using Monte Carlo simulation
     * @param numSimulations Number of battles to simulate (default 1000)
     * @return Detailed probability analysis
     */
    CombatProbability calculateWinProbability(int numSimulations = 1000) const;

    /**
     * @brief Quick probability estimate without full simulation
     * Uses heuristics based on force ratio and advantages
     */
    CombatProbability quickEstimate() const;

    // ========================================================================
    // Real-Time Battle Tracking
    // ========================================================================

    /**
     * @brief Record that a troop was lost
     * @param isAttacker True if attacker lost the troop
     * @param troopType Type of troop lost ("Infantry", "Cavalry", "Catapult", "General", "Caesar")
     */
    void recordCasualty(bool isAttacker, const QString &troopType);

    /**
     * @brief Get current battle state
     */
    const BattleState& getCurrentState() const { return m_state; }

    /**
     * @brief Recalculate probabilities based on current state
     */
    CombatProbability recalculateProbability() const;

    // ========================================================================
    // Retreat Decision
    // ========================================================================

    /**
     * @brief Should the attacker retreat?
     * Considers current odds, Caesar risk, troop preservation
     */
    RetreatRecommendation shouldAttackerRetreat() const;

    /**
     * @brief Set threshold for retreat recommendation
     * @param threshold Win probability below which retreat is recommended (default 0.3)
     */
    void setRetreatThreshold(double threshold) { m_retreatThreshold = threshold; }

    // ========================================================================
    // Single Round Simulation
    // ========================================================================

    /**
     * @brief Simulate a single round of combat
     * @param attackerRoll Die roll for attacker (1-6), or -1 to generate randomly
     * @param defenderRoll Die roll for defender (1-6), or -1 to generate randomly
     * @return Result of the round
     */
    RoundResult simulateRound(int attackerRoll = -1, int defenderRoll = -1) const;

    /**
     * @brief Simulate entire battle to completion
     * @return Final battle state after one side is eliminated or retreats
     */
    BattleState simulateFullBattle() const;

    // ========================================================================
    // Utility Methods
    // ========================================================================

    /**
     * @brief Check if battle is over
     */
    bool isBattleOver() const;

    /**
     * @brief Check who won (only valid if isBattleOver())
     * @return 1 = attacker won, -1 = defender won, 0 = ongoing
     */
    int getWinner() const;

    /**
     * @brief Generate human-readable battle report
     */
    QString generateReport() const;

    /**
     * @brief Reset to initial state (before any casualties)
     */
    void reset();

private:
    BattleState m_state;
    BattleState m_initialState;              // For reset

    double m_retreatThreshold = 0.30;        // Retreat if win chance below this

    /**
     * @brief Determine hit/miss based on roll and advantage
     * @param roll Die roll (1-6)
     * @param advantage Combat advantage
     * @return True if hit (kills enemy troop)
     */
    bool isHit(int roll, int advantage) const;

    /**
     * @brief Select which troop type to lose (random weighted selection)
     */
    QString selectCasualtyType(const ArmyComposition &army) const;

    /**
     * @brief Apply a casualty to an army composition
     */
    void applyCasualty(ArmyComposition &army, const QString &troopType);
};

#endif // COMBATSIMULATOR_H
