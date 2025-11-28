#include "aidecisionmaker.h"
#include "../gamepiece.h"
#include "../player.h"
#include "../mapgraph.h"
#include "../building.h"
#include <algorithm>
#include <QDebug>

AIDecisionMaker::AIDecisionMaker()
{
}

ScoredMove AIDecisionMaker::getBestMove(Player *player, const QList<Player*> &allPlayers, MapGraph *graph)
{
    QList<ScoredMove> allMoves = getAllScoredMoves(player, allPlayers, graph);

    if (allMoves.isEmpty()) {
        qDebug() << "AIDecisionMaker: No valid moves found for player" << player->getId();
        return ScoredMove();  // No valid moves
    }

    // Debug: Log top 5 moves
    qDebug() << "AIDecisionMaker: Found" << allMoves.size() << "valid moves. Top moves:";
    for (int i = 0; i < qMin(5, allMoves.size()); ++i) {
        const ScoredMove &m = allMoves[i];
        QString leaderType;
        if (m.leader->getType() == GamePiece::Type::Caesar) {
            leaderType = "Caesar";
        } else if (m.leader->getType() == GamePiece::Type::General) {
            leaderType = QString("General #%1").arg(static_cast<GeneralPiece*>(m.leader)->getNumber());
        } else {
            leaderType = "Galley";
        }
        qDebug() << QString("  %1. %2: %3 -> %4 (score=%5, troops=%6) - %7")
            .arg(i + 1)
            .arg(leaderType)
            .arg(m.leader->getTerritoryName())
            .arg(m.destination)
            .arg(m.score)
            .arg(m.troopsCanBring)
            .arg(m.reason);
    }

    return allMoves.first();  // Already sorted, highest score first
}

QList<ScoredMove> AIDecisionMaker::getAllScoredMoves(Player *player, const QList<Player*> &allPlayers, MapGraph *graph)
{
    QList<ScoredMove> allMoves;

    if (!player || !graph) {
        qDebug() << "AIDecisionMaker: Invalid player or graph";
        return allMoves;
    }

    if (allPlayers.isEmpty()) {
        qDebug() << "AIDecisionMaker: Empty player list";
        return allMoves;
    }

    qDebug() << "AIDecisionMaker: Getting scored moves for player" << player->getId()
             << "with" << player->getGenerals().size() << "generals";

    // Get risk assessment for all territories
    ReachabilityCalculator calc;
    QMap<QString, TerritoryRisk> riskMap = calc.assessAllTerritories(player, allPlayers, graph);

    // Helper to check if a leader has moves remaining
    auto hasMovesRemaining = [](GamePiece *piece) -> bool {
        return piece && piece->getMovesRemaining() >= 1.0;
    };

    // Process Caesars - BUT ONLY if desperate (very few generals left)
    // Caesar is EXTREMELY valuable - losing Caesar loses the game!
    // Enemy gets 100 gold bonus for capturing Caesar, so they WILL attack at equal strength.
    // Standard practice: Caesar stays in fortified city, only leads armies if most generals lost.
    const int MIN_GENERALS_BEFORE_CAESAR_MOVES = 2;
    int numGenerals = player->getGenerals().size();

    if (numGenerals < MIN_GENERALS_BEFORE_CAESAR_MOVES) {
        qDebug() << "AIDecisionMaker: DESPERATE MODE - only" << numGenerals
                 << "generals left, considering Caesar for movement";

        for (CaesarPiece *caesar : player->getCaesars()) {
            if (!hasMovesRemaining(caesar)) continue;

            QMap<QString, ReachInfo> reachable = calc.getReachableFrom(caesar, graph, player);
            for (auto it = reachable.begin(); it != reachable.end(); ++it) {
                TerritoryRisk risk;
                if (riskMap.contains(it.key())) {
                    risk = riskMap[it.key()];
                } else {
                    risk.territoryName = it.key();
                    risk.risk = RiskLevel::Safe;  // Not in risk map = probably safe
                }

                ScoredMove move = scoreMove(caesar, it.key(), it.value(), risk, player, allPlayers, graph, riskMap);
                if (move.isValid() && move.score > -9000) {
                    allMoves.append(move);
                }
            }
        }
    } else {
        qDebug() << "AIDecisionMaker: Caesar stays safe - we have" << numGenerals << "generals";
    }

    // Process all Generals
    for (GeneralPiece *general : player->getGenerals()) {
        if (!hasMovesRemaining(general)) {
            qDebug() << "  General #" << general->getNumber() << "has no moves remaining, skipping";
            continue;
        }

        QMap<QString, ReachInfo> reachable = calc.getReachableFrom(general, graph, player);
        qDebug() << "  General #" << general->getNumber() << "at" << general->getTerritoryName()
                 << "can reach" << reachable.size() << "territories";

        for (auto it = reachable.begin(); it != reachable.end(); ++it) {
            TerritoryRisk risk;
            if (riskMap.contains(it.key())) {
                risk = riskMap[it.key()];
            } else {
                risk.territoryName = it.key();
                risk.risk = RiskLevel::Safe;
            }

            ScoredMove move = scoreMove(general, it.key(), it.value(), risk, player, allPlayers, graph, riskMap);
            if (move.isValid() && move.score > -9000) {
                allMoves.append(move);
            }
        }
    }

    // Sort by score (highest first)
    std::sort(allMoves.begin(), allMoves.end(),
              [](const ScoredMove &a, const ScoredMove &b) {
                  return a.score > b.score;
              });

    return allMoves;
}

