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
    , m_markedForDestruction(false)
{
}

void City::paint(QPainter &painter, int x, int y, int width, int height) const
{
    // Choose color based on whether city is marked for destruction
    QColor cityColor;
    if (m_markedForDestruction) {
        cityColor = QColor(200, 0, 0);  // Red for marked cities
    } else {
        cityColor = QColor(150, 100, 50);  // Brown for normal cities
    }

    painter.setPen(Qt::black);
    painter.setBrush(cityColor);

    // Draw small building in top-right corner
    int citySize = width / 6;
    QRect cityRect(x + width - citySize - 2, y + 2, citySize, citySize);
    painter.drawRect(cityRect);

    // Draw roof (triangle)
    QPolygon roof;
    roof << QPoint(x + width - citySize - 2, y + 2)
         << QPoint(x + width - 2, y + 2)
         << QPoint(x + width - citySize/2 - 2, y - citySize/2 + 2);
    painter.drawPolygon(roof);

    // Draw fortification if present (wall around city)
    if (m_isFortified) {
        QColor wallColor = m_markedForDestruction ? QColor(200, 0, 0) : QColor(80, 80, 80);
        painter.setPen(QPen(wallColor, 2)); // Dark gray thick line (or red if marked)
        painter.setBrush(Qt::NoBrush);

        // Draw crenelated wall around the city icon
        int wallMargin = 1;
        QRect wallRect(x + width - citySize - 2 - wallMargin,
                     y + 2 - wallMargin,
                     citySize + wallMargin * 2,
                     citySize + wallMargin * 2);
        painter.drawRect(wallRect);

        // Draw crenelations (small rectangles on top)
        int crenelSize = citySize / 4;
        for (int i = 0; i < 3; ++i) {
            int crenelX = wallRect.left() + i * (citySize / 2);
            painter.fillRect(crenelX, wallRect.top() - crenelSize/2,
                           crenelSize, crenelSize/2, wallColor);
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
