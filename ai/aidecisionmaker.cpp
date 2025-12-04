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
            continue;
        }

        QMap<QString, ReachInfo> reachable = calc.getReachableFrom(general, graph, player);

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
            // Retreating to our own safer territory - CRITICAL for lone generals
            // This needs to be a DOMINANT bonus to ensure retreat happens FIRST
            int retreatBonus = 500;
            move.score += retreatBonus;
            move.reason = QString("URGENT RETREAT from danger (enemy=%1): +%2").arg(currentEnemyMaxForce).arg(retreatBonus);
            qDebug() << "URGENT Retreat bonus for" << leader->getTerritoryName() << "->" << destination
                     << ": +" << retreatBonus << "(enemy force=" << currentEnemyMaxForce << ")";

            // === PRIORITIZE RETREATING TOWARD HOME ===
            QString homeProvince = player->getHomeProvinceName();
            int distFromCurrent = graph->getDistance(currentTerritory, homeProvince);
            int distFromDest = graph->getDistance(destination, homeProvince);

            // Bonus for moving closer to home
            if (distFromDest >= 0 && distFromCurrent >= 0 && distFromDest < distFromCurrent) {
                int homeBonus = (distFromCurrent - distFromDest) * 50;  // 50 points per step closer
                move.score += homeBonus;
                move.reason += QString(" | Closer to home (%1->%2 steps): +%3")
                    .arg(distFromCurrent).arg(distFromDest).arg(homeBonus);

                // Extra bonus if using road network (faster retreat)
                QStringList roadFromDest = graph->getRoadConnectedTerritories(destination, player);
                if (roadFromDest.contains(homeProvince)) {
                    int roadBonus = 75;
                    move.score += roadBonus;
                    move.reason += QString(" | Road to home: +%1").arg(roadBonus);
                }
            }

            // Big bonus if destination IS home
            if (destination == homeProvince) {
                int homeSafetyBonus = 150;
                move.score += homeSafetyBonus;
                move.reason += QString(" | Reached HOME: +%1").arg(homeSafetyBonus);
            }
        } else if (destIsSafer) {
            // Retreating to neutral/enemy but safer territory
            int retreatBonus = 350;
            move.score += retreatBonus;
            move.reason = QString("Escape to safer ground (enemy=%1): +%2").arg(currentEnemyMaxForce).arg(retreatBonus);
        } else {
            // Moving to another dangerous position as a lone general - BAD IDEA
            // Strong penalty to discourage this
            int dangerPenalty = -400;
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

    // Check if this general needs more troops (has NONE currently)
    // IMPORTANT: Only generals with 0 troops should seek more - otherwise they vacillate
    // between home and the front lines endlessly. A general with 1+ troops should continue
    // their mission rather than going home for more troops.
    bool needsTroops = (move.troopsCanBring == 0);  // Only generals with NO troops should seek more
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
            }
        }
        for (CavalryPiece *cav : player->getCavalry()) {
            if (cav->getTerritoryName() == territory) {
                count++;
            }
        }
        for (CatapultPiece *cat : player->getCatapults()) {
            if (cat->getTerritoryName() == territory) {
                count++;
            }
        }
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

    // === PRIORITY 3: Build cities strategically ===
    //
    // City placement strategy:
    // 1. Find the territory most at risk (HIGH > MEDIUM, closer to home wins ties)
    // 2. Find shortest path from home city to that territory
    // 3. Place UNFORTIFIED cities along this path, starting from home and extending outward
    // 4. FORTIFIED cities can be placed standalone for defense at threatened locations
    //
    // Rules:
    // - UNFORTIFIED cities = Road network extenders (must be adjacent to existing city)
    // - FORTIFIED cities = Defensive strongholds (can be standalone for protection)

    // Find all territories with our cities
    QSet<QString> territoriesWithCities;
    for (City *city : player->getCities()) {
        territoriesWithCities.insert(city->getTerritoryName());
    }
    // Also include cities we've already decided to build this turn
    for (auto it = decision.cities.begin(); it != decision.cities.end(); ++it) {
        territoriesWithCities.insert(it.key());
    }

    // Get territories connected to home via road network
    QStringList homeRoadNetwork = graph->getRoadConnectedTerritories(homeProvince, player);
    QSet<QString> connectedToHome(homeRoadNetwork.begin(), homeRoadNetwork.end());
    connectedToHome.insert(homeProvince);

    // === STEP 1: Find the most at-risk territory we own ===
    // Priority: HIGH risk first, then MEDIUM risk
    // Tie-breaker: Closer to home (shorter path = higher priority)
    struct RiskTarget {
        QString territory;
        RiskLevel risk;
        int distanceFromHome;
        int enemyForce;
        int ourForce;
    };
    QList<RiskTarget> riskTargets;

    for (const QString &territory : ownedTerritories) {
        if (!riskMap.contains(territory)) continue;

        RiskLevel risk = riskMap[territory].risk;
        if (risk == RiskLevel::High || risk == RiskLevel::Medium) {
            RiskTarget target;
            target.territory = territory;
            target.risk = risk;
            target.enemyForce = riskMap[territory].enemyMaxForce;
            target.ourForce = riskMap[territory].ourMaxForce;

            // Calculate distance from home (or from nearest city on road network)
            if (connectedToHome.contains(territory)) {
                target.distanceFromHome = 0;  // Already on road network
            } else {
                // Find shortest path from any city on road network to this territory
                int minDistance = INT_MAX;
                for (const QString &cityTerritory : territoriesWithCities) {
                    if (!ownedSet.contains(cityTerritory)) continue;  // Must own both endpoints
                    int dist = graph->getDistance(cityTerritory, territory);
                    if (dist >= 0 && dist < minDistance) {
                        minDistance = dist;
                    }
                }
                target.distanceFromHome = (minDistance == INT_MAX) ? 999 : minDistance;
            }

            riskTargets.append(target);
        }
    }

    // Sort: HIGH risk first, then by distance (closer = higher priority)
    std::sort(riskTargets.begin(), riskTargets.end(),
              [](const RiskTarget &a, const RiskTarget &b) {
                  if (a.risk != b.risk) {
                      return a.risk == RiskLevel::High;  // HIGH before MEDIUM
                  }
                  return a.distanceFromHome < b.distanceFromHome;  // Closer first
              });

    // === STEP 2: Find the best path to extend roads toward the riskiest territory ===
    // For each at-risk territory, find the shortest path from the nearest city
    // The first territory on that path (adjacent to a city) is where we should build

    QList<QString> pathToRiskiest;
    QString riskiestTarget;

    for (const RiskTarget &target : riskTargets) {
        if (connectedToHome.contains(target.territory)) {
            continue;  // Already connected - no need to build road cities
        }

        // Find shortest path from any existing city to this target
        // We want to extend from the road network toward the target
        int bestPathLength = INT_MAX;
        QList<QString> bestPath;
        QString bestStartCity;

        for (const QString &cityTerritory : territoriesWithCities) {
            if (!ownedSet.contains(cityTerritory)) continue;

            QList<QString> path = graph->findPath(cityTerritory, target.territory);
            if (!path.isEmpty() && path.size() < bestPathLength) {
                // Verify the path only goes through territories we own
                bool validPath = true;
                for (const QString &step : path) {
                    if (!ownedSet.contains(step)) {
                        validPath = false;
                        break;
                    }
                }
                if (validPath) {
                    bestPathLength = path.size();
                    bestPath = path;
                    bestStartCity = cityTerritory;
                }
            }
        }

        if (!bestPath.isEmpty() && bestPath.size() > 1) {
            pathToRiskiest = bestPath;
            riskiestTarget = target.territory;
            break;  // Use the first (highest priority) target with a valid path
        }
    }

    // === PHASE 1: Build fortified cities for defense (can be standalone) ===
    // Fortified cities provide +1 defense, useful at threatened locations with troops
    int fortifiedCitiesBuilt = 0;

    for (const RiskTarget &target : riskTargets) {
        if (fortifiedCitiesBuilt >= 1) break;
        if (remaining < fortifiedCityPrice) break;
        if (!territoriesForCities.contains(target.territory)) continue;  // Can't build here
        if (decision.cities.contains(target.territory)) continue;  // Already planned

        // Only build fortified city if:
        // 1. Territory is at risk (already filtered above)
        // 2. We have troops there that need protection
        // 3. Fortification would make a difference (close battle)
        bool hasTroops = (target.ourForce > 0);
        bool fortHelps = (target.ourForce >= target.enemyForce - 2 &&
                          target.ourForce < target.enemyForce);

        if (hasTroops && fortHelps) {
            decision.cities[target.territory] = true;  // Fortified
            remaining -= fortifiedCityPrice;
            decision.totalCost += fortifiedCityPrice;
            fortifiedCitiesBuilt++;

            // Track that this city exists now
            territoriesWithCities.insert(target.territory);

            decision.reason += QString(" | Fortified city at %1 (defense: %2 vs enemy %3)")
                .arg(target.territory).arg(target.ourForce).arg(target.enemyForce);

            // If this is the riskiest target and we just built there, no need for road
            if (target.territory == riskiestTarget) {
                pathToRiskiest.clear();  // No longer need to build road to it
            }
        }
    }

    // === PHASE 2: Build unfortified cities along the path to riskiest territory ===
    // Start from the city end of the path and work toward the target
    // Each city must be adjacent to an existing city (the previous one in the path)

    int unfortifiedCitiesBuilt = 0;
    int maxUnfortifiedPerTurn = 2;  // Can build up to 2 road cities per turn

    if (!pathToRiskiest.isEmpty()) {
        // pathToRiskiest is: [startCity, step1, step2, ..., targetTerritory]
        // We want to build at step1 first (adjacent to startCity), then step2, etc.

        for (int i = 1; i < pathToRiskiest.size() && unfortifiedCitiesBuilt < maxUnfortifiedPerTurn; i++) {
            QString stepTerritory = pathToRiskiest[i];

            if (remaining < cityPrice) break;
            if (decision.cities.contains(stepTerritory)) continue;  // Already planned
            if (territoriesWithCities.contains(stepTerritory)) continue;  // Already have city
            if (!territoriesForCities.contains(stepTerritory)) continue;  // Can't build here

            // Verify this territory is adjacent to an existing city
            bool adjacentToCity = false;
            QStringList neighbors = graph->getNeighbors(stepTerritory);
            for (const QString &neighbor : neighbors) {
                if (territoriesWithCities.contains(neighbor)) {
                    adjacentToCity = true;
                    break;
                }
            }

            if (!adjacentToCity) {
                continue;  // Skip - unfortified cities must connect to road network
            }

            // Check risk level - prefer safe territories but build anyway if on path
            bool isSafeEnough = true;
            if (riskMap.contains(stepTerritory)) {
                RiskLevel risk = riskMap[stepTerritory].risk;
                isSafeEnough = (risk == RiskLevel::Safe || risk == RiskLevel::Low ||
                                risk == RiskLevel::Unreachable);
            }

            // Build the road city
            decision.cities[stepTerritory] = false;  // Unfortified
            remaining -= cityPrice;
            decision.totalCost += cityPrice;
            unfortifiedCitiesBuilt++;

            // Track this city so the next step can be adjacent to it
            territoriesWithCities.insert(stepTerritory);

            decision.reason += QString(" | Road city at %1 (path to %2)")
                .arg(stepTerritory).arg(riskiestTarget);
        }
    }

    // === PHASE 2b: Connect isolated fortified cities back to home road network ===
    // If we have fortified cities that aren't connected to the home city network,
    // build unfortified cities to connect them
    QString homeProvinceName = player->getHomeProvinceName();

    // Find all fortified cities that are NOT connected to home
    QList<QString> isolatedFortifiedCities;
    for (City *city : player->getCities()) {
        if (!city->isFortified()) continue;
        QString cityTerritory = city->getTerritoryName();
        if (cityTerritory == homeProvinceName) continue;  // Home is always connected

        // Check if this fortified city is connected to home via road network
        // BFS from this city - can we reach home through cities only?
        QSet<QString> visited;
        QList<QString> toVisit;
        toVisit.append(cityTerritory);
        visited.insert(cityTerritory);
        bool connectedToHome = false;

        while (!toVisit.isEmpty() && !connectedToHome) {
            QString current = toVisit.takeFirst();
            QStringList neighbors = graph->getNeighbors(current);
            for (const QString &neighbor : neighbors) {
                if (graph->isSeaTerritory(neighbor)) continue;
                if (visited.contains(neighbor)) continue;
                if (!territoriesWithCities.contains(neighbor)) continue;  // Must travel through cities

                if (neighbor == homeProvinceName) {
                    connectedToHome = true;
                    break;
                }
                visited.insert(neighbor);
                toVisit.append(neighbor);
            }
        }

        if (!connectedToHome) {
            isolatedFortifiedCities.append(cityTerritory);
        }
    }

    // For each isolated fortified city, find path to nearest connected city and build road
    for (const QString &isolatedCity : isolatedFortifiedCities) {
        if (unfortifiedCitiesBuilt >= maxUnfortifiedPerTurn) break;
        if (remaining < cityPrice) break;

        // BFS from isolated city to find shortest path to any connected city (home network)
        QMap<QString, QString> cameFrom;
        QList<QString> toVisit;
        QSet<QString> visited;
        toVisit.append(isolatedCity);
        visited.insert(isolatedCity);
        cameFrom[isolatedCity] = "";

        QString targetCity;  // First city we find that's connected to home
        bool found = false;

        while (!toVisit.isEmpty() && !found) {
            QString current = toVisit.takeFirst();
            QStringList neighbors = graph->getNeighbors(current);
            for (const QString &neighbor : neighbors) {
                if (graph->isSeaTerritory(neighbor)) continue;
                if (visited.contains(neighbor)) continue;
                if (!player->ownsTerritory(neighbor)) continue;  // Must own territory

                visited.insert(neighbor);
                cameFrom[neighbor] = current;
                toVisit.append(neighbor);

                // Check if this neighbor is connected to home (has city that's part of home network)
                if (territoriesWithCities.contains(neighbor) && neighbor != isolatedCity) {
                    // Verify it's actually connected to home
                    QSet<QString> homeCheck;
                    QList<QString> homeVisit;
                    homeVisit.append(neighbor);
                    homeCheck.insert(neighbor);
                    bool reachesHome = (neighbor == homeProvinceName);

                    while (!homeVisit.isEmpty() && !reachesHome) {
                        QString hc = homeVisit.takeFirst();
                        for (const QString &hn : graph->getNeighbors(hc)) {
                            if (graph->isSeaTerritory(hn)) continue;
                            if (homeCheck.contains(hn)) continue;
                            if (!territoriesWithCities.contains(hn)) continue;
                            if (hn == homeProvinceName) {
                                reachesHome = true;
                                break;
                            }
                            homeCheck.insert(hn);
                            homeVisit.append(hn);
                        }
                    }

                    if (reachesHome) {
                        targetCity = neighbor;
                        found = true;
                        break;
                    }
                }
            }
        }

        if (found && !targetCity.isEmpty()) {
            // Trace back path from targetCity to isolatedCity
            QList<QString> path;
            QString step = targetCity;
            while (!step.isEmpty() && step != isolatedCity) {
                path.prepend(step);
                step = cameFrom.value(step, "");
            }
            path.prepend(isolatedCity);

            // Build cities along path, starting from the connected end (targetCity side)
            // path is [isolatedCity, ..., territory_before_targetCity, targetCity]
            // We want to build from targetCity backwards toward isolatedCity
            for (int i = path.size() - 2; i >= 1; i--) {  // Skip targetCity (already has city) and isolatedCity
                if (unfortifiedCitiesBuilt >= maxUnfortifiedPerTurn) break;
                if (remaining < cityPrice) break;

                QString stepTerritory = path[i];
                if (decision.cities.contains(stepTerritory)) continue;
                if (territoriesWithCities.contains(stepTerritory)) continue;
                if (!territoriesForCities.contains(stepTerritory)) continue;

                // Verify adjacent to existing city
                bool adjacentToCity = false;
                for (const QString &neighbor : graph->getNeighbors(stepTerritory)) {
                    if (territoriesWithCities.contains(neighbor)) {
                        adjacentToCity = true;
                        break;
                    }
                }
                if (!adjacentToCity) continue;

                decision.cities[stepTerritory] = false;  // Unfortified
                remaining -= cityPrice;
                decision.totalCost += cityPrice;
                unfortifiedCitiesBuilt++;
                territoriesWithCities.insert(stepTerritory);

                decision.reason += QString(" | Road city at %1 (connecting %2 to home)")
                    .arg(stepTerritory).arg(isolatedCity);
            }
        }
    }

    // === NO PHASE 3: Don't build cities "just because" ===
    // Cities should ONLY be built to:
    // 1. Extend roads toward at-risk territories (Phase 2)
    // 2. Connect isolated fortified cities to home (Phase 2b)
    // 3. Provide fortified defense at threatened locations (Phase 1)
    //
    // If there are no at-risk territories, there's no reason to buy a city.
    // The money is better spent on troops.

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
    // RULE: Income cities are unfortified, so they MUST be adjacent to existing city
    // RULE: Prefer territories where we have a general stationed for protection
    int minBudgetForIncomeCity = 40;  // Must have at least 40 remaining after other purchases
    if (currentTroops >= 8 && remaining >= minBudgetForIncomeCity && decision.cities.isEmpty()) {
        // Find high-value safe territories without cities
        // Only consider 10-value territories - they pay off faster
        // Score: value + 100 bonus if general present (strongly prefer protected territories)
        QList<QPair<QString, int>> incomeCityCandidates;
        for (const QString &territory : territoriesForCities) {
            if (decision.cities.contains(territory)) continue;  // Already planning to build

            // RULE: Unfortified cities must be adjacent to an existing city
            bool adjacentToCity = false;
            QStringList neighbors = graph->getNeighbors(territory);
            for (const QString &neighbor : neighbors) {
                if (territoriesWithCities.contains(neighbor)) {
                    adjacentToCity = true;
                    break;
                }
            }
            if (!adjacentToCity) continue;  // Skip - must connect to road network

            int value = graph->getValue(territory);
            if (value < 10) continue;  // Only build on high-value territories for income

            if (riskMap.contains(territory)) {
                RiskLevel risk = riskMap[territory].risk;
                // Only SAFE territories - not even low risk
                if (risk == RiskLevel::Safe) {
                    // Check if we have a general in this territory for protection
                    bool hasGeneralHere = false;
                    for (GeneralPiece *gen : player->getGenerals()) {
                        if (gen->getTerritoryName() == territory) {
                            hasGeneralHere = true;
                            break;
                        }
                    }
                    // Score: base value + 100 bonus if general present
                    int score = value + (hasGeneralHere ? 100 : 0);
                    incomeCityCandidates.append({territory, score});
                }
            }
        }

        // Sort by score descending (prefer territories with generals, then by value)
        std::sort(incomeCityCandidates.begin(), incomeCityCandidates.end(),
                  [](const auto &a, const auto &b) { return a.second > b.second; });

        // Build at most 1 income city per turn
        if (!incomeCityCandidates.isEmpty() && remaining >= cityPrice) {
            const auto &incomeCandidate = incomeCityCandidates.first();
            decision.cities[incomeCandidate.first] = false;  // Unfortified
            remaining -= cityPrice;
            decision.totalCost += cityPrice;
            territoriesWithCities.insert(incomeCandidate.first);  // Track for future adjacency checks
            decision.reason += QString(" | Income city at %1 (val=%2)").arg(incomeCandidate.first).arg(incomeCandidate.second);
        }
    }

    // === PRIORITY 6: Galleys for sea expansion ===
    // Galleys are essential for reaching territories across water
    // Buy galleys if:
    // 1. We have fewer than 2 galleys
    // 2. There are sea territories we can place galleys on
    // 3. We have at least some basic troops (don't buy galley before army)
    if (currentGalleyCount < 2 && !seaTerritoriesForGalleys.isEmpty() && remaining >= galleyPrice) {
        // Check if there are valuable territories reachable only by sea
        bool needsSeaExpansion = false;

        // Simple heuristic: if we control coastal territories but haven't expanded across seas,
        // we probably need galleys. Check if there are unclaimed or enemy territories
        // reachable from our sea borders.
        for (const QString &seaOption : seaTerritoriesForGalleys) {
            // Extract sea territory name (remove " (direction)" suffix)
            QString seaTerritory = seaOption.contains(" (")
                ? seaOption.left(seaOption.indexOf(" ("))
                : seaOption;

            // Get territories connected to this sea
            QStringList seaNeighbors = graph->getNeighbors(seaTerritory);
            for (const QString &neighbor : seaNeighbors) {
                // Check if this is a land territory we don't own
                if (!graph->isSeaTerritory(neighbor) && !ownedTerritories.contains(neighbor)) {
                    needsSeaExpansion = true;
                    break;
                }
            }
            if (needsSeaExpansion) break;
        }

        // Buy galleys if sea expansion is needed
        // With currentGalleyCount < 2, we'll buy up to 2 total
        if (needsSeaExpansion && currentTroops >= 4) {  // Have at least some troops first
            int galleysWanted = qMin(2 - currentGalleyCount,
                                     static_cast<int>(seaTerritoriesForGalleys.size()));

            for (int i = 0; i < galleysWanted && remaining >= galleyPrice; i++) {
                // Pick a sea territory - use first available
                QString seaOption = seaTerritoriesForGalleys[i % seaTerritoriesForGalleys.size()];
                QString seaTerritory = seaOption.contains(" (")
                    ? seaOption.left(seaOption.indexOf(" ("))
                    : seaOption;

                decision.galleys[seaTerritory] = decision.galleys.value(seaTerritory, 0) + 1;
                remaining -= galleyPrice;
                decision.totalCost += galleyPrice;
                decision.reason += QString(" | Galley at %1 (sea expansion)").arg(seaTerritory);
            }
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

    // Get our force projection map to compare against threats
    QMap<QString, int> ourForce1Turn = calc.getForceProjectionMap(player, graph, 1);


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

        // Get risk level from riskMap for compatibility
        RiskLevel risk = RiskLevel::Safe;
        int enemyMaxForce = 0;
        if (riskMap.contains(territory)) {
            risk = riskMap[territory].risk;
            enemyMaxForce = riskMap[territory].enemyMaxForce;
        }

        if (weOwnIt) {
            // === DEFEND: Our territory under threat ===
            // ONLY defend if there's a 1-turn threat (immediate danger)
            // AND we don't have overwhelming force advantage
            // 2-turn threats are used as tiebreaker only, not primary reason to defend
            int ourForce = ourForce1Turn.value(territory, 0);

            // Calculate if we need defense:
            // - Must have immediate threat (threat1Turn > 0)
            // - Must NOT have overwhelming advantage (ourForce < threat * 3)
            // If we have 3x+ their force, we're fine - focus on expansion instead
            bool hasImmediateThreat = (threat1Turn > 0);
            bool hasOverwhelmingAdvantage = (ourForce >= threat1Turn * 3 && ourForce >= 3);
            bool needsDefense = hasImmediateThreat && !hasOverwhelmingAdvantage;

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
            // Owned territory either safe (no threat) or has overwhelming advantage - not a defense target
        } else {
            // === EXPAND or ATTACK: Not our territory ===
            if (target.enemyTroops == 0 && !target.requiresTroops) {
                // EXPAND: Undefended territory - can capture with lone general
                // Expansion is HIGH priority - every unclaimed territory is free income!
                target.type = "Expand";
                target.score = 200 + (value * 15);  // Base 200 + 15 per value point
                target.troopsNeeded = 0;

                // === KEY INSIGHT: Use heat map for safe expansion ===
                // If enemy 1-turn threat is 0, this is SAFE to take with a lone general
                if (isSafeExpansion) {
                    target.score += 150;  // BIG bonus for truly safe expansion
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
                }

                // Bonus for enemy cities (even undefended)
                if (hasEnemyCity) {
                    target.score += 80;
                }

                targets.append(target);
            } else {
                // ATTACK: Has defenders - requires troops WITH ADVANTAGE
                // Don't attack with equal force - that's too risky!
                // Require at least +2 troops OR 50% more force (whichever is greater)
                target.type = "Attack";
                target.score = 150 + (value * 15);  // Base 150 + 15 per value point

                int minAdvantage = qMax(2, (target.enemyTroops + 1) / 2);  // At least +2 or +50%
                target.troopsNeeded = target.enemyTroops + minAdvantage;
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

    // Identify "threatened" territories using the RISK MAP data directly
    // A territory is threatened if enemies can reach it (even with 0 force - they have generals nearby)
    // This uses riskMap.enemyLeaders which tracks all enemy generals that can reach each territory
    QSet<QString> threatenedTerritories;
    for (auto it = riskMap.begin(); it != riskMap.end(); ++it) {
        if (player->ownsTerritory(it.key())) {
            const TerritoryRisk &risk = it.value();
            // Territory is threatened if enemy can reach it (force > 0 OR enemy leaders can reach)
            if (risk.enemyMaxForce > 0 || !risk.enemyLeaders.isEmpty()) {
                threatenedTerritories.insert(it.key());
            }
        }
    }

    if (!threatenedTerritories.isEmpty()) {
        qDebug() << "Threatened territories (enemy can reach):" << threatenedTerritories;
    }

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
            } else if (risk.risk == RiskLevel::Low && threatenedTerritories.contains(it.key())) {
                // LOW risk but enemies CAN reach - should still maintain presence
                // This prevents troops from wandering to the back while frontline is exposed
                // The key insight: LOW risk means enemy force is 0, but they have generals nearby!
                score = 100;  // Significant score - this is the frontline
            }

            // THREATENED BONUS: Territories that enemies can reach get extra priority
            // This ensures troops are positioned toward threats, not scattered to the interior
            if (threatenedTerritories.contains(it.key())) {
                // Bonus based on number of enemy leaders that can reach (more generals = more threat)
                int enemyLeaderCount = risk.enemyLeaders.size();
                score += 30 + enemyLeaderCount * 20;  // 50 for 1 general, 70 for 2, etc.
                qDebug() << "  Threatened territory" << it.key() << "bonus:" << (30 + enemyLeaderCount * 20)
                         << "(enemy generals:" << enemyLeaderCount << ")";
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

    // STRATEGIC EXPANSION: Boost scores for expansion targets adjacent to threatened territories
    // This makes expansion go toward enemies rather than into empty space.
    for (const TargetTerritory &target : targets) {
        if (target.type != "Expand" && target.type != "Attack") continue;
        if (!effectiveRiskScore.contains(target.name)) continue;

        // Check if this expansion target is adjacent to a threatened territory
        bool adjacentToThreat = false;
        QStringList neighbors = graph->getNeighbors(target.name);
        for (const QString &neighbor : neighbors) {
            if (threatenedTerritories.contains(neighbor)) {
                adjacentToThreat = true;
                break;
            }
        }

        if (adjacentToThreat) {
            // Boost expansion targets that extend toward enemies
            effectiveRiskScore[target.name] += 100;
            qDebug() << "Boosting strategic expansion target:" << target.name << "new score:" << effectiveRiskScore[target.name];
        }
    }

    // PROPORTIONAL TROOP DISTRIBUTION: Calculate how many troops each territory should receive
    // based on its proportion of total risk. This ensures troops are distributed fairly.
    // IMPORTANT: Only consider NEARBY targets for proportional distribution to avoid scattering.
    QMap<QString, int> targetTroopAllocation;  // territory -> target number of troops
    int totalAvailableTroops = availableTroops.size();

    // Find centroid of our troops (most common location) for distance calculations
    QMap<QString, int> troopLocations;
    for (const TroopInfo &t : availableTroops) {
        troopLocations[t.territory]++;
    }
    QString troopCentroid;
    int maxTroops = 0;
    for (auto it = troopLocations.begin(); it != troopLocations.end(); ++it) {
        if (it.value() > maxTroops) {
            maxTroops = it.value();
            troopCentroid = it.key();
        }
    }

    // Calculate distance-weighted scores - nearby high-priority targets get more allocation
    // This prevents scattering troops to distant targets like Britannia when closer ones exist
    QMap<QString, int> distanceWeightedScore;
    int totalWeightedScore = 0;

    for (auto it = effectiveRiskScore.begin(); it != effectiveRiskScore.end(); ++it) {
        int baseScore = it.value();
        if (baseScore <= 0) continue;

        // Calculate distance from troop centroid using BFS
        int distance = 1;
        if (!troopCentroid.isEmpty() && it.key() != troopCentroid) {
            QMap<QString, int> dist;
            QList<QString> queue;
            queue.append(troopCentroid);
            dist[troopCentroid] = 0;
            bool found = false;

            while (!queue.isEmpty() && !found) {
                QString current = queue.takeFirst();
                if (current == it.key()) {
                    distance = dist[current];
                    found = true;
                    break;
                }
                for (const QString &neighbor : graph->getNeighbors(current)) {
                    if (graph->isSeaTerritory(neighbor)) continue;
                    if (dist.contains(neighbor)) continue;
                    dist[neighbor] = dist[current] + 1;
                    if (dist[neighbor] <= 5) {
                        queue.append(neighbor);
                    }
                }
            }
            if (!found) distance = 6;  // Far away
        }

        // Apply STRONG distance penalty to focus on nearby targets
        // Distance 1: full score, Distance 2: 50%, Distance 3: 25%, etc.
        int weightedScore = baseScore;
        if (distance > 1) {
            weightedScore = baseScore / (distance * distance);  // Quadratic falloff
        }

        // Threatened territories get distance bonus (defense is urgent regardless of distance)
        if (threatenedTerritories.contains(it.key())) {
            weightedScore = qMax(weightedScore, baseScore / 2);  // At least half score for defense
        }

        if (weightedScore > 0) {
            distanceWeightedScore[it.key()] = weightedScore;
            totalWeightedScore += weightedScore;
        }
    }

    if (totalWeightedScore > 0 && totalAvailableTroops > 0) {
        qDebug() << "=== PROPORTIONAL TROOP ALLOCATION ===";
        qDebug() << "Total weighted score:" << totalWeightedScore << ", Available troops:" << totalAvailableTroops;
        qDebug() << "Troop centroid:" << troopCentroid;

        // Calculate proportional allocation for each territory
        int allocatedSoFar = 0;
        QList<QPair<QString, int>> sortedByScore;
        for (auto it = distanceWeightedScore.begin(); it != distanceWeightedScore.end(); ++it) {
            sortedByScore.append({it.key(), it.value()});
        }
        // Sort by weighted score descending
        std::sort(sortedByScore.begin(), sortedByScore.end(),
                  [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
                      return a.second > b.second;
                  });

        // Only allocate to top targets (limit scattering)
        int maxTargets = qMin(totalAvailableTroops, 8);  // No more targets than troops, max 8
        int targetsAllocated = 0;

        for (const auto &pair : sortedByScore) {
            if (targetsAllocated >= maxTargets) break;

            const QString &territory = pair.first;
            int weightedScore = pair.second;

            // Proportional allocation: (score / totalScore) * totalTroops
            double proportion = static_cast<double>(weightedScore) / totalWeightedScore;
            int targetCount = static_cast<int>(proportion * totalAvailableTroops + 0.5);  // Round

            // Minimum 1 troop for significant targets (weighted score >= 50)
            if (targetCount == 0 && weightedScore >= 50) {
                targetCount = 1;
            }

            // Cap at remaining troops
            targetCount = qMin(targetCount, totalAvailableTroops - allocatedSoFar);

            if (targetCount > 0) {
                targetTroopAllocation[territory] = targetCount;
                allocatedSoFar += targetCount;
                targetsAllocated++;
                qDebug() << QString("  %1: weighted=%2, proportion=%3%, target=%4 troops")
                    .arg(territory)
                    .arg(weightedScore)
                    .arg(static_cast<int>(proportion * 100))
                    .arg(targetCount);
            }
        }
        qDebug() << "Total allocated:" << allocatedSoFar << "of" << totalAvailableTroops << "to" << targetsAllocated << "targets";
    }

    // Track how many troops have been assigned to each ultimate target
    QMap<QString, int> troopsAssignedToTarget;  // ultimate target -> count assigned so far

    // Helper: Check if a territory has available galley (either beached here or in adjacent sea)
    auto hasGalleyAtTerritory = [&graph, &player](const QString &territory) -> bool {
        for (GalleyPiece *galley : player->getGalleys()) {
            if (galley->hasTransportedThisTurn() ||
                galley->hasLeaderAboard() ||
                galley->getMovesRemaining() < 0.5) {
                continue;  // Galley not available
            }

            // Check for BEACHED galley at this exact territory
            if (galley->isBeached() && galley->getTerritoryName() == territory) {
                return true;
            }

            // Check for galley at sea in an adjacent sea zone
            QString galleyLocation = galley->getTerritoryName();
            if (!galley->isBeached() && graph->isSeaTerritory(galleyLocation)) {
                // Check if this sea zone is adjacent to our territory
                QStringList neighbors = graph->getNeighbors(territory);
                if (neighbors.contains(galleyLocation)) {
                    return true;
                }
            }
        }
        return false;
    };

    // Helper: Check if a route requires galley transport (no land path)
    auto requiresGalley = [&graph](const QString &from, const QString &target) -> bool {
        if (from == target) return false;

        // BFS to check for land path
        QSet<QString> visited;
        QList<QString> queue;
        queue.append(from);
        visited.insert(from);

        while (!queue.isEmpty()) {
            QString current = queue.takeFirst();
            if (current == target) return false;  // Land path exists

            for (const QString &neighbor : graph->getNeighbors(current)) {
                if (graph->isSeaTerritory(neighbor)) continue;
                if (visited.contains(neighbor)) continue;
                visited.insert(neighbor);
                queue.append(neighbor);
            }
        }
        return true;  // No land path - requires galley
    };

    // Helper: BFS to find the first step toward a distant territory
    // Considers roads: if target is on our road network, go directly there
    // Otherwise find the best road exit point toward target
    auto findFirstStepToward = [&graph, &hasGalleyAtTerritory, &player](const QString &from, const QString &target) -> QString {
        if (from == target) return QString();

        // === ROAD NETWORK CHECK ===
        // If we're at a city on our road network, check if target is also on it
        QStringList roadConnected = graph->getRoadConnectedTerritories(from, player);
        if (!roadConnected.isEmpty()) {
            // Target is directly on our road network - go straight there!
            if (roadConnected.contains(target)) {
                return target;
            }

            // Target is NOT on road network - find the best road exit point
            // This is the road-connected territory closest to the target
            QString bestRoadExit;
            int bestDistanceFromExit = 999;

            for (const QString &roadTerritory : roadConnected) {
                // Calculate distance from this road territory to the target
                int distToTarget = graph->getDistance(roadTerritory, target);
                if (distToTarget > 0 && distToTarget < bestDistanceFromExit) {
                    bestDistanceFromExit = distToTarget;
                    bestRoadExit = roadTerritory;
                }
            }

            // Also check if going directly (without using road) would be shorter
            // Sometimes the target is adjacent to 'from' but not on roads
            int directDist = graph->getDistance(from, target);
            if (directDist > 0 && directDist <= bestDistanceFromExit) {
                // Direct route is same or shorter - use standard BFS below
            } else if (!bestRoadExit.isEmpty()) {
                // Road route is shorter - take road to exit point
                return bestRoadExit;
            }
        }

        // === STANDARD BFS (no road advantage, or direct route is shorter) ===
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

        // No land path found - target might be reachable via galley
        // But ONLY if galley is actually available at the source territory!
        if (hasGalleyAtTerritory(from)) {
            return target;
        }

        // No galley available - can't reach this target
        return QString();
    };

    // Helper: Calculate distance (in hops) between two territories
    // Considers: roads (1 move for entire network), land adjacency, galley routes
    auto getDistance = [&graph, &hasGalleyAtTerritory, &player](const QString &from, const QString &target) -> int {
        if (from == target) return 0;

        // === ROAD NETWORK CHECK ===
        // If we're at a city on our road network, we can reach any other city on the network in 1 move
        QStringList roadConnected = graph->getRoadConnectedTerritories(from, player);
        if (!roadConnected.isEmpty()) {
            // Target is directly on our road network - 1 move!
            if (roadConnected.contains(target)) {
                return 1;
            }

            // Target is NOT on road network - find closest road exit point
            // BFS from all road-connected territories to find shortest path to target
            QMap<QString, int> dist;
            QList<QString> queue;

            // Start BFS from all road-connected territories (including 'from')
            // All of these are distance 1 from 'from' via roads
            dist[from] = 0;  // Starting point
            for (const QString &roadTerritory : roadConnected) {
                dist[roadTerritory] = 1;  // 1 move to reach via road
                queue.append(roadTerritory);
            }

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

            // If we reached here, target might be galley-reachable (checked below)
        } else {
            // === NO ROAD NETWORK - Standard BFS ===
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
        }

        // No land path found - check if galley-reachable
        // Must verify that galley can actually reach a sea zone adjacent to target
        if (hasGalleyAtTerritory(from)) {
            // Find sea zones adjacent to source (where galley can launch)
            QSet<QString> sourceSeaZones;
            for (const QString &neighbor : graph->getNeighbors(from)) {
                if (graph->isSeaTerritory(neighbor)) {
                    sourceSeaZones.insert(neighbor);
                }
            }

            // Find sea zones adjacent to target (where galley can disembark)
            QSet<QString> targetSeaZones;
            for (const QString &neighbor : graph->getNeighbors(target)) {
                if (graph->isSeaTerritory(neighbor)) {
                    targetSeaZones.insert(neighbor);
                }
            }

            // Check if any source sea zone can reach any target sea zone
            // Use BFS through connected sea zones (galley has ~4 moves)
            if (!sourceSeaZones.isEmpty() && !targetSeaZones.isEmpty()) {
                // BFS from source sea zones to find reachable sea zones
                QSet<QString> reachableSeaZones;
                QList<QString> seaQueue;
                QMap<QString, int> seaDist;

                for (const QString &sourceSea : sourceSeaZones) {
                    seaQueue.append(sourceSea);
                    seaDist[sourceSea] = 1;  // Launch costs 1 move
                    reachableSeaZones.insert(sourceSea);
                }

                while (!seaQueue.isEmpty()) {
                    QString currentSea = seaQueue.takeFirst();
                    int currentDist = seaDist[currentSea];

                    // Galley has 4 moves: 1 for launch + up to 3 more for sailing
                    if (currentDist >= 4) continue;

                    for (const QString &neighbor : graph->getNeighbors(currentSea)) {
                        if (graph->isSeaTerritory(neighbor) && !seaDist.contains(neighbor)) {
                            seaDist[neighbor] = currentDist + 1;
                            seaQueue.append(neighbor);
                            reachableSeaZones.insert(neighbor);
                        }
                    }
                }

                // Check if any target sea zone is reachable
                for (const QString &targetSea : targetSeaZones) {
                    if (reachableSeaZones.contains(targetSea)) {
                        // Distance = sea hops + 1 for disembark
                        int seaHops = seaDist.value(targetSea, 999);
                        return seaHops + 1;  // Approximate galley distance
                    }
                }
            }
        }

        return 999;  // Truly unreachable
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
    // Keep ~2/3 of what we'd need to fully match the threat (be aggressive, not overly defensive)
    int troopsToKeepHome = 0;
    int enemyThreatToHome = 0;
    if (!caesarTerritory.isEmpty() && riskMap.contains(caesarTerritory)) {
        const TerritoryRisk &homeRisk = riskMap[caesarTerritory];
        enemyThreatToHome = homeRisk.enemyMaxForce;

        // Base defense = 2/3 of enemy threat (rounded up), then adjust by risk level
        int baseDefense = (enemyThreatToHome * 2 + 2) / 3;  // ~67% of threat, round up

        if (homeRisk.risk == RiskLevel::High) {
            troopsToKeepHome = baseDefense + 1;  // Small buffer for high risk
        } else if (homeRisk.risk == RiskLevel::Medium) {
            troopsToKeepHome = baseDefense;
        } else if (homeRisk.risk == RiskLevel::Low) {
            troopsToKeepHome = qMax(0, baseDefense - 1);  // Can afford to be more aggressive
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
            // troopsLeavingFrom[dest] = source means "troops going TO dest came FROM source"
            //
            // If we're at territory A wanting to go to B:
            // - Check if troops from B are going to A: troopsLeavingFrom[A] == B
            // - Check if troops from A are going to B: troopsLeavingFrom[B] == A (already committed)
            //
            // Block if EITHER direction already exists - first mover wins
            if (troopsLeavingFrom.contains(firstStep) && troopsLeavingFrom[firstStep] == troop.territory) {
                // Troops going to firstStep came from our territory - we're already sending TO firstStep
                continue;
            }
            if (troopsLeavingFrom.contains(troop.territory) && troopsLeavingFrom[troop.territory] == firstStep) {
                // Troops going to our territory came from firstStep - they're coming TO us
                continue;
            }

            // Discount score by distance (closer targets are more valuable)
            // But still consider distant targets - just at reduced priority
            int effectiveScore = baseScore / distance;

            // Check if this destination requires galley transport
            bool isGalleyRoute = requiresGalley(troop.territory, ultimateTarget);

            // PROPORTIONAL ALLOCATION: Check if this target has received its fair share
            // Skip or heavily penalize targets that already have enough troops assigned
            int targetAllocation = targetTroopAllocation.value(ultimateTarget, 0);
            int alreadyAssigned = troopsAssignedToTarget.value(ultimateTarget, 0);

            if (targetAllocation > 0 && alreadyAssigned >= targetAllocation) {
                // This target has received its proportional share
                // Apply heavy penalty but don't skip entirely (allow overflow if no other options)
                effectiveScore = effectiveScore / 4;
            } else if (targetAllocation > 0 && alreadyAssigned >= targetAllocation - 1) {
                // Almost at target - small penalty to let other territories catch up
                effectiveScore = static_cast<int>(effectiveScore * 0.7);
            }

            // CONSOLIDATION LOGIC: VERY STRONGLY encourage troops to travel together
            // A general can take 5 troops, so we want FULL groups not scattered 1-troop movements
            // The key insight: it's MUCH better to send 5 troops to 1 place than 1 troop to 5 places
            int alreadyGoing = troopsGoingTo.value(firstStep, 0);

            // Count how many different destinations troops from THIS territory are already going to
            int destinationsFromHere = 0;
            for (auto it = troopsGoingTo.begin(); it != troopsGoingTo.end(); ++it) {
                // Check if this destination has troops coming from our territory
                if (troopsLeavingFrom.value(it.key()) == troop.territory && it.value() > 0) {
                    destinationsFromHere++;
                }
            }

            if (alreadyGoing > 0 && alreadyGoing < 5) {
                // MASSIVE BOOST for joining existing group - troops MUST consolidate!
                // This bonus should be so high that troops always join existing groups
                double consolidationBonus = 3.0 + (alreadyGoing * 0.5);  // 3.5x for 1, 4.0x for 2, 4.5x for 3, 5.0x for 4
                if (isGalleyRoute) {
                    consolidationBonus += 1.0;  // Even bigger bonus for galley (maximize payload)
                }
                effectiveScore = static_cast<int>(effectiveScore * consolidationBonus);
            } else if (alreadyGoing >= 5) {
                // Group is full (5 troops max per general) - heavy penalty, find another group
                effectiveScore = static_cast<int>(effectiveScore / 3.0);
            } else {
                // First troop to this destination - check if we're scattering too much
                // Penalize opening new destinations if we already have groups forming
                if (destinationsFromHere >= 1) {
                    // Already have troops going somewhere from here - BIG penalty for scattering
                    effectiveScore = static_cast<int>(effectiveScore * 0.3);
                } else if (isGalleyRoute) {
                    // First troop to a galley route - small bonus
                    effectiveScore = static_cast<int>(effectiveScore * 1.1);
                }
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

            // Track troops assigned to each ultimate target for proportional distribution
            troopsAssignedToTarget[bestUltimateTarget]++;

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

    // Convert to list and sort by EFFECTIVENESS (larger groups are more valuable per general)
    // A general taking 5 troops is 5x more efficient than a general taking 1 troop
    // So we prioritize: (1) groups with more troops, (2) higher total score as tiebreaker
    QList<TroopGroup> groups = groupMap.values();
    std::sort(groups.begin(), groups.end(),
              [](const TroopGroup &a, const TroopGroup &b) {
                  // Primary: group size (more troops = higher priority)
                  // We want full legions (5 troops) assigned first
                  int sizeA = qMin(5, a.troopIndices.size());  // Cap at 5 (one general's worth)
                  int sizeB = qMin(5, b.troopIndices.size());
                  if (sizeA != sizeB) {
                      return sizeA > sizeB;  // Larger groups first
                  }
                  // Secondary: total score as tiebreaker
                  return a.totalScore > b.totalScore;
              });

    qDebug() << "=== TROOP GROUPS ===";
    for (const TroopGroup &g : groups) {
        qDebug() << QString("  %1 -> %2: %3 troops (score=%4)")
            .arg(g.from).arg(g.to).arg(g.troopIndices.size()).arg(g.totalScore);
    }

    // Step 6: Assign generals to DIFFERENT destinations based on risk
    // Instead of assigning to troop groups, we assign each general to the best unassigned destination
    QSet<GeneralPiece*> assignedGenerals;
    QSet<int> assignedTroopIds;
    QSet<QString> assignedDestinations;  // Track where generals are already going

    // Sort risk scores to get best destinations
    QList<QPair<QString, int>> sortedDestinations;
    for (auto it = effectiveRiskScore.begin(); it != effectiveRiskScore.end(); ++it) {
        if (it.value() > 0) {
            sortedDestinations.append({it.key(), it.value()});
        }
    }
    std::sort(sortedDestinations.begin(), sortedDestinations.end(),
              [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
                  return a.second > b.second;
              });

    // Group generals by territory
    QMap<QString, QList<GeneralPiece*>> generalsByTerritory;
    for (GeneralPiece *gen : availableGenerals) {
        generalsByTerritory[gen->getTerritoryName()].append(gen);
    }

    // Calculate average legion size to determine what counts as "small"
    // In early game with 4 troops and 4 generals, average is 1 - so 1 troop is NOT small
    // Later with 20 troops and 6 generals, average is ~3 - so 1 troop IS small
    int totalTroopsInLegions = 0;
    int generalsWithTroops = 0;
    for (GeneralPiece *gen : availableGenerals) {
        int legionSize = gen->getLegion().size();
        totalTroopsInLegions += legionSize;
        if (legionSize > 0) {
            generalsWithTroops++;
        }
    }
    double averageLegionSize = (generalsWithTroops > 0) ?
        static_cast<double>(totalTroopsInLegions) / generalsWithTroops : 0.0;
    qDebug() << "Average legion size:" << averageLegionSize
             << "(total troops:" << totalTroopsInLegions << ", generals with troops:" << generalsWithTroops << ")";

    // For each territory with generals, assign them to DIFFERENT destinations
    for (auto it = generalsByTerritory.begin(); it != generalsByTerritory.end(); ++it) {
        QString fromTerritory = it.key();
        QList<GeneralPiece*> generalsHere = it.value();

        // Count available troops at this territory that are NOT already in a general's legion
        // First, build a set of troop IDs that are in any general's legion at this territory
        QSet<int> troopsInLegions;
        for (GeneralPiece *gen : generalsHere) {
            for (int troopId : gen->getLegion()) {
                troopsInLegions.insert(troopId);
            }
        }

        QList<int> troopsAtThisTerritory;
        for (int i = 0; i < availableTroops.size(); i++) {
            const TroopInfo &troop = availableTroops[i];
            if (troop.territory == fromTerritory &&
                !assignedTroopIds.contains(troop.piece->getUniqueId()) &&
                !troopsInLegions.contains(troop.piece->getUniqueId())) {
                troopsAtThisTerritory.append(i);
            }
        }

        int totalTroopsHere = troopsAtThisTerritory.size();
        int numGenerals = generalsHere.size();

        // Calculate fair share per general
        int troopsPerGeneral = 1;
        if (numGenerals > 0 && totalTroopsHere > 0) {
            troopsPerGeneral = (totalTroopsHere + numGenerals - 1) / numGenerals;
            troopsPerGeneral = qMin(5, troopsPerGeneral);  // Cap at max legion size
        }

        qDebug() << "Territory" << fromTerritory << ":" << numGenerals << "generals,"
                 << totalTroopsHere << "troops -> " << troopsPerGeneral << "troops per general";

        // Assign each general to a different destination based on risk priority
        int troopIndex = 0;
        for (GeneralPiece *gen : generalsHere) {
            if (assignedGenerals.contains(gen)) continue;

            // RULE: Never leave troops behind unless another general here will take them
            //
            // If multiple generals are at a high-risk territory:
            // - Merge legions (one general takes all troops up to max 5)
            // - Extra generals with 0 troops return home to get out of harm's way
            //
            // If a general is alone with troops, they keep their troops and act normally
            int existingLegion = gen->getLegion().size();

            // Check if there are multiple generals here and we should consolidate
            if (numGenerals > 1 && totalTroopsHere == 0) {
                // Multiple generals, no free troops - consider consolidation
                // Find if another general here can take our troops
                GeneralPiece *recipientGeneral = nullptr;
                for (GeneralPiece *otherGen : generalsHere) {
                    if (otherGen == gen) continue;
                    if (assignedGenerals.contains(otherGen)) continue;
                    int otherLegionSize = otherGen->getLegion().size();
                    // Other general has room for our troops (max legion = 5)
                    if (otherLegionSize + existingLegion <= 5) {
                        recipientGeneral = otherGen;
                        break;
                    }
                }

                if (recipientGeneral != nullptr && existingLegion > 0) {
                    // Transfer troops to the other general and return home
                    qDebug() << "  General #" << gen->getNumber() << "at" << fromTerritory
                             << "transferring" << existingLegion << "troops to General #"
                             << recipientGeneral->getNumber() << "and returning home";

                    GeneralAssignment transferAssignment;
                    transferAssignment.general = gen;
                    transferAssignment.targetTerritory = homeProvince;
                    transferAssignment.missionType = "TransferAndReturnHome";
                    transferAssignment.priority = 50;
                    transferAssignment.troopsToTake = 0;  // Giving away troops, not taking
                    transferAssignment.reason = QString("Transfer troops to General #%1, return home for more")
                        .arg(recipientGeneral->getNumber());
                    plan.assignments.append(transferAssignment);
                    assignedGenerals.insert(gen);
                    continue;
                }
            }

            // General with 0 troops (and no one to receive troops from) should return home
            if (existingLegion == 0 && totalTroopsHere == 0) {
                int distToHome = getDistance(fromTerritory, homeProvince);
                if (distToHome > 0 && distToHome <= 4 && fromTerritory != homeProvince) {
                    qDebug() << "  General #" << gen->getNumber() << "at" << fromTerritory
                             << "has no troops -> returning home";
                    GeneralAssignment returnAssignment;
                    returnAssignment.general = gen;
                    returnAssignment.targetTerritory = homeProvince;
                    returnAssignment.missionType = "ReturnHome";
                    returnAssignment.priority = 50;
                    returnAssignment.troopsToTake = 0;
                    returnAssignment.reason = "No troops, returning home for reinforcements";
                    plan.assignments.append(returnAssignment);
                    assignedGenerals.insert(gen);
                    continue;
                }
            }

            // Find best unassigned destination reachable from here
            // IMPORTANT: Prefer UNCLAIMED or ENEMY territories over our own
            QString bestDest;
            int bestScore = 0;

            for (const auto &dest : sortedDestinations) {
                QString destName = dest.first;
                int destScore = dest.second;

                // Skip if already assigned a general there (spread out!)
                if (assignedDestinations.contains(destName)) continue;

                // Skip if it's our current territory
                if (destName == fromTerritory) continue;

                // Skip if we already have a general stationed at this destination
                // This prevents multiple generals converging on the same territory
                // Exception: home province - multiple generals CAN return home to pick up troops
                // Note: homeProvince is already defined earlier in this function
                if (destName != homeProvince) {
                    bool generalAlreadyThere = false;
                    for (GeneralPiece *otherGen : availableGenerals) {
                        if (otherGen == gen) continue;  // Skip self
                        if (otherGen->getTerritoryName() == destName) {
                            generalAlreadyThere = true;
                            break;
                        }
                    }
                    if (generalAlreadyThere) {
                        qDebug() << "    Skipping" << destName << "- another general already stationed there";
                        continue;
                    }
                }

                // Skip territories we already own UNLESS they're threatened AND we have enough troops to help
                // We want to EXPAND, not shuffle troops between owned territories
                // A general with 1-2 troops can't meaningfully reinforce a threatened territory
                bool weOwnIt = player->ownsTerritory(destName);
                if (weOwnIt) {
                    // Check how many troops this general would bring
                    int troopsWeCanBring = gen->getLegion().size();
                    if (troopsWeCanBring < totalTroopsHere) {
                        // We might pick up more troops here
                        troopsWeCanBring = qMin(5, totalTroopsHere);
                    }

                    // Only reinforce threatened territory if we have meaningful force (3+ troops)
                    bool isThreatened = threatenedTerritories.contains(destName);
                    if (!isThreatened || troopsWeCanBring < 3) {
                        continue;  // Skip - either not threatened, or we're too weak to help
                    }
                }

                // Check if reachable (adjacent or via path, including galley)
                int distance = getDistance(fromTerritory, destName);
                if (distance == 0 || distance > 4) continue;  // Not reachable in reasonable moves

                // Check if this is a galley route
                bool isGalleyRoute = requiresGalley(fromTerritory, destName);
                bool hasGalley = hasGalleyAtTerritory(fromTerritory);

                // Skip galley routes if no galley available
                if (isGalleyRoute && !hasGalley) continue;

                // Score adjusted by distance
                int adjustedScore = destScore / distance;

                // Bonus for expansion targets (unclaimed/enemy)
                if (!weOwnIt) {
                    adjustedScore = static_cast<int>(adjustedScore * 1.5);  // 50% bonus for expansion
                }

                // Bonus for galley routes (use those galleys!)
                if (isGalleyRoute && hasGalley) {
                    adjustedScore = static_cast<int>(adjustedScore * 1.3);  // 30% bonus for galley usage
                    qDebug() << "    Galley route available:" << fromTerritory << "->" << destName << "score:" << adjustedScore;
                }

                if (adjustedScore > bestScore) {
                    bestScore = adjustedScore;
                    bestDest = destName;
                }
            }

            if (bestDest.isEmpty()) {
                qDebug() << "  General #" << gen->getNumber() << "at" << fromTerritory << "- no valid destination found";
                continue;
            }

            // Create assignment
            GeneralAssignment assignment;
            assignment.general = gen;
            assignment.targetTerritory = bestDest;
            assignment.missionType = "ExpandWithTroops";
            assignment.priority = bestScore;

            // Assign proportional troops
            int troopsToTake = qMin(troopsPerGeneral, totalTroopsHere - troopIndex);
            troopsToTake = qMin(troopsToTake, 5);  // Max 5 per legion
            troopsToTake = qMax(troopsToTake, 0);

            for (int i = 0; i < troopsToTake && (troopIndex + i) < troopsAtThisTerritory.size(); i++) {
                int troopIdx = troopsAtThisTerritory[troopIndex + i];
                const TroopInfo &troop = availableTroops[troopIdx];
                assignment.troopIds.append(troop.piece->getUniqueId());
                assignedTroopIds.insert(troop.piece->getUniqueId());
            }
            troopIndex += troopsToTake;
            assignment.troopsToTake = assignment.troopIds.size();

            // Check if general already has troops in their legion (from previous moves this turn)
            int existingLegionSize = gen->getLegion().size();

            // Don't create assignments with 0 troops UNLESS the general already has troops in their legion
            if (assignment.troopsToTake == 0 && existingLegionSize == 0) {
                qDebug() << "  General #" << gen->getNumber() << "- no troops to assign (total here:" << totalTroopsHere << ")";
                continue;
            }

            // If general has existing legion, they can still move even without new troops
            if (assignment.troopsToTake == 0 && existingLegionSize > 0) {
                assignment.troopsToTake = existingLegionSize;  // Will move with existing legion
                assignment.missionType = "ContinueWithLegion";
                qDebug() << "  General #" << gen->getNumber() << "has existing legion of" << existingLegionSize;
            }

            assignment.reason = QString("Move %1 troops from %2 to %3 (risk score: %4)")
                .arg(assignment.troopsToTake).arg(fromTerritory).arg(bestDest).arg(bestScore);

            plan.assignments.append(assignment);
            assignedGenerals.insert(gen);
            assignedDestinations.insert(bestDest);  // Mark destination as taken
            plan.generalsUsed++;
            plan.territoriesTargeted++;
            plan.totalTroopsDeployed += assignment.troopsToTake;

            qDebug() << "  Assigned General #" << gen->getNumber()
                     << "to take" << assignment.troopsToTake << "troops from" << fromTerritory
                     << "to" << bestDest << "(score:" << bestScore << ")";
        }
    }

    // Step 7: Identify territories with stranded troops (assigned destination but no general there)
    // Then send idle generals to pick them up
    QMap<QString, int> strandedTroopCounts;  // territory -> count of stranded troops
    QMap<QString, int> strandedTroopScores;  // territory -> total score of stranded troops

    for (int i = 0; i < availableTroops.size(); i++) {
        const TroopInfo &troop = availableTroops[i];
        if (troop.assignedDestination.isEmpty()) continue;  // Not assigned
        if (assignedTroopIds.contains(troop.piece->getUniqueId())) continue;  // Already assigned to a general

        // This troop has a destination but wasn't picked up by any general
        strandedTroopCounts[troop.territory]++;
        strandedTroopScores[troop.territory] += troop.effectiveScore;
    }

    if (!strandedTroopCounts.isEmpty()) {
        qDebug() << "=== STRANDED TROOPS (need general pickup) ===";
        for (auto it = strandedTroopCounts.begin(); it != strandedTroopCounts.end(); ++it) {
            qDebug() << "  " << it.key() << ":" << it.value() << "troops (score=" << strandedTroopScores[it.key()] << ")";
        }
    }

    // Sort stranded territories by score (highest first)
    QList<QPair<QString, int>> strandedList;
    for (auto it = strandedTroopCounts.begin(); it != strandedTroopCounts.end(); ++it) {
        strandedList.append({it.key(), strandedTroopScores[it.key()]});
    }
    std::sort(strandedList.begin(), strandedList.end(),
              [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
                  return a.second > b.second;
              });

    // Send idle generals to pick up stranded troops
    // For LARGE groups (3+ troops), generals will travel further to pick them up
    // A strong legion without a general is a waste!
    for (const auto &stranded : strandedList) {
        QString strandedTerritory = stranded.first;
        int strandedScore = stranded.second;
        int troopCount = strandedTroopCounts[strandedTerritory];

        // Determine max distance based on how many troops are stranded
        // More troops = worth traveling further to pick up
        int maxDistance = 1;  // Default: only adjacent
        if (troopCount >= 5) {
            maxDistance = 3;  // Large group - worth a longer trip
        } else if (troopCount >= 3) {
            maxDistance = 2;  // Medium group - worth going a bit further
        }

        // Find the closest idle general within max distance
        GeneralPiece *closestGeneral = nullptr;
        int closestDistance = 999;

        for (GeneralPiece *gen : availableGenerals) {
            if (assignedGenerals.contains(gen)) continue;

            // Calculate distance from this general to the stranded territory
            int dist = getDistance(gen->getTerritoryName(), strandedTerritory);

            if (dist <= maxDistance && dist < closestDistance) {
                closestDistance = dist;
                closestGeneral = gen;
            }
        }

        if (closestGeneral && closestDistance <= maxDistance) {
            // Assign this general to pick up the stranded troops
            GeneralAssignment assignment;
            assignment.general = closestGeneral;
            assignment.targetTerritory = strandedTerritory;
            assignment.missionType = "PickupTroops";
            // Higher priority for larger groups
            assignment.priority = strandedScore / 2 + troopCount * 30;
            assignment.troopsToTake = 0;  // Will pick up when they arrive
            assignment.reason = QString("Go to %1 to pick up %2 stranded troops")
                .arg(strandedTerritory).arg(troopCount);

            plan.assignments.append(assignment);
            assignedGenerals.insert(closestGeneral);

            qDebug() << "  General #" << closestGeneral->getNumber() << "at" << closestGeneral->getTerritoryName()
                     << "assigned to pick up" << troopCount << "troops at" << strandedTerritory
                     << "(distance=" << closestDistance << ")";
        } else {
            qDebug() << "  No general within" << maxDistance << "moves to pick up" << troopCount
                     << "troops at" << strandedTerritory;
        }
    }

    // Step 7b: Generals without troops should return to home city to pick up purchases
    // These are generals who weren't assigned in Step 6 because they had no troops at their location
    // Note: homeProvince is already defined earlier in this function
    qDebug() << "=== RETURN-HOME CHECK (generals without troops) ===";

    for (GeneralPiece *gen : availableGenerals) {
        if (assignedGenerals.contains(gen)) continue;

        QString genTerritory = gen->getTerritoryName();
        int legionSize = gen->getLegion().size();

        // Only applies to generals with ZERO troops
        // Generals with even 1 troop should stay and expand/collect taxes, not return home
        if (legionSize > 0) {
            qDebug() << "  General #" << gen->getNumber() << "at" << genTerritory
                     << "has" << legionSize << "troops - staying to expand";
            continue;
        }

        // Skip if already at home
        if (genTerritory == homeProvince) {
            qDebug() << "  General #" << gen->getNumber() << "already at home" << homeProvince;
            continue;
        }

        // Check if we can reach home
        int distToHome = getDistance(genTerritory, homeProvince);
        if (distToHome == 0 || distToHome > 4) {
            qDebug() << "  General #" << gen->getNumber() << "at" << genTerritory
                     << "cannot reach home (distance=" << distToHome << ")";
            continue;
        }

        // Check if galley is needed and available
        bool needsGalley = requiresGalley(genTerritory, homeProvince);
        if (needsGalley && !hasGalleyAtTerritory(genTerritory)) {
            qDebug() << "  General #" << gen->getNumber() << "at" << genTerritory
                     << "needs galley to reach home but none available";
            continue;
        }

        // Create assignment to return home
        GeneralAssignment assignment;
        assignment.general = gen;
        assignment.targetTerritory = homeProvince;
        assignment.missionType = "ReturnHome";
        assignment.priority = 100;  // Medium priority - get troops for next turn
        assignment.troopsToTake = 0;
        assignment.reason = QString("Return to %1 to pick up purchased troops").arg(homeProvince);

        plan.assignments.append(assignment);
        assignedGenerals.insert(gen);

        qDebug() << "  General #" << gen->getNumber() << "at" << genTerritory
                 << "returning home to" << homeProvince << "(distance=" << distToHome << ")";
    }

    // Step 8: Check if under-strength generals should RETREAT due to enemy threat
    // If a general's legion is smaller than the enemy force that can attack them, they should retreat!
    qDebug() << "=== RISK-BASED RETREAT CHECK ===";

    for (GeneralPiece *gen : availableGenerals) {
        if (assignedGenerals.contains(gen)) continue;

        QString genTerritory = gen->getTerritoryName();
        int legionSize = gen->getLegion().size();

        // Check the risk at this territory
        if (!riskMap.contains(genTerritory)) continue;

        const TerritoryRisk &risk = riskMap[genTerritory];
        int enemyThreat = risk.enemyMaxForce;

        // If enemy can attack with more force than we have, we should retreat
        // Give a small buffer - retreat if enemy has at least 2 more troops than us
        if (enemyThreat > legionSize + 1) {
            qDebug() << "  General #" << gen->getNumber() << "at" << genTerritory
                     << "is OUTMATCHED (legion=" << legionSize << ", enemy threat=" << enemyThreat << ")";

            // Find a safe territory to retreat to (owned by us, lower threat)
            QString bestRetreat;
            int bestRetreatScore = -999;

            QStringList neighbors = graph->getNeighbors(genTerritory);
            for (const QString &neighbor : neighbors) {
                if (graph->isSeaTerritory(neighbor)) continue;
                if (!player->ownsTerritory(neighbor)) continue;  // Can only retreat to owned territory

                // Check threat at this neighbor
                int neighborThreat = 0;
                if (riskMap.contains(neighbor)) {
                    neighborThreat = riskMap[neighbor].enemyMaxForce;
                }

                // Score: prefer lower threat, prefer home province
                int score = -neighborThreat;
                if (neighbor == homeProvince) {
                    score += 100;  // Strong preference to retreat home
                }

                if (score > bestRetreatScore) {
                    bestRetreatScore = score;
                    bestRetreat = neighbor;
                }
            }

            if (!bestRetreat.isEmpty()) {
                GeneralAssignment assignment;
                assignment.general = gen;
                assignment.targetTerritory = bestRetreat;
                assignment.missionType = "Retreat";
                assignment.priority = 200 + (enemyThreat - legionSize) * 30;  // Higher priority if more outmatched
                assignment.troopsToTake = legionSize;  // Take all troops when retreating
                assignment.reason = QString("Retreating from %1 (our force %2 vs enemy threat %3)")
                    .arg(genTerritory).arg(legionSize).arg(enemyThreat);

                plan.assignments.append(assignment);
                assignedGenerals.insert(gen);

                qDebug() << "    -> Retreating to" << bestRetreat;
            } else {
                qDebug() << "    -> No safe retreat available!";
            }
        }
    }

    // Remaining generals without assignments - check if they have stranded troops at their location
    // If so, pick them up and take them somewhere useful!
    for (GeneralPiece *gen : availableGenerals) {
        if (assignedGenerals.contains(gen)) continue;

        QString genTerritory = gen->getTerritoryName();

        // Check if there are stranded troops at this general's location
        int strandedHere = strandedTroopCounts.value(genTerritory, 0);

        GeneralAssignment assignment;
        assignment.general = gen;
        assignment.troopsToTake = 0;

        if (strandedHere > 0) {
            // There are stranded troops here! This general should pick them up and go somewhere.
            // Find the best destination for expansion
            QString bestDest;
            int bestScore = 0;

            QStringList neighbors = graph->getNeighbors(genTerritory);
            for (const QString &neighbor : neighbors) {
                if (graph->isSeaTerritory(neighbor)) continue;

                // Check if this is an expansion target (unclaimed)
                bool isUnclaimed = true;
                for (Player *p : allPlayers) {
                    if (p->ownsTerritory(neighbor)) {
                        isUnclaimed = false;
                        break;
                    }
                }

                if (isUnclaimed) {
                    // Check for enemy troops
                    bool hasEnemyTroops = false;
                    for (Player *p : allPlayers) {
                        if (p == player) continue;
                        if (!p->getPiecesAtTerritory(neighbor).isEmpty()) {
                            hasEnemyTroops = true;
                            break;
                        }
                    }

                    if (!hasEnemyTroops) {
                        int score = effectiveRiskScore.value(neighbor, 200);
                        if (score > bestScore) {
                            bestScore = score;
                            bestDest = neighbor;
                        }
                    }
                }
            }

            if (!bestDest.isEmpty()) {
                // Collect stranded troops and go expand
                assignment.targetTerritory = bestDest;
                assignment.missionType = "ExpandWithTroops";
                assignment.priority = bestScore;

                // Gather stranded troop IDs (up to 5)
                int troopsToTake = qMin(5, strandedHere);
                for (int i = 0; i < availableTroops.size() && assignment.troopIds.size() < troopsToTake; i++) {
                    const TroopInfo &troop = availableTroops[i];
                    if (troop.territory == genTerritory &&
                        !troop.assignedDestination.isEmpty() &&
                        !assignedTroopIds.contains(troop.piece->getUniqueId())) {
                        assignment.troopIds.append(troop.piece->getUniqueId());
                        assignedTroopIds.insert(troop.piece->getUniqueId());
                    }
                }
                assignment.troopsToTake = assignment.troopIds.size();

                if (assignment.troopsToTake > 0) {
                    assignment.reason = QString("Picking up %1 stranded troops and expanding to %2")
                        .arg(assignment.troopsToTake).arg(bestDest);

                    plan.assignments.append(assignment);
                    assignedGenerals.insert(gen);
                    plan.generalsUsed++;
                    plan.totalTroopsDeployed += assignment.troopsToTake;

                    qDebug() << "  General #" << gen->getNumber() << "at" << genTerritory
                             << "picking up" << assignment.troopsToTake << "stranded troops -> expanding to" << bestDest;
                    continue;
                }
            }
        }

        // No stranded troops or no valid destination - stay in place
        assignment.targetTerritory = genTerritory;
        assignment.missionType = "StayHome";
        assignment.priority = 0;
        assignment.reason = "Waiting for troops";

        plan.assignments.append(assignment);
        qDebug() << "  General #" << gen->getNumber() << "staying at" << genTerritory << "(waiting for troops)";
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

ScoredMove AIDecisionMaker::getNextMoveFromPlan(const MovementPlan &plan, Player *player, const QList<Player*> &allPlayers, MapGraph *graph)
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

        // CRITICAL: Don't move generals without troops unless they're going to pick some up
        // A general without troops cannot capture territory or fight - they're useless
        // Exception: "PickupTroops", "Retreat", or "ReturnHome" missions where the general is going TO get troops
        // NOTE: Check actual legion size, not assignment.troopsToTake, because the general may already
        // have troops from a previous move in this turn
        bool isTroopPickupMission = (assignment.missionType == "PickupTroops" ||
                                     assignment.missionType == "Retreat" ||
                                     assignment.missionType == "ReturnHome" ||
                                     assignment.missionType == "ContinueWithLegion" ||
                                     assignment.missionType == "TransferAndReturnHome");
        int actualLegionSize = 0;
        if (general->getType() == GamePiece::Type::General) {
            actualLegionSize = static_cast<GeneralPiece*>(general)->getLegion().size();
        }
        if (actualLegionSize == 0 && assignment.troopsToTake == 0 && !isTroopPickupMission) {
            qDebug() << "Skipping General #" << static_cast<GeneralPiece*>(general)->getNumber()
                     << "- has no troops and mission is" << assignment.missionType;
            continue;
        }

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

        // OPPORTUNISTIC EXPANSION: Even if target is adjacent, check for better unclaimed territories
        // A general should always capture unclaimed land if it's right there!
        int legionSize = assignment.troopsToTake;
        if (legionSize >= 1) {
            QString bestOpportunity;
            int bestValue = 0;

            for (const QString &neighbor : neighbors) {
                if (graph->isSeaTerritory(neighbor)) continue;
                if (neighbor == targetTerritory) continue;  // Don't compare with our actual target

                Territory territory = graph->getTerritory(neighbor);
                if (territory.name.isEmpty()) continue;

                // Check ownership using player methods
                bool isOurs = player->ownsTerritory(neighbor);
                bool isUnclaimed = !isOurs;

                // Check if any other player owns it
                for (Player *otherPlayer : allPlayers) {
                    if (otherPlayer != player && otherPlayer->ownsTerritory(neighbor)) {
                        isUnclaimed = false;
                        break;
                    }
                }

                bool isEnemy = !isOurs && !isUnclaimed;

                if (isUnclaimed) {
                    // Unclaimed territory - always worth capturing
                    int value = territory.value;
                    if (value > bestValue) {
                        bestValue = value;
                        bestOpportunity = neighbor;
                    }
                } else if (isEnemy) {
                    // Enemy territory - check actual enemy strength before attacking!
                    // Count enemy troops there
                    int enemyTroops = 0;
                    for (Player *p : allPlayers) {
                        if (p == player) continue;
                        for (GamePiece *piece : p->getPiecesAtTerritory(neighbor)) {
                            if (piece->getType() == GamePiece::Type::Infantry ||
                                piece->getType() == GamePiece::Type::Cavalry ||
                                piece->getType() == GamePiece::Type::Catapult) {
                                enemyTroops++;
                            }
                        }
                    }

                    // Only attack if we have significant advantage (+2 or +50%, whichever is more)
                    int minAdvantage = qMax(2, (enemyTroops + 1) / 2);
                    if (legionSize >= enemyTroops + minAdvantage) {
                        int value = territory.value;
                        if (value > bestValue) {
                            bestValue = value;
                            bestOpportunity = neighbor;
                        }
                    }
                }
            }

            // Take the opportunity if it's higher value than our target (or target is already ours)
            Territory targetTerr = graph->getTerritory(targetTerritory);
            bool targetAlreadyOurs = player->ownsTerritory(targetTerritory);
            int targetValue = targetTerr.value;

            if (!bestOpportunity.isEmpty() && (targetAlreadyOurs || bestValue > targetValue)) {
                qDebug() << "OPPORTUNISTIC EXPANSION: Capturing" << bestOpportunity
                         << "(value=" << bestValue << ") instead of" << targetTerritory;
                nextStep = bestOpportunity;
            }
        }

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
            }

            if (!found) {
                // No land path found - check if galley is available at current territory
                // Check for: 1) beached galley at this territory, 2) galley at sea in adjacent zone
                bool hasGalleyAvailable = false;

                for (GalleyPiece *galley : player->getGalleys()) {
                    if (galley->hasTransportedThisTurn() ||
                        galley->hasLeaderAboard() ||
                        galley->getMovesRemaining() < 0.5) {
                        continue;
                    }

                    // Check for BEACHED galley at this territory
                    if (galley->isBeached() && galley->getTerritoryName() == currentTerritory) {
                        hasGalleyAvailable = true;
                        break;
                    }

                    // Check for galley at sea in adjacent zone
                    QString galleyLocation = galley->getTerritoryName();
                    if (!galley->isBeached() && graph->isSeaTerritory(galleyLocation)) {
                        if (neighbors.contains(galleyLocation)) {
                            hasGalleyAvailable = true;
                            break;
                        }
                    }
                }

                if (hasGalleyAvailable) {
                    // Galley available - target might be reachable via sea
                    qDebug() << "No land path from" << currentTerritory << "to" << targetTerritory << "- galley available for transport";
                    nextStep = targetTerritory;  // Try direct - galley can make it reachable
                } else {
                    // No galley available - can't reach this target, skip this assignment
                    qDebug() << "No land path from" << currentTerritory << "to" << targetTerritory << "- NO galley available, skipping";
                    continue;
                }
            }
        }

        // Check if nextStep already has one of our generals - if so, skip this assignment
        // This prevents multiple generals from converging on the same territory
        bool generalAlreadyAtNextStep = false;
        for (GeneralPiece *otherGen : player->getGenerals()) {
            if (otherGen == general) continue;
            if (otherGen->getTerritoryName() == nextStep) {
                generalAlreadyAtNextStep = true;
                qDebug() << "Skipping move to" << nextStep << "- another general already there";
                break;
            }
        }
        if (generalAlreadyAtNextStep) {
            continue;  // Skip to next assignment in plan
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
