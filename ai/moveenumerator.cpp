#include "moveenumerator.h"
#include "../player.h"
#include "../mapgraph.h"
#include "../gamepiece.h"
#include <QDebug>
#include <QElapsedTimer>

MoveEnumerator::MoveEnumerator()
{
}

GeneralMoveSet MoveEnumerator::enumerateGeneralMoves(GamePiece *general, Player *player, MapGraph *graph)
{
    GeneralMoveSet result;
    result.general = general;
    result.startTerritory = general->getTerritoryName();

    double movesRemaining = general->getMovesRemaining();

    // If no moves remaining, general can only stay
    if (movesRemaining < 0.5) {
        GeneralMove move;
        move.general = general;
        move.move1.source = result.startTerritory;
        move.move1.sink = result.startTerritory;
        move.move2.source = result.startTerritory;
        move.move2.sink = result.startTerritory;
        result.possibleMoves.append(move);
        return result;
    }

    // Get territories reachable by land movement (no galley)
    QSet<QString> reachable1Land = getReachableIn1MoveLand(result.startTerritory, player, graph);

    // Enumerate land-based moves
    for (const QString &dest1 : reachable1Land) {
        // If only 1 move remaining (0.5 <= moves < 1.5), move2 must be stay
        if (movesRemaining < 1.5) {
            GeneralMove move;
            move.general = general;
            move.move1.source = result.startTerritory;
            move.move1.sink = dest1;
            move.move2.source = dest1;
            move.move2.sink = dest1;  // Must stay for move2
            result.possibleMoves.append(move);
        } else {
            // Full 2 moves available - but only land moves for move2
            QSet<QString> reachable2Land = getReachableIn1MoveLand(dest1, player, graph);

            for (const QString &dest2 : reachable2Land) {
                GeneralMove move;
                move.general = general;
                move.move1.source = result.startTerritory;
                move.move1.sink = dest1;
                move.move2.source = dest1;
                move.move2.sink = dest2;
                result.possibleMoves.append(move);
            }
        }
    }

    // Enumerate galley transport moves
    // Boarding a galley costs one move, but the galley's movement is free.
    // So troops board on move1, and the galley carries them to the destination.
    //
    // move1: home -> Galley_<id> (board the galley - this is the troop's one move)
    // move2: Galley_<id> -> destination (galley carries them - free ride)
    //
    // Each galley can go to multiple destinations, so we enumerate each route.
    for (GalleyPiece *galley : player->getGalleys()) {
        // Galley must be beached at the general's location
        if (galley->getTerritoryName() != result.startTerritory) continue;

        QString galleyId = QString("Galley_%1").arg(galley->getUniqueId());

        // Get galley's possible moves
        GalleyMoveSet galleyMoves = enumerateGalleyMoves(galley, graph);

        for (const GalleyMove &gm : galleyMoves.possibleMoves) {
            QString destination = gm.endingTerritory();

            // Skip stay moves (galley doesn't move)
            if (destination == result.startTerritory) continue;

            // move1: board the galley (home -> Galley_<id>)
            // move2: galley carries to destination (Galley_<id> -> destination)
            GeneralMove move;
            move.general = general;
            move.move1.source = result.startTerritory;
            move.move1.sink = galleyId;
            move.move2.source = galleyId;
            move.move2.sink = destination;
            result.possibleMoves.append(move);
        }
    }

    return result;
}

TroopMoveSet MoveEnumerator::enumerateTroopMoves(GamePiece *troop, Player *player,
                                                   const QMap<QString, QSet<QString>> &move1Destinations,
                                                   const QMap<QString, QSet<QString>> &move2Destinations)
{
    Q_UNUSED(player);

    TroopMoveSet result;
    result.troop = troop;
    result.startTerritory = troop->getTerritoryName();

    double movesRemaining = troop->getMovesRemaining();
    GamePiece::Type troopType = troop->getType();
    bool isCavalry = (troopType == GamePiece::Type::Cavalry);

    // If no moves remaining, troop can only stay
    if (movesRemaining < 0.5) {
        if (isCavalry) {
            CavalryMove cm;
            cm.cavalry = troop;
            cm.transition1.source = result.startTerritory;
            cm.transition1.sink = result.startTerritory;
            cm.transition2.source = result.startTerritory;
            cm.transition2.sink = result.startTerritory;
            result.possibleCavalryMoves.append(cm);
        } else {
            TroopMove tm;
            tm.troop = troop;
            tm.transition.source = result.startTerritory;
            tm.transition.sink = result.startTerritory;
            result.possibleMoves.append(tm);
        }
        return result;
    }

    if (isCavalry) {
        // Cavalry has 2 transitions
        // Get move1 destinations from this territory
        QSet<QString> t1Sinks = move1Destinations.value(result.startTerritory);
        t1Sinks.insert(result.startTerritory);  // Can always stay

        for (const QString &t1Sink : t1Sinks) {
            // Check if boarding a galley
            if (t1Sink.startsWith("Galley_")) {
                // Boarding a galley - cavalry gets free ride to galley's destinations
                // Galley uses both moves, so cavalry ends at galley destination (no second move)
                QSet<QString> galleyDests = move2Destinations.value(t1Sink);
                for (const QString &galleyDest : galleyDests) {
                    CavalryMove cm;
                    cm.cavalry = troop;
                    cm.transition1.source = result.startTerritory;
                    cm.transition1.sink = galleyDest;  // End at galley destination
                    cm.transition2.source = galleyDest;
                    cm.transition2.sink = galleyDest;  // No second move after galley
                    result.possibleCavalryMoves.append(cm);
                }
            } else {
                // Normal land movement
                // If only 1 move remaining, transition2 must be stay
                if (movesRemaining < 1.5) {
                    CavalryMove cm;
                    cm.cavalry = troop;
                    cm.transition1.source = result.startTerritory;
                    cm.transition1.sink = t1Sink;
                    cm.transition2.source = t1Sink;
                    cm.transition2.sink = t1Sink;  // Must stay
                    result.possibleCavalryMoves.append(cm);
                } else {
                    // Full 2 moves available
                    QSet<QString> t2Sinks = move2Destinations.value(t1Sink);
                    t2Sinks.insert(t1Sink);  // Can always stay

                    for (const QString &t2Sink : t2Sinks) {
                        // Skip galley destinations from move2 (can't board galley on second move)
                        if (t2Sink.startsWith("Galley_")) continue;

                        CavalryMove cm;
                        cm.cavalry = troop;
                        cm.transition1.source = result.startTerritory;
                        cm.transition1.sink = t1Sink;
                        cm.transition2.source = t1Sink;
                        cm.transition2.sink = t2Sink;
                        result.possibleCavalryMoves.append(cm);
                    }
                }
            }
        }
    } else {
        // Infantry/Catapult has 1 transition
        // Can ride any move1 or move2 that starts at their territory
        QSet<QString> destinations;
        destinations.insert(result.startTerritory);  // Can always stay

        // Only add movement options if troop has moves remaining
        // (movesRemaining >= 0.5 was already checked above, so we have at least 1 move)

        // Add move1 destinations
        QSet<QString> move1Dests = move1Destinations.value(result.startTerritory);
        for (const QString &dest : move1Dests) {
            if (dest.startsWith("Galley_")) {
                // Boarding a galley - troop gets free ride to galley's destinations
                // The galley's destinations are in move2Destinations[galleyId]
                destinations.unite(move2Destinations.value(dest));
            } else {
                destinations.insert(dest);
            }
        }

        // Add move2 destinations (for riding a general's second move from home)
        QSet<QString> move2Dests = move2Destinations.value(result.startTerritory);
        for (const QString &dest : move2Dests) {
            if (!dest.startsWith("Galley_")) {
                destinations.insert(dest);
            }
        }

        for (const QString &dest : destinations) {
            TroopMove tm;
            tm.troop = troop;
            tm.transition.source = result.startTerritory;
            tm.transition.sink = dest;
            result.possibleMoves.append(tm);
        }
    }

    return result;
}

