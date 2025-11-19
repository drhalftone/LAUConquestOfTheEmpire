#include "building.h"
#include <QtMath>

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
{
}

void City::paint(QPainter &painter, int x, int y, int width, int height) const
{
    // Draw city as a small square in the center of the tile
    int citySize = qMin(width, height) / 4;
    int centerX = x + width / 2;
    int centerY = y + height / 2;

    QRect cityRect(centerX - citySize/2, centerY - citySize/2, citySize, citySize);

    // City is always dark gray
    painter.setPen(QPen(Qt::black, 2));
    painter.setBrush(QColor(80, 80, 80));
    painter.drawRect(cityRect);

    // If fortified, draw crenellations (battlements) on top
    if (m_isFortified) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(80, 80, 80));

        int crenelSize = citySize / 4;
        for (int i = 0; i < 3; ++i) {
            int crenelX = cityRect.left() + i * (citySize / 2);
            painter.fillRect(crenelX, cityRect.top() - crenelSize/2,
                           crenelSize, crenelSize/2, QColor(80, 80, 80));
        }
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
