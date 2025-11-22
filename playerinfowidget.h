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

class AIPlayer;  // Forward declaration

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

    // End turn - can be called externally (e.g., from map context menu)
    void endTurn();

    // Handle right-click on territory in map
    void handleTerritoryRightClick(const QString &territoryName, const QPoint &globalPos, QChar currentPlayer);

    // === AI Integration: Read state from UI ===
    // These methods read displayed data from the widget, not from the Player object directly.
    // This allows AI to "see" what a human player would see.

    // Get the currently displayed player (selected tab)
    QChar getCurrentDisplayedPlayerId() const;

    // Read displayed wallet amount for a player
    int getDisplayedWallet(QChar playerId) const;

    // Read displayed territory count for a player
    int getDisplayedTerritoryCount(QChar playerId) const;

    // Read displayed territories list for a player
    QStringList getDisplayedTerritories(QChar playerId) const;

    // Read displayed piece count for a player
    int getDisplayedPieceCount(QChar playerId) const;

    // Structure for leader info read from UI
    struct DisplayedLeaderInfo {
        QString type;           // "Caesar" or "General"
        QString serialNumber;
        QString territory;
        int movesRemaining;
        QString onGalley;       // Empty if not on galley
    };

    // Read displayed leaders (Caesars and Generals) for a player
    QList<DisplayedLeaderInfo> getDisplayedLeaders(QChar playerId) const;

    // Get the player list (for AI to know which players exist)
    const QList<Player*>& getPlayers() const { return m_players; }

    // Get player by ID
    Player* getPlayerById(QChar playerId) const;

    // === AI Auto-Mode ===
    // When enabled, dialogs will auto-dismiss after a delay
    void setAIAutoMode(bool enabled, int delayMs = 1000);
    bool isAIAutoMode() const { return m_aiAutoMode; }
    int getAIAutoModeDelay() const { return m_aiAutoModeDelayMs; }
    void setAIPlayer(AIPlayer *aiPlayer) { m_aiPlayer = aiPlayer; }
    AIPlayer* getAIPlayer() const { return m_aiPlayer; }

    // === AI Movement ===
    // Structure describing a possible move for a leader
    struct MoveOption {
        QString destinationTerritory;
        int territoryValue;         // Tax value (5 or 10)
        QChar owner;                // '\0' = unclaimed, 'A'-'F' = player
        bool isOwnTerritory;        // True if owned by the moving player
        bool hasCombat;             // True if enemy pieces present or enemy-owned
        bool hasCity;               // True if there's a city there
        bool isViaRoad;             // True if reachable via road (costs 1 move for whole trip)
        bool isSea;                 // True if sea territory (generals can't go here)
        QString troopInfo;          // Description of troops there (e.g., "2 Inf, 1 Cav")
    };

    // Get all possible moves for a leader (general or caesar)
    QList<MoveOption> getMovesForLeader(GamePiece *leader) const;

    // Move a leader (general/caesar) to an adjacent territory
    // Returns true if move was successful
    bool aiMoveLeaderToTerritory(GamePiece *leader, const QString &destinationTerritory);

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

    // Galley transport functions
    void boardGalley(GamePiece *leader, const QString &seaTerritory, Player *player);  // Leader boards galley (auto-select)
    void boardGalleySpecific(GamePiece *leader, const QString &seaTerritory, Player *player, GalleyPiece *galley);  // Leader boards specific galley
    void disembarkFromGalley(GamePiece *leader, const QString &landTerritory, GalleyPiece *galley, Player *player);  // Leader disembarks
    void showDisembarkDialog(GamePiece *leader, GalleyPiece *galley, Player *player);  // Show dialog to choose disembark location

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

    // AI Auto-Mode settings
    bool m_aiAutoMode = false;
    int m_aiAutoModeDelayMs = 1000;
    AIPlayer *m_aiPlayer = nullptr;  // Reference to AI player for decision-making
};

#endif // PLAYERINFOWIDGET_H
