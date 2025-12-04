#include "killshotanalyzer.h"
#include "../player.h"
#include "../gamepiece.h"
#include "../mapgraph.h"
#include "../building.h"
#include <QDebug>
#include <algorithm>

KillShotAnalyzer::KillShotAnalyzer()
{
}

QString KillShotAnalyzer::generateReport(Player *player, const QList<Player*> &allPlayers, MapGraph *graph)
{
    QString report;

    if (!player || !graph || allPlayers.isEmpty()) {
        return "Error: Invalid parameters for kill shot analysis.\n";
    }

    report += QString("═══════════════════════════════════════════════════════════════\n");
    report += QString("           KILL SHOT ANALYSIS - Player %1\n").arg(player->getId());
    report += QString("═══════════════════════════════════════════════════════════════\n\n");

    // === OFFENSIVE OPPORTUNITIES ===
    report += QString("┌─────────────────────────────────────────────────────────────┐\n");
    report += QString("│              OFFENSIVE OPPORTUNITIES                        │\n");
    report += QString("└─────────────────────────────────────────────────────────────┘\n\n");

    QList<KillShotOpportunity> opportunities = findOffensiveOpportunities(player, allPlayers, graph);

    if (opportunities.isEmpty()) {
        report += "  No kill shot opportunities detected.\n";
        report += "  (No enemy home cities are reachable this turn)\n\n";
    } else {
        for (int i = 0; i < opportunities.size(); ++i) {
            const KillShotOpportunity &opp = opportunities[i];

            QString status;
            if (opp.isOverwhelming()) {
                status = "★★★ OVERWHELMING";
            } else if (opp.isHighConfidence()) {
                status = "★★  HIGH CONFIDENCE";
            } else if (opp.isViable()) {
                status = "★   VIABLE";
            } else {
                status = "    LOW ODDS";
            }

            report += QString("  Target #%1: Player %2 at %3\n")
                .arg(i + 1)
                .arg(opp.targetPlayer->getId())
                .arg(opp.targetHomeCity);
            report += QString("  ─────────────────────────────────────\n");
            report += QString("  Status:           %1\n").arg(status);
            report += QString("  Win Probability:  %1%\n").arg(int(opp.winProbability * 100));
            report += QString("  Our Force:        %1 troops (%2 inf, %3 cav, %4 cat)\n")
                .arg(opp.ourMaxForce)
                .arg(opp.infantryCount)
                .arg(opp.cavalryCount)
                .arg(opp.catapultCount);
            report += QString("  Enemy Defenders:  %1 troops\n").arg(opp.enemyDefenders);
            report += QString("  Fortified City:   %1\n").arg(opp.enemyHasFortifiedCity ? "YES (walls)" : "No");
            report += QString("  Caesar Present:   %1\n").arg(opp.enemyHasCaesar ? "YES" : "No");
            report += QString("  Generals to use:  %1\n").arg(opp.generalsToUse.size());
            // List the generals and their origins
            for (GamePiece *piece : opp.generalsToUse) {
                QString leaderName;
                if (piece->getType() == GamePiece::Type::Caesar) {
                    leaderName = "Caesar";
                } else if (piece->getType() == GamePiece::Type::General) {
                    leaderName = QString("General #%1").arg(static_cast<GeneralPiece*>(piece)->getNumber());
                }
                report += QString("                    - %1 from %2\n")
                    .arg(leaderName)
                    .arg(piece->getTerritoryName());
            }
            report += QString("  Expected Losses:  ~%1 troops\n").arg(int(opp.expectedCasualties));
            report += "\n";
        }
    }

    // === DEFENSIVE THREATS ===
    report += QString("┌─────────────────────────────────────────────────────────────┐\n");
    report += QString("│              DEFENSIVE THREATS                              │\n");
    report += QString("└─────────────────────────────────────────────────────────────┘\n\n");

    QString ourHome = player->getHomeProvinceName();
    int ourDefenders = countTroopsAt(ourHome, player);
    bool weHaveWalls = hasFortifiedCityAt(ourHome, player);

    report += QString("  Our Home: %1\n").arg(ourHome);
    report += QString("  Our Defenders: %1 troops\n").arg(ourDefenders);
    report += QString("  Fortified: %1\n\n").arg(weHaveWalls ? "YES (walls)" : "No");

    QList<KillShotThreat> threats = findDefensiveThreats(player, allPlayers, graph);

    if (threats.isEmpty()) {
        report += "  No immediate threats detected.\n";
        report += "  (No enemy forces can reach our home this turn)\n\n";
    } else {
        for (int i = 0; i < threats.size(); ++i) {
            const KillShotThreat &threat = threats[i];

            QString severity;
            if (threat.isUrgent()) {
                severity = "🔴 URGENT - DEFEND NOW!";
            } else if (threat.isCritical()) {
                severity = "🟠 CRITICAL";
            } else {
                severity = "🟡 MONITOR";
            }

            report += QString("  Threat #%1: Player %2\n")
                .arg(i + 1)
                .arg(threat.threateningPlayer->getId());
            report += QString("  ─────────────────────────────────────\n");
            report += QString("  Severity:         %1\n").arg(severity);
            report += QString("  Their Win Prob:   %1%\n").arg(int(threat.enemyWinProbability * 100));
            report += QString("  Enemy Force:      %1 troops\n").arg(threat.enemyMaxForce);
            report += QString("  Turns Away:       %1\n").arg(threat.turnsUntilThreat);
            report += "\n";
        }
    }

    // === RECOMMENDATION ===
    report += QString("┌─────────────────────────────────────────────────────────────┐\n");
    report += QString("│              RECOMMENDATION                                 │\n");
    report += QString("└─────────────────────────────────────────────────────────────┘\n\n");

    KillShotAction action = analyze(player, allPlayers, graph);

    switch (action.type) {
        case KillShotAction::ActionType::ExecuteKillShot:
            report += QString("  ▶ EXECUTE KILL SHOT\n");
            report += QString("    Target: Player %1 at %2\n")
                .arg(action.opportunity.targetPlayer->getId())
                .arg(action.opportunity.targetHomeCity);
            report += QString("    Win Probability: %1%\n")
                .arg(int(action.opportunity.winProbability * 100));
            report += QString("    Priority: %1/100\n").arg(action.priority);
            break;

        case KillShotAction::ActionType::DefendHome:
            report += QString("  ▶ DEFEND HOME\n");
            report += QString("    Threat from: Player %1\n")
                .arg(action.threat.threateningPlayer->getId());
            report += QString("    Their Win Probability: %1%\n")
                .arg(int(action.threat.enemyWinProbability * 100));
            report += QString("    Priority: %1/100\n").arg(action.priority);
            break;

        case KillShotAction::ActionType::PreemptiveStrike:
            report += QString("  ▶ PREEMPTIVE STRIKE\n");
            report += QString("    Strike Player %1 before they attack us\n")
                .arg(action.opportunity.targetPlayer->getId());
            report += QString("    Priority: %1/100\n").arg(action.priority);
            break;

        case KillShotAction::ActionType::Retreat:
            report += QString("  ▶ RETREAT\n");
            report += QString("    Pull forces back to defend home\n");
            break;

        case KillShotAction::ActionType::None:
        default:
            report += QString("  ▶ NO ACTION NEEDED\n");
            report += QString("    Proceed with normal AI movement\n");
            break;
    }

    report += QString("\n═══════════════════════════════════════════════════════════════\n");

    return report;
}

