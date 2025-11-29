#include "reachabilitycalculator.h"
#include "../gamepiece.h"
#include "../player.h"
#include "../mapgraph.h"
#include "../building.h"
#include <QDebug>

ReachabilityCalculator::ReachabilityCalculator()
{
}

QMap<QString, ReachInfo> ReachabilityCalculator::getReachableFrom(GamePiece *leader, MapGraph *graph, Player *player, int turnMultiplier)
{
    QMap<QString, ReachInfo> results;

    if (!leader || !graph || !player) {
        return results;
    }

    QString startTerritory = leader->getTerritoryName();
    double movesRemaining = leader->getMovesRemaining();

    // For threat assessment purposes, assume leaders have at least 1 move
    // (enemy leaders may have 0 moves remaining after their turn, but they'll
    // have full moves on THEIR next turn when they could attack us)
    if (movesRemaining < 1.0) {
        movesRemaining = 2.0;  // Assume full moves for threat projection
    }

    // Apply turn multiplier to leader's moves for multi-turn projection
    movesRemaining *= turnMultiplier;

    if (startTerritory.isEmpty()) {
        return results;
    }

    GamePiece::Type leaderType = leader->getType();

    // Handle galleys separately (sea movement only)
    if (leaderType == GamePiece::Type::Galley) {
        QSet<QString> visited;
        QMap<QString, double> landingSpots;

        // BFS through sea zones
        QList<QPair<QString, double>> toVisit;
        toVisit.append({startTerritory, movesRemaining});
        visited.insert(startTerritory);

        while (!toVisit.isEmpty()) {
            auto [current, moves] = toVisit.takeFirst();

            // Check land neighbors for potential landing spots
            QList<QString> neighbors = graph->getNeighbors(current);
            for (const QString &neighbor : neighbors) {
                if (!graph->isSeaTerritory(neighbor)) {
                    // This is a land territory - galley can drop off here
                    if (!landingSpots.contains(neighbor) || landingSpots[neighbor] < moves) {
                        landingSpots[neighbor] = moves;
                    }
                } else if (moves >= 1.0 && !visited.contains(neighbor)) {
                    // Sea territory - galley can move here
                    visited.insert(neighbor);
                    toVisit.append({neighbor, moves - 1.0});
                }
            }
        }

        // Convert landing spots to ReachInfo
        // Troops aboard galley travel by sea, no land movement restriction
        for (auto it = landingSpots.begin(); it != landingSpots.end(); ++it) {
            ReachInfo info;
            info.territoryName = it.key();
            info.leadersWhoCanReach.append(leader);
            info.maxTroopStrength = calculateTroopStrength(leader, player, 0.0, turnMultiplier);  // All troops can land from galley
            info.bestMovesRemaining = it.value();
            info.viaGalley = true;
            results[it.key()] = info;
        }

        return results;
    }

    // For Caesar/General - land movement
    QSet<QString> visited;
    QMap<QString, double> reachableByLand;

    // Check if leader is on a galley (needs to disembark first)
    bool isOnGalley = leader->isOnGalley();

    if (isOnGalley) {
        // Leader is on a galley - can only reach adjacent land territories
        // The galley's position is the leader's current territory (sea zone)
        // Troops traveled by sea, so all can disembark (0 land moves used)
        QList<QString> neighbors = graph->getNeighbors(startTerritory);
        for (const QString &neighbor : neighbors) {
            if (!graph->isSeaTerritory(neighbor)) {
                // Can disembark here
                ReachInfo info;
                info.territoryName = neighbor;
                info.leadersWhoCanReach.append(leader);
                info.maxTroopStrength = calculateTroopStrength(leader, player, 0.0, turnMultiplier);  // All troops can disembark
                info.bestMovesRemaining = movesRemaining - 1.0;  // Disembarking costs 1 move
                info.viaGalley = true;
                results[neighbor] = info;
            }
        }
    } else {
        // Normal land movement - BFS
        getReachableByLand(startTerritory, movesRemaining, graph, visited, reachableByLand, movesRemaining);

        // Convert to ReachInfo
        for (auto it = reachableByLand.begin(); it != reachableByLand.end(); ++it) {
            if (it.key() != startTerritory) {  // Don't include starting territory
                ReachInfo info;
                info.territoryName = it.key();
                info.leadersWhoCanReach.append(leader);
                // Calculate how many moves it takes to reach this territory
                double movesUsed = movesRemaining - it.value();
                info.maxTroopStrength = calculateTroopStrength(leader, player, movesUsed, turnMultiplier);
                info.bestMovesRemaining = it.value();
                results[it.key()] = info;
            }
        }

        // Check road network (if starting from a territory with our city)
        // Road travel costs 1 move for the entire trip, all troops can follow
        QStringList roadReachable = getReachableByRoad(startTerritory, player, graph);
        for (const QString &territory : roadReachable) {
            if (territory != startTerritory) {
                if (results.contains(territory)) {
                    // Already reachable by land - mark as also via road
                    results[territory].viaRoad = true;
                    // Road travel costs 1 move, so update if better
                    if (movesRemaining - 1.0 > results[territory].bestMovesRemaining) {
                        results[territory].bestMovesRemaining = movesRemaining - 1.0;
                        // Update troop strength for road travel (1 move)
                        int roadTroops = calculateTroopStrength(leader, player, 1.0, turnMultiplier);
                        if (roadTroops > results[territory].maxTroopStrength) {
                            results[territory].maxTroopStrength = roadTroops;
                        }
                    }
                } else {
                    // Only reachable via road
                    ReachInfo info;
                    info.territoryName = territory;
                    info.leadersWhoCanReach.append(leader);
                    info.maxTroopStrength = calculateTroopStrength(leader, player, 1.0, turnMultiplier);  // Road costs 1 move
                    info.bestMovesRemaining = movesRemaining - 1.0;  // Road travel costs 1 move
                    info.viaRoad = true;
                    results[territory] = info;
                }
            }
        }

        // Check galley transport options
        // Troops travel by sea with the galley, no land movement restriction
        QMap<QString, double> galleyReachable = getReachableByGalley(leader, player, graph);
        for (auto it = galleyReachable.begin(); it != galleyReachable.end(); ++it) {
            if (results.contains(it.key())) {
                results[it.key()].viaGalley = true;
                if (it.value() > results[it.key()].bestMovesRemaining) {
                    results[it.key()].bestMovesRemaining = it.value();
                }
                // Galley transport allows all troops to follow (0 land moves used)
                int galleyTroops = calculateTroopStrength(leader, player, 0.0, turnMultiplier);
                if (galleyTroops > results[it.key()].maxTroopStrength) {
                    results[it.key()].maxTroopStrength = galleyTroops;
                }
            } else {
                ReachInfo info;
                info.territoryName = it.key();
                info.leadersWhoCanReach.append(leader);
                info.maxTroopStrength = calculateTroopStrength(leader, player, 0.0, turnMultiplier);  // All troops can take galley
                info.bestMovesRemaining = it.value();
                info.viaGalley = true;
                results[it.key()] = info;
            }
        }
    }

    return results;
}

