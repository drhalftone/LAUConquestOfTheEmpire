#include "combatsimulator.h"
#include "../player.h"
#include "../gamepiece.h"
#include "../mapgraph.h"
#include "../building.h"
#include <QDebug>

CombatSimulator::CombatSimulator()
{
}

// ============================================================================
// Setup Methods
// ============================================================================

void CombatSimulator::initializeBattle(const ArmyComposition &attacker,
                                        const ArmyComposition &defender,
                                        const CombatTerrain &terrain)
{
    // TODO: Store initial state
    m_state.attacker = attacker;
    m_state.defender = defender;
    m_state.terrain = terrain;
    m_state.isAttackersTurn = true;
    m_state.roundNumber = 0;
    m_state.attackerCasualties = 0;
    m_state.defenderCasualties = 0;

    m_initialState = m_state;
}

void CombatSimulator::initializeFromGame(Player *attackingPlayer,
                                          Player *defendingPlayer,
                                          const QString &territoryName,
                                          MapGraph *graph)
{
    // TODO: Extract army compositions from actual game pieces
    // - Count infantry, cavalry, catapults at territory for each player
    // - Check for Caesar and generals
    // - Check for fortified city

    ArmyComposition attacker;
    ArmyComposition defender;
    CombatTerrain terrain;
    terrain.territoryName = territoryName;

    // TODO: Populate from game state

    initializeBattle(attacker, defender, terrain);
}

// ============================================================================
// Advantage Calculation
// ============================================================================

int CombatSimulator::calculateAttackerAdvantage() const
{
    // Catapults provide advantage (matches CombatDialog logic)
    return m_state.attacker.catapults;
}

int CombatSimulator::calculateDefenderAdvantage() const
{
    int advantage = m_state.defender.catapults;

    // Fortified city gives +1 advantage
    if (m_state.terrain.defenderHasFortifiedCity) {
        advantage++;
    }

    return advantage;
}

int CombatSimulator::getNetAdvantage(bool forAttacker) const
{
    int attackerAdv = calculateAttackerAdvantage();
    int defenderAdv = calculateDefenderAdvantage();
    int difference = attackerAdv - defenderAdv;

    if (forAttacker) {
        return (difference > 0) ? difference : 0;  // Only positive for attacker
    } else {
        return (difference < 0) ? -difference : 0;  // Only positive for defender
    }
}

// ============================================================================
// Probability Estimation
// ============================================================================