ScoredMove AIDecisionMaker::scoreMove(GamePiece *leader,
                                       const QString &destination,
                                       const ReachInfo &reachInfo,
                                       const TerritoryRisk &risk,
                                       Player *player,
                                       const QList<Player*> &allPlayers,
                                       MapGraph *graph,
                                       const QMap<QString, TerritoryRisk> &riskMap)
{
    ScoredMove move;
    move.leader = leader;
    move.destination = destination;
    move.troopsCanBring = reachInfo.maxTroopStrength;
    move.score = 0;

    int territoryValue = graph->getValue(destination);
    bool weOwnIt = playerOwnsTerritory(player, destination);

    // === RETREAT LOGIC: Check if leader needs to escape current position ===
    // A lone general (no troops) in a HIGH risk territory should retreat!
    QString currentTerritory = leader->getTerritoryName();
    bool isLoneGeneral = (move.troopsCanBring == 0);

    // Check if current position is dangerous for a lone general
    // We consider it dangerous if enemy troops CAN REACH current position (not just currently there)
    bool currentPositionDangerous = false;
    int currentEnemyMaxForce = 0;  // Enemy force that can reach our current position

    if (isLoneGeneral) {
        // Check for enemy troops that can reach current position
        ReachabilityCalculator calc;
        QMap<QString, TerritoryRisk> riskMap = calc.assessAllTerritories(player, allPlayers, graph);
        if (riskMap.contains(currentTerritory)) {
            TerritoryRisk currentRisk = riskMap[currentTerritory];
            currentEnemyMaxForce = currentRisk.enemyMaxForce;
            // Dangerous if HIGH risk OR MEDIUM risk with significant enemy force
            if ((currentRisk.risk == RiskLevel::High && currentEnemyMaxForce > 0) ||
                (currentRisk.risk == RiskLevel::Medium && currentEnemyMaxForce >= 2)) {
                currentPositionDangerous = true;
            }
        }
    }

    // If current position is dangerous and destination is safer, give retreat bonus
    if (currentPositionDangerous) {
        // Check if destination is safer (lower risk level or fewer enemy troops can reach)
        bool destIsSafer = (risk.risk == RiskLevel::Safe || risk.risk == RiskLevel::Low) ||
                          (risk.enemyMaxForce < currentEnemyMaxForce);

        if (destIsSafer && weOwnIt) {
            // Retreating to our own safer territory - HIGHLY valuable for lone generals
            // This needs to be a DOMINANT bonus to ensure retreat happens
            int retreatBonus = 300;
            move.score += retreatBonus;
            move.reason = QString("RETREAT from danger (enemy=%1): +%2").arg(currentEnemyMaxForce).arg(retreatBonus);
            qDebug() << "Retreat bonus for" << leader->getTerritoryName() << "->" << destination
                     << ": +" << retreatBonus << "(enemy force=" << currentEnemyMaxForce << ")";
        } else if (destIsSafer) {
            // Retreating to neutral/enemy but safer territory
            int retreatBonus = 150;
            move.score += retreatBonus;
            move.reason = QString("Escape to safer ground (enemy=%1): +%2").arg(currentEnemyMaxForce).arg(retreatBonus);
        } else {
            // Moving to another dangerous position as a lone general - BAD IDEA
            // Strong penalty to discourage this
            int dangerPenalty = -250;
            move.score += dangerPenalty;
            move.reason = QString("DANGER: Lone general moving to risky area: %1").arg(dangerPenalty);
        }
    } else {
        move.reason = QString("");
    }

    // Debug: Log when a lone general is in danger but not retreating properly
    if (isLoneGeneral && currentPositionDangerous) {
        qDebug() << "Lone general at" << currentTerritory << "(danger! enemy=" << currentEnemyMaxForce << ")"
                 << "considering move to" << destination << "(enemy=" << risk.enemyMaxForce << ")";
    }

    // Also penalize lone generals moving INTO danger even if they weren't in danger before
    if (isLoneGeneral && !currentPositionDangerous) {
        if (risk.risk == RiskLevel::High && risk.enemyMaxForce > 0) {
            int dangerPenalty = -200;
            move.score += dangerPenalty;
            move.reason += QString(" | WARNING: Moving lone general into HIGH risk: %1").arg(dangerPenalty);
        } else if (risk.risk == RiskLevel::Medium && risk.enemyMaxForce > 0) {
            int dangerPenalty = -50;
            move.score += dangerPenalty;
            move.reason += QString(" | Caution: Lone general entering MEDIUM risk: %1").arg(dangerPenalty);
        }
    }

    // Check for city at destination and count current defenders
    Player *cityOwner = findCityOwnerAt(destination, allPlayers);
    bool hasCity = (cityOwner != nullptr);
    bool hasFortifiedCity = false;
    if (hasCity) {
        City *city = cityOwner->getCityAtTerritory(destination);
        hasFortifiedCity = city && city->isFortified();
    }
    bool enemyCity = hasCity && cityOwner != player;

    // Count CURRENT enemy troops at the territory (not future reinforcements)
    int currentEnemyTroops = countEnemyTroopsAt(destination, player, allPlayers);
    // Check for ANY enemy presence (including lone generals) - requires troops to attack
    bool hasEnemyPresence = hasEnemyPresenceAt(destination, player, allPlayers);
    // Territory is only "undefended" if there are no enemy pieces at all (not even generals)
    bool isUndefended = !hasEnemyPresence;

    // Base value of territory - BUT only if we don't already own it!
    // Moving within our own territory doesn't gain us anything
    if (!weOwnIt) {
        move.score += territoryValue * m_territoryValueWeight;
        if (move.reason.isEmpty()) {
            move.reason = QString("Base value: %1 * %2 = %3")
                .arg(territoryValue).arg(m_territoryValueWeight).arg(territoryValue * m_territoryValueWeight);
        } else {
            move.reason += QString(" | Base value: %1").arg(territoryValue * m_territoryValueWeight);
        }
    } else if (move.reason.isEmpty()) {
        move.reason = QString("Own territory (no base value)");
    }

    // === BONUS FOR UNDEFENDED ENEMY TERRITORIES ===
    // Undefended territories can be captured without combat
    // BUT: A lone general capturing territory may get counter-attacked!
    if (!weOwnIt && isUndefended) {
        // Base bonus for free capture
        move.score += 150;  // Reduced from 250 - need to consider counter-attack risk
        move.reason += " | UNDEFENDED (free capture): +150";

        // Check if the territory will be SAFE after we capture it
        // The risk assessment tells us if enemy can reach this territory
        bool safeAfterCapture = (risk.risk == RiskLevel::Safe || risk.risk == RiskLevel::Low);
        bool canBeCounterAttacked = (risk.enemyMaxForce > 0);

        if (move.troopsCanBring == 0) {
            // Lone general capturing - need to be careful!
            if (safeAfterCapture) {
                // Safe expansion - enemy can't counter-attack
                move.score += 100;
                move.reason += " | Safe expansion (no counter-attack): +100";
            } else if (canBeCounterAttacked) {
                // DANGER: Lone general will be captured on enemy's turn!
                int counterAttackPenalty = -200;
                if (risk.enemyMaxForce >= 3) {
                    counterAttackPenalty = -300;  // Overwhelming force nearby
                }
                move.score += counterAttackPenalty;
                move.reason += QString(" | DANGER: Lone general can be counter-attacked (enemy force=%1): %2")
                    .arg(risk.enemyMaxForce).arg(counterAttackPenalty);
            }
        } else {
            // We have troops - can defend after capture
            if (move.troopsCanBring >= risk.enemyMaxForce) {
                move.score += 50;  // We can hold it
                move.reason += " | Can hold after capture: +50";
            }
        }

        // Extra bonus for cities (value + 5 income)
        if (enemyCity) {
            int cityBonus = hasFortifiedCity ? 150 : 100;  // Fortified cities worth more
            move.score += cityBonus;
            move.reason += QString(" | ENEMY CITY%1: +%2")
                .arg(hasFortifiedCity ? " (fortified)" : "")
                .arg(cityBonus);
        }

        // Generals without troops CAN claim undefended territories
        // (no combat = no troops needed)
        // Skip the "no troops" check below for undefended territories
    } else {
        // Territory has defenders - normal risk-based scoring
        switch (risk.risk) {
            case RiskLevel::Safe:
                if (!weOwnIt) {
                    move.score += m_safetyBonus;
                    move.reason += QString(" | Safe target: +%1").arg(m_safetyBonus);
                }
                break;

            case RiskLevel::Low:
                if (!weOwnIt) {
                    move.score += m_lowRiskBonus;
                    move.reason += QString(" | Low risk: +%1").arg(m_lowRiskBonus);
                }
                break;

            case RiskLevel::Medium:
                // No bonus or penalty for medium risk
                move.reason += " | Medium risk: +0";
                break;

            case RiskLevel::High:
                if (!weOwnIt) {
                    // Attacking a high-risk target - but still valuable if it has a city!
                    if (enemyCity) {
                        // Reduce penalty for attacking cities - they're worth the risk
                        int reducedPenalty = m_highRiskPenalty / 2;
                        move.score -= reducedPenalty;
                        move.reason += QString(" | High risk city attack: -%1").arg(reducedPenalty);
                    } else {
                        move.score -= m_highRiskPenalty;
                        move.reason += QString(" | High risk attack: -%1").arg(m_highRiskPenalty);
                    }
                }
                break;

            case RiskLevel::Unreachable:
                // Shouldn't happen since we're iterating reachable territories
                move.score = -9999;
                move.reason = "Unreachable";
                return move;
        }
    }

    // Defensive considerations - DEFENDING OUR TERRITORY IS TOP PRIORITY
    // BUT: A general without troops is NOT a defender - they're just more bait!
    if (weOwnIt) {
        if (risk.risk == RiskLevel::High || risk.risk == RiskLevel::Medium) {
            // Check if this general can actually bring troops to defend
            if (move.troopsCanBring == 0) {
                // General has NO troops - "defending" with them is COUNTERPRODUCTIVE
                // They're just sending more bait to get captured
                // Heavy penalty to discourage this behavior
                move.score -= 200;
                move.reason += QString(" | NO TROOPS TO DEFEND WITH: -200 (don't send bait!)");
            } else {
                // Reinforcing a threatened territory with actual troops - this is CRITICAL!
                int defBonus = m_defenseBonus;  // Base 100

                // Check if WE have a city there
                bool weHaveCity = hasCity && cityOwner == player;
                bool weHaveFortifiedCity = weHaveCity && hasFortifiedCity;

                if (weHaveFortifiedCity) {
                    // FORTIFIED CITIES ARE EXTREMELY VALUABLE - defend at all costs!
                    // Worth: territory value + 5 income + fortification cost (50 total investment)
                    // Plus strategic value of defensive position
                    if (risk.risk == RiskLevel::High) {
                        defBonus += 300;  // URGENT: fortified city under HIGH threat
                        move.reason += QString(" | URGENT: Fortified city under attack! +%1").arg(defBonus);
                    } else {
                        defBonus += 200;  // Important: fortified city under medium threat
                        move.reason += QString(" | Fortified city threatened: +%1").arg(defBonus);
                    }
                } else if (weHaveCity) {
                    // Unfortified city - still valuable
                    if (risk.risk == RiskLevel::High) {
                        defBonus += 150;
                        move.reason += QString(" | City under attack! +%1").arg(defBonus);
                    } else {
                        defBonus += 100;
                        move.reason += QString(" | City threatened: +%1").arg(defBonus);
                    }
                } else {
                    // No city, just territory
                    if (risk.risk == RiskLevel::High) {
                        defBonus += 50;
                        move.reason += QString(" | Territory under attack: +%1").arg(defBonus);
                    } else {
                        move.reason += QString(" | Territory threatened: +%1").arg(defBonus);
                    }
                }
                move.score += defBonus;
            }
        } else {
            // Moving to a safe territory we already own - usually wasteful UNLESS
            // it positions us to defend threatened territories, attack enemies,
            // or capture unowned territories (STEPPING STONE)
            bool isRepositioning = false;
            int repositionBonus = 0;
            QString repositionReason;

            // Check if destination is adjacent to territories we could capture/attack
            QStringList neighbors = graph->getNeighbors(destination);
            QStringList ourTerritories = player->getOwnedTerritories();
            QSet<QString> ourSet(ourTerritories.begin(), ourTerritories.end());

            for (const QString &neighbor : neighbors) {
                // Skip sea territories
                if (graph->isSeaTerritory(neighbor)) continue;

                // Check for enemy troops at the neighbor
                int enemyTroops = countEnemyTroopsAt(neighbor, player, allPlayers);

                if (ourSet.contains(neighbor)) {
                    // Our territory - check if it has enemy threat (only relevant if we have troops)
                    if (move.troopsCanBring > 0 && enemyTroops > 0) {
                        // Our territory is being threatened - moving adjacent is valuable!
                        isRepositioning = true;
                        if (repositionBonus < 100) {
                            repositionBonus = 100;
                            repositionReason = QString("adjacent to threatened %1").arg(neighbor);
                        }
                    }
                } else {
                    // Not our territory - check if it's a good expansion target
                    Player *cityOwnerAtNeighbor = findCityOwnerAt(neighbor, allPlayers);
                    bool enemyHasCity = (cityOwnerAtNeighbor != nullptr && cityOwnerAtNeighbor != player);

                    // Find who owns this territory
                    Player *territoryOwner = nullptr;
                    for (Player *p : allPlayers) {
                        if (p->ownsTerritory(neighbor)) {
                            territoryOwner = p;
                            break;
                        }
                    }
                    bool isUnowned = (territoryOwner == nullptr);

                    // === STEPPING STONE BONUS ===
                    // If this destination puts us adjacent to UNOWNED territory, that's valuable!
                    // We can capture it next turn for free (no combat)
                    if (isUnowned && enemyTroops == 0) {
                        int neighborValue = graph->getValue(neighbor);
                        // Higher bonus for high-value unowned territories
                        int steppingStoneBonus = 80 + (neighborValue >= 10 ? 40 : 0);

                        // Check risk at that neighbor - is it safe to capture?
                        if (riskMap.contains(neighbor)) {
                            RiskLevel neighborRisk = riskMap[neighbor].risk;
                            if (neighborRisk == RiskLevel::Safe || neighborRisk == RiskLevel::Low) {
                                steppingStoneBonus += 30;  // Extra bonus for safe expansion
                            }
                        }

                        isRepositioning = true;
                        if (steppingStoneBonus > repositionBonus) {
                            repositionBonus = steppingStoneBonus;
                            repositionReason = QString("stepping stone to unowned %1 (val=%2)")
                                .arg(neighbor).arg(neighborValue);
                        }
                    }
                    // Repositioning to be adjacent to enemy positions is also strategic
                    else if (move.troopsCanBring > 0) {
                        if (enemyHasCity) {
                            // Adjacent to enemy city - good attack position
                            isRepositioning = true;
                            if (repositionBonus < 60) {
                                repositionBonus = 60;
                                repositionReason = QString("adjacent to enemy city at %1").arg(neighbor);
                            }
                        } else if (enemyTroops > 0) {
                            // Adjacent to enemy troops
                            isRepositioning = true;
                            if (repositionBonus < 40) {
                                repositionBonus = 40;
                                repositionReason = QString("adjacent to enemy troops at %1").arg(neighbor);
                            }
                        }
                    }
                }
            }

            if (isRepositioning) {
                move.score += repositionBonus;
                move.reason += QString(" | Strategic repositioning (%1): +%2").arg(repositionReason).arg(repositionBonus);
            } else {
                move.score -= m_ownTerritoryPenalty;
                move.reason += QString(" | Own safe territory: -%1").arg(m_ownTerritoryPenalty);
            }
        }
    }

    // Force advantage consideration (only matters if there are current defenders)
    if (!isUndefended) {
        int forceAdvantage = move.troopsCanBring - currentEnemyTroops;
        int forceBonus = forceAdvantage * m_forceAdvantageWeight;
        move.score += forceBonus;
        move.reason += QString(" | Force advantage (%1 vs %2): %3%4")
            .arg(move.troopsCanBring)
            .arg(currentEnemyTroops)
            .arg(forceBonus >= 0 ? "+" : "")
            .arg(forceBonus);
    }

    // Bonus for high-value targets (10-value territories)
    if (territoryValue >= 10 && !weOwnIt) {
        move.score += 20;
        move.reason += " | High-value target: +20";
    }

    // INVALID if we can't bring any troops AND there are enemy pieces (combat requires troops)
    // Even lone enemy generals require troops to capture - you can't attack without an army!
    if (move.troopsCanBring == 0 && hasEnemyPresence) {
        move.score = -9999;
        move.reason = "INVALID: Cannot enter combat without troops";
        return move;
    }

    // === BONUS FOR GENERALS TO PICK UP TROOPS ===
    // A general with few or no troops should strongly prefer moving to territories
    // where there are friendly unassigned troops they can recruit

    // Helper lambda to count unassigned troops at a territory
    auto countUnassignedTroopsAt = [&player](const QString &territory) -> int {
        int unassigned = 0;

        for (InfantryPiece *inf : player->getInfantry()) {
            if (inf->getTerritoryName() != territory) continue;
            bool inLegion = false;
            for (CaesarPiece *caesar : player->getCaesars()) {
                if (caesar->getLegion().contains(inf->getUniqueId())) {
                    inLegion = true;
                    break;
                }
            }
            if (!inLegion) {
                for (GeneralPiece *gen : player->getGenerals()) {
                    if (gen->getLegion().contains(inf->getUniqueId())) {
                        inLegion = true;
                        break;
                    }
                }
            }
            if (!inLegion) unassigned++;
        }

        for (CavalryPiece *cav : player->getCavalry()) {
            if (cav->getTerritoryName() != territory) continue;
            bool inLegion = false;
            for (CaesarPiece *caesar : player->getCaesars()) {
                if (caesar->getLegion().contains(cav->getUniqueId())) {
                    inLegion = true;
                    break;
                }
            }
            if (!inLegion) {
                for (GeneralPiece *gen : player->getGenerals()) {
                    if (gen->getLegion().contains(cav->getUniqueId())) {
                        inLegion = true;
                        break;
                    }
                }
            }
            if (!inLegion) unassigned++;
        }

        for (CatapultPiece *cat : player->getCatapults()) {
            if (cat->getTerritoryName() != territory) continue;
            bool inLegion = false;
            for (CaesarPiece *caesar : player->getCaesars()) {
                if (caesar->getLegion().contains(cat->getUniqueId())) {
                    inLegion = true;
                    break;
                }
            }
            if (!inLegion) {
                for (GeneralPiece *gen : player->getGenerals()) {
                    if (gen->getLegion().contains(cat->getUniqueId())) {
                        inLegion = true;
                        break;
                    }
                }
            }
            if (!inLegion) unassigned++;
        }

        return unassigned;
    };

    // Check if this general needs more troops (has few or none)
    bool needsTroops = (move.troopsCanBring <= 2);  // Generals with 0-2 troops should seek more
    QString homeProvince = player->getHomeProvinceName();

    if (needsTroops && weOwnIt) {
        // Count unassigned troops at destination
        int unassignedAtDest = countUnassignedTroopsAt(destination);

        if (unassignedAtDest > 0) {
            // Direct pickup - arriving at territory with unassigned troops
            int pickupBonus = 150 + (unassignedAtDest * 25);  // Base 150 + 25 per troop available

            // Extra bonus if this is the home province (main recruitment center)
            if (destination == homeProvince) {
                pickupBonus += 100;  // Home province is the best place to recruit
                move.reason += QString(" | RETURN HOME for troops (%1 available): +%2")
                    .arg(unassignedAtDest).arg(pickupBonus);
            } else {
                move.reason += QString(" | RECRUIT TROOPS (%1 available): +%2")
                    .arg(unassignedAtDest).arg(pickupBonus);
            }
            move.score += pickupBonus;
        }
        // Check if home province has troops and we're moving TOWARD it
        else if (destination != homeProvince) {
            int troopsAtHome = countUnassignedTroopsAt(homeProvince);

            if (troopsAtHome > 0) {
                // Check if destination is closer to home than current position
                // Use road network if available, otherwise just check adjacency
                QStringList destNeighbors = graph->getNeighbors(destination);
                QStringList currentNeighbors = graph->getNeighbors(currentTerritory);

                bool destCloserToHome = false;

                // If destination IS adjacent to home, big bonus
                if (destNeighbors.contains(homeProvince)) {
                    destCloserToHome = true;
                }
                // If destination is on road network to home
                else {
                    QStringList roadFromDest = graph->getRoadConnectedTerritories(destination, player);
                    QStringList roadFromCurrent = graph->getRoadConnectedTerritories(currentTerritory, player);

                    if (roadFromDest.contains(homeProvince) && !roadFromCurrent.contains(homeProvince)) {
                        destCloserToHome = true;  // Destination connects to home via road, current doesn't
                    }
                }

                if (destCloserToHome) {
                    int approachBonus = 75 + (troopsAtHome * 10);  // Moving toward troops
                    move.score += approachBonus;
                    move.reason += QString(" | Moving toward home (%1 troops waiting): +%2")
                        .arg(troopsAtHome).arg(approachBonus);
                }
            }
        }
    }

    // Even generals with some troops should consider going home if there are MANY unassigned troops
    // This handles the case where a general has a small force but could build a much larger army
    if (move.troopsCanBring > 2 && move.troopsCanBring < 6 && destination == homeProvince) {
        int troopsAtHome = countUnassignedTroopsAt(homeProvince);
        if (troopsAtHome >= 3) {
            // Worth going home to bulk up the army
            int bulkUpBonus = 50 + (troopsAtHome * 15);
            move.score += bulkUpBonus;
            move.reason += QString(" | Reinforce army at home (%1 available): +%2")
                .arg(troopsAtHome).arg(bulkUpBonus);
        }
    }

    return move;
}

