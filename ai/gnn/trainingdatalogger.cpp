#include "trainingdatalogger.h"
#include "gamestatesnapshot.h"
#include "gnnfeatureextractor.h"
#include "../../player.h"
#include "../../mapgraph.h"
#include "../reachabilitycalculator.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDateTime>
#include <QDebug>

TrainingDataLogger::TrainingDataLogger()
{
    // Default output directory
    m_outputDir = "training_data";
}

TrainingDataLogger::~TrainingDataLogger()
{
    if (m_gameInProgress) {
        endGame('\0');  // End with no winner if destructor called mid-game
    }
}

void TrainingDataLogger::setOutputDirectory(const QString &dir)
{
    m_outputDir = dir;

    // Create directory if it doesn't exist
    QDir d(dir);
    if (!d.exists()) {
        d.mkpath(".");
    }
}

void TrainingDataLogger::startGame(const QString &gameId, const QList<QChar> &playerIds)
{
    if (m_gameInProgress) {
        qWarning() << "TrainingDataLogger: Starting new game without ending previous one";
        endGame('\0');
    }

    m_gameId = gameId;
    m_playerIds = playerIds;
    m_snapshotCount = 0;
    m_combatOutcomes.clear();
    m_territoryCaptures.clear();
    m_gameInProgress = true;

    // Create game subdirectory
    QString gameDir = m_outputDir + "/" + m_gameId;
    QDir().mkpath(gameDir);

    qDebug() << "TrainingDataLogger: Started game" << gameId << "with" << playerIds.size() << "players";
}

void TrainingDataLogger::logSnapshot(Player *currentPlayer,
                                      const QList<Player*> &allPlayers,
                                      MapGraph *graph,
                                      ReachabilityCalculator *reachCalc,
                                      int turnNumber,
                                      const QString &phase)
{
    if (!m_enabled || !m_gameInProgress) {
        return;
    }

    if (!currentPlayer || !graph) {
        qWarning() << "TrainingDataLogger: Invalid player or graph";
        return;
    }

    // Capture game state
    GameStateSnapshot snapshot;
    snapshot.capture(currentPlayer, allPlayers, graph, reachCalc);

    // Extract GNN features
    GNNFeatureExtractor extractor;
    QString filename = generateSnapshotFilename(turnNumber, phase, currentPlayer->getId());
    QString filepath = m_outputDir + "/" + m_gameId + "/" + filename;

    if (extractor.extractToFile(snapshot, filepath, true)) {
        m_snapshotCount++;
        qDebug() << "TrainingDataLogger: Saved snapshot" << m_snapshotCount << "to" << filename;
    } else {
        qWarning() << "TrainingDataLogger: Failed to save snapshot to" << filepath;
    }
}

void TrainingDataLogger::logCombatOutcome(const QString &territory,
                                           QChar attackerId,
                                           QChar defenderId,
                                           bool attackerWon,
                                           int attackerCasualties,
                                           int defenderCasualties)
{
    if (!m_enabled || !m_gameInProgress) {
        return;
    }

    CombatOutcome outcome;
    outcome.snapshotId = m_snapshotCount;
    outcome.territory = territory;
    outcome.attackerId = attackerId;
    outcome.defenderId = defenderId;
    outcome.attackerWon = attackerWon;
    outcome.attackerCasualties = attackerCasualties;
    outcome.defenderCasualties = defenderCasualties;

    m_combatOutcomes.append(outcome);
    qDebug() << "TrainingDataLogger: Logged combat at" << territory
             << "winner:" << (attackerWon ? attackerId : defenderId);
}

void TrainingDataLogger::logTerritoryCapture(const QString &territory,
                                              QChar previousOwner,
                                              QChar newOwner,
                                              int turnNumber)
{
    if (!m_enabled || !m_gameInProgress) {
        return;
    }

    TerritoryCapture capture;
    capture.territory = territory;
    capture.previousOwner = previousOwner;
    capture.newOwner = newOwner;
    capture.turnNumber = turnNumber;
    capture.snapshotId = m_snapshotCount;

    m_territoryCaptures.append(capture);
    qDebug() << "TrainingDataLogger: Logged capture of" << territory
             << "by" << newOwner << "from" << previousOwner;
}