TurnMoveEnumeration MoveEnumerator::enumerateAllMoves(Player *player,
                                                        const QList<Player*> &allPlayers,
                                                        MapGraph *graph)
{
    QElapsedTimer timer;
    timer.start();

    TurnMoveEnumeration result;
    result.player = player;

    // Step 1: Enumerate all general moves (including Caesar)
    // Caesar
    for (CaesarPiece *caesar : player->getCaesars()) {
        GeneralMoveSet gms = enumerateGeneralMoves(caesar, player, graph);
        result.generalMoveSets.append(gms);
    }

    // Generals
    for (GeneralPiece *general : player->getGenerals()) {
        GeneralMoveSet gms = enumerateGeneralMoves(general, player, graph);
        result.generalMoveSets.append(gms);
    }

    qDebug() << "Step 1 (enumerate generals):" << timer.elapsed() << "ms, total moves:" << result.totalGeneralMoveCount();

    // Step 2: Validate general moves (remove those without valid escorts)
    validateGeneralMoves(result.generalMoveSets, player, allPlayers);

    qDebug() << "Step 2 (validate):" << timer.elapsed() << "ms, moves after validation:" << result.totalGeneralMoveCount();

    // Step 3: Build lookup maps for troop enumeration (optimization)
    // Map from territory -> set of destinations reachable via move1
    // Map from territory -> set of destinations reachable via move2
    QMap<QString, QSet<QString>> move1Destinations;
    QMap<QString, QSet<QString>> move2Destinations;

    for (const GeneralMoveSet &gms : result.generalMoveSets) {
        for (const GeneralMove &gm : gms.possibleMoves) {
            if (!gm.move1.isStay()) {
                move1Destinations[gm.move1.source].insert(gm.move1.sink);
            }
            if (!gm.move2.isStay()) {
                move2Destinations[gm.move2.source].insert(gm.move2.sink);
            }
        }
    }

    qDebug() << "Step 3 (build lookup maps):" << timer.elapsed() << "ms";

    // Step 4: Enumerate troop moves based on validated general moves
    // Infantry
    for (InfantryPiece *infantry : player->getInfantry()) {
        TroopMoveSet tms = enumerateTroopMoves(infantry, player, move1Destinations, move2Destinations);
        result.troopMoveSets.append(tms);
    }

    // Cavalry
    for (CavalryPiece *cavalry : player->getCavalry()) {
        TroopMoveSet tms = enumerateTroopMoves(cavalry, player, move1Destinations, move2Destinations);
        result.troopMoveSets.append(tms);
    }

    // Catapults
    for (CatapultPiece *catapult : player->getCatapults()) {
        TroopMoveSet tms = enumerateTroopMoves(catapult, player, move1Destinations, move2Destinations);
        result.troopMoveSets.append(tms);
    }

    qDebug() << "Step 4 (enumerate troops):" << timer.elapsed() << "ms, troop moves:" << result.totalTroopMoveCount();

    // Step 5: Enumerate galley moves
    for (GalleyPiece *galley : player->getGalleys()) {
        GalleyMoveSet gms = enumerateGalleyMoves(galley, graph);
        result.galleyMoveSets.append(gms);
    }

    qDebug() << "Step 5 (enumerate galleys):" << timer.elapsed() << "ms, galley moves:" << result.totalGalleyMoveCount();

    return result;
}

QSet<QString> MoveEnumerator::getReachableIn1MoveLand(const QString &from, Player *player, MapGraph *graph)
{
    QSet<QString> result;

    // Can always stay in place
    result.insert(from);

    // Add adjacent land territories
    QList<QString> neighbors = graph->getNeighbors(from);
    for (const QString &neighbor : neighbors) {
        if (graph->isLandTerritory(neighbor)) {
            result.insert(neighbor);
        }
    }

    // Add road-connected territories
    QStringList roadConnected = graph->getRoadConnectedTerritories(from, player);
    for (const QString &dest : roadConnected) {
        result.insert(dest);
    }

    return result;
}

QSet<QString> MoveEnumerator::getReachableIn1Move(const QString &from, Player *player, MapGraph *graph)
{
    // Land movement plus galley transport
    QSet<QString> result = getReachableIn1MoveLand(from, player, graph);

    // Add galley-reachable territories
    QSet<QString> galleyReach = getGalleyReachableFrom(from, player, graph);
    result.unite(galleyReach);

    return result;
}

