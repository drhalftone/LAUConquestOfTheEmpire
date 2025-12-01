#ifndef MOVEENUMERATOR_H
#define MOVEENUMERATOR_H

#include <QString>
#include <QList>
#include <QSet>
#include <QMap>

// Forward declarations
class GamePiece;
class Player;
class MapGraph;

/**
 * @brief A single transition from one territory to another (or same for staying)
 */
struct Transition {
    QString source;
    QString sink;

    bool isStay() const { return source == sink; }

    bool operator==(const Transition &other) const {
        return source == other.source && sink == other.sink;
    }
};

/**
 * @brief A complete move for one general (2 transitions)
 */
struct GeneralMove {
    GamePiece *general = nullptr;
    Transition move1;
    Transition move2;

    QString startingTerritory() const { return move1.source; }
    QString endingTerritory() const { return move2.sink; }
    int totalMoves() const {
        int count = 0;
        if (!move1.isStay()) count++;
        if (!move2.isStay()) count++;
        return count;
    }

    bool operator==(const GeneralMove &other) const {
        return general == other.general && move1 == other.move1 && move2 == other.move2;
    }
};

/**
 * @brief All possible moves for a single general
 */
struct GeneralMoveSet {
    GamePiece *general = nullptr;
    QString startTerritory;
    QList<GeneralMove> possibleMoves;

    int count() const { return possibleMoves.size(); }
};

/**
 * @brief A troop's possible move (1 transition for infantry/catapult)
 */
struct TroopMove {
    GamePiece *troop = nullptr;
    Transition transition;

    QString startingTerritory() const { return transition.source; }
    QString endingTerritory() const { return transition.sink; }
};

/**
 * @brief A cavalry's possible move (2 transitions)
 */
struct CavalryMove {
    GamePiece *cavalry = nullptr;
    Transition transition1;
    Transition transition2;

    QString startingTerritory() const { return transition1.source; }
    QString endingTerritory() const { return transition2.sink; }
};

/**
 * @brief All possible moves for a single troop
 */
struct TroopMoveSet {
    GamePiece *troop = nullptr;
    QString startTerritory;
    QList<TroopMove> possibleMoves;           // For infantry/catapults (1 transition)
    QList<CavalryMove> possibleCavalryMoves;  // For cavalry (2 transitions)

    bool isCavalry() const { return !possibleCavalryMoves.isEmpty(); }
    int count() const {
        return isCavalry() ? possibleCavalryMoves.size() : possibleMoves.size();
    }
};

/**
 * @brief A galley's possible move (2 sea transitions)
 */
struct GalleyMove {
    GamePiece *galley = nullptr;
    Transition transition1;  // First sea move (or stay)
    Transition transition2;  // Second sea move (or stay)

    QString startingTerritory() const { return transition1.source; }
    QString endingTerritory() const { return transition2.sink; }
};

/**
 * @brief All possible moves for a single galley
 */
struct GalleyMoveSet {
    GamePiece *galley = nullptr;
    QString startTerritory;
    QList<GalleyMove> possibleMoves;
    bool isBeached = false;  // True if galley starts on land (can load troops)

    int count() const { return possibleMoves.size(); }
};

/**
 * @brief Breakdown of reachability by unit type for a territory
 */
struct ReachabilityBreakdown {
    int infantry = 0;
    int cavalry = 0;
    int catapults = 0;
    int galleys = 0;

    int total() const { return infantry + cavalry + catapults; }
    int totalWithGalleys() const { return infantry + cavalry + catapults + galleys; }
};

/**
 * @brief Complete enumeration of all possible moves for a player's turn
 */
struct TurnMoveEnumeration {
    Player *player = nullptr;
    QList<GeneralMoveSet> generalMoveSets;  // One per general (includes Caesar)
    QList<TroopMoveSet> troopMoveSets;      // One per troop
    QList<GalleyMoveSet> galleyMoveSets;    // One per galley

    int totalGenerals() const { return generalMoveSets.size(); }
    int totalTroops() const { return troopMoveSets.size(); }
    int totalGalleys() const { return galleyMoveSets.size(); }

