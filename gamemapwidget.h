#ifndef GAMEMAPWIDGET_H
#define GAMEMAPWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLTexture>
#include <QOpenGLFramebufferObject>
#include <QMatrix4x4>
#include <QImage>
#include <QTimer>
#include <QElapsedTimer>
#include <QMap>
#include <QMenuBar>
#include <QSoundEffect>

#include "common.h"
#include "mapgraph.h"

// Forward declarations
class Player;
class PlayerInfoWidget;
class GamePiece;
class QMenu;

// Alias for compatibility - OpenGL widget uses territory names, not grid positions
// This allows code to work with both MapWidget and GameMapWidget
class GameMapWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GameMapWidget(QWidget *parent = nullptr);
    ~GameMapWidget();

    // === Common Interface (shared with MapWidget) ===

    // Player management
    void setPlayers(const QList<Player*> &players);
    void setCurrentPlayerIndex(int index) { m_currentPlayerIndex = index; }
    int getCurrentPlayerIndex() const { return m_currentPlayerIndex; }
    void setPlayerInfoWidget(PlayerInfoWidget *widget) { m_playerInfoWidget = widget; }

    // Graph access
    MapGraph* getGraph() { return m_graph; }
    const MapGraph* getGraph() const { return m_graph; }

    // Grid compatibility methods (return dummy values - OpenGL map is not grid-based)
    int rows() const { return 0; }
    int cols() const { return 0; }

    // Territory queries (by name)
    QString getHoveredTerritory() const;
    bool isSeaTerritory(const QString &name) const;
    int getTerritoryValue(const QString &name) const;

    // Territory queries (by grid position - for MapWidget compatibility)
    QString getTerritoryNameAt(int row, int col) const;
    int getTerritoryValueAt(int row, int col) const;
    QChar getTerritoryOwnerAt(int row, int col) const;
    bool isSeaTerritory(int row, int col) const;
    QList<Position> getAdjacentSeaTerritories(const Position &pos) const;  // DEPRECATED: Returns empty list for OpenGL map
    bool hasEnemyPiecesAt(int row, int col, QChar currentPlayer) const;
    Position territoryNameToPosition(const QString &territoryName) const;

    // Get adjacent sea territories by territory name (graph-based, works with OpenGL map)
    QList<QString> getAdjacentSeaTerritories(const QString &landTerritoryName) const;

    // Building management (for MapWidget compatibility)
    void removeCityAt(int row, int col);
    void removeFortificationAt(int row, int col);
    void updateRoads();

    // Get player color
    QColor getPlayerColor(QChar player) const;

    // Game state
    void updateScores(const QMap<QChar, int> &scores);
    int getInflationMultiplier() const { return m_inflationMultiplier; }
    void setInflationMultiplier(int multiplier) { m_inflationMultiplier = qBound(1, multiplier, 3); }

    // Turn state
    bool isAtStartOfTurn() const { return m_isAtStartOfTurn; }
    void setAtStartOfTurn(bool atStart) { m_isAtStartOfTurn = atStart; }

    // Score calculation
    QMap<QChar, int> calculateScores() const;

    // === OpenGL-specific methods ===

    // Get territory ID at widget position (0 if none/background)
    int getTerritoryIdAt(const QPointF &widgetPos) const;

    // Zoom/pan control
    void resetView();
    void zoomToTerritory(const QString &name);

    // Highlight control (for external menus)
    void setHoveredTerritoryById(int territoryId);

public slots:
    void saveGame();
    void loadGame();
    void showAbout();
    void updateTerritoryOwnership();  // Call when any territory ownership changes

