# MapViewer Project Plan

## Goal
Create a Qt OpenGL widget that displays the game map and highlights territory boundaries on mouse hover.

## Design Decisions
- **Modern OpenGL** with shaders (no legacy fixed-function pipeline)
- **Pan**: Left mouse button drag
- **Zoom**: Scroll wheel (zoom centered on mouse cursor)
- **Context menus**: Right mouse button (future use)
- **Aspect ratio**: Map always displays at native aspect ratio (letterbox/pillarbox if needed)

## Data Files
- `Map.jpg` (3840x2274) - The visual map image
- `territories_index.png` (3840x2274) - 8-bit image where pixel value = territory ID (0=background, 1-60=territories)
- `territories.csv` - Territory metadata (ID, name, centroid, tax value, neighbors)

---

## Phase 1: Basic Map Display with Pan/Zoom

### 1.1 Project Setup
- Create `MapViewer.pro` with Qt OpenGL widgets
- Create `resources.qrc` referencing data files
- Copy data files to project

### 1.2 Shader Program
- **Vertex shader**: Transform quad vertices using model-view-projection matrix
- **Fragment shader**: Sample map texture

### 1.3 View Transform
- Store `m_zoom` (float, 1.0 = fit to window)
- Store `m_pan` (QPointF, offset in map coordinates)
- Compute MVP matrix from zoom and pan
- Zoom centered on mouse cursor position

### 1.4 Mouse Handling
- **Scroll wheel**: Adjust zoom level, keeping point under cursor stationary
- **Left button drag**: Pan the map (adjust m_pan based on delta)
- **Right button**: Reserved for context menus

### 1.5 Aspect Ratio
- Calculate viewport to maintain map's native aspect ratio (3840:2274 ≈ 1.688:1)
- Center the viewport in the widget (letterbox or pillarbox)

---

## Phase 2: Territory Detection

### 2.1 Load Territory Index
- Load `territories_index.png` as a QImage (not texture)
- Keep in CPU memory for pixel lookups

### 2.2 Mouse Tracking
- Enable mouse tracking on widget
- On mouse move, convert widget coordinates to image coordinates
- Look up pixel value in territory index image
- Store current hovered territory ID

### 2.3 Load Territory Data
- Parse `territories.csv` into a `QMap<int, Territory>`
- Territory struct: id, name, area, centroid, taxValue, neighbors

---

## Phase 3: Boundary Highlighting

### 3.1 Approach Options

**Option A: Pre-compute boundary pixels**
- At load time, scan index image to find all edge pixels (where adjacent pixel has different ID)
- Store as `QMap<int, QVector<QPoint>>` mapping territory ID to its boundary pixels
- On hover, draw those pixels as overlay
- Pros: Fast at runtime
- Cons: Memory for storing all boundary pixels

**Option B: Compute boundary on demand**
- When territory changes, scan only that territory's pixels to find edges
- Pros: Less memory
- Cons: Slower on hover change (but probably fine for 60 territories)

**Option C: Use edge detection shader**
- Upload index image as texture
- In fragment shader, compare current pixel to neighbors
- If different, draw highlight color
- Pros: GPU-accelerated, clean
- Cons: More complex shader code

### 3.2 Recommended Approach
Start with **Option A** (pre-compute boundaries) because:
- Simple to implement
- Fast runtime performance
- Memory cost is reasonable (~60 territories, each with maybe 1000-3000 boundary pixels)

### 3.3 Rendering the Highlight
- Draw boundary pixels as GL_POINTS or a line loop
- Use bright color (yellow/white) with some transparency
- Could also draw filled overlay with alpha for "selected" effect

---

## Phase 4: UI Polish

### 4.1 Status Display
- Show territory name on hover (tooltip or status bar)
- Show tax value and neighbor count

### 4.2 Click Selection
- On click, "select" territory
- Keep it highlighted until another is clicked
- Emit signal with territory ID for future game integration

---

## Class Structure

