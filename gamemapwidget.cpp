#include "gamemapwidget.h"
#include "player.h"
#include "building.h"
#include "gamepiece.h"
#include "playerinfowidget.h"

#include <QDebug>
#include <QMenu>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QtMath>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QUrl>

// Vertex shader - transforms position by MVP matrix
static const char *vertexShaderSource = R"(
    #version 330 core
    layout(location = 0) in vec2 position;
    layout(location = 1) in vec2 texCoord;

    uniform mat4 mvp;

    out vec2 fragTexCoord;

    void main() {
        gl_Position = mvp * vec4(position, 0.0, 1.0);
        fragTexCoord = texCoord;
    }
)";

// Fragment shader - sample map texture, highlight hovered territory
static const char *fragmentShaderSource = R"(
    #version 330 core
    in vec2 fragTexCoord;
    out vec4 fragColor;

    uniform sampler2D mapTexture;
    uniform sampler2D indexTexture;
    uniform int highlightedTerritory;

    void main() {
        vec4 mapColor = texture(mapTexture, fragTexCoord);

        // Sample territory index (8-bit grayscale, value 0-255)
        float territoryValue = texture(indexTexture, fragTexCoord).r * 255.0;
        int territoryId = int(territoryValue + 0.5);  // Round to nearest int

        if (highlightedTerritory > 0) {
            // A territory is being hovered
            if (territoryId == highlightedTerritory) {
                // Brighten and add yellow tint to highlighted territory
                vec3 highlight = vec3(1.0, 1.0, 0.6);  // Warm yellow
                fragColor = vec4(mix(mapColor.rgb, highlight, 0.35), 1.0);
            } else {
                // Darken non-highlighted areas
                fragColor = vec4(mapColor.rgb * 0.7, 1.0);
            }
        } else {
            // No territory hovered - show normal
            fragColor = mapColor;
        }
    }
)";

GameMapWidget::GameMapWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_vbo(QOpenGLBuffer::VertexBuffer)
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
    delete m_mapTexture;
    delete m_indexTexture;
    m_vbo.destroy();
    m_vao.destroy();

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
    updateMvpMatrix();
}

void GameMapWidget::createShaders()
{
    m_shaderProgram = new QOpenGLShaderProgram(this);

    if (!m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        qWarning() << "Vertex shader compilation failed:" << m_shaderProgram->log();
        return;
    }

    if (!m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        qWarning() << "Fragment shader compilation failed:" << m_shaderProgram->log();
        return;
    }

    if (!m_shaderProgram->link()) {
        qWarning() << "Shader linking failed:" << m_shaderProgram->log();
        return;
    }

    qDebug() << "Shaders compiled and linked successfully";
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
    // Account for menu bar in viewport
    int menuHeight = m_menuBar ? m_menuBar->height() * devicePixelRatio() : 0;
    glViewport(0, 0, m_windowPixelSize.width(), m_windowPixelSize.height());
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_shaderProgram || !m_mapTexture || !m_indexTexture) {
        return;
    }

    m_shaderProgram->bind();
    m_shaderProgram->setUniformValue("mvp", m_mvpMatrix);
    m_shaderProgram->setUniformValue("highlightedTerritory", m_hoveredTerritoryId);

    // Bind map texture to unit 0
    glActiveTexture(GL_TEXTURE0);
    m_mapTexture->bind();
    m_shaderProgram->setUniformValue("mapTexture", 0);

    // Bind index texture to unit 1
    glActiveTexture(GL_TEXTURE1);
    m_indexTexture->bind();
    m_shaderProgram->setUniformValue("indexTexture", 1);

    m_vao.bind();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    m_vao.release();

    // Release textures
    glActiveTexture(GL_TEXTURE1);
    m_indexTexture->release();
    glActiveTexture(GL_TEXTURE0);
    m_mapTexture->release();

    m_shaderProgram->release();
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

