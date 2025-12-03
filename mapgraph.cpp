#include "mapgraph.h"
#include "player.h"
#include <QQueue>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QDebug>

MapGraph::MapGraph()
{
    loadFromCSV();
    loadBeachPositions();
}

void MapGraph::loadFromCSV()
{
    QFile file(":/images/territories.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open territories.csv from resources";
        return;
    }

    QTextStream in(&file);

    // Skip header line
    if (!in.atEnd()) {
        in.readLine();
    }

    // First pass: read all territories and build ID-to-name map
    QMap<int, QStringList> neighborIds;  // Store neighbor IDs for second pass

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList fields = line.split(',');
        if (fields.size() < 8) continue;

        int id = fields[0].toInt();
        int area = fields[1].toInt();
        double centroidX = fields[3].toDouble();
        double centroidY = fields[4].toDouble();
        QString name = fields[5].trimmed();
        QString pointsStr = fields[6].trimmed();
        QString neighborsStr = fields[7].trimmed();

        // Determine type: sea territories have names starting with "Mare" or "Oceanus"
        TerritoryType type = TerritoryType::Land;
        if (name.startsWith("Mare") || name.startsWith("Oceanus")) {
            type = TerritoryType::Sea;
        }

        // Parse value (empty = 0 for sea territories)
        int value = pointsStr.isEmpty() ? 0 : pointsStr.toInt();

        // Create territory
        Territory territory(id, name, QPointF(centroidX, centroidY), type, value, area);
        m_territories[name] = territory;
        m_idToName[id] = name;

        // Store neighbor IDs for second pass
        if (!neighborsStr.isEmpty()) {
            neighborIds[id] = neighborsStr.split(';');
        }
    }

    file.close();

    // Second pass: resolve neighbor IDs to names
    for (auto it = neighborIds.begin(); it != neighborIds.end(); ++it) {
        int id = it.key();
        QString territoryName = m_idToName[id];

        for (const QString &neighborIdStr : it.value()) {
            int neighborId = neighborIdStr.toInt();
            if (m_idToName.contains(neighborId)) {
                m_territories[territoryName].neighbors.append(m_idToName[neighborId]);
            }
        }
    }

    qDebug() << "Loaded" << m_territories.size() << "territories from CSV";
}

void MapGraph::loadBeachPositions()
{
    QFile file(":/images/beach_positions.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open beach_positions.csv from resources";
        return;
    }

    QTextStream in(&file);

    // Skip header line
    if (!in.atEnd()) {
        in.readLine();
    }

    int count = 0;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList fields = line.split(',');
        if (fields.size() < 6) continue;

        QString landName = fields[1].trimmed();
        QString seaName = fields[3].trimmed();
        double beachX = fields[4].toDouble();
        double beachY = fields[5].toDouble();

        // Create key as "landName|seaName"
        QString key = landName + "|" + seaName;
        m_beachPositions[key] = QPointF(beachX, beachY);
        count++;
    }

    file.close();
    qDebug() << "Loaded" << count << "beach positions from CSV";
}

// === Territory Management ===

void MapGraph::addTerritory(const Territory &territory)
{
    m_territories[territory.name] = territory;
    if (territory.id > 0) {
        m_idToName[territory.id] = territory.name;
    }
}

Territory MapGraph::getTerritory(const QString &name) const
{
    if (m_territories.contains(name)) {
        return m_territories[name];
    }
    // Return empty territory if not found
    return Territory();
}

QList<QString> MapGraph::getTerritoryNames() const
{
    return m_territories.keys();
}

bool MapGraph::exists(const QString &name) const
{
    return m_territories.contains(name);
}

void MapGraph::removeTerritory(const QString &name)
{
    if (!exists(name)) {
        return;
    }

    // Remove this territory from all neighbors' adjacency lists
    const Territory &territory = m_territories[name];
    for (const QString &neighborName : territory.neighbors) {
        if (exists(neighborName)) {
            m_territories[neighborName].neighbors.removeAll(name);
        }
    }

    // Remove from ID map
    m_idToName.remove(territory.id);

    // Remove the territory itself
    m_territories.remove(name);
}

void MapGraph::clear()
{
    m_territories.clear();
    m_idToName.clear();
}

// === Adjacency and Navigation ===

QList<QString> MapGraph::getNeighbors(const QString &name) const
{
    if (exists(name)) {
        return m_territories[name].neighbors;
    }
    return QList<QString>();
}