    int totalGeneralMoveCount() const {
        int count = 0;
        for (const auto &gms : generalMoveSets) {
            count += gms.count();
        }
        return count;
    }

    int totalTroopMoveCount() const {
        int count = 0;
        for (const auto &tms : troopMoveSets) {
            count += tms.count();
        }
        return count;
    }

    int totalGalleyMoveCount() const {
        int count = 0;
        for (const auto &gms : galleyMoveSets) {
            count += gms.count();
        }
        return count;
    }

    /**
     * @brief Get max troops that can reach each territory
     * @return Map of territory name -> max troop count that can end up there
     */
    QMap<QString, int> getMaxReachabilityMap() const {
        QMap<QString, int> result;

        // For each territory, count max troops that could end there
        // We need to find, for each territory, what's the maximum number
        // of troops that could simultaneously end up there

        // Simple approach: for each troop, count which territories they can reach
        // This gives an upper bound (not accounting for mutual exclusion)
        for (const TroopMoveSet &tms : troopMoveSets) {
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
                result[territory] = result.value(territory, 0) + 1;
            }
        }

        return result;
    }

    /**
     * @brief Get reachability breakdown by unit type for each territory
     * @return Map of territory name -> ReachabilityBreakdown
     */
    QMap<QString, ReachabilityBreakdown> getReachabilityBreakdown() const;

    /**
     * @brief Get troops that could be at sea (on galleys) for each sea zone
     * For each sea zone a galley can reach, count the troops at the galley's
     * starting (beached) location that could be aboard.
     * @return Map of sea zone name -> troop count that could be aboard galleys there
     */
    QMap<QString, int> getSeaZoneTroopProjection() const;
};

// ============================================================================
// 2-Turn Projection Structures
// ============================================================================

/**
 * @brief A general's 2-turn move (4 transitions: 2 per turn)
 */
struct GeneralMove2Turn {
    GamePiece *general = nullptr;
    Transition slot1;  // Turn 1, move 1
    Transition slot2;  // Turn 1, move 2
    Transition slot3;  // Turn 2, move 1
    Transition slot4;  // Turn 2, move 2

    QString startingTerritory() const { return slot1.source; }
    QString endOfTurn1() const { return slot2.sink; }
    QString endingTerritory() const { return slot4.sink; }

    bool operator==(const GeneralMove2Turn &other) const {
        return general == other.general &&
               slot1 == other.slot1 && slot2 == other.slot2 &&
               slot3 == other.slot3 && slot4 == other.slot4;
    }
};

/**
 * @brief All possible 2-turn moves for a single general
 */
struct GeneralMoveSet2Turn {
    GamePiece *general = nullptr;
    QString startTerritory;
    QList<GeneralMove2Turn> possibleMoves;

    int count() const { return possibleMoves.size(); }
};

/**
 * @brief Infantry/catapult 2-turn move (2 transitions: 1 per turn)
 */
struct TroopMove2Turn {
    GamePiece *troop = nullptr;
    Transition turn1;  // Can ride ONE slot in turn 1 (slot1 or slot2)
    Transition turn2;  // Can ride ONE slot in turn 2 (slot3 or slot4)

    QString startingTerritory() const { return turn1.source; }
    QString endOfTurn1() const { return turn1.sink; }
    QString endingTerritory() const { return turn2.sink; }

    bool operator==(const TroopMove2Turn &other) const {
        return troop == other.troop && turn1 == other.turn1 && turn2 == other.turn2;
    }
};

/**
 * @brief All possible 2-turn moves for infantry/catapult
 */
struct TroopMoveSet2Turn {
    GamePiece *troop = nullptr;
    QString startTerritory;
    QList<TroopMove2Turn> possibleMoves;

    int count() const { return possibleMoves.size(); }
};

/**
 * @brief Cavalry 2-turn move (4 transitions: 2 per turn)
 */
struct CavalryMove2Turn {
    GamePiece *cavalry = nullptr;
    Transition slot1;  // Turn 1, move 1
    Transition slot2;  // Turn 1, move 2
    Transition slot3;  // Turn 2, move 1
    Transition slot4;  // Turn 2, move 2

