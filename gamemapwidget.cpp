#include "gamemapwidget.h"
#include "player.h"
#include "building.h"
#include "gamepiece.h"
#include "playerinfowidget.h"

#include <QDebug>
#include <QMenu>
#include <QActionGroup>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QtMath>
#include <QVector2D>
#include <QVector4D>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QUrl>
#include <QSettings>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <random>

// Helper function to load shader source from resource file
static QString loadShaderSource(const QString &resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open shader file:" << resourcePath;
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

// Poisson disc sampling to distribute points within a territory
// Uses index image to verify points are inside the territory
// seed parameter ensures deterministic results for the same territory
static QList<QPointF> poissonDiscSample(const QPointF &center, int numPoints, float minDistance,
                                         uint seed, const QImage &indexImage, int territoryId,
                                         const QPointF &cityPos = QPointF(), float cityExclusionRadius = 0.0f)
{
    QList<QPointF> points;
    if (numPoints <= 0) return points;

    // Check if index image is valid
    bool hasValidImage = !indexImage.isNull() && indexImage.width() > 0 && indexImage.height() > 0;

    // Check if we need to exclude city area
    bool hasCity = !cityPos.isNull() && cityExclusionRadius > 0.0f;

    // Helper lambda to check if a point is inside the territory
    auto isInsideTerritory = [&](const QPointF &pt) -> bool {
        if (!hasValidImage) return true;  // If no image, accept all points
        int x = qBound(0, static_cast<int>(pt.x()), indexImage.width() - 1);
        int y = qBound(0, static_cast<int>(pt.y()), indexImage.height() - 1);
        return indexImage.pixelColor(x, y).red() == territoryId;
    };

    // Helper lambda to check if a point is too close to the city
    auto isTooCloseToCity = [&](const QPointF &pt) -> bool {
        if (!hasCity) return false;
        float dist = qSqrt(qPow(pt.x() - cityPos.x(), 2) + qPow(pt.y() - cityPos.y(), 2));
        return dist < cityExclusionRadius;
    };

    // First point at center (unless there's a city there)
    if (numPoints == 1) {
        if (!isTooCloseToCity(center)) {
            points.append(center);
        } else {
            // Place next to city instead
            points.append(QPointF(center.x() + cityExclusionRadius * 1.2f, center.y()));
        }
        return points;
    }

    // Use deterministic seed based on territory
    std::mt19937 gen(seed);

    // For small numbers of points, try circular arrangement first, validate against territory
    if (numPoints <= 8) {
        float angleStep = 2.0f * M_PI / numPoints;
        // If there's a city, place units outside the exclusion zone, otherwise use tight circle
        float radius = hasCity ? (cityExclusionRadius * 1.3f) : (minDistance * 0.6f);

        for (int i = 0; i < numPoints; ++i) {
            float angle = i * angleStep - M_PI / 2;
            QPointF candidate(center.x() + radius * qCos(angle),
                              center.y() + radius * qSin(angle));

            // If outside territory or too close to city, try to find a valid point nearby
            if (!isInsideTerritory(candidate) || isTooCloseToCity(candidate)) {
                bool found = false;
                std::uniform_real_distribution<float> offsetDist(-minDistance, minDistance);
                for (int attempt = 0; attempt < 20 && !found; ++attempt) {
                    QPointF adjusted(center.x() + offsetDist(gen), center.y() + offsetDist(gen));
                    if (isInsideTerritory(adjusted) && !isTooCloseToCity(adjusted)) {
                        // Check distance from existing points
                        bool farEnough = true;
                        for (const QPointF &existing : points) {
                            float dist = qSqrt(qPow(adjusted.x() - existing.x(), 2) +
                                               qPow(adjusted.y() - existing.y(), 2));
                            if (dist < minDistance * 0.5f) {
                                farEnough = false;
                                break;
                            }
                        }
                        if (farEnough) {
                            candidate = adjusted;
                            found = true;
                        }
                    }
                }
                if (!found) {
                    // Fall back: if there's a city, offset from it, otherwise use center
                    if (hasCity) {
                        // Place at edge of city exclusion zone
                        float angle = i * angleStep - M_PI / 2;
                        candidate = QPointF(center.x() + cityExclusionRadius * 1.3f * qCos(angle),
                                           center.y() + cityExclusionRadius * 1.3f * qSin(angle));
                    } else {
                        candidate = center;
                    }
                }
            }
            points.append(candidate);
        }
        return points;
    }

    // For larger numbers, use Poisson disc sampling with territory validation
    QList<QPointF> activeList;
    const int k = 50;  // Number of candidates to try before giving up

    // Start with center point
    points.append(center);
    activeList.append(center);

    std::uniform_real_distribution<float> angleDist(0, 2.0f * M_PI);
    std::uniform_real_distribution<float> radiusDist(minDistance, minDistance * 2.0f);

    while (!activeList.isEmpty() && points.size() < numPoints) {
        std::uniform_int_distribution<int> indexDist(0, activeList.size() - 1);
        int randomIndex = indexDist(gen);
        QPointF point = activeList[randomIndex];

        bool foundCandidate = false;
        for (int i = 0; i < k; ++i) {
            float angle = angleDist(gen);
            float radius = radiusDist(gen);
            QPointF candidate(point.x() + radius * qCos(angle),
                              point.y() + radius * qSin(angle));

            // Check if candidate is inside the territory and not too close to city
            if (!isInsideTerritory(candidate) || isTooCloseToCity(candidate)) continue;

            // Check if candidate is far enough from all existing points
            bool valid = true;
            for (const QPointF &existing : points) {
                float dist = qSqrt(qPow(candidate.x() - existing.x(), 2) +
                                   qPow(candidate.y() - existing.y(), 2));
                if (dist < minDistance) {
                    valid = false;
                    break;
                }
            }

            if (valid) {
                points.append(candidate);
                activeList.append(candidate);
                foundCandidate = true;
                break;
            }
        }

        if (!foundCandidate) {
            activeList.removeAt(randomIndex);
        }
    }

    return points;
}

GameMapWidget::GameMapWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_vbo(QOpenGLBuffer::VertexBuffer)
    , m_iconVbo(QOpenGLBuffer::VertexBuffer)
{
    // Create the map graph (loads from CSV in constructor)
    m_graph = new MapGraph();

    // Set minimum size
    setMinimumSize(800, 600);

    // Enable mouse tracking for hover detection
    setMouseTracking(true);

    // Setup momentum timer (60 FPS)
    m_momentumTimer.setInterval(16);
    connect(&m_momentumTimer, &QTimer::timeout, this, &GameMapWidget::onMomentumTick);

    // Setup click sound
    m_clickSound = new QSoundEffect(this);
    m_clickSound->setSource(QUrl("qrc:/images/click.wav"));
    m_clickSound->setVolume(0.3f);  // Faint volume for all clicks
    m_clickTimer.start();  // Start timer for throttling clicks

    // Create menu bar
    createMenuBar();
}

GameMapWidget::~GameMapWidget()
{
    makeCurrent();

    delete m_shaderProgram;
    delete m_screenShader;
    delete m_iconShader;
    delete m_lineShader;
    delete m_mapTexture;
    delete m_indexTexture;
    delete m_ownershipTexture;
    delete m_cityIconTexture;
    delete m_fortifiedCityIconTexture;
    delete m_burningCityIconTexture;
    for (int p = 0; p < NUM_PLAYER_COLORS; ++p) {
        delete m_galleyIconTextures[p];
    }
    for (int u = 0; u < NUM_UNIT_TYPES; ++u) {
        for (int p = 0; p < NUM_PLAYER_COLORS; ++p) {
            delete m_unitIconTextures[u][p];
        }
    }
    delete m_fbo;
    m_vbo.destroy();
    m_vao.destroy();
    m_iconVbo.destroy();
    m_iconVao.destroy();

    doneCurrent();

    delete m_graph;
}

void GameMapWidget::createMenuBar()
{
    m_menuBar = new QMenuBar(this);

    // Style menu bar for better visibility
    m_menuBar->setStyleSheet(
        "QMenuBar { background-color: white; color: black; }"
        "QMenuBar::item { background-color: white; color: black; }"
        "QMenuBar::item:selected { background-color: lightblue; }"
        "QMenu { background-color: white; color: black; }"
        "QMenu::item:selected { background-color: lightblue; }"
    );

    QMenu *fileMenu = m_menuBar->addMenu("&File");

    QAction *saveAction = fileMenu->addAction(
        QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton),
        "&Save Game",
        this,
        &GameMapWidget::saveGame,
        QKeySequence::Save
    );

    QAction *loadAction = fileMenu->addAction(
        QApplication::style()->standardIcon(QStyle::SP_DirOpenIcon),
        "&Load Game",
        this,
        &GameMapWidget::loadGame,
        QKeySequence::Open
    );

    fileMenu->addSeparator();

    QAction *exitAction = fileMenu->addAction(
        QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton),
        "E&xit",
        this,
        &QWidget::close,
        QKeySequence::Quit
    );

    // View menu
    QMenu *viewMenu = m_menuBar->addMenu("&View");
    QAction *showPlayerViewerAction = viewMenu->addAction(
        "Show &Player Viewer",
        this,
        [this]() {
            if (m_playerInfoWidget) {
                m_playerInfoWidget->show();
                m_playerInfoWidget->raise();
                m_playerInfoWidget->activateWindow();
            }
        }
    );

    viewMenu->addSeparator();

    // Heat map visualization submenu
    QMenu *heatMapMenu = viewMenu->addMenu("&Heat Map");

    // Create action group for radio button behavior
    m_heatMapActionGroup = new QActionGroup(this);
    m_heatMapActionGroup->setExclusive(true);

    QAction *heatMapNone = heatMapMenu->addAction("&None (Ownership Only)");
    heatMapNone->setCheckable(true);
    heatMapNone->setChecked(true);
    heatMapNone->setData(static_cast<int>(HeatMapMode::None));
    m_heatMapActionGroup->addAction(heatMapNone);

    QAction *heatMapReach = heatMapMenu->addAction("Player &Reachability");
    heatMapReach->setCheckable(true);
    heatMapReach->setData(static_cast<int>(HeatMapMode::PlayerReachability));
    m_heatMapActionGroup->addAction(heatMapReach);

    QAction *heatMapForce = heatMapMenu->addAction("Max &Force Projection (1 Turn)");
    heatMapForce->setCheckable(true);
    heatMapForce->setData(static_cast<int>(HeatMapMode::MaxForceProjection));
    m_heatMapActionGroup->addAction(heatMapForce);

    QAction *heatMapForce2 = heatMapMenu->addAction("Max Force Projection (&2 Turn)");
    heatMapForce2->setCheckable(true);
    heatMapForce2->setData(static_cast<int>(HeatMapMode::MaxForceProjectionTwoTurn));
    m_heatMapActionGroup->addAction(heatMapForce2);

    QAction *heatMapThreat = heatMapMenu->addAction("&Enemy Threat (1 Turn)");
    heatMapThreat->setCheckable(true);
    heatMapThreat->setData(static_cast<int>(HeatMapMode::EnemyThreat));
    m_heatMapActionGroup->addAction(heatMapThreat);

    QAction *heatMapThreat2 = heatMapMenu->addAction("Enemy Threat (&2 Turn)");
    heatMapThreat2->setCheckable(true);
    heatMapThreat2->setData(static_cast<int>(HeatMapMode::EnemyThreatTwoTurn));
    m_heatMapActionGroup->addAction(heatMapThreat2);

    QAction *heatMapRisk = heatMapMenu->addAction("Risk &Level");
    heatMapRisk->setCheckable(true);
    heatMapRisk->setData(static_cast<int>(HeatMapMode::RiskLevel));
    m_heatMapActionGroup->addAction(heatMapRisk);

    // Connect action group to slot
    connect(m_heatMapActionGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        HeatMapMode mode = static_cast<HeatMapMode>(action->data().toInt());
        setHeatMapMode(mode);
    });

    QMenu *helpMenu = m_menuBar->addMenu("&Help");
    QAction *aboutAction = helpMenu->addAction(
        QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation),
        "&About",
        this,
        &GameMapWidget::showAbout
    );
}

void GameMapWidget::initializeGL()
{
    initializeOpenGLFunctions();

    qDebug() << "OpenGL version:" << reinterpret_cast<const char*>(glGetString(GL_VERSION));

    // Dark gray background for letterbox areas
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

    // Initialize window pixel size
    m_windowPixelSize = QSize(width() * devicePixelRatio(), height() * devicePixelRatio());

    createShaders();
    createGeometry();
    loadTextures();
    createOwnershipTexture();
    createIconResources();
    createFramebuffer();
    updateMvpMatrix();

    // Update ownership now that OpenGL resources are ready
    // (setPlayers may have been called before initializeGL)
    updateTerritoryOwnership();
}

void GameMapWidget::createShaders()
{
    // Create map processing shader (renders to FBO)
    m_shaderProgram = new QOpenGLShaderProgram(this);

    QString mapVertSource = loadShaderSource(":/shaders/shaders/map.vert");
    QString mapFragSource = loadShaderSource(":/shaders/shaders/map.frag");

    if (mapVertSource.isEmpty() || mapFragSource.isEmpty()) {
        qWarning() << "Failed to load map shader source files";
        return;
    }

    if (!m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, mapVertSource)) {
        qWarning() << "Vertex shader compilation failed:" << m_shaderProgram->log();
        return;
    }

    if (!m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, mapFragSource)) {
        qWarning() << "Fragment shader compilation failed:" << m_shaderProgram->log();
        return;
    }

    if (!m_shaderProgram->link()) {
        qWarning() << "Shader linking failed:" << m_shaderProgram->log();
        return;
    }

    qDebug() << "Map shader compiled and linked successfully";

    // Create screen shader (renders FBO texture to screen)
    m_screenShader = new QOpenGLShaderProgram(this);

    QString screenVertSource = loadShaderSource(":/shaders/shaders/screen.vert");
    QString screenFragSource = loadShaderSource(":/shaders/shaders/screen.frag");

    if (screenVertSource.isEmpty() || screenFragSource.isEmpty()) {
        qWarning() << "Failed to load screen shader source files";
        return;
    }

    if (!m_screenShader->addShaderFromSourceCode(QOpenGLShader::Vertex, screenVertSource)) {
        qWarning() << "Screen vertex shader compilation failed:" << m_screenShader->log();
        return;
    }

    if (!m_screenShader->addShaderFromSourceCode(QOpenGLShader::Fragment, screenFragSource)) {
        qWarning() << "Screen fragment shader compilation failed:" << m_screenShader->log();
        return;
    }

    if (!m_screenShader->link()) {
        qWarning() << "Screen shader linking failed:" << m_screenShader->log();
        return;
    }

    qDebug() << "Screen shader compiled and linked successfully";
}

void GameMapWidget::createGeometry()
{
    // Quad vertices: position (x, y) and texCoord (u, v)
    float vertices[] = {
        // Position      // TexCoord
        -1.0f,  1.0f,    0.0f, 0.0f,   // Top-left
         1.0f,  1.0f,    1.0f, 0.0f,   // Top-right
         1.0f, -1.0f,    1.0f, 1.0f,   // Bottom-right
        -1.0f, -1.0f,    0.0f, 1.0f,   // Bottom-left
    };

    m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices, sizeof(vertices));

    // Position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);

    // TexCoord attribute (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));

    m_vbo.release();
    m_vao.release();

    qDebug() << "Geometry created";
}

void GameMapWidget::loadTextures()
{
    // Load map image from resources
    QImage mapImage(":/images/Map.jpg");
    if (mapImage.isNull()) {
        qWarning() << "Failed to load map image from resources";
        return;
    }

    m_mapSize = mapImage.size();
    qDebug() << "Map loaded:" << m_mapSize.width() << "x" << m_mapSize.height();

    // Create map texture with linear filtering
    m_mapTexture = new QOpenGLTexture(mapImage);
    m_mapTexture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
    m_mapTexture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_mapTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
    qDebug() << "Map texture created";

    // Load territory index image
    m_indexImage = QImage(":/images/territories_index.png");
    if (m_indexImage.isNull()) {
        qWarning() << "Failed to load territory index image from resources";
        return;
    }

    // Convert to grayscale format for consistent pixel access
    m_indexImage = m_indexImage.convertToFormat(QImage::Format_Grayscale8);
    qDebug() << "Territory index loaded:" << m_indexImage.width() << "x" << m_indexImage.height();

    // Create index texture with NEAREST filtering (no interpolation!)
    m_indexTexture = new QOpenGLTexture(m_indexImage);
    m_indexTexture->setMinificationFilter(QOpenGLTexture::Nearest);
    m_indexTexture->setMagnificationFilter(QOpenGLTexture::Nearest);
    m_indexTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
    qDebug() << "Territory index texture created";
}

void GameMapWidget::createFramebuffer()
{
    // Delete existing FBO if any
    delete m_fbo;
    m_fbo = nullptr;

    if (m_mapSize.isEmpty()) {
        qWarning() << "Cannot create framebuffer: map size not set";
        return;
    }

    // Create FBO at map texture resolution for full-quality processing
    QOpenGLFramebufferObjectFormat format;
    format.setMipmap(false);
    format.setInternalTextureFormat(GL_RGBA8);

    m_fbo = new QOpenGLFramebufferObject(m_mapSize, format);

    if (!m_fbo->isValid()) {
        qWarning() << "Failed to create framebuffer object";
        delete m_fbo;
        m_fbo = nullptr;
        return;
    }

    qDebug() << "Framebuffer created:" << m_mapSize.width() << "x" << m_mapSize.height();
}