bool MapGraph::areAdjacent(const QString &territory1, const QString &territory2) const
{
    if (!exists(territory1) || !exists(territory2)) {
        return false;
    }

    return m_territories[territory1].neighbors.contains(territory2);
}

void MapGraph::addEdge(const QString &territory1, const QString &territory2)
{
    if (!exists(territory1) || !exists(territory2)) {
        return;
    }

    // Add bidirectional edge
    if (!m_territories[territory1].neighbors.contains(territory2)) {
        m_territories[territory1].neighbors.append(territory2);
    }
    if (!m_territories[territory2].neighbors.contains(territory1)) {
        m_territories[territory2].neighbors.append(territory1);
    }
}

void MapGraph::removeEdge(const QString &territory1, const QString &territory2)
{
    if (!exists(territory1) || !exists(territory2)) {
        return;
    }

    // Remove bidirectional edge
    m_territories[territory1].neighbors.removeAll(territory2);
    m_territories[territory2].neighbors.removeAll(territory1);
}

// === Spatial Queries ===

QPointF MapGraph::getCentroid(const QString &name) const
{
    if (exists(name)) {
        return m_territories[name].centroid;
    }
    return QPointF(0, 0);
}

Territory MapGraph::getTerritoryById(int id) const
{
    if (m_idToName.contains(id)) {
        return m_territories[m_idToName[id]];
    }
    return Territory();
}

QString MapGraph::getTerritoryNameById(int id) const
{
    return m_idToName.value(id, QString());
}

// === Type Queries ===

bool MapGraph::isSeaTerritory(const QString &name) const
{
    if (exists(name)) {
        return m_territories[name].type == TerritoryType::Sea;
    }
    return false;
}

bool MapGraph::isLandTerritory(const QString &name) const
{
    if (exists(name)) {
        return m_territories[name].type == TerritoryType::Land;
    }
    return false;
}

TerritoryType MapGraph::getType(const QString &name) const
{
    if (exists(name)) {
        return m_territories[name].type;
    }
    return TerritoryType::Land;  // Default to Land if not found
}

int MapGraph::getValue(const QString &name) const
{
    if (exists(name)) {
        return m_territories[name].value;
    }
    return 0;
}

QList<QString> MapGraph::getAdjacentSeaTerritories(const QString &landTerritoryName) const
{
    QList<QString> seaTerritories;

    if (!exists(landTerritoryName)) {
        return seaTerritories;
    }

    // Get all neighbors
    QList<QString> neighbors = getNeighbors(landTerritoryName);

    // Filter for sea territories only
    for (const QString &neighbor : neighbors) {
        if (isSeaTerritory(neighbor)) {
            seaTerritories.append(neighbor);
        }
    }

    return seaTerritories;
}

QList<QString> MapGraph::getConnectedBeachSeaZones(const QString &landTerritory, const QString &seaZone) const
{
    QList<QString> result;

    if (!exists(landTerritory) || !isSeaTerritory(seaZone)) {
        return result;
    }

    // Get neighbors in clockwise order
    QList<QString> neighbors = getNeighbors(landTerritory);

    // Find the index of the seaZone in the neighbor list
    int seaIndex = neighbors.indexOf(seaZone);
    if (seaIndex == -1) {
        // seaZone is not a neighbor of landTerritory
        return result;
    }

    // Always include the original sea zone
    result.append(seaZone);

    // Check the neighbor before (wrap around if needed)
    int prevIndex = (seaIndex - 1 + neighbors.size()) % neighbors.size();
    if (isSeaTerritory(neighbors[prevIndex])) {
        result.append(neighbors[prevIndex]);
    }

    // Check the neighbor after (wrap around if needed)
    int nextIndex = (seaIndex + 1) % neighbors.size();
    if (isSeaTerritory(neighbors[nextIndex])) {
        result.append(neighbors[nextIndex]);
    }

    return result;
}

// === Beach Position Queries ===

QPointF MapGraph::getBeachPosition(const QString &landTerritory, const QString &seaTerritory) const
{
    QString key = landTerritory + "|" + seaTerritory;
    return m_beachPositions.value(key, QPointF(0, 0));
}

bool MapGraph::hasBeachPosition(const QString &landTerritory, const QString &seaTerritory) const
{
    QString key = landTerritory + "|" + seaTerritory;
    return m_beachPositions.contains(key);
}