QColor GameMapWidget::getPlayerColor(QChar player) const
{
    switch (player.toLatin1()) {
        case 'A': return QColor(255, 0, 0);      // Red
        case 'B': return QColor(0, 0, 255);      // Blue
        case 'C': return QColor(0, 200, 0);      // Green
        case 'D': return QColor(255, 255, 0);    // Yellow
        case 'E': return QColor(255, 165, 0);    // Orange
        case 'F': return QColor(128, 0, 128);    // Purple
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

void GameMapWidget::showTerritoryContextMenu(const QPoint &pos, int territoryId)
{
    if (!m_graph || !m_playerInfoWidget) return;

    QString territoryName = m_graph->getTerritoryNameById(territoryId);
    if (territoryName.isEmpty()) return;

    Territory territory = m_graph->getTerritory(territoryName);
    int originalTerritory = territoryId;

    QMenu menu(this);

    // Find the owner of this territory
    QChar owner = '\0';
    for (Player *player : m_players) {
        if (player && player->ownsTerritory(territoryName)) {
            owner = player->getId();
            break;
        }
    }

    // Header with territory name and value
    QString headerText = QString("%1").arg(territory.name);
    if (territory.value > 0) {
        headerText += QString(" (%1 pts)").arg(territory.value);
    } else {
        headerText += " (Sea)";
    }

    // Determine flag icon based on owner
    QIcon flagIcon;
    if (owner != '\0') {
        QString flagPath;
        switch (owner.toLatin1()) {
            case 'A': flagPath = ":/images/redFlag.png"; break;
            case 'B': flagPath = ":/images/greenFlag.png"; break;
            case 'C': flagPath = ":/images/blueFlag.png"; break;
            case 'D': flagPath = ":/images/yellowFlag.png"; break;
            case 'E': flagPath = ":/images/blackFlag.png"; break;
            case 'F': flagPath = ":/images/orangeFlag.png"; break;
        }
        if (!flagPath.isEmpty()) {
            flagIcon = QIcon(flagPath);
        }
    }

    QAction *header = menu.addAction(flagIcon, headerText);
    header->setEnabled(false);
    QFont boldFont = header->font();
    boldFont.setBold(true);
    header->setFont(boldFont);

    menu.addSeparator();

    // Find all movable pieces in this territory for the CURRENT player only
    bool foundPieces = false;

    // Map to track which actions correspond to which territories (for hover highlighting)
    QMap<QAction*, QString> actionToTerritory;

    // Only show movement options for the current player's pieces
    if (m_currentPlayerIndex >= 0 && m_currentPlayerIndex < m_players.size()) {
        Player *currentPlayer = m_players[m_currentPlayerIndex];
        if (currentPlayer) {
            // Check for Caesars
            for (CaesarPiece *caesar : currentPlayer->getCaesars()) {
                if (caesar->getTerritoryName() == territoryName && caesar->getMovesRemaining() > 0) {
                    QMenu *caesarMenu = menu.addMenu(QIcon(":/images/ceasarIcon.png"), QString("Caesar (Player %1)").arg(currentPlayer->getId()));
                    addMovementOptionsToMenu(caesarMenu, caesar, territoryName, actionToTerritory);
                    foundPieces = true;
                }
            }

            // Check for Generals
            for (GeneralPiece *general : currentPlayer->getGenerals()) {
                if (general->getTerritoryName() == territoryName && general->getMovesRemaining() > 0) {
                    QMenu *generalMenu = menu.addMenu(QIcon(":/images/generalIcon.png"), QString("General %1 (Player %2)").arg(general->getNumber()).arg(currentPlayer->getId()));
                    addMovementOptionsToMenu(generalMenu, general, territoryName, actionToTerritory);
                    foundPieces = true;
                }
            }

            // Check for Galleys
            for (GalleyPiece *galley : currentPlayer->getGalleys()) {
                if (galley->getTerritoryName() == territoryName && galley->getMovesRemaining() > 0) {
                    QMenu *galleyMenu = menu.addMenu(QIcon(":/images/galleyIcon.png"), QString("Galley (Player %1)").arg(currentPlayer->getId()));
                    addMovementOptionsToMenu(galleyMenu, galley, territoryName, actionToTerritory);
                    foundPieces = true;
                }
            }
        }
    }

    if (!foundPieces) {
        QAction *noPieces = menu.addAction("No movable pieces here");
        noPieces->setEnabled(false);
    }

    // Hover timer to highlight territories as user hovers over menu items
    QTimer hoverTimer;
    hoverTimer.setInterval(50);
    connect(&hoverTimer, &QTimer::timeout, this, [this, &actionToTerritory, originalTerritory]() {
        // Find the currently active menu item (could be in main menu or submenu)
        QAction *activeAction = nullptr;
        QWidget *activeWidget = QApplication::activePopupWidget();
        if (activeWidget) {
            QMenu *activeMenu = qobject_cast<QMenu*>(activeWidget);
            if (activeMenu) {
                activeAction = activeMenu->activeAction();
            }
        }

        if (activeAction && actionToTerritory.contains(activeAction)) {
            QString hoveredTerritoryName = actionToTerritory[activeAction];
            Territory hoveredTerritory = m_graph->getTerritory(hoveredTerritoryName);
            if (hoveredTerritory.id > 0 && m_hoveredTerritoryId != hoveredTerritory.id) {
                m_hoveredTerritoryId = hoveredTerritory.id;
                // Play click sound for menu hover using the shared method
                playMenuClickSound(activeAction);
                update();
            }
        } else {
            if (m_hoveredTerritoryId != originalTerritory) {
                m_hoveredTerritoryId = originalTerritory;
                update();
            }
        }
    });
    hoverTimer.start();

    // Reset last hovered action when menu opens
    m_lastHoveredAction = nullptr;

    // Connect hover sound to all menus (main and submenus)
    connect(&menu, &QMenu::hovered, this, &GameMapWidget::playMenuClickSound);
    for (QMenu *submenu : menu.findChildren<QMenu*>()) {
        connect(submenu, &QMenu::hovered, this, &GameMapWidget::playMenuClickSound);
    }

    menu.exec(mapToGlobal(pos));

    hoverTimer.stop();

    // Restore hover after menu closes
    QPoint globalPos = QCursor::pos();
    QPoint localPos = mapFromGlobal(globalPos);
    if (rect().contains(localPos)) {
        updateHoveredTerritory(localPos);
    } else {
        m_hoveredTerritoryId = 0;
        update();
    }
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
        if (m_hoveredTerritoryId > 0) {
            showTerritoryContextMenu(event->pos(), m_hoveredTerritoryId);
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
    // TODO: Prompt to save game
    event->accept();
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
    QString fileName = QFileDialog::getSaveFileName(this, "Save Game", "", "COE Save Files (*.coe);;All Files (*)");
    if (fileName.isEmpty()) return;

    // TODO: Implement save game
    QMessageBox::information(this, "Save Game", "Save game not yet implemented for OpenGL map.");
}

void GameMapWidget::loadGame()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Load Game", "", "COE Save Files (*.coe);;All Files (*)");
    if (fileName.isEmpty()) return;

    // TODO: Implement load game
    QMessageBox::information(this, "Load Game", "Load game not yet implemented for OpenGL map.");
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

void GameMapWidget::updateRoads()
{
    // Roads are managed by Player objects in OpenGL version
    // This is a no-op for compatibility
    // If needed, trigger a repaint to show updated roads
    update();
}

QString GameMapWidget::buildTerritoryTooltip(const QString &territoryName) const
{
    if (territoryName.isEmpty()) {
        return "";
    }

    QStringList lines;

    // Territory name and value
    int territoryValue = m_graph->getValue(territoryName);
    lines << QString("<b>%1</b>").arg(territoryName);
    lines << QString("Value: %1").arg(territoryValue);

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
            if (hasCaesar) playerPieces << "Caesar";
            if (!generals.isEmpty()) playerPieces << generals.join(", ");
            if (infantryCount > 0) playerPieces << QString("%1 Infantry").arg(infantryCount);
            if (cavalryCount > 0) playerPieces << QString("%1 Cavalry").arg(cavalryCount);
            if (catapultCount > 0) playerPieces << QString("%1 Catapult").arg(catapultCount);
            if (galleyCount > 0) playerPieces << QString("%1 Galley").arg(galleyCount);

            pieces << QString("Player %1: %2").arg(player->getId()).arg(playerPieces.join(", "));
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

void GameMapWidget::addMovementOptionsToMenu(QMenu *menu, GamePiece *piece, const QString &fromTerritory, QMap<QAction*, QString> &actionToTerritory)
{
    if (!menu || !piece || !m_graph || !m_playerInfoWidget) return;

    // Get neighboring territories from the graph
    Territory territory = m_graph->getTerritory(fromTerritory);

    if (territory.neighbors.isEmpty()) {
        QAction *noMoves = menu->addAction("No adjacent territories");
        noMoves->setEnabled(false);
        return;
    }

    // Check if this piece is a galley (galleys can move to sea, generals/caesars cannot)
    GalleyPiece *galley = dynamic_cast<GalleyPiece*>(piece);
    bool isGalley = (galley != nullptr);

    // Add each neighbor as a movement option
    for (const QString &neighborName : territory.neighbors) {
        Territory neighbor = m_graph->getTerritory(neighborName);

        bool isSea = (neighbor.value == 0);

        // Find who owns this territory
        QChar owner = '\0';
        for (Player *player : m_players) {
            if (player && player->ownsTerritory(neighborName)) {
                owner = player->getId();
                break;
            }
        }

        // For sea territories and non-galley pieces, check if there's a galley we can board
        bool hasGalley = false;
        if (isSea && !isGalley && m_players.size() > m_currentPlayerIndex) {
            Player *currentPlayer = m_players[m_currentPlayerIndex];
            if (currentPlayer && currentPlayer->getId() == piece->getPlayer()) {
                // Check if this player has a galley in this sea territory
                for (GalleyPiece *galley : currentPlayer->getGalleys()) {
                    if (galley->getTerritoryName() == neighborName) {
                        hasGalley = true;
                        break;
                    }
                }
            }
        }

        // Build display text with territory value
        QString displayText = neighborName;
        if (neighbor.value > 0) {
            displayText += QString(" (%1)").arg(neighbor.value);
        } else {
            displayText += " (Sea)";
        }

        // Determine icon based on owner or galley availability
        QIcon itemIcon;
        if (isSea && hasGalley) {
            // Show galley icon for sea territories with available galley
            itemIcon = QIcon(":/images/galleyIcon.png");
        } else if (owner != '\0' && !isSea) {
            // Show flag icon for owned land territories
            QString flagPath;
            switch (owner.toLatin1()) {
                case 'A': flagPath = ":/images/redFlag.png"; break;
                case 'B': flagPath = ":/images/greenFlag.png"; break;
                case 'C': flagPath = ":/images/blueFlag.png"; break;
                case 'D': flagPath = ":/images/yellowFlag.png"; break;
                case 'E': flagPath = ":/images/blackFlag.png"; break;
                case 'F': flagPath = ":/images/orangeFlag.png"; break;
            }
            if (!flagPath.isEmpty()) {
                itemIcon = QIcon(flagPath);
            }
        }

        QAction *moveAction = menu->addAction(itemIcon, displayText);

        // Disable sea territories unless this is a galley or there's a galley to board
        if (isSea) {
            moveAction->setEnabled(isGalley || hasGalley);
        }

        // Track this action for hover highlighting
        actionToTerritory[moveAction] = neighborName;

        // Connect the action to trigger movement via PlayerInfoWidget
        connect(moveAction, &QAction::triggered, [this, piece, neighborName]() {
            // Delegate to PlayerInfoWidget to handle the movement with legion composition dialog
            if (m_playerInfoWidget) {
                m_playerInfoWidget->moveLeaderToTerritory(piece, neighborName);
            }
        });
    }
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