void GameMapWidget::createOwnershipTexture()
{
    // Create 60 rows x 8 columns RGB image for ownership/heat map lookup
    // Row = territory ID (1-60 maps to rows 0-59)
    // Column 0 = border color (ownership), Columns 1-4 = heat map modes
    m_ownershipImage = QImage(LUT_WIDTH, LUT_HEIGHT, QImage::Format_RGB888);
    m_ownershipImage.fill(Qt::black);  // Initialize all to black (unowned)

    // Create the texture with NEAREST filtering (no interpolation)
    m_ownershipTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
    m_ownershipTexture->setSize(LUT_WIDTH, LUT_HEIGHT);
    m_ownershipTexture->setFormat(QOpenGLTexture::RGB8_UNorm);
    m_ownershipTexture->allocateStorage();
    m_ownershipTexture->setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt8, m_ownershipImage.constBits());
    m_ownershipTexture->setMinificationFilter(QOpenGLTexture::Nearest);
    m_ownershipTexture->setMagnificationFilter(QOpenGLTexture::Nearest);
    m_ownershipTexture->setWrapMode(QOpenGLTexture::ClampToEdge);

    qDebug() << "Ownership texture created:" << LUT_WIDTH << "x" << LUT_HEIGHT << "RGB";
}

void GameMapWidget::updateTerritoryOwnership()
{
    if (!m_ownershipTexture || m_ownershipImage.isNull()) {
        return;
    }

    // Make OpenGL context current
    makeCurrent();

    // Clear to black (unowned)
    m_ownershipImage.fill(Qt::black);

    // Update ownership colors based on player territories
    for (Player *player : m_players) {
        QColor playerColor = getPlayerColor(player->getId());

        // Darken the color if it's not this player's turn
        if (!player->isMyTurn()) {
            // Make it darker (multiply RGB by 0.65 for a dimmed effect)
            playerColor = QColor(
                static_cast<int>(playerColor.red() * 0.65),
                static_cast<int>(playerColor.green() * 0.65),
                static_cast<int>(playerColor.blue() * 0.65)
            );
        }

        const QList<QString> &territories = player->getOwnedTerritories();

        for (const QString &territoryName : territories) {
            Territory territory = m_graph->getTerritory(territoryName);
            if (territory.id > 0 && territory.id <= 60) {
                // Row = territory ID - 1 (0-indexed), Column 0
                int row = territory.id - 1;
                m_ownershipImage.setPixelColor(0, row, playerColor);
            }
        }
    }

    // Upload updated image to texture
    m_ownershipTexture->setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt8, m_ownershipImage.constBits());

    doneCurrent();

    // Trigger repaint
    update();
}

void GameMapWidget::setHeatMapMode(HeatMapMode mode)
{
    if (m_heatMapMode == mode) {
        return;
    }

    m_heatMapMode = mode;
    qDebug() << "Heat map mode changed to:" << static_cast<int>(mode);

    // Update the heat map data for the new mode
    if (mode != HeatMapMode::None) {
        updateHeatMap();
    }

    // Trigger repaint
    update();
}

void GameMapWidget::updateHeatMap()
{
    if (!m_ownershipTexture || m_ownershipImage.isNull()) {
        return;
    }

    if (m_heatMapMode == HeatMapMode::None) {
        return;  // Nothing to update
    }

    // Call the appropriate helper based on mode
    switch (m_heatMapMode) {
        case HeatMapMode::PlayerReachability:
            updateHeatMapPlayerReachability();
            break;
        case HeatMapMode::MaxForceProjection:
            updateHeatMapMaxForce();
            break;
        case HeatMapMode::MaxForceProjectionTwoTurn:
            updateHeatMapMaxForceTwoTurn();
            break;
        case HeatMapMode::EnemyThreat:
            updateHeatMapEnemyThreat();
            break;
        case HeatMapMode::EnemyThreatTwoTurn:
            updateHeatMapEnemyThreatTwoTurn();
            break;
        case HeatMapMode::RiskLevel:
            updateHeatMapRiskLevel();
            break;
        default:
            break;
    }
}

void GameMapWidget::updateHeatMapPlayerReachability()
{
    if (m_players.isEmpty() || m_currentPlayerIndex < 0 || m_currentPlayerIndex >= m_players.size()) {
        return;
    }

    Player *currentPlayer = m_players[m_currentPlayerIndex];
    if (!currentPlayer) return;

    makeCurrent();

    // Clear column 1 (reachability) to black
    for (int row = 0; row < LUT_HEIGHT; row++) {
        m_ownershipImage.setPixelColor(1, row, Qt::black);
    }

    // First, mark territories where we already have troops or generals (bright cyan)
    QSet<QString> occupiedTerritories;
    QColor occupiedColor(0, 220, 220);  // Bright cyan for occupied

    for (CaesarPiece *caesar : currentPlayer->getCaesars()) {
        occupiedTerritories.insert(caesar->getTerritoryName());
    }
    for (GeneralPiece *general : currentPlayer->getGenerals()) {
        occupiedTerritories.insert(general->getTerritoryName());
    }
    for (InfantryPiece *infantry : currentPlayer->getInfantry()) {
        occupiedTerritories.insert(infantry->getTerritoryName());
    }
    for (CavalryPiece *cavalry : currentPlayer->getCavalry()) {
        occupiedTerritories.insert(cavalry->getTerritoryName());
    }
    for (CatapultPiece *catapult : currentPlayer->getCatapults()) {
        occupiedTerritories.insert(catapult->getTerritoryName());
    }

    for (const QString &territoryName : occupiedTerritories) {
        Territory territory = m_graph->getTerritory(territoryName);
        if (territory.id > 0 && territory.id <= 60) {
            int row = territory.id - 1;
            m_ownershipImage.setPixelColor(1, row, occupiedColor);
        }
    }

    // Use reachability calculator to find all reachable territories
    ReachabilityCalculator calc;
    QMap<QString, ReachInfo> reachable = calc.getAllReachable(currentPlayer, m_graph);

    // Color reachable territories green (brighter = more moves remaining)
    // Skip territories we already marked as occupied
    for (auto it = reachable.begin(); it != reachable.end(); ++it) {
        const QString &territoryName = it.key();
        const ReachInfo &info = it.value();

        // Skip if already occupied (those are cyan)
        if (occupiedTerritories.contains(territoryName)) {
            continue;
        }

        Territory territory = m_graph->getTerritory(territoryName);
        if (territory.id > 0 && territory.id <= 60) {
            int row = territory.id - 1;
            // Vary intensity based on how many leaders can reach
            int intensity = qMin(255, 100 + info.leadersWhoCanReach.size() * 50);
            m_ownershipImage.setPixelColor(1, row, QColor(0, intensity, intensity / 2));
        }
    }

    // Upload to texture
    m_ownershipTexture->setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt8, m_ownershipImage.constBits());
    doneCurrent();

    qDebug() << "Updated player reachability heat map:" << occupiedTerritories.size() << "occupied,"
             << reachable.size() << "reachable";
}

void GameMapWidget::updateHeatMapMaxForce()
{
    if (m_players.isEmpty() || m_currentPlayerIndex < 0 || m_currentPlayerIndex >= m_players.size()) {
        return;
    }

    Player *currentPlayer = m_players[m_currentPlayerIndex];
    if (!currentPlayer) return;

    makeCurrent();

    // Clear column 2 (max force) to black
    for (int row = 0; row < LUT_HEIGHT; row++) {
        m_ownershipImage.setPixelColor(2, row, Qt::black);
    }

    // First, count current TROOPS at each territory (not leaders)
    QMap<QString, int> currentForce;
    for (InfantryPiece *infantry : currentPlayer->getInfantry()) {
        currentForce[infantry->getTerritoryName()]++;
    }
    for (CavalryPiece *cavalry : currentPlayer->getCavalry()) {
        currentForce[cavalry->getTerritoryName()]++;
    }
    for (CatapultPiece *catapult : currentPlayer->getCatapults()) {
        currentForce[catapult->getTerritoryName()]++;
    }

    // Use reachability calculator
    // For force projection, we want to show maximum potential reach assuming full moves
    // So we temporarily set all leaders to have 2 moves for calculation purposes
    ReachabilityCalculator calc;

    // Save current moves and set to full for projection
    QMap<GamePiece*, double> savedMoves;
    for (CaesarPiece *caesar : currentPlayer->getCaesars()) {
        savedMoves[caesar] = caesar->getMovesRemaining();
        caesar->setMovesRemaining(2.0);
    }
    for (GeneralPiece *general : currentPlayer->getGenerals()) {
        savedMoves[general] = general->getMovesRemaining();
        general->setMovesRemaining(2.0);
    }

    QMap<QString, ReachInfo> reachable = calc.getAllReachable(currentPlayer, m_graph);

    // Restore original moves
    for (auto it = savedMoves.begin(); it != savedMoves.end(); ++it) {
        it.key()->setMovesRemaining(it.value());
    }

    qDebug() << "Force projection: reachable territories:" << reachable.size();
    for (auto it = reachable.begin(); it != reachable.end(); ++it) {
        qDebug() << "  " << it.key() << "force:" << it.value().maxTroopStrength
                 << "leaders:" << it.value().leadersWhoCanReach.size();
    }

    // Build combined force map (max of current and reachable)
    QMap<QString, int> maxForceMap;

    // Add current positions
    for (auto it = currentForce.begin(); it != currentForce.end(); ++it) {
        maxForceMap[it.key()] = it.value();
    }

    // Add/update with reachable forces
    for (auto it = reachable.begin(); it != reachable.end(); ++it) {
        const QString &territoryName = it.key();
        int reachableForce = it.value().maxTroopStrength;
        maxForceMap[territoryName] = qMax(maxForceMap.value(territoryName, 0), reachableForce);
    }

    // Track territories that are reachable by leaders (even if no troops can reach)
    QSet<QString> reachableByLeader;
    for (auto it = reachable.begin(); it != reachable.end(); ++it) {
        if (!it.value().leadersWhoCanReach.isEmpty()) {
            reachableByLeader.insert(it.key());
        }
    }

    qDebug() << "Force projection: maxForceMap has" << maxForceMap.size() << "territories";

    // Find max force for color scaling
    int maxForce = 1;
    for (int force : maxForceMap) {
        maxForce = qMax(maxForce, force);
    }

    // Color ALL territories
    QList<QString> allTerritories = m_graph->getTerritoryNames();
    for (const QString &territoryName : allTerritories) {
        if (m_graph->isSeaTerritory(territoryName)) continue;

        Territory territory = m_graph->getTerritory(territoryName);
        if (territory.id <= 0 || territory.id > 60) continue;

        int row = territory.id - 1;
        int force = maxForceMap.value(territoryName, 0);

        // Check if we only have leaders (no troops) at this territory
        int troopsOnly = 0;
        for (InfantryPiece *infantry : currentPlayer->getInfantry()) {
            if (infantry->getTerritoryName() == territoryName) troopsOnly++;
        }
        for (CavalryPiece *cavalry : currentPlayer->getCavalry()) {
            if (cavalry->getTerritoryName() == territoryName) troopsOnly++;
        }
        for (CatapultPiece *catapult : currentPlayer->getCatapults()) {
            if (catapult->getTerritoryName() == territoryName) troopsOnly++;
        }

        // Check reachable troop strength (not counting lone generals)
        int reachableTroops = 0;
        if (reachable.contains(territoryName)) {
            reachableTroops = reachable[territoryName].maxTroopStrength;
            // maxTroopStrength includes the leader, so subtract leaders to get just troops
            // Actually, let's check if any leader can bring troops
            for (GamePiece *leader : reachable[territoryName].leadersWhoCanReach) {
                int troopCount = calc.calculateTroopStrength(leader, currentPlayer, 0);
                if (troopCount > 1) {  // More than just the leader itself
                    reachableTroops = qMax(reachableTroops, troopCount);
                }
            }
        }

        int maxTroops = qMax(troopsOnly, reachableTroops);

        int r, g, b;
        if (force == 0 && !reachableByLeader.contains(territoryName)) {
            // No force can reach and no leaders can reach - gray
            r = 80;
            g = 80;
            b = 80;
        } else if (maxTroops == 0) {
            // Can only get a general there, no troops - red (vulnerable)
            // This includes force == 0 but reachableByLeader == true
            r = 255;
            g = 50;
            b = 50;
        } else {
            // Scale force to color: Blue (low) -> Yellow (medium) -> Green (high)
            float ratio = static_cast<float>(force) / maxForce;

            if (ratio < 0.5f) {
                // Blue to Yellow
                float t = ratio * 2.0f;
                r = static_cast<int>(t * 255);
                g = static_cast<int>(t * 255);
                b = static_cast<int>((1.0f - t) * 255);
            } else {
                // Yellow to Green
                float t = (ratio - 0.5f) * 2.0f;
                r = static_cast<int>((1.0f - t) * 255);
                g = 255;
                b = 0;
            }
        }

        m_ownershipImage.setPixelColor(2, row, QColor(r, g, b));
    }

    // Upload to texture
    m_ownershipTexture->setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt8, m_ownershipImage.constBits());
    doneCurrent();

    qDebug() << "Updated max force heat map, max force:" << maxForce;
}

void GameMapWidget::updateHeatMapMaxForceTwoTurn()
{
    if (m_players.isEmpty() || m_currentPlayerIndex < 0 || m_currentPlayerIndex >= m_players.size()) {
        return;
    }

    Player *currentPlayer = m_players[m_currentPlayerIndex];
    if (!currentPlayer) return;

    makeCurrent();

    // Clear column 6 (max force 2-turn) to black
    for (int row = 0; row < LUT_HEIGHT; row++) {
        m_ownershipImage.setPixelColor(6, row, Qt::black);
    }

    // First, count current TROOPS at each territory (not leaders)
    QMap<QString, int> currentForce;
    for (InfantryPiece *infantry : currentPlayer->getInfantry()) {
        currentForce[infantry->getTerritoryName()]++;
    }
    for (CavalryPiece *cavalry : currentPlayer->getCavalry()) {
        currentForce[cavalry->getTerritoryName()]++;
    }
    for (CatapultPiece *catapult : currentPlayer->getCatapults()) {
        currentForce[catapult->getTerritoryName()]++;
    }

    // Use reachability calculator with turnMultiplier=2 for 2-turn projection
    ReachabilityCalculator calc;

    // Save current moves and set to full for projection
    QMap<GamePiece*, double> savedMoves;
    for (CaesarPiece *caesar : currentPlayer->getCaesars()) {
        savedMoves[caesar] = caesar->getMovesRemaining();
        caesar->setMovesRemaining(2.0);
    }
    for (GeneralPiece *general : currentPlayer->getGenerals()) {
        savedMoves[general] = general->getMovesRemaining();
        general->setMovesRemaining(2.0);
    }

    // Use turnMultiplier=2 for 2-turn projection
    QMap<QString, ReachInfo> reachable = calc.getAllReachable(currentPlayer, m_graph, 2);

    // Restore original moves
    for (auto it = savedMoves.begin(); it != savedMoves.end(); ++it) {
        it.key()->setMovesRemaining(it.value());
    }

    qDebug() << "Force projection (2-turn): reachable territories:" << reachable.size();

    // Build combined force map (max of current and reachable)
    QMap<QString, int> maxForceMap;

    // Add current positions
    for (auto it = currentForce.begin(); it != currentForce.end(); ++it) {
        maxForceMap[it.key()] = it.value();
    }

    // Add/update with reachable forces
    for (auto it = reachable.begin(); it != reachable.end(); ++it) {
        const QString &territoryName = it.key();
        int reachableForce = it.value().maxTroopStrength;
        maxForceMap[territoryName] = qMax(maxForceMap.value(territoryName, 0), reachableForce);
    }

    // Track territories that are reachable by leaders (even if no troops can reach)
    QSet<QString> reachableByLeader;
    for (auto it = reachable.begin(); it != reachable.end(); ++it) {
        if (!it.value().leadersWhoCanReach.isEmpty()) {
            reachableByLeader.insert(it.key());
        }
    }

    // Find max force for color scaling
    int maxForce = 1;
    for (int force : maxForceMap) {
        maxForce = qMax(maxForce, force);
    }

    // Color ALL territories
    QList<QString> allTerritories = m_graph->getTerritoryNames();
    for (const QString &territoryName : allTerritories) {
        if (m_graph->isSeaTerritory(territoryName)) continue;

        Territory territory = m_graph->getTerritory(territoryName);
        if (territory.id <= 0 || territory.id > 60) continue;

        int row = territory.id - 1;
        int force = maxForceMap.value(territoryName, 0);

        // Check if we only have leaders (no troops) at this territory
        int troopsOnly = 0;
        for (InfantryPiece *infantry : currentPlayer->getInfantry()) {
            if (infantry->getTerritoryName() == territoryName) troopsOnly++;
        }
        for (CavalryPiece *cavalry : currentPlayer->getCavalry()) {
            if (cavalry->getTerritoryName() == territoryName) troopsOnly++;
        }
        for (CatapultPiece *catapult : currentPlayer->getCatapults()) {
            if (catapult->getTerritoryName() == territoryName) troopsOnly++;
        }

        // Check reachable troop strength using turnMultiplier=2
        int reachableTroops = 0;
        if (reachable.contains(territoryName)) {
            reachableTroops = reachable[territoryName].maxTroopStrength;
            for (GamePiece *leader : reachable[territoryName].leadersWhoCanReach) {
                int troopCount = calc.calculateTroopStrength(leader, currentPlayer, 0, 2);
                if (troopCount > 1) {
                    reachableTroops = qMax(reachableTroops, troopCount);
                }
            }
        }

        int maxTroops = qMax(troopsOnly, reachableTroops);

        int r, g, b;
        if (force == 0 && !reachableByLeader.contains(territoryName)) {
            // No force can reach and no leaders can reach - gray
            r = 80;
            g = 80;
            b = 80;
        } else if (maxTroops == 0) {
            // Can only get a general there, no troops - red (vulnerable)
            r = 255;
            g = 50;
            b = 50;
        } else {
            // Scale force to color: Blue (low) -> Yellow (medium) -> Green (high)
            float ratio = static_cast<float>(force) / maxForce;

            if (ratio < 0.5f) {
                // Blue to Yellow
                float t = ratio * 2.0f;
                r = static_cast<int>(t * 255);
                g = static_cast<int>(t * 255);
                b = static_cast<int>((1.0f - t) * 255);
            } else {
                // Yellow to Green
                float t = (ratio - 0.5f) * 2.0f;
                r = static_cast<int>((1.0f - t) * 255);
                g = 255;
                b = 0;
            }
        }

        m_ownershipImage.setPixelColor(6, row, QColor(r, g, b));
    }

    // Upload to texture
    m_ownershipTexture->setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt8, m_ownershipImage.constBits());
    doneCurrent();

    qDebug() << "Updated max force (2-turn) heat map, max force:" << maxForce;
}