QList<QString> MapGraph::getSeaZonesAtBeach(const QString &landTerritory, const QPointF &beachPos) const
{
    QList<QString> seaZones;

    // Find all sea zones that share this beach position with the land territory
    for (auto it = m_beachPositions.begin(); it != m_beachPositions.end(); ++it) {
        // Check if this entry is for the same land territory and same position
        if (it.key().startsWith(landTerritory + "|")) {
            QPointF pos = it.value();
            // Use small epsilon for floating point comparison
            if (qAbs(pos.x() - beachPos.x()) < 1.0 && qAbs(pos.y() - beachPos.y()) < 1.0) {
                // Extract sea name from key
                QString seaName = it.key().mid(landTerritory.length() + 1);
                seaZones.append(seaName);
            }
        }
    }

    return seaZones;
}

// === Road Queries ===

QStringList MapGraph::getRoadConnectedTerritories(const QString &startTerritory, const Player *player) const
{
    QStringList result;

    if (!player || !exists(startTerritory)) {
        return result;
    }

    // Build set of territories with cities owned by this player
    QSet<QString> cityTerritories;
    for (City *city : player->getCities()) {
        cityTerritories.insert(city->getTerritoryName());
    }

    // If start territory doesn't have a city, no road connections possible
    if (!cityTerritories.contains(startTerritory)) {
        return result;
    }

    // BFS through road network
    QSet<QString> visited;
    QList<QString> toVisit;

    visited.insert(startTerritory);
    toVisit.append(startTerritory);

    while (!toVisit.isEmpty()) {
        QString current = toVisit.takeFirst();

        // Check all neighbors for road connections
        for (const QString &neighbor : getNeighbors(current)) {
            // Skip if already visited
            if (visited.contains(neighbor)) {
                continue;
            }

            // Skip sea territories (roads only on land)
            if (isSeaTerritory(neighbor)) {
                continue;
            }

            // Skip if player doesn't own this territory
            if (!player->ownsTerritory(neighbor)) {
                continue;
            }

            // Skip if no city at this territory
            if (!cityTerritories.contains(neighbor)) {
                continue;
            }

            // Valid road connection found
            visited.insert(neighbor);
            toVisit.append(neighbor);
            result.append(neighbor);
        }
    }

    return result;
}

QList<QPair<QString, QString>> MapGraph::getRoadSegments(const Player *player) const
{
    QList<QPair<QString, QString>> segments;

    if (!player) {
        return segments;
    }

    // Build set of territories with cities owned by this player
    QSet<QString> cityTerritories;
    for (City *city : player->getCities()) {
        cityTerritories.insert(city->getTerritoryName());
    }

    // Track processed pairs to avoid duplicates
    QSet<QString> processedPairs;

    // For each city territory, check neighbors for road connections
    for (const QString &cityTerritory : cityTerritories) {
        // Player must own the territory
        if (!player->ownsTerritory(cityTerritory)) continue;

        for (const QString &neighbor : getNeighbors(cityTerritory)) {
            // Skip sea territories
            if (isSeaTerritory(neighbor)) continue;

            // Skip if no city at neighbor
            if (!cityTerritories.contains(neighbor)) continue;

            // Skip if player doesn't own neighbor
            if (!player->ownsTerritory(neighbor)) continue;

            // Create sorted pair key to avoid duplicates (A-B same as B-A)
            QString key = (cityTerritory < neighbor)
                ? cityTerritory + "|" + neighbor
                : neighbor + "|" + cityTerritory;

            if (!processedPairs.contains(key)) {
                processedPairs.insert(key);
                segments.append({cityTerritory, neighbor});
            }
        }
    }

    return segments;
}

// === Pathfinding ===

QList<QString> MapGraph::breadthFirstSearch(const QString &from, const QString &to) const
{
    if (!exists(from) || !exists(to)) {
        return QList<QString>();
    }

    if (from == to) {
        return QList<QString>() << from;
    }

    // BFS with parent tracking for path reconstruction
    QMap<QString, QString> parent;
    QSet<QString> visited;
    QQueue<QString> queue;

    queue.enqueue(from);
    visited.insert(from);
    parent[from] = QString();  // Starting node has no parent

    while (!queue.isEmpty()) {
        QString current = queue.dequeue();

        if (current == to) {
            // Reconstruct path from parent map
            QList<QString> path;
            QString node = to;
            while (!node.isEmpty()) {
                path.prepend(node);
                node = parent[node];
            }
            return path;
        }

        // Explore neighbors
        const QList<QString> &neighbors = getNeighbors(current);
        for (const QString &neighbor : neighbors) {
            if (!visited.contains(neighbor)) {
                visited.insert(neighbor);
                parent[neighbor] = current;
                queue.enqueue(neighbor);
            }
        }
    }

    // No path found
    return QList<QString>();
}