void ReachabilityCalculator::getReachableByLand(const QString &startTerritory,
                                                double movesRemaining,
                                                MapGraph *graph,
                                                QSet<QString> &visited,
                                                QMap<QString, double> &results,
                                                double originalMoves)
{
    if (movesRemaining < 1.0 || startTerritory.isEmpty()) {
        return;
    }

    visited.insert(startTerritory);

    // Use BFS to find ALL territories reachable within movesRemaining moves
    // This is important for PLANNING - we need to know what territories a general
    // can reach by the END of their turn, not just after one step.
    // The AI movement loop will handle the actual step-by-step execution.
    QList<QString> neighbors = graph->getNeighbors(startTerritory);

    for (const QString &neighbor : neighbors) {
        // Skip sea territories (land movement only)
        if (graph->isSeaTerritory(neighbor)) {
            continue;
        }

        // Each land move costs 1 movement point
        double newMoves = movesRemaining - 1.0;

        // Check if this is a new territory or a better path to an existing one
        bool isBetterPath = !results.contains(neighbor) || results[neighbor] < newMoves;

        // Add/update neighbor if not already added with better moves
        if (isBetterPath) {
            results[neighbor] = newMoves;
        }

        // RECURSE to find territories reachable in 2+ moves
        // We recurse if:
        // 1. We have moves remaining (newMoves >= 1.0)
        // 2. Either: we haven't visited this neighbor yet, OR we found a better path
        // The second condition allows us to re-explore from a node if we reach it faster
        if (newMoves >= 1.0 && (!visited.contains(neighbor) || isBetterPath)) {
            visited.insert(neighbor);  // Mark as visited before recursing
            getReachableByLand(neighbor, newMoves, graph, visited, results, originalMoves);
        }
    }
}

QStringList ReachabilityCalculator::getReachableByRoad(const QString &startTerritory, Player *player, MapGraph *graph)
{
    if (!player || !graph) {
        return QStringList();
    }

    // Check if we have a city at the start territory
    City *startCity = player->getCityAtTerritory(startTerritory);
    if (!startCity) {
        return QStringList();  // No road network access without a city
    }

    // Use the graph's road-connected territories function
    return graph->getRoadConnectedTerritories(startTerritory, player);
}

QMap<QString, double> ReachabilityCalculator::getReachableByGalley(GamePiece *leader, Player *player, MapGraph *graph)
{
    QMap<QString, double> results;

    if (!leader || !player || !graph) {
        return results;
    }

    // Leader must not already be on a galley
    if (leader->isOnGalley()) {
        return results;
    }

    QString leaderTerritory = leader->getTerritoryName();

    // Find adjacent sea zones
    QList<QString> neighbors = graph->getNeighbors(leaderTerritory);
    for (const QString &seaZone : neighbors) {
        if (!graph->isSeaTerritory(seaZone)) {
            continue;
        }

        // Check if player has a galley here that can transport
        for (GalleyPiece *galley : player->getGalleys()) {
            if (galley->getTerritoryName() == seaZone &&
                !galley->hasTransportedThisTurn() &&
                !galley->hasLeaderAboard() &&
                galley->getMovesRemaining() >= 0.5) {

                // Found a galley - calculate where it can take us
                double galleyMoves = galley->getMovesRemaining() - 0.5;  // Boarding costs 0.5

                // BFS from galley position through sea zones
                QSet<QString> visited;
                QList<QPair<QString, double>> toVisit;
                toVisit.append({seaZone, galleyMoves});
                visited.insert(seaZone);

                while (!toVisit.isEmpty()) {
                    auto [current, moves] = toVisit.takeFirst();

                    // Check land neighbors for disembark options
                    QList<QString> seaNeighbors = graph->getNeighbors(current);
                    for (const QString &neighbor : seaNeighbors) {
                        if (!graph->isSeaTerritory(neighbor)) {
                            // Land territory - can disembark here
                            // Disembarking doesn't cost the leader extra moves
                            double landMoves = leader->getMovesRemaining() - 1.0;  // Landing costs 1 leader move
                            if (landMoves >= 0) {
                                if (!results.contains(neighbor) || results[neighbor] < landMoves) {
                                    results[neighbor] = landMoves;
                                }
                            }
                        } else if (moves >= 1.0 && !visited.contains(neighbor)) {
                            // Sea territory - galley can move here
                            visited.insert(neighbor);
                            toVisit.append({neighbor, moves - 1.0});
                        }
                    }
                }
            }
        }
    }

    return results;
}

