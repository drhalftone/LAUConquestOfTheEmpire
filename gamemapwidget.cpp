#include "gamemapwidget.h"

#include <QDebug>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QtMath>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
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
    m_audioOutput = new QAudioOutput(this);
    m_audioOutput->setVolume(0.5f);
    m_clickPlayer = new QMediaPlayer(this);
    m_clickPlayer->setAudioOutput(m_audioOutput);
    m_clickPlayer->setSource(QUrl("qrc:/images/click.mp3"));

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

    QMenu *fileMenu = m_menuBar->addMenu("&File");
    fileMenu->addAction("&Save Game", this, &GameMapWidget::saveGame, QKeySequence::Save);
    fileMenu->addAction("&Load Game", this, &GameMapWidget::loadGame, QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close, QKeySequence::Quit);

    QMenu *helpMenu = m_menuBar->addMenu("&Help");
    helpMenu->addAction("&About", this, &GameMapWidget::showAbout);
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

        // Play click sound when a new territory is activated
        if (newTerritory > 0) {
            m_clickPlayer->setPosition(0);
            m_clickPlayer->play();
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
    if (!m_graph) return;

    QString territoryName = m_graph->getTerritoryNameById(territoryId);
    if (territoryName.isEmpty()) return;

    Territory territory = m_graph->getTerritory(territoryName);
    int originalTerritory = territoryId;

    QMenu menu(this);

    // Header with territory name and value
    QString headerText = QString("%1").arg(territory.name);
    if (territory.value > 0) {
        headerText += QString(" (%1 pts)").arg(territory.value);
    } else {
        headerText += " (Sea)";
    }
    QAction *header = menu.addAction(headerText);
    header->setEnabled(false);
    QFont boldFont = header->font();
    boldFont.setBold(true);
    header->setFont(boldFont);

    menu.addSeparator();

    // Show owner if any
    // TODO: Get owner from player data
    // QAction *ownerAction = menu.addAction("Owner: None");
    // ownerAction->setEnabled(false);

    // Add neighbors section
    QAction *neighborsLabel = menu.addAction("Neighbors:");
    neighborsLabel->setEnabled(false);

    QMap<QAction*, int> actionToNeighbor;

    for (const QString &neighborName : territory.neighbors) {
        Territory neighbor = m_graph->getTerritory(neighborName);
        if (neighbor.id > 0) {
            QString label = QString("  %1").arg(neighborName);
            if (neighbor.value > 0) {
                label += QString(" (%1)").arg(neighbor.value);
            }
            QAction *action = menu.addAction(label);
            actionToNeighbor[action] = neighbor.id;
        }
    }

    if (territory.neighbors.isEmpty()) {
        QAction *noNeighbors = menu.addAction("  (none)");
        noNeighbors->setEnabled(false);
    }

    // Hover timer to highlight neighbors
    QTimer hoverTimer;
    hoverTimer.setInterval(50);
    connect(&hoverTimer, &QTimer::timeout, this, [this, &menu, &actionToNeighbor, originalTerritory]() {
        QAction *activeAction = menu.activeAction();
        if (activeAction && actionToNeighbor.contains(activeAction)) {
            int neighborId = actionToNeighbor[activeAction];
            if (m_hoveredTerritoryId != neighborId) {
                m_hoveredTerritoryId = neighborId;
                m_clickPlayer->setPosition(0);
                m_clickPlayer->play();
                update();
            }
        } else {
            if (m_hoveredTerritoryId != originalTerritory) {
                m_hoveredTerritoryId = originalTerritory;
                m_clickPlayer->setPosition(0);
                m_clickPlayer->play();
                update();
            }
        }
    });
    hoverTimer.start();

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
    QMessageBox::about(this, "About Conquest of the Empire",
        "Conquest of the Empire\n\n"
        "A strategy board game set in ancient Rome.\n\n"
        "OpenGL Map Version");
}
