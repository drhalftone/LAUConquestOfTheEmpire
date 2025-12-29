#include "gamestatesnapshot.h"
#include "../reachabilitycalculator.h"
#include "../../player.h"
#include "../../mapgraph.h"
#include "../../gamepiece.h"
#include "../../building.h"

#include <QJsonDocument>
#include <QFile>
#include <QDateTime>

// === TerritorySnapshot ===

QJsonObject TerritorySnapshot::toJson() const
{
    QJsonObject obj;

    // Identity
    obj["name"] = name;
    obj["id"] = id;
    obj["isSea"] = isSea;
    obj["value"] = value;

    // Ownership
    obj["owner"] = owner.isNull() ? "" : QString(owner);
    obj["isMine"] = isMine;
    obj["isEnemy"] = isEnemy;
    obj["isNeutral"] = isNeutral;

    // My forces
    obj["myInfantry"] = myInfantry;
    obj["myCavalry"] = myCavalry;
    obj["myCatapults"] = myCatapults;
    obj["myGenerals"] = myGenerals;
    obj["myGalleys"] = myGalleys;
    obj["myCaesarHere"] = myCaesarHere;

    // Enemy forces
    obj["enemyInfantry"] = enemyInfantry;
    obj["enemyCavalry"] = enemyCavalry;
    obj["enemyCatapults"] = enemyCatapults;
    obj["enemyGenerals"] = enemyGenerals;
    obj["enemyGalleys"] = enemyGalleys;
    obj["enemyCaesarHere"] = enemyCaesarHere;

    // Infrastructure
    obj["hasCity"] = hasCity;
    obj["hasFortification"] = hasFortification;
    obj["onMyRoadNetwork"] = onMyRoadNetwork;

    // Strategic
    obj["isHomeProvince"] = isHomeProvince;
    obj["distToMyCaesar"] = distToMyCaesar;
    obj["distToNearestEnemy"] = distToNearestEnemy;

    // Heat map
    obj["myForce1Turn"] = myForce1Turn;
    obj["myForce2Turn"] = myForce2Turn;
    obj["enemyThreat1Turn"] = enemyThreat1Turn;
    obj["enemyThreat2Turn"] = enemyThreat2Turn;

    // Risk labels
    obj["riskLevel"] = riskLevel;
    obj["riskScore"] = riskScore;

    // Neighbors
    QJsonArray neighborsArray;
    for (const QString &n : neighbors) {
        neighborsArray.append(n);
    }
    obj["neighbors"] = neighborsArray;

    return obj;
}

TerritorySnapshot TerritorySnapshot::fromJson(const QJsonObject &obj)
{
    TerritorySnapshot ts;

    // Identity
    ts.name = obj["name"].toString();
    ts.id = obj["id"].toInt();
    ts.isSea = obj["isSea"].toBool();
    ts.value = obj["value"].toInt();

    // Ownership
    QString ownerStr = obj["owner"].toString();
    ts.owner = ownerStr.isEmpty() ? '\0' : ownerStr.at(0);
    ts.isMine = obj["isMine"].toBool();
    ts.isEnemy = obj["isEnemy"].toBool();
    ts.isNeutral = obj["isNeutral"].toBool();

    // My forces
    ts.myInfantry = obj["myInfantry"].toInt();
    ts.myCavalry = obj["myCavalry"].toInt();
    ts.myCatapults = obj["myCatapults"].toInt();
    ts.myGenerals = obj["myGenerals"].toInt();
    ts.myGalleys = obj["myGalleys"].toInt();
    ts.myCaesarHere = obj["myCaesarHere"].toBool();

    // Enemy forces
    ts.enemyInfantry = obj["enemyInfantry"].toInt();
    ts.enemyCavalry = obj["enemyCavalry"].toInt();
    ts.enemyCatapults = obj["enemyCatapults"].toInt();
    ts.enemyGenerals = obj["enemyGenerals"].toInt();
    ts.enemyGalleys = obj["enemyGalleys"].toInt();
    ts.enemyCaesarHere = obj["enemyCaesarHere"].toBool();

    // Infrastructure
    ts.hasCity = obj["hasCity"].toBool();
    ts.hasFortification = obj["hasFortification"].toBool();
    ts.onMyRoadNetwork = obj["onMyRoadNetwork"].toBool();

    // Strategic
    ts.isHomeProvince = obj["isHomeProvince"].toBool();
    ts.distToMyCaesar = obj["distToMyCaesar"].toInt();
    ts.distToNearestEnemy = obj["distToNearestEnemy"].toInt();

    // Heat map
    ts.myForce1Turn = obj["myForce1Turn"].toInt();
    ts.myForce2Turn = obj["myForce2Turn"].toInt();
    ts.enemyThreat1Turn = obj["enemyThreat1Turn"].toInt();
    ts.enemyThreat2Turn = obj["enemyThreat2Turn"].toInt();

    // Risk labels
    ts.riskLevel = obj["riskLevel"].toInt();
    ts.riskScore = obj["riskScore"].toDouble();

    // Neighbors
    QJsonArray neighborsArray = obj["neighbors"].toArray();
    for (const QJsonValue &v : neighborsArray) {
        ts.neighbors.append(v.toString());
    }

    return ts;
}

