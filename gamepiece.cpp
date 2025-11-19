#include "gamepiece.h"
#include <QtMath>

// Helper function to get player color
static QColor getPlayerColor(QChar player)
{
    switch (player.toLatin1()) {
        case 'A': return Qt::red;
        case 'B': return Qt::green;
        case 'C': return Qt::blue;
        case 'D': return Qt::yellow;
        case 'E': return Qt::black;
        case 'F': return QColor(255, 165, 0); // Orange
        default: return Qt::gray;
    }
}

// ========== GamePiece Base Class ==========

// Initialize static counter
int GamePiece::s_instanceCounter = 0;

GamePiece::GamePiece(QChar player, const Position &position, QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_position(position)
    , m_territoryName("")
    , m_movesRemaining(2)
    , m_uniqueId(0)  // Will be set by subclass
    , m_onGalleySerialNumber("")  // Not on a galley initially
{
}

int GamePiece::generateUniqueId(int typePrefix)
{
    // Increment the static counter
    s_instanceCounter++;

    // Wrap counter at 1000 (keep it 3 digits: 001-999)
    if (s_instanceCounter >= 1000) {
        s_instanceCounter = 1;
    }

    // Combine type prefix (2 digits) with instance counter (3 digits)
    // Example: Caesar (10) + instance 1 = 10001
    return (typePrefix * 1000) + s_instanceCounter;
}

QString GamePiece::getSerialNumber() const
{
    // Format as 5-digit string with leading zeros
    return QString("%1").arg(m_uniqueId, 5, 10, QChar('0'));
}

void GamePiece::resetCounter()
{
    s_instanceCounter = 0;
}

bool GamePiece::canMoveTo(const Position &from, const Position &to) const
{
    // Default: pieces can move up to 2 squares orthogonally
    int rowDiff = qAbs(to.row - from.row);
    int colDiff = qAbs(to.col - from.col);
    int totalDistance = rowDiff + colDiff;

    return totalDistance <= m_movesRemaining;
}

// ========== CaesarPiece ==========

CaesarPiece::CaesarPiece(QChar player, const Position &position, QObject *parent)
    : GamePiece(player, position, parent)
{
    m_uniqueId = generateUniqueId(TYPE_PREFIX_CAESAR);
    m_movesRemaining = 2;  // Caesars can move 2 provinces
}

void CaesarPiece::paint(QPainter &painter, int x, int y, int width, int height) const
{
    QColor playerColor = getPlayerColor(m_player);

    // Draw larger circle for Caesar
    int radius = qMin(width, height) * 0.35;
    int centerX = x + width / 2;
    int centerY = y + height / 2;

    painter.setBrush(playerColor);
    painter.setPen(QPen(Qt::black, 2));
    painter.drawEllipse(QPoint(centerX, centerY), radius, radius);

    // Draw player letter
    QColor textColor = (m_player == 'E' || m_player == 'C') ? Qt::white : Qt::black;
    painter.setPen(textColor);

    QFont font = painter.font();
    font.setPointSize(qMax(6, static_cast<int>(radius * 0.7)));
    font.setBold(true);
    painter.setFont(font);

    painter.drawText(QRect(centerX - radius, centerY - radius, radius * 2, radius * 2),
                    Qt::AlignCenter, QString(m_player));
}

// ========== GeneralPiece ==========

GeneralPiece::GeneralPiece(QChar player, const Position &position, int number, QObject *parent)
    : GamePiece(player, position, parent)
    , m_number(number)
{
    m_uniqueId = generateUniqueId(TYPE_PREFIX_GENERAL);
    m_movesRemaining = 2;  // Generals can move 2 provinces
}

void GeneralPiece::paint(QPainter &painter, int x, int y, int width, int height) const
{
    QColor playerColor = getPlayerColor(m_player);

    // Draw smaller circle for General
    int radius = qMin(width, height) * 0.2;
    int centerX = x + width / 2;
    int centerY = y + height / 2;

    painter.setBrush(playerColor);
    painter.setPen(QPen(Qt::black, 2));
    painter.drawEllipse(QPoint(centerX, centerY), radius, radius);

    // Draw general number
    QColor textColor = (m_player == 'E' || m_player == 'C') ? Qt::white : Qt::black;
    painter.setPen(textColor);

    QFont font = painter.font();
    font.setPointSize(qMax(6, static_cast<int>(radius * 0.7)));
    font.setBold(true);
    painter.setFont(font);

    painter.drawText(QRect(centerX - radius, centerY - radius, radius * 2, radius * 2),
                    Qt::AlignCenter, QString::number(m_number));
}

// ========== InfantryPiece ==========

InfantryPiece::InfantryPiece(QChar player, const Position &position, QObject *parent)
    : GamePiece(player, position, parent)
{
    m_uniqueId = generateUniqueId(TYPE_PREFIX_INFANTRY);
    m_movesRemaining = 1;  // Infantry can move 1 province (needs general/caesar to move)
}

void InfantryPiece::paint(QPainter &painter, int x, int y, int width, int height) const
{
    // White square with dark gray border
    int size = qMin(width, height) / 7;
    int posX = x + 2;
    int posY = y + height - size - 2;

    painter.setPen(QPen(QColor(80, 80, 80), 2));  // Dark gray border
    painter.setBrush(Qt::white);
    QRect rect(posX, posY, size, size);
    painter.drawRect(rect);

    // Draw "I" for Infantry
    painter.setPen(Qt::black);
    QFont font = painter.font();
    font.setPointSize(qMax(6, size / 3));
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter, "I");
}