void GameMapWidget::updateHeatMapEnemyThreat()
{
    if (m_players.isEmpty() || m_currentPlayerIndex < 0 || m_currentPlayerIndex >= m_players.size()) {
        return;
    }

    Player *currentPlayer = m_players[m_currentPlayerIndex];
    if (!currentPlayer) return;

    makeCurrent();

    // Clear column 3 (enemy threat) to black
    for (int row = 0; row < LUT_HEIGHT; row++) {
        m_ownershipImage.setPixelColor(3, row, Qt::black);
    }

    // Calculate enemy threat for each territory
    // This includes BOTH current enemy positions AND territories they can reach
    ReachabilityCalculator calc;
    QMap<QString, int> enemyThreat;  // territory -> max enemy force
    int maxThreat = 1;

    for (Player *player : m_players) {
        if (player == currentPlayer) continue;  // Skip self

        // First, count current enemy troops at each territory
        QMap<QString, int> currentTroops;
        for (CaesarPiece *caesar : player->getCaesars()) {
            currentTroops[caesar->getTerritoryName()]++;  // Caesar counts as 1
        }
        for (GeneralPiece *general : player->getGenerals()) {
            currentTroops[general->getTerritoryName()]++;  // General counts as 1
        }
        for (InfantryPiece *infantry : player->getInfantry()) {
            currentTroops[infantry->getTerritoryName()]++;
        }
        for (CavalryPiece *cavalry : player->getCavalry()) {
            currentTroops[cavalry->getTerritoryName()]++;
        }
        for (CatapultPiece *catapult : player->getCatapults()) {
            currentTroops[catapult->getTerritoryName()]++;
        }

        // Add current positions to threat map
        for (auto it = currentTroops.begin(); it != currentTroops.end(); ++it) {
            const QString &territoryName = it.key();
            int troops = it.value();
            if (!enemyThreat.contains(territoryName)) {
                enemyThreat[territoryName] = 0;
            }
            enemyThreat[territoryName] = qMax(enemyThreat[territoryName], troops);
            maxThreat = qMax(maxThreat, troops);
        }

        // Then add territories they can reach
        QMap<QString, ReachInfo> enemyReach = calc.getAllReachable(player, m_graph);
        for (auto it = enemyReach.begin(); it != enemyReach.end(); ++it) {
            const QString &territoryName = it.key();
            const ReachInfo &info = it.value();

            if (!enemyThreat.contains(territoryName)) {
                enemyThreat[territoryName] = 0;
            }
            enemyThreat[territoryName] = qMax(enemyThreat[territoryName], info.maxTroopStrength);
            maxThreat = qMax(maxThreat, enemyThreat[territoryName]);
        }
    }

    // Color ALL territories by enemy threat - green (safe) -> yellow (mid) -> red (high)
    QList<QString> allTerritories = m_graph->getTerritoryNames();
    for (const QString &territoryName : allTerritories) {
        if (m_graph->isSeaTerritory(territoryName)) continue;  // Skip sea territories

        Territory territory = m_graph->getTerritory(territoryName);
        if (territory.id <= 0 || territory.id > 60) continue;

        int row = territory.id - 1;
        int threat = enemyThreat.value(territoryName, 0);

        int r, g, b;
        if (threat == 0) {
            // No enemy threat - bright green (safe)
            r = 0;
            g = 200;
            b = 50;
        } else {
            // Scale threat to color: green (low) -> yellow (mid) -> red (high)
            float ratio = static_cast<float>(threat) / maxThreat;

            if (ratio < 0.5f) {
                // Green to Yellow
                float t = ratio * 2.0f;
                r = static_cast<int>(t * 255);
                g = 255;
                b = 0;
            } else {
                // Yellow to Red
                float t = (ratio - 0.5f) * 2.0f;
                r = 255;
                g = static_cast<int>((1.0f - t) * 255);
                b = 0;
            }
        }

        m_ownershipImage.setPixelColor(3, row, QColor(r, g, b));
    }

    // Upload to texture
    m_ownershipTexture->setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt8, m_ownershipImage.constBits());
    doneCurrent();

    qDebug() << "Updated enemy threat heat map, max threat:" << maxThreat;
}

void GameMapWidget::updateHeatMapEnemyThreatTwoTurn()
{
    if (m_players.isEmpty() || m_currentPlayerIndex < 0 || m_currentPlayerIndex >= m_players.size()) {
        return;
    }

    Player *currentPlayer = m_players[m_currentPlayerIndex];
    if (!currentPlayer) return;

    makeCurrent();

    // Clear column 4 (two-turn enemy threat) to black - matches enum value
    for (int row = 0; row < LUT_HEIGHT; row++) {
        m_ownershipImage.setPixelColor(4, row, Qt::black);
    }

    // Calculate enemy threat for each territory
    // Combined: max(1-turn threat, 0.5 * 2-turn threat)
    ReachabilityCalculator calc;
    QMap<QString, float> combinedThreat;  // territory -> combined threat value
    float maxThreat = 1.0f;

    for (Player *player : m_players) {
        if (player == currentPlayer) continue;  // Skip self

        // === 1-Turn Threat ===
        // Current enemy positions
        QMap<QString, int> currentTroops;
        for (CaesarPiece *caesar : player->getCaesars()) {
            currentTroops[caesar->getTerritoryName()]++;
        }
        for (GeneralPiece *general : player->getGenerals()) {
            currentTroops[general->getTerritoryName()]++;
        }
        for (InfantryPiece *infantry : player->getInfantry()) {
            currentTroops[infantry->getTerritoryName()]++;
        }
        for (CavalryPiece *cavalry : player->getCavalry()) {
            currentTroops[cavalry->getTerritoryName()]++;
        }
        for (CatapultPiece *catapult : player->getCatapults()) {
            currentTroops[catapult->getTerritoryName()]++;
        }

        // Add current positions as 1-turn threat
        for (auto it = currentTroops.begin(); it != currentTroops.end(); ++it) {
            float threat = static_cast<float>(it.value());
            combinedThreat[it.key()] = qMax(combinedThreat.value(it.key(), 0.0f), threat);
        }

        // 1-turn reachability (current moves)
        QMap<QString, ReachInfo> oneTurnReach = calc.getAllReachable(player, m_graph);
        for (auto it = oneTurnReach.begin(); it != oneTurnReach.end(); ++it) {
            float threat = static_cast<float>(it.value().maxTroopStrength);
            combinedThreat[it.key()] = qMax(combinedThreat.value(it.key(), 0.0f), threat);
        }

        // === 2-Turn Threat ===
        // Use turnMultiplier=2 to calculate reachability over 2 turns
        // This multiplies both leader movement and troop movement ranges by 2
        QMap<QString, ReachInfo> twoTurnReach = calc.getAllReachable(player, m_graph, 2);

        // Add 2-turn threat at 0.5 weight
        for (auto it = twoTurnReach.begin(); it != twoTurnReach.end(); ++it) {
            float twoTurnThreat = static_cast<float>(it.value().maxTroopStrength) * 0.5f;
            combinedThreat[it.key()] = qMax(combinedThreat.value(it.key(), 0.0f), twoTurnThreat);
        }
    }

    // Find max threat for color scaling
    for (float threat : combinedThreat) {
        maxThreat = qMax(maxThreat, threat);
    }

    // Color ALL territories
    QList<QString> allTerritories = m_graph->getTerritoryNames();
    for (const QString &territoryName : allTerritories) {
        if (m_graph->isSeaTerritory(territoryName)) continue;

        Territory territory = m_graph->getTerritory(territoryName);
        if (territory.id <= 0 || territory.id > 60) continue;

        int row = territory.id - 1;
        float threat = combinedThreat.value(territoryName, 0.0f);

        int r, g, b;
        if (threat < 0.01f) {
            // No enemy threat - bright green (safe)
            r = 0;
            g = 200;
            b = 50;
        } else {
            // Scale threat to color: green (low) -> yellow (mid) -> red (high)
            float ratio = threat / maxThreat;

            if (ratio < 0.5f) {
                // Green to Yellow
                float t = ratio * 2.0f;
                r = static_cast<int>(t * 255);
                g = 255;
                b = 0;
            } else {
                // Yellow to Red
                float t = (ratio - 0.5f) * 2.0f;
                r = 255;
                g = static_cast<int>((1.0f - t) * 255);
                b = 0;
            }
        }

        m_ownershipImage.setPixelColor(4, row, QColor(r, g, b));
    }

    // Upload to texture
    m_ownershipTexture->setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt8, m_ownershipImage.constBits());
    doneCurrent();

    qDebug() << "Updated 2-turn enemy threat heat map, max threat:" << maxThreat;
}

void GameMapWidget::updateHeatMapRiskLevel()
{
    if (m_players.isEmpty() || m_currentPlayerIndex < 0 || m_currentPlayerIndex >= m_players.size()) {
        return;
    }

    Player *currentPlayer = m_players[m_currentPlayerIndex];
    if (!currentPlayer) return;

    makeCurrent();

    // Clear column 5 (risk level) to black - matches enum value
    for (int row = 0; row < LUT_HEIGHT; row++) {
        m_ownershipImage.setPixelColor(5, row, Qt::black);
    }

    // Calculate reachability for current player and all enemies
    ReachabilityCalculator calc;
    QMap<QString, ReachInfo> ourReach = calc.getAllReachable(currentPlayer, m_graph);

    // Also include territories where we already have pieces
    QSet<QString> ourOccupied;
    QMap<QString, int> ourCurrentForce;
    for (CaesarPiece *caesar : currentPlayer->getCaesars()) {
        ourOccupied.insert(caesar->getTerritoryName());
        ourCurrentForce[caesar->getTerritoryName()]++;
    }
    for (GeneralPiece *general : currentPlayer->getGenerals()) {
        ourOccupied.insert(general->getTerritoryName());
        ourCurrentForce[general->getTerritoryName()]++;
    }
    for (InfantryPiece *infantry : currentPlayer->getInfantry()) {
        ourOccupied.insert(infantry->getTerritoryName());
        ourCurrentForce[infantry->getTerritoryName()]++;
    }
    for (CavalryPiece *cavalry : currentPlayer->getCavalry()) {
        ourOccupied.insert(cavalry->getTerritoryName());
        ourCurrentForce[cavalry->getTerritoryName()]++;
    }
    for (CatapultPiece *catapult : currentPlayer->getCatapults()) {
        ourOccupied.insert(catapult->getTerritoryName());
        ourCurrentForce[catapult->getTerritoryName()]++;
    }

    // Calculate enemy threat for all territories
    QMap<QString, int> enemyThreat;
    QSet<QString> enemyOccupied;
    for (Player *player : m_players) {
        if (player == currentPlayer) continue;

        // Current enemy positions
        for (CaesarPiece *caesar : player->getCaesars()) {
            enemyOccupied.insert(caesar->getTerritoryName());
            enemyThreat[caesar->getTerritoryName()]++;
        }
        for (GeneralPiece *general : player->getGenerals()) {
            enemyOccupied.insert(general->getTerritoryName());
            enemyThreat[general->getTerritoryName()]++;
        }
        for (InfantryPiece *infantry : player->getInfantry()) {
            enemyOccupied.insert(infantry->getTerritoryName());
            enemyThreat[infantry->getTerritoryName()]++;
        }
        for (CavalryPiece *cavalry : player->getCavalry()) {
            enemyOccupied.insert(cavalry->getTerritoryName());
            enemyThreat[cavalry->getTerritoryName()]++;
        }
        for (CatapultPiece *catapult : player->getCatapults()) {
            enemyOccupied.insert(catapult->getTerritoryName());
            enemyThreat[catapult->getTerritoryName()]++;
        }

        // Enemy reachability
        QMap<QString, ReachInfo> enemyReach = calc.getAllReachable(player, m_graph);
        for (auto it = enemyReach.begin(); it != enemyReach.end(); ++it) {
            enemyThreat[it.key()] = qMax(enemyThreat[it.key()], it.value().maxTroopStrength);
        }
    }

    // Define colors for risk levels
    QColor safeColor(0, 180, 0);       // Green - SAFE (we have presence, enemy cannot reach)
    QColor lowColor(100, 200, 100);    // Light green - LOW (we have force advantage)
    QColor mediumColor(255, 200, 0);   // Yellow - MEDIUM (forces roughly equal)
    QColor highColor(255, 50, 50);     // Red - HIGH (enemy has advantage)
    QColor enemyOccupiedColor(180, 0, 0);  // Dark red - enemy occupied, we can't reach
    QColor neutralColor(128, 128, 128);    // Gray - no one can reach

    // Iterate through ALL territories
    QList<QString> allTerritories = m_graph->getTerritoryNames();
    for (const QString &territoryName : allTerritories) {
        if (m_graph->isSeaTerritory(territoryName)) continue;  // Skip sea territories

        Territory territory = m_graph->getTerritory(territoryName);
        if (territory.id <= 0 || territory.id > 60) continue;

        int row = territory.id - 1;

        // Calculate our max force (current + reachable)
        int ourForce = ourCurrentForce.value(territoryName, 0);
        if (ourReach.contains(territoryName)) {
            ourForce = qMax(ourForce, ourReach[territoryName].maxTroopStrength);
        }

        // Get enemy force
        int enemyForce = enemyThreat.value(territoryName, 0);

        // Determine risk level
        QColor color;
        if (ourForce == 0 && enemyForce == 0) {
            // Neither side can reach
            color = neutralColor;
        } else if (ourForce == 0 && enemyForce > 0) {
            // Enemy only - high risk (dark red if occupied, red if just reachable)
            color = enemyOccupied.contains(territoryName) ? enemyOccupiedColor : highColor;
        } else if (ourForce > 0 && enemyForce == 0) {
            // We only - safe
            color = safeColor;
        } else {
            // Both can reach - compare forces
            if (ourForce > enemyForce * 1.5) {
                color = safeColor;  // Strong advantage
            } else if (ourForce > enemyForce) {
                color = lowColor;   // Slight advantage
            } else if (ourForce * 1.5 >= enemyForce) {
                color = mediumColor; // Roughly equal
            } else {
                color = highColor;  // Enemy advantage
            }
        }

        m_ownershipImage.setPixelColor(5, row, color);
    }

    // Upload to texture
    m_ownershipTexture->setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt8, m_ownershipImage.constBits());
    doneCurrent();

    qDebug() << "Updated risk level heat map for all territories";
}