QSet<QString> MoveEnumerator::getGalleyReachableFrom(const QString &from, Player *player, MapGraph *graph)
{
    QSet<QString> result;

    // Check if 'from' is a coastal territory
    QList<QString> adjacentSeas = graph->getAdjacentSeaTerritories(from);
    if (adjacentSeas.isEmpty()) {
        return result;  // Not coastal, no galley transport possible
    }

    // Check each galley owned by the player
    for (GalleyPiece *galley : player->getGalleys()) {
        QString galleyTerritory = galley->getTerritoryName();

        // Galley can pick up from 'from' ONLY if beached at 'from'
        // (Pickup happens while beached, not from sea)

        if (galleyTerritory == from) {
            // Galley transport rules:
            // - Pickup: Galley must be beached at same territory as troops (no move cost)
            // - Move 1: Leave beach → enter adjacent sea zone
            // - Move 2: From sea → beach on adjacent land to drop off
            //
            // So in one turn, galley can only transport to lands adjacent to the ONE sea zone
            // it enters from the beach. Cannot go 2 seas and drop off in same turn.

            // Get seas adjacent to the pickup territory
            QList<QString> adjacentSeasFromPickup = graph->getAdjacentSeaTerritories(from);

            // For each sea the galley can enter (move 1), find lands it can beach on (move 2)
            for (const QString &sea : adjacentSeasFromPickup) {
                QList<QString> seaNeighbors = graph->getNeighbors(sea);
                for (const QString &neighbor : seaNeighbors) {
                    if (graph->isLandTerritory(neighbor) && neighbor != from) {
                        result.insert(neighbor);
                    }
                }
            }
        }
    }

    return result;
}

GalleyMoveSet MoveEnumerator::enumerateGalleyMoves(GamePiece *galley, MapGraph *graph)
{
    GalleyMoveSet result;
    result.galley = galley;
    result.startTerritory = galley->getTerritoryName();

    double movesRemaining = galley->getMovesRemaining();

    // If no moves remaining, galley can only stay
    if (movesRemaining < 0.5) {
        GalleyMove gm;
        gm.galley = galley;
        gm.transition1.source = result.startTerritory;
        gm.transition1.sink = result.startTerritory;
        gm.transition2.source = result.startTerritory;
        gm.transition2.sink = result.startTerritory;
        result.possibleMoves.append(gm);
        return result;
    }

    // Galley movement rules:
    // - Can move up to 2 sea zones per turn
    // - If beached, first move is to go to adjacent sea
    // - Second move can be to adjacent sea zone OR beach on adjacent land
    // - Galleys cannot move from land to land directly

    bool startAtSea = graph->isSeaTerritory(result.startTerritory);
    result.isBeached = !startAtSea;  // Galley is beached if it starts on land

    // Always can stay in place
    {
        GalleyMove gm;
        gm.galley = galley;
        gm.transition1.source = result.startTerritory;
        gm.transition1.sink = result.startTerritory;
        gm.transition2.source = result.startTerritory;
        gm.transition2.sink = result.startTerritory;
        result.possibleMoves.append(gm);
    }

    if (startAtSea) {
        // Already at sea - can move to adjacent seas (move 1)
        QList<QString> adjacentSeas = graph->getNeighbors(result.startTerritory);

        for (const QString &sea1 : adjacentSeas) {
            if (!graph->isSeaTerritory(sea1)) continue;

            // Move 1 only (stay at sea1 for move 2)
            if (movesRemaining >= 0.5) {
                GalleyMove gm;
                gm.galley = galley;
                gm.transition1.source = result.startTerritory;
                gm.transition1.sink = sea1;
                gm.transition2.source = sea1;
                gm.transition2.sink = sea1;
                result.possibleMoves.append(gm);
            }

            // Move 2: from sea1, go to adjacent sea or beach on land
            if (movesRemaining >= 1.5) {
                QList<QString> neighbors2 = graph->getNeighbors(sea1);
                for (const QString &dest2 : neighbors2) {
                    if (dest2 == result.startTerritory) continue;  // Don't go back to start via same path
                    if (graph->isSeaTerritory(dest2) || graph->isLandTerritory(dest2)) {
                        GalleyMove gm;
                        gm.galley = galley;
                        gm.transition1.source = result.startTerritory;
                        gm.transition1.sink = sea1;
                        gm.transition2.source = sea1;
                        gm.transition2.sink = dest2;
                        result.possibleMoves.append(gm);
                    }
                }
            }
        }

        // Can also beach directly on adjacent land (1 move)
        if (movesRemaining >= 0.5) {
            QList<QString> neighbors = graph->getNeighbors(result.startTerritory);
            for (const QString &land : neighbors) {
                if (graph->isLandTerritory(land)) {
                    GalleyMove gm;
                    gm.galley = galley;
                    gm.transition1.source = result.startTerritory;
                    gm.transition1.sink = land;
                    gm.transition2.source = land;
                    gm.transition2.sink = land;
                    result.possibleMoves.append(gm);
                }
            }
        }
    } else {
        // Beached on land - first move must be to the sea zone the galley came from
        // (galley remembers which sea zone it beached from via lastSeaZone)
        GalleyPiece *galleyPiece = qobject_cast<GalleyPiece*>(galley);
        QString sea1 = galleyPiece ? galleyPiece->getLastSeaZone() : QString();

        if (sea1.isEmpty()) {
            // No last sea zone recorded - galley can't move
            return result;
        }

        // Move 1 only (stay at sea1 for move 2)
        if (movesRemaining >= 0.5) {
            GalleyMove gm;
            gm.galley = galley;
            gm.transition1.source = result.startTerritory;
            gm.transition1.sink = sea1;
            gm.transition2.source = sea1;
            gm.transition2.sink = sea1;
            result.possibleMoves.append(gm);
        }

        // Move 2: from sea1, can EITHER go to adjacent sea OR beach on adjacent land
        // (landing costs a move, so can't go sea->sea->land in one turn)
        if (movesRemaining >= 1.5) {
            QList<QString> neighbors2 = graph->getNeighbors(sea1);
            for (const QString &dest2 : neighbors2) {
                if (dest2 == result.startTerritory) continue;  // Don't return to starting land
                if (graph->isSeaTerritory(dest2) || graph->isLandTerritory(dest2)) {
                    GalleyMove gm;
                    gm.galley = galley;
                    gm.transition1.source = result.startTerritory;
                    gm.transition1.sink = sea1;
                    gm.transition2.source = sea1;
                    gm.transition2.sink = dest2;
                    result.possibleMoves.append(gm);
                }
            }
        }
    }

    return result;
}

bool MoveEnumerator::isOwnTerritory(const QString &territory, Player *player) const
{
    return player->ownsTerritory(territory);
}

bool MoveEnumerator::isUnclaimedTerritory(const QString &territory, const QList<Player*> &allPlayers) const
{
    for (Player *p : allPlayers) {
        if (p->ownsTerritory(territory)) {
            return false;
        }
    }
    return true;
}

bool MoveEnumerator::isEnemyTerritory(const QString &territory, Player *player, const QList<Player*> &allPlayers) const
{
    for (Player *p : allPlayers) {
        if (p != player && p->ownsTerritory(territory)) {
            return true;
        }
    }
    return false;
}

