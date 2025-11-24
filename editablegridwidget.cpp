#include "editablegridwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QAction>
#include <QSet>
#include <QMessageBox>

EditableGridWidget::EditableGridWidget(QWidget *parent)
    : QWidget(parent)
    , m_rows(8)
    , m_cols(8)
    , m_numPlayers(4)
    , m_cellSize(40)
    , m_contextMenu(nullptr)
    , m_contextRow(-1)
    , m_contextCol(-1)
{
    setMinimumSize(200, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    loadFlagImages();
    setGridSize(m_rows, m_cols);
}

void EditableGridWidget::loadFlagImages()
{
    // Load flag images in player order: Red, Blue, Green, Yellow, Orange, Black
    m_flagPixmaps.clear();
    m_flagPixmaps.append(QPixmap(":/images/redFlag.png"));
    m_flagPixmaps.append(QPixmap(":/images/blueFlag.png"));
    m_flagPixmaps.append(QPixmap(":/images/greenFlag.png"));
    m_flagPixmaps.append(QPixmap(":/images/yellowFlag.png"));
    m_flagPixmaps.append(QPixmap(":/images/orangeFlag.png"));
    m_flagPixmaps.append(QPixmap(":/images/blackFlag.png"));
}

QString EditableGridWidget::playerName(int playerIndex) const
{
    static const QStringList names = {"Red", "Blue", "Green", "Yellow", "Orange", "Black"};
    if (playerIndex >= 0 && playerIndex < names.size()) {
        return names[playerIndex];
    }
    return "Unknown";
}

void EditableGridWidget::setGridSize(int rows, int cols)
{
    if (rows < 1) rows = 1;
    if (cols < 1) cols = 1;
    if (rows > 50) rows = 50;
    if (cols > 50) cols = 50;

    m_rows = rows;
    m_cols = cols;

    // Resize cell data
    m_cells.resize(m_rows);
    for (int r = 0; r < m_rows; ++r) {
        m_cells[r].resize(m_cols);
    }

    calculateCellSize();
    emit gridSizeChanged(m_rows, m_cols);
    emit flagsChanged();
    update();
}

void EditableGridWidget::setNumPlayers(int numPlayers)
{
    if (numPlayers < 2) numPlayers = 2;
    if (numPlayers > 6) numPlayers = 6;
    m_numPlayers = numPlayers;

    // Reset any home territories that exceed the new player count
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            if (m_cells[r][c].homePlayer >= m_numPlayers) {
                m_cells[r][c].homePlayer = -1;
            }
        }
    }

    emit flagsChanged();
    update();
}

CellData EditableGridWidget::getCellData(int row, int col) const
{
    if (row >= 0 && row < m_rows && col >= 0 && col < m_cols) {
        return m_cells[row][col];
    }
    return CellData();
}

void EditableGridWidget::setCellData(int row, int col, const CellData &data)
{
    if (row >= 0 && row < m_rows && col >= 0 && col < m_cols) {
        m_cells[row][col] = data;
        emit cellChanged(row, col);
        emit flagsChanged();
        update();
    }
}

void EditableGridWidget::clearGrid()
{
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            m_cells[r][c] = CellData();
        }
    }
    emit flagsChanged();
    update();
}

void EditableGridWidget::setAllCells(const QVector<QVector<CellData>> &cells)
{
    int newRows = cells.size();
    if (newRows > 0) {
        int newCols = cells[0].size();
        setGridSize(newRows, newCols);
        m_cells = cells;
        emit flagsChanged();
        update();
    }
}

bool EditableGridWidget::allFlagsPlaced() const
{
    return getMissingFlags().isEmpty();
}

QList<int> EditableGridWidget::getMissingFlags() const
{
    QList<int> missing;
    QSet<int> placedFlags;

    // Find all placed flags
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            int hp = m_cells[r][c].homePlayer;
            if (hp >= 0 && hp < m_numPlayers) {
                placedFlags.insert(hp);
            }
        }
    }

    // Check which flags are missing
    for (int p = 0; p < m_numPlayers; ++p) {
        if (!placedFlags.contains(p)) {
            missing.append(p);
        }
    }

    return missing;
}

void EditableGridWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Draw cells
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            QRect rect = cellRect(r, c);
            const CellData &cell = m_cells[r][c];

            // Fill based on terrain type
            QColor fillColor;
            switch (cell.terrain) {
                case TerrainType::Land5:
                    fillColor = QColor(144, 238, 144);  // Light green
                    break;
                case TerrainType::Land10:
                    fillColor = QColor(60, 140, 60);    // Darker green
                    break;
                case TerrainType::Sea:
                    fillColor = QColor(100, 150, 220);  // Blue
                    break;
            }
            painter.fillRect(rect, fillColor);

            // Draw value label for land
            if (cell.isLand()) {
                painter.setPen(cell.terrain == TerrainType::Land5 ? Qt::darkGreen : Qt::white);
                QFont font = painter.font();
                font.setPointSize(m_cellSize / 4);
                font.setBold(true);
                painter.setFont(font);
                painter.drawText(rect, Qt::AlignTop | Qt::AlignLeft,
                    QString(" %1").arg(cell.value()));
            }

            // Draw grid border
            painter.setPen(QPen(Qt::darkGray, 1));
            painter.drawRect(rect);

            // Draw flag image if home territory is set
            if (cell.homePlayer >= 0 && cell.homePlayer < m_flagPixmaps.size()) {
                const QPixmap &flagPix = m_flagPixmaps[cell.homePlayer];
                if (!flagPix.isNull()) {
                    // Scale flag to fit in cell with some padding
                    int flagSize = m_cellSize * 2 / 3;
                    int x = rect.center().x() - flagSize / 2;
                    int y = rect.center().y() - flagSize / 2;
                    painter.drawPixmap(x, y, flagSize, flagSize, flagPix);
                }
            }
        }
    }
}

