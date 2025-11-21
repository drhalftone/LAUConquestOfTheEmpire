#include "gamepiece.h"
#include <QtMath>
#include <QPixmap>

// Helper function to get player color (gray for black player so icon is visible)
static QColor getPlayerColor(QChar player)
{
    switch (player.toLatin1()) {
        case 'A': return Qt::red;
        case 'B': return Qt::green;
        case 'C': return Qt::blue;
        case 'D': return Qt::yellow;
        case 'E': return QColor(128, 128, 128); // Gray instead of black
        case 'F': return QColor(255, 165, 0); // Orange
        default: return Qt::gray;
    }
}

// Helper function to draw a piece icon inside a colored circle
static void drawPieceWithIcon(QPainter &painter, int centerX, int centerY, int radius,
                               QChar player, const QString &iconPath)
{
    QColor playerColor = getPlayerColor(player);

    // Draw circle with player color fill and black border
    painter.setBrush(playerColor);
    painter.setPen(QPen(Qt::black, 2));
    painter.drawEllipse(QPoint(centerX, centerY), radius, radius);

    // Load and draw the icon inside the circle
    QPixmap icon(iconPath);
    if (!icon.isNull()) {
        // Scale icon to fit inside the circle (about 70% of diameter)
        int iconSize = static_cast<int>(radius * 1.4);
        QPixmap scaledIcon = icon.scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // Center the icon in the circle
        int iconX = centerX - scaledIcon.width() / 2;
        int iconY = centerY - scaledIcon.height() / 2;
        painter.drawPixmap(iconX, iconY, scaledIcon);
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
    // Draw larger circle for Caesar with icon
    int radius = qMin(width, height) * 0.35;
    int centerX = x + width / 2;
    int centerY = y + height / 2;

    drawPieceWithIcon(painter, centerX, centerY, radius, m_player, ":/images/ceasarIcon.png");
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
    // Draw smaller circle for General with icon
    int radius = qMin(width, height) * 0.25;
    int centerX = x + width / 2;
    int centerY = y + height / 2;

    drawPieceWithIcon(painter, centerX, centerY, radius, m_player, ":/images/generalIcon.png");
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
    // Draw circle with icon at bottom-left corner
    int radius = qMin(width, height) / 7;
    int centerX = x + radius + 2;
    int centerY = y + height - radius - 2;

    drawPieceWithIcon(painter, centerX, centerY, radius, m_player, ":/images/infantryIcon.png");
}

void InfantryPiece::paint(QPainter &painter, int x, int y, int width, int height, int count) const
{
    // Draw circle with icon at bottom-left corner
    int radius = qMin(width, height) / 7;
    int centerX = x + radius + 2;
    int centerY = y + height - radius - 2;

    drawPieceWithIcon(painter, centerX, centerY, radius, m_player, ":/images/infantryIcon.png");

    // Draw count number overlay
    QColor textColor = Qt::white;
    painter.setPen(QPen(Qt::black, 2));  // Black outline for visibility
    QFont font = painter.font();
    font.setPointSize(qMax(6, radius / 2));
    font.setBold(true);
    painter.setFont(font);

    // Draw text with outline effect
    QRect textRect(centerX - radius, centerY - radius, radius * 2, radius * 2);
    painter.drawText(textRect.adjusted(-1, -1, -1, -1), Qt::AlignCenter, QString::number(count));
    painter.drawText(textRect.adjusted(1, -1, 1, -1), Qt::AlignCenter, QString::number(count));
    painter.drawText(textRect.adjusted(-1, 1, -1, 1), Qt::AlignCenter, QString::number(count));
    painter.drawText(textRect.adjusted(1, 1, 1, 1), Qt::AlignCenter, QString::number(count));
    painter.setPen(textColor);
    painter.drawText(textRect, Qt::AlignCenter, QString::number(count));
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
    // Draw circle with icon at bottom-left corner
    int radius = qMin(width, height) / 7;
    int centerX = x + radius + 2;
    int centerY = y + height - radius - 2;

    drawPieceWithIcon(painter, centerX, centerY, radius, m_player, ":/images/cavalryIcon.png");
}

void CavalryPiece::paint(QPainter &painter, int x, int y, int width, int height, int count) const
{
    // Draw circle with icon at bottom-left corner
    int radius = qMin(width, height) / 7;
    int centerX = x + radius + 2;
    int centerY = y + height - radius - 2;

    drawPieceWithIcon(painter, centerX, centerY, radius, m_player, ":/images/cavalryIcon.png");

    // Draw count number overlay
    QColor textColor = Qt::white;
    painter.setPen(QPen(Qt::black, 2));  // Black outline for visibility
    QFont font = painter.font();
    font.setPointSize(qMax(6, radius / 2));
    font.setBold(true);
    painter.setFont(font);

    // Draw text with outline effect
    QRect textRect(centerX - radius, centerY - radius, radius * 2, radius * 2);
    painter.drawText(textRect.adjusted(-1, -1, -1, -1), Qt::AlignCenter, QString::number(count));
    painter.drawText(textRect.adjusted(1, -1, 1, -1), Qt::AlignCenter, QString::number(count));
    painter.drawText(textRect.adjusted(-1, 1, -1, 1), Qt::AlignCenter, QString::number(count));
    painter.drawText(textRect.adjusted(1, 1, 1, 1), Qt::AlignCenter, QString::number(count));
    painter.setPen(textColor);
    painter.drawText(textRect, Qt::AlignCenter, QString::number(count));
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
    // Draw circle with icon at bottom-left corner
    int radius = qMin(width, height) / 7;
    int centerX = x + radius + 2;
    int centerY = y + height - radius - 2;

    drawPieceWithIcon(painter, centerX, centerY, radius, m_player, ":/images/catapultIcon.png");
}

void CatapultPiece::paint(QPainter &painter, int x, int y, int width, int height, int count) const
{
    // Draw circle with icon at bottom-left corner
    int radius = qMin(width, height) / 7;
    int centerX = x + radius + 2;
    int centerY = y + height - radius - 2;

    drawPieceWithIcon(painter, centerX, centerY, radius, m_player, ":/images/catapultIcon.png");

    // Draw count number overlay
    QColor textColor = Qt::white;
    painter.setPen(QPen(Qt::black, 2));  // Black outline for visibility
    QFont font = painter.font();
    font.setPointSize(qMax(6, radius / 2));
    font.setBold(true);
    painter.setFont(font);

    // Draw text with outline effect
    QRect textRect(centerX - radius, centerY - radius, radius * 2, radius * 2);
    painter.drawText(textRect.adjusted(-1, -1, -1, -1), Qt::AlignCenter, QString::number(count));
    painter.drawText(textRect.adjusted(1, -1, 1, -1), Qt::AlignCenter, QString::number(count));
    painter.drawText(textRect.adjusted(-1, 1, -1, 1), Qt::AlignCenter, QString::number(count));
    painter.drawText(textRect.adjusted(1, 1, 1, 1), Qt::AlignCenter, QString::number(count));
    painter.setPen(textColor);
    painter.drawText(textRect, Qt::AlignCenter, QString::number(count));
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
    // Draw circle with icon at bottom-left corner
    int radius = qMin(width, height) / 7;
    int centerX = x + radius + 2;
    int centerY = y + height - radius - 2;

    drawPieceWithIcon(painter, centerX, centerY, radius, m_player, ":/images/galleyIcon.png");
}

void GalleyPiece::paint(QPainter &painter, int x, int y, int width, int height, int count) const
{
    // Draw circle with icon at bottom-left corner
    int radius = qMin(width, height) / 7;
    int centerX = x + radius + 2;
    int centerY = y + height - radius - 2;

    drawPieceWithIcon(painter, centerX, centerY, radius, m_player, ":/images/galleyIcon.png");

    // Draw count number overlay
    QColor textColor = Qt::white;
    painter.setPen(QPen(Qt::black, 2));  // Black outline for visibility
    QFont font = painter.font();
    font.setPointSize(qMax(6, radius / 2));
    font.setBold(true);
    painter.setFont(font);

    // Draw text with outline effect
    QRect textRect(centerX - radius, centerY - radius, radius * 2, radius * 2);
    painter.drawText(textRect.adjusted(-1, -1, -1, -1), Qt::AlignCenter, QString::number(count));
    painter.drawText(textRect.adjusted(1, -1, 1, -1), Qt::AlignCenter, QString::number(count));
    painter.drawText(textRect.adjusted(-1, 1, -1, 1), Qt::AlignCenter, QString::number(count));
    painter.drawText(textRect.adjusted(1, 1, 1, 1), Qt::AlignCenter, QString::number(count));
    painter.setPen(textColor);
    painter.drawText(textRect, Qt::AlignCenter, QString::number(count));
}

bool GalleyPiece::canMoveTo(const Position &from, const Position &to) const
{
    // Galleys can only move on water tiles (implement later)
    return GamePiece::canMoveTo(from, to);
}