bool MoveEnumerator::requiresEscort(const QString &territory, Player *player, const QList<Player*> &allPlayers) const
{
    // Escort required if territory is unclaimed or enemy-owned
    return !isOwnTerritory(territory, player);
}

void MoveEnumerator::validateGeneralMoves(QList<GeneralMoveSet> &generalMoveSets,
                                           Player *player,
                                           const QList<Player*> &allPlayers)
{
    // Step 1: Validate move2 transitions
    // For each general move, if move2.sink requires escort, check if escort is available
    // Skip validation for galley moves (move1.sink starts with "Galley_")
    for (GeneralMoveSet &gms : generalMoveSets) {
        for (int i = 0; i < gms.possibleMoves.size(); ++i) {
            GeneralMove &gm = gms.possibleMoves[i];

            // Skip galley moves - troops board with the general, no separate escort needed
            if (gm.move1.sink.startsWith("Galley_")) continue;

            if (!gm.move2.isStay() && requiresEscort(gm.move2.sink, player, allPlayers)) {
                // Check if escort is available for move2
                if (!hasMove2Escort(gm.move1, gm.move2, player)) {
                    // No escort - replace move2 with stay
                    gm.move2.sink = gm.move2.source;
                }
            }
        }
    }

    // Remove duplicate moves that may have been created
    for (GeneralMoveSet &gms : generalMoveSets) {
        QList<GeneralMove> uniqueMoves;
        for (const GeneralMove &gm : gms.possibleMoves) {
            bool isDuplicate = false;
            for (const GeneralMove &existing : uniqueMoves) {
                if (existing.move1 == gm.move1 && existing.move2 == gm.move2) {
                    isDuplicate = true;
                    break;
                }
            }
            if (!isDuplicate) {
                uniqueMoves.append(gm);
            }
        }
        gms.possibleMoves = uniqueMoves;
    }

    // Step 2: Validate move1 transitions
    // For each general move, if move1.sink requires escort, check if escort is available
    // Skip validation for galley moves (move1.sink starts with "Galley_")
    for (GeneralMoveSet &gms : generalMoveSets) {
        QList<GeneralMove> validMoves;
        for (const GeneralMove &gm : gms.possibleMoves) {
            // Skip galley moves - troops board with the general, no separate escort needed
            if (gm.move1.sink.startsWith("Galley_")) {
                validMoves.append(gm);
                continue;
            }

            if (!gm.move1.isStay() && requiresEscort(gm.move1.sink, player, allPlayers)) {
                // Check if escort is available for move1
                if (hasMove1Escort(gm.move1, player)) {
                    validMoves.append(gm);
                }
                // else: no escort, move is invalid - don't add
            } else {
                // No escort needed or staying - move is valid
                validMoves.append(gm);
            }
        }
        gms.possibleMoves = validMoves;
    }
}

bool MoveEnumerator::hasMove1Escort(const Transition &move1, Player *player) const
{
    // Check if any troop at move1.source can ride this transition
    QString source = move1.source;

    // Check infantry
    for (InfantryPiece *infantry : player->getInfantry()) {
        if (infantry->getTerritoryName() == source) {
            return true;
        }
    }

    // Check cavalry
    for (CavalryPiece *cavalry : player->getCavalry()) {
        if (cavalry->getTerritoryName() == source) {
            return true;
        }
    }

    // Check catapults
    for (CatapultPiece *catapult : player->getCatapults()) {
        if (catapult->getTerritoryName() == source) {
            return true;
        }
    }

    return false;
}

