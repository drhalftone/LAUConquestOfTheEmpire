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

    explicit GamePiece(QChar player, const QString &territoryName, QObject *parent = nullptr);
    virtual ~GamePiece() = default;

    // Pure virtual function - each piece draws itself
    virtual void paint(QPainter &painter, int x, int y, int width, int height) const = 0;

    // Virtual function for getting piece type
    virtual Type getType() const = 0;

    // Unique identifier
    int getUniqueId() const { return m_uniqueId; }
    QString getSerialNumber() const;  // Returns formatted 5-digit serial number (e.g., "10001")

    // Getters and setters
    QChar getPlayer() const { return m_player; }
    void setPlayer(QChar player) { m_player = player; }

    QString getTerritoryName() const { return m_territoryName; }
    void setTerritoryName(const QString &name) { m_territoryName = name; }

    double getMovesRemaining() const { return m_movesRemaining; }
    void setMovesRemaining(double moves) { m_movesRemaining = moves; }

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
    QString m_territoryName;           // Name of the territory this piece is in
    double m_movesRemaining;
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
    explicit CaesarPiece(QChar player, const QString &territoryName, QObject *parent = nullptr);

    void paint(QPainter &painter, int x, int y, int width, int height) const override;
    Type getType() const override { return Type::Caesar; }

    // Legion management
    QList<int> getLegion() const { return m_legion; }
    void setLegion(const QList<int> &legion) { m_legion = legion; }
    void addToLegion(int pieceId) { if (!m_legion.contains(pieceId)) m_legion.append(pieceId); }
    void removeFromLegion(int pieceId) { m_legion.removeAll(pieceId); }
    void clearLegion() { m_legion.clear(); }

    // Last territory tracking (for retreat)
    QString getLastTerritoryName() const { return m_lastTerritoryName; }
    void setLastTerritoryName(const QString &name) { m_lastTerritoryName = name; }
    bool hasLastTerritory() const { return !m_lastTerritoryName.isEmpty(); }
    void clearLastTerritory() { m_lastTerritoryName.clear(); }

private:
    QList<int> m_legion;  // List of piece IDs that belong to this Caesar's legion
    QString m_lastTerritoryName;  // Previous territory name (for retreat)
};

// General piece - commander (numbered 1-5)
class GeneralPiece : public GamePiece
{
    Q_OBJECT

public:
    explicit GeneralPiece(QChar player, const QString &territoryName, int number, QObject *parent = nullptr);

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
    QString getLastTerritoryName() const { return m_lastTerritoryName; }
    void setLastTerritoryName(const QString &name) { m_lastTerritoryName = name; }
    bool hasLastTerritory() const { return !m_lastTerritoryName.isEmpty(); }
    void clearLastTerritory() { m_lastTerritoryName.clear(); }

    // Capture status
    bool isCaptured() const { return m_capturedBy != '\0'; }
    QChar getCapturedBy() const { return m_capturedBy; }
    void setCapturedBy(QChar player) { m_capturedBy = player; }
    void clearCaptured() { m_capturedBy = '\0'; }

private:
    int m_number;  // 1-5
    QList<int> m_legion;  // List of piece IDs that belong to this General's legion
    QString m_lastTerritoryName;  // Previous territory name (for retreat)
    QChar m_capturedBy = '\0';  // Player who captured this general ('\0' = not captured)
};

// Infantry piece - basic combat unit
class InfantryPiece : public GamePiece
{
    Q_OBJECT

public:
    explicit InfantryPiece(QChar player, const QString &territoryName, QObject *parent = nullptr);

    void paint(QPainter &painter, int x, int y, int width, int height) const override;
    void paint(QPainter &painter, int x, int y, int width, int height, int count) const;
    Type getType() const override { return Type::Infantry; }
};

// Cavalry piece - fast combat unit
class CavalryPiece : public GamePiece
{
    Q_OBJECT

public:
    explicit CavalryPiece(QChar player, const QString &territoryName, QObject *parent = nullptr);