CombatProbability CombatSimulator::calculateWinProbability(int numSimulations) const
{
    CombatProbability result;

    bool isSeaCombat = m_state.terrain.isSeaCombat;

    // Edge cases - in sea combat, galleys matter; in land combat, only troops matter
    bool attackerHasForces = m_state.attacker.hasTroops() || (isSeaCombat && m_state.attacker.hasGalleys());
    bool defenderHasForces = m_state.defender.hasTroops() || (isSeaCombat && m_state.defender.hasGalleys());

    if (!attackerHasForces) {
        result.defenderWinChance = 1.0;
        return result;
    }
    if (!defenderHasForces) {
        result.attackerWinChance = 1.0;
        return result;
    }

    int attackerWins = 0;
    int defenderWins = 0;
    double totalAttackerSurvivors = 0.0;
    double totalDefenderSurvivors = 0.0;
    double totalAttackerCasualties = 0.0;
    double totalDefenderCasualties = 0.0;

    // Hit thresholds: Infantry 4+, Cavalry 5+, Catapult 6+, Galley 3+ (no advantage)
    const int INFANTRY_THRESHOLD = 4;
    const int CAVALRY_THRESHOLD = 5;
    const int CATAPULT_THRESHOLD = 6;
    const int GALLEY_THRESHOLD = 3;

    for (int sim = 0; sim < numSimulations; sim++) {
        // Copy current state for simulation
        ArmyComposition attacker = m_state.attacker;
        ArmyComposition defender = m_state.defender;
        bool defenderHasFortifiedCity = m_state.terrain.defenderHasFortifiedCity;

        bool isAttackerTurn = true;

        // Battle continues while both sides have forces
        // In sea combat: troops must be killed before galleys can be targeted
        auto hasForces = [isSeaCombat](const ArmyComposition &army) {
            return army.hasTroops() || (isSeaCombat && army.hasGalleys());
        };

        while (hasForces(attacker) && hasForces(defender)) {
            // Calculate current advantage (catapult difference)
            int attackerCatapults = attacker.catapults;
            int defenderCatapults = defender.catapults + (defenderHasFortifiedCity ? 1 : 0);

            if (isAttackerTurn) {
                // Attacker attacks defender
                int advantage = attackerCatapults - defenderCatapults;
                advantage = (advantage > 0) ? advantage : 0;

                // Choose target: catapult > cavalry > infantry > galley (only in sea combat)
                int threshold;
                int *targetCount;
                bool targetingGalley = false;

                if (defender.catapults > 0) {
                    threshold = CATAPULT_THRESHOLD;
                    targetCount = &defender.catapults;
                } else if (defender.cavalry > 0) {
                    threshold = CAVALRY_THRESHOLD;
                    targetCount = &defender.cavalry;
                } else if (defender.infantry > 0) {
                    threshold = INFANTRY_THRESHOLD;
                    targetCount = &defender.infantry;
                } else if (isSeaCombat && defender.galleys > 0) {
                    // Galleys can only be targeted after all troops are gone
                    threshold = GALLEY_THRESHOLD;
                    targetCount = &defender.galleys;
                    targetingGalley = true;
                } else {
                    break;  // No valid targets
                }

                // Galleys don't use advantage modifier
                int roll = (rand() % 6) + 1 + (targetingGalley ? 0 : advantage);
                if (roll >= threshold) {
                    (*targetCount)--;
                }
            } else {
                // Defender attacks attacker
                int advantage = defenderCatapults - attackerCatapults;
                advantage = (advantage > 0) ? advantage : 0;

                // Choose target: catapult > cavalry > infantry > galley (only in sea combat)
                int threshold;
                int *targetCount;
                bool targetingGalley = false;

                if (attacker.catapults > 0) {
                    threshold = CATAPULT_THRESHOLD;
                    targetCount = &attacker.catapults;
                } else if (attacker.cavalry > 0) {
                    threshold = CAVALRY_THRESHOLD;
                    targetCount = &attacker.cavalry;
                } else if (attacker.infantry > 0) {
                    threshold = INFANTRY_THRESHOLD;
                    targetCount = &attacker.infantry;
                } else if (isSeaCombat && attacker.galleys > 0) {
                    // Galleys can only be targeted after all troops are gone
                    threshold = GALLEY_THRESHOLD;
                    targetCount = &attacker.galleys;
                    targetingGalley = true;
                } else {
                    break;  // No valid targets
                }

                // Galleys don't use advantage modifier
                int roll = (rand() % 6) + 1 + (targetingGalley ? 0 : advantage);
                if (roll >= threshold) {
                    (*targetCount)--;
                }
            }

            isAttackerTurn = !isAttackerTurn;
        }

        // Record outcome
        if (hasForces(attacker)) {
            attackerWins++;
            totalAttackerSurvivors += attacker.totalTroops() + (isSeaCombat ? attacker.galleys : 0);
        } else {
            defenderWins++;
            totalDefenderSurvivors += defender.totalTroops() + (isSeaCombat ? defender.galleys : 0);
        }

        int attackerStartForces = m_state.attacker.totalTroops() + (isSeaCombat ? m_state.attacker.galleys : 0);
        int attackerEndForces = attacker.totalTroops() + (isSeaCombat ? attacker.galleys : 0);
        int defenderStartForces = m_state.defender.totalTroops() + (isSeaCombat ? m_state.defender.galleys : 0);
        int defenderEndForces = defender.totalTroops() + (isSeaCombat ? defender.galleys : 0);

        totalAttackerCasualties += attackerStartForces - attackerEndForces;
        totalDefenderCasualties += defenderStartForces - defenderEndForces;
    }

    // Calculate probabilities
    result.attackerWinChance = (double)attackerWins / numSimulations;
    result.defenderWinChance = (double)defenderWins / numSimulations;
    result.drawChance = 0.0;  // Mutual destruction not possible in alternating combat

    // Calculate expected outcomes
    result.expectedAttackerCasualties = totalAttackerCasualties / numSimulations;
    result.expectedDefenderCasualties = totalDefenderCasualties / numSimulations;
    result.expectedAttackerSurvivors = (attackerWins > 0) ? totalAttackerSurvivors / attackerWins : 0;
    result.expectedDefenderSurvivors = (defenderWins > 0) ? totalDefenderSurvivors / defenderWins : 0;

    return result;
}

