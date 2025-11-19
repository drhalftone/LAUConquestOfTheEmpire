#ifndef GAMEPIECE_H
#define GAMEPIECE_H

#include <QObject>
#include <QPainter>
#include <QColor>
#include <QString>
#include "common.h"

// Base class for all game pieces
class GamePiece : public QObject
{
    Q_OBJECT

public:
    enum class Type {
        Caesar,
        General,
        Infantry,
        Cavalry,
        Catapult,
        Galley
    };

    // Type prefixes for unique IDs (2 digits)
    static constexpr int TYPE_PREFIX_CAESAR = 10;
    static constexpr int TYPE_PREFIX_GENERAL = 20;
    static constexpr int TYPE_PREFIX_INFANTRY = 30;
    static constexpr int TYPE_PREFIX_CAVALRY = 40;
    static constexpr int TYPE_PREFIX_CATAPULT = 50;
    static constexpr int TYPE_PREFIX_GALLEY = 60;

    explicit GamePiece(QChar player, const Position &position, QObject *parent = nullptr);
    virtual ~GamePiece() = default;

    // Pure virtual function - each piece draws itself
    virtual void paint(QPainter &painter, int x, int y, int width, int height) const = 0;

    // Virtual function for movement rules (can be overridden)
    virtual bool canMoveTo(const Position &from, const Position &to) const;

    // Virtual function for getting piece type
    virtual Type getType() const = 0;

    // Unique identifier
    int getUniqueId() const { return m_uniqueId; }
    QString getSerialNumber() const;  // Returns formatted 5-digit serial number (e.g., "10001")

    // Getters and setters
    QChar getPlayer() const { return m_player; }
    void setPlayer(QChar player) { m_player = player; }
    Position getPosition() const { return m_position; }
    void setPosition(const Position &pos) { m_position = pos; }

    QString getTerritoryName() const { return m_territoryName; }
    void setTerritoryName(const QString &name) { m_territoryName = name; }

    int getMovesRemaining() const { return m_movesRemaining; }
    void setMovesRemaining(int moves) { m_movesRemaining = moves; }

    // Galley tracking - which galley is this piece on (empty if not on a galley)
    QString getOnGalley() const { return m_onGalleySerialNumber; }
    void setOnGalley(const QString &galleySerialNumber) { m_onGalleySerialNumber = galleySerialNumber; }
    void clearGalley() { m_onGalleySerialNumber.clear(); }
    bool isOnGalley() const { return !m_onGalleySerialNumber.isEmpty(); }

    // Static method to reset counter (for testing or new game)
    static void resetCounter();

protected:
    // Generate unique ID based on type prefix and instance counter
    int generateUniqueId(int typePrefix);

    QChar m_player;
    Position m_position;
    QString m_territoryName;           // Name of the territory this piece is in
    int m_movesRemaining;
    int m_uniqueId;                    // Unique 5-digit ID (type prefix + instance number)
    QString m_onGalleySerialNumber;    // Serial number of galley this piece is on (empty if not on galley)

private:
    static int s_instanceCounter;  // Shared counter across all pieces (3-digit max: 000-999)
};

// Caesar piece - leader
class CaesarPiece : public GamePiece
{
    Q_OBJECT

public:
    explicit CaesarPiece(QChar player, const Position &position, QObject *parent = nullptr);

    void paint(QPainter &painter, int x, int y, int width, int height) const override;
    Type getType() const override { return Type::Caesar; }

    // Legion management
    QList<int> getLegion() const { return m_legion; }
    void setLegion(const QList<int> &legion) { m_legion = legion; }
    void addToLegion(int pieceId) { if (!m_legion.contains(pieceId)) m_legion.append(pieceId); }
    void removeFromLegion(int pieceId) { m_legion.removeAll(pieceId); }
    void clearLegion() { m_legion.clear(); }

    // Last territory tracking (for retreat)
    Position getLastTerritory() const { return m_lastTerritory; }
    void setLastTerritory(const Position &pos) { m_lastTerritory = pos; }
    bool hasLastTerritory() const { return m_lastTerritory.row != -1; }
    void clearLastTerritory() { m_lastTerritory = {-1, -1}; }

private:
    QList<int> m_legion;  // List of piece IDs that belong to this Caesar's legion
    Position m_lastTerritory = {-1, -1};  // Previous territory (for retreat purposes)
};

