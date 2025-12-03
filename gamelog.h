/*********************************************************************************
 * Copyright (c) 2025, Dr. Daniel L. Lau - All rights reserved.
 *********************************************************************************/

#ifndef GAMELOG_H
#define GAMELOG_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>

/**
 * @brief Singleton class for logging game events to a file
 *
 * Logs all significant game actions that would normally require user interaction:
 * - Movement (generals moving with troops)
 * - Purchases (cities, troops, galleys, fortifications)
 * - Combat (attacks, retreats, casualties)
 * - Turn events (start, end, taxes)
 */
class GameLog
{
public:
    static GameLog& instance();

    // Initialize logging for a new game
    void startNewGame(const QString &gameDescription = "");

    // Close the current log file
    void endGame();

    // Log different types of events
    void logTurnStart(const QString &playerName, int turnNumber);
    void logTurnEnd(const QString &playerName, int taxesCollected, int wallet);

    void logMovement(const QString &playerName, const QString &leaderName,
                     const QString &fromTerritory, const QString &toTerritory,
                     int troopsInLegion);

    void logPurchase(const QString &playerName, const QString &itemType,
                     const QString &location, int cost);

    void logCombatStart(const QString &territory, const QString &attacker,
                        const QString &defender);
    void logCombatRoll(const QString &playerName, const QString &unitType,
                       int dieRoll, bool isHit, const QString &targetType = "");
    void logCombatCasualty(const QString &playerName, const QString &unitType,
                           const QString &territory);
    void logCombatRetreat(const QString &playerName, const QString &fromTerritory,
                          const QString &toTerritory);
    void logCombatEnd(const QString &territory, const QString &winner);

    void logCityDestroyed(const QString &playerName, const QString &territory,
                          const QString &reason);

    void logGeneral(const QString &playerName, const QString &message);

    // Get the current log file path
    QString getLogFilePath() const { return m_logFilePath; }

private:
    GameLog();
    ~GameLog();

    // Prevent copying
    GameLog(const GameLog&) = delete;
    GameLog& operator=(const GameLog&) = delete;

    void write(const QString &category, const QString &message);
    QString timestamp() const;

    QFile m_logFile;
    QTextStream m_stream;
    QString m_logFilePath;
    int m_currentTurn = 0;
    QString m_currentPlayer;
};

// Convenience macro for quick logging
#define GAME_LOG GameLog::instance()

#endif // GAMELOG_H