void InfantryPiece::paint(QPainter &painter, int x, int y, int width, int height, int count) const
{
    // White square with dark gray border
    int size = qMin(width, height) / 7;
    int posX = x + 2;
    int posY = y + height - size - 2;

    painter.setPen(QPen(QColor(80, 80, 80), 2));  // Dark gray border
    painter.setBrush(Qt::white);
    QRect rect(posX, posY, size, size);
    painter.drawRect(rect);

    // Draw count number for Infantry
    painter.setPen(Qt::black);
    QFont font = painter.font();
    font.setPointSize(qMax(6, size / 3));
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter, QString::number(count));
}

bool InfantryPiece::canMoveTo(const Position &from, const Position &to) const
{
    // Infantry needs a commander to move (for now, use default)
    return GamePiece::canMoveTo(from, to);
}

// ========== CavalryPiece ==========

CavalryPiece::CavalryPiece(QChar player, const Position &position, QObject *parent)
    : GamePiece(player, position, parent)
{
    m_uniqueId = generateUniqueId(TYPE_PREFIX_CAVALRY);
    m_movesRemaining = 2;  // Cavalry can move 2 provinces (or more, needs general/caesar)
}

void CavalryPiece::paint(QPainter &painter, int x, int y, int width, int height) const
{
    // Yellow square with brown border
    int size = qMin(width, height) / 7;
    int posX = x + 2;
    int posY = y + height - size - 2;

    painter.setPen(QPen(QColor(139, 90, 43), 2));  // Brown border
    painter.setBrush(Qt::yellow);
    QRect rect(posX, posY, size, size);
    painter.drawRect(rect);

    // Draw "C" for Cavalry
    painter.setPen(Qt::black);
    QFont font = painter.font();
    font.setPointSize(qMax(6, size / 3));
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter, "C");
}

void CavalryPiece::paint(QPainter &painter, int x, int y, int width, int height, int count) const
{
    // Yellow square with brown border
    int size = qMin(width, height) / 7;
    int posX = x + 2;
    int posY = y + height - size - 2;

    painter.setPen(QPen(QColor(139, 90, 43), 2));  // Brown border
    painter.setBrush(Qt::yellow);
    QRect rect(posX, posY, size, size);
    painter.drawRect(rect);

    // Draw count number for Cavalry
    painter.setPen(Qt::black);
    QFont font = painter.font();
    font.setPointSize(qMax(6, size / 3));
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter, QString::number(count));
}