    void paint(QPainter &painter, int x, int y, int width, int height) const override;
    void paint(QPainter &painter, int x, int y, int width, int height, int count) const;
    Type getType() const override { return Type::Cavalry; }
};

// Catapult piece - siege weapon
class CatapultPiece : public GamePiece
{
    Q_OBJECT

public:
    explicit CatapultPiece(QChar player, const QString &territoryName, QObject *parent = nullptr);

    void paint(QPainter &painter, int x, int y, int width, int height) const override;
    void paint(QPainter &painter, int x, int y, int width, int height, int count) const;
    Type getType() const override { return Type::Catapult; }
};

// Galley piece - naval unit
class GalleyPiece : public GamePiece
{
    Q_OBJECT

public:
    explicit GalleyPiece(QChar player, const QString &territoryName, QObject *parent = nullptr);

    void paint(QPainter &painter, int x, int y, int width, int height) const override;
    void paint(QPainter &painter, int x, int y, int width, int height, int count) const;
    Type getType() const override { return Type::Galley; }

    // Legion management
    QList<int> getLegion() const { return m_legion; }
    void setLegion(const QList<int> &legion) { m_legion = legion; }
    void addToLegion(int pieceId) { if (!m_legion.contains(pieceId)) m_legion.append(pieceId); }
    void removeFromLegion(int pieceId) { m_legion.removeAll(pieceId); }
    void clearLegion() { m_legion.clear(); }

    // Last territory tracking (for retreat)
    QString getLastTerritoryName() const { return m_lastTerritoryName; }
    void setLastTerritoryName(const QString &name) { m_lastTerritoryName = name; }
    bool hasLastTerritory() const { return !m_lastTerritoryName.isEmpty(); }
    void clearLastTerritory() { m_lastTerritoryName.clear(); }

    // Transport tracking (one legion per galley per turn)
    bool hasTransportedThisTurn() const { return m_hasTransportedThisTurn; }
    void setTransportedThisTurn(bool transported) { m_hasTransportedThisTurn = transported; }
    void resetTransportFlag() { m_hasTransportedThisTurn = false; }

    // Track which leader is currently aboard (0 = none)
    int getLeaderAboard() const { return m_leaderAboard; }
    void setLeaderAboard(int leaderId) { m_leaderAboard = leaderId; }
    bool hasLeaderAboard() const { return m_leaderAboard != 0; }

    // Movement tracking for 2-movement system
    QString getLastSeaZone() const { return m_lastSeaZone; }
    void setLastSeaZone(const QString &seaZone) { m_lastSeaZone = seaZone; }
    bool hasLastSeaZone() const { return !m_lastSeaZone.isEmpty(); }
    void clearLastSeaZone() { m_lastSeaZone.clear(); }

    bool hasDockedThisTurn() const { return m_hasDockedThisTurn; }
    void setDockedThisTurn(bool docked) { m_hasDockedThisTurn = docked; }

    QString getDockedCoast() const { return m_dockedCoast; }
    void setDockedCoast(const QString &coast) { m_dockedCoast = coast; }
    void clearDockedCoast() { m_dockedCoast.clear(); }
    bool isDockedOnCoast() const { return !m_dockedCoast.isEmpty(); }

    // Check if galley is currently beached (on land territory)
    bool isBeached() const;

private:
    QList<int> m_legion;  // List of piece IDs that belong to this Galley's legion
    QString m_lastTerritoryName;  // Previous territory name (for retreat)
    bool m_hasTransportedThisTurn = false;  // True if galley has transported a legion this turn
    int m_leaderAboard = 0;  // Unique ID of leader currently aboard (0 = none)
    // Movement tracking fields
    QString m_lastSeaZone;  // Sea zone galley came from (for same-turn docking rule and beach positioning)
    bool m_hasDockedThisTurn = false;  // Whether galley landed on coast this turn
    QString m_dockedCoast;  // Which coast/province galley is docked at (empty if at sea)
};

#endif // GAMEPIECE_H