int AIDecisionMaker::countEnemyTroopsAt(const QString &territory, Player *us, const QList<Player*> &allPlayers)
{
    int count = 0;
    for (Player *player : allPlayers) {
        if (player == us) continue;

        // Count infantry, cavalry, catapults
        for (InfantryPiece *inf : player->getInfantry()) {
            if (inf->getTerritoryName() == territory) {
                count++;
                qDebug() << "  Found enemy infantry at" << territory << "owned by player" << player->getId();
            }
        }
        for (CavalryPiece *cav : player->getCavalry()) {
            if (cav->getTerritoryName() == territory) {
                count++;
                qDebug() << "  Found enemy cavalry at" << territory << "owned by player" << player->getId();
            }
        }
        for (CatapultPiece *cat : player->getCatapults()) {
            if (cat->getTerritoryName() == territory) {
                count++;
                qDebug() << "  Found enemy catapult at" << territory << "owned by player" << player->getId();
            }
        }
    }

    if (count == 0) {
        qDebug() << "  countEnemyTroopsAt(" << territory << "): 0 enemy troops found";
    }

    return count;
}

/**
 * @brief Check if any enemy pieces (including leaders) are at a territory
 * This is different from countEnemyTroopsAt - it checks for ANY enemy presence
 * which would trigger combat or block entry
 */
