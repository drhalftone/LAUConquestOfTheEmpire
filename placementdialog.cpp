#include "placementdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFont>
#include <QFrame>
#include <QPainter>
#include <QDrag>
#include <QMimeData>
#include <QApplication>
#include <QDebug>

// DraggableIconLabel implementation
DraggableIconLabel::DraggableIconLabel(const QString &itemType, const QString &text, QWidget *parent)
    : QLabel(text, parent)
    , m_itemType(itemType)
{
}

void DraggableIconLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPosition = event->pos();
    }
    QLabel::mousePressEvent(event);
}

void DraggableIconLabel::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        return;
    }

    if ((event->pos() - m_dragStartPosition).manhattanLength() < QApplication::startDragDistance()) {
        return;
    }

    // Start drag operation
    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData();

    // Store the item type in mime data
    mimeData->setText(m_itemType);
    drag->setMimeData(mimeData);

    // Create a pixmap of this label for drag visual
    QPixmap pixmap(size());
    render(&pixmap);
    drag->setPixmap(pixmap);
    drag->setHotSpot(event->pos());

    // Execute the drag
    drag->exec(Qt::CopyAction);
}

PlacementDialog::PlacementDialog(QChar player,
                               int infantryCount,
                               int cavalryCount,
                               int catapultCount,
                               int galleyCount,
                               int cityCount,
                               int fortificationCount,
                               int roadCount,
                               QWidget *parent)
    : QDialog(parent)
    , m_player(player)
    , m_infantryRemaining(infantryCount)
    , m_cavalryRemaining(cavalryCount)
    , m_catapultRemaining(catapultCount)
    , m_galleyRemaining(galleyCount)
    , m_cityRemaining(cityCount)
    , m_fortificationRemaining(fortificationCount)
    , m_roadRemaining(roadCount)
{
    setWindowTitle(QString("Placement Phase - Player %1").arg(player));
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    resize(400, 500);

    setupUI();
}

bool PlacementDialog::allItemsPlaced() const
{
    return m_infantryRemaining == 0 &&
           m_cavalryRemaining == 0 &&
           m_catapultRemaining == 0 &&
           m_galleyRemaining == 0 &&
           m_cityRemaining == 0 &&
           m_fortificationRemaining == 0 &&
           m_roadRemaining == 0;
}

QWidget* PlacementDialog::createItemBox(const QString &itemName, const QString &iconText,
                                       const QColor &color, int count, int &remainingRef)
{
    QWidget *box = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(box);
    layout->setSpacing(5);
    layout->setContentsMargins(10, 10, 10, 10);

    // Item name
    QLabel *nameLabel = new QLabel(itemName);
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(11);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    nameLabel->setAlignment(Qt::AlignCenter);

    // Icon representation (large colored square/circle with text) - DRAGGABLE
    DraggableIconLabel *iconLabel = new DraggableIconLabel(itemName, iconText);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setMinimumSize(80, 80);
    iconLabel->setMaximumSize(80, 80);

    QFont iconFont = iconLabel->font();
    iconFont.setPointSize(24);
    iconFont.setBold(true);
    iconLabel->setFont(iconFont);

    // Style the icon with background color
    QString styleSheet = QString("QLabel { "
                                "background-color: %1; "
                                "border: 3px solid %2; "
                                "border-radius: 5px; "
                                "color: white; "
                                "}").arg(color.lighter(130).name()).arg(color.darker(150).name());
    iconLabel->setStyleSheet(styleSheet);
    iconLabel->setCursor(Qt::OpenHandCursor);  // Show it's draggable

    // Count label
    QLabel *countLabel = new QLabel(QString("Remaining: %1").arg(count));
    QFont countFont = countLabel->font();
    countFont.setPointSize(12);
    countFont.setBold(true);
    countLabel->setFont(countFont);
    countLabel->setAlignment(Qt::AlignCenter);

    // Store label reference for updates
    if (itemName == "Infantry") m_infantryLabel = countLabel;
    else if (itemName == "Cavalry") m_cavalryLabel = countLabel;
    else if (itemName == "Catapult") m_catapultLabel = countLabel;
    else if (itemName == "Galley") m_galleyLabel = countLabel;
    else if (itemName == "City") m_cityLabel = countLabel;
    else if (itemName == "Fortification") m_fortificationLabel = countLabel;
    else if (itemName == "Road") m_roadLabel = countLabel;

    layout->addWidget(nameLabel);
    layout->addWidget(iconLabel);
    layout->addWidget(countLabel);

    // Add border to box
    box->setStyleSheet("QWidget { border: 2px solid #ccc; border-radius: 8px; background-color: #f9f9f9; }");

    return box;
}

void PlacementDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Title
    QLabel *titleLabel = new QLabel(QString("Player %1 - Place Your Purchased Units").arg(m_player));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // Instructions
    QLabel *instructLabel = new QLabel("Click on the map to place items from this holding pen");
    QFont instructFont = instructLabel->font();
    instructFont.setPointSize(10);
    instructFont.setItalic(true);
    instructLabel->setFont(instructFont);
    instructLabel->setAlignment(Qt::AlignCenter);
    instructLabel->setStyleSheet("color: #666;");
    mainLayout->addWidget(instructLabel);

    mainLayout->addSpacing(10);

    // Grid layout for items (2 columns)
    m_gridLayout = new QGridLayout();
    m_gridLayout->setSpacing(15);

    int row = 0, col = 0;

    // Helper to add items to grid
    auto addItemToGrid = [&](const QString &name, const QString &icon, const QColor &color, int count, int &remainingRef) {
        if (count > 0) {
            QWidget *box = createItemBox(name, icon, color, count, remainingRef);
            m_gridLayout->addWidget(box, row, col);
            col++;
            if (col >= 2) {
                col = 0;
                row++;
            }
        }
    };

    // Add all purchased items
    addItemToGrid("Infantry", "I", QColor(100, 100, 200), m_infantryRemaining, m_infantryRemaining);
    addItemToGrid("Cavalry", "C", QColor(200, 100, 100), m_cavalryRemaining, m_cavalryRemaining);
    addItemToGrid("Catapult", "K", QColor(150, 150, 150), m_catapultRemaining, m_catapultRemaining);
    addItemToGrid("Galley", "G", QColor(50, 150, 200), m_galleyRemaining, m_galleyRemaining);
    addItemToGrid("City", "⌂", QColor(180, 140, 80), m_cityRemaining, m_cityRemaining);
    addItemToGrid("Fortification", "▮", QColor(100, 100, 100), m_fortificationRemaining, m_fortificationRemaining);
    addItemToGrid("Road", "═", QColor(139, 90, 43), m_roadRemaining, m_roadRemaining);

    mainLayout->addLayout(m_gridLayout);

    // Add stretch to push everything to top
    mainLayout->addStretch();

    // Separator
    QFrame *separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    // Info label at bottom
    QLabel *infoLabel = new QLabel("This window stays open while you place items.\nClose it when all items are placed.");
    infoLabel->setAlignment(Qt::AlignCenter);
    infoLabel->setStyleSheet("color: #666; padding: 10px;");
    QFont infoFont = infoLabel->font();
    infoFont.setPointSize(9);
    infoLabel->setFont(infoFont);
    mainLayout->addWidget(infoLabel);

    // Note: No "Done" button - window stays open during placement
    // User can close it manually when done, or it auto-closes when all items placed
}

void PlacementDialog::decrementItemCount(QString itemType)
{
    // Decrement the appropriate counter
    if (itemType == "Infantry" && m_infantryRemaining > 0) {
        m_infantryRemaining--;
        if (m_infantryLabel) {
            m_infantryLabel->setText(QString("Remaining: %1").arg(m_infantryRemaining));
        }
    }
    else if (itemType == "Cavalry" && m_cavalryRemaining > 0) {
        m_cavalryRemaining--;
        if (m_cavalryLabel) {
            m_cavalryLabel->setText(QString("Remaining: %1").arg(m_cavalryRemaining));
        }
    }
    else if (itemType == "Catapult" && m_catapultRemaining > 0) {
        m_catapultRemaining--;
        if (m_catapultLabel) {
            m_catapultLabel->setText(QString("Remaining: %1").arg(m_catapultRemaining));
        }
    }
    else if (itemType == "Galley" && m_galleyRemaining > 0) {
        m_galleyRemaining--;
        if (m_galleyLabel) {
            m_galleyLabel->setText(QString("Remaining: %1").arg(m_galleyRemaining));
        }
    }
    else if (itemType == "City" && m_cityRemaining > 0) {
        m_cityRemaining--;
        if (m_cityLabel) {
            m_cityLabel->setText(QString("Remaining: %1").arg(m_cityRemaining));
        }
    }
    else if (itemType == "Fortification" && m_fortificationRemaining > 0) {
        m_fortificationRemaining--;
        if (m_fortificationLabel) {
            m_fortificationLabel->setText(QString("Remaining: %1").arg(m_fortificationRemaining));
        }
    }
    else if (itemType == "Road" && m_roadRemaining > 0) {
        m_roadRemaining--;
        if (m_roadLabel) {
            m_roadLabel->setText(QString("Remaining: %1").arg(m_roadRemaining));
        }
    }

    // Auto-close dialog if all items placed
    if (allItemsPlaced()) {
        qDebug() << "All items placed! Auto-closing placement dialog.";
        accept();
    }
}
