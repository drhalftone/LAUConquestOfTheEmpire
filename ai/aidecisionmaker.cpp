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
    // BUT: Generals/Caesars CANNOT capture territories without troops - they need at least 1 troop!
    if (!weOwnIt && isUndefended) {
        // Generals cannot capture without troops!
        if (move.troopsCanBring == 0) {
            // Cannot capture - skip this bonus section entirely
            // The move will still be evaluated but won't get capture bonuses
            move.reason += " | UNDEFENDED but no troops to capture";
        } else {
            // Base bonus for free capture (only if we have troops)
            move.score += 150;  // Reduced from 250 - need to consider counter-attack risk
            move.reason += " | UNDEFENDED (free capture): +150";

            // Check if the territory will be SAFE after we capture it
            // The risk assessment tells us if enemy can reach this territory
            bool safeAfterCapture = (risk.risk == RiskLevel::Safe || risk.risk == RiskLevel::Low);
            bool canBeCounterAttacked = (risk.enemyMaxForce > 0);

            // We have troops - can defend after capture
            if (move.troopsCanBring >= risk.enemyMaxForce) {
                move.score += 50;  // We can hold it
                move.reason += " | Can hold after capture: +50";
            } else if (canBeCounterAttacked && move.troopsCanBring < risk.enemyMaxForce) {
                // We might lose troops to counter-attack
                int counterAttackPenalty = -50;
                move.score += counterAttackPenalty;
                move.reason += QString(" | Risk: enemy force=%1 > our troops=%2: %3")
                    .arg(risk.enemyMaxForce).arg(move.troopsCanBring).arg(counterAttackPenalty);
            }

            // Extra bonus for cities (value + 5 income)
            if (enemyCity) {
                int cityBonus = hasFortifiedCity ? 150 : 100;  // Fortified cities worth more
                move.score += cityBonus;
                move.reason += QString(" | ENEMY CITY%1: +%2")
                    .arg(hasFortifiedCity ? " (fortified)" : "")
                    .arg(cityBonus);
            }
        }
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

// =============================================================================
// MOVEMENT PLANNING - End-State Based Approach
// =============================================================================
//
// The key insight: instead of making greedy decisions one move at a time,
// we plan the DESIRED END STATE first, then figure out how to get there.
//
// Algorithm:
// 1. Identify all valuable target territories (unowned, enemy, at-risk own)
// 2. Score and prioritize targets
// 3. For each target, find which generals can reach it
// 4. Assign generals to targets, concentrating troops on fewer generals
// 5. Execute moves in optimal order
//

QList<AIDecisionMaker::TargetTerritory> AIDecisionMaker::identifyTargets(
    Player *player,
    const QList<Player*> &allPlayers,
    MapGraph *graph,
    const QMap<QString, TerritoryRisk> &riskMap)
{
    QList<TargetTerritory> targets;
    QStringList ownedTerritories = player->getOwnedTerritories();
    QSet<QString> ownedSet(ownedTerritories.begin(), ownedTerritories.end());

    // === USE HEAT MAP DATA FOR THREAT ASSESSMENT ===
    // Get 1-turn and 2-turn enemy threat maps
    ReachabilityCalculator calc;
    QMap<QString, int> enemyThreat1Turn = calc.getEnemyThreatMap(player, allPlayers, graph, 1);
    QMap<QString, int> enemyThreat2Turn = calc.getEnemyThreatMap(player, allPlayers, graph, 2);

    qDebug() << "=== THREAT MAP ANALYSIS ===";
    qDebug() << "Territories with 1-turn enemy threat:" << enemyThreat1Turn.size();
    qDebug() << "Territories with 2-turn enemy threat:" << enemyThreat2Turn.size();

    // Get all territories from the graph
    QStringList allTerritories = graph->getTerritoryNames();

    for (const QString &territory : allTerritories) {
        // Skip sea territories
        if (graph->isSeaTerritory(territory)) continue;

        TargetTerritory target;
        target.name = territory;
        target.enemyTroops = countEnemyTroopsAt(territory, player, allPlayers);
        target.requiresTroops = hasEnemyPresenceAt(territory, player, allPlayers);

        int value = graph->getValue(territory);
        bool weOwnIt = ownedSet.contains(territory);

        // Check for cities
        Player *cityOwner = findCityOwnerAt(territory, allPlayers);
        bool hasEnemyCity = (cityOwner != nullptr && cityOwner != player);
        bool hasOurCity = (cityOwner == player);

        // === USE THREAT MAPS DIRECTLY ===
        int threat1Turn = enemyThreat1Turn.value(territory, 0);
        int threat2Turn = enemyThreat2Turn.value(territory, 0);
        bool isSafeExpansion = (threat1Turn == 0);  // No enemy can reach in 1 turn = SAFE

        // DEBUG: Monitor Italia specifically
        if (territory == "Italia") {
            qDebug() << "[ITALIA DEBUG in identifyTargets]";
            qDebug() << "  weOwnIt:" << weOwnIt;
            qDebug() << "  threat1Turn:" << threat1Turn;
            qDebug() << "  threat2Turn:" << threat2Turn;
            qDebug() << "  isSafeExpansion:" << isSafeExpansion;
            qDebug() << "  enemyTroops (current):" << target.enemyTroops;
            qDebug() << "  requiresTroops:" << target.requiresTroops;
            qDebug() << "  hasEnemyCity:" << hasEnemyCity;
            qDebug() << "  hasOurCity:" << hasOurCity;
        }

        // Get risk level from riskMap for compatibility
        RiskLevel risk = RiskLevel::Safe;
        int enemyMaxForce = 0;
        if (riskMap.contains(territory)) {
            risk = riskMap[territory].risk;
            enemyMaxForce = riskMap[territory].enemyMaxForce;
        }

        // DEBUG: More Italia info
        if (territory == "Italia") {
            qDebug() << "  riskMap contains Italia:" << riskMap.contains(territory);
            qDebug() << "  risk level:" << static_cast<int>(risk);
            qDebug() << "  enemyMaxForce from riskMap:" << enemyMaxForce;
        }

        if (weOwnIt) {
            // === DEFEND: Our territory under threat ===
            // ONLY defend if there's a 1-turn threat (immediate danger)
            // 2-turn threats are used as tiebreaker only, not primary reason to defend
            bool needsDefense = (threat1Turn > 0);  // Immediate threat ONLY

            if (needsDefense) {
                target.type = "Defend";
                target.score = 50;  // Base defense value

                // Higher priority for cities
                if (hasOurCity) {
                    City *city = player->getCityAtTerritory(territory);
                    target.score += city->isFortified() ? 150 : 100;
                }

                // Use 1-turn threat level for primary scoring
                if (threat1Turn >= 4) {
                    target.score += 150;  // Heavy immediate threat
                } else if (threat1Turn >= 2) {
                    target.score += 100;  // Moderate immediate threat
                } else if (threat1Turn > 0) {
                    target.score += 50;   // Light immediate threat
                }

                // 2-turn threat as TIEBREAKER only (small bonus)
                // This helps prioritize between territories with similar 1-turn threats
                if (threat2Turn > threat1Turn) {
                    target.score += qMin(20, (threat2Turn - threat1Turn) * 2);
                }

                // Higher priority for home province
                if (territory == player->getHomeProvinceName()) {
                    target.score += 200;
                }

                // Troops needed = based on 1-turn threat (what we're actually defending against)
                target.troopsNeeded = threat1Turn;
                target.requiresTroops = true;  // Defense requires troops

                targets.append(target);
            }
            // Safe own territory (no 1-turn threat) - not a defense target
        } else {
            // === EXPAND or ATTACK: Not our territory ===
            if (target.enemyTroops == 0 && !target.requiresTroops) {
                // EXPAND: Undefended territory - can capture with lone general
                target.type = "Expand";
                target.score = 100 + (value * 10);  // Higher value = higher priority
                target.troopsNeeded = 0;

                // === KEY INSIGHT: Use heat map for safe expansion ===
                // If enemy 1-turn threat is 0, this is SAFE to take with a lone general
                if (isSafeExpansion) {
                    target.score += 100;  // BIG bonus for truly safe expansion
                    qDebug() << "SAFE EXPANSION:" << territory
                             << "(value=" << value << ") - no enemy can reach in 1 turn";
                } else {
                    // Risky expansion - enemy can counter-attack
                    // But capturing contested territory is still valuable:
                    // - Gains income for at least 1 turn
                    // - Forces enemy to respond/recapture
                    // - Projects threat toward enemy

                    // Lighter penalty - capturing is still worth it even if we might lose it
                    // Only heavy penalty if we'd be massively outnumbered
                    int penalty = 0;
                    if (threat1Turn >= 5) {
                        // Heavy enemy presence - risky
                        penalty = 40;
                    } else if (threat1Turn >= 3) {
                        // Moderate threat - small penalty
                        penalty = 20;
                    }
                    // Light threat (1-2) - no penalty, capturing is worth it

                    target.score -= penalty;
                    qDebug() << "RISKY EXPANSION:" << territory
                             << "- enemy threat:" << threat1Turn << "(penalty:" << penalty << ")";
                }

                // Bonus for enemy cities (even undefended)
                if (hasEnemyCity) {
                    target.score += 80;
                }

                targets.append(target);

                // DEBUG: Italia final decision for Expand
                if (territory == "Italia") {
                    qDebug() << "[ITALIA] Decision: EXPAND";
                    qDebug() << "  final score:" << target.score;
                    qDebug() << "  isSafeExpansion:" << isSafeExpansion;
                }
            } else {
                // ATTACK: Has defenders - requires troops
                target.type = "Attack";
                target.score = 80 + (value * 10);
                target.troopsNeeded = target.enemyTroops + 1;  // Need advantage
                target.requiresTroops = true;

                // Big bonus for enemy cities
                if (hasEnemyCity) {
                    target.score += 100;
                }

                // Penalty based on enemy reinforcement capability (2-turn threat)
                if (threat2Turn > threat1Turn) {
                    // Enemy can bring MORE troops in 2 turns - risky attack
                    int penalty = (threat2Turn - threat1Turn) * 10;
                    target.score -= penalty;
                }

                targets.append(target);

                // DEBUG: Italia final decision for Attack
                if (territory == "Italia") {
                    qDebug() << "[ITALIA] Decision: ATTACK";
                    qDebug() << "  final score:" << target.score;
                    qDebug() << "  troopsNeeded:" << target.troopsNeeded;
                }
            }
        }

        // DEBUG: Italia - if we own it and didn't add as defense target, log why
        if (territory == "Italia" && weOwnIt) {
            bool needsDefense = (threat1Turn > 0);
            qDebug() << "[ITALIA] We own it:";
            qDebug() << "  needsDefense (1-turn threat > 0):" << needsDefense;
            qDebug() << "  2-turn threat (tiebreaker only):" << threat2Turn;
            if (!needsDefense) {
                qDebug() << "  -> NOT added as target (safe, no immediate 1-turn threat)";
            }
        }
    }

    // Sort by score descending
    std::sort(targets.begin(), targets.end(),
              [](const TargetTerritory &a, const TargetTerritory &b) {
                  return a.score > b.score;
              });

    return targets;
}

int AIDecisionMaker::countAvailableTroopsAt(const QString &territory, Player *player)
{
    if (!player) return 0;

    int count = 0;

    // Count all troops at this territory
    for (InfantryPiece *inf : player->getInfantry()) {
        if (inf->getTerritoryName() == territory && inf->getMovesRemaining() > 0) {
            count++;
        }
    }
    for (CavalryPiece *cav : player->getCavalry()) {
        if (cav->getTerritoryName() == territory && cav->getMovesRemaining() > 0) {
            count++;
        }
    }
    for (CatapultPiece *cat : player->getCatapults()) {
        if (cat->getTerritoryName() == territory && cat->getMovesRemaining() > 0) {
            count++;
        }
    }

    return count;
}

void AIDecisionMaker::assignTroopsToGeneral(GeneralAssignment &assignment, Player *player, int maxTroops, bool requiresMultiHop)
{
    if (!player || !assignment.general) return;

    QString territory = assignment.general->getTerritoryName();
    int troopsAssigned = 0;

    // Get IDs of troops already in OTHER generals' legions
    QSet<int> assignedElsewhere;
    for (GeneralPiece *gen : player->getGenerals()) {
        if (gen != assignment.general) {
            for (int id : gen->getLegion()) {
                assignedElsewhere.insert(id);
            }
        }
    }
    for (CaesarPiece *caesar : player->getCaesars()) {
        for (int id : caesar->getLegion()) {
            assignedElsewhere.insert(id);
        }
    }

    // For multi-hop routes (2+ moves to reach target), ONLY use cavalry
    // Infantry/catapults only have 1 move and will be left behind!
    if (requiresMultiHop) {
        qDebug() << "assignTroopsToGeneral: Multi-hop mission - only assigning cavalry (2 moves)";
        for (CavalryPiece *cav : player->getCavalry()) {
            if (troopsAssigned >= maxTroops) break;
            if (cav->getTerritoryName() != territory) continue;
            if (cav->getMovesRemaining() < 2) continue;  // Need FULL 2 moves for multi-hop
            if (assignedElsewhere.contains(cav->getUniqueId())) continue;

            assignment.troopIds.append(cav->getUniqueId());
            troopsAssigned++;
        }
        assignment.troopsToTake = troopsAssigned;
        return;
    }

    // Single-hop route: Priority: catapults > cavalry > infantry
    // First add catapults
    for (CatapultPiece *cat : player->getCatapults()) {
        if (troopsAssigned >= maxTroops) break;
        if (cat->getTerritoryName() != territory) continue;
        if (cat->getMovesRemaining() <= 0) continue;
        if (assignedElsewhere.contains(cat->getUniqueId())) continue;

        assignment.troopIds.append(cat->getUniqueId());
        troopsAssigned++;
    }

    // Then cavalry
    for (CavalryPiece *cav : player->getCavalry()) {
        if (troopsAssigned >= maxTroops) break;
        if (cav->getTerritoryName() != territory) continue;
        if (cav->getMovesRemaining() <= 0) continue;
        if (assignedElsewhere.contains(cav->getUniqueId())) continue;

        assignment.troopIds.append(cav->getUniqueId());
        troopsAssigned++;
    }

    // Then infantry
    for (InfantryPiece *inf : player->getInfantry()) {
        if (troopsAssigned >= maxTroops) break;
        if (inf->getTerritoryName() != territory) continue;
        if (inf->getMovesRemaining() <= 0) continue;
        if (assignedElsewhere.contains(inf->getUniqueId())) continue;

        assignment.troopIds.append(inf->getUniqueId());
        troopsAssigned++;
    }

    assignment.troopsToTake = troopsAssigned;
}

MovementPlan AIDecisionMaker::planMovement(Player *player, const QList<Player*> &allPlayers, MapGraph *graph)
{
    MovementPlan plan;

    if (!player || !graph) {
        plan.summary = "Invalid player or graph";
        return plan;
    }

    qDebug() << "=== PER-TROOP MOVEMENT PLANNING ===";

    // Step 1: Get risk assessment and identify targets
    ReachabilityCalculator calc;
    QMap<QString, TerritoryRisk> riskMap = calc.assessAllTerritories(player, allPlayers, graph);
    QList<TargetTerritory> targets = identifyTargets(player, allPlayers, graph, riskMap);

    qDebug() << "Identified" << targets.size() << "potential targets";
    for (int i = 0; i < qMin(5, targets.size()); i++) {
        const TargetTerritory &t = targets[i];
        qDebug() << QString("  %1. %2: score=%3").arg(i+1).arg(t.name).arg(t.score);
    }

    QString homeProvince = player->getHomeProvinceName();

    // Step 2: Collect all available troops with their locations
    struct TroopInfo {
        GamePiece *piece;
        QString territory;
        int moves;  // 1 for infantry/catapult, 2 for cavalry
        QString assignedDestination;  // First step - will be filled in per-troop planning
        QString ultimateTarget;       // Final destination we're moving toward
        int effectiveScore;           // Score for this troop's assignment
    };
    QList<TroopInfo> availableTroops;

    for (InfantryPiece *inf : player->getInfantry()) {
        if (inf->getMovesRemaining() > 0) {
            availableTroops.append({inf, inf->getTerritoryName(), 1, QString(), QString(), 0});
        }
    }
    for (CavalryPiece *cav : player->getCavalry()) {
        if (cav->getMovesRemaining() > 0) {
            availableTroops.append({cav, cav->getTerritoryName(), 2, QString(), QString(), 0});
        }
    }
    for (CatapultPiece *cat : player->getCatapults()) {
        if (cat->getMovesRemaining() > 0) {
            availableTroops.append({cat, cat->getTerritoryName(), 1, QString(), QString(), 0});
        }
    }

    qDebug() << "Available troops:" << availableTroops.size();

    // Step 3: Collect all available generals with their locations
    QList<GeneralPiece*> availableGenerals;
    for (GeneralPiece *gen : player->getGenerals()) {
        if (gen->getMovesRemaining() >= 1.0) {
            availableGenerals.append(gen);
        }
    }

    qDebug() << "Available generals:" << availableGenerals.size();

    if (availableGenerals.isEmpty()) {
        plan.summary = "No generals available";
        return plan;
    }

    // Build a map of target scores by name for quick lookup
    QMap<QString, int> targetScores;
    for (const TargetTerritory &t : targets) {
        targetScores[t.name] = t.score;
    }

    // Step 4: For EACH troop, find the best destination based on RISK MAP
    // Look at distant high-risk territories and move TOWARD them
    // After assigning a troop, reduce the risk (since reinforcements are coming)

    // Create a mutable copy of risk scores that we'll update as troops are assigned
    QMap<QString, int> effectiveRiskScore;  // territory -> current risk priority
    for (auto it = riskMap.begin(); it != riskMap.end(); ++it) {
        const TerritoryRisk &risk = it.value();
        int score = 0;

        // High/Medium risk territories we own MAY need reinforcement
        // BUT only if we don't already have enough troops there!
        if (player->ownsTerritory(it.key())) {
            int ourForce = risk.ourMaxForce;
            int enemyForce = risk.enemyMaxForce;

            // Only prioritize defense if we're OUTNUMBERED or close to it
            // If we already have more troops than enemy can bring, no need to pile more in
            int forceDeficit = enemyForce - ourForce;

            if (risk.risk == RiskLevel::High) {
                if (forceDeficit > 0) {
                    // We're outnumbered - HIGH priority to reinforce
                    score = 300 + forceDeficit * 50;
                } else if (forceDeficit >= -2) {
                    // Close match - medium priority
                    score = 100;
                }
                // else we have comfortable advantage - no extra defense needed
            } else if (risk.risk == RiskLevel::Medium) {
                if (forceDeficit > 0) {
                    // Outnumbered in medium risk - should reinforce
                    score = 150 + forceDeficit * 30;
                } else if (forceDeficit >= -1) {
                    // Very close - small bonus
                    score = 50;
                }
                // else we're fine - focus on expansion instead
            }
        }

        // Also consider target territories (expansion)
        // Expansion scores should be competitive with defense when we're already well-defended
        if (targetScores.contains(it.key())) {
            score = qMax(score, targetScores[it.key()]);
        }
        effectiveRiskScore[it.key()] = score;
    }
    // Add target scores for territories not in risk map
    for (auto it = targetScores.begin(); it != targetScores.end(); ++it) {
        if (!effectiveRiskScore.contains(it.key())) {
            effectiveRiskScore[it.key()] = it.value();
        }
    }

    // Helper: BFS to find the first step toward a distant territory
    auto findFirstStepToward = [&graph](const QString &from, const QString &target) -> QString {
        if (from == target) return QString();

        // BFS to find shortest path
        QMap<QString, QString> cameFrom;  // territory -> previous territory
        QList<QString> queue;
        queue.append(from);
        cameFrom[from] = "";

        while (!queue.isEmpty()) {
            QString current = queue.takeFirst();

            if (current == target) {
                // Backtrack to find first step
                QString step = current;
                while (cameFrom.contains(step) && cameFrom[step] != from && !cameFrom[step].isEmpty()) {
                    step = cameFrom[step];
                }
                return step;
            }

            for (const QString &neighbor : graph->getNeighbors(current)) {
                if (graph->isSeaTerritory(neighbor)) continue;
                if (cameFrom.contains(neighbor)) continue;

                cameFrom[neighbor] = current;
                queue.append(neighbor);
            }
        }
        return QString();  // No path found
    };

    // Helper: Calculate distance (in hops) between two territories
    auto getDistance = [&graph](const QString &from, const QString &target) -> int {
        if (from == target) return 0;

        QMap<QString, int> dist;
        QList<QString> queue;
        queue.append(from);
        dist[from] = 0;

        while (!queue.isEmpty()) {
            QString current = queue.takeFirst();

            if (current == target) {
                return dist[current];
            }

            for (const QString &neighbor : graph->getNeighbors(current)) {
                if (graph->isSeaTerritory(neighbor)) continue;
                if (dist.contains(neighbor)) continue;

                dist[neighbor] = dist[current] + 1;
                queue.append(neighbor);

                // Limit search depth
                if (dist[neighbor] > 5) continue;
            }
        }
        return 999;  // Unreachable
    };

    QMap<QString, int> troopsGoingTo;  // first-step destination -> count
    QMap<QString, QString> troopsLeavingFrom;  // destination -> source territory (to detect swaps)

    // Check where Caesar is - troops should prefer staying with Caesar for protection
    QString caesarTerritory;
    for (CaesarPiece *caesar : player->getCaesars()) {
        caesarTerritory = caesar->getTerritoryName();
        break;  // Only one Caesar per player
    }

    // Calculate how many troops should stay home to defend Caesar based on RISK
    // Use the risk map to determine enemy threat to home province
    int troopsToKeepHome = 0;
    int enemyThreatToHome = 0;
    if (!caesarTerritory.isEmpty() && riskMap.contains(caesarTerritory)) {
        const TerritoryRisk &homeRisk = riskMap[caesarTerritory];
        enemyThreatToHome = homeRisk.enemyMaxForce;

        // Keep enough troops to match enemy threat, plus a small buffer
        // If enemy can bring 5 troops, we want at least 5-6 at home
        if (homeRisk.risk == RiskLevel::High) {
            troopsToKeepHome = enemyThreatToHome + 2;  // Extra buffer for high risk
        } else if (homeRisk.risk == RiskLevel::Medium) {
            troopsToKeepHome = enemyThreatToHome + 1;
        } else if (homeRisk.risk == RiskLevel::Low) {
            troopsToKeepHome = qMax(0, enemyThreatToHome);  // Match threat
        } else {
            troopsToKeepHome = 0;  // Safe - no need to keep troops home
        }

        qDebug() << QString("Home defense: Caesar at %1, enemy threat=%2, risk=%3, keeping %4 troops home")
            .arg(caesarTerritory)
            .arg(enemyThreatToHome)
            .arg(static_cast<int>(homeRisk.risk))
            .arg(troopsToKeepHome);
    }

    // Count how many troops are currently at home
    int troopsAtHome = 0;
    for (const TroopInfo &t : availableTroops) {
        if (t.territory == caesarTerritory) {
            troopsAtHome++;
        }
    }

    // Track how many home troops we've assigned to leave
    int homeTroopsAssigned = 0;
    int homeTroopsAvailableToLeave = qMax(0, troopsAtHome - troopsToKeepHome);

    qDebug() << QString("Home troops: %1 total, %2 can leave (keeping %3 for defense)")
        .arg(troopsAtHome).arg(homeTroopsAvailableToLeave).arg(troopsToKeepHome);

    for (int i = 0; i < availableTroops.size(); i++) {
        TroopInfo &troop = availableTroops[i];

        QString bestDest;
        QString bestUltimateTarget;
        int bestScore = -9999;
        int bestDistance = 999;

        bool troopIsWithCaesar = (troop.territory == caesarTerritory);

        // Look at ALL territories with risk/target scores, not just adjacent
        for (auto it = effectiveRiskScore.begin(); it != effectiveRiskScore.end(); ++it) {
            const QString &ultimateTarget = it.key();
            int baseScore = it.value();

            if (baseScore <= 0) continue;

            // Find distance and first step
            int distance = getDistance(troop.territory, ultimateTarget);
            if (distance == 0 || distance > 5) continue;  // Skip if we're already there or too far

            QString firstStep = findFirstStepToward(troop.territory, ultimateTarget);
            if (firstStep.isEmpty()) continue;

            // PREVENT SWAPS: Don't move to a territory if troops from there are coming here
            // This prevents useless A->B, B->A swaps that accomplish nothing
            if (troopsLeavingFrom.contains(firstStep) && troopsLeavingFrom[firstStep] == troop.territory) {
                // Someone from firstStep is already going to our territory - don't swap!
                continue;
            }

            // Discount score by distance (closer targets are more valuable)
            // But still consider distant targets - just at reduced priority
            int effectiveScore = baseScore / distance;

            // Apply diminishing returns for troops already going to same first step
            int alreadyGoing = troopsGoingTo.value(firstStep, 0);
            if (alreadyGoing > 0) {
                effectiveScore = static_cast<int>(effectiveScore / (1.0 + alreadyGoing * 0.3));
            }

            if (effectiveScore > bestScore ||
                (effectiveScore == bestScore && distance < bestDistance)) {
                bestScore = effectiveScore;
                bestDest = firstStep;
                bestUltimateTarget = ultimateTarget;
                bestDistance = distance;
            }
        }

        // If troop is with Caesar, check if we've already sent enough troops away
        // Keep troopsToKeepHome at home for defense
        if (troopIsWithCaesar) {
            if (homeTroopsAssigned >= homeTroopsAvailableToLeave) {
                // We've sent enough troops - this one stays to defend Caesar
                qDebug() << QString("  Troop %1 at %2 STAYING with Caesar (already sent %3, max=%4)")
                    .arg(troop.piece->getUniqueId())
                    .arg(troop.territory)
                    .arg(homeTroopsAssigned)
                    .arg(homeTroopsAvailableToLeave);
                continue;  // Don't assign this troop - they stay home
            }
        }

        if (!bestDest.isEmpty() && bestScore > 0) {
            troop.assignedDestination = bestDest;
            troop.ultimateTarget = bestUltimateTarget;
            troop.effectiveScore = bestScore;
            troopsGoingTo[bestDest]++;

            // Track movement direction to prevent swaps
            // Record: troops going TO bestDest are coming FROM troop.territory
            if (!troopsLeavingFrom.contains(bestDest)) {
                troopsLeavingFrom[bestDest] = troop.territory;
            }

            // Track home troops leaving
            if (troopIsWithCaesar) {
                homeTroopsAssigned++;
            }

            // REDUCE the risk score for the ultimate target since we're sending reinforcement
            // This makes the next troop iteration see different priorities
            if (effectiveRiskScore.contains(bestUltimateTarget)) {
                int oldScore = effectiveRiskScore[bestUltimateTarget];
                int newScore = static_cast<int>(oldScore * 0.6);  // 40% reduction per troop
                effectiveRiskScore[bestUltimateTarget] = newScore;
            }

            QString pieceType = "?";
            if (troop.piece->getType() == GamePiece::Type::Infantry) pieceType = "Infantry";
            else if (troop.piece->getType() == GamePiece::Type::Cavalry) pieceType = "Cavalry";
            else if (troop.piece->getType() == GamePiece::Type::Catapult) pieceType = "Catapult";

            qDebug() << QString("  Troop %1 (%2) at %3 -> %4 (toward %5, dist=%6, score=%7)")
                .arg(troop.piece->getUniqueId())
                .arg(pieceType)
                .arg(troop.territory)
                .arg(bestDest)
                .arg(bestUltimateTarget)
                .arg(bestDistance)
                .arg(bestScore);
        } else {
            qDebug() << QString("  Troop %1 at %2: no valid destination found")
                .arg(troop.piece->getUniqueId())
                .arg(troop.territory);
        }
    }

    // Step 5: Group troops by (from, to) pairs
    // Key: "from|to", Value: list of troop indices
    struct TroopGroup {
        QString from;
        QString to;
        QList<int> troopIndices;  // indices into availableTroops
        int totalScore;  // Sum of effective scores for troops in this group
    };
    QMap<QString, TroopGroup> groupMap;

    for (int i = 0; i < availableTroops.size(); i++) {
        const TroopInfo &troop = availableTroops[i];
        if (troop.assignedDestination.isEmpty()) continue;

        QString key = troop.territory + "|" + troop.assignedDestination;
        if (!groupMap.contains(key)) {
            groupMap[key] = TroopGroup{troop.territory, troop.assignedDestination, {}, 0};
        }
        groupMap[key].troopIndices.append(i);
        // Use the troop's effective score (based on ultimate target), not the first step's score
        groupMap[key].totalScore += troop.effectiveScore;
    }

    // Convert to list and sort by total score (highest first)
    QList<TroopGroup> groups = groupMap.values();
    std::sort(groups.begin(), groups.end(),
              [](const TroopGroup &a, const TroopGroup &b) {
                  return a.totalScore > b.totalScore;
              });

    qDebug() << "=== TROOP GROUPS ===";
    for (const TroopGroup &g : groups) {
        qDebug() << QString("  %1 -> %2: %3 troops (score=%4)")
            .arg(g.from).arg(g.to).arg(g.troopIndices.size()).arg(g.totalScore);
    }

    // Step 6: Assign generals to each group
    QSet<GeneralPiece*> assignedGenerals;
    QSet<int> assignedTroopIds;

    for (const TroopGroup &group : groups) {
        if (group.troopIndices.isEmpty()) continue;

        // Find a general at the 'from' territory who isn't assigned yet
        GeneralPiece *bestGeneral = nullptr;
        for (GeneralPiece *gen : availableGenerals) {
            if (assignedGenerals.contains(gen)) continue;
            if (gen->getTerritoryName() == group.from) {
                bestGeneral = gen;
                break;
            }
        }

        if (!bestGeneral) {
            qDebug() << "  No general at" << group.from << "- cannot move" << group.troopIndices.size() << "troops to" << group.to;
            continue;
        }

        // Create assignment
        GeneralAssignment assignment;
        assignment.general = bestGeneral;
        assignment.targetTerritory = group.to;
        assignment.missionType = "ExpandWithTroops";
        assignment.priority = group.totalScore;

        // Assign troops (up to 5, leaving room for general in legion of 6)
        int troopsToAssign = qMin(5, group.troopIndices.size());
        for (int i = 0; i < troopsToAssign; i++) {
            int troopIdx = group.troopIndices[i];
            const TroopInfo &troop = availableTroops[troopIdx];
            if (!assignedTroopIds.contains(troop.piece->getUniqueId())) {
                assignment.troopIds.append(troop.piece->getUniqueId());
                assignedTroopIds.insert(troop.piece->getUniqueId());
            }
        }
        assignment.troopsToTake = assignment.troopIds.size();

        if (assignment.troopsToTake == 0) {
            qDebug() << "  No troops available to assign for" << group.from << "->" << group.to;
            continue;
        }

        assignment.reason = QString("Move %1 troops from %2 to %3")
            .arg(assignment.troopsToTake).arg(group.from).arg(group.to);

        plan.assignments.append(assignment);
        assignedGenerals.insert(bestGeneral);
        plan.generalsUsed++;
        plan.territoriesTargeted++;
        plan.totalTroopsDeployed += assignment.troopsToTake;

        qDebug() << "  Assigned General #" << bestGeneral->getNumber()
                 << "to take" << assignment.troopsToTake << "troops from" << group.from << "to" << group.to;
    }

    // Step 7: Generals without troops stay in place
    for (GeneralPiece *gen : availableGenerals) {
        if (assignedGenerals.contains(gen)) continue;

        GeneralAssignment assignment;
        assignment.general = gen;
        assignment.targetTerritory = gen->getTerritoryName();
        assignment.missionType = "StayHome";
        assignment.priority = 0;
        assignment.troopsToTake = 0;
        assignment.reason = "No troops to move";

        plan.assignments.append(assignment);
        qDebug() << "  General #" << gen->getNumber() << "staying at" << gen->getTerritoryName() << "(no troops)";
    }

    // Sort by priority for execution
    std::sort(plan.assignments.begin(), plan.assignments.end(),
              [](const GeneralAssignment &a, const GeneralAssignment &b) {
                  return a.priority > b.priority;
              });

    plan.summary = QString("Plan: %1 generals moving, %2 troops deployed to %3 destinations")
        .arg(plan.generalsUsed).arg(plan.totalTroopsDeployed).arg(troopsGoingTo.size());

    qDebug() << "=== PLAN COMPLETE ===" << plan.summary;

    return plan;
}

ScoredMove AIDecisionMaker::getNextMoveFromPlan(const MovementPlan &plan, Player *player, MapGraph *graph)
{
    ScoredMove move;

    if (!player || !graph || plan.isEmpty()) {
        return move;  // Invalid
    }

    // Find the first assignment where the general hasn't reached their target yet
    // and still has moves remaining
    for (const GeneralAssignment &assignment : plan.assignments) {
        if (!assignment.isValid()) continue;

        GamePiece *general = assignment.general;
        if (general->getMovesRemaining() < 1.0) continue;

        QString currentTerritory = general->getTerritoryName();
        QString targetTerritory = assignment.targetTerritory;

        // Skip if already at target
        if (currentTerritory == targetTerritory) continue;

        // Skip "StayHome" missions
        if (assignment.missionType == "StayHome") continue;

        // Check if target is adjacent (reachable in one move)
        QList<QString> neighbors = graph->getNeighbors(currentTerritory);
        bool targetIsAdjacent = false;
        for (const QString &neighbor : neighbors) {
            if (neighbor == targetTerritory && !graph->isSeaTerritory(neighbor)) {
                targetIsAdjacent = true;
                break;
            }
        }

        QString nextStep = targetTerritory;

        if (!targetIsAdjacent) {
            // Target is 2+ moves away - find the best intermediate step
            // Use BFS to find shortest path to target
            QMap<QString, QString> cameFrom;  // territory -> previous territory
            QList<QString> toVisit;
            QSet<QString> visited;

            toVisit.append(currentTerritory);
            visited.insert(currentTerritory);
            cameFrom[currentTerritory] = "";

            bool found = false;
            while (!toVisit.isEmpty() && !found) {
                QString current = toVisit.takeFirst();
                QList<QString> currentNeighbors = graph->getNeighbors(current);

                for (const QString &neighbor : currentNeighbors) {
                    if (graph->isSeaTerritory(neighbor)) continue;
                    if (visited.contains(neighbor)) continue;

                    visited.insert(neighbor);
                    cameFrom[neighbor] = current;
                    toVisit.append(neighbor);

                    if (neighbor == targetTerritory) {
                        found = true;
                        break;
                    }
                }
            }

            if (found) {
                // Trace back path to find first step
                QString step = targetTerritory;
                while (cameFrom.contains(step) && cameFrom[step] != currentTerritory) {
                    step = cameFrom[step];
                }
                nextStep = step;
                qDebug() << "Multi-hop path: General needs to go through" << nextStep << "to reach" << targetTerritory;
            } else {
                qDebug() << "WARNING: No path found from" << currentTerritory << "to" << targetTerritory;
                continue;  // Skip this assignment, try next
            }
        }

        // This general needs to move - return this as the next move
        move.leader = general;
        move.destination = nextStep;  // Use the next step, not final target
        move.troopsCanBring = assignment.troopsToTake;
        move.score = assignment.priority;
        move.reason = assignment.reason;

        qDebug() << "Next move from plan: General #"
                 << static_cast<GeneralPiece*>(general)->getNumber()
                 << "from" << currentTerritory << "to" << nextStep
                 << (nextStep != targetTerritory ? QString("(heading to %1)").arg(targetTerritory) : "");

        return move;
    }

    // No more moves in plan
    return move;
}
