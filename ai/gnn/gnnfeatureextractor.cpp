#include "gnnfeatureextractor.h"
#include "gamestatesnapshot.h"

#include <QJsonDocument>
#include <QFile>
#include <cmath>

// === GNNFeatureSpec ===

QString GNNFeatureSpec::featureName(int index)
{
    switch (index) {
        case F_IS_MINE: return "is_mine";
        case F_IS_ENEMY: return "is_enemy";
        case F_IS_NEUTRAL: return "is_neutral";
        case F_MY_INFANTRY: return "my_infantry";
        case F_MY_CAVALRY: return "my_cavalry";
        case F_MY_CATAPULTS: return "my_catapults";
        case F_MY_GENERALS: return "my_generals";
        case F_MY_GALLEYS: return "my_galleys";
        case F_MY_CAESAR: return "my_caesar";
        case F_ENEMY_INFANTRY: return "enemy_infantry";
        case F_ENEMY_CAVALRY: return "enemy_cavalry";
        case F_ENEMY_CATAPULTS: return "enemy_catapults";
        case F_ENEMY_GENERALS: return "enemy_generals";
        case F_ENEMY_GALLEYS: return "enemy_galleys";
        case F_ENEMY_CAESAR: return "enemy_caesar";
        case F_HAS_CITY: return "has_city";
        case F_HAS_FORTIFICATION: return "has_fortification";
        case F_ON_ROAD_NETWORK: return "on_road_network";
        case F_VALUE_NORMALIZED: return "value_normalized";
        case F_IS_SEA: return "is_sea";
        case F_IS_HOME_PROVINCE: return "is_home_province";
        case F_DIST_TO_CAESAR: return "dist_to_caesar";
        case F_DIST_TO_ENEMY: return "dist_to_enemy";
        case F_MY_FORCE_1TURN: return "my_force_1turn";
        case F_MY_FORCE_2TURN: return "my_force_2turn";
        case F_ENEMY_THREAT_1TURN: return "enemy_threat_1turn";
        case F_ENEMY_THREAT_2TURN: return "enemy_threat_2turn";
        case F_RISK_SCORE: return "risk_score";
        case F_RISK_LEVEL: return "risk_level";
        default: return QString("unknown_%1").arg(index);
    }
}

// === GNNData ===

float GNNData::getFeature(int nodeIdx, int featureIdx) const
{
    if (nodeIdx < 0 || nodeIdx >= numNodes || featureIdx < 0 || featureIdx >= featureDim) {
        return 0.0f;
    }
    return nodeFeatures[nodeIdx * featureDim + featureIdx];
}

void GNNData::setFeature(int nodeIdx, int featureIdx, float value)
{
    if (nodeIdx < 0 || nodeIdx >= numNodes || featureIdx < 0 || featureIdx >= featureDim) {
        return;
    }
    nodeFeatures[nodeIdx * featureDim + featureIdx] = value;
}

QJsonObject GNNData::toJson() const
{
    QJsonObject obj;

    // Metadata
    obj["num_nodes"] = numNodes;
    obj["feature_dim"] = featureDim;
    obj["num_edges"] = numEdges;

    // Feature names
    QJsonArray featureNames;
    for (int i = 0; i < featureDim; ++i) {
        featureNames.append(GNNFeatureSpec::featureName(i));
    }
    obj["feature_names"] = featureNames;

    // Node features as 2D array
    QJsonArray features;
    for (int i = 0; i < numNodes; ++i) {
        QJsonArray row;
        for (int j = 0; j < featureDim; ++j) {
            row.append(static_cast<double>(getFeature(i, j)));
        }
        features.append(row);
    }
    obj["node_features"] = features;

    // Edge index as 2D array [[src...], [dst...]]
    QJsonArray edgeIndex;
    QJsonArray srcArray, dstArray;
    for (int i = 0; i < numEdges; ++i) {
        srcArray.append(edgeSrc[i]);
        dstArray.append(edgeDst[i]);
    }
    edgeIndex.append(srcArray);
    edgeIndex.append(dstArray);
    obj["edge_index"] = edgeIndex;

    // Labels
    QJsonArray labelsArray;
    for (float l : labels) {
        labelsArray.append(static_cast<double>(l));
    }
    obj["labels"] = labelsArray;

    // Territory mapping
    QJsonArray nodeNames;
    for (const QString &name : indexToName) {
        nodeNames.append(name);
    }
    obj["node_names"] = nodeNames;

    return obj;
}

