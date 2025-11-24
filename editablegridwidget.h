#ifndef EDITABLEGRIDWIDGET_H
#define EDITABLEGRIDWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPixmap>
#include <QMenu>

// Territory type for cells
enum class TerrainType {
    Land5,   // Land worth 5 (light green)
    Land10,  // Land worth 10 (darker green)
    Sea      // Sea (blue)
};

// Cell data for the map editor
struct CellData {
    TerrainType terrain;  // Land(5), Land(10), or Sea
    int homePlayer;       // -1 = no home, 0-5 = player index

    CellData() : terrain(TerrainType::Land5), homePlayer(-1) {}

    bool isLand() const { return terrain != TerrainType::Sea; }
    int value() const {
        switch (terrain) {
            case TerrainType::Land5: return 5;
            case TerrainType::Land10: return 10;
            default: return 0;
        }
    }
};

class EditableGridWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EditableGridWidget(QWidget *parent = nullptr);

    // Grid management
    void setGridSize(int rows, int cols);
    int rows() const { return m_rows; }
    int cols() const { return m_cols; }

    // Player count (affects flag options)
    void setNumPlayers(int numPlayers);
    int numPlayers() const { return m_numPlayers; }

    // Cell access
    CellData getCellData(int row, int col) const;
    void setCellData(int row, int col, const CellData &data);

    // Clear all cells to default (land, no home)
    void clearGrid();

    // Get all cell data
    const QVector<QVector<CellData>>& getAllCells() const { return m_cells; }

    // Set all cell data (for loading)
    void setAllCells(const QVector<QVector<CellData>> &cells);

    // Check if all required flags are placed
    bool allFlagsPlaced() const;

    // Get list of missing flag player indices
    QList<int> getMissingFlags() const;

signals:
    void cellChanged(int row, int col);
    void gridSizeChanged(int rows, int cols);
    void flagsChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private slots:
    void onAddFlag(int playerIndex);
    void onRemoveFlag();

private:
    int m_rows;
    int m_cols;
    int m_numPlayers;
    int m_cellSize;
    QVector<QVector<CellData>> m_cells;

    // Context menu
    QMenu *m_contextMenu;
    int m_contextRow;
    int m_contextCol;

    // Flag images
    QVector<QPixmap> m_flagPixmaps;

    // Helper functions
    QPoint cellAt(const QPoint &pos) const;
    QRect cellRect(int row, int col) const;
    void calculateCellSize();
    void loadFlagImages();
    void showContextMenu(const QPoint &pos, int row, int col);
    void removeFlagFromPlayer(int playerIndex);
    QString playerName(int playerIndex) const;
};

#endif // EDITABLEGRIDWIDGET_H