bool CavalryPiece::canMoveTo(const Position &from, const Position &to) const
{
    // Cavalry might have special movement rules (for now, use default)
    return GamePiece::canMoveTo(from, to);
}

// ========== CatapultPiece ==========

CatapultPiece::CatapultPiece(QChar player, const Position &position, QObject *parent)
    : GamePiece(player, position, parent)
{
    m_uniqueId = generateUniqueId(TYPE_PREFIX_CATAPULT);
    m_movesRemaining = 1;  // Catapults can move 1 province (needs general/caesar to move)
}

void CatapultPiece::paint(QPainter &painter, int x, int y, int width, int height) const
{
    // Light magenta square with dark magenta border
    int size = qMin(width, height) / 7;
    int posX = x + 2;
    int posY = y + height - size - 2;

    painter.setPen(QPen(QColor(139, 0, 139), 2));  // Dark magenta border
    painter.setBrush(QColor(255, 192, 255));  // Light magenta
    QRect rect(posX, posY, size, size);
    painter.drawRect(rect);

    // Draw "K" for Katapult
    painter.setPen(Qt::black);
    QFont font = painter.font();
    font.setPointSize(qMax(6, size / 3));
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter, "K");
}

void CatapultPiece::paint(QPainter &painter, int x, int y, int width, int height, int count) const
{
    // Light magenta square with dark magenta border
    int size = qMin(width, height) / 7;
    int posX = x + 2;
    int posY = y + height - size - 2;

    painter.setPen(QPen(QColor(139, 0, 139), 2));  // Dark magenta border
    painter.setBrush(QColor(255, 192, 255));  // Light magenta
    QRect rect(posX, posY, size, size);
    painter.drawRect(rect);

    // Draw count number for Catapult
    painter.setPen(Qt::black);
    QFont font = painter.font();
    font.setPointSize(qMax(6, size / 3));
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter, QString::number(count));
}

bool CatapultPiece::canMoveTo(const Position &from, const Position &to) const
{
    // Catapults might move slower (for now, use default)
    return GamePiece::canMoveTo(from, to);
}

// ========== GalleyPiece ==========

GalleyPiece::GalleyPiece(QChar player, const Position &position, QObject *parent)
    : GamePiece(player, position, parent)
{
    m_uniqueId = generateUniqueId(TYPE_PREFIX_GALLEY);
    m_movesRemaining = 2;  // Galleys can move 2 sea provinces independently
}

void GalleyPiece::paint(QPainter &painter, int x, int y, int width, int height) const
{
    // Light blue square with dark blue border
    int size = qMin(width, height) / 7;
    int posX = x + 2;
    int posY = y + height - size - 2;

    painter.setPen(QPen(QColor(0, 0, 139), 2));  // Dark blue border
    painter.setBrush(QColor(173, 216, 230));  // Light blue
    QRect rect(posX, posY, size, size);
    painter.drawRect(rect);

    // Draw "G" for Galley
    painter.setPen(Qt::black);
    QFont font = painter.font();
    font.setPointSize(qMax(6, size / 3));
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter, "G");
}

void GalleyPiece::paint(QPainter &painter, int x, int y, int width, int height, int count) const
{
    // Light blue square with dark blue border
    int size = qMin(width, height) / 7;
    int posX = x + 2;
    int posY = y + height - size - 2;

    painter.setPen(QPen(QColor(0, 0, 139), 2));  // Dark blue border
    painter.setBrush(QColor(173, 216, 230));  // Light blue
    QRect rect(posX, posY, size, size);
    painter.drawRect(rect);

    // Draw count number for Galley
    painter.setPen(Qt::black);
    QFont font = painter.font();
    font.setPointSize(qMax(6, size / 3));
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter, QString::number(count));
}

bool GalleyPiece::canMoveTo(const Position &from, const Position &to) const
{
    // Galleys can only move on water tiles (implement later)
    return GamePiece::canMoveTo(from, to);
}