// === GameStateSnapshot ===

GameStateSnapshot::GameStateSnapshot()
{
}

void GameStateSnapshot::capture(Player *currentPlayer,
                                 const QList<Player*> &allPlayers,
                                 MapGraph *graph,
                                 ReachabilityCalculator *reachCalc)
{
    if (!currentPlayer || !graph) {
        return;
    }

    m_territories.clear();
    m_territoryNames.clear();

    m_currentPlayerId = currentPlayer->getId();
    m_timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);

    // Get heat maps if reachability calculator is provided
    QMap<QString, int> myForce1Turn;
    QMap<QString, int> myForce2Turn;
    QMap<QString, int> enemyThreat1Turn;
    QMap<QString, int> enemyThreat2Turn;

    if (reachCalc) {
        myForce1Turn = reachCalc->getForceProjectionMap(currentPlayer, graph, 1);
        myForce2Turn = reachCalc->getForceProjectionMap(currentPlayer, graph, 2);
        enemyThreat1Turn = reachCalc->getEnemyThreatMap(currentPlayer, allPlayers, graph, 1);
        enemyThreat2Turn = reachCalc->getEnemyThreatMap(currentPlayer, allPlayers, graph, 2);
    }

    // Find Caesar position for distance calculations
    QString myCaesarTerritory;
    for (CaesarPiece *caesar : currentPlayer->getCaesars()) {
        myCaesarTerritory = caesar->getTerritoryName();
        break;
    }

    // Collect all home province names
    QSet<QString> homeProvinces;
    for (Player *p : allPlayers) {
        homeProvinces.insert(p->getHomeProvinceName());
    }

    // Iterate through all territories
    QList<QString> territoryNames = graph->getTerritoryNames();
    for (const QString &name : territoryNames) {
        m_territoryNames.append(name);

        Territory terr = graph->getTerritory(name);
        TerritorySnapshot ts;

        // Identity
        ts.name = name;
        ts.id = terr.id;
        ts.isSea = (terr.type == TerritoryType::Sea);
        ts.value = terr.value;
        ts.neighbors = terr.neighbors;

        // Ownership
        ts.owner = '\0';
        ts.isNeutral = true;
        ts.isMine = false;
        ts.isEnemy = false;

        for (Player *p : allPlayers) {
            if (p->ownsTerritory(name)) {
                ts.owner = p->getId();
                ts.isNeutral = false;
                if (p == currentPlayer) {
                    ts.isMine = true;
                } else {
                    ts.isEnemy = true;
                }
                break;
            }
        }

        // Count forces at this territory
        for (Player *p : allPlayers) {
            bool isMe = (p == currentPlayer);

            // Count units
            int infantry = p->getInfantryAtTerritory(name).size();
            int cavalry = p->getCavalryAtTerritory(name).size();
            int catapults = p->getCatapultsAtTerritory(name).size();
            int generals = p->getGeneralsAtTerritory(name).size();
            int galleys = p->getGalleysAtTerritory(name).size();
            bool hasCaesar = !p->getCaesarsAtTerritory(name).isEmpty();

            if (isMe) {
                ts.myInfantry = infantry;
                ts.myCavalry = cavalry;
                ts.myCatapults = catapults;
                ts.myGenerals = generals;
                ts.myGalleys = galleys;
                ts.myCaesarHere = hasCaesar;
            } else {
                ts.enemyInfantry += infantry;
                ts.enemyCavalry += cavalry;
                ts.enemyCatapults += catapults;
                ts.enemyGenerals += generals;
                ts.enemyGalleys += galleys;
                ts.enemyCaesarHere = ts.enemyCaesarHere || hasCaesar;
            }

            // Check for city
            City *city = p->getCityAtTerritory(name);
            if (city) {
                ts.hasCity = true;
                ts.hasFortification = city->isFortified();
            }
        }

        // Road network check
        ts.onMyRoadNetwork = isOnRoadNetwork(name, currentPlayer, graph);

        // Strategic features
        ts.isHomeProvince = homeProvinces.contains(name);

        // Distance to my Caesar
        if (!myCaesarTerritory.isEmpty() && !ts.isSea) {
            ts.distToMyCaesar = graph->getDistance(name, myCaesarTerritory);
        } else {
            ts.distToMyCaesar = -1;
        }

        // Distance to nearest enemy
        ts.distToNearestEnemy = calculateDistanceToNearestEnemy(name, currentPlayer, allPlayers, graph);

        // Heat map features
        if (reachCalc) {
            ts.myForce1Turn = myForce1Turn.value(name, 0);
            ts.myForce2Turn = myForce2Turn.value(name, 0);
            ts.enemyThreat1Turn = enemyThreat1Turn.value(name, 0);
            ts.enemyThreat2Turn = enemyThreat2Turn.value(name, 0);
        }

        // Calculate risk labels from heuristic
        // Risk level based on force comparison
        if (ts.myForce1Turn > 0 && ts.enemyThreat1Turn == 0) {
            ts.riskLevel = 0;  // Safe
            ts.riskScore = 0.0;
        } else if (ts.myForce1Turn > ts.enemyThreat1Turn * 1.5) {
            ts.riskLevel = 1;  // Low
            ts.riskScore = 0.25;
        } else if (ts.myForce1Turn > ts.enemyThreat1Turn * 0.8) {
            ts.riskLevel = 2;  // Medium
            ts.riskScore = 0.5;
        } else if (ts.enemyThreat1Turn > 0) {
            if (ts.myForce1Turn > 0) {
                ts.riskLevel = 4;  // Contested
                ts.riskScore = 0.75;
            } else {
                ts.riskLevel = 3;  // High
                ts.riskScore = 1.0;
            }
        } else {
            ts.riskLevel = 0;  // Safe (no threat)
            ts.riskScore = 0.0;
        }

        m_territories[name] = ts;
    }
}