void GameMapWidget::createIconResources()
{
    // Create icon shader
    m_iconShader = new QOpenGLShaderProgram(this);

    QString iconVertSource = loadShaderSource(":/shaders/shaders/icon.vert");
    QString iconFragSource = loadShaderSource(":/shaders/shaders/icon.frag");

    if (iconVertSource.isEmpty() || iconFragSource.isEmpty()) {
        qWarning() << "Failed to load icon shader source files";
        return;
    }

    if (!m_iconShader->addShaderFromSourceCode(QOpenGLShader::Vertex, iconVertSource)) {
        qWarning() << "Icon vertex shader compilation failed:" << m_iconShader->log();
        return;
    }

    if (!m_iconShader->addShaderFromSourceCode(QOpenGLShader::Fragment, iconFragSource)) {
        qWarning() << "Icon fragment shader compilation failed:" << m_iconShader->log();
        return;
    }

    if (!m_iconShader->link()) {
        qWarning() << "Icon shader linking failed:" << m_iconShader->log();
        return;
    }

    qDebug() << "Icon shader compiled and linked successfully";

    // Create line shader for roads (simple solid color)
    m_lineShader = new QOpenGLShaderProgram(this);

    const char* lineVertSource = R"(
        #version 330 core
        layout(location = 0) in vec2 position;
        void main() {
            gl_Position = vec4(position, 0.0, 1.0);
        }
    )";

    const char* lineFragSource = R"(
        #version 330 core
        uniform vec4 lineColor;
        out vec4 fragColor;
        void main() {
            fragColor = lineColor;
        }
    )";

    if (!m_lineShader->addShaderFromSourceCode(QOpenGLShader::Vertex, lineVertSource)) {
        qWarning() << "Line vertex shader compilation failed:" << m_lineShader->log();
    } else if (!m_lineShader->addShaderFromSourceCode(QOpenGLShader::Fragment, lineFragSource)) {
        qWarning() << "Line fragment shader compilation failed:" << m_lineShader->log();
    } else if (!m_lineShader->link()) {
        qWarning() << "Line shader linking failed:" << m_lineShader->log();
    } else {
        qDebug() << "Line shader compiled and linked successfully";
    }

    // Load city icon texture
    QImage cityImage(":/images/newCityIcon.png");
    if (!cityImage.isNull()) {
        m_cityIconTexture = new QOpenGLTexture(cityImage.mirrored());
        m_cityIconTexture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
        m_cityIconTexture->setMagnificationFilter(QOpenGLTexture::Linear);
        m_cityIconTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
        qDebug() << "City icon texture loaded:" << cityImage.size();
    } else {
        qWarning() << "Failed to load city icon texture";
    }

    // Load fortified city icon texture
    QImage fortifiedImage(":/images/walledCityIcon.png");
    if (!fortifiedImage.isNull()) {
        m_fortifiedCityIconTexture = new QOpenGLTexture(fortifiedImage.mirrored());
        m_fortifiedCityIconTexture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
        m_fortifiedCityIconTexture->setMagnificationFilter(QOpenGLTexture::Linear);
        m_fortifiedCityIconTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
        qDebug() << "Fortified city icon texture loaded:" << fortifiedImage.size();
    } else {
        qWarning() << "Failed to load fortified city icon texture";
    }

    // Load burning city icon texture (for cities marked for destruction)
    QImage burningImage(":/images/fireCityIcon.png");
    if (!burningImage.isNull()) {
        m_burningCityIconTexture = new QOpenGLTexture(burningImage.mirrored());
        m_burningCityIconTexture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
        m_burningCityIconTexture->setMagnificationFilter(QOpenGLTexture::Linear);
        m_burningCityIconTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
        qDebug() << "Burning city icon texture loaded:" << burningImage.size();
    } else {
        qWarning() << "Failed to load burning city icon texture";
    }

    // Load player-colored galley icon textures
    const char* galleyColorNames[NUM_PLAYER_COLORS] = {"red", "blue", "green", "yellow", "orange", "black"};
    for (int p = 0; p < NUM_PLAYER_COLORS; ++p) {
        QString path = QString(":/images/colored/galleyIcon_%1.png").arg(galleyColorNames[p]);
        QImage galleyImage(path);
        if (!galleyImage.isNull()) {
            m_galleyIconTextures[p] = new QOpenGLTexture(galleyImage.mirrored());
            m_galleyIconTextures[p]->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
            m_galleyIconTextures[p]->setMagnificationFilter(QOpenGLTexture::Linear);
            m_galleyIconTextures[p]->setWrapMode(QOpenGLTexture::ClampToEdge);
        } else {
            qWarning() << "Failed to load galley icon texture:" << path;
        }
    }
    qDebug() << "Galley icon textures loaded";

    // Load player-colored unit icon textures
    // Unit types: 0=Caesar, 1=General, 2=Infantry, 3=Cavalry, 4=Catapult
    // Player colors: 0=red, 1=blue, 2=green, 3=yellow, 4=orange, 5=black
    const char* unitNames[NUM_UNIT_TYPES] = {"ceasar", "general", "infantry", "cavalry", "catapult"};
    const char* colorNames[NUM_PLAYER_COLORS] = {"red", "blue", "green", "yellow", "orange", "black"};

    for (int u = 0; u < NUM_UNIT_TYPES; ++u) {
        for (int p = 0; p < NUM_PLAYER_COLORS; ++p) {
            QString path = QString(":/images/colored/%1Icon_%2.png").arg(unitNames[u]).arg(colorNames[p]);
            QImage image(path);
            if (!image.isNull()) {
                m_unitIconTextures[u][p] = new QOpenGLTexture(image.mirrored());
                m_unitIconTextures[u][p]->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
                m_unitIconTextures[u][p]->setMagnificationFilter(QOpenGLTexture::Linear);
                m_unitIconTextures[u][p]->setWrapMode(QOpenGLTexture::ClampToEdge);
            } else {
                qWarning() << "Failed to load unit icon texture:" << path;
            }
        }
    }
    qDebug() << "Unit icon textures loaded";

    // Create VAO and VBO for icon rendering (dynamic, will be updated each frame)
    m_iconVao.create();
    m_iconVbo.create();
    m_iconVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    qDebug() << "Icon resources created";
}

void GameMapWidget::renderRoads()
{
    if (!m_lineShader || !m_graph) {
        return;
    }

    // Collect all road segments from all players
    struct RoadSegment {
        QPointF from;
        QPointF to;
    };
    QList<RoadSegment> segments;

    for (Player *player : m_players) {
        QList<QPair<QString, QString>> playerRoads = m_graph->getRoadSegments(player);
        for (const auto &road : playerRoads) {
            Territory fromTerritory = m_graph->getTerritory(road.first);
            Territory toTerritory = m_graph->getTerritory(road.second);
            if (!fromTerritory.name.isEmpty() && !toTerritory.name.isEmpty()) {
                RoadSegment seg;
                seg.from = fromTerritory.centroid;
                seg.to = toTerritory.centroid;
                segments.append(seg);
            }
        }
    }

    if (segments.isEmpty()) {
        return;
    }

    // Enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_lineShader->bind();

    // Light gray color with some transparency
    m_lineShader->setUniformValue("lineColor", QVector4D(0.7f, 0.7f, 0.7f, 0.8f));

    // Road thickness in map pixels
    const float roadThickness = 8.0f;

    // Draw each road segment as a thick line (quad)
    m_iconVao.bind();
    m_iconVbo.bind();

    for (const RoadSegment &seg : segments) {
        // Calculate direction vector
        float dx = seg.to.x() - seg.from.x();
        float dy = seg.to.y() - seg.from.y();
        float length = qSqrt(dx * dx + dy * dy);
        if (length < 0.001f) continue;

        // Perpendicular vector for thickness
        float perpX = -dy / length * roadThickness / 2.0f;
        float perpY = dx / length * roadThickness / 2.0f;

        // Convert to NDC
        auto toNDC = [this](float x, float y) -> QPointF {
            float ndcX = (x / m_mapSize.width()) * 2.0f - 1.0f;
            float ndcY = 1.0f - (y / m_mapSize.height()) * 2.0f;
            return QPointF(ndcX, ndcY);
        };

        QPointF p1 = toNDC(seg.from.x() + perpX, seg.from.y() + perpY);
        QPointF p2 = toNDC(seg.from.x() - perpX, seg.from.y() - perpY);
        QPointF p3 = toNDC(seg.to.x() - perpX, seg.to.y() - perpY);
        QPointF p4 = toNDC(seg.to.x() + perpX, seg.to.y() + perpY);

        // Create quad vertices (two triangles)
        float vertices[] = {
            (float)p1.x(), (float)p1.y(),
            (float)p2.x(), (float)p2.y(),
            (float)p3.x(), (float)p3.y(),
            (float)p1.x(), (float)p1.y(),
            (float)p3.x(), (float)p3.y(),
            (float)p4.x(), (float)p4.y(),
        };

        m_iconVbo.allocate(vertices, sizeof(vertices));

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    m_iconVbo.release();
    m_iconVao.release();
    m_lineShader->release();
}

void GameMapWidget::renderCityIcons()
{
    if (!m_iconShader || !m_cityIconTexture || !m_fortifiedCityIconTexture) {
        return;
    }

    // Collect all cities and their positions
    struct CityInfo {
        QPointF centroid;
        bool isFortified;
        bool isMarkedForDestruction;
        float scale;  // Scale factor based on territory area
        Player *owner;  // Player who owns this city
    };
    QList<CityInfo> cities;

    // Calculate average area for scaling reference
    const float referenceArea = 50000.0f;  // Approximate average territory area

    for (Player *player : m_players) {
        for (Building *building : player->getAllBuildings()) {
            if (building->getType() == Building::Type::City) {
                QString territoryName = building->getTerritoryName();
                Territory territory = m_graph->getTerritory(territoryName);
                if (!territory.name.isEmpty()) {
                    CityInfo info;
                    info.centroid = territory.centroid;
                    // Check if city is fortified and/or marked for destruction
                    City *city = dynamic_cast<City*>(building);
                    info.isFortified = (city && city->isFortified());
                    info.isMarkedForDestruction = (city && city->isMarkedForDestruction());
                    // Scale based on sqrt of area ratio (clamped to reasonable range)
                    float areaRatio = static_cast<float>(territory.area) / referenceArea;
                    info.scale = qBound(0.75f, qSqrt(areaRatio), 2.0f);
                    info.owner = player;  // Track the owner
                    cities.append(info);
                }
            }
        }
    }

    if (cities.isEmpty()) {
        return;
    }

    // Enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_iconShader->bind();

    // Convert map coordinates to normalized device coordinates
    // Map coords: (0,0) top-left to (mapWidth, mapHeight) bottom-right
    // NDC: (-1,-1) bottom-left to (1,1) top-right

    // Render each city
    for (const CityInfo &city : cities) {
        // Scale icon size based on territory area
        float scaledIconSize = m_iconSize * city.scale;
        float halfIconW = scaledIconSize / 2.0f;
        float halfIconH = scaledIconSize / 2.0f;

        // Convert centroid to NDC
        float ndcX = (city.centroid.x() / m_mapSize.width()) * 2.0f - 1.0f;
        float ndcY = 1.0f - (city.centroid.y() / m_mapSize.height()) * 2.0f;  // Flip Y

        // Icon half-size in NDC
        float halfW = (halfIconW / m_mapSize.width()) * 2.0f;
        float halfH = (halfIconH / m_mapSize.height()) * 2.0f;

        // Create quad vertices for this icon
        float vertices[] = {
            // Position              // TexCoord
            ndcX - halfW, ndcY + halfH,  0.0f, 1.0f,   // Top-left
            ndcX + halfW, ndcY + halfH,  1.0f, 1.0f,   // Top-right
            ndcX + halfW, ndcY - halfH,  1.0f, 0.0f,   // Bottom-right
            ndcX - halfW, ndcY - halfH,  0.0f, 0.0f,   // Bottom-left
        };

        // Upload vertices
        m_iconVao.bind();
        m_iconVbo.bind();
        m_iconVbo.allocate(vertices, sizeof(vertices));

        // Setup vertex attributes
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              reinterpret_cast<void*>(2 * sizeof(float)));

        // Bind appropriate texture (burning > fortified > regular)
        glActiveTexture(GL_TEXTURE0);
        if (city.isMarkedForDestruction && m_burningCityIconTexture) {
            m_burningCityIconTexture->bind();
        } else if (city.isFortified) {
            m_fortifiedCityIconTexture->bind();
        } else {
            m_cityIconTexture->bind();
        }
        m_iconShader->setUniformValue("iconTexture", 0);

        // Set brightness based on whether it's this player's turn
        float brightness = (city.owner && city.owner->isMyTurn()) ? 1.0f : 0.65f;
        m_iconShader->setUniformValue("brightness", brightness);

        // Draw the icon quad
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // Cleanup
        if (city.isFortified) {
            m_fortifiedCityIconTexture->release();
        } else {
            m_cityIconTexture->release();
        }
        m_iconVbo.release();
        m_iconVao.release();
    }

    // === Render Galleys ===
    if (m_galleyIconTextures[0]) {
        // Helper to convert player ID to color index
        auto playerToColorIndex = [](QChar playerId) -> int {
            // Must match Player::getColorForPlayer() mapping
            switch (playerId.toLatin1()) {
                case 'A': return 0;  // red
                case 'B': return 2;  // green
                case 'C': return 1;  // blue
                case 'D': return 3;  // yellow
                case 'E': return 5;  // black
                case 'F': return 4;  // orange
                default:  return 0;
            }
        };

        // First pass: compute base position for each galley
        struct GalleyRenderInfo {
            GalleyPiece *galley;
            QPointF basePos;
            float scale;
            int playerIndex;  // For texture selection
            Player *owner;  // Player who owns this galley
        };
        QList<GalleyRenderInfo> galleyInfos;

        for (Player *player : m_players) {
            int playerIdx = playerToColorIndex(player->getId());
            for (GalleyPiece *galley : player->getGalleys()) {
                QString territoryName = galley->getTerritoryName();
                Territory territory = m_graph->getTerritory(territoryName);
                if (territory.name.isEmpty()) continue;

                QPointF basePos;
                if (galley->isBeached() && galley->hasLastSeaZone()) {
                    basePos = m_graph->getBeachPosition(territoryName, galley->getLastSeaZone());
                    if (basePos.isNull()) {
                        basePos = territory.centroid;
                    }
                } else {
                    basePos = territory.centroid;
                }

                float areaRatio = static_cast<float>(territory.area) / referenceArea;
                float scale = qBound(0.75f, qSqrt(areaRatio), 2.0f);

                galleyInfos.append({galley, basePos, scale, playerIdx, player});
            }
        }

        // Second pass: group galleys by similar positions (within 50 pixels)
        const float proximityThreshold = 50.0f;
        QMap<QString, QList<int>> positionGroups;  // Key is "x|y" rounded, value is indices

        for (int i = 0; i < galleyInfos.size(); ++i) {
            // Round position to grid for grouping
            int gridX = static_cast<int>(galleyInfos[i].basePos.x() / proximityThreshold);
            int gridY = static_cast<int>(galleyInfos[i].basePos.y() / proximityThreshold);
            QString gridKey = QString("%1|%2").arg(gridX).arg(gridY);
            positionGroups[gridKey].append(i);
        }

        // Third pass: render with offsets for overlapping galleys
        for (auto it = positionGroups.begin(); it != positionGroups.end(); ++it) {
            const QList<int> &indices = it.value();
            int groupSize = indices.size();

            for (int j = 0; j < groupSize; ++j) {
                const GalleyRenderInfo &info = galleyInfos[indices[j]];
                QPointF galleyPos = info.basePos;

                float scaledIconSize = m_iconSize * info.scale;
                float halfIconW = scaledIconSize / 2.0f;
                float halfIconH = scaledIconSize / 2.0f;

                // Apply offset for multiple galleys at similar positions
                if (groupSize > 1) {
                    float offsetSpacing = scaledIconSize * 0.7f;
                    float totalWidth = offsetSpacing * (groupSize - 1);
                    float startOffset = -totalWidth / 2.0f;
                    galleyPos.setX(galleyPos.x() + startOffset + j * offsetSpacing);
                    galleyPos.setY(galleyPos.y() + (j % 2 == 0 ? -8 : 8));
                }

                // Convert position to NDC
                float ndcX = (galleyPos.x() / m_mapSize.width()) * 2.0f - 1.0f;
                float ndcY = 1.0f - (galleyPos.y() / m_mapSize.height()) * 2.0f;

                float halfW = (halfIconW / m_mapSize.width()) * 2.0f;
                float halfH = (halfIconH / m_mapSize.height()) * 2.0f;

                float vertices[] = {
                    ndcX - halfW, ndcY + halfH,  0.0f, 1.0f,
                    ndcX + halfW, ndcY + halfH,  1.0f, 1.0f,
                    ndcX + halfW, ndcY - halfH,  1.0f, 0.0f,
                    ndcX - halfW, ndcY - halfH,  0.0f, 0.0f,
                };

                m_iconVao.bind();
                m_iconVbo.bind();
                m_iconVbo.allocate(vertices, sizeof(vertices));

                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                                      reinterpret_cast<void*>(2 * sizeof(float)));

                glActiveTexture(GL_TEXTURE0);
                int texIdx = qBound(0, info.playerIndex, NUM_PLAYER_COLORS - 1);
                m_galleyIconTextures[texIdx]->bind();
                m_iconShader->setUniformValue("iconTexture", 0);

                // Set brightness based on whether it's this player's turn
                float brightness = (info.owner && info.owner->isMyTurn()) ? 1.0f : 0.65f;
                m_iconShader->setUniformValue("brightness", brightness);

                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

                m_galleyIconTextures[texIdx]->release();
                m_iconVbo.release();
                m_iconVao.release();
            }
        }
    }

    // === Render Units (Caesars, Generals, Infantry, Cavalry, Catapults) with Poisson disc distribution ===
    // Check if at least some unit textures are loaded
    if (m_unitIconTextures[0][0]) {
        // Unit types for texture selection (must match array indices)
        // 0=Caesar, 1=General, 2=Infantry, 3=Cavalry, 4=Catapult
        struct UnitInfo {
            GamePiece *piece;
            int unitType;      // Index into m_unitIconTextures first dimension
            int playerIndex;   // Index into m_unitIconTextures second dimension
            Player *owner;     // Player who owns this unit
        };
        QMap<QString, QList<UnitInfo>> unitsByTerritory;

        // Helper to convert player ID to color index
        auto playerToColorIndex = [](QChar playerId) -> int {
            // Must match Player::getColorForPlayer() mapping
            switch (playerId.toLatin1()) {
                case 'A': return 0;  // red
                case 'B': return 2;  // green
                case 'C': return 1;  // blue
                case 'D': return 3;  // yellow
                case 'E': return 5;  // black
                case 'F': return 4;  // orange
                default:  return 0;
            }
        };

        for (Player *player : m_players) {
            int colorIdx = playerToColorIndex(player->getId());

            // Collect caesars
            for (CaesarPiece *caesar : player->getCaesars()) {
                if (caesar->isOnGalley()) continue;
                QString territory = caesar->getTerritoryName();
                if (!territory.isEmpty()) {
                    unitsByTerritory[territory].append({caesar, 0, colorIdx, player});
                }
            }
            // Collect generals
            for (GeneralPiece *general : player->getGenerals()) {
                if (general->isOnGalley()) continue;
                QString territory = general->getTerritoryName();
                if (!territory.isEmpty()) {
                    unitsByTerritory[territory].append({general, 1, colorIdx, player});
                }
            }
            // Collect infantry
            for (InfantryPiece *infantry : player->getInfantry()) {
                if (infantry->isOnGalley()) continue;
                QString territory = infantry->getTerritoryName();
                if (!territory.isEmpty()) {
                    unitsByTerritory[territory].append({infantry, 2, colorIdx, player});
                }
            }
            // Collect cavalry
            for (CavalryPiece *cavalry : player->getCavalry()) {
                if (cavalry->isOnGalley()) continue;
                QString territory = cavalry->getTerritoryName();
                if (!territory.isEmpty()) {
                    unitsByTerritory[territory].append({cavalry, 3, colorIdx, player});
                }
            }
            // Collect catapults
            for (CatapultPiece *catapult : player->getCatapults()) {
                if (catapult->isOnGalley()) continue;
                QString territory = catapult->getTerritoryName();
                if (!territory.isEmpty()) {
                    unitsByTerritory[territory].append({catapult, 4, colorIdx, player});
                }
            }
        }

        // Render units with distributed positions
        for (auto it = unitsByTerritory.begin(); it != unitsByTerritory.end(); ++it) {
            const QString &territoryName = it.key();
            const QList<UnitInfo> &units = it.value();

            Territory territory = m_graph->getTerritory(territoryName);
            if (territory.name.isEmpty()) continue;

            // Scale icon size slightly based on territory area, but keep spacing consistent
            float areaRatio = static_cast<float>(territory.area) / referenceArea;
            float scale = qBound(0.85f, qSqrt(areaRatio), 1.5f);  // Reduced scaling range
            float scaledIconSize = m_iconSize * scale;

            // Check if there's a city in this territory
            QPointF cityPos;
            float cityExclusionRadius = 0.0f;
            for (Player *player : m_players) {
                City *city = player->getCityAtTerritory(territoryName);
                if (city) {
                    // City found - use territory centroid and exclude area around it
                    cityPos = territory.centroid;
                    // Exclusion radius is half the scaled city icon size (just the city itself)
                    float cityScale = qBound(0.85f, qSqrt(areaRatio), 1.5f);
                    cityExclusionRadius = (m_iconSize * cityScale) * 0.5f;  // Half the city icon size
                    break;
                }
            }

            // Use Poisson disc sampling to distribute units within territory bounds
            // Seed with territory ID for deterministic positioning
            // Use base icon size for spacing to keep it consistent across territories
            float minDistance = m_iconSize * 0.5f;  // Consistent spacing regardless of territory size
            QList<QPointF> positions = poissonDiscSample(territory.centroid, units.size(), minDistance,
                                                          territory.id, m_indexImage, territory.id,
                                                          cityPos, cityExclusionRadius);

            // Render each unit at its distributed position
            for (int i = 0; i < units.size() && i < positions.size(); ++i) {
                const UnitInfo &unit = units[i];
                QPointF unitPos = positions[i];

                // Get the player-colored texture for this unit
                QOpenGLTexture *texture = m_unitIconTextures[unit.unitType][unit.playerIndex];
                if (!texture) continue;

                // Apply type-specific scaling
                // Caesar/General: 20% smaller (0.8x), Troops: 20% larger (1.2x)
                float typeScale = 1.0f;
                if (unit.unitType == 0 || unit.unitType == 1) {
                    // Caesar or General
                    typeScale = 0.8f;
                } else if (unit.unitType >= 2 && unit.unitType <= 4) {
                    // Infantry, Cavalry, or Catapult
                    typeScale = 1.2f;
                }
                float finalIconSize = scaledIconSize * typeScale;

                float halfIconW = finalIconSize / 2.0f;
                float halfIconH = finalIconSize / 2.0f;

                // Convert position to NDC
                float ndcX = (unitPos.x() / m_mapSize.width()) * 2.0f - 1.0f;
                float ndcY = 1.0f - (unitPos.y() / m_mapSize.height()) * 2.0f;

                float halfW = (halfIconW / m_mapSize.width()) * 2.0f;
                float halfH = (halfIconH / m_mapSize.height()) * 2.0f;

                float vertices[] = {
                    ndcX - halfW, ndcY + halfH,  0.0f, 1.0f,
                    ndcX + halfW, ndcY + halfH,  1.0f, 1.0f,
                    ndcX + halfW, ndcY - halfH,  1.0f, 0.0f,
                    ndcX - halfW, ndcY - halfH,  0.0f, 0.0f,
                };

                m_iconVao.bind();
                m_iconVbo.bind();
                m_iconVbo.allocate(vertices, sizeof(vertices));

                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                                      reinterpret_cast<void*>(2 * sizeof(float)));

                glActiveTexture(GL_TEXTURE0);
                texture->bind();
                m_iconShader->setUniformValue("iconTexture", 0);

                // Set brightness based on whether it's this player's turn
                float brightness = (unit.owner && unit.owner->isMyTurn()) ? 1.0f : 0.65f;
                m_iconShader->setUniformValue("brightness", brightness);

                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

                texture->release();
                m_iconVbo.release();
                m_iconVao.release();
            }
        }
    }

    m_iconShader->release();
    glDisable(GL_BLEND);
}