    QString startingTerritory() const { return slot1.source; }
    QString endOfTurn1() const { return slot2.sink; }
    QString endingTerritory() const { return slot4.sink; }

    bool operator==(const CavalryMove2Turn &other) const {
        return cavalry == other.cavalry &&
               slot1 == other.slot1 && slot2 == other.slot2 &&
               slot3 == other.slot3 && slot4 == other.slot4;
    }
};

/**
 * @brief All possible 2-turn moves for cavalry
 */
struct CavalryMoveSet2Turn {
    GamePiece *cavalry = nullptr;
    QString startTerritory;
    QList<CavalryMove2Turn> possibleMoves;

    int count() const { return possibleMoves.size(); }
};

/**
 * @brief Galley 2-turn move (4 transitions: 2 per turn)
 */
struct GalleyMove2Turn {
    GamePiece *galley = nullptr;
    Transition slot1;  // Turn 1, move 1
    Transition slot2;  // Turn 1, move 2
    Transition slot3;  // Turn 2, move 1
    Transition slot4;  // Turn 2, move 2

    QString startingTerritory() const { return slot1.source; }
    QString endOfTurn1() const { return slot2.sink; }
    QString endingTerritory() const { return slot4.sink; }

    bool operator==(const GalleyMove2Turn &other) const {
        return galley == other.galley &&
               slot1 == other.slot1 && slot2 == other.slot2 &&
               slot3 == other.slot3 && slot4 == other.slot4;
    }
};

/**
 * @brief All possible 2-turn moves for galley
 */
struct GalleyMoveSet2Turn {
    GamePiece *galley = nullptr;
    QString startTerritory;
    QList<GalleyMove2Turn> possibleMoves;
    bool isBeached = false;  // True if galley starts on land (can load troops)

    int count() const { return possibleMoves.size(); }
};

/**
 * @brief Complete 2-turn enumeration for a player
 */
struct TwoTurnMoveEnumeration {
    Player *player = nullptr;
    QList<GeneralMoveSet2Turn> generalMoveSets;
    QList<TroopMoveSet2Turn> troopMoveSets;      // Infantry and catapults
    QList<CavalryMoveSet2Turn> cavalryMoveSets;
    QList<GalleyMoveSet2Turn> galleyMoveSets;    // Galleys

    int totalGenerals() const { return generalMoveSets.size(); }
    int totalTroops() const { return troopMoveSets.size(); }
    int totalCavalry() const { return cavalryMoveSets.size(); }
    int totalGalleys() const { return galleyMoveSets.size(); }

    int totalGeneralMoveCount() const {
        int count = 0;
        for (const auto &gms : generalMoveSets) {
            count += gms.count();
        }
        return count;
    }

    int totalTroopMoveCount() const {
        int count = 0;
        for (const auto &tms : troopMoveSets) {
            count += tms.count();
        }
        return count;
    }

    int totalCavalryMoveCount() const {
        int count = 0;
        for (const auto &cms : cavalryMoveSets) {
            count += cms.count();
        }
        return count;
    }

    int totalGalleyMoveCount() const {
        int count = 0;
        for (const auto &gms : galleyMoveSets) {
            count += gms.count();
        }
        return count;
    }

    /**
     * @brief Get max troops that can reach each territory in 2 turns
     * @return Map of territory name -> max troop count that can end up there
     */
    QMap<QString, int> getMaxReachabilityMap() const {
        QMap<QString, int> result;

        // Count infantry/catapults
        for (const TroopMoveSet2Turn &tms : troopMoveSets) {
            QSet<QString> reachable;
            for (const TroopMove2Turn &tm : tms.possibleMoves) {
                reachable.insert(tm.endingTerritory());
            }
            for (const QString &territory : reachable) {
                result[territory] = result.value(territory, 0) + 1;
            }
        }

        // Count cavalry
        for (const CavalryMoveSet2Turn &cms : cavalryMoveSets) {
            QSet<QString> reachable;
            for (const CavalryMove2Turn &cm : cms.possibleMoves) {
                reachable.insert(cm.endingTerritory());
            }
            for (const QString &territory : reachable) {
                result[territory] = result.value(territory, 0) + 1;
            }
        }

        return result;
    }