QMap<QString, ReachInfo> ReachabilityCalculator::getAllReachable(Player *player, MapGraph *graph, int turnMultiplier)
{
    QMap<QString, ReachInfo> results;

    if (!player || !graph) {
        return results;
    }

    // For each destination territory, track which starting territories have contributed troops
    // This avoids double-counting troops when multiple leaders share the same starting territory
    QMap<QString, QSet<QString>> destToStartTerritories;  // destination -> set of starting territories counted

    // Helper lambda to process a leader
    auto processLeader = [&](GamePiece *leader) {
        QMap<QString, ReachInfo> leaderReach = getReachableFrom(leader, graph, player, turnMultiplier);
        QString startTerritory = leader->getTerritoryName();
        // Note: troop count is now per-destination (stored in ReachInfo.maxTroopStrength)
        // because different destinations may have different distances and thus different troops can reach

        for (auto it = leaderReach.begin(); it != leaderReach.end(); ++it) {
            QString destTerritory = it.key();
            int troopsForThisDest = it.value().maxTroopStrength;

            if (results.contains(destTerritory)) {
                // Merge: add this leader to existing entry
                results[destTerritory].leadersWhoCanReach.append(leader);

                // Only add troop strength if this starting territory hasn't been counted for this destination
                if (!destToStartTerritories[destTerritory].contains(startTerritory)) {
                    results[destTerritory].maxTroopStrength += troopsForThisDest;
                    destToStartTerritories[destTerritory].insert(startTerritory);
                }

                if (it.value().bestMovesRemaining > results[destTerritory].bestMovesRemaining) {
                    results[destTerritory].bestMovesRemaining = it.value().bestMovesRemaining;
                }
                results[destTerritory].viaRoad = results[destTerritory].viaRoad || it.value().viaRoad;
                results[destTerritory].viaGalley = results[destTerritory].viaGalley || it.value().viaGalley;
            } else {
                results[destTerritory] = it.value();
                destToStartTerritories[destTerritory].insert(startTerritory);
            }
        }
    };

    // Process all Caesars
    for (CaesarPiece *caesar : player->getCaesars()) {
        processLeader(caesar);
    }

    // Process all Generals
    for (GeneralPiece *general : player->getGenerals()) {
        processLeader(general);
    }

    // Process all Galleys (they can reach sea zones and landing spots)
    for (GalleyPiece *galley : player->getGalleys()) {
        QMap<QString, ReachInfo> galleyReach = getReachableFrom(galley, graph, player, turnMultiplier);
        QString galleyTerritory = galley->getTerritoryName();

        for (auto it = galleyReach.begin(); it != galleyReach.end(); ++it) {
            QString destTerritory = it.key();

            if (results.contains(destTerritory)) {
                results[destTerritory].leadersWhoCanReach.append(galley);

                // Galley troops - check if this galley's territory has been counted
                if (!destToStartTerritories[destTerritory].contains(galleyTerritory)) {
                    results[destTerritory].maxTroopStrength += it.value().maxTroopStrength;
                    destToStartTerritories[destTerritory].insert(galleyTerritory);
                }

                if (it.value().bestMovesRemaining > results[destTerritory].bestMovesRemaining) {
                    results[destTerritory].bestMovesRemaining = it.value().bestMovesRemaining;
                }
                results[destTerritory].viaGalley = true;
            } else {
                results[destTerritory] = it.value();
                destToStartTerritories[destTerritory].insert(galleyTerritory);
            }
        }
    }

    return results;
}

int ReachabilityCalculator::calculateTroopStrength(GamePiece *leader, Player *player, double movesUsed, int turnMultiplier)
{
    if (!leader || !player) {
        return 0;
    }

    int strength = 0;
    QString leaderTerritory = leader->getTerritoryName();

    // Movement ranges for different unit types, multiplied by turn count
    const double INFANTRY_MOVEMENT = 1.0 * turnMultiplier;
    const double CAVALRY_MOVEMENT = 2.0 * turnMultiplier;
    const double CATAPULT_MOVEMENT = 1.0 * turnMultiplier;

    // For galleys, count troops aboard (they travel with the galley, no movement limit)
    if (leader->getType() == GamePiece::Type::Galley) {
        GalleyPiece *galley = static_cast<GalleyPiece*>(leader);
        // Count troops in the galley's territory that are on this galley
        for (InfantryPiece *inf : player->getInfantry()) {
            if (inf->getOnGalley() == galley->getSerialNumber()) {
                strength++;
            }
        }
        for (CavalryPiece *cav : player->getCavalry()) {
            if (cav->getOnGalley() == galley->getSerialNumber()) {
                strength++;
            }
        }
        for (CatapultPiece *cat : player->getCatapults()) {
            if (cat->getOnGalley() == galley->getSerialNumber()) {
                strength++;
            }
        }
        return strength;
    }

    // For Caesar/General, count troops in the same territory that could follow them
    // NOTE: We do NOT check getMovesRemaining() here because:
    // 1. For enemy threat assessment, their troops have 0 moves (they used them on their turn)
    //    but they'll have full moves on THEIR next turn when attacking us
    // 2. This function is used for force projection, not current-turn movement planning
    // Movement range restrictions still apply (infantry=1, cavalry=2, catapults=1)

    // Infantry can only move 1 space
    if (movesUsed <= INFANTRY_MOVEMENT) {
        for (InfantryPiece *inf : player->getInfantry()) {
            if (inf->getTerritoryName() == leaderTerritory) {
                strength++;
            }
        }
    }

    // Cavalry can move 2 spaces
    if (movesUsed <= CAVALRY_MOVEMENT) {
        for (CavalryPiece *cav : player->getCavalry()) {
            if (cav->getTerritoryName() == leaderTerritory) {
                strength++;
            }
        }
    }

    // Catapults can only move 1 space
    if (movesUsed <= CATAPULT_MOVEMENT) {
        for (CatapultPiece *cat : player->getCatapults()) {
            if (cat->getTerritoryName() == leaderTerritory) {
                strength++;
            }
        }
    }

    return strength;
}