void GameMapWidget::resizeGL(int w, int h)
{
    Q_UNUSED(w);
    Q_UNUSED(h);

    // Position menu bar at top
    if (m_menuBar) {
        m_menuBar->setGeometry(0, 0, width(), m_menuBar->sizeHint().height());
        m_menuBar->raise();  // Ensure it's on top
        m_menuBar->show();
    }

    // Get actual pixel dimensions (for Retina displays)
    m_windowPixelSize = QSize(width() * devicePixelRatio(), height() * devicePixelRatio());

    // Account for menu bar
    int menuHeight = m_menuBar ? m_menuBar->height() * devicePixelRatio() : 0;
    m_windowPixelSize.setHeight(m_windowPixelSize.height() - menuHeight);

    if (m_mapSize.isEmpty()) {
        m_aspectScaleX = 1.0f;
        m_aspectScaleY = 1.0f;
        return;
    }

    // Calculate aspect ratio correction factors
    float mapAspect = static_cast<float>(m_mapSize.width()) / m_mapSize.height();
    float windowAspect = static_cast<float>(m_windowPixelSize.width()) / m_windowPixelSize.height();

    if (windowAspect > mapAspect) {
        m_aspectScaleX = mapAspect / windowAspect;
        m_aspectScaleY = 1.0f;
    } else {
        m_aspectScaleX = 1.0f;
        m_aspectScaleY = windowAspect / mapAspect;
    }

    updateMvpMatrix();
    update();
}

void GameMapWidget::paintGL()
{
    if (!m_shaderProgram || !m_screenShader || !m_mapTexture || !m_indexTexture || !m_ownershipTexture || !m_fbo) {
        // Fallback: clear to background color if resources not ready
        glViewport(0, 0, m_windowPixelSize.width(), m_windowPixelSize.height());
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    // ========================================================================
    // PASS 1: Render map to framebuffer (at full map resolution)
    // ========================================================================
    m_fbo->bind();
    glViewport(0, 0, m_mapSize.width(), m_mapSize.height());
    glClear(GL_COLOR_BUFFER_BIT);

    m_shaderProgram->bind();

    // Use identity matrix for FBO pass - we're rendering the full map
    QMatrix4x4 identityMatrix;
    m_shaderProgram->setUniformValue("mvp", identityMatrix);
    m_shaderProgram->setUniformValue("highlightedTerritory", m_hoveredTerritoryId);
    m_shaderProgram->setUniformValue("selectedTerritory", m_highlightedTerritoryId);
    m_shaderProgram->setUniformValue("borderRadius", m_borderRadius);
    m_shaderProgram->setUniformValue("mapSize", QVector2D(m_mapSize.width(), m_mapSize.height()));

    // Heat map mode: map enum to LUT column
    // Column 0 = ownership (reserved), 1 = reachability, 2 = max force, 3 = enemy threat,
    // 4 = enemy threat 2-turn, 5 = risk level, 6 = max force 2-turn
    int heatMapColumn = 0;
    switch (m_heatMapMode) {
        case HeatMapMode::None: heatMapColumn = 0; break;
        case HeatMapMode::PlayerReachability: heatMapColumn = 1; break;
        case HeatMapMode::MaxForceProjection: heatMapColumn = 2; break;
        case HeatMapMode::MaxForceProjectionTwoTurn: heatMapColumn = 6; break;
        case HeatMapMode::EnemyThreat: heatMapColumn = 3; break;
        case HeatMapMode::EnemyThreatTwoTurn: heatMapColumn = 4; break;
        case HeatMapMode::RiskLevel: heatMapColumn = 5; break;
    }
    m_shaderProgram->setUniformValue("heatMapColumn", heatMapColumn);
    m_shaderProgram->setUniformValue("lutWidth", static_cast<float>(LUT_WIDTH));

    // Bind map texture to unit 0
    glActiveTexture(GL_TEXTURE0);
    m_mapTexture->bind();
    m_shaderProgram->setUniformValue("mapTexture", 0);

    // Bind index texture to unit 1
    glActiveTexture(GL_TEXTURE1);
    m_indexTexture->bind();
    m_shaderProgram->setUniformValue("indexTexture", 1);

    // Bind ownership texture to unit 2
    glActiveTexture(GL_TEXTURE2);
    m_ownershipTexture->bind();
    m_shaderProgram->setUniformValue("ownershipTexture", 2);

    m_vao.bind();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    m_vao.release();

    // Release textures
    glActiveTexture(GL_TEXTURE2);
    m_ownershipTexture->release();
    glActiveTexture(GL_TEXTURE1);
    m_indexTexture->release();
    glActiveTexture(GL_TEXTURE0);
    m_mapTexture->release();

    m_shaderProgram->release();

    // Render roads first (under cities)
    renderRoads();

    // Render city icons on top of the map (still in FBO)
    renderCityIcons();

    m_fbo->release();

    // ========================================================================
    // PASS 2: Render FBO texture to screen (with zoom/pan transform)
    // ========================================================================
    glViewport(0, 0, m_windowPixelSize.width(), m_windowPixelSize.height());
    glClear(GL_COLOR_BUFFER_BIT);

    m_screenShader->bind();
    m_screenShader->setUniformValue("mvp", m_mvpMatrix);

    // Bind FBO texture to unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_fbo->texture());
    m_screenShader->setUniformValue("screenTexture", 0);

    m_vao.bind();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    m_vao.release();

    glBindTexture(GL_TEXTURE_2D, 0);
    m_screenShader->release();
}

void GameMapWidget::updateMvpMatrix()
{
    m_mvpMatrix.setToIdentity();
    m_mvpMatrix.translate(m_pan.x(), m_pan.y(), 0.0f);
    m_mvpMatrix.scale(m_zoom, m_zoom, 1.0f);
    m_mvpMatrix.scale(m_aspectScaleX, m_aspectScaleY, 1.0f);
}

QPointF GameMapWidget::widgetToNormalized(const QPointF &widgetPos) const
{
    // Account for menu bar
    int menuHeight = m_menuBar ? m_menuBar->height() : 0;
    float adjustedY = widgetPos.y() - menuHeight;
    float adjustedHeight = height() - menuHeight;

    float nx = 2.0f * widgetPos.x() / width() - 1.0f;
    float ny = 1.0f - 2.0f * adjustedY / adjustedHeight;
    return QPointF(nx, ny);
}

QPointF GameMapWidget::widgetToMapCoords(const QPointF &widgetPos) const
{
    QPointF normPos = widgetToNormalized(widgetPos);

    // Inverse of MVP transform
    float quadX = (normPos.x() - m_pan.x()) / (m_aspectScaleX * m_zoom);
    float quadY = (normPos.y() - m_pan.y()) / (m_aspectScaleY * m_zoom);

    // Convert from quad coords (-1 to 1) to texture coords (0 to 1)
    float texU = (quadX + 1.0f) / 2.0f;
    float texV = (1.0f - quadY) / 2.0f;

    // Convert to map pixel coordinates
    float mapX = texU * m_mapSize.width();
    float mapY = texV * m_mapSize.height();

    return QPointF(mapX, mapY);
}

int GameMapWidget::getTerritoryIdAt(const QPointF &widgetPos) const
{
    if (m_indexImage.isNull()) {
        return 0;
    }

    QPointF mapCoords = widgetToMapCoords(widgetPos);
    int x = qBound(0, static_cast<int>(mapCoords.x()), m_indexImage.width() - 1);
    int y = qBound(0, static_cast<int>(mapCoords.y()), m_indexImage.height() - 1);

    return m_indexImage.pixelColor(x, y).red();
}

void GameMapWidget::updateHoveredTerritory(const QPointF &widgetPos)
{
    int newTerritory = getTerritoryIdAt(widgetPos);

    if (newTerritory != m_hoveredTerritoryId) {
        m_hoveredTerritoryId = newTerritory;
        update();

        // Emit signal with territory name
        QString territoryName = m_graph->getTerritoryNameById(newTerritory);
        emit territoryHovered(territoryName);

        // Play click sound when a new territory is activated (throttled to prevent overload)
        if (newTerritory > 0 && m_clickTimer.elapsed() > 50) {
            if (m_clickSound->isPlaying()) {
                m_clickSound->stop();
            }
            m_clickSound->play();
            m_clickTimer.restart();
        }

        // Generate and set tooltip with territory information
        if (newTerritory > 0 && !territoryName.isEmpty()) {
            QString tooltip = buildTerritoryTooltip(territoryName);
            setToolTip(tooltip);
        } else {
            setToolTip("");
        }
    }
}

QString GameMapWidget::getHoveredTerritory() const
{
    return m_graph->getTerritoryNameById(m_hoveredTerritoryId);
}

bool GameMapWidget::isSeaTerritory(const QString &name) const
{
    return m_graph->isSeaTerritory(name);
}

int GameMapWidget::getTerritoryValue(const QString &name) const
{
    return m_graph->getValue(name);
}

void GameMapWidget::setPlayers(const QList<Player*> &players)
{
    m_players = players;

    // Connect to player signals for territory ownership changes
    for (Player *player : m_players) {
        connect(player, &Player::territoryClaimed, this, &GameMapWidget::updateTerritoryOwnership);
        connect(player, &Player::territoryUnclaimed, this, &GameMapWidget::updateTerritoryOwnership);
        // Update border colors when turn state changes
        connect(player, &Player::turnStarted, this, &GameMapWidget::updateTerritoryOwnership);
        connect(player, &Player::turnEnded, this, &GameMapWidget::updateTerritoryOwnership);
    }

    // Initial ownership update
    updateTerritoryOwnership();
}

QColor GameMapWidget::getPlayerColor(QChar player) const
{
    switch (player.toLatin1()) {
        case 'A': return QColor(255, 0, 0);      // Red
        case 'B': return QColor(0, 255, 0);      // Green
        case 'C': return QColor(0, 0, 255);      // Blue
        case 'D': return QColor(255, 255, 0);    // Yellow
        case 'E': return QColor(128, 128, 128);  // Gray (Black would be invisible)
        case 'F': return QColor(255, 165, 0);    // Orange
        default: return QColor(128, 128, 128);   // Gray
    }
}

void GameMapWidget::updateScores(const QMap<QChar, int> &scores)
{
    m_scores = scores;
    emit scoresChanged();
}

QMap<QChar, int> GameMapWidget::calculateScores() const
{
    QMap<QChar, int> scores;

    // Calculate scores using MapGraph territory values
    for (int i = 0; i < m_players.size(); ++i) {
        // For now, return 0 - actual calculation done in main.cpp
        // This avoids needing to include player.h here
        scores[QChar('A' + i)] = 0;
    }

    return scores;
}

void GameMapWidget::wheelEvent(QWheelEvent *event)
{
    QPointF mousePos = event->position();
    QPointF normPos = widgetToNormalized(mousePos);

    QPointF mapPos((normPos.x() - m_pan.x()) / (m_aspectScaleX * m_zoom),
                   (normPos.y() - m_pan.y()) / (m_aspectScaleY * m_zoom));

    float delta = event->angleDelta().y();
    float zoomFactor = 1.0f + delta / 1200.0f;

    float newZoom = qBound(1.0f, m_zoom * zoomFactor, 20.0f);
    m_zoom = newZoom;

    // Adjust pan to keep point under mouse stationary
    m_pan.setX(normPos.x() - mapPos.x() * m_aspectScaleX * m_zoom);
    m_pan.setY(normPos.y() - mapPos.y() * m_aspectScaleY * m_zoom);

    // Clamp pan
    float maxPanX = qMax(0.0f, m_aspectScaleX * m_zoom - 1.0f);
    float maxPanY = qMax(0.0f, m_aspectScaleY * m_zoom - 1.0f);
    m_pan.setX(qBound(-maxPanX, static_cast<float>(m_pan.x()), maxPanX));
    m_pan.setY(qBound(-maxPanY, static_cast<float>(m_pan.y()), maxPanY));

    updateMvpMatrix();
    update();

    event->accept();
}

void GameMapWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_momentumTimer.stop();
        m_velocity = QPointF(0, 0);

        m_dragging = true;
        m_lastMousePos = event->position();
        m_dragTimer.start();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    } else if (event->button() == Qt::RightButton) {
        if (m_hoveredTerritoryId > 0 && m_playerInfoWidget && m_graph) {
            // Delegate to PlayerInfoWidget for context menu (single source of truth)
            QString territoryName = m_graph->getTerritoryNameById(m_hoveredTerritoryId);
            QChar currentPlayer = '\0';
            if (m_currentPlayerIndex >= 0 && m_currentPlayerIndex < m_players.size()) {
                currentPlayer = m_players[m_currentPlayerIndex]->getId();
            }
            m_playerInfoWidget->handleTerritoryRightClick(territoryName, mapToGlobal(event->pos()), currentPlayer);
        }
        event->accept();
    }
}

void GameMapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        QPointF currentPos = event->position();
        QPointF delta = currentPos - m_lastMousePos;

        // Account for menu bar
        int menuHeight = m_menuBar ? m_menuBar->height() : 0;
        float adjustedHeight = height() - menuHeight;

        float dx = 2.0f * delta.x() / width();
        float dy = -2.0f * delta.y() / adjustedHeight;

        // Calculate velocity
        qint64 elapsed = m_dragTimer.elapsed();
        if (elapsed > 0) {
            float dt = elapsed / 1000.0f;
            float alpha = 0.3f;
            m_velocity.setX(alpha * (dx / dt) + (1.0f - alpha) * m_velocity.x());
            m_velocity.setY(alpha * (dy / dt) + (1.0f - alpha) * m_velocity.y());
        }
        m_dragTimer.restart();

        // Update pan
        m_pan.setX(m_pan.x() + dx);
        m_pan.setY(m_pan.y() + dy);

        // Clamp pan
        float maxPanX = qMax(0.0f, m_aspectScaleX * m_zoom - 1.0f);
        float maxPanY = qMax(0.0f, m_aspectScaleY * m_zoom - 1.0f);
        m_pan.setX(qBound(-maxPanX, static_cast<float>(m_pan.x()), maxPanX));
        m_pan.setY(qBound(-maxPanY, static_cast<float>(m_pan.y()), maxPanY));

        m_lastMousePos = currentPos;
        updateMvpMatrix();
        update();

        event->accept();
    } else {
        updateHoveredTerritory(event->position());
    }
}

void GameMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);

        // Start momentum if velocity is significant
        float speed = qSqrt(m_velocity.x() * m_velocity.x() + m_velocity.y() * m_velocity.y());
        if (speed > m_minVelocity) {
            m_dragTimer.restart();
            m_momentumTimer.start();
        }

        event->accept();
    }
}

void GameMapWidget::closeEvent(QCloseEvent *event)
{
    // Warn user about losing game progress
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Exit Game");
    msgBox.setText("Closing the map will exit the game.\n\n"
                   "All unsaved progress will be lost!\n\n"
                   "Do you want to save your game before exiting?");
    msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);

    int reply = msgBox.exec();

    if (reply == QMessageBox::Save) {
        // Try to save the game
        saveGame();
        // Only exit if we're at start of turn (save would have succeeded)
        if (m_isAtStartOfTurn) {
            event->accept();
            qApp->quit();
        } else {
            // Save was blocked due to mid-turn, ask if they still want to exit
            QMessageBox confirmBox(this);
            confirmBox.setWindowTitle("Exit Without Saving");
            confirmBox.setText("Cannot save mid-turn.\n\n"
                               "Do you still want to exit and lose your progress?");
            confirmBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            confirmBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            confirmBox.setDefaultButton(QMessageBox::No);

            if (confirmBox.exec() == QMessageBox::Yes) {
                event->accept();
                qApp->quit();
            } else {
                event->ignore();
            }
        }
    } else if (reply == QMessageBox::Discard) {
        // Exit without saving
        event->accept();
        qApp->quit();
    } else {
        // Cancel - don't close
        event->ignore();
    }
}

void GameMapWidget::onMomentumTick()
{
    float dt = m_dragTimer.elapsed() / 1000.0f;
    m_dragTimer.restart();

    m_pan.setX(m_pan.x() + m_velocity.x() * dt);
    m_pan.setY(m_pan.y() + m_velocity.y() * dt);

    float maxPanX = qMax(0.0f, m_aspectScaleX * m_zoom - 1.0f);
    float maxPanY = qMax(0.0f, m_aspectScaleY * m_zoom - 1.0f);
    m_pan.setX(qBound(-maxPanX, static_cast<float>(m_pan.x()), maxPanX));
    m_pan.setY(qBound(-maxPanY, static_cast<float>(m_pan.y()), maxPanY));

    float decay = qExp(-m_friction * dt);
    m_velocity *= decay;

    float speed = qSqrt(m_velocity.x() * m_velocity.x() + m_velocity.y() * m_velocity.y());
    bool hitBoundaryX = (m_pan.x() <= -maxPanX || m_pan.x() >= maxPanX) && maxPanX > 0;
    bool hitBoundaryY = (m_pan.y() <= -maxPanY || m_pan.y() >= maxPanY) && maxPanY > 0;

    if (speed < m_minVelocity || (hitBoundaryX && hitBoundaryY)) {
        m_momentumTimer.stop();
        m_velocity = QPointF(0, 0);
    } else {
        if (hitBoundaryX) m_velocity.setX(0);
        if (hitBoundaryY) m_velocity.setY(0);
    }

    updateMvpMatrix();
    update();
}

void GameMapWidget::resetView()
{
    m_zoom = 1.0f;
    m_pan = QPointF(0, 0);
    updateMvpMatrix();
    update();
}

void GameMapWidget::setHoveredTerritoryById(int territoryId)
{
    if (m_hoveredTerritoryId != territoryId) {
        m_hoveredTerritoryId = territoryId;
        update();
    }
}

void GameMapWidget::setHighlightedTerritory(const QString &territoryName)
{
    Territory territory = m_graph->getTerritory(territoryName);
    if (!territory.name.isEmpty()) {
        m_highlightedTerritoryId = territory.id;
        update();
    }
}

void GameMapWidget::clearHighlightedTerritory()
{
    m_highlightedTerritoryId = 0;
    update();
}

void GameMapWidget::zoomToTerritory(const QString &name)
{
    Territory territory = m_graph->getTerritory(name);
    if (territory.id == 0) return;

    // Calculate normalized position for territory centroid
    float texU = territory.centroid.x() / m_mapSize.width();
    float texV = territory.centroid.y() / m_mapSize.height();

    // Convert to quad coords
    float quadX = texU * 2.0f - 1.0f;
    float quadY = 1.0f - texV * 2.0f;

    // Set zoom and pan to center on territory
    m_zoom = 3.0f;
    m_pan.setX(-quadX * m_aspectScaleX * m_zoom);
    m_pan.setY(-quadY * m_aspectScaleY * m_zoom);

    // Clamp pan
    float maxPanX = qMax(0.0f, m_aspectScaleX * m_zoom - 1.0f);
    float maxPanY = qMax(0.0f, m_aspectScaleY * m_zoom - 1.0f);
    m_pan.setX(qBound(-maxPanX, static_cast<float>(m_pan.x()), maxPanX));
    m_pan.setY(qBound(-maxPanY, static_cast<float>(m_pan.y()), maxPanY));

    updateMvpMatrix();
    update();
}

void GameMapWidget::saveGame()
{
    QSettings settings;
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString lastDir = settings.value("Game/lastSaveDirectory", defaultDir).toString();

    QString fileName = QFileDialog::getSaveFileName(this, "Save Game", lastDir, "JSON Files (*.json);;All Files (*)");
    if (fileName.isEmpty()) return;

    // Remember the directory for next time
    QFileInfo fileInfo(fileName);
    settings.setValue("Game/lastSaveDirectory", fileInfo.absolutePath());

    QJsonObject gameState;

    // Save current player index
    gameState["currentPlayerIndex"] = m_currentPlayerIndex;

    // Save all players
    QJsonArray playersArray;
    for (Player *player : m_players) {
        QJsonObject playerObj;
        playerObj["id"] = QString(player->getId());
        playerObj["wallet"] = player->getWallet();
        playerObj["homeName"] = player->getHomeProvinceName();

        // Save owned territories
        QJsonArray territoriesArray;
        for (const QString &territory : player->getOwnedTerritories()) {
            territoriesArray.append(territory);
        }
        playerObj["ownedTerritories"] = territoriesArray;

        // Save Caesar
        QJsonArray caesarsArray;
        for (CaesarPiece *caesar : player->getCaesars()) {
            QJsonObject caesarObj;
            caesarObj["serialNumber"] = caesar->getSerialNumber();
            caesarObj["territory"] = caesar->getTerritoryName();
            caesarObj["movesRemaining"] = caesar->getMovesRemaining();
            caesarObj["onGalley"] = caesar->getOnGalley();

            QJsonArray legionArray;
            for (int pieceId : caesar->getLegion()) {
                legionArray.append(pieceId);
            }
            caesarObj["legion"] = legionArray;
            caesarsArray.append(caesarObj);
        }
        playerObj["caesars"] = caesarsArray;

        // Save Generals
        QJsonArray generalsArray;
        for (GeneralPiece *general : player->getGenerals()) {
            QJsonObject generalObj;
            generalObj["serialNumber"] = general->getSerialNumber();
            generalObj["number"] = general->getNumber();
            generalObj["territory"] = general->getTerritoryName();
            generalObj["movesRemaining"] = general->getMovesRemaining();
            generalObj["onGalley"] = general->getOnGalley();

            QJsonArray legionArray;
            for (int pieceId : general->getLegion()) {
                legionArray.append(pieceId);
            }
            generalObj["legion"] = legionArray;
            generalsArray.append(generalObj);
        }
        playerObj["generals"] = generalsArray;

        // Save Captured Generals
        QJsonArray capturedGeneralsArray;
        for (GeneralPiece *general : player->getCapturedGenerals()) {
            QJsonObject generalObj;
            generalObj["serialNumber"] = general->getSerialNumber();
            generalObj["originalPlayer"] = QString(general->getPlayer());
            generalObj["number"] = general->getNumber();
            generalObj["territory"] = general->getTerritoryName();
            capturedGeneralsArray.append(generalObj);
        }
        playerObj["capturedGenerals"] = capturedGeneralsArray;

        // Save Infantry
        QJsonArray infantryArray;
        for (InfantryPiece *infantry : player->getInfantry()) {
            QJsonObject infantryObj;
            infantryObj["serialNumber"] = infantry->getSerialNumber();
            infantryObj["territory"] = infantry->getTerritoryName();
            infantryObj["movesRemaining"] = infantry->getMovesRemaining();
            infantryObj["onGalley"] = infantry->getOnGalley();
            infantryArray.append(infantryObj);
        }
        playerObj["infantry"] = infantryArray;

        // Save Cavalry
        QJsonArray cavalryArray;
        for (CavalryPiece *cavalry : player->getCavalry()) {
            QJsonObject cavalryObj;
            cavalryObj["serialNumber"] = cavalry->getSerialNumber();
            cavalryObj["territory"] = cavalry->getTerritoryName();
            cavalryObj["movesRemaining"] = cavalry->getMovesRemaining();
            cavalryObj["onGalley"] = cavalry->getOnGalley();
            cavalryArray.append(cavalryObj);
        }
        playerObj["cavalry"] = cavalryArray;

        // Save Catapults
        QJsonArray catapultsArray;
        for (CatapultPiece *catapult : player->getCatapults()) {
            QJsonObject catapultObj;
            catapultObj["serialNumber"] = catapult->getSerialNumber();
            catapultObj["territory"] = catapult->getTerritoryName();
            catapultObj["movesRemaining"] = catapult->getMovesRemaining();
            catapultObj["onGalley"] = catapult->getOnGalley();
            catapultsArray.append(catapultObj);
        }
        playerObj["catapults"] = catapultsArray;

        // Save Galleys
        QJsonArray galleysArray;
        for (GalleyPiece *galley : player->getGalleys()) {
            QJsonObject galleyObj;
            galleyObj["serialNumber"] = galley->getSerialNumber();
            galleyObj["territory"] = galley->getTerritoryName();
            galleyObj["movesRemaining"] = galley->getMovesRemaining();
            galleyObj["leaderAboard"] = galley->getLeaderAboard();
            galleyObj["transportedThisTurn"] = galley->hasTransportedThisTurn();

            if (galley->hasLastSeaZone()) {
                galleyObj["lastSeaZone"] = galley->getLastSeaZone();
            }

            galleysArray.append(galleyObj);
        }
        playerObj["galleys"] = galleysArray;

        // Save Cities
        QJsonArray citiesArray;
        for (City *city : player->getCities()) {
            QJsonObject cityObj;
            cityObj["territory"] = city->getTerritoryName();
            cityObj["isFortified"] = city->isFortified();
            citiesArray.append(cityObj);
        }
        playerObj["cities"] = citiesArray;

        playersArray.append(playerObj);
    }
    gameState["players"] = playersArray;

    // Write to file
    QJsonDocument doc(gameState);
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Game Saved");
        msgBox.setText(QString("Game saved successfully to:\n%1").arg(fileName));
        msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.exec();
    } else {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Save Failed");
        msgBox.setText(QString("Failed to save game to:\n%1").arg(fileName));
        msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.exec();
    }
}

