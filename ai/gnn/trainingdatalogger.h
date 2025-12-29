#ifndef TRAININGDATALOGGER_H
#define TRAININGDATALOGGER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QDir>
#include <QJsonArray>

class Player;
class MapGraph;
class ReachabilityCalculator;
class GameStateSnapshot;

/**
 * @brief Logs game states for GNN training data collection
 *
 * Automatically captures snapshots during gameplay and saves them
 * in a format suitable for training the risk assessment GNN.
 */
class TrainingDataLogger
{
public:
    TrainingDataLogger();
    ~TrainingDataLogger();

    // === Configuration ===

    /**
     * @brief Set output directory for training data
     * @param dir Directory path (will be created if doesn't exist)
     */
    void setOutputDirectory(const QString &dir);
    QString getOutputDirectory() const { return m_outputDir; }

    /**
     * @brief Enable/disable logging
     */
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    /**
     * @brief Set game ID for this session
     * @param gameId Unique identifier for this game (used in filenames)
     */
    void setGameId(const QString &gameId) { m_gameId = gameId; }
    QString getGameId() const { return m_gameId; }

    // === Logging Operations ===

    /**
     * @brief Start a new game session
     * @param gameId Unique identifier for this game
     * @param playerIds List of player IDs (e.g., ['A', 'B', 'C'])
     *
     * Call this at game start to initialize a new logging session.
     */
    void startGame(const QString &gameId, const QList<QChar> &playerIds);

    /**
     * @brief Log a game state snapshot
     * @param currentPlayer The player whose perspective to capture
     * @param allPlayers All players in the game
     * @param graph The map graph
     * @param reachCalc Reachability calculator (optional but recommended)
     * @param turnNumber Current turn number
     * @param phase Game phase description (e.g., "movement", "combat", "purchase")
     *
     * Call this at key points during gameplay to capture state.
     */
    void logSnapshot(Player *currentPlayer,
                     const QList<Player*> &allPlayers,
                     MapGraph *graph,
                     ReachabilityCalculator *reachCalc,
                     int turnNumber,
                     const QString &phase = "turn_start");

    /**
     * @brief Log a combat outcome for post-hoc labeling
     * @param territory Territory where combat occurred
     * @param attackerId Attacker player ID
     * @param defenderId Defender player ID
     * @param attackerWon True if attacker won
     * @param attackerCasualties Troops lost by attacker
     * @param defenderCasualties Troops lost by defender
     */
    void logCombatOutcome(const QString &territory,
                          QChar attackerId,
                          QChar defenderId,
                          bool attackerWon,
                          int attackerCasualties,
                          int defenderCasualties);

    /**
     * @brief Log a territory capture event
     * @param territory Territory that was captured
     * @param previousOwner Previous owner (or '\0' if neutral)
     * @param newOwner New owner
     * @param turnNumber Turn when capture occurred
     */
    void logTerritoryCapture(const QString &territory,
                             QChar previousOwner,
                             QChar newOwner,
                             int turnNumber);

    /**
     * @brief End the current game session
     * @param winnerId Player ID of winner (or '\0' if draw/incomplete)
     *
     * Call this at game end to finalize logging and write summary.
     */
    void endGame(QChar winnerId);

    // === Data Access ===

    /**
     * @brief Get number of snapshots logged this session
     */
    int getSnapshotCount() const { return m_snapshotCount; }

    /**
     * @brief Get number of combat outcomes logged this session
     */
    int getCombatCount() const { return m_combatOutcomes.size(); }

    /**
     * @brief Flush any pending data to disk
     */
    void flush();

private:
    // Generate filename for snapshot
    QString generateSnapshotFilename(int turnNumber, const QString &phase, QChar playerId);

    // Write combat outcomes to file
    void writeCombatOutcomes();

    // Write territory captures to file
    void writeTerritoryCaptures();

    // Write game summary
    void writeGameSummary(QChar winnerId);

    // Configuration
    QString m_outputDir;
    QString m_gameId;
    bool m_enabled = false;

    // Session state
    QList<QChar> m_playerIds;
    int m_snapshotCount = 0;
    bool m_gameInProgress = false;

    // Logged events
    struct CombatOutcome {
        int snapshotId;
        QString territory;
        QChar attackerId;
        QChar defenderId;
        bool attackerWon;
        int attackerCasualties;
        int defenderCasualties;
    };
    QList<CombatOutcome> m_combatOutcomes;

    struct TerritoryCapture {
        QString territory;
        QChar previousOwner;
        QChar newOwner;
        int turnNumber;
        int snapshotId;
    };
    QList<TerritoryCapture> m_territoryCaptures;
};

#endif // TRAININGDATALOGGER_H