bool AIDecisionMaker::hasEnemyPresenceAt(const QString &territory, Player *us, const QList<Player*> &allPlayers)
{
    for (Player *player : allPlayers) {
        if (player == us) continue;

        // Check for troops
        for (InfantryPiece *inf : player->getInfantry()) {
            if (inf->getTerritoryName() == territory) return true;
        }
        for (CavalryPiece *cav : player->getCavalry()) {
            if (cav->getTerritoryName() == territory) return true;
        }
        for (CatapultPiece *cat : player->getCatapults()) {
            if (cat->getTerritoryName() == territory) return true;
        }

        // Check for leaders (Caesars and Generals)
        for (CaesarPiece *caesar : player->getCaesars()) {
            if (caesar->getTerritoryName() == territory) return true;
        }
        for (GeneralPiece *gen : player->getGenerals()) {
            if (gen->getTerritoryName() == territory) return true;
        }
    }
    return false;
}

Player* AIDecisionMaker::findCityOwnerAt(const QString &territory, const QList<Player*> &allPlayers)
{
    for (Player *player : allPlayers) {
        City *city = player->getCityAtTerritory(territory);
        if (city) {
            return player;
        }
    }
    return nullptr;
}

bool AIDecisionMaker::playerOwnsTerritory(Player *player, const QString &territory)
{
    if (!player) return false;
    return player->getOwnedTerritories().contains(territory);
}