signals:
    // Common signals
    void scoresChanged();
    void taxesCollected(QChar player, int amount);
    void purchasePhaseNeeded(QChar player, int availableMoney, int inflationMultiplier);
    void itemPlaced(QString itemType);

    // Territory interaction
    void territoryClicked(const QString &territoryName);
    void territoryRightClicked(const QString &territoryName, const QPoint &globalPos);
    void territoryHovered(const QString &territoryName);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void createShaders();
    void createGeometry();
    void loadTextures();
    void createFramebuffer();
    void createOwnershipTexture();
    void createIconResources();
    void renderCityIcons();
    void updateMvpMatrix();
    void updateHoveredTerritory(const QPointF &widgetPos);
    void showTerritoryContextMenu(const QPoint &pos, int territoryId);
    QPointF widgetToNormalized(const QPointF &widgetPos) const;
    QPointF widgetToMapCoords(const QPointF &widgetPos) const;
    void createMenuBar();
    QString buildTerritoryTooltip(const QString &territoryName) const;
    void addMovementOptionsToMenu(QMenu *menu, GamePiece *piece, const QString &fromTerritory, QMap<QAction*, QString> &actionToTerritory);
    void playMenuClickSound(QAction *action);

    // Map graph (owned by this widget)
    MapGraph *m_graph = nullptr;

    // Player references
    QList<Player*> m_players;
    PlayerInfoWidget *m_playerInfoWidget = nullptr;
    int m_currentPlayerIndex = 0;

    // Game state
    bool m_isAtStartOfTurn = true;
    int m_inflationMultiplier = 1;
    QMap<QChar, int> m_scores;

    // Menu bar
    QMenuBar *m_menuBar = nullptr;

    // OpenGL resources - map processing shader (renders to FBO)
    QOpenGLShaderProgram *m_shaderProgram = nullptr;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo;
    QOpenGLTexture *m_mapTexture = nullptr;
    QOpenGLTexture *m_indexTexture = nullptr;

    // Framebuffer for intermediate rendering
    QOpenGLFramebufferObject *m_fbo = nullptr;
    QOpenGLShaderProgram *m_screenShader = nullptr;  // Renders FBO texture to screen

    // Ownership lookup texture (60 rows x 4 columns, RGB)
    // Row = territory ID, Column 0 = border color
    QOpenGLTexture *m_ownershipTexture = nullptr;
    QImage m_ownershipImage;  // CPU-side data for updating
    int m_borderRadius = 8;   // Border thickness in pixels

    // City icons
    QOpenGLTexture *m_cityIconTexture = nullptr;
    QOpenGLTexture *m_fortifiedCityIconTexture = nullptr;
    QOpenGLTexture *m_galleyIconTexture = nullptr;
    QOpenGLTexture *m_caesarIconTexture = nullptr;
    QOpenGLTexture *m_generalIconTexture = nullptr;
    QOpenGLTexture *m_infantryIconTexture = nullptr;
    QOpenGLTexture *m_cavalryIconTexture = nullptr;
    QOpenGLTexture *m_catapultIconTexture = nullptr;
    QOpenGLShaderProgram *m_iconShader = nullptr;
    QOpenGLBuffer m_iconVbo;
    QOpenGLVertexArrayObject m_iconVao;
    float m_iconSize = 60.0f;  // Icon size in map pixels

    // Territory detection (CPU side for mouse lookup)
    QImage m_indexImage;
    int m_hoveredTerritoryId = 0;  // Currently hovered territory ID (0 = none/background)

    // Map dimensions
    QSize m_mapSize;

    // Window size in pixels (for Retina support)
    QSize m_windowPixelSize;

    // Aspect ratio correction factors
    float m_aspectScaleX = 1.0f;
    float m_aspectScaleY = 1.0f;

    // View transform
    float m_zoom = 1.0f;
    QPointF m_pan;
    QMatrix4x4 m_mvpMatrix;

    // Dragging state
    bool m_dragging = false;
    QPointF m_lastMousePos;

    // Momentum/inertia
    QPointF m_velocity;
    QTimer m_momentumTimer;
    QElapsedTimer m_dragTimer;
    static constexpr float m_friction = 5.0f;
    static constexpr float m_minVelocity = 0.01f;

    // Audio
    QSoundEffect *m_clickSound = nullptr;
    QElapsedTimer m_clickTimer;  // Throttle click sounds
    QAction *m_lastHoveredAction = nullptr;  // Track last hovered action for click sounds

private slots:
    void onMomentumTick();
};

#endif // GAMEMAPWIDGET_H