void TrainingDataLogger::endGame(QChar winnerId)
{
    if (!m_gameInProgress) {
        return;
    }

    // Write combat outcomes
    writeCombatOutcomes();

    // Write territory captures
    writeTerritoryCaptures();

    // Write game summary
    writeGameSummary(winnerId);

    m_gameInProgress = false;

    qDebug() << "TrainingDataLogger: Ended game" << m_gameId
             << "with" << m_snapshotCount << "snapshots,"
             << m_combatOutcomes.size() << "combats,"
             << m_territoryCaptures.size() << "captures";
}

void TrainingDataLogger::flush()
{
    if (m_gameInProgress) {
        writeCombatOutcomes();
        writeTerritoryCaptures();
    }
}

QString TrainingDataLogger::generateSnapshotFilename(int turnNumber, const QString &phase, QChar playerId)
{
    return QString("snapshot_t%1_%2_p%3.json")
            .arg(turnNumber, 4, 10, QChar('0'))
            .arg(phase)
            .arg(playerId);
}

void TrainingDataLogger::writeCombatOutcomes()
{
    if (m_combatOutcomes.isEmpty()) {
        return;
    }

    QJsonArray outcomes;
    for (const CombatOutcome &co : m_combatOutcomes) {
        QJsonObject obj;
        obj["snapshot_id"] = co.snapshotId;
        obj["territory"] = co.territory;
        obj["attacker"] = QString(co.attackerId);
        obj["defender"] = QString(co.defenderId);
        obj["attacker_won"] = co.attackerWon;
        obj["attacker_casualties"] = co.attackerCasualties;
        obj["defender_casualties"] = co.defenderCasualties;
        outcomes.append(obj);
    }

    QJsonObject root;
    root["game_id"] = m_gameId;
    root["combat_outcomes"] = outcomes;

    QString filepath = m_outputDir + "/" + m_gameId + "/combat_outcomes.json";
    QFile file(filepath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(root);
        file.write(doc.toJson(QJsonDocument::Indented));
    }
}

void TrainingDataLogger::writeTerritoryCaptures()
{
    if (m_territoryCaptures.isEmpty()) {
        return;
    }

    QJsonArray captures;
    for (const TerritoryCapture &tc : m_territoryCaptures) {
        QJsonObject obj;
        obj["territory"] = tc.territory;
        obj["previous_owner"] = tc.previousOwner.isNull() ? "" : QString(tc.previousOwner);
        obj["new_owner"] = QString(tc.newOwner);
        obj["turn_number"] = tc.turnNumber;
        obj["snapshot_id"] = tc.snapshotId;
        captures.append(obj);
    }

    QJsonObject root;
    root["game_id"] = m_gameId;
    root["territory_captures"] = captures;

    QString filepath = m_outputDir + "/" + m_gameId + "/territory_captures.json";
    QFile file(filepath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(root);
        file.write(doc.toJson(QJsonDocument::Indented));
    }
}

void TrainingDataLogger::writeGameSummary(QChar winnerId)
{
    QJsonObject summary;
    summary["game_id"] = m_gameId;
    summary["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    summary["winner"] = winnerId.isNull() ? "" : QString(winnerId);
    summary["total_snapshots"] = m_snapshotCount;
    summary["total_combats"] = m_combatOutcomes.size();
    summary["total_captures"] = m_territoryCaptures.size();

    QJsonArray players;
    for (QChar p : m_playerIds) {
        players.append(QString(p));
    }
    summary["players"] = players;

    QString filepath = m_outputDir + "/" + m_gameId + "/game_summary.json";
    QFile file(filepath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(summary);
        file.write(doc.toJson(QJsonDocument::Indented));
    }
}
