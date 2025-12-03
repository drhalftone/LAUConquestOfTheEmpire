#ifndef MAPVIEWERWIDGET_H
#define MAPVIEWERWIDGET_H

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
#include <QVector>
#include <QMenu>
#include <QMediaPlayer>
#include <QAudioOutput>

// Territory data from CSV
struct Territory {
    int id = 0;
    QString name;
    QPointF centroid;        // Centroid in map pixel coordinates
    QVector<int> neighbors;  // Neighbor IDs in order from CSV
};

class MapViewerWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit MapViewerWidget(QWidget *parent = nullptr);
    ~MapViewerWidget();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void createShaders();
    void createGeometry();
    void loadTextures();
    void loadTerritories();
    void updateMvpMatrix();
    void updateHoveredTerritory(const QPointF &widgetPos);
    void showTerritoryContextMenu(const QPoint &pos, int territoryId);
    QPointF widgetToNormalized(const QPointF &widgetPos) const;
    QPointF widgetToMapCoords(const QPointF &widgetPos) const;

    // OpenGL resources
    QOpenGLShaderProgram *m_shaderProgram = nullptr;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo;
    QOpenGLTexture *m_mapTexture = nullptr;
    QOpenGLTexture *m_indexTexture = nullptr;  // Territory index texture (GPU)

    // Territory detection (CPU side for mouse lookup)
    QImage m_indexImage;             // Territory index image for CPU lookups
    int m_hoveredTerritory = 0;      // Currently hovered territory (0 = none/background)

    // Territory data from CSV
    QMap<int, Territory> m_territories;

    // Map dimensions
    QSize m_mapSize;

    // Window size in pixels (for Retina support)
    QSize m_windowPixelSize;

    // Aspect ratio correction factors
    float m_aspectScaleX = 1.0f;
    float m_aspectScaleY = 1.0f;

    // View transform
    float m_zoom = 1.0f;           // 1.0 = fit to window, >1 = zoomed in
    QPointF m_pan;                  // Pan offset in normalized coords (-1 to 1)
    QMatrix4x4 m_mvpMatrix;

    // Dragging state
    bool m_dragging = false;
    QPointF m_lastMousePos;

    // Momentum/inertia
    QPointF m_velocity;              // Current velocity in normalized coords per second
    QTimer m_momentumTimer;
    QElapsedTimer m_dragTimer;       // For calculating velocity during drag
    static constexpr float m_friction = 5.0f;  // Friction coefficient (higher = faster stop)
    static constexpr float m_minVelocity = 0.01f;  // Stop when velocity below this

    // Audio
    QMediaPlayer *m_clickPlayer = nullptr;
    QAudioOutput *m_audioOutput = nullptr;

private slots:
    void onMomentumTick();
};

#endif // MAPVIEWERWIDGET_H