TerritorySnapshot GameStateSnapshot::getTerritory(const QString &name) const
{
    return m_territories.value(name, TerritorySnapshot());
}

QList<QString> GameStateSnapshot::getNeighbors(const QString &territory) const
{
    if (m_territories.contains(territory)) {
        return m_territories[territory].neighbors;
    }
    return QList<QString>();
}

int GameStateSnapshot::calculateDistanceToCaesar(const QString &territory, Player *player, MapGraph *graph)
{
    for (CaesarPiece *caesar : player->getCaesars()) {
        QString caesarTerritory = caesar->getTerritoryName();
        if (!caesarTerritory.isEmpty()) {
            return graph->getDistance(territory, caesarTerritory);
        }
    }
    return -1;
}

int GameStateSnapshot::calculateDistanceToNearestEnemy(const QString &territory, Player *currentPlayer,
                                                        const QList<Player*> &allPlayers, MapGraph *graph)
{
    int minDist = -1;

    for (Player *p : allPlayers) {
        if (p == currentPlayer) continue;

        // Check all pieces of this enemy
        for (GamePiece *piece : p->getAllPieces()) {
            QString pieceTerr = piece->getTerritoryName();
            if (pieceTerr.isEmpty()) continue;

            int dist = graph->getDistance(territory, pieceTerr);
            if (dist >= 0 && (minDist < 0 || dist < minDist)) {
                minDist = dist;
            }
        }
    }

    return minDist;
}

bool GameStateSnapshot::isOnRoadNetwork(const QString &territory, Player *player, MapGraph *graph)
{
    // Check if this territory has a city owned by the player
    City *city = player->getCityAtTerritory(territory);
    if (!city) {
        return false;
    }

    // Check if there are road-connected territories (meaning it's on a network)
    QStringList connected = graph->getRoadConnectedTerritories(territory, player);
    return !connected.isEmpty();
}

QJsonObject GameStateSnapshot::toJson() const
{
    QJsonObject obj;

    // Metadata
    obj["currentPlayer"] = QString(m_currentPlayerId);
    obj["turnNumber"] = m_turnNumber;
    obj["timestamp"] = m_timestamp;
    obj["territoryCount"] = m_territories.size();

    // Territory order (for consistent indexing)
    QJsonArray nameOrder;
    for (const QString &name : m_territoryNames) {
        nameOrder.append(name);
    }
    obj["territoryOrder"] = nameOrder;

    // Territories
    QJsonObject territoriesObj;
    for (auto it = m_territories.begin(); it != m_territories.end(); ++it) {
        territoriesObj[it.key()] = it.value().toJson();
    }
    obj["territories"] = territoriesObj;

    return obj;
}

bool GameStateSnapshot::fromJson(const QJsonObject &obj)
{
    m_territories.clear();
    m_territoryNames.clear();

    // Metadata
    QString playerStr = obj["currentPlayer"].toString();
    m_currentPlayerId = playerStr.isEmpty() ? '\0' : playerStr.at(0);
    m_turnNumber = obj["turnNumber"].toInt();
    m_timestamp = obj["timestamp"].toString();

    // Territory order
    QJsonArray nameOrder = obj["territoryOrder"].toArray();
    for (const QJsonValue &v : nameOrder) {
        m_territoryNames.append(v.toString());
    }

    // Territories
    QJsonObject territoriesObj = obj["territories"].toObject();
    for (auto it = territoriesObj.begin(); it != territoriesObj.end(); ++it) {
        TerritorySnapshot ts = TerritorySnapshot::fromJson(it.value().toObject());
        m_territories[it.key()] = ts;
    }

    return true;
}

bool GameStateSnapshot::saveToFile(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

bool GameStateSnapshot::loadFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        return false;
    }

    return fromJson(doc.object());
}

int GameStateSnapshot::landTerritoryCount() const
{
    int count = 0;
    for (const TerritorySnapshot &ts : m_territories) {
        if (!ts.isSea) count++;
    }
    return count;
}

int GameStateSnapshot::seaTerritoryCount() const
{
    int count = 0;
    for (const TerritorySnapshot &ts : m_territories) {
        if (ts.isSea) count++;
    }
    return count;
}