QString ReachabilityCalculator::generateReport(Player *player, MapGraph *graph)
{
    QString report;

    if (!player || !graph) {
        return "Error: Invalid player or graph";
    }

    report += QString("=== REACHABILITY REPORT FOR PLAYER %1 ===\n").arg(player->getId());
    report += QString("Home Province: %1\n\n").arg(player->getHomeProvinceName());

    // Report for each leader individually
    report += "--- LEADERS ---\n\n";

    // Caesars
    for (CaesarPiece *caesar : player->getCaesars()) {
        report += QString("CAESAR at %1 (%2 moves remaining)\n")
            .arg(caesar->getTerritoryName())
            .arg(caesar->getMovesRemaining(), 0, 'f', 1);

        int troopStrength = calculateTroopStrength(caesar, player);
        report += QString("  Troops available: %1\n").arg(troopStrength);

        if (caesar->isOnGalley()) {
            report += QString("  Status: On galley %1\n").arg(caesar->getOnGalley());
        }

        QMap<QString, ReachInfo> reachable = getReachableFrom(caesar, graph, player);
        report += QString("  Can reach %1 territories:\n").arg(reachable.size());

        for (auto it = reachable.begin(); it != reachable.end(); ++it) {
            QString method;
            if (it.value().viaRoad && it.value().viaGalley) {
                method = " [road/galley]";
            } else if (it.value().viaRoad) {
                method = " [road]";
            } else if (it.value().viaGalley) {
                method = " [galley]";
            }

            int territoryValue = graph->getValue(it.key());
            report += QString("    - %1 (value: %2, troops: %3, moves left: %4)%5\n")
                .arg(it.key())
                .arg(territoryValue)
                .arg(it.value().maxTroopStrength)
                .arg(it.value().bestMovesRemaining, 0, 'f', 1)
                .arg(method);
        }
        report += "\n";
    }

    // Generals
    for (GeneralPiece *general : player->getGenerals()) {
        report += QString("GENERAL #%1 at %2 (%3 moves remaining)\n")
            .arg(general->getNumber())
            .arg(general->getTerritoryName())
            .arg(general->getMovesRemaining(), 0, 'f', 1);

        int troopStrength = calculateTroopStrength(general, player);
        report += QString("  Troops available: %1\n").arg(troopStrength);

        if (general->isOnGalley()) {
            report += QString("  Status: On galley %1\n").arg(general->getOnGalley());
        }

        QMap<QString, ReachInfo> reachable = getReachableFrom(general, graph, player);
        report += QString("  Can reach %1 territories:\n").arg(reachable.size());

        for (auto it = reachable.begin(); it != reachable.end(); ++it) {
            QString method;
            if (it.value().viaRoad && it.value().viaGalley) {
                method = " [road/galley]";
            } else if (it.value().viaRoad) {
                method = " [road]";
            } else if (it.value().viaGalley) {
                method = " [galley]";
            }

            int territoryValue = graph->getValue(it.key());
            report += QString("    - %1 (value: %2, troops: %3, moves left: %4)%5\n")
                .arg(it.key())
                .arg(territoryValue)
                .arg(it.value().maxTroopStrength)
                .arg(it.value().bestMovesRemaining, 0, 'f', 1)
                .arg(method);
        }
        report += "\n";
    }

    // Galleys
    for (GalleyPiece *galley : player->getGalleys()) {
        report += QString("GALLEY at %1 (%2 moves remaining)\n")
            .arg(galley->getTerritoryName())
            .arg(galley->getMovesRemaining(), 0, 'f', 1);

        int troopStrength = calculateTroopStrength(galley, player);
        report += QString("  Troops aboard: %1\n").arg(troopStrength);

        if (galley->hasLeaderAboard()) {
            report += QString("  Leader aboard: ID %1\n").arg(galley->getLeaderAboard());
        }

        QMap<QString, ReachInfo> reachable = getReachableFrom(galley, graph, player);
        report += QString("  Can reach %1 landing spots:\n").arg(reachable.size());

        for (auto it = reachable.begin(); it != reachable.end(); ++it) {
            int territoryValue = graph->getValue(it.key());
            report += QString("    - %1 (value: %2, troops: %3, moves left: %4)\n")
                .arg(it.key())
                .arg(territoryValue)
                .arg(it.value().maxTroopStrength)
                .arg(it.value().bestMovesRemaining, 0, 'f', 1);
        }
        report += "\n";
    }

    // Summary - all reachable territories
    report += "--- SUMMARY ---\n\n";

    QMap<QString, ReachInfo> allReachable = getAllReachable(player, graph);
    report += QString("Total unique territories reachable: %1\n\n").arg(allReachable.size());

    // Sort by territory value (highest first)
    QList<QPair<QString, int>> sortedByValue;
    for (auto it = allReachable.begin(); it != allReachable.end(); ++it) {
        int value = graph->getValue(it.key());
        sortedByValue.append({it.key(), value});
    }
    std::sort(sortedByValue.begin(), sortedByValue.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    report += "All reachable territories (sorted by value):\n";
    for (const auto &pair : sortedByValue) {
        const ReachInfo &info = allReachable[pair.first];
        report += QString("  %1 (value: %2) - %3 leader(s), %4 max troops\n")
            .arg(pair.first)
            .arg(pair.second)
            .arg(info.leadersWhoCanReach.size())
            .arg(info.maxTroopStrength);
    }

    return report;
}

// === Risk Assessment Implementation ===

QMap<QString, TerritoryRisk> ReachabilityCalculator::assessAllTerritories(Player *us, const QList<Player*> &allPlayers, MapGraph *graph)
{
    QMap<QString, TerritoryRisk> results;

    if (!us || !graph) {
        return results;
    }

    // Get our reachability
    QMap<QString, ReachInfo> ourReach = getAllReachable(us, graph);

    // Get combined enemy reachability
    QMap<QString, ReachInfo> enemyReach;
    for (Player *player : allPlayers) {
        if (player == us) {
            continue;
        }

        QMap<QString, ReachInfo> playerReach = getAllReachable(player, graph);
        for (auto it = playerReach.begin(); it != playerReach.end(); ++it) {
            if (enemyReach.contains(it.key())) {
                // Merge: combine forces from multiple enemies
                enemyReach[it.key()].leadersWhoCanReach.append(it.value().leadersWhoCanReach);
                enemyReach[it.key()].maxTroopStrength += it.value().maxTroopStrength;
                if (it.value().bestMovesRemaining > enemyReach[it.key()].bestMovesRemaining) {
                    enemyReach[it.key()].bestMovesRemaining = it.value().bestMovesRemaining;
                }
            } else {
                enemyReach[it.key()] = it.value();
            }
        }
    }

    // Build set of all territories to analyze
    QSet<QString> allTerritories;
    for (const QString &t : ourReach.keys()) {
        allTerritories.insert(t);
    }
    for (const QString &t : enemyReach.keys()) {
        allTerritories.insert(t);
    }
    // Also include our owned territories (even if not reachable by movement)
    for (const QString &t : us->getOwnedTerritories()) {
        allTerritories.insert(t);
    }

    // Helper to count troops in a territory for a player
    auto countTroopsInTerritory = [](Player *player, const QString &territory) -> int {
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
    };

    // Helper to count leaders in a territory for a player
    auto countLeadersInTerritory = [](Player *player, const QString &territory) -> int {
        int count = 0;
        for (CaesarPiece *caesar : player->getCaesars()) {
            if (caesar->getTerritoryName() == territory) count++;
        }
        for (GeneralPiece *general : player->getGenerals()) {
            if (general->getTerritoryName() == territory) count++;
        }
        return count;
    };

    // Assess each territory
    for (const QString &territory : allTerritories) {
        TerritoryRisk risk;
        risk.territoryName = territory;

        // Our force projection (troops that can reach + troops already there)
        if (ourReach.contains(territory)) {
            risk.ourMaxForce = ourReach[territory].maxTroopStrength;
            risk.ourLeaders = ourReach[territory].leadersWhoCanReach;
        }

        // IMPORTANT: Also count troops ALREADY AT this territory!
        // (getReachableFrom excludes the starting territory, so troops AT a territory
        // aren't counted in ourReach for that territory)
        int ourLeadersAtTerritory = countLeadersInTerritory(us, territory);
        int ourGarrison = countTroopsInTerritory(us, territory);

        // Add garrison troops that aren't already counted
        // NOTE: Leaders do NOT count as force - they are bait without troops!
        // A general alone has 0 combat value.
        if (ourGarrison > 0) {
            risk.ourMaxForce += ourGarrison;
        }

        // Leaders at territory mark presence (can claim/hold territory) but NOT combat force
        // We track this separately so we know we "can reach" this territory
        bool weHaveLeadersHere = (ourLeadersAtTerritory > 0);

        // Enemy force projection (troops that can reach + troops already there)
        // NOTE: enemyReach[].maxTroopStrength already only counts TROOPS, not leaders
        if (enemyReach.contains(territory)) {
            risk.enemyMaxForce = enemyReach[territory].maxTroopStrength;
            risk.enemyLeaders = enemyReach[territory].leadersWhoCanReach;
        }
        // Add enemy troops already stationed there
        // NOTE: Leaders do NOT count as force - only troops matter for combat
        for (Player *enemy : allPlayers) {
            if (enemy == us) continue;
            int enemyGarrison = countTroopsInTerritory(enemy, territory);
            // Only count actual troops, not leaders (leaders alone are bait)
            if (enemyGarrison > 0) {
                risk.enemyMaxForce += enemyGarrison;
            }
        }

        // Also check for enemy troops in ADJACENT territories - they're a strategic threat
        // even if they can't immediately attack (enemy could bring a leader next turn)
        int adjacentEnemyTroops = 0;
        QStringList neighbors = graph->getNeighbors(territory);
        for (const QString &neighbor : neighbors) {
            if (graph->isSeaTerritory(neighbor)) continue;
            for (Player *enemy : allPlayers) {
                if (enemy == us) continue;
                adjacentEnemyTroops += countTroopsInTerritory(enemy, neighbor);
            }
        }

        // Determine risk level
        // We can "reach" a territory if we have force projection OR if we already have leaders there
        // (leaders can claim territory even without troops, they just can't fight)
        bool weCanReach = (risk.ourMaxForce > 0 || !risk.ourLeaders.isEmpty() || weHaveLeadersHere);
        bool enemyCanReach = (risk.enemyMaxForce > 0 || !risk.enemyLeaders.isEmpty());
        bool enemyAdjacent = (adjacentEnemyTroops > 0);

        // Special case: A lone general (no troops) is HIGH RISK if ANY enemy force can reach
        // Lone generals are tempting targets - enemies will attack to capture them for free
        bool weHaveLoneGeneral = (weHaveLeadersHere && risk.ourMaxForce == 0);

        if (!weCanReach) {
            risk.risk = RiskLevel::Unreachable;
        } else if (!enemyCanReach && !enemyAdjacent) {
            risk.risk = RiskLevel::Safe;
        } else if (weHaveLoneGeneral && (risk.enemyMaxForce > 0 || adjacentEnemyTroops > 0)) {
            // Lone general with any enemy troops in reach = HIGH RISK (they WILL attack)
            risk.risk = RiskLevel::High;
        } else {
            // Compare forces - include adjacent enemy troops as potential threat
            // Weight adjacent troops at 50% since they need a leader to actually attack
            int effectiveEnemyForce = risk.enemyMaxForce + (adjacentEnemyTroops / 2);
            int forceDiff = risk.ourMaxForce - effectiveEnemyForce;

            if (forceDiff >= 3) {
                risk.risk = RiskLevel::Low;      // We have significant advantage
            } else if (forceDiff >= -2) {
                risk.risk = RiskLevel::Medium;   // Roughly equal
            } else {
                risk.risk = RiskLevel::High;     // Enemy has advantage
            }
        }

        results[territory] = risk;
    }

    return results;
}

QString ReachabilityCalculator::generateDefensiveReport(Player *us, const QList<Player*> &allPlayers, MapGraph *graph)
{
    QString report;

    if (!us || !graph) {
        return "Error: Invalid player or graph";
    }

    report += QString("=== DEFENSIVE RISK REPORT FOR PLAYER %1 ===\n\n").arg(us->getId());

    QMap<QString, TerritoryRisk> allRisk = assessAllTerritories(us, allPlayers, graph);

    // Filter to only our territories
    QList<TerritoryRisk> ourTerritories;
    for (const QString &territory : us->getOwnedTerritories()) {
        if (allRisk.contains(territory)) {
            ourTerritories.append(allRisk[territory]);
        }
    }

    // Sort by risk level (highest risk first)
    std::sort(ourTerritories.begin(), ourTerritories.end(),
              [](const TerritoryRisk &a, const TerritoryRisk &b) {
                  return static_cast<int>(a.risk) > static_cast<int>(b.risk);
              });

    // Count by risk level
    int highRisk = 0, mediumRisk = 0, lowRisk = 0, safe = 0;
    for (const TerritoryRisk &tr : ourTerritories) {
        switch (tr.risk) {
            case RiskLevel::High: highRisk++; break;
            case RiskLevel::Medium: mediumRisk++; break;
            case RiskLevel::Low: lowRisk++; break;
            case RiskLevel::Safe: safe++; break;
            default: break;
        }
    }

    report += QString("Summary: %1 HIGH risk, %2 MEDIUM risk, %3 LOW risk, %4 SAFE\n\n")
        .arg(highRisk).arg(mediumRisk).arg(lowRisk).arg(safe);

    // Helper to convert risk level to string
    auto riskToString = [](RiskLevel r) -> QString {
        switch (r) {
            case RiskLevel::Safe: return "SAFE";
            case RiskLevel::Low: return "LOW";
            case RiskLevel::Medium: return "MEDIUM";
            case RiskLevel::High: return "HIGH";
            case RiskLevel::Unreachable: return "UNREACHABLE";
        }
        return "?";
    };

    // Report each territory
    for (const TerritoryRisk &tr : ourTerritories) {
        int value = graph->getValue(tr.territoryName);
        report += QString("%1 [%2] (value: %3)\n")
            .arg(tr.territoryName)
            .arg(riskToString(tr.risk))
            .arg(value);

        report += QString("  Our force: %1 troops, %2 leader(s)\n")
            .arg(tr.ourMaxForce)
            .arg(tr.ourLeaders.size());

        if (tr.enemyMaxForce > 0 || !tr.enemyLeaders.isEmpty()) {
            report += QString("  Enemy threat: %1 troops, %2 leader(s)\n")
                .arg(tr.enemyMaxForce)
                .arg(tr.enemyLeaders.size());
        } else {
            report += "  No enemy threat this turn\n";
        }
        report += "\n";
    }

    return report;
}

QString ReachabilityCalculator::generateOffensiveReport(Player *us, const QList<Player*> &allPlayers, MapGraph *graph)
{
    QString report;

    if (!us || !graph) {
        return "Error: Invalid player or graph";
    }

    report += QString("=== OFFENSIVE OPPORTUNITIES FOR PLAYER %1 ===\n\n").arg(us->getId());

    QMap<QString, TerritoryRisk> allRisk = assessAllTerritories(us, allPlayers, graph);

    // Filter to territories we can reach that we don't own
    QList<TerritoryRisk> targets;
    QStringList ownedList = us->getOwnedTerritories();
    QSet<QString> ourTerritories(ownedList.begin(), ownedList.end());

    for (auto it = allRisk.begin(); it != allRisk.end(); ++it) {
        if (!ourTerritories.contains(it.key()) && it.value().risk != RiskLevel::Unreachable) {
            targets.append(it.value());
        }
    }

    // Sort by: Safe first, then by territory value (highest first)
    std::sort(targets.begin(), targets.end(),
              [graph](const TerritoryRisk &a, const TerritoryRisk &b) {
                  if (a.risk != b.risk) {
                      return static_cast<int>(a.risk) < static_cast<int>(b.risk);  // Safe < Low < Medium < High
                  }
                  return graph->getValue(a.territoryName) > graph->getValue(b.territoryName);
              });

    // Helper to convert risk level to string
    auto riskToString = [](RiskLevel r) -> QString {
        switch (r) {
            case RiskLevel::Safe: return "SAFE";
            case RiskLevel::Low: return "LOW";
            case RiskLevel::Medium: return "MEDIUM";
            case RiskLevel::High: return "HIGH";
            case RiskLevel::Unreachable: return "UNREACHABLE";
        }
        return "?";
    };

    // Count opportunities
    int safeTargets = 0, contestedTargets = 0;
    for (const TerritoryRisk &tr : targets) {
        if (tr.risk == RiskLevel::Safe) safeTargets++;
        else contestedTargets++;
    }

    report += QString("Found %1 reachable targets: %2 uncontested, %3 contested\n\n")
        .arg(targets.size()).arg(safeTargets).arg(contestedTargets);

    // Helper to find territory owner
    auto findOwner = [&allPlayers](const QString &territory) -> QString {
        for (Player *p : allPlayers) {
            if (p->getOwnedTerritories().contains(territory)) {
                return QString("Player %1").arg(p->getId());
            }
        }
        return "Neutral";
    };

    // Report best opportunities first
    report += "--- BEST TARGETS (Safe/Low Risk) ---\n\n";
    for (const TerritoryRisk &tr : targets) {
        if (tr.risk != RiskLevel::Safe && tr.risk != RiskLevel::Low) continue;

        int value = graph->getValue(tr.territoryName);
        QString ownerStr = findOwner(tr.territoryName);

        report += QString("%1 [%2] (value: %3, owner: %4)\n")
            .arg(tr.territoryName)
            .arg(riskToString(tr.risk))
            .arg(value)
            .arg(ownerStr);

        report += QString("  Our force: %1 troops, %2 leader(s)\n")
            .arg(tr.ourMaxForce)
            .arg(tr.ourLeaders.size());

        if (tr.enemyMaxForce > 0) {
            report += QString("  Enemy can reinforce: %1 troops\n").arg(tr.enemyMaxForce);
        }
        report += "\n";
    }

    // Also show risky targets
    report += "--- RISKY TARGETS (Medium/High Risk) ---\n\n";
    for (const TerritoryRisk &tr : targets) {
        if (tr.risk != RiskLevel::Medium && tr.risk != RiskLevel::High) continue;

        int value = graph->getValue(tr.territoryName);
        QString ownerStr = findOwner(tr.territoryName);

        report += QString("%1 [%2] (value: %3, owner: %4)\n")
            .arg(tr.territoryName)
            .arg(riskToString(tr.risk))
            .arg(value)
            .arg(ownerStr);

        report += QString("  Our force: %1 troops vs Enemy: %2 troops\n")
            .arg(tr.ourMaxForce)
            .arg(tr.enemyMaxForce);
        report += "\n";
    }

    return report;
}

QString ReachabilityCalculator::generateRiskDashboard(Player *us, const QList<Player*> &allPlayers, MapGraph *graph)
{
    QString report;

    if (!us || !graph) {
        return "Error: Invalid player or graph";
    }

    report += QString("╔════════════════════════════════════════════════════════════╗\n");
    report += QString("║       STRATEGIC RISK DASHBOARD - PLAYER %1                  ║\n").arg(us->getId());
    report += QString("╚════════════════════════════════════════════════════════════╝\n\n");

    QMap<QString, TerritoryRisk> allRisk = assessAllTerritories(us, allPlayers, graph);
    QStringList ownedList = us->getOwnedTerritories();
    QSet<QString> ourTerritories(ownedList.begin(), ownedList.end());

    // Categorize all assessed territories
    QList<TerritoryRisk> threatened;      // Our territories at risk
    QList<TerritoryRisk> safeExpansion;   // Uncontested expansion
    QList<TerritoryRisk> contested;       // Contested targets

    for (auto it = allRisk.begin(); it != allRisk.end(); ++it) {
        if (ourTerritories.contains(it.key())) {
            if (it.value().risk == RiskLevel::Medium || it.value().risk == RiskLevel::High) {
                threatened.append(it.value());
            }
        } else if (it.value().risk != RiskLevel::Unreachable) {
            if (it.value().risk == RiskLevel::Safe || it.value().risk == RiskLevel::Low) {
                safeExpansion.append(it.value());
            } else {
                contested.append(it.value());
            }
        }
    }

    // Sort by value
    auto sortByValue = [graph](const TerritoryRisk &a, const TerritoryRisk &b) {
        return graph->getValue(a.territoryName) > graph->getValue(b.territoryName);
    };
    std::sort(threatened.begin(), threatened.end(), sortByValue);
    std::sort(safeExpansion.begin(), safeExpansion.end(), sortByValue);
    std::sort(contested.begin(), contested.end(), sortByValue);

    // Helper
    auto riskToString = [](RiskLevel r) -> QString {
        switch (r) {
            case RiskLevel::Safe: return "SAFE";
            case RiskLevel::Low: return "LOW";
            case RiskLevel::Medium: return "MED";
            case RiskLevel::High: return "HIGH";
            default: return "?";
        }
    };

    // Section 1: Threatened territories (DEFEND!)
    report += QString("🛡️  DEFEND (%1 territories at risk)\n").arg(threatened.size());
    report += QString("────────────────────────────────────\n");
    if (threatened.isEmpty()) {
        report += "  All territories secure!\n";
    } else {
        for (const TerritoryRisk &tr : threatened) {
            int value = graph->getValue(tr.territoryName);
            report += QString("  ⚠️  %1 [%2] val:%3 | us:%4 vs enemy:%5\n")
                .arg(tr.territoryName, -15)
                .arg(riskToString(tr.risk))
                .arg(value)
                .arg(tr.ourMaxForce)
                .arg(tr.enemyMaxForce);
        }
    }
    report += "\n";

    // Helper to find territory owner
    auto findOwner = [&allPlayers](const QString &territory) -> QString {
        for (Player *p : allPlayers) {
            if (p->getOwnedTerritories().contains(territory)) {
                return QString("P%1").arg(p->getId());
            }
        }
        return "neutral";
    };

    // Section 2: Safe expansion (ATTACK!)
    report += QString("⚔️  ATTACK - Safe Targets (%1 available)\n").arg(safeExpansion.size());
    report += QString("────────────────────────────────────\n");
    if (safeExpansion.isEmpty()) {
        report += "  No uncontested targets in range\n";
    } else {
        int shown = 0;
        for (const TerritoryRisk &tr : safeExpansion) {
            if (shown++ >= 5) {
                report += QString("  ... and %1 more\n").arg(safeExpansion.size() - 5);
                break;
            }
            int value = graph->getValue(tr.territoryName);
            QString ownerStr = findOwner(tr.territoryName);
            report += QString("  ✓ %1 [%2] val:%3 | %4 troops (%5)\n")
                .arg(tr.territoryName, -15)
                .arg(riskToString(tr.risk))
                .arg(value)
                .arg(tr.ourMaxForce)
                .arg(ownerStr);
        }
    }
    report += "\n";

    // Section 3: Contested zones (CAUTION!)
    report += QString("⚡ CONTESTED - Risky Targets (%1 in range)\n").arg(contested.size());
    report += QString("────────────────────────────────────\n");
    if (contested.isEmpty()) {
        report += "  No contested targets\n";
    } else {
        int shown = 0;
        for (const TerritoryRisk &tr : contested) {
            if (shown++ >= 5) {
                report += QString("  ... and %1 more\n").arg(contested.size() - 5);
                break;
            }
            int value = graph->getValue(tr.territoryName);
            report += QString("  ⚡ %1 [%2] val:%3 | us:%4 vs enemy:%5\n")
                .arg(tr.territoryName, -15)
                .arg(riskToString(tr.risk))
                .arg(value)
                .arg(tr.ourMaxForce)
                .arg(tr.enemyMaxForce);
        }
    }
    report += "\n";

    // Summary stats
    report += QString("────────────────────────────────────\n");
    report += QString("Territories owned: %1 | Reachable targets: %2\n")
        .arg(ourTerritories.size())
        .arg(safeExpansion.size() + contested.size());

    return report;
}

QList<MultiTurnReachInfo> ReachabilityCalculator::getMultiTurnReachability(GamePiece *leader, Player *player, MapGraph *graph, int maxTurns)
{
    QList<MultiTurnReachInfo> results;

    if (!leader || !player || !graph || maxTurns < 1) {
        return results;
    }

    // Only consider leaders with troops - generals without troops can't defend!
    int troops = calculateTroopStrength(leader, player, 0.0);
    if (troops == 0) {
        return results;  // No troops = can't defend = not useful for defense planning
    }

    QString startTerritory = leader->getTerritoryName();
    if (startTerritory.isEmpty()) {
        return results;
    }

    // BFS to find all territories reachable within maxTurns
    // Each turn, a leader can move to adjacent territory (or via road/galley)
    // We track: territory -> (turns to reach, troops that can make it)

    struct PathInfo {
        QString territory;
        int turns;
        int troopsCanBring;
        QString path;
    };

    QMap<QString, PathInfo> bestPath;  // territory -> best way to reach it
    QList<PathInfo> frontier;

    // Start position (turn 0)
    PathInfo start;
    start.territory = startTerritory;
    start.turns = 0;
    start.troopsCanBring = troops;
    start.path = startTerritory;
    bestPath[startTerritory] = start;

    // Seed frontier with turn 1 destinations
    QStringList neighbors = graph->getNeighbors(startTerritory);
    for (const QString &neighbor : neighbors) {
        if (graph->isSeaTerritory(neighbor)) continue;

        PathInfo p;
        p.territory = neighbor;
        p.turns = 1;
        // For 1 turn, infantry/catapults can follow (1 move)
        p.troopsCanBring = troops;  // All troops can move 1
        p.path = startTerritory + " -> " + neighbor;
        frontier.append(p);

        if (!bestPath.contains(neighbor) || bestPath[neighbor].turns > p.turns) {
            bestPath[neighbor] = p;
        }
    }

    // Also check road network (can reach any road-connected city in 1 move)
    QStringList roadConnected = graph->getRoadConnectedTerritories(startTerritory, player);
    for (const QString &roadDest : roadConnected) {
        if (roadDest == startTerritory) continue;

        PathInfo p;
        p.territory = roadDest;
        p.turns = 1;
        p.troopsCanBring = troops;  // All troops can use roads
        p.path = startTerritory + " -(road)-> " + roadDest;

        if (!bestPath.contains(roadDest) || bestPath[roadDest].turns > p.turns) {
            bestPath[roadDest] = p;
            frontier.append(p);
        }
    }

    // Expand frontier for turns 2 and 3
    for (int turn = 2; turn <= maxTurns; turn++) {
        QList<PathInfo> newFrontier;

        for (const PathInfo &current : frontier) {
            if (current.turns != turn - 1) continue;  // Only expand from previous turn

            // Expand to neighbors
            QStringList nextNeighbors = graph->getNeighbors(current.territory);
            for (const QString &next : nextNeighbors) {
                if (graph->isSeaTerritory(next)) continue;

                PathInfo p;
                p.territory = next;
                p.turns = turn;
                // After multiple moves, only cavalry can keep up (infantry moves 1/turn)
                // But we're tracking TURNS not moves, and each turn resets movement
                // So all troops can follow if we take multiple turns
                p.troopsCanBring = current.troopsCanBring;
                p.path = current.path + " -> " + next;

                if (!bestPath.contains(next) || bestPath[next].turns > p.turns ||
                    (bestPath[next].turns == p.turns && bestPath[next].troopsCanBring < p.troopsCanBring)) {
                    bestPath[next] = p;
                    newFrontier.append(p);
                }
            }

            // Also expand via roads from current position
            QStringList roadFromCurrent = graph->getRoadConnectedTerritories(current.territory, player);
            for (const QString &roadDest : roadFromCurrent) {
                if (roadDest == current.territory) continue;

                PathInfo p;
                p.territory = roadDest;
                p.turns = turn;
                p.troopsCanBring = current.troopsCanBring;
                p.path = current.path + " -(road)-> " + roadDest;

                if (!bestPath.contains(roadDest) || bestPath[roadDest].turns > p.turns ||
                    (bestPath[roadDest].turns == p.turns && bestPath[roadDest].troopsCanBring < p.troopsCanBring)) {
                    bestPath[roadDest] = p;
                    newFrontier.append(p);
                }
            }
        }

        frontier.append(newFrontier);
    }

    // Convert bestPath to results (excluding start position)
    for (auto it = bestPath.begin(); it != bestPath.end(); ++it) {
        if (it.key() == startTerritory) continue;  // Don't include starting position

        MultiTurnReachInfo info;
        info.territoryName = it.key();
        info.leader = leader;
        info.turnsToReach = it.value().turns;
        info.troopsCanBring = it.value().troopsCanBring;
        info.pathDescription = it.value().path;

        results.append(info);
    }

    return results;
}

QMap<QString, MultiTurnReachInfo> ReachabilityCalculator::getAllMultiTurnReachability(
    Player *player, const QList<Player*> &allPlayers, MapGraph *graph, int maxTurns)
{
    QMap<QString, MultiTurnReachInfo> results;

    if (!player || !graph) {
        return results;
    }

    // Get all leaders (Caesars + Generals)
    QList<GamePiece*> leaders;
    for (CaesarPiece *caesar : player->getCaesars()) {
        leaders.append(caesar);
    }
    for (GeneralPiece *gen : player->getGenerals()) {
        leaders.append(gen);
    }

    // For each leader, get their multi-turn reachability
    for (GamePiece *leader : leaders) {
        QList<MultiTurnReachInfo> leaderReach = getMultiTurnReachability(leader, player, graph, maxTurns);

        for (const MultiTurnReachInfo &info : leaderReach) {
            // Keep the best option: fewest turns, then most troops
            if (!results.contains(info.territoryName)) {
                results[info.territoryName] = info;
            } else {
                const MultiTurnReachInfo &existing = results[info.territoryName];
                if (info.turnsToReach < existing.turnsToReach ||
                    (info.turnsToReach == existing.turnsToReach && info.troopsCanBring > existing.troopsCanBring)) {
                    results[info.territoryName] = info;
                }
            }
        }
    }

    // Add city info for each territory
    for (auto it = results.begin(); it != results.end(); ++it) {
        for (Player *p : allPlayers) {
            City *city = p->getCityAtTerritory(it.key());
            if (city) {
                it.value().hasCity = true;
                it.value().hasFortifiedCity = city->isFortified();
                break;
            }
        }
    }

    return results;
}
