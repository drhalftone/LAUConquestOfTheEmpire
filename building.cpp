#include "building.h"
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

// ========== Building Base Class ==========

Building::Building(QChar owner, const Position &position, const QString &territoryName, QObject *parent)
    : QObject(parent)
    , m_owner(owner)
    , m_position(position)
    , m_territoryName(territoryName)
{
}

// ========== City Class ==========

City::City(QChar owner, const Position &position, const QString &territoryName, bool fortified, QObject *parent)
    : Building(owner, position, territoryName, parent)
    , m_isFortified(fortified)
    , m_markedForDestruction(false)
{
}

void City::paint(QPainter &painter, int x, int y, int width, int height) const
{
    // Draw city icon in top-right corner (no colored circle background)
    int iconSize = qMin(width, height) / 3;
    int iconX = x + width - iconSize - 2;
    int iconY = y + 2;

    // Choose icon based on fortification status
    QString iconPath = m_isFortified ? ":/images/walledCityIcon.png" : ":/images/newCityIcon.png";

    // Load and draw the icon
    QPixmap icon(iconPath);
    if (!icon.isNull()) {
        QPixmap scaledIcon = icon.scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawPixmap(iconX, iconY, scaledIcon);
    }

    // If marked for destruction, draw a red X over the icon
    if (m_markedForDestruction) {
        painter.setPen(QPen(QColor(200, 0, 0), 3));
        painter.drawLine(iconX, iconY, iconX + iconSize, iconY + iconSize);
        painter.drawLine(iconX + iconSize, iconY, iconX, iconY + iconSize);
    }
}

// ========== Road Class ==========

Road::Road(QChar owner, const Position &position, const QString &territoryName, QObject *parent)
    : Building(owner, position, territoryName, parent)
    , m_toPosition({-1, -1})  // Initialize to invalid position
{
}

void Road::paint(QPainter &painter, int x, int y, int width, int height) const
{
    // Draw road as a brown line across the tile
    painter.setPen(QPen(QColor(139, 90, 43), 4));

    // Draw a cross pattern for the road
    int centerX = x + width / 2;
    int centerY = y + height / 2;

    // Horizontal line
    painter.drawLine(x, centerY, x + width, centerY);

    // Vertical line
    painter.drawLine(centerX, y, centerX, y + height);
}