KillShotAction KillShotAnalyzer::analyze(Player *player, const QList<Player*> &allPlayers, MapGraph *graph)
{
    KillShotAction action;
    action.type = KillShotAction::ActionType::None;
    action.priority = 0;

    if (!player || !graph || allPlayers.isEmpty()) {
        return action;
    }

    qDebug() << "=== KILL SHOT ANALYSIS for Player" << player->getId() << "===";

    // First, check defensive threats (survival is priority #1)
    QList<KillShotThreat> threats = findDefensiveThreats(player, allPlayers, graph);

    KillShotThreat worstThreat;
    for (const KillShotThreat &threat : threats) {
        if (threat.enemyWinProbability > worstThreat.enemyWinProbability) {
            worstThreat = threat;
        }
    }

    // Check offensive opportunities
    QList<KillShotOpportunity> opportunities = findOffensiveOpportunities(player, allPlayers, graph);

    KillShotOpportunity bestOpportunity;
    for (const KillShotOpportunity &opp : opportunities) {
        if (opp.winProbability > bestOpportunity.winProbability) {
            bestOpportunity = opp;
        }
    }

    // Decision logic: balance offense vs defense
    bool underSeriousThreat = worstThreat.isCritical() && worstThreat.turnsUntilThreat <= 1;
    bool haveGoodOpportunity = bestOpportunity.isHighConfidence();
    bool haveOverwhelmingOpportunity = bestOpportunity.isOverwhelming();

    // If we have an overwhelming kill shot AND aren't about to die, take it!
    if (haveOverwhelmingOpportunity && !worstThreat.isUrgent()) {
        action.type = KillShotAction::ActionType::ExecuteKillShot;
        action.opportunity = bestOpportunity;
        action.priority = 95;  // Very high priority
        action.summary = QString("EXECUTE KILL SHOT on Player %1 (win prob: %2%)")
            .arg(bestOpportunity.targetPlayer->getId())
            .arg(int(bestOpportunity.winProbability * 100));
        qDebug() << ">>> KILL SHOT RECOMMENDED:" << action.summary;
        return action;
    }

    // If we're under urgent threat, prioritize defense
    if (worstThreat.isUrgent()) {
        action.type = KillShotAction::ActionType::DefendHome;
        action.threat = worstThreat;
        action.priority = 100;  // Maximum priority - survival!
        action.summary = QString("URGENT: Defend home against Player %1 (their win prob: %2%)")
            .arg(worstThreat.threateningPlayer->getId())
            .arg(int(worstThreat.enemyWinProbability * 100));
        qDebug() << ">>> DEFENSE RECOMMENDED:" << action.summary;
        return action;
    }

    // If we have a good (but not overwhelming) kill shot and low threat
    if (haveGoodOpportunity && !underSeriousThreat) {
        action.type = KillShotAction::ActionType::ExecuteKillShot;
        action.opportunity = bestOpportunity;
        action.priority = 85;
        action.summary = QString("Kill shot opportunity on Player %1 (win prob: %2%)")
            .arg(bestOpportunity.targetPlayer->getId())
            .arg(int(bestOpportunity.winProbability * 100));
        qDebug() << ">>> KILL SHOT OPPORTUNITY:" << action.summary;
        return action;
    }

    // If under moderate threat, consider preemptive strike or defense
    if (underSeriousThreat) {
        // Check if we can strike the threatening player first
        for (const KillShotOpportunity &opp : opportunities) {
            if (opp.targetPlayer == worstThreat.threateningPlayer && opp.isViable()) {
                action.type = KillShotAction::ActionType::PreemptiveStrike;
                action.opportunity = opp;
                action.threat = worstThreat;
                action.priority = 90;
                action.summary = QString("Preemptive strike on threatening Player %1")
                    .arg(opp.targetPlayer->getId());
                qDebug() << ">>> PREEMPTIVE STRIKE RECOMMENDED:" << action.summary;
                return action;
            }
        }

        // Otherwise, defend
        action.type = KillShotAction::ActionType::DefendHome;
        action.threat = worstThreat;
        action.priority = 80;
        action.summary = QString("Reinforce home against Player %1 threat")
            .arg(worstThreat.threateningPlayer->getId());
        qDebug() << ">>> REINFORCE HOME:" << action.summary;
        return action;
    }

    qDebug() << ">>> No kill shot action needed - proceed with normal AI";
    return action;
}