    /**
     * @brief Get reachability breakdown by unit type for each territory
     * @return Map of territory name -> ReachabilityBreakdown
     */
    QMap<QString, ReachabilityBreakdown> getReachabilityBreakdown() const;

    /**
     * @brief Get troops that could be at sea (on galleys) for each sea zone
     * For each sea zone a galley can reach, count the troops at the galley's
     * starting (beached) location that could be aboard.
     * @return Map of sea zone name -> troop count that could be aboard galleys there
     */
    QMap<QString, int> getSeaZoneTroopProjection() const;
};

/**
 * @brief Enumerates all possible moves for a player's turn
 *
 * This class generates all legal move combinations for generals and troops,
 * taking into account:
 * - Land adjacency
 * - Road networks (city-to-city instant travel)
 * - Galley transport
 * - Troop movement constraints (infantry/catapults: 1 move, cavalry: 2 moves)
 * - Escort requirements (generals need troops to enter unclaimed/enemy territory)
 */
class MoveEnumerator
{
public:
    MoveEnumerator();

    /**
     * @brief Enumerate all possible moves for a single general
     * @param general The general (or Caesar) to enumerate moves for
     * @param player The player who owns the general
     * @param graph The map graph
     * @return GeneralMoveSet with all possible (move1, move2) pairs
     */
    GeneralMoveSet enumerateGeneralMoves(
        GamePiece *general,
        Player *player,
        MapGraph *graph
    );

    /**
     * @brief Enumerate all possible moves for a single troop
     * @param troop The troop (infantry, cavalry, or catapult)
     * @param player The player who owns the troop
     * @param move1Destinations Lookup map: territory -> destinations via move1
     * @param move2Destinations Lookup map: territory -> destinations via move2
     * @return TroopMoveSet with all possible moves
     */
    TroopMoveSet enumerateTroopMoves(
        GamePiece *troop,
        Player *player,
        const QMap<QString, QSet<QString>> &move1Destinations,
        const QMap<QString, QSet<QString>> &move2Destinations
    );

    /**
     * @brief Enumerate all possible moves for a single galley
     * @param galley The galley piece
     * @param graph The map graph
     * @return GalleyMoveSet with all possible (transition1, transition2) pairs
     */
    GalleyMoveSet enumerateGalleyMoves(
        GamePiece *galley,
        MapGraph *graph
    );

    /**
     * @brief Enumerate all possible moves for all of a player's pieces
     * @param player The player
     * @param allPlayers All players (for territory ownership checks)
     * @param graph The map graph
     * @return TurnMoveEnumeration containing all pieces' move options
     */
    TurnMoveEnumeration enumerateAllMoves(
        Player *player,
        const QList<Player*> &allPlayers,
        MapGraph *graph
    );

    /**
     * @brief Get territories reachable in exactly 1 move from a territory
     * Considers: land adjacency, galley transport, road network
     * @param from Starting territory
     * @param player The player (for road/galley access)
     * @param graph The map graph
     * @return Set of reachable territory names (includes staying in place)
     */
    QSet<QString> getReachableIn1Move(
        const QString &from,
        Player *player,
        MapGraph *graph
    );

    /**
     * @brief Get territories reachable in exactly 1 move by land only (no galley)
     * Considers: land adjacency, road network
     * @param from Starting territory
     * @param player The player (for road access)
     * @param graph The map graph
     * @return Set of reachable territory names (includes staying in place)
     */
    QSet<QString> getReachableIn1MoveLand(
        const QString &from,
        Player *player,
        MapGraph *graph
    );

    // ========================================================================
    // 2-Turn Projection Methods
    // ========================================================================

    /**
     * @brief Enumerate all possible 2-turn moves for a single general
     * @param general The general (or Caesar)
     * @param player The player who owns the general
     * @param graph The map graph
     * @return GeneralMoveSet2Turn with all possible 4-slot combinations
     */
    GeneralMoveSet2Turn enumerateGeneralMoves2Turn(
        GamePiece *general,
        Player *player,
        MapGraph *graph
    );