void GameMapWidget::loadGame()
{
    QSettings settings;
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString lastDir = settings.value("Game/lastSaveDirectory", defaultDir).toString();

    QString fileName = QFileDialog::getOpenFileName(this, "Load Game", lastDir, "JSON Files (*.json);;All Files (*)");
    if (fileName.isEmpty()) return;

    // Remember the directory for next time
    QFileInfo fileInfo(fileName);
    settings.setValue("Game/lastSaveDirectory", fileInfo.absolutePath());

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Load Failed", QString("Failed to open file:\n%1").arg(fileName));
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, "Load Failed", QString("Failed to parse JSON:\n%1").arg(parseError.errorString()));
        return;
    }

    QJsonObject gameState = doc.object();

    qDebug() << "=== LOADING GAME ===";
    qDebug() << "m_players.size():" << m_players.size();

    // Clear existing game state
    for (Player *player : m_players) {
        player->clearAllPiecesAndBuildings();
        player->clearAllTerritories();
    }

    // Load current player index
    m_currentPlayerIndex = gameState["currentPlayerIndex"].toInt(0);

    // Load players
    QJsonArray playersArray = gameState["players"].toArray();
    qDebug() << "playersArray.size():" << playersArray.size();

    for (int i = 0; i < playersArray.size() && i < m_players.size(); ++i) {
        QJsonObject playerObj = playersArray[i].toObject();
        Player *player = m_players[i];
        qDebug() << "Loading player" << i << "id:" << playerObj["id"].toString() << "m_player id:" << player->getId();

        player->setWallet(playerObj["wallet"].toInt(0));

        // Load owned territories (already cleared above)
        QJsonArray territoriesArray = playerObj["ownedTerritories"].toArray();
        for (const QJsonValue &val : territoriesArray) {
            player->claimTerritory(val.toString());
        }

        // Load Caesars
        QJsonArray caesarsArray = playerObj["caesars"].toArray();
        for (const QJsonValue &val : caesarsArray) {
            QJsonObject caesarObj = val.toObject();
            QString territory = caesarObj["territory"].toString();

            CaesarPiece *caesar = new CaesarPiece(player->getId(), territory, player);
            caesar->setMovesRemaining(caesarObj["movesRemaining"].toDouble(2));
            caesar->setOnGalley(caesarObj["onGalley"].toString());

            // Note: Legion IDs will be rebuilt after all pieces are loaded
            // since unique IDs are generated fresh on piece creation
            player->addCaesar(caesar);
            qDebug() << "  Added Caesar at" << territory << "- caesars count:" << player->getCaesarCount();
        }

        // Load Generals
        QJsonArray generalsArray = playerObj["generals"].toArray();
        for (const QJsonValue &val : generalsArray) {
            QJsonObject generalObj = val.toObject();
            QString territory = generalObj["territory"].toString();

            GeneralPiece *general = new GeneralPiece(player->getId(), territory, generalObj["number"].toInt(), player);
            general->setMovesRemaining(generalObj["movesRemaining"].toDouble(2));
            general->setOnGalley(generalObj["onGalley"].toString());

            // Note: Legion membership is not restored - troops will need to be reassigned
            player->addGeneral(general);
            qDebug() << "  Added General" << generalObj["number"].toInt() << "at" << territory;
        }
        qDebug() << "  Total generals:" << player->getGeneralCount();

        // Load Infantry
        QJsonArray infantryArray = playerObj["infantry"].toArray();
        for (const QJsonValue &val : infantryArray) {
            QJsonObject infantryObj = val.toObject();
            QString territory = infantryObj["territory"].toString();

            InfantryPiece *infantry = new InfantryPiece(player->getId(), territory, player);
            infantry->setMovesRemaining(infantryObj["movesRemaining"].toDouble(1));
            infantry->setOnGalley(infantryObj["onGalley"].toString());

            player->addInfantry(infantry);
        }
        qDebug() << "  Total infantry:" << player->getInfantryCount();

        // Load Cavalry
        QJsonArray cavalryArray = playerObj["cavalry"].toArray();
        for (const QJsonValue &val : cavalryArray) {
            QJsonObject cavalryObj = val.toObject();
            QString territory = cavalryObj["territory"].toString();

            CavalryPiece *cavalry = new CavalryPiece(player->getId(), territory, player);
            cavalry->setMovesRemaining(cavalryObj["movesRemaining"].toDouble(2));
            cavalry->setOnGalley(cavalryObj["onGalley"].toString());

            player->addCavalry(cavalry);
        }

        // Load Catapults
        QJsonArray catapultsArray = playerObj["catapults"].toArray();
        for (const QJsonValue &val : catapultsArray) {
            QJsonObject catapultObj = val.toObject();
            QString territory = catapultObj["territory"].toString();

            CatapultPiece *catapult = new CatapultPiece(player->getId(), territory, player);
            catapult->setMovesRemaining(catapultObj["movesRemaining"].toDouble(1));
            catapult->setOnGalley(catapultObj["onGalley"].toString());

            player->addCatapult(catapult);
        }

        // Load Galleys
        QJsonArray galleysArray = playerObj["galleys"].toArray();
        for (const QJsonValue &val : galleysArray) {
            QJsonObject galleyObj = val.toObject();
            QString territory = galleyObj["territory"].toString();

            GalleyPiece *galley = new GalleyPiece(player->getId(), territory, player);
            galley->setMovesRemaining(galleyObj["movesRemaining"].toDouble(2));
            // Note: leaderAboard is not restored since leader IDs change on load
            if (galleyObj["transportedThisTurn"].toBool()) {
                galley->setTransportedThisTurn(true);
            }
            if (galleyObj.contains("lastSeaZone")) {
                galley->setLastSeaZone(galleyObj["lastSeaZone"].toString());
            }

            player->addGalley(galley);
        }

        // Load Cities
        QJsonArray citiesArray = playerObj["cities"].toArray();
        for (const QJsonValue &val : citiesArray) {
            QJsonObject cityObj = val.toObject();
            QString territory = cityObj["territory"].toString();
            Position pos = territoryNameToPosition(territory);
            bool isFortified = cityObj["isFortified"].toBool();

            City *city = new City(player->getId(), pos, territory, isFortified, player);
            player->addCity(city);
            qDebug() << "  Added city at" << territory << "fortified:" << isFortified;
        }
        qDebug() << "  Total cities:" << player->getCityCount();
        qDebug() << "  Owned territories:" << player->getOwnedTerritories();
    }

    // Clear invalid onGalley references (old serial numbers that don't exist anymore)
    qDebug() << "Clearing invalid galley references...";
    for (Player *player : m_players) {
        for (CaesarPiece *caesar : player->getCaesars()) {
            caesar->clearGalley();
        }
        for (GeneralPiece *general : player->getGenerals()) {
            general->clearGalley();
        }
        for (InfantryPiece *infantry : player->getInfantry()) {
            infantry->clearGalley();
        }
        for (CavalryPiece *cavalry : player->getCavalry()) {
            cavalry->clearGalley();
        }
        for (CatapultPiece *catapult : player->getCatapults()) {
            catapult->clearGalley();
        }
        for (GalleyPiece *galley : player->getGalleys()) {
            galley->setLeaderAboard(0);  // Clear invalid leader reference
        }
    }

    // Rebuild galley-leader relationships and legion membership
    // Leaders in sea territories should be on galleys in the same territory
    // Troops in sea territories should be in the legion of a leader in the same territory
    qDebug() << "Rebuilding galley-leader relationships and legions...";
    for (Player *player : m_players) {
        // Check caesars
        for (CaesarPiece *caesar : player->getCaesars()) {
            QString territory = caesar->getTerritoryName();
            // Check if territory is a sea zone (starts with "Mare" or "Oceanus")
            if (territory.startsWith("Mare") || territory.startsWith("Oceanus")) {
                // Find a galley in the same territory
                for (GalleyPiece *galley : player->getGalleys()) {
                    if (galley->getTerritoryName() == territory && !galley->hasLeaderAboard()) {
                        // Establish the relationship
                        caesar->setOnGalley(galley->getSerialNumber());
                        galley->setLeaderAboard(caesar->getUniqueId());
                        qDebug() << "  Linked Caesar to galley in" << territory;
                        break;
                    }
                }

                // Add troops in the same sea territory to the caesar's legion
                caesar->clearLegion();
                for (InfantryPiece *infantry : player->getInfantry()) {
                    if (infantry->getTerritoryName() == territory) {
                        caesar->addToLegion(infantry->getUniqueId());
                        infantry->setOnGalley(caesar->getOnGalley());
                    }
                }
                for (CavalryPiece *cavalry : player->getCavalry()) {
                    if (cavalry->getTerritoryName() == territory) {
                        caesar->addToLegion(cavalry->getUniqueId());
                        cavalry->setOnGalley(caesar->getOnGalley());
                    }
                }
                for (CatapultPiece *catapult : player->getCatapults()) {
                    if (catapult->getTerritoryName() == territory) {
                        caesar->addToLegion(catapult->getUniqueId());
                        catapult->setOnGalley(caesar->getOnGalley());
                    }
                }
                qDebug() << "    Caesar's legion now has" << caesar->getLegion().size() << "troops";
            }
        }

        // Check generals
        for (GeneralPiece *general : player->getGenerals()) {
            QString territory = general->getTerritoryName();
            // Check if territory is a sea zone (starts with "Mare" or "Oceanus")
            if (territory.startsWith("Mare") || territory.startsWith("Oceanus")) {
                // Find a galley in the same territory
                for (GalleyPiece *galley : player->getGalleys()) {
                    if (galley->getTerritoryName() == territory && !galley->hasLeaderAboard()) {
                        // Establish the relationship
                        general->setOnGalley(galley->getSerialNumber());
                        galley->setLeaderAboard(general->getUniqueId());
                        qDebug() << "  Linked General" << general->getNumber() << "to galley in" << territory;
                        break;
                    }
                }

                // Add troops in the same sea territory to the general's legion
                // But only if this general is on a galley (to avoid assigning troops to multiple leaders)
                if (general->isOnGalley()) {
                    general->clearLegion();
                    for (InfantryPiece *infantry : player->getInfantry()) {
                        if (infantry->getTerritoryName() == territory && !infantry->isOnGalley()) {
                            general->addToLegion(infantry->getUniqueId());
                            infantry->setOnGalley(general->getOnGalley());
                        }
                    }
                    for (CavalryPiece *cavalry : player->getCavalry()) {
                        if (cavalry->getTerritoryName() == territory && !cavalry->isOnGalley()) {
                            general->addToLegion(cavalry->getUniqueId());
                            cavalry->setOnGalley(general->getOnGalley());
                        }
                    }
                    for (CatapultPiece *catapult : player->getCatapults()) {
                        if (catapult->getTerritoryName() == territory && !catapult->isOnGalley()) {
                            general->addToLegion(catapult->getUniqueId());
                            catapult->setOnGalley(general->getOnGalley());
                        }
                    }
                    qDebug() << "    General" << general->getNumber() << "'s legion now has" << general->getLegion().size() << "troops";
                }
            }
        }
    }

    qDebug() << "=== LOAD COMPLETE ===";

    // Update display
    if (m_playerInfoWidget) {
        m_playerInfoWidget->updateAllPlayers();
    }
    update();

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Game Loaded");
    msgBox.setText(QString("Game loaded successfully from:\n%1").arg(fileName));
    msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    msgBox.exec();
}

void GameMapWidget::showAbout()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("About Conquest of the Empire");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText("<h3>Conquest of the Empire</h3>"
                   "<p>A strategic board game of territorial conquest.</p>"
                   "<p><b>Game Features:</b></p>"
                   "<ul>"
                   "<li>6 Player support (A-F)</li>"
                   "<li>Multiple unit types: Caesar, Generals, Infantry, Cavalry, Catapults, Galleys</li>"
                   "<li>Territory control and taxation</li>"
                   "<li>Cities, roads, and fortifications</li>"
                   "<li>Combat system with general capture and ransom</li>"
                   "<li>Economic management</li>"
                   "</ul>"
                   "<p><b>How to Play:</b></p>"
                   "<ul>"
                   "<li>Right-click territories to move pieces and manage your empire</li>"
                   "<li>Right-click pieces in Player Info to see movement options</li>"
                   "<li>Collect taxes from owned territories at the end of your turn</li>"
                   "<li>Purchase new units and buildings with your wealth</li>"
                   "<li>Capture enemy generals and negotiate ransoms</li>"
                   "</ul>"
                   "<p>Developed with Qt C++ and OpenGL</p>");
    msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    msgBox.exec();
}

// ============================================================================
// MapWidget Compatibility Methods (Grid-based API)
// ============================================================================
// These methods provide compatibility with code that expects grid-based
// MapWidget API. Since GameMapWidget uses a graph-based system with territory
// names, these methods return stub/default values.
// ============================================================================

QString GameMapWidget::getTerritoryNameAt(int row, int col) const
{
    Q_UNUSED(row);
    Q_UNUSED(col);
    // OpenGL map doesn't use grid positions - return empty string
    // Callers should use territory names directly
    return QString();
}

int GameMapWidget::getTerritoryValueAt(int row, int col) const
{
    Q_UNUSED(row);
    Q_UNUSED(col);
    // OpenGL map doesn't use grid positions
    return 0;
}

QChar GameMapWidget::getTerritoryOwnerAt(int row, int col) const
{
    Q_UNUSED(row);
    Q_UNUSED(col);
    // OpenGL map doesn't use grid positions
    return '\0';  // No owner
}

bool GameMapWidget::isSeaTerritory(int row, int col) const
{
    Q_UNUSED(row);
    Q_UNUSED(col);
    // OpenGL map doesn't use grid positions
    return false;
}

QList<Position> GameMapWidget::getAdjacentSeaTerritories(const Position &pos) const
{
    Q_UNUSED(pos);
    // OpenGL map doesn't use grid positions
    // This method is deprecated - use the territory name version instead
    return QList<Position>();
}

QList<QString> GameMapWidget::getAdjacentSeaTerritories(const QString &landTerritoryName) const
{
    if (m_graph) {
        return m_graph->getAdjacentSeaTerritories(landTerritoryName);
    }
    return QList<QString>();
}

bool GameMapWidget::hasEnemyPiecesAt(int row, int col, QChar currentPlayer) const
{
    Q_UNUSED(row);
    Q_UNUSED(col);
    Q_UNUSED(currentPlayer);
    // OpenGL map doesn't use grid positions
    return false;
}

Position GameMapWidget::territoryNameToPosition(const QString &territoryName) const
{
    Q_UNUSED(territoryName);
    // OpenGL map doesn't use grid positions
    // Return invalid position
    return Position{-1, -1};
}

void GameMapWidget::removeCityAt(int row, int col)
{
    Q_UNUSED(row);
    Q_UNUSED(col);
    // Cities are managed by Player objects in OpenGL version
    // This is a no-op for compatibility
}

void GameMapWidget::removeFortificationAt(int row, int col)
{
    Q_UNUSED(row);
    Q_UNUSED(col);
    // Fortifications are managed by Player objects in OpenGL version
    // This is a no-op for compatibility
}