QList<KillShotOpportunity> KillShotAnalyzer::findOffensiveOpportunities(
    Player *player,
    const QList<Player*> &allPlayers,
    MapGraph *graph)
{
    QList<KillShotOpportunity> opportunities;

    if (!player || !graph) {
        return opportunities;
    }

    ReachabilityCalculator calc;

    // Check each enemy player's home city
    for (Player *enemy : allPlayers) {
        if (enemy == player) continue;
        if (enemy->getCaesars().isEmpty()) continue;  // Already eliminated

        QString enemyHome = enemy->getHomeProvinceName();
        if (enemyHome.isEmpty()) continue;

        KillShotOpportunity opp;
        opp.targetPlayer = enemy;
        opp.targetHomeCity = enemyHome;

        // Calculate max force we can bring using heat map
        ReachabilityBreakdown breakdown;
        opp.ourMaxForce = calculateMaxConcentration(enemyHome, player, allPlayers, graph, opp.generalsToUse, &breakdown);

        // If no generals can reach, skip
        if (opp.generalsToUse.isEmpty()) {
            continue;
        }

        // Count enemy defenders
        opp.enemyDefenders = countTroopsAt(enemyHome, enemy);
        opp.enemyHasFortifiedCity = hasFortifiedCityAt(enemyHome, enemy);
        opp.enemyHasCaesar = hasCaesarAt(enemyHome, enemy);

        // Store unit breakdown from heat map
        opp.infantryCount = breakdown.infantry;
        opp.cavalryCount = breakdown.cavalry;
        opp.catapultCount = breakdown.catapults;

        // Use catapults from heat map breakdown (accurately counts those that can reach)
        int catapults = breakdown.catapults;

        // Estimate win probability
        opp.winProbability = estimateWinProbability(
            opp.ourMaxForce,
            opp.enemyDefenders,
            opp.enemyHasFortifiedCity,
            catapults);

        opp.expectedCasualties = estimateCasualties(
            opp.ourMaxForce,
            opp.enemyDefenders,
            opp.enemyHasFortifiedCity);

        opp.turnsToReach = 1;  // Assuming this turn for now

        opp.reason = QString("Home city attack: %1 troops vs %2 defenders%3")
            .arg(opp.ourMaxForce)
            .arg(opp.enemyDefenders)
            .arg(opp.enemyHasFortifiedCity ? " (WALLED)" : "");

        qDebug() << "Kill shot opportunity vs Player" << enemy->getId()
                 << "at" << enemyHome << ":" << opp.ourMaxForce << "vs" << opp.enemyDefenders
                 << "win prob:" << int(opp.winProbability * 100) << "%";

        opportunities.append(opp);
    }

    // Sort by win probability (highest first)
    std::sort(opportunities.begin(), opportunities.end(),
              [](const KillShotOpportunity &a, const KillShotOpportunity &b) {
                  return a.winProbability > b.winProbability;
              });

    return opportunities;
}