    /**
     * @brief Enumerate all possible 2-turn moves for infantry/catapult
     * @param troop The infantry or catapult piece
     * @param slotLookups Lookup maps for each slot (territory -> destinations)
     * @param endOfTurn1Map Map of territory -> set of generals' end-of-turn-1 positions
     * @return TroopMoveSet2Turn with all possible (turn1, turn2) combinations
     */
    TroopMoveSet2Turn enumerateTroopMoves2Turn(
        GamePiece *troop,
        const QMap<QString, QSet<QString>> slotLookups[4],
        const QMap<QString, QSet<QString>> &endOfTurn1Map
    );

    /**
     * @brief Enumerate all possible 2-turn moves for cavalry
     * @param cavalry The cavalry piece
     * @param slotLookups Lookup maps for each slot (territory -> destinations)
     * @param endOfTurn1Map Map of territory -> set of generals' end-of-turn-1 positions
     * @return CavalryMoveSet2Turn with all possible 4-slot combinations
     */
    CavalryMoveSet2Turn enumerateCavalryMoves2Turn(
        GamePiece *cavalry,
        const QMap<QString, QSet<QString>> slotLookups[4],
        const QMap<QString, QSet<QString>> &endOfTurn1Map
    );

    /**
     * @brief Enumerate all possible 2-turn moves for a single galley
     * @param galley The galley piece
     * @param graph The map graph
     * @return GalleyMoveSet2Turn with all possible 4-slot combinations
     */
    GalleyMoveSet2Turn enumerateGalleyMoves2Turn(
        GamePiece *galley,
        MapGraph *graph
    );

    /**
     * @brief Enumerate all possible 2-turn moves for all pieces
     * @param player The player
     * @param allPlayers All players (for territory ownership checks)
     * @param graph The map graph
     * @return TwoTurnMoveEnumeration containing all pieces' 2-turn options
     */
    TwoTurnMoveEnumeration enumerateAllMoves2Turn(
        Player *player,
        const QList<Player*> &allPlayers,
        MapGraph *graph
    );

private:
    /**
     * @brief Get territories reachable via galley transport
     * @param from Starting land territory
     * @param player The player
     * @param graph The map graph
     * @return Set of coastal territories reachable via player's galleys
     */
    QSet<QString> getGalleyReachableFrom(
        const QString &from,
        Player *player,
        MapGraph *graph
    );

    /**
     * @brief Check if a territory is owned by the player
     */
    bool isOwnTerritory(const QString &territory, Player *player) const;

    /**
     * @brief Check if a territory is unclaimed (no player owns it)
     */
    bool isUnclaimedTerritory(const QString &territory, const QList<Player*> &allPlayers) const;

    /**
     * @brief Check if a territory is enemy-owned
     */
    bool isEnemyTerritory(const QString &territory, Player *player, const QList<Player*> &allPlayers) const;

    /**
     * @brief Check if a general move requires escort (entering unclaimed/enemy)
     */
    bool requiresEscort(const QString &territory, Player *player, const QList<Player*> &allPlayers) const;

    /**
     * @brief Validate general moves and remove those without valid escorts
     * @param generalMoveSets All general move sets (will be modified)
     * @param player The player
     * @param allPlayers All players
     */
    void validateGeneralMoves(
        QList<GeneralMoveSet> &generalMoveSets,
        Player *player,
        const QList<Player*> &allPlayers
    );

    /**
     * @brief Check if any troop can escort a general's move1
     * @param move1 The first transition
     * @param player The player
     * @return true if at least one troop at move1.source can ride
     */
    bool hasMove1Escort(const Transition &move1, Player *player) const;

    /**
     * @brief Check if any troop can escort a general's move2
     * @param move1 The first transition (cavalry may have ridden this)
     * @param move2 The second transition
     * @param player The player
     * @return true if escort available (cavalry from move1 or troop at move2.source)
     */
    bool hasMove2Escort(const Transition &move1, const Transition &move2, Player *player) const;
};

#endif // MOVEENUMERATOR_H