bool MoveEnumerator::hasMove2Escort(const Transition &move1, const Transition &move2, Player *player) const
{
    // Escort for move2 can come from:
    // 1. Cavalry that rode move1 (started at move1.source, now at move2.source)
    // 2. Any troop originally at move2.source

    QString move1Source = move1.source;
    QString move2Source = move2.source;  // = move1.sink

    // Check cavalry that could ride move1 and then move2
    // (cavalry at move1.source that follows move1 ends up at move2.source)
    if (!move1.isStay()) {
        for (CavalryPiece *cavalry : player->getCavalry()) {
            if (cavalry->getTerritoryName() == move1Source) {
                // This cavalry could ride move1, ending at move2.source
                // It can then escort move2
                return true;
            }
        }
    }

    // Check troops originally at move2.source (intermediate territory)
    // Infantry
    for (InfantryPiece *infantry : player->getInfantry()) {
        if (infantry->getTerritoryName() == move2Source) {
            return true;
        }
    }

    // Cavalry already at move2.source
    for (CavalryPiece *cavalry : player->getCavalry()) {
        if (cavalry->getTerritoryName() == move2Source) {
            return true;
        }
    }

    // Catapults
    for (CatapultPiece *catapult : player->getCatapults()) {
        if (catapult->getTerritoryName() == move2Source) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// 2-Turn Projection Implementation
// ============================================================================

GeneralMoveSet2Turn MoveEnumerator::enumerateGeneralMoves2Turn(GamePiece *general, Player *player, MapGraph *graph)
{
    GeneralMoveSet2Turn result;
    result.general = general;
    result.startTerritory = general->getTerritoryName();

    QString start = result.startTerritory;
    double movesRemaining = general->getMovesRemaining();

    // For 2-turn projection, we have up to 4 moves total (2 per turn)
    // Current turn: slots 1-2 use movesRemaining
    // Next turn: slots 3-4 assume full 2 moves

    // Determine how many slots are available this turn based on movesRemaining
    int slotsThisTurn = 0;
    if (movesRemaining >= 1.5) slotsThisTurn = 2;
    else if (movesRemaining >= 0.5) slotsThisTurn = 1;

    // Helper lambda to generate stay transition
    auto stayAt = [](const QString &territory) {
        Transition t;
        t.source = territory;
        t.sink = territory;
        return t;
    };

    // If no moves this turn, slot1 and slot2 are stays
    if (slotsThisTurn == 0) {
        // Turn 1: stay, stay. Turn 2: full moves
        QSet<QString> reachable3 = getReachableIn1Move(start, player, graph);
        for (const QString &dest3 : reachable3) {
            QSet<QString> reachable4 = getReachableIn1Move(dest3, player, graph);
            for (const QString &dest4 : reachable4) {
                GeneralMove2Turn move;
                move.general = general;
                move.slot1 = stayAt(start);
                move.slot2 = stayAt(start);
                move.slot3.source = start;
                move.slot3.sink = dest3;
                move.slot4.source = dest3;
                move.slot4.sink = dest4;
                result.possibleMoves.append(move);
            }
        }
        return result;
    }

    // Get reachable territories for slot1
    QSet<QString> reachable1 = getReachableIn1Move(start, player, graph);

    for (const QString &dest1 : reachable1) {
        // If only 1 move this turn, slot2 must be stay
        if (slotsThisTurn == 1) {
            // dest1 is where general ends turn 1
            QSet<QString> reachable3 = getReachableIn1Move(dest1, player, graph);
            for (const QString &dest3 : reachable3) {
                QSet<QString> reachable4 = getReachableIn1Move(dest3, player, graph);
                for (const QString &dest4 : reachable4) {
                    GeneralMove2Turn move;
                    move.general = general;
                    move.slot1.source = start;
                    move.slot1.sink = dest1;
                    move.slot2 = stayAt(dest1);
                    move.slot3.source = dest1;
                    move.slot3.sink = dest3;
                    move.slot4.source = dest3;
                    move.slot4.sink = dest4;
                    result.possibleMoves.append(move);
                }
            }
        } else {
            // Full 2 moves this turn
            QSet<QString> reachable2 = getReachableIn1Move(dest1, player, graph);

            for (const QString &dest2 : reachable2) {
                // dest2 is where general ends turn 1
                QSet<QString> reachable3 = getReachableIn1Move(dest2, player, graph);

                for (const QString &dest3 : reachable3) {
                    QSet<QString> reachable4 = getReachableIn1Move(dest3, player, graph);

                    for (const QString &dest4 : reachable4) {
                        GeneralMove2Turn move;
                        move.general = general;
                        move.slot1.source = start;
                        move.slot1.sink = dest1;
                        move.slot2.source = dest1;
                        move.slot2.sink = dest2;
                        move.slot3.source = dest2;
                        move.slot3.sink = dest3;
                        move.slot4.source = dest3;
                        move.slot4.sink = dest4;
                        result.possibleMoves.append(move);
                    }
                }
            }
        }
    }

    return result;
}

TroopMoveSet2Turn MoveEnumerator::enumerateTroopMoves2Turn(
    GamePiece *troop,
    const QMap<QString, QSet<QString>> slotLookups[4],
    const QMap<QString, QSet<QString>> &endOfTurn1Map)
{
    TroopMoveSet2Turn result;
    result.troop = troop;
    result.startTerritory = troop->getTerritoryName();

    QString start = result.startTerritory;
    double movesRemaining = troop->getMovesRemaining();

    // Infantry/catapult can ride one slot per turn
    // Turn 1: ride slot1 OR slot2 (whichever starts at their territory) - if has move remaining
    // Turn 2: ride slot3 OR slot4 (whichever starts at their end-of-turn-1 position) - always has move

    // Build turn1 destinations (slot1 or slot2 starting at 'start')
    QSet<QString> turn1Destinations;
    turn1Destinations.insert(start);  // Can always stay

    // Only add movement options if troop has moves remaining this turn
    if (movesRemaining >= 0.5) {
        turn1Destinations.unite(slotLookups[0].value(start));  // slot1 destinations
        turn1Destinations.unite(slotLookups[1].value(start));  // slot2 destinations
    }

    for (const QString &t1Dest : turn1Destinations) {
        // For turn 2, troop needs to be at a territory where a general ends turn 1
        // OR stay at their turn1 destination if no general passes through

        // Build turn2 destinations - turn 2 always has full moves
        QSet<QString> turn2Destinations;
        turn2Destinations.insert(t1Dest);  // Can always stay

        // Check if any general ends turn 1 at t1Dest
        // If so, we can ride their slot3 or slot4
        if (endOfTurn1Map.contains(t1Dest)) {
            // Generals end turn 1 here, so slot3 starts here
            turn2Destinations.unite(slotLookups[2].value(t1Dest));  // slot3 destinations
            turn2Destinations.unite(slotLookups[3].value(t1Dest));  // slot4 destinations
        }

        for (const QString &t2Dest : turn2Destinations) {
            TroopMove2Turn move;
            move.troop = troop;
            move.turn1.source = start;
            move.turn1.sink = t1Dest;
            move.turn2.source = t1Dest;
            move.turn2.sink = t2Dest;

            // Avoid duplicates
            bool isDuplicate = false;
            for (const TroopMove2Turn &existing : result.possibleMoves) {
                if (existing == move) {
                    isDuplicate = true;
                    break;
                }
            }
            if (!isDuplicate) {
                result.possibleMoves.append(move);
            }
        }
    }

    return result;
}

CavalryMoveSet2Turn MoveEnumerator::enumerateCavalryMoves2Turn(
    GamePiece *cavalry,
    const QMap<QString, QSet<QString>> slotLookups[4],
    const QMap<QString, QSet<QString>> &endOfTurn1Map)
{
    CavalryMoveSet2Turn result;
    result.cavalry = cavalry;
    result.startTerritory = cavalry->getTerritoryName();

    QString start = result.startTerritory;
    double movesRemaining = cavalry->getMovesRemaining();

    // Cavalry can ride one slot per move, 2 moves per turn = 4 slots total
    // Current turn: slots 1-2 use movesRemaining
    // Next turn: slots 3-4 assume full 2 moves

    int slotsThisTurn = 0;
    if (movesRemaining >= 1.5) slotsThisTurn = 2;
    else if (movesRemaining >= 0.5) slotsThisTurn = 1;

    // Slot 1 destinations
    QSet<QString> slot1Destinations;
    slot1Destinations.insert(start);  // Can always stay
    if (slotsThisTurn >= 1) {
        slot1Destinations.unite(slotLookups[0].value(start));
    }

    for (const QString &s1Dest : slot1Destinations) {
        // Slot 2 destinations
        QSet<QString> slot2Destinations;
        slot2Destinations.insert(s1Dest);  // Can always stay
        if (slotsThisTurn >= 2) {
            slot2Destinations.unite(slotLookups[1].value(s1Dest));
        }

        for (const QString &s2Dest : slot2Destinations) {
            // s2Dest is where cavalry ends turn 1
            // For turn 2, cavalry needs to be at a territory where a general ends turn 1
            // to have access to slot3/slot4 moves

            // Slot 3 destinations - turn 2 always has full moves
            QSet<QString> slot3Destinations;
            slot3Destinations.insert(s2Dest);  // Can always stay

            // Check if any general ends turn 1 at s2Dest
            if (endOfTurn1Map.contains(s2Dest)) {
                slot3Destinations.unite(slotLookups[2].value(s2Dest));
            }

            for (const QString &s3Dest : slot3Destinations) {
                // Slot 4 destinations
                QSet<QString> slot4Destinations;
                slot4Destinations.insert(s3Dest);  // Can always stay

                // slot4 starts where slot3 ends
                slot4Destinations.unite(slotLookups[3].value(s3Dest));

                for (const QString &s4Dest : slot4Destinations) {
                    CavalryMove2Turn move;
                    move.cavalry = cavalry;
                    move.slot1.source = start;
                    move.slot1.sink = s1Dest;
                    move.slot2.source = s1Dest;
                    move.slot2.sink = s2Dest;
                    move.slot3.source = s2Dest;
                    move.slot3.sink = s3Dest;
                    move.slot4.source = s3Dest;
                    move.slot4.sink = s4Dest;

                    result.possibleMoves.append(move);
                }
            }
        }
    }

    return result;
}

TwoTurnMoveEnumeration MoveEnumerator::enumerateAllMoves2Turn(
    Player *player,
    const QList<Player*> &allPlayers,
    MapGraph *graph)
{
    Q_UNUSED(allPlayers);  // For future escort validation

    QElapsedTimer timer;
    timer.start();

    TwoTurnMoveEnumeration result;
    result.player = player;

    // Step 1: Enumerate all general 2-turn moves
    // Caesar
    for (CaesarPiece *caesar : player->getCaesars()) {
        GeneralMoveSet2Turn gms = enumerateGeneralMoves2Turn(caesar, player, graph);
        result.generalMoveSets.append(gms);
    }

    // Generals
    for (GeneralPiece *general : player->getGenerals()) {
        GeneralMoveSet2Turn gms = enumerateGeneralMoves2Turn(general, player, graph);
        result.generalMoveSets.append(gms);
    }

    qDebug() << "2-Turn Step 1 (enumerate generals):" << timer.elapsed() << "ms, total moves:" << result.totalGeneralMoveCount();

    // Step 2: Build lookup maps for each slot
    // slotLookups[i] maps territory -> set of destinations via that slot
    QMap<QString, QSet<QString>> slotLookups[4];

    // Also build endOfTurn1Map: territory -> set of generals that end turn 1 there
    QMap<QString, QSet<QString>> endOfTurn1Map;

    for (const GeneralMoveSet2Turn &gms : result.generalMoveSets) {
        for (const GeneralMove2Turn &gm : gms.possibleMoves) {
            // Slot 1: source -> sink
            if (!gm.slot1.isStay()) {
                slotLookups[0][gm.slot1.source].insert(gm.slot1.sink);
            }
            // Slot 2: source -> sink
            if (!gm.slot2.isStay()) {
                slotLookups[1][gm.slot2.source].insert(gm.slot2.sink);
            }
            // Slot 3: source -> sink
            if (!gm.slot3.isStay()) {
                slotLookups[2][gm.slot3.source].insert(gm.slot3.sink);
            }
            // Slot 4: source -> sink
            if (!gm.slot4.isStay()) {
                slotLookups[3][gm.slot4.source].insert(gm.slot4.sink);
            }

            // Track where generals end turn 1 (slot2.sink = slot3.source)
            endOfTurn1Map[gm.endOfTurn1()].insert(gm.slot3.source);
        }
    }

    qDebug() << "2-Turn Step 2 (build lookup maps):" << timer.elapsed() << "ms";

    // Step 3: Enumerate troop moves (infantry and catapults)
    for (InfantryPiece *infantry : player->getInfantry()) {
        TroopMoveSet2Turn tms = enumerateTroopMoves2Turn(infantry, slotLookups, endOfTurn1Map);
        result.troopMoveSets.append(tms);
    }

    for (CatapultPiece *catapult : player->getCatapults()) {
        TroopMoveSet2Turn tms = enumerateTroopMoves2Turn(catapult, slotLookups, endOfTurn1Map);
        result.troopMoveSets.append(tms);
    }

    qDebug() << "2-Turn Step 3 (enumerate infantry/catapults):" << timer.elapsed() << "ms, troop moves:" << result.totalTroopMoveCount();

    // Step 4: Enumerate cavalry moves
    for (CavalryPiece *cavalry : player->getCavalry()) {
        CavalryMoveSet2Turn cms = enumerateCavalryMoves2Turn(cavalry, slotLookups, endOfTurn1Map);
        result.cavalryMoveSets.append(cms);
    }

    qDebug() << "2-Turn Step 4 (enumerate cavalry):" << timer.elapsed() << "ms, cavalry moves:" << result.totalCavalryMoveCount();

    // Step 5: Enumerate galley moves
    for (GalleyPiece *galley : player->getGalleys()) {
        GalleyMoveSet2Turn gms = enumerateGalleyMoves2Turn(galley, graph);
        result.galleyMoveSets.append(gms);
    }

    qDebug() << "2-Turn Step 5 (enumerate galleys):" << timer.elapsed() << "ms, galley moves:" << result.totalGalleyMoveCount();

    return result;
}

GalleyMoveSet2Turn MoveEnumerator::enumerateGalleyMoves2Turn(GamePiece *galley, MapGraph *graph)
{
    GalleyMoveSet2Turn result;
    result.galley = galley;
    result.startTerritory = galley->getTerritoryName();
    result.isBeached = !graph->isSeaTerritory(result.startTerritory);

    // Galley movement rules (per turn):
    // - Can move up to 2 sea zones per turn
    // - If beached, first move is to adjacent sea, second move to adjacent sea or beach
    // - If at sea, can move to adjacent sea (move 1), then adjacent sea or beach (move 2)
    // - Galleys cannot move from land to land directly

    // For 2-turn projection, we enumerate turn 1 moves, then turn 2 moves from end positions
    // Use the 1-turn enumeration logic twice

    // First, get all possible end-of-turn-1 positions using 1-turn galley enumeration
    GalleyMoveSet turn1Moves = enumerateGalleyMoves(galley, graph);

    // For each turn 1 ending position, enumerate turn 2 moves
    for (const GalleyMove &t1 : turn1Moves.possibleMoves) {
        QString endOfTurn1 = t1.endingTerritory();

        // Create a temporary "galley" at the end-of-turn-1 position to enumerate turn 2 moves
        // We simulate this by getting reachable positions from endOfTurn1

        bool turn2AtSea = graph->isSeaTerritory(endOfTurn1);

        // Turn 2: stay in place
        {
            GalleyMove2Turn gm;
            gm.galley = galley;
            gm.slot1 = t1.transition1;
            gm.slot2 = t1.transition2;
            gm.slot3.source = endOfTurn1;
            gm.slot3.sink = endOfTurn1;
            gm.slot4.source = endOfTurn1;
            gm.slot4.sink = endOfTurn1;
            result.possibleMoves.append(gm);
        }

        if (turn2AtSea) {
            // At sea at start of turn 2
            QList<QString> adjacentSeas = graph->getNeighbors(endOfTurn1);

            for (const QString &sea3 : adjacentSeas) {
                if (!graph->isSeaTerritory(sea3)) continue;

                // Move 3 only (stay at sea3 for move 4)
                {
                    GalleyMove2Turn gm;
                    gm.galley = galley;
                    gm.slot1 = t1.transition1;
                    gm.slot2 = t1.transition2;
                    gm.slot3.source = endOfTurn1;
                    gm.slot3.sink = sea3;
                    gm.slot4.source = sea3;
                    gm.slot4.sink = sea3;
                    result.possibleMoves.append(gm);
                }

                // Move 4: from sea3, go to adjacent sea or beach on land
                QList<QString> neighbors4 = graph->getNeighbors(sea3);
                for (const QString &dest4 : neighbors4) {
                    if (dest4 == endOfTurn1) continue;
                    if (graph->isSeaTerritory(dest4) || graph->isLandTerritory(dest4)) {
                        GalleyMove2Turn gm;
                        gm.galley = galley;
                        gm.slot1 = t1.transition1;
                        gm.slot2 = t1.transition2;
                        gm.slot3.source = endOfTurn1;
                        gm.slot3.sink = sea3;
                        gm.slot4.source = sea3;
                        gm.slot4.sink = dest4;
                        result.possibleMoves.append(gm);
                    }
                }
            }

            // Can also beach directly on adjacent land (move 3 only)
            QList<QString> neighbors = graph->getNeighbors(endOfTurn1);
            for (const QString &land : neighbors) {
                if (graph->isLandTerritory(land)) {
                    GalleyMove2Turn gm;
                    gm.galley = galley;
                    gm.slot1 = t1.transition1;
                    gm.slot2 = t1.transition2;
                    gm.slot3.source = endOfTurn1;
                    gm.slot3.sink = land;
                    gm.slot4.source = land;
                    gm.slot4.sink = land;
                    result.possibleMoves.append(gm);
                }
            }
        } else {
            // Beached on land at start of turn 2
            QList<QString> adjacentSeas = graph->getAdjacentSeaTerritories(endOfTurn1);

            for (const QString &sea3 : adjacentSeas) {
                // Move 3 only (stay at sea3 for move 4)
                {
                    GalleyMove2Turn gm;
                    gm.galley = galley;
                    gm.slot1 = t1.transition1;
                    gm.slot2 = t1.transition2;
                    gm.slot3.source = endOfTurn1;
                    gm.slot3.sink = sea3;
                    gm.slot4.source = sea3;
                    gm.slot4.sink = sea3;
                    result.possibleMoves.append(gm);
                }

                // Move 4: from sea3, go to adjacent sea or beach on different land
                QList<QString> neighbors4 = graph->getNeighbors(sea3);
                for (const QString &dest4 : neighbors4) {
                    if (dest4 == endOfTurn1) continue;
                    if (graph->isSeaTerritory(dest4) || graph->isLandTerritory(dest4)) {
                        GalleyMove2Turn gm;
                        gm.galley = galley;
                        gm.slot1 = t1.transition1;
                        gm.slot2 = t1.transition2;
                        gm.slot3.source = endOfTurn1;
                        gm.slot3.sink = sea3;
                        gm.slot4.source = sea3;
                        gm.slot4.sink = dest4;
                        result.possibleMoves.append(gm);
                    }
                }
            }
        }
    }

    return result;
}

// ============================================================================
// Reachability Breakdown Implementation
// ============================================================================

QMap<QString, ReachabilityBreakdown> TurnMoveEnumeration::getReachabilityBreakdown() const
{
    QMap<QString, ReachabilityBreakdown> result;

    for (const TroopMoveSet &tms : troopMoveSets) {
        if (!tms.troop) continue;

        GamePiece::Type type = tms.troop->getType();

        QSet<QString> reachable;
        if (tms.isCavalry()) {
            for (const CavalryMove &cm : tms.possibleCavalryMoves) {
                reachable.insert(cm.endingTerritory());
            }
        } else {
            for (const TroopMove &tm : tms.possibleMoves) {
                reachable.insert(tm.endingTerritory());
            }
        }

        for (const QString &territory : reachable) {
            switch (type) {
                case GamePiece::Type::Infantry:
                    result[territory].infantry++;
                    break;
                case GamePiece::Type::Cavalry:
                    result[territory].cavalry++;
                    break;
                case GamePiece::Type::Catapult:
                    result[territory].catapults++;
                    break;
                default:
                    break;
            }
        }
    }

    // Count galleys - only beached galleys can carry NEW troops
    for (const GalleyMoveSet &gms : galleyMoveSets) {
        if (!gms.galley) continue;
        if (!gms.isBeached) continue;  // Skip galleys at sea (can't load troops)

        QSet<QString> reachable;
        for (const GalleyMove &gm : gms.possibleMoves) {
            reachable.insert(gm.endingTerritory());
        }

        for (const QString &territory : reachable) {
            result[territory].galleys++;
        }
    }

    // Count troops already aboard galleys at sea
    // These troops have isOnGalley() == true and their leader is on the galley
    // They can reach wherever the galley can land
    if (player) {
        for (const GalleyMoveSet &gms : galleyMoveSets) {
            if (!gms.galley) continue;
            if (gms.isBeached) continue;  // Only process galleys at sea

            GalleyPiece *galley = qobject_cast<GalleyPiece*>(gms.galley);
            if (!galley) continue;
            if (!galley->hasLeaderAboard()) continue;  // No troops without a leader

            // Find the leader aboard this galley
            int leaderId = galley->getLeaderAboard();
            GamePiece *leader = player->getPieceByUniqueId(leaderId);
            if (!leader) continue;

            // Get the leader's legion (troops aboard)
            QList<int> legion;
            if (CaesarPiece *caesar = qobject_cast<CaesarPiece*>(leader)) {
                legion = caesar->getLegion();
            } else if (GeneralPiece *general = qobject_cast<GeneralPiece*>(leader)) {
                legion = general->getLegion();
            }

            if (legion.isEmpty()) continue;

            // Get all land territories this galley can reach
            QSet<QString> landDestinations;
            for (const GalleyMove &gm : gms.possibleMoves) {
                QString dest = gm.endingTerritory();
                // Only count land destinations (not sea zones)
                if (!dest.startsWith("Mare")) {
                    landDestinations.insert(dest);
                }
            }

            // Count troops in the legion
            int infantryCount = 0;
            int cavalryCount = 0;
            int catapultCount = 0;

            for (int troopId : legion) {
                GamePiece *troop = player->getPieceByUniqueId(troopId);
                if (!troop) continue;

                switch (troop->getType()) {
                    case GamePiece::Type::Infantry:
                        infantryCount++;
                        break;
                    case GamePiece::Type::Cavalry:
                        cavalryCount++;
                        break;
                    case GamePiece::Type::Catapult:
                        catapultCount++;
                        break;
                    default:
                        break;
                }
            }

            // Add these troops to each reachable land destination
            for (const QString &territory : landDestinations) {
                result[territory].infantry += infantryCount;
                result[territory].cavalry += cavalryCount;
                result[territory].catapults += catapultCount;
            }
        }
    }

    return result;
}

QMap<QString, ReachabilityBreakdown> TwoTurnMoveEnumeration::getReachabilityBreakdown() const
{
    QMap<QString, ReachabilityBreakdown> result;

    // Count infantry/catapults from troopMoveSets
    for (const TroopMoveSet2Turn &tms : troopMoveSets) {
        if (!tms.troop) continue;

        GamePiece::Type type = tms.troop->getType();

        QSet<QString> reachable;
        for (const TroopMove2Turn &tm : tms.possibleMoves) {
            reachable.insert(tm.endingTerritory());
        }

        for (const QString &territory : reachable) {
            switch (type) {
                case GamePiece::Type::Infantry:
                    result[territory].infantry++;
                    break;
                case GamePiece::Type::Catapult:
                    result[territory].catapults++;
                    break;
                default:
                    break;
            }
        }
    }

    // Count cavalry from cavalryMoveSets
    for (const CavalryMoveSet2Turn &cms : cavalryMoveSets) {
        QSet<QString> reachable;
        for (const CavalryMove2Turn &cm : cms.possibleMoves) {
            reachable.insert(cm.endingTerritory());
        }

        for (const QString &territory : reachable) {
            result[territory].cavalry++;
        }
    }

    // Count galleys from galleyMoveSets - only beached galleys can carry NEW troops
    for (const GalleyMoveSet2Turn &gms : galleyMoveSets) {
        if (!gms.galley) continue;
        if (!gms.isBeached) continue;  // Skip galleys at sea (can't load troops)

        QSet<QString> reachable;
        for (const GalleyMove2Turn &gm : gms.possibleMoves) {
            reachable.insert(gm.endingTerritory());
        }

        for (const QString &territory : reachable) {
            result[territory].galleys++;
        }
    }

    // Count troops already aboard galleys at sea
    // These troops have isOnGalley() == true and their leader is on the galley
    // They can reach wherever the galley can land within 2 turns
    if (player) {
        for (const GalleyMoveSet2Turn &gms : galleyMoveSets) {
            if (!gms.galley) continue;
            if (gms.isBeached) continue;  // Only process galleys at sea

            GalleyPiece *galley = qobject_cast<GalleyPiece*>(gms.galley);
            if (!galley) continue;
            if (!galley->hasLeaderAboard()) continue;  // No troops without a leader

            // Find the leader aboard this galley
            int leaderId = galley->getLeaderAboard();
            GamePiece *leader = player->getPieceByUniqueId(leaderId);
            if (!leader) continue;

            // Get the leader's legion (troops aboard)
            QList<int> legion;
            if (CaesarPiece *caesar = qobject_cast<CaesarPiece*>(leader)) {
                legion = caesar->getLegion();
            } else if (GeneralPiece *general = qobject_cast<GeneralPiece*>(leader)) {
                legion = general->getLegion();
            }

            if (legion.isEmpty()) continue;

            // Get all land territories this galley can reach in 2 turns
            QSet<QString> landDestinations;
            for (const GalleyMove2Turn &gm : gms.possibleMoves) {
                QString dest = gm.endingTerritory();
                // Only count land destinations (not sea zones)
                if (!dest.startsWith("Mare")) {
                    landDestinations.insert(dest);
                }
            }

            // Count troops in the legion
            int infantryCount = 0;
            int cavalryCount = 0;
            int catapultCount = 0;

            for (int troopId : legion) {
                GamePiece *troop = player->getPieceByUniqueId(troopId);
                if (!troop) continue;

                switch (troop->getType()) {
                    case GamePiece::Type::Infantry:
                        infantryCount++;
                        break;
                    case GamePiece::Type::Cavalry:
                        cavalryCount++;
                        break;
                    case GamePiece::Type::Catapult:
                        catapultCount++;
                        break;
                    default:
                        break;
                }
            }

            // Add these troops to each reachable land destination
            for (const QString &territory : landDestinations) {
                result[territory].infantry += infantryCount;
                result[territory].cavalry += cavalryCount;
                result[territory].catapults += catapultCount;
            }
        }
    }

    return result;
}

QMap<QString, int> TurnMoveEnumeration::getSeaZoneTroopProjection() const
{
    // With the new approach, galley boarding is a transition like any other.
    // General moves include: home -> Galley_<id> -> destination
    // Troops ride these transitions, so getReachabilityBreakdown() already
    // includes sea zones as valid destinations.
    //
    // This function is now deprecated - use getReachabilityBreakdown() instead.
    // Kept for backwards compatibility.

    QMap<QString, int> result;
    QMap<QString, ReachabilityBreakdown> breakdown = getReachabilityBreakdown();

    for (auto it = breakdown.constBegin(); it != breakdown.constEnd(); ++it) {
        result[it.key()] = it.value().total();
    }

    return result;
}

QMap<QString, int> TwoTurnMoveEnumeration::getSeaZoneTroopProjection() const
{
    // With the new approach, galley boarding is a transition like any other.
    // This function is now deprecated - use getReachabilityBreakdown() instead.
    // Kept for backwards compatibility.

    QMap<QString, int> result;
    QMap<QString, ReachabilityBreakdown> breakdown = getReachabilityBreakdown();

    for (auto it = breakdown.constBegin(); it != breakdown.constEnd(); ++it) {
        result[it.key()] = it.value().total();
    }

    return result;
}