QVector<QVector<float>> GNNData::getFeatureMatrix() const
{
    QVector<QVector<float>> matrix(numNodes);
    for (int i = 0; i < numNodes; ++i) {
        matrix[i].resize(featureDim);
        for (int j = 0; j < featureDim; ++j) {
            matrix[i][j] = getFeature(i, j);
        }
    }
    return matrix;
}

// === GNNFeatureExtractor ===

GNNFeatureExtractor::GNNFeatureExtractor()
{
}

GNNData GNNFeatureExtractor::extract(const GameStateSnapshot &snapshot, bool includeLandOnly)
{
    GNNData data;

    // Build territory list and index mapping
    QList<QString> territoryNames = snapshot.getTerritoryNames();
    int nodeIdx = 0;

    for (const QString &name : territoryNames) {
        TerritorySnapshot ts = snapshot.getTerritory(name);

        // Skip sea territories if requested
        if (includeLandOnly && ts.isSea) {
            continue;
        }

        data.nameToIndex[name] = nodeIdx;
        data.indexToName.append(name);
        nodeIdx++;
    }

    data.numNodes = data.indexToName.size();
    data.featureDim = GNNFeatureSpec::FEATURE_DIM;
    data.nodeFeatures.resize(data.numNodes * data.featureDim, 0.0f);
    data.labels.resize(data.numNodes, 0.0f);

    // Extract features for each node
    for (int i = 0; i < data.numNodes; ++i) {
        QString name = data.indexToName[i];
        TerritorySnapshot ts = snapshot.getTerritory(name);

        using F = GNNFeatureSpec;

        // Ownership
        data.setFeature(i, F::F_IS_MINE, ts.isMine ? 1.0f : 0.0f);
        data.setFeature(i, F::F_IS_ENEMY, ts.isEnemy ? 1.0f : 0.0f);
        data.setFeature(i, F::F_IS_NEUTRAL, ts.isNeutral ? 1.0f : 0.0f);

        // My forces (normalized)
        data.setFeature(i, F::F_MY_INFANTRY, normalizeTroops(ts.myInfantry));
        data.setFeature(i, F::F_MY_CAVALRY, normalizeTroops(ts.myCavalry));
        data.setFeature(i, F::F_MY_CATAPULTS, normalizeTroops(ts.myCatapults));
        data.setFeature(i, F::F_MY_GENERALS, normalizeTroops(ts.myGenerals));
        data.setFeature(i, F::F_MY_GALLEYS, normalizeTroops(ts.myGalleys));
        data.setFeature(i, F::F_MY_CAESAR, ts.myCaesarHere ? 1.0f : 0.0f);

        // Enemy forces (normalized)
        data.setFeature(i, F::F_ENEMY_INFANTRY, normalizeTroops(ts.enemyInfantry));
        data.setFeature(i, F::F_ENEMY_CAVALRY, normalizeTroops(ts.enemyCavalry));
        data.setFeature(i, F::F_ENEMY_CATAPULTS, normalizeTroops(ts.enemyCatapults));
        data.setFeature(i, F::F_ENEMY_GENERALS, normalizeTroops(ts.enemyGenerals));
        data.setFeature(i, F::F_ENEMY_GALLEYS, normalizeTroops(ts.enemyGalleys));
        data.setFeature(i, F::F_ENEMY_CAESAR, ts.enemyCaesarHere ? 1.0f : 0.0f);

        // Infrastructure
        data.setFeature(i, F::F_HAS_CITY, ts.hasCity ? 1.0f : 0.0f);
        data.setFeature(i, F::F_HAS_FORTIFICATION, ts.hasFortification ? 1.0f : 0.0f);
        data.setFeature(i, F::F_ON_ROAD_NETWORK, ts.onMyRoadNetwork ? 1.0f : 0.0f);

        // Territory properties
        data.setFeature(i, F::F_VALUE_NORMALIZED, normalizeValue(ts.value));
        data.setFeature(i, F::F_IS_SEA, ts.isSea ? 1.0f : 0.0f);
        data.setFeature(i, F::F_IS_HOME_PROVINCE, ts.isHomeProvince ? 1.0f : 0.0f);

        // Strategic distances
        data.setFeature(i, F::F_DIST_TO_CAESAR, normalizeDistance(ts.distToMyCaesar));
        data.setFeature(i, F::F_DIST_TO_ENEMY, normalizeDistance(ts.distToNearestEnemy));

        // Heat map features
        data.setFeature(i, F::F_MY_FORCE_1TURN, normalizeForce(ts.myForce1Turn));
        data.setFeature(i, F::F_MY_FORCE_2TURN, normalizeForce(ts.myForce2Turn));
        data.setFeature(i, F::F_ENEMY_THREAT_1TURN, normalizeForce(ts.enemyThreat1Turn));
        data.setFeature(i, F::F_ENEMY_THREAT_2TURN, normalizeForce(ts.enemyThreat2Turn));

        // Risk labels
        data.setFeature(i, F::F_RISK_SCORE, static_cast<float>(ts.riskScore));
        data.setFeature(i, F::F_RISK_LEVEL, static_cast<float>(ts.riskLevel) / 4.0f);  // Normalize 0-4 to 0-1

        // Training label
        data.labels[i] = static_cast<float>(ts.riskScore);
    }

    // Build edge list
    buildEdgeList(snapshot, data.nameToIndex, data.edgeSrc, data.edgeDst);
    data.numEdges = data.edgeSrc.size();

    return data;
}