CombatProbability CombatSimulator::quickEstimate() const
{
    // TODO: Implement heuristic-based probability estimation
    // Based on force ratios and advantages

    CombatProbability result;

    int attackerTroops = m_state.attacker.totalTroops();
    int defenderTroops = m_state.defender.totalTroops();

    if (attackerTroops == 0) {
        result.defenderWinChance = 1.0;
        return result;
    }

    if (defenderTroops == 0) {
        result.attackerWinChance = 1.0;
        return result;
    }

    // Placeholder: simple ratio
    double ratio = (double)attackerTroops / (double)defenderTroops;
    // TODO: Factor in advantages

    result.attackerWinChance = qMin(0.95, ratio / (ratio + 1.0));
    result.defenderWinChance = 1.0 - result.attackerWinChance;

    return result;
}

// ============================================================================
// Real-Time Battle Tracking
// ============================================================================

void CombatSimulator::recordCasualty(bool isAttacker, const QString &troopType)
{
    // TODO: Update battle state when a casualty occurs
    // - Decrement appropriate troop count
    // - Increment casualty counters
    // - Check for battle end

    if (isAttacker) {
        m_state.attackerCasualties++;
        applyCasualty(m_state.attacker, troopType);
    } else {
        m_state.defenderCasualties++;
        applyCasualty(m_state.defender, troopType);
    }
}

CombatProbability CombatSimulator::recalculateProbability() const
{
    // Recalculate based on current (post-casualty) state using Monte Carlo
    return calculateWinProbability(1000);
}

// ============================================================================
// Retreat Decision
// ============================================================================

RetreatRecommendation CombatSimulator::shouldAttackerRetreat() const
{
    // TODO: Implement retreat logic
    // Consider:
    // - Current win probability
    // - Caesar at risk
    // - Expected future casualties
    // - Value of continuing vs preserving troops

    RetreatRecommendation rec;

    CombatProbability prob = recalculateProbability();
    rec.currentWinChance = prob.attackerWinChance;

    if (prob.attackerWinChance < m_retreatThreshold) {
        rec.shouldRetreat = true;
        rec.reason = QString("Win chance (%1%) below threshold (%2%)")
            .arg(int(prob.attackerWinChance * 100))
            .arg(int(m_retreatThreshold * 100));
    }

    // TODO: Check Caesar risk
    if (m_state.attacker.hasCaesar && m_state.attacker.totalTroops() <= 2) {
        rec.caesarAtRisk = true;
        rec.shouldRetreat = true;
        rec.reason = "Caesar at risk with few remaining troops";
    }

    return rec;
}

// ============================================================================
// Single Round Simulation
// ============================================================================

