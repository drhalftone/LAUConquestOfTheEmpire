#ifndef PLAYERINFOWIDGET_H
#define PLAYERINFOWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>
#include "player.h"
#include "mapwidget.h"

class PlayerInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlayerInfoWidget(QWidget *parent = nullptr);
    ~PlayerInfoWidget();

    // Add players to display
    void addPlayer(Player *player);
    void setPlayers(const QList<Player*> &players);

    // Set map widget reference for territory lookups
    void setMapWidget(MapWidget *mapWidget) { m_mapWidget = mapWidget; }

    // Update display for specific player
    void updatePlayerInfo(Player *player);
    void updateAllPlayers();

    // Handle right-click on territory in map
    void handleTerritoryRightClick(const QString &territoryName, const QPoint &globalPos, QChar currentPlayer);

signals:
    void pieceMoved(int fromRow, int fromCol, int toRow, int toCol);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onEndTurnClicked();

private:
    // Create a tab for a single player
    QWidget* createPlayerTab(Player *player);

    // Create sections for player tab
    QGroupBox* createBasicInfoSection(Player *player);
    QGroupBox* createPiecesSection(Player *player);
    QGroupBox* createTerritoriesSection(Player *player);
    QGroupBox* createEconomicsSection(Player *player);
    QGroupBox* createCapturedGeneralsSection(Player *player);

    // Create global captured generals section
    QGroupBox* createAllCapturedGeneralsSection();
    void updateCapturedGeneralsTable();

    // Context menu handlers
    void showCaesarContextMenu(CaesarPiece *piece, const QPoint &pos);
    void showGeneralContextMenu(GeneralPiece *piece, const QPoint &pos);
    void showCapturedGeneralContextMenu(GeneralPiece *general, const QPoint &pos);
    void showTerritoryContextMenu(Player *player, const QString &territoryName, const QPoint &pos);
    void movePiece(GamePiece *piece, int rowDelta, int colDelta);
    void movePieceWithoutCost(GamePiece *piece, int rowDelta, int colDelta);

    // Leader movement with troops
    void moveLeaderWithTroops(GamePiece *leader, int rowDelta, int colDelta);
    void moveLeaderToTerritory(GamePiece *leader, const QString &destinationTerritory);  // Territory-based movement

    // Leader movement via road (only costs 1 movement point)
    void moveLeaderViaRoad(GamePiece *leader, const Position &destination);

    // Helper to get territory name at position
    QString getTerritoryNameAt(int row, int col) const;

    // Helper to get troop information at a position
    QString getTroopInfoAt(int row, int col) const;

    // Create icon for territory (shows ownership color and combat indicator)
    QIcon createTerritoryIcon(int row, int col, QChar currentPlayer) const;

    // Save/load window geometry
    void saveSettings();
    void loadSettings();

    QTabWidget *m_tabWidget;
    QMap<Player*, QWidget*> m_playerTabs;  // Map player to their tab widget
    QList<Player*> m_players;
    MapWidget *m_mapWidget;  // Reference to map for territory lookups

    // Global captured generals section
    QGroupBox *m_capturedGeneralsGroupBox;
    QTableWidget *m_capturedGeneralsTable;
};

#endif // PLAYERINFOWIDGET_H