```
MapViewerWidget : QOpenGLWidget, QOpenGLFunctions
│
├── OpenGL Resources
│   ├── m_shaderProgram (QOpenGLShaderProgram*)
│   ├── m_mapTexture (GLuint) - Map image texture
│   ├── m_vao (QOpenGLVertexArrayObject)
│   ├── m_vbo (QOpenGLBuffer) - Quad vertices
│   └── m_boundaryVbo (QOpenGLBuffer) - Boundary line vertices
│
├── View State
│   ├── m_zoom (float) - Zoom level (1.0 = fit to window)
│   ├── m_pan (QPointF) - Pan offset in normalized map coords
│   └── m_mvpMatrix (QMatrix4x4) - Model-view-projection
│
├── Map Data
│   ├── m_mapSize (QSize) - Original map dimensions (3840x2274)
│   ├── m_indexImage (QImage) - Territory index for lookups (CPU side)
│   ├── m_territories (QMap<int, Territory>) - Territory metadata
│   └── m_boundaries (QMap<int, QVector<QPointF>>) - Pre-computed boundary vertices
│
├── Interaction State
│   ├── m_hoveredTerritory (int) - Currently hovered (0 = none)
│   ├── m_selectedTerritory (int) - Currently selected (0 = none)
│   ├── m_dragging (bool) - Is left button held?
│   └── m_lastMousePos (QPoint) - For drag delta calculation
│
├── QOpenGLWidget Overrides
│   ├── initializeGL() - Create shaders, textures, VBOs, compute boundaries
│   ├── paintGL() - Draw map quad, draw boundary overlay
│   └── resizeGL() - Update projection matrix, maintain aspect ratio
│
├── Mouse Events
│   ├── mousePressEvent() - Start drag or select territory
│   ├── mouseReleaseEvent() - End drag
│   ├── mouseMoveEvent() - Update pan or update hover
│   └── wheelEvent() - Zoom in/out centered on cursor
│
└── Private Helpers
    ├── loadTerritories() - Parse CSV file
    ├── computeBoundaries() - Scan index image for edge pixels
    ├── updateMvpMatrix() - Rebuild MVP from zoom/pan
    ├── widgetToMap(QPointF) - Convert widget coords to map coords
    └── mapToWidget(QPointF) - Convert map coords to widget coords
```

---

## Coordinate Systems

1. **Widget coords**: (0,0) top-left, pixels, size varies with window
2. **Normalized Device Coords (NDC)**: (-1,-1) to (1,1), used by OpenGL
3. **Map coords**: (0,0) to (3840,2274), matches the image pixels

The MVP matrix handles Map → NDC. We need helper functions for Widget ↔ Map.

---

## Shader Details

### Vertex Shader
```glsl
#version 330 core
layout(location = 0) in vec2 position;  // Map coords (0-3840, 0-2274)
layout(location = 1) in vec2 texCoord;  // UV (0-1)

uniform mat4 mvp;

out vec2 fragTexCoord;

void main() {
    gl_Position = mvp * vec4(position, 0.0, 1.0);
    fragTexCoord = texCoord;
}
```

### Fragment Shader (Map)
```glsl
#version 330 core
in vec2 fragTexCoord;
out vec4 fragColor;

uniform sampler2D mapTexture;

void main() {
    fragColor = texture(mapTexture, fragTexCoord);
}
```

### Fragment Shader (Boundary) - simple solid color
```glsl
#version 330 core
out vec4 fragColor;

uniform vec4 highlightColor;

void main() {
    fragColor = highlightColor;
}
```

---

## Remaining Questions

1. **Sea territories**: Should they highlight differently? (Different color?)

2. **Boundary line width**: OpenGL line width is limited on some GPUs. May need geometry shader or draw as thin quads for thick lines.

3. **Zoom limits**: Min/max zoom levels to prevent zooming too far in/out?

---

## Next Steps

1. ✅ Review this plan
2. ✅ Design decisions made
3. Implement Phase 1 (map display with pan/zoom)
4. Test, then proceed to Phase 2 (territory detection)
5. Phase 3 (boundary highlighting)
6. Phase 4 (UI polish)