void EditableGridWidget::mousePressEvent(QMouseEvent *event)
{
    QPoint cell = cellAt(event->pos());
    if (cell.x() < 0 || cell.y() < 0) {
        return;
    }

    int row = cell.y();
    int col = cell.x();

    if (event->button() == Qt::LeftButton) {
        // Cycle through: Land5 -> Land10 -> Sea -> Land5
        TerrainType currentTerrain = m_cells[row][col].terrain;
        TerrainType nextTerrain;

        switch (currentTerrain) {
            case TerrainType::Land5:
                nextTerrain = TerrainType::Land10;
                break;
            case TerrainType::Land10:
                // Check if trying to change to sea when flag is present
                if (m_cells[row][col].homePlayer >= 0) {
                    QMessageBox::warning(this, "Cannot Change to Sea",
                        "You can't have sea squares as home territories.\n"
                        "Remove the flag first if you want to make this a sea square.");
                    return;
                }
                nextTerrain = TerrainType::Sea;
                break;
            case TerrainType::Sea:
                nextTerrain = TerrainType::Land5;
                break;
        }

        m_cells[row][col].terrain = nextTerrain;
        emit cellChanged(row, col);
        update();
    }
    else if (event->button() == Qt::RightButton) {
        // Only show context menu for land squares
        if (!m_cells[row][col].isLand()) {
            return;  // Don't show menu for sea squares
        }
        showContextMenu(event->globalPos(), row, col);
    }
}

void EditableGridWidget::showContextMenu(const QPoint &pos, int row, int col)
{
    m_contextRow = row;
    m_contextCol = col;

    // Create context menu
    if (m_contextMenu) {
        delete m_contextMenu;
    }
    m_contextMenu = new QMenu(this);

    // Add flag submenu
    QMenu *addFlagMenu = m_contextMenu->addMenu("Add Flag");

    for (int p = 0; p < m_numPlayers; ++p) {
        QAction *action = addFlagMenu->addAction(
            QIcon(m_flagPixmaps[p]),
            QString("Player %1 (%2)").arg(p + 1).arg(playerName(p))
        );
        connect(action, &QAction::triggered, this, [this, p]() {
            onAddFlag(p);
        });
    }

    // Add remove flag option if this cell has a flag
    if (m_cells[row][col].homePlayer >= 0) {
        m_contextMenu->addSeparator();
        QAction *removeAction = m_contextMenu->addAction("Remove Flag");
        connect(removeAction, &QAction::triggered, this, &EditableGridWidget::onRemoveFlag);
    }

    m_contextMenu->popup(pos);
}

void EditableGridWidget::onAddFlag(int playerIndex)
{
    if (m_contextRow < 0 || m_contextCol < 0) {
        return;
    }

    // Remove existing flag for this player (only one of each color allowed)
    removeFlagFromPlayer(playerIndex);

    // Set the flag on the selected cell
    m_cells[m_contextRow][m_contextCol].homePlayer = playerIndex;
    emit cellChanged(m_contextRow, m_contextCol);
    emit flagsChanged();
    update();

    m_contextRow = -1;
    m_contextCol = -1;
}

void EditableGridWidget::onRemoveFlag()
{
    if (m_contextRow < 0 || m_contextCol < 0) {
        return;
    }

    m_cells[m_contextRow][m_contextCol].homePlayer = -1;
    emit cellChanged(m_contextRow, m_contextCol);
    emit flagsChanged();
    update();

    m_contextRow = -1;
    m_contextCol = -1;
}

void EditableGridWidget::removeFlagFromPlayer(int playerIndex)
{
    // Find and remove any existing flag for this player
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            if (m_cells[r][c].homePlayer == playerIndex) {
                m_cells[r][c].homePlayer = -1;
                emit cellChanged(r, c);
            }
        }
    }
}

void EditableGridWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    calculateCellSize();
}

QSize EditableGridWidget::sizeHint() const
{
    return QSize(m_cols * 50, m_rows * 50);
}

QSize EditableGridWidget::minimumSizeHint() const
{
    return QSize(m_cols * 20, m_rows * 20);
}

QPoint EditableGridWidget::cellAt(const QPoint &pos) const
{
    int col = pos.x() / m_cellSize;
    int row = pos.y() / m_cellSize;

    if (row >= 0 && row < m_rows && col >= 0 && col < m_cols) {
        return QPoint(col, row);
    }
    return QPoint(-1, -1);
}

QRect EditableGridWidget::cellRect(int row, int col) const
{
    return QRect(col * m_cellSize, row * m_cellSize, m_cellSize, m_cellSize);
}

void EditableGridWidget::calculateCellSize()
{
    if (m_rows == 0 || m_cols == 0) {
        m_cellSize = 40;
        return;
    }

    int availableWidth = width();
    int availableHeight = height();

    int cellWidth = availableWidth / m_cols;
    int cellHeight = availableHeight / m_rows;

    m_cellSize = qMin(cellWidth, cellHeight);
    if (m_cellSize < 15) m_cellSize = 15;
    if (m_cellSize > 80) m_cellSize = 80;
}
