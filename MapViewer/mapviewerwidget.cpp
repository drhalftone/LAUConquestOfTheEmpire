#include "mapviewerwidget.h"
#include <QDebug>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QtMath>
#include <QFile>
#include <QTextStream>
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

MapViewerWidget::MapViewerWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_vbo(QOpenGLBuffer::VertexBuffer)
{
    // Set minimum size
    setMinimumSize(800, 600);

    // Enable mouse tracking for hover detection
    setMouseTracking(true);

    // Setup momentum timer (60 FPS)
    m_momentumTimer.setInterval(16);
    connect(&m_momentumTimer, &QTimer::timeout, this, &MapViewerWidget::onMomentumTick);

    // Setup click sound
    m_audioOutput = new QAudioOutput(this);
    m_audioOutput->setVolume(0.5f);
    m_clickPlayer = new QMediaPlayer(this);
    m_clickPlayer->setAudioOutput(m_audioOutput);
    m_clickPlayer->setSource(QUrl("qrc:/click.mp3"));
}

MapViewerWidget::~MapViewerWidget()
{
    makeCurrent();

    delete m_shaderProgram;
    delete m_mapTexture;
    delete m_indexTexture;
    m_vbo.destroy();
    m_vao.destroy();

    doneCurrent();
}

void MapViewerWidget::initializeGL()
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
    loadTerritories();
    updateMvpMatrix();
}

