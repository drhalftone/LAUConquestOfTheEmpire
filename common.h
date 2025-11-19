#ifndef COMMON_H
#define COMMON_H

#include <QString>

// Position struct - unified across the codebase
struct Position {
    int row;
    int col;

    bool operator==(const Position &other) const {
        return row == other.row && col == other.col;
    }

    bool operator!=(const Position &other) const {
        return !(*this == other);
    }

    bool operator<(const Position &other) const {
        if (row != other.row) {
            return row < other.row;
        }
        return col < other.col;
    }
};

// Territory location - can be identified by name or position
struct TerritoryLocation {
    QString name;       // Unique territory name (e.g., "Lion", "Tiger")
    Position position;  // Grid position (row, col)

    bool operator==(const TerritoryLocation &other) const {
        return name == other.name && position == other.position;
    }
};

#endif // COMMON_H