QString AIDecisionMaker::generateDecisionReport(Player *player, const QList<Player*> &allPlayers, MapGraph *graph)
{
    QString report;

    if (!player || !graph) {
        return "Error: Invalid player or graph";
    }

    report += QString("=== AI DECISION ANALYSIS FOR PLAYER %1 ===\n\n").arg(player->getId());

    QList<ScoredMove> allMoves = getAllScoredMoves(player, allPlayers, graph);

    if (allMoves.isEmpty()) {
        report += "No valid moves available.\n";
        return report;
    }

    report += QString("Total possible moves: %1\n\n").arg(allMoves.size());

    // Show top 10 moves
    report += "--- TOP MOVES ---\n\n";
    int shown = 0;
    for (const ScoredMove &move : allMoves) {
        if (shown++ >= 10) break;

        QString leaderType;
        if (move.leader->getType() == GamePiece::Type::Caesar) {
            leaderType = "Caesar";
        } else if (move.leader->getType() == GamePiece::Type::General) {
            GeneralPiece *gen = static_cast<GeneralPiece*>(move.leader);
            leaderType = QString("General #%1").arg(gen->getNumber());
        } else {
            leaderType = "Leader";
        }

        QString fromTerritory = move.leader->getTerritoryName();
        int value = graph->getValue(move.destination);

        report += QString("%1. [Score: %2] %3: %4 -> %5 (val:%6, troops:%7)\n")
            .arg(shown, 2)
            .arg(move.score, 4)
            .arg(leaderType)
            .arg(fromTerritory)
            .arg(move.destination)
            .arg(value)
            .arg(move.troopsCanBring);

        report += QString("   Reason: %1\n\n").arg(move.reason);
    }

    // Recommendation
    if (!allMoves.isEmpty()) {
        const ScoredMove &best = allMoves.first();
        report += "--- RECOMMENDATION ---\n\n";

        if (best.score > 0) {
            QString leaderType;
            if (best.leader->getType() == GamePiece::Type::Caesar) {
                leaderType = "Caesar";
            } else {
                GeneralPiece *gen = static_cast<GeneralPiece*>(best.leader);
                leaderType = QString("General #%1").arg(gen->getNumber());
            }

            report += QString("MOVE %1 from %2 to %3 with %4 troops\n")
                .arg(leaderType)
                .arg(best.leader->getTerritoryName())
                .arg(best.destination)
                .arg(best.troopsCanBring);
        } else {
            report += "No moves with positive score. Consider ending turn.\n";
        }
    }

    return report;
}

// === Purchase Decision Implementation ===