void MapViewerWidget::createShaders()
{
    // Main map shader
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

void MapViewerWidget::createGeometry()
{
    // Quad vertices: position (x, y) and texCoord (u, v)
    // Position in NDC (-1 to 1), texCoord (0 to 1)
    // Flip V so image displays right-side up
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

void MapViewerWidget::loadTextures()
{
    // Load map image
    QImage mapImage(":/data/Map.jpg");
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

    // Load territory index image (for both GPU texture and CPU lookups)
    m_indexImage = QImage(":/data/territories_index.png");
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

void MapViewerWidget::loadTerritories()
{
    QFile file(":/data/territories.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open territories.csv";
        return;
    }

    QTextStream in(&file);

    // Skip header line
    if (!in.atEnd()) {
        in.readLine();
    }

    // Parse CSV: ID,Area,Color,CentroidX,CentroidY,Name,Points,Neighbors
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList fields = line.split(',');

        if (fields.size() >= 8) {
            Territory t;
            t.id = fields[0].toInt();
            t.centroid = QPointF(fields[3].toFloat(), fields[4].toFloat());
            t.name = fields[5];

            // Parse neighbors (semicolon-separated IDs)
            QString neighborsStr = fields[7];
            if (!neighborsStr.isEmpty()) {
                QStringList neighborIds = neighborsStr.split(';');
                for (const QString &nid : neighborIds) {
                    t.neighbors.append(nid.toInt());
                }
            }

            m_territories[t.id] = t;
        }
    }

    file.close();
    qDebug() << "Loaded" << m_territories.size() << "territories";
}

void MapViewerWidget::resizeGL(int w, int h)
{
    Q_UNUSED(w);
    Q_UNUSED(h);

    // Get actual pixel dimensions (for Retina displays)
    m_windowPixelSize = QSize(width() * devicePixelRatio(), height() * devicePixelRatio());

    if (m_mapSize.isEmpty()) {
        m_aspectScaleX = 1.0f;
        m_aspectScaleY = 1.0f;
        return;
    }

    // Calculate aspect ratio correction factors
    // These scale the quad so the map fits the window at zoom=1
    float mapAspect = static_cast<float>(m_mapSize.width()) / m_mapSize.height();
    float windowAspect = static_cast<float>(m_windowPixelSize.width()) / m_windowPixelSize.height();

    if (windowAspect > mapAspect) {
        // Window is wider than map - shrink X to fit
        m_aspectScaleX = mapAspect / windowAspect;
        m_aspectScaleY = 1.0f;
    } else {
        // Window is taller than map - shrink Y to fit
        m_aspectScaleX = 1.0f;
        m_aspectScaleY = windowAspect / mapAspect;
    }

    qDebug() << "Aspect scale:" << m_aspectScaleX << "x" << m_aspectScaleY
             << "Window:" << m_windowPixelSize.width() << "x" << m_windowPixelSize.height();

    updateMvpMatrix();
    update();
}

void MapViewerWidget::paintGL()
{
    // Use full window viewport - aspect ratio handled by MVP matrix
    glViewport(0, 0, m_windowPixelSize.width(), m_windowPixelSize.height());
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_shaderProgram || !m_mapTexture || !m_indexTexture) {
        return;
    }

    m_shaderProgram->bind();
    m_shaderProgram->setUniformValue("mvp", m_mvpMatrix);
    m_shaderProgram->setUniformValue("highlightedTerritory", m_hoveredTerritory);

    // Bind map texture to unit 0
    glActiveTexture(GL_TEXTURE0);
    m_mapTexture->bind();
    m_shaderProgram->setUniformValue("mapTexture", 0);

    // Bind index texture to unit 1
    glActiveTexture(GL_TEXTURE1);
    m_indexTexture->bind();
    m_shaderProgram->setUniformValue("indexTexture", 1);

    m_vao.bind();

    // Draw quad as triangle fan (4 vertices)
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    m_vao.release();

    // Release textures
    glActiveTexture(GL_TEXTURE1);
    m_indexTexture->release();
    glActiveTexture(GL_TEXTURE0);
    m_mapTexture->release();

    m_shaderProgram->release();
}

void MapViewerWidget::updateMvpMatrix()
{
    m_mvpMatrix.setToIdentity();

    // Apply pan (translate)
    m_mvpMatrix.translate(m_pan.x(), m_pan.y(), 0.0f);

    // Apply zoom (scale)
    m_mvpMatrix.scale(m_zoom, m_zoom, 1.0f);

    // Apply aspect ratio correction (makes map fit window at zoom=1)
    m_mvpMatrix.scale(m_aspectScaleX, m_aspectScaleY, 1.0f);
}

QPointF MapViewerWidget::widgetToNormalized(const QPointF &widgetPos) const
{
    // Convert widget coordinates to normalized coordinates (-1 to 1)
    // Using logical pixel coordinates (not physical)
    float nx = 2.0f * widgetPos.x() / width() - 1.0f;
    float ny = 1.0f - 2.0f * widgetPos.y() / height();
    return QPointF(nx, ny);
}

QPointF MapViewerWidget::widgetToMapCoords(const QPointF &widgetPos) const
{
    // Convert widget coordinates to map image coordinates (0 to mapWidth/Height)
    QPointF normPos = widgetToNormalized(widgetPos);

    // Inverse of MVP transform: mapPos = (screenPos - pan) / (aspectScale * zoom)
    // This gives us coordinates in the range -1 to 1 on the map quad
    float quadX = (normPos.x() - m_pan.x()) / (m_aspectScaleX * m_zoom);
    float quadY = (normPos.y() - m_pan.y()) / (m_aspectScaleY * m_zoom);

    // Convert from quad coords (-1 to 1) to texture coords (0 to 1)
    float texU = (quadX + 1.0f) / 2.0f;
    float texV = (1.0f - quadY) / 2.0f;  // Flip Y for image coordinates

    // Convert to map pixel coordinates
    float mapX = texU * m_mapSize.width();
    float mapY = texV * m_mapSize.height();

    return QPointF(mapX, mapY);
}

void MapViewerWidget::updateHoveredTerritory(const QPointF &widgetPos)
{
    if (m_indexImage.isNull()) {
        return;
    }

    QPointF mapCoords = widgetToMapCoords(widgetPos);
    int x = qBound(0, static_cast<int>(mapCoords.x()), m_indexImage.width() - 1);
    int y = qBound(0, static_cast<int>(mapCoords.y()), m_indexImage.height() - 1);

    // Get pixel value from grayscale index image
    int newTerritory = m_indexImage.pixelColor(x, y).red();  // Grayscale, so R=G=B

    if (newTerritory != m_hoveredTerritory) {
        m_hoveredTerritory = newTerritory;
        update();  // Trigger repaint
        qDebug() << "Hovered territory:" << m_hoveredTerritory;

        // Play click sound when a new territory is activated
        if (newTerritory > 0) {
            m_clickPlayer->setPosition(0);
            m_clickPlayer->play();
        }
    }
}

void MapViewerWidget::showTerritoryContextMenu(const QPoint &pos, int territoryId)
{
    if (!m_territories.contains(territoryId)) {
        return;
    }

    const Territory &territory = m_territories[territoryId];
    int originalTerritory = territoryId;  // Remember the clicked territory

    QMenu menu(this);
    menu.setTitle(territory.name);

    // Add territory name as disabled header
    QAction *header = menu.addAction(territory.name);
    header->setEnabled(false);
    QFont boldFont = header->font();
    boldFont.setBold(true);
    header->setFont(boldFont);

    menu.addSeparator();

    // Add "Neighbors:" label
    QAction *neighborsLabel = menu.addAction("Neighbors:");
    neighborsLabel->setEnabled(false);

    // Map from action to neighbor ID for hover detection
    QMap<QAction*, int> actionToNeighbor;

    // Add each neighbor by name (in order from CSV)
    for (int neighborId : territory.neighbors) {
        if (m_territories.contains(neighborId)) {
            QString neighborName = m_territories[neighborId].name;
            QAction *action = menu.addAction(QString("  %1").arg(neighborName));
            actionToNeighbor[action] = neighborId;
        }
    }

    if (territory.neighbors.isEmpty()) {
        QAction *noNeighbors = menu.addAction("  (none)");
        noNeighbors->setEnabled(false);
    }

    // Use a timer to poll the active action and update highlight
    QTimer hoverTimer;
    hoverTimer.setInterval(50);  // Check every 50ms
    connect(&hoverTimer, &QTimer::timeout, this, [this, &menu, &actionToNeighbor, originalTerritory]() {
        QAction *activeAction = menu.activeAction();
        if (activeAction && actionToNeighbor.contains(activeAction)) {
            // Hovering over a neighbor - highlight that territory
            int neighborId = actionToNeighbor[activeAction];
            if (m_hoveredTerritory != neighborId) {
                m_hoveredTerritory = neighborId;
                m_clickPlayer->setPosition(0);
                m_clickPlayer->play();
                update();
            }
        } else {
            // Hovering over something else - restore original
            if (m_hoveredTerritory != originalTerritory) {
                m_hoveredTerritory = originalTerritory;
                m_clickPlayer->setPosition(0);
                m_clickPlayer->play();
                update();
            }
        }
    });
    hoverTimer.start();

    menu.exec(mapToGlobal(pos));

    hoverTimer.stop();

    // After menu closes, update hover based on current mouse position
    QPoint globalPos = QCursor::pos();
    QPoint localPos = mapFromGlobal(globalPos);
    if (rect().contains(localPos)) {
        updateHoveredTerritory(localPos);
    } else {
        m_hoveredTerritory = 0;
        update();
    }
}

void MapViewerWidget::wheelEvent(QWheelEvent *event)
{
    // Get mouse position in normalized coordinates before zoom
    QPointF mousePos = event->position();
    QPointF normPos = widgetToNormalized(mousePos);

    // Calculate the map position under the mouse (inverse of MVP transform)
    // screenPos = mapPos * aspectScale * zoom + pan
    // mapPos = (screenPos - pan) / (aspectScale * zoom)
    QPointF mapPos((normPos.x() - m_pan.x()) / (m_aspectScaleX * m_zoom),
                   (normPos.y() - m_pan.y()) / (m_aspectScaleY * m_zoom));

    // Calculate zoom factor
    float delta = event->angleDelta().y();
    float zoomFactor = 1.0f + delta / 1200.0f;  // Smooth zooming

    // Apply zoom
    float newZoom = m_zoom * zoomFactor;

    // Clamp zoom: minimum 1.0 (fit to window), maximum 20.0
    newZoom = qBound(1.0f, newZoom, 20.0f);
    m_zoom = newZoom;

    // Adjust pan so the map point under the mouse stays stationary
    // newScreenPos = mapPos * aspectScale * newZoom + newPan
    // newPan = screenPos - mapPos * aspectScale * newZoom
    m_pan.setX(normPos.x() - mapPos.x() * m_aspectScaleX * m_zoom);
    m_pan.setY(normPos.y() - mapPos.y() * m_aspectScaleY * m_zoom);

    // Clamp pan to keep map on screen
    // Map edge in screen coords = aspectScale * zoom
    // For map to fill screen, need aspectScale * zoom >= 1
    // Max pan = aspectScale * zoom - 1 (but at least 0)
    float maxPanX = qMax(0.0f, m_aspectScaleX * m_zoom - 1.0f);
    float maxPanY = qMax(0.0f, m_aspectScaleY * m_zoom - 1.0f);
    m_pan.setX(qBound(-maxPanX, static_cast<float>(m_pan.x()), maxPanX));
    m_pan.setY(qBound(-maxPanY, static_cast<float>(m_pan.y()), maxPanY));

    updateMvpMatrix();
    update();

    event->accept();
}

void MapViewerWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Stop any ongoing momentum
        m_momentumTimer.stop();
        m_velocity = QPointF(0, 0);

        m_dragging = true;
        m_lastMousePos = event->position();
        m_dragTimer.start();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    } else if (event->button() == Qt::RightButton) {
        // Right-click - show context menu for territory
        if (m_hoveredTerritory > 0) {
            showTerritoryContextMenu(event->pos(), m_hoveredTerritory);
        }
        event->accept();
    }
}

void MapViewerWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        QPointF currentPos = event->position();
        QPointF delta = currentPos - m_lastMousePos;

        // Convert pixel delta to normalized coordinates
        float dx = 2.0f * delta.x() / width();
        float dy = -2.0f * delta.y() / height();  // Flip Y

        // Calculate velocity (normalized coords per second)
        qint64 elapsed = m_dragTimer.elapsed();
        if (elapsed > 0) {
            float dt = elapsed / 1000.0f;
            // Smooth velocity with exponential moving average
            float alpha = 0.3f;
            m_velocity.setX(alpha * (dx / dt) + (1.0f - alpha) * m_velocity.x());
            m_velocity.setY(alpha * (dy / dt) + (1.0f - alpha) * m_velocity.y());
        }
        m_dragTimer.restart();

        // Update pan
        m_pan.setX(m_pan.x() + dx);
        m_pan.setY(m_pan.y() + dy);

        // Clamp pan to keep map on screen
        float maxPanX = qMax(0.0f, m_aspectScaleX * m_zoom - 1.0f);
        float maxPanY = qMax(0.0f, m_aspectScaleY * m_zoom - 1.0f);
        m_pan.setX(qBound(-maxPanX, static_cast<float>(m_pan.x()), maxPanX));
        m_pan.setY(qBound(-maxPanY, static_cast<float>(m_pan.y()), maxPanY));

        m_lastMousePos = currentPos;
        updateMvpMatrix();
        update();

        event->accept();
    } else {
        // Not dragging - update hovered territory
        updateHoveredTerritory(event->position());
    }
}

