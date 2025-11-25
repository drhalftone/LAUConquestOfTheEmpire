#ifndef GAMEMAPWIDGET_H
#define GAMEMAPWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLTexture>
#include <QMatrix4x4>
#include <QImage>
#include <QTimer>
#include <QElapsedTimer>
#include <QMap>
#include <QMenuBar>
#include <QMediaPlayer>
#include <QAudioOutput>

#include "common.h"
#include "mapgraph.h"

// Forward declarations
class Player;
class PlayerInfoWidget;

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
    void setPlayers(const QList<Player*> &players) { m_players = players; }
    void setCurrentPlayerIndex(int index) { m_currentPlayerIndex = index; }
    int getCurrentPlayerIndex() const { return m_currentPlayerIndex; }
    void setPlayerInfoWidget(PlayerInfoWidget *widget) { m_playerInfoWidget = widget; }

    // Graph access
    MapGraph* getGraph() { return m_graph; }
    const MapGraph* getGraph() const { return m_graph; }

    // Territory queries (by name)
    QString getHoveredTerritory() const;
    bool isSeaTerritory(const QString &name) const;
    int getTerritoryValue(const QString &name) const;

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

public slots:
    void saveGame();
    void loadGame();
    void showAbout();

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
    void updateMvpMatrix();
    void updateHoveredTerritory(const QPointF &widgetPos);
    void showTerritoryContextMenu(const QPoint &pos, int territoryId);
    QPointF widgetToNormalized(const QPointF &widgetPos) const;
    QPointF widgetToMapCoords(const QPointF &widgetPos) const;
    void createMenuBar();

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

    // OpenGL resources
    QOpenGLShaderProgram *m_shaderProgram = nullptr;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo;
    QOpenGLTexture *m_mapTexture = nullptr;
    QOpenGLTexture *m_indexTexture = nullptr;

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
    QMediaPlayer *m_clickPlayer = nullptr;
    QAudioOutput *m_audioOutput = nullptr;

private slots:
    void onMomentumTick();
};

#endif // GAMEMAPWIDGET_H