// General piece - commander (numbered 1-5)
class GeneralPiece : public GamePiece
{
    Q_OBJECT

public:
    explicit GeneralPiece(QChar player, const Position &position, int number, QObject *parent = nullptr);

    void paint(QPainter &painter, int x, int y, int width, int height) const override;
    Type getType() const override { return Type::General; }

    int getNumber() const { return m_number; }

    // Legion management
    QList<int> getLegion() const { return m_legion; }
    void setLegion(const QList<int> &legion) { m_legion = legion; }
    void addToLegion(int pieceId) { if (!m_legion.contains(pieceId)) m_legion.append(pieceId); }
    void removeFromLegion(int pieceId) { m_legion.removeAll(pieceId); }
    void clearLegion() { m_legion.clear(); }

    // Last territory tracking (for retreat)
    Position getLastTerritory() const { return m_lastTerritory; }
    void setLastTerritory(const Position &pos) { m_lastTerritory = pos; }
    bool hasLastTerritory() const { return m_lastTerritory.row != -1; }
    void clearLastTerritory() { m_lastTerritory = {-1, -1}; }

    // Capture status
    bool isCaptured() const { return m_capturedBy != '\0'; }
    QChar getCapturedBy() const { return m_capturedBy; }
    void setCapturedBy(QChar player) { m_capturedBy = player; }
    void clearCaptured() { m_capturedBy = '\0'; }

private:
    int m_number;  // 1-5
    QList<int> m_legion;  // List of piece IDs that belong to this General's legion
    Position m_lastTerritory = {-1, -1};  // Previous territory (for retreat purposes)
    QChar m_capturedBy = '\0';  // Player who captured this general ('\0' = not captured)
};

// Infantry piece - basic combat unit
class InfantryPiece : public GamePiece
{
    Q_OBJECT

public:
    explicit InfantryPiece(QChar player, const Position &position, QObject *parent = nullptr);

    void paint(QPainter &painter, int x, int y, int width, int height) const override;
    void paint(QPainter &painter, int x, int y, int width, int height, int count) const;
    Type getType() const override { return Type::Infantry; }
    bool canMoveTo(const Position &from, const Position &to) const override;
};

// Cavalry piece - fast combat unit
class CavalryPiece : public GamePiece
{
    Q_OBJECT

public:
    explicit CavalryPiece(QChar player, const Position &position, QObject *parent = nullptr);

    void paint(QPainter &painter, int x, int y, int width, int height) const override;
    void paint(QPainter &painter, int x, int y, int width, int height, int count) const;
    Type getType() const override { return Type::Cavalry; }
    bool canMoveTo(const Position &from, const Position &to) const override;
};

// Catapult piece - siege weapon
class CatapultPiece : public GamePiece
{
    Q_OBJECT

public:
    explicit CatapultPiece(QChar player, const Position &position, QObject *parent = nullptr);

    void paint(QPainter &painter, int x, int y, int width, int height) const override;
    void paint(QPainter &painter, int x, int y, int width, int height, int count) const;
    Type getType() const override { return Type::Catapult; }
    bool canMoveTo(const Position &from, const Position &to) const override;
};

// Galley piece - naval unit
class GalleyPiece : public GamePiece
{
    Q_OBJECT

public:
    explicit GalleyPiece(QChar player, const Position &position, QObject *parent = nullptr);

    void paint(QPainter &painter, int x, int y, int width, int height) const override;
    void paint(QPainter &painter, int x, int y, int width, int height, int count) const;
    Type getType() const override { return Type::Galley; }
    bool canMoveTo(const Position &from, const Position &to) const override;

    // Legion management
    QList<int> getLegion() const { return m_legion; }
    void setLegion(const QList<int> &legion) { m_legion = legion; }
    void addToLegion(int pieceId) { if (!m_legion.contains(pieceId)) m_legion.append(pieceId); }
    void removeFromLegion(int pieceId) { m_legion.removeAll(pieceId); }
    void clearLegion() { m_legion.clear(); }

    // Last territory tracking (for retreat)
    Position getLastTerritory() const { return m_lastTerritory; }
    void setLastTerritory(const Position &pos) { m_lastTerritory = pos; }
    bool hasLastTerritory() const { return m_lastTerritory.row != -1; }
    void clearLastTerritory() { m_lastTerritory = {-1, -1}; }

private:
    QList<int> m_legion;  // List of piece IDs that belong to this Galley's legion
    Position m_lastTerritory = {-1, -1};  // Previous territory (for retreat purposes)
};

#endif // GAMEPIECE_H
