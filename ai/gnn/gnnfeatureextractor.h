#ifndef GNNFEATUREEXTRACTOR_H
#define GNNFEATUREEXTRACTOR_H

#include <QString>
#include <QList>
#include <QMap>
#include <QVector>
#include <QPair>
#include <QJsonObject>
#include <QJsonArray>

class GameStateSnapshot;

/**
 * @brief Feature names and indices for GNN input
 *
 * Defines the order and meaning of features in the node feature vector.
 */
struct GNNFeatureSpec {
    // Total feature dimension
    static constexpr int FEATURE_DIM = 30;

    // Feature indices (must match Python model)
    enum FeatureIndex {
        // Ownership (3)
        F_IS_MINE = 0,
        F_IS_ENEMY = 1,
        F_IS_NEUTRAL = 2,

        // My forces (6)
        F_MY_INFANTRY = 3,
        F_MY_CAVALRY = 4,
        F_MY_CATAPULTS = 5,
        F_MY_GENERALS = 6,
        F_MY_GALLEYS = 7,
        F_MY_CAESAR = 8,

        // Enemy forces (6)
        F_ENEMY_INFANTRY = 9,
        F_ENEMY_CAVALRY = 10,
        F_ENEMY_CATAPULTS = 11,
        F_ENEMY_GENERALS = 12,
        F_ENEMY_GALLEYS = 13,
        F_ENEMY_CAESAR = 14,

        // Infrastructure (3)
        F_HAS_CITY = 15,
        F_HAS_FORTIFICATION = 16,
        F_ON_ROAD_NETWORK = 17,

        // Territory properties (3)
        F_VALUE_NORMALIZED = 18,  // 0, 0.25, 0.5, 1.0 for 0, 5, 10, 20
        F_IS_SEA = 19,
        F_IS_HOME_PROVINCE = 20,

        // Strategic distances (2)
        F_DIST_TO_CAESAR = 21,    // Normalized (0 = here, 1 = far/unreachable)
        F_DIST_TO_ENEMY = 22,     // Normalized

        // Heat map features (4)
        F_MY_FORCE_1TURN = 23,    // Normalized
        F_MY_FORCE_2TURN = 24,
        F_ENEMY_THREAT_1TURN = 25,
        F_ENEMY_THREAT_2TURN = 26,

        // Risk labels (for supervised training) (3)
        F_RISK_SCORE = 27,        // 0.0 to 1.0
        F_RISK_LEVEL = 28,        // Categorical: 0-4
        F_UNUSED = 29,            // Padding for alignment
    };

    // Get feature name for debugging
    static QString featureName(int index);
};

/**
 * @brief Extracted features ready for GNN consumption
 *
 * Contains node features as a 2D array and edge list for adjacency.
 */
struct GNNData {
    // Node features: [num_nodes, feature_dim]
    // Stored as flat vector, row-major order
    QVector<float> nodeFeatures;
    int numNodes = 0;
    int featureDim = GNNFeatureSpec::FEATURE_DIM;

    // Edge list: pairs of (source_idx, target_idx)
    // Stored as two parallel lists for COO format
    QVector<int> edgeSrc;
    QVector<int> edgeDst;
    int numEdges = 0;

    // Node labels (risk scores) for training
    QVector<float> labels;

    // Territory name to index mapping
    QMap<QString, int> nameToIndex;
    QList<QString> indexToName;

    // Access node features
    float getFeature(int nodeIdx, int featureIdx) const;
    void setFeature(int nodeIdx, int featureIdx, float value);

    // Serialize to JSON (for Python interop)
    QJsonObject toJson() const;

    // Get feature matrix as nested array (for debugging)
    QVector<QVector<float>> getFeatureMatrix() const;
};

/**
 * @brief Converts GameStateSnapshot to GNN-ready tensor format
 *
 * Extracts node features and builds adjacency information
 * in a format suitable for PyTorch Geometric.
 */
class GNNFeatureExtractor
{
public:
    GNNFeatureExtractor();

    /**
     * @brief Extract GNN data from a game state snapshot
     * @param snapshot The captured game state
     * @param includeLandOnly If true, only include land territories (default: true)
     * @return GNNData structure ready for model input
     */
    GNNData extract(const GameStateSnapshot &snapshot, bool includeLandOnly = true);

    /**
     * @brief Extract and save to JSON file (for Python training)
     * @param snapshot The captured game state
     * @param filePath Output file path
     * @param includeLandOnly If true, only include land territories
     * @return true if successful
     */
    bool extractToFile(const GameStateSnapshot &snapshot, const QString &filePath, bool includeLandOnly = true);

    // === Normalization Settings ===

    // Set maximum troop count for normalization (default: 20)
    void setMaxTroops(int max) { m_maxTroops = max; }

    // Set maximum distance for normalization (default: 15)
    void setMaxDistance(int max) { m_maxDistance = max; }

    // Set maximum force projection for normalization (default: 30)
    void setMaxForce(int max) { m_maxForce = max; }

private:
    // Normalize troop counts (0 to 1)
    float normalizeTroops(int count) const;

    // Normalize distance (-1 = unreachable → 1.0, 0 → 0.0)
    float normalizeDistance(int dist) const;

    // Normalize territory value (0, 5, 10, 20 → 0, 0.25, 0.5, 1.0)
    float normalizeValue(int value) const;

    // Normalize force projection
    float normalizeForce(int force) const;

    // Build edge list from neighbor relationships
    void buildEdgeList(const GameStateSnapshot &snapshot,
                       const QMap<QString, int> &nameToIndex,
                       QVector<int> &edgeSrc,
                       QVector<int> &edgeDst);

    // Normalization parameters
    int m_maxTroops = 20;
    int m_maxDistance = 15;
    int m_maxForce = 30;
};

#endif // GNNFEATUREEXTRACTOR_H