RoundResult CombatSimulator::simulateRound(int attackerRoll, int defenderRoll) const
{
    // TODO: Simulate one round of combat
    // - Generate random rolls if not provided
    // - Apply advantages
    // - Determine hits
    // - Select casualties

    RoundResult result;

    // Generate random rolls if needed
    if (attackerRoll < 1 || attackerRoll > 6) {
        attackerRoll = (rand() % 6) + 1;
    }
    if (defenderRoll < 1 || defenderRoll > 6) {
        defenderRoll = (rand() % 6) + 1;
    }

    result.attackerRoll = attackerRoll;
    result.defenderRoll = defenderRoll;
    result.attackerAdvantage = calculateAttackerAdvantage();
    result.defenderAdvantage = calculateDefenderAdvantage();

    // TODO: Determine hits based on combat rules
    // result.attackerHit = isHit(attackerRoll, result.attackerAdvantage);
    // result.defenderHit = isHit(defenderRoll, result.defenderAdvantage);

    return result;
}

BattleState CombatSimulator::simulateFullBattle() const
{
    // TODO: Simulate complete battle
    // - Run rounds until one side has no troops
    // - Return final state

    BattleState state = m_state;

    // Placeholder
    while (state.attacker.hasTroops() && state.defender.hasTroops()) {
        // Simulate rounds...
        break;  // Prevent infinite loop in placeholder
    }

    return state;
}

// ============================================================================
// Utility Methods
// ============================================================================

bool CombatSimulator::isBattleOver() const
{
    return !m_state.attacker.hasTroops() || !m_state.defender.hasTroops();
}

int CombatSimulator::getWinner() const
{
    if (!isBattleOver()) return 0;

    if (m_state.attacker.hasTroops() && !m_state.defender.hasTroops()) {
        return 1;  // Attacker wins
    } else if (!m_state.attacker.hasTroops() && m_state.defender.hasTroops()) {
        return -1;  // Defender wins
    }
    return 0;  // Draw (both eliminated)
}

QString CombatSimulator::generateReport() const
{
    // TODO: Generate detailed battle report

    QString report;
    report += QString("Combat at %1\n").arg(m_state.terrain.territoryName);
    report += QString("═══════════════════════════════════════\n");
    report += QString("Attacker: %1 troops (%2 inf, %3 cav, %4 cat)\n")
        .arg(m_state.attacker.totalTroops())
        .arg(m_state.attacker.infantry)
        .arg(m_state.attacker.cavalry)
        .arg(m_state.attacker.catapults);
    report += QString("Defender: %1 troops (%2 inf, %3 cav, %4 cat)\n")
        .arg(m_state.defender.totalTroops())
        .arg(m_state.defender.infantry)
        .arg(m_state.defender.cavalry)
        .arg(m_state.defender.catapults);

    if (m_state.terrain.defenderHasFortifiedCity) {
        report += QString("Defender has FORTIFIED CITY (+2 advantage)\n");
    }

    return report;
}

void CombatSimulator::reset()
{
    m_state = m_initialState;
}

// ============================================================================
// Private Methods
// ============================================================================

bool CombatSimulator::isHit(int roll, int advantage) const
{
    // TODO: Implement hit determination based on game rules
    // Likely: roll + advantage >= threshold

    return false;  // Placeholder
}

QString CombatSimulator::selectCasualtyType(const ArmyComposition &army) const
{
    // TODO: Select which troop type dies
    // Game rules may have specific selection order or random weighted

    if (army.infantry > 0) return "Infantry";
    if (army.cavalry > 0) return "Cavalry";
    if (army.catapults > 0) return "Catapult";
    if (army.generals > 0) return "General";
    if (army.hasCaesar) return "Caesar";
    return "";
}

void CombatSimulator::applyCasualty(ArmyComposition &army, const QString &troopType)
{
    // TODO: Apply the casualty to the army

    if (troopType == "Infantry" && army.infantry > 0) {
        army.infantry--;
    } else if (troopType == "Cavalry" && army.cavalry > 0) {
        army.cavalry--;
    } else if (troopType == "Catapult" && army.catapults > 0) {
        army.catapults--;
    } else if (troopType == "General" && army.generals > 0) {
        army.generals--;
    } else if (troopType == "Caesar" && army.hasCaesar) {
        army.hasCaesar = false;
    }
}