void MapViewerWidget::mouseReleaseEvent(QMouseEvent *event)
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

void MapViewerWidget::onMomentumTick()
{
    float dt = m_dragTimer.elapsed() / 1000.0f;
    m_dragTimer.restart();

    // Apply velocity to pan
    m_pan.setX(m_pan.x() + m_velocity.x() * dt);
    m_pan.setY(m_pan.y() + m_velocity.y() * dt);

    // Clamp pan to keep map on screen
    float maxPanX = qMax(0.0f, m_aspectScaleX * m_zoom - 1.0f);
    float maxPanY = qMax(0.0f, m_aspectScaleY * m_zoom - 1.0f);
    m_pan.setX(qBound(-maxPanX, static_cast<float>(m_pan.x()), maxPanX));
    m_pan.setY(qBound(-maxPanY, static_cast<float>(m_pan.y()), maxPanY));

    // Apply friction (exponential decay)
    float decay = qExp(-m_friction * dt);
    m_velocity *= decay;

    // Stop if velocity is very small or hit boundary
    float speed = qSqrt(m_velocity.x() * m_velocity.x() + m_velocity.y() * m_velocity.y());
    bool hitBoundaryX = (m_pan.x() <= -maxPanX || m_pan.x() >= maxPanX) && maxPanX > 0;
    bool hitBoundaryY = (m_pan.y() <= -maxPanY || m_pan.y() >= maxPanY) && maxPanY > 0;

    if (speed < m_minVelocity || (hitBoundaryX && hitBoundaryY)) {
        m_momentumTimer.stop();
        m_velocity = QPointF(0, 0);
    } else {
        // Stop velocity component if hit boundary in that direction
        if (hitBoundaryX) m_velocity.setX(0);
        if (hitBoundaryY) m_velocity.setY(0);
    }

    updateMvpMatrix();
    update();
}