QList<QString> MapGraph::findPath(const QString &from, const QString &to) const
{
    return breadthFirstSearch(from, to);
}

int MapGraph::getDistance(const QString &from, const QString &to) const
{
    QList<QString> path = findPath(from, to);
    if (path.isEmpty()) {
        return -1;  // No path exists
    }
    return path.size() - 1;  // Distance is number of edges (nodes - 1)
}

bool MapGraph::isReachable(const QString &from, const QString &to) const
{
    return !findPath(from, to).isEmpty();
}

QList<QString> MapGraph::getTerritoriesWithinDistance(const QString &from, int maxDistance) const
{
    if (!exists(from) || maxDistance < 0) {
        return QList<QString>();
    }

    QList<QString> result;
    QMap<QString, int> distance;
    QSet<QString> visited;
    QQueue<QString> queue;

    queue.enqueue(from);
    visited.insert(from);
    distance[from] = 0;

    while (!queue.isEmpty()) {
        QString current = queue.dequeue();
        int currentDist = distance[current];

        // Add to result if within distance
        if (currentDist <= maxDistance) {
            result.append(current);
        }

        // Don't explore beyond maxDistance
        if (currentDist >= maxDistance) {
            continue;
        }

        // Explore neighbors
        const QList<QString> &neighbors = getNeighbors(current);
        for (const QString &neighbor : neighbors) {
            if (!visited.contains(neighbor)) {
                visited.insert(neighbor);
                distance[neighbor] = currentDist + 1;
                queue.enqueue(neighbor);
            }
        }
    }

    return result;
}

// === Statistics ===

int MapGraph::countByType(TerritoryType type) const
{
    int count = 0;
    for (auto it = m_territories.begin(); it != m_territories.end(); ++it) {
        if (it.value().type == type) {
            count++;
        }
    }
    return count;
}

// === Serialization ===

bool MapGraph::loadFromJson(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        return false;
    }

    return loadFromJsonObject(doc.object());
}

bool MapGraph::saveToJson(const QString &filePath) const
{
    QJsonDocument doc(saveToJsonObject());
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(doc.toJson());
    file.close();

    return true;
}

bool MapGraph::loadFromJsonObject(const QJsonObject &graphObj)
{
    // Clear existing territories
    clear();

    // Load territories
    QJsonArray territoriesArray = graphObj["territories"].toArray();
    for (const QJsonValue &territoryValue : territoriesArray) {
        QJsonObject territoryObj = territoryValue.toObject();

        Territory territory;
        territory.id = territoryObj["id"].toInt();
        territory.name = territoryObj["name"].toString();
        territory.centroid = QPointF(territoryObj["centroidX"].toDouble(), territoryObj["centroidY"].toDouble());
        territory.value = territoryObj["value"].toInt();

        // Load type
        QString typeStr = territoryObj["type"].toString();
        territory.type = (typeStr == "Sea") ? TerritoryType::Sea : TerritoryType::Land;

        // Load neighbors
        QJsonArray neighborsArray = territoryObj["neighbors"].toArray();
        for (const QJsonValue &neighborValue : neighborsArray) {
            territory.neighbors.append(neighborValue.toString());
        }

        m_territories[territory.name] = territory;
        m_idToName[territory.id] = territory.name;
    }

    return true;
}

QJsonObject MapGraph::saveToJsonObject() const
{
    QJsonObject graphObj;

    // Save territories
    QJsonArray territoriesArray;
    for (const Territory &territory : m_territories) {
        QJsonObject territoryObj;
        territoryObj["id"] = territory.id;
        territoryObj["name"] = territory.name;
        territoryObj["centroidX"] = territory.centroid.x();
        territoryObj["centroidY"] = territory.centroid.y();
        territoryObj["value"] = territory.value;
        territoryObj["type"] = (territory.type == TerritoryType::Sea) ? "Sea" : "Land";

        // Save neighbors
        QJsonArray neighborsArray;
        for (const QString &neighbor : territory.neighbors) {
            neighborsArray.append(neighbor);
        }
        territoryObj["neighbors"] = neighborsArray;

        territoriesArray.append(territoryObj);
    }
    graphObj["territories"] = territoriesArray;

    return graphObj;
}