bool GNNFeatureExtractor::extractToFile(const GameStateSnapshot &snapshot, const QString &filePath, bool includeLandOnly)
{
    GNNData data = extract(snapshot, includeLandOnly);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonDocument doc(data.toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

float GNNFeatureExtractor::normalizeTroops(int count) const
{
    if (count <= 0) return 0.0f;
    return std::min(1.0f, static_cast<float>(count) / m_maxTroops);
}

float GNNFeatureExtractor::normalizeDistance(int dist) const
{
    if (dist < 0) return 1.0f;  // Unreachable = max distance
    if (dist == 0) return 0.0f;
    return std::min(1.0f, static_cast<float>(dist) / m_maxDistance);
}

float GNNFeatureExtractor::normalizeValue(int value) const
{
    switch (value) {
        case 0: return 0.0f;
        case 5: return 0.25f;
        case 10: return 0.5f;
        case 20: return 1.0f;
        default: return static_cast<float>(value) / 20.0f;
    }
}

float GNNFeatureExtractor::normalizeForce(int force) const
{
    if (force <= 0) return 0.0f;
    return std::min(1.0f, static_cast<float>(force) / m_maxForce);
}

void GNNFeatureExtractor::buildEdgeList(const GameStateSnapshot &snapshot,
                                         const QMap<QString, int> &nameToIndex,
                                         QVector<int> &edgeSrc,
                                         QVector<int> &edgeDst)
{
    edgeSrc.clear();
    edgeDst.clear();

    // Build bidirectional edges from neighbor relationships
    for (auto it = nameToIndex.begin(); it != nameToIndex.end(); ++it) {
        QString srcName = it.key();
        int srcIdx = it.value();

        QList<QString> neighbors = snapshot.getNeighbors(srcName);
        for (const QString &dstName : neighbors) {
            // Only add edge if destination is in our index
            if (nameToIndex.contains(dstName)) {
                int dstIdx = nameToIndex[dstName];
                edgeSrc.append(srcIdx);
                edgeDst.append(dstIdx);
            }
        }
    }
}