QList<KillShotThreat> KillShotAnalyzer::findDefensiveThreats(
    Player *player,
    const QList<Player*> &allPlayers,
    MapGraph *graph)
{
    QList<KillShotThreat> threats;

    if (!player || !graph) {
        return threats;
    }

    QString ourHome = player->getHomeProvinceName();
    if (ourHome.isEmpty()) {
        return threats;
    }

    // Our defensive situation
    int ourDefenders = countTroopsAt(ourHome, player);
    bool weHaveWalls = hasFortifiedCityAt(ourHome, player);
    bool caesarHome = hasCaesarAt(ourHome, player);

    // Check each enemy's ability to attack our home
    for (Player *enemy : allPlayers) {
        if (enemy == player) continue;
        if (enemy->getCaesars().isEmpty()) continue;  // Already eliminated

        Player *threatSource = nullptr;
        int enemyForce = calculateEnemyMaxForce(ourHome, player, {enemy}, graph, &threatSource);

        if (enemyForce == 0) continue;

        KillShotThreat threat;
        threat.threateningPlayer = enemy;
        threat.enemyMaxForce = enemyForce;
        threat.ourDefenders = ourDefenders;
        threat.weHaveFortifiedCity = weHaveWalls;
        threat.caesarAtHome = caesarHome;
        threat.turnsUntilThreat = 1;  // Assuming immediate threat

        // Calculate their win probability (from their perspective)
        threat.enemyWinProbability = estimateWinProbability(
            enemyForce,
            ourDefenders,
            weHaveWalls,
            0);  // Assume they have no catapults for now

        threat.reason = QString("Player %1 can attack with %2 troops vs our %3 defenders%4")
            .arg(enemy->getId())
            .arg(enemyForce)
            .arg(ourDefenders)
            .arg(weHaveWalls ? " (we have walls)" : "");

        qDebug() << "Kill shot threat from Player" << enemy->getId()
                 << ":" << enemyForce << "vs" << ourDefenders
                 << "their win prob:" << int(threat.enemyWinProbability * 100) << "%";

        threats.append(threat);
    }

    // Sort by enemy win probability (highest threat first)
    std::sort(threats.begin(), threats.end(),
              [](const KillShotThreat &a, const KillShotThreat &b) {
                  return a.enemyWinProbability > b.enemyWinProbability;
              });

    return threats;
}

