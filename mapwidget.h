#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QWidget>
#include <QVector>
#include <QMap>
#include <QMenuBar>
#include "common.h"
#include "mapgraph.h"

// Forward declarations
class Player;

class MapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MapWidget(QWidget *parent = nullptr);

    static constexpr int COLUMNS = 12;
    static constexpr int ROWS = 8;

    enum class TileType {
        Land,
        Sea
    };

    // Note: Position struct is now defined in common.h and used throughout the codebase

    struct TerritoryInfo {
        QString name;    // Animal name
        int value;       // Either 5 or 10
    };

    enum class PieceType {
        Caesar,
        General
    };

    struct Piece {
        PieceType type;
        int generalNumber;  // 1-5 for generals, 0 for caesar
        Position position;
        int movesRemaining;  // 0-2
    };

    QMap<QChar, int> calculateScores() const;

    // Get territory name at position
    QString getTerritoryNameAt(int row, int col) const;

    // Get territory owner at position ('\0' if unowned)
    QChar getTerritoryOwnerAt(int row, int col) const;

    // Get territory tax value at position
    int getTerritoryValueAt(int row, int col) const;

    // Check if a tile is sea
    bool isSeaTerritory(int row, int col) const;

    // Get adjacent sea territories for a given position
    QList<Position> getAdjacentSeaTerritories(const Position &pos) const;

    // Check if there are enemy pieces at position
    bool hasEnemyPiecesAt(int row, int col, QChar currentPlayer) const;

    // Get player color
    QColor getPlayerColor(QChar player) const;

    // Get random home provinces for 6 players
    struct HomeProvinceInfo {
        ::Position position;  // Use global Position from common.h
        QString name;
    };
    QVector<HomeProvinceInfo> getRandomHomeProvinces();

    // Set player list for querying pieces and ownership
    void setPlayers(const QList<Player*> &players) { m_players = players; }

    // Set current player turn index
    void setCurrentPlayerIndex(int index) { m_currentPlayerIndex = index; }
    int getCurrentPlayerIndex() const { return m_currentPlayerIndex; }

    // Set territory data (for loading saved games)
    void setTerritoryAt(int row, int col, const QString &name, int value, bool isLand);
    void clearMap();  // Clear existing map before loading

    // Remove city and fortification at specific position
    void removeCityAt(int row, int col);
    void removeFortificationAt(int row, int col);

    // Update scores display
    void updateScores(const QMap<QChar, int> &scores);

    // Check and create roads between adjacent cities owned by the same player
    void updateRoads();

    // Get all territories reachable via roads from a starting position for a player
    QList<Position> getTerritoriesConnectedByRoad(const Position &startPos, QChar playerId);

    // Check if we're at the start of a turn (no moves made yet)
    bool isAtStartOfTurn() const { return m_isAtStartOfTurn; }
    void setAtStartOfTurn(bool atStart) { m_isAtStartOfTurn = atStart; }

    // === Graph-based Map System (Phase 2) ===
    // Converters between grid Position and graph territory names
    QString positionToTerritoryName(const Position &pos) const;
    Position territoryNameToPosition(const QString &territoryName) const;

    // Access to graph (for future migration)
    MapGraph* getGraph() { return m_graph; }
    const MapGraph* getGraph() const { return m_graph; }

public slots:
    void saveGame();
    void loadGame();
    void showAbout();

signals:
    void scoresChanged();
    void taxesCollected(QChar player, int amount);
    void purchasePhaseNeeded(QChar player, int availableMoney, int inflationMultiplier);
    void itemPlaced(QString itemType);  // Notify placement dialog that an item was placed

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

protected:
    bool event(QEvent *event) override;

private:
    void initializeMap();
    void placeCaesars();
    void assignTerritoryNames();
    bool isInsidePiece(const QPoint &pos, const Position &piecePos, int radius) const;
    bool isValidMove(const Position &from, const Position &to) const;
    Piece* getPieceAt(const QPoint &pos, QChar player);
    QVector<Piece*> getPiecesAtPosition(const Position &pos, QChar player);
    void createMenuBar();

    // Graph-based map system
    void buildGraphFromGrid();  // Convert grid to graph representation

    QMenuBar *m_menuBar;
    QVector<QVector<TileType>> m_tiles;
    QMap<QChar, QVector<Piece>> m_playerPieces;  // Maps 'A'-'F' to their 6 pieces (1 caesar + 5 generals) - DEPRECATED, use m_players
    QVector<QVector<TerritoryInfo>> m_territories;  // Territory info for each tile
    QVector<QVector<QChar>> m_ownership;  // Which player owns each square ('\0' if none) - DEPRECATED, query m_players
    QList<Player*> m_players;  // Reference to player objects for querying pieces and ownership
    int m_tileWidth;
    int m_tileHeight;
    int m_currentPlayerIndex;  // Index of current player in m_players list
    bool m_isAtStartOfTurn;  // True if at start of turn, false if moves have been made
    QMap<QChar, int> m_scores;  // Player scores for display

    // Dragging state
    bool m_dragging;
    Piece *m_draggedPiece;
    QPoint m_dragPosition;
    Position m_originalPosition;  // Store original position when dragging starts

    // Economic tracking
    QMap<QChar, int> m_playerWallets;  // Accumulated wealth for each player
    int m_inflationMultiplier;  // 1, 2, or 3 based on wealth thresholds
    int m_highestWallet;  // Track highest wallet to trigger inflation

    // Home provinces and buildings
    QMap<QChar, Position> m_homeProvinces;  // Each player's home province (where Caesar started)
    QVector<QVector<bool>> m_hasCity;  // Does this tile have a city?
    QVector<QVector<bool>> m_hasFortification;  // Does this tile have a fortification (wall)?

    // Troop counts per tile (per player)
    struct TroopCounts {
        int infantry = 0;
        int cavalry = 0;
        int catapult = 0;
        int galley = 0;
    };
    QMap<QChar, QVector<QVector<TroopCounts>>> m_playerTroops;  // Maps player to 2D grid of troop counts

    // Graph-based map system
    MapGraph *m_graph;  // Graph representation of the map (coexists with grid during migration)
};

#endif // MAPWIDGET_H
