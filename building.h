#ifndef BUILDING_H
#define BUILDING_H

#include <QObject>
#include <QString>
#include <QPainter>
#include "common.h"

// Base class for buildings
class Building : public QObject
{
    Q_OBJECT

public:
    enum class Type {
        City,
        Road
    };

    explicit Building(QChar owner, const Position &position, const QString &territoryName, QObject *parent = nullptr);
    virtual ~Building() = default;

    // Pure virtual - each building draws itself
    virtual void paint(QPainter &painter, int x, int y, int width, int height) const = 0;
    virtual Type getType() const = 0;

    // Getters and setters
    QChar getOwner() const { return m_owner; }
    void setOwner(QChar owner) { m_owner = owner; }
    Position getPosition() const { return m_position; }
    void setPosition(const Position &pos) { m_position = pos; }

    QString getTerritoryName() const { return m_territoryName; }
    void setTerritoryName(const QString &name) { m_territoryName = name; }

protected:
    QChar m_owner;              // Which player owns this building
    Position m_position;        // Grid position
    QString m_territoryName;    // Territory name where this building is located
};

// City class - can be fortified
class City : public Building
{
    Q_OBJECT

public:
    explicit City(QChar owner, const Position &position, const QString &territoryName, bool fortified = false, QObject *parent = nullptr);

    void paint(QPainter &painter, int x, int y, int width, int height) const override;
    Type getType() const override { return Type::City; }

    // Fortification
    bool isFortified() const { return m_isFortified; }
    void setFortified(bool fortified) { m_isFortified = fortified; }
    void addFortification() { m_isFortified = true; }
    void removeFortification() { m_isFortified = false; }

    // Mark for destruction
    bool isMarkedForDestruction() const { return m_markedForDestruction; }
    void setMarkedForDestruction(bool marked) { m_markedForDestruction = marked; }

private:
    bool m_isFortified;  // Does this city have walls/fortification?
    bool m_markedForDestruction;  // Is this city marked for destruction at end of turn?
};

// Road class - connects territories
class Road : public Building
{
    Q_OBJECT

public:
    explicit Road(QChar owner, const Position &position, const QString &territoryName, QObject *parent = nullptr);

    void paint(QPainter &painter, int x, int y, int width, int height) const override;
    Type getType() const override { return Type::Road; }

    // Road endpoints (for roads connecting two positions)
    Position getFromPosition() const { return m_position; }  // Use base position as "from"
    Position getToPosition() const { return m_toPosition; }
    void setToPosition(const Position &pos) { m_toPosition = pos; }

private:
    Position m_toPosition;  // Second endpoint of the road
};

#endif // BUILDING_H