AIPurchaseDecision AIDecisionMaker::decidePurchases(
    Player *player,
    const QList<Player*> &allPlayers,
    MapGraph *graph,
    int budget,
    int inflationMultiplier,
    const QStringList &territoriesForCities,
    const QStringList &territoriesForFortification,
    const QStringList &seaTerritoriesForGalleys,
    int currentGalleyCount,
    int maxGalleys)
{
    AIPurchaseDecision decision;
    int remaining = budget;

    if (!player || !graph || budget <= 0) {
        decision.reason = "No budget or invalid state";
        return decision;
    }

    // Calculate current prices with inflation
    int infantryPrice = INFANTRY_COST * inflationMultiplier;
    int cavalryPrice = CAVALRY_COST * inflationMultiplier;
    int catapultPrice = CATAPULT_COST * inflationMultiplier;
    int cityPrice = CITY_COST * inflationMultiplier;
    int fortifiedCityPrice = (CITY_COST + FORTIFICATION_COST) * inflationMultiplier;
    int fortificationPrice = FORTIFICATION_COST * inflationMultiplier;
    int galleyPrice = GALLEY_COST * inflationMultiplier;

    // Get risk assessment
    ReachabilityCalculator calc;
    QMap<QString, TerritoryRisk> riskMap = calc.assessAllTerritories(player, allPlayers, graph);

    // Count current military strength
    int currentInfantry = player->getInfantry().size();
    int currentCavalry = player->getCavalry().size();
    int currentTroops = currentInfantry + currentCavalry + player->getCatapults().size();

    QString homeProvince = player->getHomeProvinceName();
    QStringList ownedTerritories = player->getOwnedTerritories();
    QSet<QString> ownedSet(ownedTerritories.begin(), ownedTerritories.end());

    // Categorize our territories by risk level
    QStringList highRiskTerritories;
    QStringList mediumRiskTerritories;
    QStringList safeOrLowRiskTerritories;
    bool homeProvinceAtRisk = false;

    for (const QString &territory : ownedTerritories) {
        if (!riskMap.contains(territory)) continue;

        RiskLevel risk = riskMap[territory].risk;
        if (risk == RiskLevel::High) {
            highRiskTerritories.append(territory);
            if (territory == homeProvince) homeProvinceAtRisk = true;
        } else if (risk == RiskLevel::Medium) {
            mediumRiskTerritories.append(territory);
            if (territory == homeProvince) homeProvinceAtRisk = true;
        } else {
            safeOrLowRiskTerritories.append(territory);
        }
    }

    // Find territories on the road network path from home to at-risk territories
    // (for strategic city placement to enable fast reinforcement)
    QSet<QString> roadNetworkNeeded;
    QStringList roadConnected = graph->getRoadConnectedTerritories(homeProvince, player);
    QSet<QString> alreadyConnected(roadConnected.begin(), roadConnected.end());
    alreadyConnected.insert(homeProvince);

    // Count enemy fortified cities nearby (within reachable range)
    // This influences whether we need catapults
    int enemyFortifiedCitiesNearby = 0;
    int currentCatapults = player->getCatapults().size();

    for (Player *enemy : allPlayers) {
        if (enemy == player) continue;
        for (City *city : enemy->getCities()) {
            if (city && city->isFortified()) {
                // Check if this city is in a territory we could potentially attack
                QString cityTerritory = city->getTerritoryName();
                // Consider it "nearby" if it's adjacent to any of our territories
                for (const QString &ourTerritory : ownedTerritories) {
                    QStringList neighbors = graph->getNeighbors(ourTerritory);
                    if (neighbors.contains(cityTerritory)) {
                        enemyFortifiedCitiesNearby++;
                        break;  // Don't count same city multiple times
                    }
                }
            }
        }
    }

    // Check which at-risk territories are NOT on the road network
    for (const QString &atRisk : highRiskTerritories + mediumRiskTerritories) {
        if (!alreadyConnected.contains(atRisk)) {
            // This territory needs road connection - find path territories
            // For now, mark territories adjacent to road-connected areas as candidates
            QStringList neighbors = graph->getNeighbors(atRisk);
            for (const QString &neighbor : neighbors) {
                if (ownedSet.contains(neighbor) && !alreadyConnected.contains(neighbor)) {
                    roadNetworkNeeded.insert(neighbor);
                }
            }
        }
    }

    decision.reason = QString("Risk: %1 HIGH, %2 MED, %3 safe | Army: %4 inf, %5 cav, %6 cat")
        .arg(highRiskTerritories.size())
        .arg(mediumRiskTerritories.size())
        .arg(safeOrLowRiskTerritories.size())
        .arg(currentInfantry)
        .arg(currentCavalry)
        .arg(currentCatapults);
    if (enemyFortifiedCitiesNearby > 0) {
        decision.reason += QString(" | Enemy forts nearby: %1").arg(enemyFortifiedCitiesNearby);
    }

    // === PRIORITY 0: Destroy cities at HIGH risk territories we truly can't defend ===
    //
    // Decision factors:
    // 1. How soon can enemy attack? (they're at HIGH risk = can attack THIS turn)
    // 2. How soon can we reinforce, and with how many troops?
    // 3. Can we match or exceed enemy force before they overwhelm us?
    //
    // For HIGH risk territories, enemy can attack THIS turn, so we need defenders NOW
    // or reinforcements that can arrive before the city falls.

    QMap<QString, MultiTurnReachInfo> multiTurnReach = calc.getAllMultiTurnReachability(player, allPlayers, graph, 3);

    for (const QString &territory : highRiskTerritories) {
        City *city = player->getCityAtTerritory(territory);
        if (!city) continue;

        int ourCurrentForce = riskMap[territory].ourMaxForce;  // Force we can project THIS turn
        int enemyForce = riskMap[territory].enemyMaxForce;
        bool isFortified = city->isFortified();

        // Fortification gives +1 effective defense
        int effectiveDefense = ourCurrentForce + (isFortified ? 1 : 0);

        // If we can defend THIS turn with adequate force, don't destroy
        if (effectiveDefense >= enemyForce) {
            continue;  // We can hold it
        }

        // Check reinforcements - can we get enough troops there in time?
        int reinforcementsIn1Turn = 0;
        int reinforcementsIn2Turns = 0;
        int reinforcementsIn3Turns = 0;
        QString reinforcementPath;

        if (multiTurnReach.contains(territory)) {
            const MultiTurnReachInfo &reachInfo = multiTurnReach[territory];
            reinforcementPath = reachInfo.pathDescription;

            if (reachInfo.turnsToReach == 1) {
                reinforcementsIn1Turn = reachInfo.troopsCanBring;
            } else if (reachInfo.turnsToReach == 2) {
                reinforcementsIn2Turns = reachInfo.troopsCanBring;
            } else if (reachInfo.turnsToReach == 3) {
                reinforcementsIn3Turns = reachInfo.troopsCanBring;
            }
        }

        // Enemy is at HIGH risk = can attack THIS turn
        // If we have no current defense, the city will likely fall before reinforcements arrive
        // UNLESS enemy chooses not to attack (but we can't count on that)

        bool canDefend = false;
        QString defenseReason;

        if (ourCurrentForce > 0) {
            // We have some defense now
            if (effectiveDefense >= enemyForce) {
                canDefend = true;
                defenseReason = QString("current defense %1 vs enemy %2").arg(effectiveDefense).arg(enemyForce);
            } else if (reinforcementsIn1Turn > 0 && (effectiveDefense + reinforcementsIn1Turn) >= enemyForce) {
                // Even if we lose the first battle, reinforcements next turn might retake
                canDefend = true;
                defenseReason = QString("reinforce next turn: +%1 troops").arg(reinforcementsIn1Turn);
            }
        } else {
            // No current defense - city is vulnerable THIS turn
            // Only hope is if we have overwhelming reinforcements coming soon
            // and enemy force is small enough that a lone general might survive

            if (enemyForce <= 2 && reinforcementsIn1Turn >= enemyForce) {
                // Small enemy force, we can retake next turn
                canDefend = true;
                defenseReason = QString("small threat (%1), retake with %2 troops next turn")
                    .arg(enemyForce).arg(reinforcementsIn1Turn);
            } else if (enemyForce == 1 && reinforcementsIn2Turns >= 3) {
                // Single enemy troop, we can likely retake even if delayed
                canDefend = true;
                defenseReason = QString("single enemy, strong reinforcement in 2 turns");
            }
        }

        if (canDefend) {
            qDebug() << "City at" << territory << "- keeping (can defend):" << defenseReason;
            qDebug() << "  Current:" << ourCurrentForce << "vs enemy:" << enemyForce
                     << "| Reinforcements: T1=" << reinforcementsIn1Turn
                     << "T2=" << reinforcementsIn2Turns << "T3=" << reinforcementsIn3Turns;
            if (!reinforcementPath.isEmpty()) {
                qDebug() << "  Path:" << reinforcementPath;
            }
            continue;
        }

        // Can't defend - destroy the city to deny it to the enemy
        decision.citiesToDestroy.append(territory);
        decision.reason += QString(" | DESTROY %1 (enemy=%2, us=%3, no reinforcements)")
            .arg(territory).arg(enemyForce).arg(ourCurrentForce);

        qDebug() << "City at" << territory << "- DESTROYING (can't defend)";
        qDebug() << "  Current:" << ourCurrentForce << "vs enemy:" << enemyForce
                 << "| Reinforcements: T1=" << reinforcementsIn1Turn
                 << "T2=" << reinforcementsIn2Turns << "T3=" << reinforcementsIn3Turns;
    }

    // === PRIORITY 1: Emergency Defense - Home at risk ===
    if (homeProvinceAtRisk) {
        decision.reason += " | HOME AT RISK";

        // First, fortify home if possible (gives +1 defense)
        if (territoriesForFortification.contains(homeProvince) && remaining >= fortificationPrice) {
            decision.fortifications.append(homeProvince);
            remaining -= fortificationPrice;
            decision.totalCost += fortificationPrice;
            decision.reason += " | Fortify home";
        }

        // Then buy as many troops as possible
        while (remaining >= infantryPrice) {
            decision.infantry++;
            remaining -= infantryPrice;
            decision.totalCost += infantryPrice;
        }
        decision.reason += QString(" | Emergency troops: %1").arg(decision.infantry);
        return decision;
    }

    // === PRIORITY 2: Fortify at-risk territories with legions ===
    // Fortification gives +1 defense which can tip battles in our favor
    // Prioritize territories with our troops stationed there

    // Helper: Check if we have a legion (leader + troops) stationed at a territory
    auto hasLegionAt = [&player](const QString &territory) -> int {
        int troopsAtTerritory = 0;
        bool leaderAtTerritory = false;

        // Check for leaders at territory
        for (CaesarPiece *caesar : player->getCaesars()) {
            if (caesar->getTerritoryName() == territory) {
                leaderAtTerritory = true;
                break;
            }
        }
        if (!leaderAtTerritory) {
            for (GeneralPiece *gen : player->getGenerals()) {
                if (gen->getTerritoryName() == territory) {
                    leaderAtTerritory = true;
                    break;
                }
            }
        }

        if (!leaderAtTerritory) return 0;  // No legion without a leader

        // Count troops at territory
        for (InfantryPiece *inf : player->getInfantry()) {
            if (inf->getTerritoryName() == territory) troopsAtTerritory++;
        }
        for (CavalryPiece *cav : player->getCavalry()) {
            if (cav->getTerritoryName() == territory) troopsAtTerritory++;
        }
        for (CatapultPiece *cat : player->getCatapults()) {
            if (cat->getTerritoryName() == territory) troopsAtTerritory++;
        }

        return troopsAtTerritory;
    };

    // First pass: HIGH risk territories with stationed legions
    // These need fortification urgently to help them survive
    for (const QString &territory : highRiskTerritories) {
        if (remaining < fortificationPrice) break;
        if (decision.citiesToDestroy.contains(territory)) continue;  // Already decided to destroy

        int legionStrength = hasLegionAt(territory);
        if (legionStrength == 0) continue;  // No legion here to protect

        // Check if fortifying would help (enemy force is close to our force)
        int enemyForce = riskMap[territory].enemyMaxForce;

        // Only fortify if walls would make a difference (close battle)
        // If we're hopelessly outmatched, walls won't save us
        // Walls give +1, so fortify if we're within 2 of enemy force
        if (legionStrength >= enemyForce - 2 && legionStrength < enemyForce) {
            // Walls could tip this battle!
            if (territoriesForFortification.contains(territory)) {
                decision.fortifications.append(territory);
                remaining -= fortificationPrice;
                decision.totalCost += fortificationPrice;
                decision.reason += QString(" | Fortify %1 (HIGH risk, legion of %2 vs enemy %3)")
                    .arg(territory).arg(legionStrength).arg(enemyForce);
            }
            // If no city exists but we can build one, build fortified city to protect legion
            else if (territoriesForCities.contains(territory) && remaining >= fortifiedCityPrice) {
                decision.cities[territory] = true;  // Fortified
                remaining -= fortifiedCityPrice;
                decision.totalCost += fortifiedCityPrice;
                decision.reason += QString(" | Fortified city at %1 (protect legion of %2)")
                    .arg(territory).arg(legionStrength);
            }
        }
    }

    // Second pass: MEDIUM risk territories - only fortify existing cities, don't build new ones
    // Building cities at medium risk is usually wasteful - enemy might take it anyway
    for (const QString &territory : mediumRiskTerritories) {
        if (remaining < fortificationPrice) break;

        // Check if this territory can be fortified (has unfortified city)
        // Only fortify existing cities - don't build new ones at medium risk
        if (territoriesForFortification.contains(territory)) {
            decision.fortifications.append(territory);
            remaining -= fortificationPrice;
            decision.totalCost += fortificationPrice;
            decision.reason += QString(" | Fortify %1 (medium risk)").arg(territory);
        }
        // Don't build new cities at medium risk - too likely to be captured
    }

    // === PRIORITY 3: Build cities to extend road network ===
    // Cities create roads - prioritize:
    // 1. Cities adjacent to existing cities (extend network)
    // 2. Cities that bridge disconnected networks
    // 3. Cities on path to at-risk territories

    // Find all territories with our cities
    QSet<QString> territoriesWithCities;
    for (City *city : player->getCities()) {
        territoriesWithCities.insert(city->getTerritoryName());
    }

    // Score each potential city location for road network value
    struct CityCandidate {
        QString territory;
        int roadScore;      // How many existing cities it connects to
        bool bridgesNetworks; // Does it connect two previously disconnected areas?
        RiskLevel risk;
    };
    QList<CityCandidate> roadCityCandidates;

    for (const QString &territory : territoriesForCities) {
        if (decision.cities.contains(territory)) continue;  // Already planned
        if (!riskMap.contains(territory)) continue;

        CityCandidate candidate;
        candidate.territory = territory;
        candidate.risk = riskMap[territory].risk;
        candidate.roadScore = 0;
        candidate.bridgesNetworks = false;

        // Count adjacent cities (our cities only)
        QStringList neighbors = graph->getNeighbors(territory);
        QSet<QString> adjacentCityTerritories;
        for (const QString &neighbor : neighbors) {
            if (territoriesWithCities.contains(neighbor)) {
                candidate.roadScore += 10;  // Each adjacent city is valuable
                adjacentCityTerritories.insert(neighbor);
            }
        }

        // Check if this would bridge two disconnected road networks
        if (adjacentCityTerritories.size() >= 2) {
            // Check if any two adjacent cities are NOT already road-connected
            QList<QString> adjList = adjacentCityTerritories.values();
            for (int i = 0; i < adjList.size(); i++) {
                for (int j = i + 1; j < adjList.size(); j++) {
                    QStringList network1 = graph->getRoadConnectedTerritories(adjList[i], player);
                    if (!network1.contains(adjList[j])) {
                        candidate.bridgesNetworks = true;
                        candidate.roadScore += 50;  // Big bonus for bridging networks
                        break;
                    }
                }
                if (candidate.bridgesNetworks) break;
            }
        }

        // Only consider if it has road network value (adjacent to existing city)
        if (candidate.roadScore > 0) {
            roadCityCandidates.append(candidate);
        }
    }

    // Sort by road score descending
    std::sort(roadCityCandidates.begin(), roadCityCandidates.end(),
              [](const CityCandidate &a, const CityCandidate &b) {
                  return a.roadScore > b.roadScore;
              });

    // Build road network cities - ONLY if they bridge disconnected networks
    // Be conservative: only 1 road city per turn, and only if it truly connects networks
    for (const CityCandidate &candidate : roadCityCandidates) {
        if (decision.cities.size() >= 1) break;  // Limit to 1 city per turn total (conservative)

        // Only build if this city bridges disconnected networks - that's the real strategic value
        if (!candidate.bridgesNetworks) continue;

        bool isSafe = (candidate.risk == RiskLevel::Safe || candidate.risk == RiskLevel::Low);

        if (isSafe && remaining >= cityPrice) {
            // Safe location - build unfortified
            decision.cities[candidate.territory] = false;
            remaining -= cityPrice;
            decision.totalCost += cityPrice;
            decision.reason += QString(" | Road city at %1 (bridges networks!)").arg(candidate.territory);
        } else if (remaining >= fortifiedCityPrice) {
            // Risky but bridges networks - build fortified for protection
            decision.cities[candidate.territory] = true;
            remaining -= fortifiedCityPrice;
            decision.totalCost += fortifiedCityPrice;
            decision.reason += QString(" | Fortified road city at %1 (bridges networks)").arg(candidate.territory);
        }
    }

    // === PRIORITY 4: Buy troops for defense/expansion ===
    // More troops if we have at-risk territories, fewer if everything is safe
    int threatenedCount = highRiskTerritories.size() + mediumRiskTerritories.size();
    int targetTroops;

    if (threatenedCount > 0) {
        // Need troops for defense: 2-3 per threatened territory
        targetTroops = qMax(8, threatenedCount * 3 + 4);
    } else {
        // Expansion mode: minimum standing army
        targetTroops = qMax(6, ownedTerritories.size());
    }

    int troopsNeeded = targetTroops - currentTroops;

    if (troopsNeeded > 0 || enemyFortifiedCitiesNearby > 0) {
        int cavalryToBuy = 0;
        int catapultsToBuy = 0;
        int infantryToBuy = 0;

        // === CATAPULTS: Buy if enemy has fortified cities nearby ===
        // Catapults are essential for sieging fortified cities
        // Without catapults, you cannot damage fortification walls
        if (enemyFortifiedCitiesNearby > 0 && currentCatapults < 2) {
            // Buy 1-2 catapults based on how many fortified cities are nearby
            int catapultsWanted = qMin(2 - currentCatapults, enemyFortifiedCitiesNearby);
            for (int i = 0; i < catapultsWanted && remaining >= catapultPrice; i++) {
                catapultsToBuy++;
                remaining -= catapultPrice;
                decision.totalCost += catapultPrice;
                if (troopsNeeded > 0) troopsNeeded--;
            }
        }

        // === CAVALRY: Buy for mobility ===
        // Cavalry can move 2 spaces - good for:
        // 1. Rapid response to threats (threatened territories)
        // 2. Expansion when road network is incomplete
        // 3. Chasing down enemy generals
        // Target: 2-4 cavalry depending on situation
        int targetCavalry = 2;  // Base target
        if (threatenedCount > 0) {
            targetCavalry = 4;  // Need more mobility when under threat
        } else if (ownedTerritories.size() > 5 && alreadyConnected.size() < ownedTerritories.size() / 2) {
            targetCavalry = 3;  // Larger empire without good roads = need cavalry
        }

        int cavalryWanted = targetCavalry - currentCavalry;
        if (cavalryWanted > 0 && remaining >= cavalryPrice) {
            // Don't buy more cavalry than we have troops needed
            int maxCavalry = (troopsNeeded > 0) ? qMin(cavalryWanted, troopsNeeded) : cavalryWanted;
            // Cap at 2 per turn to maintain infantry/cavalry balance
            maxCavalry = qMin(maxCavalry, 2);

            for (int i = 0; i < maxCavalry && remaining >= cavalryPrice; i++) {
                cavalryToBuy++;
                remaining -= cavalryPrice;
                decision.totalCost += cavalryPrice;
                if (troopsNeeded > 0) troopsNeeded--;
            }
        }

        // === INFANTRY: Fill remaining troop needs ===
        // Infantry is the backbone - cheap and effective
        while (troopsNeeded > 0 && remaining >= infantryPrice) {
            infantryToBuy++;
            remaining -= infantryPrice;
            decision.totalCost += infantryPrice;
            troopsNeeded--;
        }

        decision.infantry += infantryToBuy;
        decision.cavalry += cavalryToBuy;
        decision.catapults += catapultsToBuy;

        if (infantryToBuy > 0 || cavalryToBuy > 0 || catapultsToBuy > 0) {
            decision.reason += QString(" | Troops: %1 inf, %2 cav, %3 cat")
                .arg(infantryToBuy).arg(cavalryToBuy).arg(catapultsToBuy);
            if (enemyFortifiedCitiesNearby > 0) {
                decision.reason += QString(" (siege targets: %1)").arg(enemyFortifiedCitiesNearby);
            }
        }
    }

    // === PRIORITY 5: Build income cities in SAFE territories ===
    // Only if we have plenty of troops AND a significant budget surplus
    // Be very conservative - troops are more important than cities early game
    int minBudgetForIncomeCity = 40;  // Must have at least 40 remaining after other purchases
    if (currentTroops >= 8 && remaining >= minBudgetForIncomeCity && decision.cities.isEmpty()) {
        // Find high-value safe territories without cities
        // Only consider 10-value territories - they pay off faster
        QList<QPair<QString, int>> cityCandidates;
        for (const QString &territory : territoriesForCities) {
            if (decision.cities.contains(territory)) continue;  // Already planning to build

            int value = graph->getValue(territory);
            if (value < 10) continue;  // Only build on high-value territories for income

            if (riskMap.contains(territory)) {
                RiskLevel risk = riskMap[territory].risk;
                // Only SAFE territories - not even low risk
                if (risk == RiskLevel::Safe) {
                    cityCandidates.append({territory, value});
                }
            }
        }

        // Sort by value descending (prefer 10-value territories)
        std::sort(cityCandidates.begin(), cityCandidates.end(),
                  [](const auto &a, const auto &b) { return a.second > b.second; });

        // Build at most 1 income city per turn
        if (!cityCandidates.isEmpty() && remaining >= cityPrice) {
            const auto &candidate = cityCandidates.first();
            decision.cities[candidate.first] = false;  // Unfortified
            remaining -= cityPrice;
            decision.totalCost += cityPrice;
            decision.reason += QString(" | Income city at %1 (val=%2)").arg(candidate.first).arg(candidate.second);
        }
    }

    // === PRIORITY 6: Galleys if needed for expansion ===
    if (currentGalleyCount < 2 && !seaTerritoriesForGalleys.isEmpty() && remaining >= galleyPrice) {
        if (currentGalleyCount == 0) {
            QString seaTerritory = seaTerritoriesForGalleys.first();
            decision.galleys[seaTerritory] = 1;
            remaining -= galleyPrice;
            decision.totalCost += galleyPrice;
            decision.reason += QString(" | Galley at %1").arg(seaTerritory);
        }
    }

    // === PRIORITY 7: Spend remaining on infantry ===
    // Don't hoard money
    int extraInfantry = 0;
    while (remaining >= infantryPrice) {
        decision.infantry++;
        extraInfantry++;
        remaining -= infantryPrice;
        decision.totalCost += infantryPrice;
    }

    if (extraInfantry > 0) {
        decision.reason += QString(" | Extra inf: %1").arg(extraInfantry);
    }

    return decision;
}