int KillShotAnalyzer::calculateMaxConcentration(
    const QString &targetTerritory,
    Player *player,
    const QList<Player*> &allPlayers,
    MapGraph *graph,
    QList<GamePiece*> &outGenerals,
    ReachabilityBreakdown *outBreakdown)
{
    outGenerals.clear();
    int totalForce = 0;

    if (!player || !graph) {
        return 0;
    }

    // Use MoveEnumerator for accurate force projection (heat map)
    MoveEnumerator enumerator;
    TurnMoveEnumeration moves = enumerator.enumerateAllMoves(player, allPlayers, graph);

    // Get the reachability breakdown for all territories
    QMap<QString, ReachabilityBreakdown> reachabilityMap = moves.getReachabilityBreakdown();

    if (reachabilityMap.contains(targetTerritory)) {
        ReachabilityBreakdown breakdown = reachabilityMap[targetTerritory];
        totalForce = breakdown.total();  // infantry + cavalry + catapults

        if (outBreakdown) {
            *outBreakdown = breakdown;
        }

        qDebug() << "  Kill shot heat map for" << targetTerritory << ":"
                 << breakdown.infantry << "inf," << breakdown.cavalry << "cav,"
                 << breakdown.catapults << "cat," << breakdown.galleys << "galleys"
                 << "= total" << totalForce;
    }

    // Also determine which generals can reach the target
    // (for the report - we still need to know WHO is attacking)
    ReachabilityCalculator calc;

    for (CaesarPiece *caesar : player->getCaesars()) {
        if (caesar->getMovesRemaining() < 1.0) continue;
        QMap<QString, ReachInfo> reachable = calc.getReachableFrom(caesar, graph, player);
        if (reachable.contains(targetTerritory)) {
            outGenerals.append(caesar);
        }
    }

    for (GeneralPiece *general : player->getGenerals()) {
        if (general->getMovesRemaining() < 1.0) continue;
        QMap<QString, ReachInfo> reachable = calc.getReachableFrom(general, graph, player);
        if (reachable.contains(targetTerritory)) {
            outGenerals.append(general);
        }
    }

    return totalForce;
}

int KillShotAnalyzer::calculateEnemyMaxForce(
    const QString &territory,
    Player *us,
    const QList<Player*> &allPlayers,
    MapGraph *graph,
    Player **outThreateningPlayer,
    ReachabilityBreakdown *outBreakdown)
{
    int maxForce = 0;
    ReachabilityBreakdown maxBreakdown;

    MoveEnumerator enumerator;

    for (Player *enemy : allPlayers) {
        if (enemy == us) continue;

        // Use MoveEnumerator for accurate enemy force projection
        TurnMoveEnumeration moves = enumerator.enumerateAllMoves(enemy, allPlayers, graph);
        QMap<QString, ReachabilityBreakdown> reachabilityMap = moves.getReachabilityBreakdown();

        if (reachabilityMap.contains(territory)) {
            ReachabilityBreakdown breakdown = reachabilityMap[territory];
            int enemyForce = breakdown.total();

            if (enemyForce > maxForce) {
                maxForce = enemyForce;
                maxBreakdown = breakdown;
                if (outThreateningPlayer) {
                    *outThreateningPlayer = enemy;
                }
            }
        }
    }

    if (outBreakdown) {
        *outBreakdown = maxBreakdown;
    }

    return maxForce;
}

