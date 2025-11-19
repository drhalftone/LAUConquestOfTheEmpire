#include "mapgraph.h"
#include <QQueue>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

MapGraph::MapGraph()
{
    // Constructor - empty graph initially
}

// === Territory Management ===

void MapGraph::addTerritory(const Territory &territory)
{
    m_territories[territory.name] = territory;
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

    // Remove the territory itself
    m_territories.remove(name);
}

void MapGraph::clear()
{
    m_territories.clear();
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

QString MapGraph::getTerritoryAt(const QPointF &point) const
{
    // Check each territory's boundary polygon
    for (auto it = m_territories.begin(); it != m_territories.end(); ++it) {
        const Territory &territory = it.value();
        if (!territory.boundary.isEmpty() && territory.boundary.containsPoint(point, Qt::OddEvenFill)) {
            return territory.name;
        }
    }

    // No territory contains this point
    return QString();
}

QPointF MapGraph::getCentroid(const QString &name) const
{
    if (exists(name)) {
        return m_territories[name].centroid;
    }
    return QPointF(0, 0);
}

QPolygonF MapGraph::getBoundary(const QString &name) const
{
    if (exists(name)) {
        return m_territories[name].boundary;
    }
    return QPolygonF();
}

QPointF MapGraph::getLabelPosition(const QString &name) const
{
    if (exists(name)) {
        const Territory &territory = m_territories[name];
        // Use label position if set, otherwise use centroid
        if (territory.labelPosition != QPointF(0, 0)) {
            return territory.labelPosition;
        }
        return territory.centroid;
    }
    return QPointF(0, 0);
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
    return TerritoryType::Impassable;
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
    // TODO: Implement JSON loading in Phase 4
    // For now, return false
    Q_UNUSED(filePath);
    return false;
}

bool MapGraph::saveToJson(const QString &filePath) const
{
    // TODO: Implement JSON saving in Phase 4
    // For now, return false
    Q_UNUSED(filePath);
    return false;
}