QString AIDecisionMaker::generatePurchaseReport(const AIPurchaseDecision &decision, int budget)
{
    QString report;

    report += "=== AI PURCHASE DECISION ===\n\n";
    report += QString("Budget: %1 talents\n").arg(budget);
    report += QString("Total spending: %1 talents\n").arg(decision.totalCost);
    report += QString("Remaining: %1 talents\n\n").arg(budget - decision.totalCost);

    if (decision.isEmpty()) {
        report += "No purchases planned.\n";
    } else {
        // Show cities to destroy first (important strategic decision)
        if (!decision.citiesToDestroy.isEmpty()) {
            report += "Cities to DESTROY (can't defend):\n";
            for (const QString &territory : decision.citiesToDestroy) {
                report += QString("  - DESTROY city at %1\n").arg(territory);
            }
            report += "\n";
        }

        report += "Planned purchases:\n";

        if (decision.infantry > 0) {
            report += QString("  - Infantry: %1\n").arg(decision.infantry);
        }
        if (decision.cavalry > 0) {
            report += QString("  - Cavalry: %1\n").arg(decision.cavalry);
        }
        if (decision.catapults > 0) {
            report += QString("  - Catapults: %1\n").arg(decision.catapults);
        }

        for (auto it = decision.cities.begin(); it != decision.cities.end(); ++it) {
            QString fortified = it.value() ? " (fortified)" : "";
            report += QString("  - City at %1%2\n").arg(it.key()).arg(fortified);
        }

        for (const QString &territory : decision.fortifications) {
            report += QString("  - Fortify %1\n").arg(territory);
        }

        for (auto it = decision.galleys.begin(); it != decision.galleys.end(); ++it) {
            report += QString("  - Galley at %1: %2\n").arg(it.key()).arg(it.value());
        }
    }

    report += QString("\nReason: %1\n").arg(decision.reason);

    return report;
}