double KillShotAnalyzer::estimateWinProbability(
    int attackerTroops,
    int defenderTroops,
    bool defenderHasWalls,
    int attackerCatapults)
{
    /*
     * TODO: Replace this with proper CombatSimulator Monte Carlo simulation
     *
     * This is a placeholder estimation based on simple force ratios.
     * The actual combat system uses dice rolls and is more nuanced.
     *
     * Rough heuristics used here:
     * - Walls give defender +2 effective troops
     * - Catapults can negate walls
     * - Force advantage correlates with win probability
     */

    if (attackerTroops == 0) {
        return 0.0;
    }

    // Adjust for walls
    int effectiveDefenders = defenderTroops;
    if (defenderHasWalls) {
        // Walls help a lot, but catapults can counter
        int wallBonus = 3;  // Walls are worth ~3 troops
        wallBonus -= attackerCatapults;  // Each catapult reduces wall effectiveness
        if (wallBonus < 0) wallBonus = 0;
        effectiveDefenders += wallBonus;
    }

    // If defender has no troops, attacker wins automatically
    if (effectiveDefenders == 0) {
        return 1.0;
    }

    // Calculate force ratio
    double ratio = (double)attackerTroops / (double)effectiveDefenders;

    // Convert ratio to probability
    // These are rough estimates - real combat is more random
    if (ratio >= 3.0) return 0.95;      // 3:1 advantage - near certain
    if (ratio >= 2.5) return 0.90;
    if (ratio >= 2.0) return 0.85;      // 2:1 advantage - very good
    if (ratio >= 1.75) return 0.75;
    if (ratio >= 1.5) return 0.65;      // 1.5:1 - decent odds
    if (ratio >= 1.25) return 0.55;
    if (ratio >= 1.0) return 0.45;      // Even odds - slight defender advantage
    if (ratio >= 0.75) return 0.30;
    if (ratio >= 0.5) return 0.15;      // 1:2 disadvantage - poor odds
    return 0.05;                         // Worse than 1:2 - very unlikely
}

double KillShotAnalyzer::estimateCasualties(
    int attackerTroops,
    int defenderTroops,
    bool defenderHasWalls)
{
    /*
     * TODO: Replace with CombatSimulator
     *
     * Rough estimate: expect to lose troops proportional to enemy strength
     */

    if (defenderTroops == 0) {
        return 0.0;
    }

    // Base casualty rate
    double casualtyRate = 0.5;  // Expect to lose ~50% of what defender has

    if (defenderHasWalls) {
        casualtyRate = 0.7;  // Walls increase attacker casualties
    }

    return defenderTroops * casualtyRate;
}

int KillShotAnalyzer::countTroopsAt(const QString &territory, Player *player)
{
    int count = 0;

    for (InfantryPiece *inf : player->getInfantry()) {
        if (inf->getTerritoryName() == territory) count++;
    }
    for (CavalryPiece *cav : player->getCavalry()) {
        if (cav->getTerritoryName() == territory) count++;
    }
    for (CatapultPiece *cat : player->getCatapults()) {
        if (cat->getTerritoryName() == territory) count++;
    }

    return count;
}

bool KillShotAnalyzer::hasFortifiedCityAt(const QString &territory, Player *player)
{
    City *city = player->getCityAtTerritory(territory);
    return city && city->isFortified();
}

bool KillShotAnalyzer::hasCaesarAt(const QString &territory, Player *player)
{
    for (CaesarPiece *caesar : player->getCaesars()) {
        if (caesar->getTerritoryName() == territory) {
            return true;
        }
    }
    return false;
}
