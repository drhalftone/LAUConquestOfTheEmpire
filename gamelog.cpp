/*********************************************************************************
 * Copyright (c) 2025, Dr. Daniel L. Lau - All rights reserved.
 *********************************************************************************/

#include "gamelog.h"
#include <QStandardPaths>
#include <QCoreApplication>

GameLog::GameLog()
{
}

GameLog::~GameLog()
{
    endGame();
}

GameLog& GameLog::instance()
{
    static GameLog instance;
    return instance;
}

void GameLog::startNewGame(const QString &gameDescription)
{
    // Close any existing log
    endGame();

    // Create log directory in app data location
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (logDir.isEmpty()) {
        logDir = QDir::currentPath();
    }
    QDir().mkpath(logDir);

    // Create filename with timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    m_logFilePath = logDir + "/game_" + timestamp + ".log";

    m_logFile.setFileName(m_logFilePath);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_stream.setDevice(&m_logFile);

        // Build version ID - increment this when making code changes
        const int BUILD_VERSION = 1010;

        // Write header
        m_stream << "===============================================================================\n";
        m_stream << "CONQUEST OF THE EMPIRE - GAME LOG\n";
        m_stream << "===============================================================================\n";
        m_stream << "Build: " << BUILD_VERSION << "\n";
        m_stream << "Started: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
        if (!gameDescription.isEmpty()) {
            m_stream << "Description: " << gameDescription << "\n";
        }
        m_stream << "Log file: " << m_logFilePath << "\n";
        m_stream << "===============================================================================\n\n";
        m_stream.flush();

        qDebug() << "Game log started:" << m_logFilePath;
    } else {
        qWarning() << "Failed to create game log file:" << m_logFilePath;
    }

    m_currentTurn = 0;
    m_currentPlayer.clear();
}

void GameLog::endGame()
{
    if (m_logFile.isOpen()) {
        m_stream << "\n===============================================================================\n";
        m_stream << "GAME ENDED: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
        m_stream << "===============================================================================\n";
        m_stream.flush();
        m_logFile.close();
    }
}

QString GameLog::timestamp() const
{
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

void GameLog::write(const QString &category, const QString &message)
{
    if (!m_logFile.isOpen()) return;

    m_stream << "[" << timestamp() << "] [" << category << "] " << message << "\n";
    m_stream.flush();
}

void GameLog::logTurnStart(const QString &playerName, int turnNumber)
{
    if (!m_logFile.isOpen()) return;

    m_currentTurn = turnNumber;
    m_currentPlayer = playerName;

    m_stream << "\n--- TURN " << turnNumber << ": " << playerName << " ---\n";
    m_stream.flush();
}

void GameLog::logTurnEnd(const QString &playerName, int taxesCollected, int wallet)
{
    write("TURN", QString("%1 turn ended. Taxes: %2, Wallet: %3")
          .arg(playerName).arg(taxesCollected).arg(wallet));
}

void GameLog::logMovement(const QString &playerName, const QString &leaderName,
                          const QString &fromTerritory, const QString &toTerritory,
                          int troopsInLegion)
{
    write("MOVE", QString("%1: %2 moved from %3 to %4 with %5 troops")
          .arg(playerName).arg(leaderName).arg(fromTerritory).arg(toTerritory).arg(troopsInLegion));
}

void GameLog::logPurchase(const QString &playerName, const QString &itemType,
                          const QString &location, int cost)
{
    if (location.isEmpty()) {
        write("BUY", QString("%1: Purchased %2 for %3 talents")
              .arg(playerName).arg(itemType).arg(cost));
    } else {
        write("BUY", QString("%1: Purchased %2 at %3 for %4 talents")
              .arg(playerName).arg(itemType).arg(location).arg(cost));
    }
}

void GameLog::logCombatStart(const QString &territory, const QString &attacker,
                             const QString &defender)
{
    if (!m_logFile.isOpen()) return;

    m_stream << "\n  [COMBAT] Battle at " << territory << ": " << attacker << " vs " << defender << "\n";
    m_stream.flush();
}

void GameLog::logCombatRoll(const QString &playerName, const QString &unitType,
                            int dieRoll, bool isHit, const QString &targetType)
{
    QString result = isHit ? "HIT" : "MISS";
    QString target = targetType.isEmpty() ? "" : QString(" -> %1").arg(targetType);
    write("COMBAT", QString("  %1 %2 rolled %3: %4%5")
          .arg(playerName).arg(unitType).arg(dieRoll).arg(result).arg(target));
}

void GameLog::logCombatCasualty(const QString &playerName, const QString &unitType,
                                const QString &territory)
{
    write("COMBAT", QString("  %1 %2 destroyed at %3")
          .arg(playerName).arg(unitType).arg(territory));
}

void GameLog::logCombatRetreat(const QString &playerName, const QString &fromTerritory,
                               const QString &toTerritory)
{
    write("COMBAT", QString("  %1 retreated from %2 to %3")
          .arg(playerName).arg(fromTerritory).arg(toTerritory));
}

void GameLog::logCombatEnd(const QString &territory, const QString &winner)
{
    write("COMBAT", QString("  Battle at %1 ended. Winner: %2")
          .arg(territory).arg(winner));
}

void GameLog::logCityDestroyed(const QString &playerName, const QString &territory,
                               const QString &reason)
{
    write("CITY", QString("%1: City at %2 destroyed (%3)")
          .arg(playerName).arg(territory).arg(reason));
}

void GameLog::logGeneral(const QString &playerName, const QString &message)
{
    write("INFO", QString("%1: %2").arg(playerName).arg(message));
}