QString GameMapWidget::buildTerritoryTooltip(const QString &territoryName) const
{
    if (territoryName.isEmpty()) {
        return "";
    }

    QStringList lines;

    // Territory name, ID, and value
    Territory territory = m_graph->getTerritory(territoryName);
    lines << QString("<b>%1</b> (ID: %2)").arg(territoryName).arg(territory.id);
    lines << QString("Value: %1").arg(territory.value);

    // If heat map mode is active, show heat map specific information
    if (m_heatMapMode != HeatMapMode::None && !m_players.isEmpty() &&
        m_currentPlayerIndex >= 0 && m_currentPlayerIndex < m_players.size()) {

        Player *currentPlayer = m_players[m_currentPlayerIndex];
        ReachabilityCalculator calc;

        lines << "";
        QString heatMapName;
        switch (m_heatMapMode) {
            case HeatMapMode::PlayerReachability: heatMapName = "Player Reachability"; break;
            case HeatMapMode::MaxForceProjection: heatMapName = "Max Force Projection (1 Turn)"; break;
            case HeatMapMode::MaxForceProjectionTwoTurn: heatMapName = "Max Force Projection (2 Turn)"; break;
            case HeatMapMode::EnemyThreat: heatMapName = "Enemy Threat (1 Turn)"; break;
            case HeatMapMode::EnemyThreatTwoTurn: heatMapName = "Enemy Threat (2 Turn)"; break;
            case HeatMapMode::RiskLevel: heatMapName = "Risk Level"; break;
            default: heatMapName = "Unknown"; break;
        }
        lines << QString("<b><font color='#FF6600'>Heat Map: %1</font></b>").arg(heatMapName);

        switch (m_heatMapMode) {
            case HeatMapMode::PlayerReachability: {
                // Check if we already have pieces at this territory
                int leadersHere = 0;
                int troopsHere = 0;
                QStringList piecesHere;

                for (CaesarPiece *caesar : currentPlayer->getCaesars()) {
                    if (caesar->getTerritoryName() == territoryName) {
                        leadersHere++;
                        piecesHere << "Caesar";
                    }
                }
                for (GeneralPiece *general : currentPlayer->getGenerals()) {
                    if (general->getTerritoryName() == territoryName) {
                        leadersHere++;
                        piecesHere << QString("General %1").arg(general->getNumber());
                    }
                }
                for (InfantryPiece *infantry : currentPlayer->getInfantry()) {
                    if (infantry->getTerritoryName() == territoryName) troopsHere++;
                }
                for (CavalryPiece *cavalry : currentPlayer->getCavalry()) {
                    if (cavalry->getTerritoryName() == territoryName) troopsHere++;
                }
                for (CatapultPiece *catapult : currentPlayer->getCatapults()) {
                    if (catapult->getTerritoryName() == territoryName) troopsHere++;
                }

                if (leadersHere > 0 || troopsHere > 0) {
                    // Already have presence here
                    lines << QString("<font color='#00CC66'>✓ Already occupied</font>");
                    if (leadersHere > 0) {
                        lines << QString("  Leaders here: %1").arg(piecesHere.join(", "));
                    }
                    if (troopsHere > 0) {
                        lines << QString("  Troops here: %1").arg(troopsHere);
                    }
                } else {
                    // Check if we can reach it
                    QMap<QString, ReachInfo> reachable = calc.getAllReachable(currentPlayer, m_graph);
                    if (reachable.contains(territoryName)) {
                        const ReachInfo &info = reachable[territoryName];
                        lines << QString("<font color='#00CC66'>✓ Reachable this turn</font>");
                        lines << QString("  Leaders who can reach: %1").arg(info.leadersWhoCanReach.size());
                        for (GamePiece *leader : info.leadersWhoCanReach) {
                            QString leaderName = leader->getType() == GamePiece::Type::Caesar ? "Caesar" :
                                                 QString("General %1").arg(static_cast<GeneralPiece*>(leader)->getNumber());
                            lines << QString("    • %1 (%.1f moves left)").arg(leaderName).arg(info.bestMovesRemaining);
                        }
                        lines << QString("  Max troop strength: %1").arg(info.maxTroopStrength);
                        if (info.viaRoad) lines << "  <font color='#996633'>Via road network</font>";
                        if (info.viaGalley) lines << "  <font color='#3366CC'>Via galley transport</font>";
                    } else {
                        lines << QString("<font color='#CC0000'>✗ Not reachable this turn</font>");
                    }
                }
                break;
            }

            case HeatMapMode::MaxForceProjection: {
                // Count current forces at this territory
                int currentForce = 0;
                QStringList currentPieces;
                for (CaesarPiece *caesar : currentPlayer->getCaesars()) {
                    if (caesar->getTerritoryName() == territoryName) {
                        currentForce++;
                        currentPieces << "Caesar";
                    }
                }
                for (GeneralPiece *general : currentPlayer->getGenerals()) {
                    if (general->getTerritoryName() == territoryName) {
                        currentForce++;
                        currentPieces << QString("General %1").arg(general->getNumber());
                    }
                }
                for (InfantryPiece *infantry : currentPlayer->getInfantry()) {
                    if (infantry->getTerritoryName() == territoryName) currentForce++;
                }
                for (CavalryPiece *cavalry : currentPlayer->getCavalry()) {
                    if (cavalry->getTerritoryName() == territoryName) currentForce++;
                }
                for (CatapultPiece *catapult : currentPlayer->getCatapults()) {
                    if (catapult->getTerritoryName() == territoryName) currentForce++;
                }

                // Check reachable forces
                QMap<QString, ReachInfo> reachable = calc.getAllReachable(currentPlayer, m_graph);
                int reachableForce = 0;
                if (reachable.contains(territoryName)) {
                    reachableForce = reachable[territoryName].maxTroopStrength;
                }

                int maxForce = qMax(currentForce, reachableForce);

                if (maxForce > 0) {
                    lines << QString("<font color='#0066CC'>Max force: %1</font>").arg(maxForce);

                    if (currentForce > 0) {
                        lines << QString("  Currently here: %1").arg(currentForce);
                        if (!currentPieces.isEmpty()) {
                            lines << QString("    Leaders: %1").arg(currentPieces.join(", "));
                        }
                    }

                    if (reachable.contains(territoryName)) {
                        const ReachInfo &info = reachable[territoryName];
                        if (reachableForce > currentForce) {
                            lines << QString("  Can bring: %1 (via movement)").arg(reachableForce);
                        }
                        for (GamePiece *leader : info.leadersWhoCanReach) {
                            QString leaderName = leader->getType() == GamePiece::Type::Caesar ? "Caesar" :
                                                 QString("General %1").arg(static_cast<GeneralPiece*>(leader)->getNumber());
                            int troopCount = calc.calculateTroopStrength(leader, currentPlayer, 2.0 - info.bestMovesRemaining);
                            lines << QString("    • %1 can bring %2 troops").arg(leaderName).arg(troopCount);
                        }
                    }
                } else {
                    lines << QString("<font color='#666666'>No forces can reach</font>");
                }
                break;
            }

            case HeatMapMode::MaxForceProjectionTwoTurn: {
                // Count current forces at this territory
                int currentForce = 0;
                QStringList currentPieces;
                for (CaesarPiece *caesar : currentPlayer->getCaesars()) {
                    if (caesar->getTerritoryName() == territoryName) {
                        currentForce++;
                        currentPieces << "Caesar";
                    }
                }
                for (GeneralPiece *general : currentPlayer->getGenerals()) {
                    if (general->getTerritoryName() == territoryName) {
                        currentForce++;
                        currentPieces << QString("General %1").arg(general->getNumber());
                    }
                }
                for (InfantryPiece *infantry : currentPlayer->getInfantry()) {
                    if (infantry->getTerritoryName() == territoryName) currentForce++;
                }
                for (CavalryPiece *cavalry : currentPlayer->getCavalry()) {
                    if (cavalry->getTerritoryName() == territoryName) currentForce++;
                }
                for (CatapultPiece *catapult : currentPlayer->getCatapults()) {
                    if (catapult->getTerritoryName() == territoryName) currentForce++;
                }

                // Check reachable forces with 2-turn projection
                QMap<QString, ReachInfo> reachable = calc.getAllReachable(currentPlayer, m_graph, 2);
                int reachableForce = 0;
                if (reachable.contains(territoryName)) {
                    reachableForce = reachable[territoryName].maxTroopStrength;
                }

                int maxForce = qMax(currentForce, reachableForce);

                if (maxForce > 0) {
                    lines << QString("<font color='#0066CC'>Max force (2 turns): %1</font>").arg(maxForce);

                    if (currentForce > 0) {
                        lines << QString("  Currently here: %1").arg(currentForce);
                        if (!currentPieces.isEmpty()) {
                            lines << QString("    Leaders: %1").arg(currentPieces.join(", "));
                        }
                    }

                    if (reachable.contains(territoryName)) {
                        const ReachInfo &info = reachable[territoryName];
                        if (reachableForce > currentForce) {
                            lines << QString("  Can bring in 2 turns: %1").arg(reachableForce);
                        }
                        for (GamePiece *leader : info.leadersWhoCanReach) {
                            QString leaderName = leader->getType() == GamePiece::Type::Caesar ? "Caesar" :
                                                 QString("General %1").arg(static_cast<GeneralPiece*>(leader)->getNumber());
                            // Use turnMultiplier=2 for troop strength calculation
                            double movesUsed = 4.0 - info.bestMovesRemaining;  // 4.0 is max moves with multiplier
                            int troopCount = calc.calculateTroopStrength(leader, currentPlayer, movesUsed, 2);
                            lines << QString("    • %1 can bring %2 troops").arg(leaderName).arg(troopCount);
                        }
                    }
                } else {
                    lines << QString("<font color='#666666'>No forces can reach in 2 turns</font>");
                }
                break;
            }

            case HeatMapMode::EnemyThreat: {
                int totalThreat = 0;
                QStringList threatDetails;

                for (Player *player : m_players) {
                    if (player == currentPlayer) continue;

                    // Count current enemy troops at this territory
                    int currentTroops = 0;
                    bool hasLeaderHere = false;
                    for (CaesarPiece *caesar : player->getCaesars()) {
                        if (caesar->getTerritoryName() == territoryName) {
                            currentTroops++;
                            hasLeaderHere = true;
                        }
                    }
                    for (GeneralPiece *general : player->getGenerals()) {
                        if (general->getTerritoryName() == territoryName) {
                            currentTroops++;
                            hasLeaderHere = true;
                        }
                    }
                    for (InfantryPiece *infantry : player->getInfantry()) {
                        if (infantry->getTerritoryName() == territoryName) currentTroops++;
                    }
                    for (CavalryPiece *cavalry : player->getCavalry()) {
                        if (cavalry->getTerritoryName() == territoryName) currentTroops++;
                    }
                    for (CatapultPiece *catapult : player->getCatapults()) {
                        if (catapult->getTerritoryName() == territoryName) currentTroops++;
                    }

                    if (currentTroops > 0) {
                        totalThreat = qMax(totalThreat, currentTroops);
                        threatDetails << QString("  <font color='#FF0000'>Player %1: %2 troops HERE</font>")
                            .arg(player->getId())
                            .arg(currentTroops);
                    }

                    // Also check what they can bring
                    QMap<QString, ReachInfo> enemyReach = calc.getAllReachable(player, m_graph);
                    if (enemyReach.contains(territoryName)) {
                        const ReachInfo &info = enemyReach[territoryName];
                        totalThreat = qMax(totalThreat, info.maxTroopStrength);
                        if (currentTroops == 0) {  // Don't duplicate if already shown
                            threatDetails << QString("  Player %1: %2 troops can reach (%3 leader%4)")
                                .arg(player->getId())
                                .arg(info.maxTroopStrength)
                                .arg(info.leadersWhoCanReach.size())
                                .arg(info.leadersWhoCanReach.size() > 1 ? "s" : "");
                        } else if (info.maxTroopStrength > currentTroops) {
                            // Can reinforce
                            threatDetails << QString("    + %1 more can reinforce")
                                .arg(info.maxTroopStrength - currentTroops);
                        }
                    }
                }

                if (totalThreat > 0) {
                    QString threatColor = totalThreat > 6 ? "#CC0000" : totalThreat > 3 ? "#CC6600" : "#CCCC00";
                    lines << QString("<font color='%1'>Max enemy threat: %2</font>").arg(threatColor).arg(totalThreat);
                    lines << threatDetails;
                } else {
                    lines << QString("<font color='#00CC00'>No enemy threat this turn</font>");
                }
                break;
            }

            case HeatMapMode::EnemyThreatTwoTurn: {
                // Combined threat: max(1-turn threat, 0.5 * 2-turn threat)
                float maxOneTurn = 0;
                float maxTwoTurn = 0;
                QStringList threatDetails;

                for (Player *player : m_players) {
                    if (player == currentPlayer) continue;

                    // Count current enemy troops at this territory
                    int currentTroops = 0;
                    for (CaesarPiece *caesar : player->getCaesars()) {
                        if (caesar->getTerritoryName() == territoryName) currentTroops++;
                    }
                    for (GeneralPiece *general : player->getGenerals()) {
                        if (general->getTerritoryName() == territoryName) currentTroops++;
                    }
                    for (InfantryPiece *infantry : player->getInfantry()) {
                        if (infantry->getTerritoryName() == territoryName) currentTroops++;
                    }
                    for (CavalryPiece *cavalry : player->getCavalry()) {
                        if (cavalry->getTerritoryName() == territoryName) currentTroops++;
                    }
                    for (CatapultPiece *catapult : player->getCatapults()) {
                        if (catapult->getTerritoryName() == territoryName) currentTroops++;
                    }

                    if (currentTroops > 0) {
                        maxOneTurn = qMax(maxOneTurn, static_cast<float>(currentTroops));
                        threatDetails << QString("  <font color='#FF0000'>Player %1: %2 troops HERE</font>")
                            .arg(player->getId())
                            .arg(currentTroops);
                    }

                    // 1-turn reachability
                    QMap<QString, ReachInfo> enemyReach1 = calc.getAllReachable(player, m_graph);
                    if (enemyReach1.contains(territoryName)) {
                        const ReachInfo &info = enemyReach1[territoryName];
                        maxOneTurn = qMax(maxOneTurn, static_cast<float>(info.maxTroopStrength));
                        if (currentTroops == 0) {
                            threatDetails << QString("  Player %1: %2 troops (1 turn)")
                                .arg(player->getId())
                                .arg(info.maxTroopStrength);
                        }
                    }

                    // 2-turn reachability (use turnMultiplier=2)
                    QMap<QString, ReachInfo> enemyReach2 = calc.getAllReachable(player, m_graph, 2);
                    if (enemyReach2.contains(territoryName)) {
                        const ReachInfo &info = enemyReach2[territoryName];
                        maxTwoTurn = qMax(maxTwoTurn, static_cast<float>(info.maxTroopStrength));
                        // Only show 2-turn threat if not already shown as 1-turn
                        if (!enemyReach1.contains(territoryName) && currentTroops == 0) {
                            threatDetails << QString("  <font color='#CC6600'>Player %1: %2 troops (2 turns)</font>")
                                .arg(player->getId())
                                .arg(info.maxTroopStrength);
                        }
                    }
                }

                float combinedThreat = qMax(maxOneTurn, 0.5f * maxTwoTurn);

                if (combinedThreat > 0) {
                    QString threatColor = combinedThreat > 6 ? "#CC0000" : combinedThreat > 3 ? "#CC6600" : "#CCCC00";
                    lines << QString("<font color='%1'>Combined threat: %.1f</font>").arg(threatColor).arg(combinedThreat);
                    lines << QString("  1-turn max: %1, 2-turn max: %2")
                        .arg(static_cast<int>(maxOneTurn))
                        .arg(static_cast<int>(maxTwoTurn));
                    lines << QString("  Formula: max(%1, 0.5 × %2) = %.1f")
                        .arg(static_cast<int>(maxOneTurn))
                        .arg(static_cast<int>(maxTwoTurn))
                        .arg(combinedThreat);
                    lines << threatDetails;
                } else {
                    lines << QString("<font color='#00CC00'>No enemy threat within 2 turns</font>");
                }
                break;
            }

            case HeatMapMode::RiskLevel: {
                // Calculate our force (current + reachable)
                int ourCurrentForce = 0;
                for (CaesarPiece *caesar : currentPlayer->getCaesars()) {
                    if (caesar->getTerritoryName() == territoryName) ourCurrentForce++;
                }
                for (GeneralPiece *general : currentPlayer->getGenerals()) {
                    if (general->getTerritoryName() == territoryName) ourCurrentForce++;
                }
                for (InfantryPiece *infantry : currentPlayer->getInfantry()) {
                    if (infantry->getTerritoryName() == territoryName) ourCurrentForce++;
                }
                for (CavalryPiece *cavalry : currentPlayer->getCavalry()) {
                    if (cavalry->getTerritoryName() == territoryName) ourCurrentForce++;
                }
                for (CatapultPiece *catapult : currentPlayer->getCatapults()) {
                    if (catapult->getTerritoryName() == territoryName) ourCurrentForce++;
                }

                int ourMaxForce = ourCurrentForce;
                QMap<QString, ReachInfo> ourReach = calc.getAllReachable(currentPlayer, m_graph);
                if (ourReach.contains(territoryName)) {
                    ourMaxForce = qMax(ourMaxForce, ourReach[territoryName].maxTroopStrength);
                }

                // Calculate enemy force (current + reachable)
                int enemyCurrentForce = 0;
                int enemyMaxForce = 0;
                for (Player *player : m_players) {
                    if (player == currentPlayer) continue;

                    int thisEnemyCurrent = 0;
                    for (CaesarPiece *caesar : player->getCaesars()) {
                        if (caesar->getTerritoryName() == territoryName) thisEnemyCurrent++;
                    }
                    for (GeneralPiece *general : player->getGenerals()) {
                        if (general->getTerritoryName() == territoryName) thisEnemyCurrent++;
                    }
                    for (InfantryPiece *infantry : player->getInfantry()) {
                        if (infantry->getTerritoryName() == territoryName) thisEnemyCurrent++;
                    }
                    for (CavalryPiece *cavalry : player->getCavalry()) {
                        if (cavalry->getTerritoryName() == territoryName) thisEnemyCurrent++;
                    }
                    for (CatapultPiece *catapult : player->getCatapults()) {
                        if (catapult->getTerritoryName() == territoryName) thisEnemyCurrent++;
                    }
                    enemyCurrentForce += thisEnemyCurrent;

                    QMap<QString, ReachInfo> enemyReach = calc.getAllReachable(player, m_graph);
                    if (enemyReach.contains(territoryName)) {
                        enemyMaxForce = qMax(enemyMaxForce, enemyReach[territoryName].maxTroopStrength);
                    }
                    enemyMaxForce = qMax(enemyMaxForce, thisEnemyCurrent);
                }

                // Determine risk level
                QString riskStr, riskColor;
                if (ourMaxForce == 0 && enemyMaxForce == 0) {
                    riskStr = "NEUTRAL"; riskColor = "#808080";
                } else if (ourMaxForce == 0 && enemyMaxForce > 0) {
                    riskStr = enemyCurrentForce > 0 ? "ENEMY OCCUPIED" : "ENEMY REACHABLE";
                    riskColor = "#B40000";
                } else if (ourMaxForce > 0 && enemyMaxForce == 0) {
                    riskStr = "SAFE"; riskColor = "#00B400";
                } else if (ourMaxForce > enemyMaxForce * 1.5) {
                    riskStr = "SAFE"; riskColor = "#00B400";
                } else if (ourMaxForce > enemyMaxForce) {
                    riskStr = "LOW"; riskColor = "#64C864";
                } else if (ourMaxForce * 1.5 >= enemyMaxForce) {
                    riskStr = "MEDIUM"; riskColor = "#FFC800";
                } else {
                    riskStr = "HIGH"; riskColor = "#FF3232";
                }

                lines << QString("<font color='%1'><b>Risk: %2</b></font>").arg(riskColor).arg(riskStr);
                lines << QString("  Our force: %1 here, %2 max").arg(ourCurrentForce).arg(ourMaxForce);
                lines << QString("  Enemy force: %1 here, %2 max").arg(enemyCurrentForce).arg(enemyMaxForce);
                break;
            }

            default:
                break;
        }

        lines << "";  // Separator before normal tooltip content
    }

    // Check if it's a sea territory
    if (m_graph->isSeaTerritory(territoryName)) {
        lines << "<i>Sea Territory</i>";
    }

    // Find owner and pieces in this territory
    QChar owner = '\0';
    QStringList buildings;
    QStringList pieces;

    for (Player *player : m_players) {
        if (!player) continue;

        // Check if player owns this territory
        if (player->getOwnedTerritories().contains(territoryName)) {
            owner = player->getId();
        }

        // Check for buildings (cities/fortifications)
        for (City *city : player->getCities()) {
            if (city->getTerritoryName() == territoryName) {
                if (city->isFortified()) {
                    buildings << QString("Fortified City (Player %1)").arg(player->getId());
                } else {
                    buildings << QString("City (Player %1)").arg(player->getId());
                }
            }
        }

        // Check for pieces
        int infantryCount = 0;
        int cavalryCount = 0;
        int catapultCount = 0;
        int galleyCount = 0;
        bool hasCaesar = false;
        QStringList generals;

        for (CaesarPiece *caesar : player->getCaesars()) {
            if (caesar->getTerritoryName() == territoryName) {
                hasCaesar = true;
            }
        }

        for (GeneralPiece *general : player->getGenerals()) {
            if (general->getTerritoryName() == territoryName) {
                generals << QString("General %1").arg(general->getNumber());
            }
        }

        for (InfantryPiece *infantry : player->getInfantry()) {
            if (infantry->getTerritoryName() == territoryName) {
                infantryCount++;
            }
        }

        for (CavalryPiece *cavalry : player->getCavalry()) {
            if (cavalry->getTerritoryName() == territoryName) {
                cavalryCount++;
            }
        }

        for (CatapultPiece *catapult : player->getCatapults()) {
            if (catapult->getTerritoryName() == territoryName) {
                catapultCount++;
            }
        }

        for (GalleyPiece *galley : player->getGalleys()) {
            if (galley->getTerritoryName() == territoryName) {
                galleyCount++;
            }
        }

        // Build pieces string for this player
        if (hasCaesar || !generals.isEmpty() || infantryCount > 0 || cavalryCount > 0 || catapultCount > 0 || galleyCount > 0) {
            QStringList playerPieces;
            playerPieces << QString("Player %1:").arg(player->getId());
            if (hasCaesar) playerPieces << "    1 Caesar";
            if (!generals.isEmpty()) playerPieces << QString("    %1 General%2").arg(generals.size()).arg(generals.size() > 1 ? "s" : "");
            if (infantryCount > 0) playerPieces << QString("    %1 Infantry").arg(infantryCount);
            if (cavalryCount > 0) playerPieces << QString("    %1 Cavalry").arg(cavalryCount);
            if (catapultCount > 0) playerPieces << QString("    %1 Catapult%2").arg(catapultCount).arg(catapultCount > 1 ? "s" : "");
            if (galleyCount > 0) playerPieces << QString("    %1 Galley%2").arg(galleyCount).arg(galleyCount > 1 ? "s" : "");

            pieces << playerPieces.join("<br>");
        }
    }

    // Add owner info
    if (owner != '\0') {
        lines << QString("<font color='blue'>Owner: Player %1</font>").arg(owner);
    } else {
        lines << "<font color='gray'>Unowned</font>";
    }

    // Add buildings
    if (!buildings.isEmpty()) {
        lines << "";
        lines << "<b>Buildings:</b>";
        for (const QString &building : buildings) {
            lines << QString("  • %1").arg(building);
        }
    }

    // Add pieces
    if (!pieces.isEmpty()) {
        lines << "";
        lines << "<b>Pieces:</b>";
        for (const QString &piece : pieces) {
            lines << QString("  • %1").arg(piece);
        }
    }

    return lines.join("<br>");
}

void GameMapWidget::playMenuClickSound(QAction *action)
{
    // Only play if this is a different action than the last one hovered AND enough time has passed
    if (m_clickSound && action && action != m_lastHoveredAction && m_clickTimer.elapsed() > 50) {
        m_lastHoveredAction = action;
        if (m_clickSound->isPlaying()) {
            m_clickSound->stop();
        }
        m_clickSound->play();
        m_clickTimer.restart();
    }
}
