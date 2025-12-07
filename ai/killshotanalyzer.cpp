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
        report += "  (No enemy Caesar or home cities are reachable this turn)\n\n";
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

            QString typeStr = opp.isCaesarKill() ? "⚔️ CAESAR KILL" : "🏰 HOME CITY CAPTURE";
            report += QString("  Target #%1: %2 - Player %3 at %4\n")
                .arg(i + 1)
                .arg(typeStr)
                .arg(opp.targetPlayer->getId())
                .arg(opp.targetTerritory);
            report += QString("  ─────────────────────────────────────\n");
            report += QString("  Status:           %1\n").arg(status);
            report += QString("  Win Probability:  %1%\n").arg(int(opp.winProbability * 100));
            report += QString("  Our Force:        %1 troops (%2 inf, %3 cav, %4 cat)\n")
                .arg(opp.ourMaxForce)
                .arg(opp.infantryCount)
                .arg(opp.cavalryCount)
                .arg(opp.catapultCount);
            report += QString("  Enemy Defenders:  %1 troops (%2 inf, %3 cav, %4 cat)\n")
                .arg(opp.enemyDefenders)
                .arg(opp.enemyInfantryCount)
                .arg(opp.enemyCavalryCount)
                .arg(opp.enemyCatapultCount);
            report += QString("  Fortified City:   %1\n").arg(opp.enemyHasFortifiedCity ? "YES (walls)" : "No");
            if (opp.isCaesarKill()) {
                report += QString("  Caesar Location:  %1\n").arg(opp.caesarLocation);
            } else {
                report += QString("  Caesar Location:  %1 (NOT at home)\n").arg(opp.caesarLocation);
                report += QString("  Home City:        %1\n").arg(opp.homeCity);
            }
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
            report += QString("  Enemy Force:      %1 troops (%2 inf, %3 cav, %4 cat)\n")
                .arg(threat.enemyMaxForce)
                .arg(threat.enemyInfantryCount)
                .arg(threat.enemyCavalryCount)
                .arg(threat.enemyCatapultCount);
            report += QString("  Our Defense:      %1 troops (%2 inf, %3 cav, %4 cat)\n")
                .arg(threat.ourDefenders)
                .arg(threat.ourInfantryCount)
                .arg(threat.ourCavalryCount)
                .arg(threat.ourCatapultCount);
            report += QString("  Expected Enemy Losses: ~%1 troops\n").arg(int(threat.expectedEnemyCasualties));
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
        case KillShotAction::ActionType::ExecuteKillShot: {
            QString typeStr = action.opportunity.isCaesarKill() ? "CAESAR KILL" : "HOME CITY CAPTURE";
            report += QString("  ▶ EXECUTE %1\n").arg(typeStr);
            report += QString("    Target: Player %1 at %2\n")
                .arg(action.opportunity.targetPlayer->getId())
                .arg(action.opportunity.targetTerritory);
            if (!action.opportunity.isCaesarKill()) {
                report += QString("    (Caesar is at %1)\n").arg(action.opportunity.caesarLocation);
            }
            report += QString("    Win Probability: %1%\n")
                .arg(int(action.opportunity.winProbability * 100));
            report += QString("    Priority: %1/100\n").arg(action.priority);
            break;
        }

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
        // Caesar kills get higher priority than home city captures
        action.priority = bestOpportunity.isCaesarKill() ? 98 : 95;
        QString typeStr = bestOpportunity.isCaesarKill() ? "CAESAR KILL" : "HOME CITY CAPTURE";
        action.summary = QString("EXECUTE %1 on Player %2 at %3 (win prob: %4%)")
            .arg(typeStr)
            .arg(bestOpportunity.targetPlayer->getId())
            .arg(bestOpportunity.targetTerritory)
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
        // Caesar kills get higher priority than home city captures
        action.priority = bestOpportunity.isCaesarKill() ? 88 : 85;
        QString typeStr = bestOpportunity.isCaesarKill() ? "Caesar kill" : "Home city capture";
        action.summary = QString("%1 opportunity on Player %2 at %3 (win prob: %4%)")
            .arg(typeStr)
            .arg(bestOpportunity.targetPlayer->getId())
            .arg(bestOpportunity.targetTerritory)
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

    // Check each enemy player
    for (Player *enemy : allPlayers) {
        if (enemy == player) continue;
        if (enemy->getCaesars().isEmpty()) continue;  // Already eliminated

        QString enemyHome = enemy->getHomeProvinceName();

        // Find Caesar's current location
        QString caesarLocation;
        for (CaesarPiece *caesar : enemy->getCaesars()) {
            if (caesar) {
                caesarLocation = caesar->getTerritoryName();
                break;  // Only one Caesar per player
            }
        }

        if (caesarLocation.isEmpty()) continue;

        // === OPPORTUNITY 1: Kill Caesar (wherever he is) ===
        {
            KillShotOpportunity opp;
            opp.type = KillShotType::CaesarKill;
            opp.targetPlayer = enemy;
            opp.targetTerritory = caesarLocation;
            opp.caesarLocation = caesarLocation;
            opp.homeCity = enemyHome;
            opp.caesarAtTarget = true;  // By definition, Caesar is at target

            // Calculate max force we can bring using heat map
            ReachabilityBreakdown breakdown;
            opp.ourMaxForce = calculateMaxConcentration(caesarLocation, player, allPlayers, graph, opp.generalsToUse, &breakdown);

            // If no generals can reach, skip this opportunity
            if (!opp.generalsToUse.isEmpty()) {
                // Count enemy defenders at Caesar's location
                opp.enemyDefenders = countTroopsAt(caesarLocation, enemy);
                opp.enemyHasFortifiedCity = hasFortifiedCityAt(caesarLocation, enemy);

                // Store unit breakdowns
                opp.infantryCount = breakdown.infantry;
                opp.cavalryCount = breakdown.cavalry;
                opp.catapultCount = breakdown.catapults;

                ReachabilityBreakdown defenderBreakdown = countDefenderBreakdown(caesarLocation, enemy);
                opp.enemyInfantryCount = defenderBreakdown.infantry;
                opp.enemyCavalryCount = defenderBreakdown.cavalry;
                opp.enemyCatapultCount = defenderBreakdown.catapults;

                // Run Monte Carlo simulation
                CombatProbability combatResult = simulateCombat(breakdown, defenderBreakdown, opp.enemyHasFortifiedCity);

                opp.winProbability = combatResult.attackerWinChance;
                opp.expectedCasualties = combatResult.expectedAttackerCasualties;
                opp.turnsToReach = 1;

                opp.reason = QString("CAESAR KILL at %1: %2 troops (%3i/%4c/%5cat) vs %6 defenders (%7i/%8c/%9cat)%10")
                    .arg(caesarLocation)
                    .arg(opp.ourMaxForce)
                    .arg(opp.infantryCount).arg(opp.cavalryCount).arg(opp.catapultCount)
                    .arg(opp.enemyDefenders)
                    .arg(opp.enemyInfantryCount).arg(opp.enemyCavalryCount).arg(opp.enemyCatapultCount)
                    .arg(opp.enemyHasFortifiedCity ? " [FORTIFIED]" : "");

                qDebug() << "Caesar kill opportunity vs Player" << enemy->getId()
                         << "at" << caesarLocation << ":" << opp.ourMaxForce << "vs" << opp.enemyDefenders
                         << "win prob:" << int(opp.winProbability * 100) << "%";

                opportunities.append(opp);
            }
        }

        // === OPPORTUNITY 2: Capture Home City (if different from Caesar location) ===
        if (!enemyHome.isEmpty() && enemyHome != caesarLocation) {
            KillShotOpportunity opp;
            opp.type = KillShotType::HomeCityCapture;
            opp.targetPlayer = enemy;
            opp.targetTerritory = enemyHome;
            opp.caesarLocation = caesarLocation;
            opp.homeCity = enemyHome;
            opp.caesarAtTarget = false;  // Caesar is elsewhere

            // Calculate max force we can bring using heat map
            ReachabilityBreakdown breakdown;
            opp.ourMaxForce = calculateMaxConcentration(enemyHome, player, allPlayers, graph, opp.generalsToUse, &breakdown);

            // If no generals can reach, skip this opportunity
            if (!opp.generalsToUse.isEmpty()) {
                // Count enemy defenders at home city
                opp.enemyDefenders = countTroopsAt(enemyHome, enemy);
                opp.enemyHasFortifiedCity = hasFortifiedCityAt(enemyHome, enemy);

                // Store unit breakdowns
                opp.infantryCount = breakdown.infantry;
                opp.cavalryCount = breakdown.cavalry;
                opp.catapultCount = breakdown.catapults;

                ReachabilityBreakdown defenderBreakdown = countDefenderBreakdown(enemyHome, enemy);
                opp.enemyInfantryCount = defenderBreakdown.infantry;
                opp.enemyCavalryCount = defenderBreakdown.cavalry;
                opp.enemyCatapultCount = defenderBreakdown.catapults;

                // Run Monte Carlo simulation
                CombatProbability combatResult = simulateCombat(breakdown, defenderBreakdown, opp.enemyHasFortifiedCity);

                opp.winProbability = combatResult.attackerWinChance;
                opp.expectedCasualties = combatResult.expectedAttackerCasualties;
                opp.turnsToReach = 1;

                opp.reason = QString("HOME CITY CAPTURE at %1: %2 troops (%3i/%4c/%5cat) vs %6 defenders (%7i/%8c/%9cat)%10")
                    .arg(enemyHome)
                    .arg(opp.ourMaxForce)
                    .arg(opp.infantryCount).arg(opp.cavalryCount).arg(opp.catapultCount)
                    .arg(opp.enemyDefenders)
                    .arg(opp.enemyInfantryCount).arg(opp.enemyCavalryCount).arg(opp.enemyCatapultCount)
                    .arg(opp.enemyHasFortifiedCity ? " [FORTIFIED]" : "");

                qDebug() << "Home city capture opportunity vs Player" << enemy->getId()
                         << "at" << enemyHome << ":" << opp.ourMaxForce << "vs" << opp.enemyDefenders
                         << "win prob:" << int(opp.winProbability * 100) << "%";

                opportunities.append(opp);
            }
        }
    }

    // Sort by: 1) Caesar kills first, 2) then by win probability
    std::sort(opportunities.begin(), opportunities.end(),
              [](const KillShotOpportunity &a, const KillShotOpportunity &b) {
                  // Caesar kills take priority over home city captures at similar win rates
                  if (a.isCaesarKill() != b.isCaesarKill()) {
                      // If win probabilities are close (within 15%), prefer Caesar kill
                      if (std::abs(a.winProbability - b.winProbability) < 0.15) {
                          return a.isCaesarKill();
                      }
                  }
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
    ReachabilityBreakdown ourBreakdown = countDefenderBreakdown(ourHome, player);
    int ourDefenders = ourBreakdown.total();
    bool weHaveWalls = hasFortifiedCityAt(ourHome, player);
    bool caesarHome = hasCaesarAt(ourHome, player);

    MoveEnumerator enumerator;

    // Check each enemy's ability to attack our home
    for (Player *enemy : allPlayers) {
        if (enemy == player) continue;
        if (enemy->getCaesars().isEmpty()) continue;  // Already eliminated

        // Use MoveEnumerator to get accurate enemy force projection with unit breakdown
        TurnMoveEnumeration moves = enumerator.enumerateAllMoves(enemy, allPlayers, graph);
        QMap<QString, ReachabilityBreakdown> reachabilityMap = moves.getReachabilityBreakdown();

        if (!reachabilityMap.contains(ourHome)) continue;

        ReachabilityBreakdown enemyBreakdown = reachabilityMap[ourHome];
        int enemyForce = enemyBreakdown.total();

        if (enemyForce == 0) continue;

        KillShotThreat threat;
        threat.threateningPlayer = enemy;
        threat.enemyMaxForce = enemyForce;
        threat.ourDefenders = ourDefenders;
        threat.weHaveFortifiedCity = weHaveWalls;
        threat.caesarAtHome = caesarHome;
        threat.turnsUntilThreat = 1;  // Assuming immediate threat

        // Store unit breakdowns
        threat.enemyInfantryCount = enemyBreakdown.infantry;
        threat.enemyCavalryCount = enemyBreakdown.cavalry;
        threat.enemyCatapultCount = enemyBreakdown.catapults;
        threat.ourInfantryCount = ourBreakdown.infantry;
        threat.ourCavalryCount = ourBreakdown.cavalry;
        threat.ourCatapultCount = ourBreakdown.catapults;

        // Run Monte Carlo simulation (from enemy's perspective as attacker)
        CombatProbability combatResult = simulateCombat(enemyBreakdown, ourBreakdown, weHaveWalls);

        threat.enemyWinProbability = combatResult.attackerWinChance;
        threat.expectedEnemyCasualties = combatResult.expectedAttackerCasualties;

        threat.reason = QString("Player %1 can attack with %2 troops (%3i/%4c/%5cat) vs our %6 defenders (%7i/%8c/%9cat)%10")
            .arg(enemy->getId())
            .arg(enemyForce)
            .arg(enemyBreakdown.infantry)
            .arg(enemyBreakdown.cavalry)
            .arg(enemyBreakdown.catapults)
            .arg(ourDefenders)
            .arg(ourBreakdown.infantry)
            .arg(ourBreakdown.cavalry)
            .arg(ourBreakdown.catapults)
            .arg(weHaveWalls ? " [FORTIFIED]" : "");

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

CombatProbability KillShotAnalyzer::simulateCombat(
    const ReachabilityBreakdown &attackerBreakdown,
    const ReachabilityBreakdown &defenderBreakdown,
    bool defenderHasFortifiedCity)
{
    // Build army compositions from breakdowns
    ArmyComposition attacker;
    attacker.infantry = attackerBreakdown.infantry;
    attacker.cavalry = attackerBreakdown.cavalry;
    attacker.catapults = attackerBreakdown.catapults;
    attacker.galleys = attackerBreakdown.galleys;

    ArmyComposition defender;
    defender.infantry = defenderBreakdown.infantry;
    defender.cavalry = defenderBreakdown.cavalry;
    defender.catapults = defenderBreakdown.catapults;
    defender.galleys = defenderBreakdown.galleys;

    // Set up terrain
    CombatTerrain terrain;
    terrain.defenderHasFortifiedCity = defenderHasFortifiedCity;
    terrain.isSeaCombat = false;  // Kill shots are always land-based

    // Run Monte Carlo simulation
    CombatSimulator simulator;
    simulator.initializeBattle(attacker, defender, terrain);
    CombatProbability result = simulator.calculateWinProbability(1000);

    qDebug() << "  Combat simulation:"
             << attacker.totalTroops() << "troops (" << attacker.infantry << "inf,"
             << attacker.cavalry << "cav," << attacker.catapults << "cat) vs"
             << defender.totalTroops() << "troops (" << defender.infantry << "inf,"
             << defender.cavalry << "cav," << defender.catapults << "cat)"
             << (defenderHasFortifiedCity ? "[FORTIFIED]" : "")
             << "-> Attacker win:" << int(result.attackerWinChance * 100) << "%";

    return result;
}

ReachabilityBreakdown KillShotAnalyzer::countDefenderBreakdown(const QString &territory, Player *player)
{
    ReachabilityBreakdown breakdown;

    for (InfantryPiece *inf : player->getInfantry()) {
        if (inf->getTerritoryName() == territory) breakdown.infantry++;
    }
    for (CavalryPiece *cav : player->getCavalry()) {
        if (cav->getTerritoryName() == territory) breakdown.cavalry++;
    }
    for (CatapultPiece *cat : player->getCatapults()) {
        if (cat->getTerritoryName() == territory) breakdown.catapults++;
    }

    return breakdown;
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
