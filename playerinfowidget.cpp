#include "playerinfowidget.h"
#include "mapwidget.h"
#include "purchasedialog.h"
#include "troopselectiondialog.h"
#include "combatdialog.h"
#include "citydestructiondialog.h"
#include "gamepiece.h"
#include "building.h"
#include <QScrollArea>
#include <QGridLayout>
#include <QFrame>
#include <QHeaderView>
#include <QMenu>
#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QDebug>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QCheckBox>
#include <QLabel>

PlayerInfoWidget::PlayerInfoWidget(QWidget *parent)
    : QWidget(parent)
    , m_tabWidget(new QTabWidget(this))
    , m_mapWidget(nullptr)
    , m_capturedGeneralsGroupBox(nullptr)
    , m_capturedGeneralsTable(nullptr)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Add tab widget with stretch factor to make it expand
    mainLayout->addWidget(m_tabWidget, 1);  // Stretch factor of 1

    // Add global captured generals section (no stretch - minimal space)
    m_capturedGeneralsGroupBox = createAllCapturedGeneralsSection();
    mainLayout->addWidget(m_capturedGeneralsGroupBox, 0);  // No stretch

    // Add End Turn button at the bottom (no stretch - minimal space)
    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    QPushButton *endTurnButton = buttonBox->addButton("End Turn", QDialogButtonBox::ActionRole);
    connect(endTurnButton, &QPushButton::clicked, this, &PlayerInfoWidget::onEndTurnClicked);
    mainLayout->addWidget(buttonBox, 0);  // No stretch

    setLayout(mainLayout);

    setWindowTitle("Player Information");

    // Load saved geometry, or use default if none saved
    loadSettings();
}

PlayerInfoWidget::~PlayerInfoWidget()
{
    saveSettings();
}

void PlayerInfoWidget::addPlayer(Player *player)
{
    if (!player || m_players.contains(player)) {
        return;
    }

    m_players.append(player);

    // Create tab for this player
    QWidget *playerTab = createPlayerTab(player);
    m_playerTabs[player] = playerTab;

    // Add tab with player ID as label and flag icon
    QString tabLabel = QString("Player %1").arg(player->getId());
    QIcon flagIcon;
    switch (player->getId().toLatin1()) {
        case 'A': flagIcon = QIcon(":/images/redFlag.png"); break;
        case 'B': flagIcon = QIcon(":/images/greenFlag.png"); break;
        case 'C': flagIcon = QIcon(":/images/blueFlag.png"); break;
        case 'D': flagIcon = QIcon(":/images/yellowFlag.png"); break;
        case 'E': flagIcon = QIcon(":/images/blackFlag.png"); break;
        case 'F': flagIcon = QIcon(":/images/orangeFlag.png"); break;
    }
    m_tabWidget->addTab(playerTab, flagIcon, tabLabel);
}

void PlayerInfoWidget::setPlayers(const QList<Player*> &players)
{
    // Clear existing tabs
    m_tabWidget->clear();
    m_playerTabs.clear();
    m_players.clear();

    // Add all players
    for (Player *player : players) {
        addPlayer(player);
    }

    // Update captured generals table
    updateCapturedGeneralsTable();
}

QWidget* PlayerInfoWidget::createPlayerTab(Player *player)
{
    QWidget *tab = new QWidget();
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);

    QWidget *contentWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(contentWidget);

    // Add sections
    layout->addWidget(createBasicInfoSection(player));
    layout->addWidget(createEconomicsSection(player));
    layout->addWidget(createTerritoriesSection(player));
    layout->addWidget(createPiecesSection(player));
    layout->addWidget(createCapturedGeneralsSection(player));

    layout->addStretch();

    scrollArea->setWidget(contentWidget);

    QVBoxLayout *tabLayout = new QVBoxLayout(tab);
    tabLayout->addWidget(scrollArea);
    tabLayout->setContentsMargins(0, 0, 0, 0);

    return tab;
}

QGroupBox* PlayerInfoWidget::createBasicInfoSection(Player *player)
{
    QGroupBox *groupBox = new QGroupBox("Basic Information");
    QGridLayout *layout = new QGridLayout();

    // Player ID
    layout->addWidget(new QLabel("<b>Player ID:</b>"), 0, 0);
    layout->addWidget(new QLabel(QString(player->getId())), 0, 1);

    // Player Color
    layout->addWidget(new QLabel("<b>Color:</b>"), 1, 0);
    QLabel *colorLabel = new QLabel();
    QColor color = player->getColor();
    colorLabel->setStyleSheet(QString("background-color: rgb(%1, %2, %3); border: 2px solid black; padding: 5px;")
                              .arg(color.red()).arg(color.green()).arg(color.blue()));
    colorLabel->setFixedSize(100, 30);
    layout->addWidget(colorLabel, 1, 1);

    // Home Province
    layout->addWidget(new QLabel("<b>Home Province:</b>"), 2, 0);
    QString homeName = player->getHomeProvinceName();
    Position homePos = m_mapWidget->territoryNameToPosition(homeName);  // Convert for display
    QString homeText = QString("%1 [Row: %2, Col: %3]")
                       .arg(homeName)
                       .arg(homePos.row)
                       .arg(homePos.col);
    layout->addWidget(new QLabel(homeText), 2, 1);

    // Home Fortified City
    layout->addWidget(new QLabel("<b>Home City:</b>"), 3, 0);
    QString cityText = player->hasCity() ? "Fortified City" : "None";
    layout->addWidget(new QLabel(cityText), 3, 1);

    groupBox->setLayout(layout);
    return groupBox;
}

QGroupBox* PlayerInfoWidget::createEconomicsSection(Player *player)
{
    QGroupBox *groupBox = new QGroupBox("Economics");
    QGridLayout *layout = new QGridLayout();

    // Current Wallet
    layout->addWidget(new QLabel("<b>Current Money:</b>"), 0, 0);
    layout->addWidget(new QLabel(QString("%1 talents").arg(player->getWallet())), 0, 1);

    // Total territories owned
    layout->addWidget(new QLabel("<b>Territories Owned:</b>"), 1, 0);
    layout->addWidget(new QLabel(QString::number(player->getOwnedTerritoryCount())), 1, 1);

    // Calculate total tax value from all owned territories
    int totalTaxValue = 0;
    if (m_mapWidget) {
        const QList<QString> &territories = player->getOwnedTerritories();
        for (const QString &territoryName : territories) {
            // Find the territory position by searching the map
            for (int row = 0; row < 8; ++row) {
                for (int col = 0; col < 12; ++col) {
                    if (m_mapWidget->getTerritoryNameAt(row, col) == territoryName) {
                        totalTaxValue += m_mapWidget->getTerritoryValueAt(row, col);
                        break;
                    }
                }
            }
        }
    }

    // Add 5 talents for each city owned (cities are worth 5 each)
    int cityTaxes = player->getCities().size() * 5;
    totalTaxValue += cityTaxes;

    layout->addWidget(new QLabel("<b>Total Tax Value:</b>"), 2, 0);
    layout->addWidget(new QLabel(QString("%1 talents").arg(totalTaxValue)), 2, 1);

    groupBox->setLayout(layout);
    return groupBox;
}

QGroupBox* PlayerInfoWidget::createTerritoriesSection(Player *player)
{
    QGroupBox *groupBox = new QGroupBox(QString("Owned Territories (%1)").arg(player->getOwnedTerritories().size()));
    QVBoxLayout *layout = new QVBoxLayout();

    const QList<QString> &territories = player->getOwnedTerritories();

    if (territories.isEmpty()) {
        QLabel *emptyLabel = new QLabel("(No territories owned)");
        emptyLabel->setStyleSheet("font-style: italic; color: gray;");
        layout->addWidget(emptyLabel);
    } else {
        // Create a frame with white background to match tables
        QFrame *frame = new QFrame();
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Sunken);
        frame->setStyleSheet("QFrame { background-color: white; border: 1px solid #c0c0c0; }");

        // Create a grid layout with 3 columns
        QGridLayout *gridLayout = new QGridLayout(frame);
        gridLayout->setSpacing(5);
        gridLayout->setContentsMargins(5, 5, 5, 5);

        int row = 0;
        int col = 0;
        const int NUM_COLUMNS = 3;

        for (const QString &territoryName : territories) {
            // Find the tax value for this territory
            int taxValue = 0;
            bool found = false;
            if (m_mapWidget) {
                for (int r = 0; r < 8 && !found; ++r) {
                    for (int c = 0; c < 12; ++c) {
                        if (m_mapWidget->getTerritoryNameAt(r, c) == territoryName) {
                            taxValue = m_mapWidget->getTerritoryValueAt(r, c);
                            found = true;
                            break;
                        }
                    }
                }
            }

            // Check if player has a city here
            City *city = player->getCityAtTerritory(territoryName);

            QString itemText = territoryName;

            // Add tax value
            if (taxValue > 0) {
                itemText += QString(" (%1)").arg(taxValue);
            }

            if (city) {
                if (city->isFortified()) {
                    itemText += " - [Fortified City]";
                } else {
                    itemText += " - [City]";
                }

                // Mark cities marked for destruction
                if (city->isMarkedForDestruction()) {
                    itemText += " (MARKED FOR DESTRUCTION)";
                }
            }

            // Add roads if any
            QList<Road*> roads = player->getRoadsAtTerritory(territoryName);
            if (!roads.isEmpty()) {
                itemText += QString(" [%1 road(s)]").arg(roads.size());
            }

            // Create a label for this territory
            QLabel *territoryLabel = new QLabel(itemText);
            territoryLabel->setWordWrap(true);
            territoryLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

            // Style differently if city is marked for destruction
            QString labelStyle = "padding: 5px; background-color: transparent;";
            if (city && city->isMarkedForDestruction()) {
                labelStyle = "padding: 5px; background-color: #ffcccc; color: #cc0000; font-weight: bold;";
            }
            territoryLabel->setStyleSheet(labelStyle);
            territoryLabel->setContextMenuPolicy(Qt::CustomContextMenu);

            // Store territory name as property for context menu
            territoryLabel->setProperty("territoryName", territoryName);
            territoryLabel->setProperty("playerId", player->getId());

            // Connect context menu for marking cities for destruction
            connect(territoryLabel, &QLabel::customContextMenuRequested, [this, player, territoryName, territoryLabel](const QPoint &pos) {
                showTerritoryContextMenu(player, territoryName, territoryLabel->mapToGlobal(pos));
            });

            // Add to grid
            gridLayout->addWidget(territoryLabel, row, col);

            // Move to next column, or next row if we've filled all columns
            col++;
            if (col >= NUM_COLUMNS) {
                col = 0;
                row++;
            }
        }

        layout->addWidget(frame);
    }

    groupBox->setLayout(layout);

    return groupBox;
}

QGroupBox* PlayerInfoWidget::createPiecesSection(Player *player)
{
    QGroupBox *groupBox = new QGroupBox("Pieces Inventory");
    QVBoxLayout *mainLayout = new QVBoxLayout();

    // Caesars - fixed height since there's exactly one
    QGroupBox *caesarBox = new QGroupBox(QString("Caesars (%1)").arg(player->getCaesarCount()));
    QTableWidget *caesarTable = new QTableWidget();
    caesarTable->setColumnCount(4);
    caesarTable->setHorizontalHeaderLabels({"Serial Number", "Territory", "Movement", "On Galley"});
    caesarTable->horizontalHeader()->setStretchLastSection(true);
    caesarTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    caesarTable->setAlternatingRowColors(true);
    caesarTable->setContextMenuPolicy(Qt::CustomContextMenu);
    caesarTable->setRowCount(player->getCaesarCount());

    // Set fixed height for Caesar table (1 row + header)
    caesarTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    caesarTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    caesarTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Connect context menu signal
    connect(caesarTable, &QTableWidget::customContextMenuRequested, [this, player, caesarTable](const QPoint &pos) {
        int row = caesarTable->rowAt(pos.y());
        if (row >= 0 && row < player->getCaesars().size()) {
            CaesarPiece *piece = player->getCaesars()[row];
            showCaesarContextMenu(piece, caesarTable->mapToGlobal(pos));
        }
    });

    int row = 0;
    for (CaesarPiece *piece : player->getCaesars()) {
        caesarTable->setItem(row, 0, new QTableWidgetItem(piece->getSerialNumber()));
        caesarTable->setItem(row, 1, new QTableWidgetItem(piece->getTerritoryName()));
        caesarTable->setItem(row, 2, new QTableWidgetItem(QString::number(piece->getMovesRemaining())));
        caesarTable->setItem(row, 3, new QTableWidgetItem(piece->getOnGalley()));
        row++;
    }

    // Calculate and set exact height for Caesar table (header + 1 row)
    if (player->getCaesarCount() > 0) {
        caesarTable->resizeRowsToContents();
        int tableHeight = caesarTable->horizontalHeader()->height() + caesarTable->rowHeight(0) + 2;
        caesarTable->setFixedHeight(tableHeight);
    }

    QVBoxLayout *caesarLayout = new QVBoxLayout();
    caesarLayout->addWidget(caesarTable);
    caesarBox->setLayout(caesarLayout);
    if (player->getCaesarCount() > 0) {
        mainLayout->addWidget(caesarBox);
    }

    // Generals
    QGroupBox *generalBox = new QGroupBox(QString("Generals (%1)").arg(player->getGeneralCount()));
    QTableWidget *generalTable = new QTableWidget();
    generalTable->setColumnCount(4);
    generalTable->setHorizontalHeaderLabels({"Serial Number", "Territory", "Movement", "On Galley"});
    generalTable->horizontalHeader()->setStretchLastSection(true);
    generalTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    generalTable->setAlternatingRowColors(true);
    generalTable->setContextMenuPolicy(Qt::CustomContextMenu);
    generalTable->setRowCount(player->getGeneralCount());
    generalTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    // Connect context menu signal
    connect(generalTable, &QTableWidget::customContextMenuRequested, [this, player, generalTable](const QPoint &pos) {
        int row = generalTable->rowAt(pos.y());
        if (row >= 0 && row < player->getGenerals().size()) {
            GeneralPiece *piece = player->getGenerals()[row];
            showGeneralContextMenu(piece, generalTable->mapToGlobal(pos));
        }
    });

    row = 0;
    for (GeneralPiece *piece : player->getGenerals()) {
        generalTable->setItem(row, 0, new QTableWidgetItem(piece->getSerialNumber()));
        generalTable->setItem(row, 1, new QTableWidgetItem(piece->getTerritoryName()));
        generalTable->setItem(row, 2, new QTableWidgetItem(QString::number(piece->getMovesRemaining())));
        generalTable->setItem(row, 3, new QTableWidgetItem(piece->getOnGalley()));

        row++;
    }
    // Resize generals table to fit content (max 10 rows visible)
    if (player->getGeneralCount() > 0) {
        generalTable->resizeRowsToContents();
        // Set a reasonable max height based on row count
        int visibleRows = qMin(player->getGeneralCount(), 10);
        int estimatedRowHeight = 25;  // Estimated row height
        int tableHeight = 30 + (visibleRows * estimatedRowHeight);  // Header + rows
        generalTable->setMaximumHeight(tableHeight);
    }

    QVBoxLayout *generalLayout = new QVBoxLayout();
    generalLayout->addWidget(generalTable);
    generalBox->setLayout(generalLayout);
    if (player->getGeneralCount() > 0) {
        mainLayout->addWidget(generalBox);
    }

    // Infantry
    QGroupBox *infantryBox = new QGroupBox(QString("Infantry (%1)").arg(player->getInfantryCount()));
    QTableWidget *infantryTable = new QTableWidget();
    infantryTable->setColumnCount(4);
    infantryTable->setHorizontalHeaderLabels({"Serial Number", "Territory", "Movement", "On Galley"});
    infantryTable->horizontalHeader()->setStretchLastSection(true);
    infantryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    infantryTable->setAlternatingRowColors(true);
    infantryTable->setRowCount(player->getInfantryCount());
    infantryTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    row = 0;
    for (InfantryPiece *piece : player->getInfantry()) {
        infantryTable->setItem(row, 0, new QTableWidgetItem(piece->getSerialNumber()));
        infantryTable->setItem(row, 1, new QTableWidgetItem(piece->getTerritoryName()));
        infantryTable->setItem(row, 2, new QTableWidgetItem(QString::number(piece->getMovesRemaining())));
        infantryTable->setItem(row, 3, new QTableWidgetItem(piece->getOnGalley()));
        row++;
    }
    // Resize infantry table to fit content (max 10 rows visible)
    infantryTable->resizeRowsToContents();
    if (player->getInfantryCount() > 0) {
        int visibleRows = qMin(player->getInfantryCount(), 10);
        int estimatedRowHeight = 25;
        int tableHeight = 30 + (visibleRows * estimatedRowHeight);
        infantryTable->setMaximumHeight(tableHeight);
    } else {
        // Minimal height when empty
        infantryTable->setMaximumHeight(50);
    }

    QVBoxLayout *infantryLayout = new QVBoxLayout();
    infantryLayout->addWidget(infantryTable);
    infantryBox->setLayout(infantryLayout);
    mainLayout->addWidget(infantryBox);  // Always show, even if count is 0

    // Cavalry
    QGroupBox *cavalryBox = new QGroupBox(QString("Cavalry (%1)").arg(player->getCavalryCount()));
    QTableWidget *cavalryTable = new QTableWidget();
    cavalryTable->setColumnCount(4);
    cavalryTable->setHorizontalHeaderLabels({"Serial Number", "Territory", "Movement", "On Galley"});
    cavalryTable->horizontalHeader()->setStretchLastSection(true);
    cavalryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    cavalryTable->setAlternatingRowColors(true);
    cavalryTable->setRowCount(player->getCavalryCount());
    cavalryTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    row = 0;
    for (CavalryPiece *piece : player->getCavalry()) {
        cavalryTable->setItem(row, 0, new QTableWidgetItem(piece->getSerialNumber()));
        cavalryTable->setItem(row, 1, new QTableWidgetItem(piece->getTerritoryName()));
        cavalryTable->setItem(row, 2, new QTableWidgetItem(QString::number(piece->getMovesRemaining())));
        cavalryTable->setItem(row, 3, new QTableWidgetItem(piece->getOnGalley()));
        row++;
    }
    // Resize cavalry table to fit content (max 10 rows visible)
    cavalryTable->resizeRowsToContents();
    if (player->getCavalryCount() > 0) {
        int visibleRows = qMin(player->getCavalryCount(), 10);
        int estimatedRowHeight = 25;
        int tableHeight = 30 + (visibleRows * estimatedRowHeight);
        cavalryTable->setMaximumHeight(tableHeight);
    } else {
        // Minimal height when empty
        cavalryTable->setMaximumHeight(50);
    }

    QVBoxLayout *cavalryLayout = new QVBoxLayout();
    cavalryLayout->addWidget(cavalryTable);
    cavalryBox->setLayout(cavalryLayout);
    // Always show cavalry box
    mainLayout->addWidget(cavalryBox);

    // Catapults
    QGroupBox *catapultBox = new QGroupBox(QString("Catapults (%1)").arg(player->getCatapultCount()));
    QTableWidget *catapultTable = new QTableWidget();
    catapultTable->setColumnCount(4);
    catapultTable->setHorizontalHeaderLabels({"Serial Number", "Territory", "Movement", "On Galley"});
    catapultTable->horizontalHeader()->setStretchLastSection(true);
    catapultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    catapultTable->setAlternatingRowColors(true);
    catapultTable->setRowCount(player->getCatapultCount());
    catapultTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    row = 0;
    for (CatapultPiece *piece : player->getCatapults()) {
        catapultTable->setItem(row, 0, new QTableWidgetItem(piece->getSerialNumber()));
        catapultTable->setItem(row, 1, new QTableWidgetItem(piece->getTerritoryName()));
        catapultTable->setItem(row, 2, new QTableWidgetItem(QString::number(piece->getMovesRemaining())));
        catapultTable->setItem(row, 3, new QTableWidgetItem(piece->getOnGalley()));
        row++;
    }
    // Resize catapult table to fit content (max 10 rows visible)
    catapultTable->resizeRowsToContents();
    if (player->getCatapultCount() > 0) {
        int visibleRows = qMin(player->getCatapultCount(), 10);
        int estimatedRowHeight = 25;
        int tableHeight = 30 + (visibleRows * estimatedRowHeight);
        catapultTable->setMaximumHeight(tableHeight);
    } else {
        // Minimal height when empty
        catapultTable->setMaximumHeight(50);
    }

    QVBoxLayout *catapultLayout = new QVBoxLayout();
    catapultLayout->addWidget(catapultTable);
    catapultBox->setLayout(catapultLayout);
    // Always show catapult box
    mainLayout->addWidget(catapultBox);

    // Galleys
    QGroupBox *galleyBox = new QGroupBox(QString("Galleys (%1)").arg(player->getGalleyCount()));
    QTableWidget *galleyTable = new QTableWidget();
    galleyTable->setColumnCount(4);
    galleyTable->setHorizontalHeaderLabels({"Serial Number", "Territory", "Movement", "On Galley"});
    galleyTable->horizontalHeader()->setStretchLastSection(true);
    galleyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    galleyTable->setAlternatingRowColors(true);
    galleyTable->setRowCount(player->getGalleyCount());
    galleyTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    row = 0;
    for (GalleyPiece *piece : player->getGalleys()) {
        galleyTable->setItem(row, 0, new QTableWidgetItem(piece->getSerialNumber()));
        galleyTable->setItem(row, 1, new QTableWidgetItem(piece->getTerritoryName()));
        galleyTable->setItem(row, 2, new QTableWidgetItem(QString::number(piece->getMovesRemaining())));
        galleyTable->setItem(row, 3, new QTableWidgetItem(piece->getOnGalley()));
        row++;
    }
    // Resize galley table to fit content (max 10 rows visible)
    galleyTable->resizeRowsToContents();
    if (player->getGalleyCount() > 0) {
        int visibleRows = qMin(player->getGalleyCount(), 10);
        int estimatedRowHeight = 25;
        int tableHeight = 30 + (visibleRows * estimatedRowHeight);
        galleyTable->setMaximumHeight(tableHeight);
    } else {
        // Minimal height when empty
        galleyTable->setMaximumHeight(50);
    }

    QVBoxLayout *galleyLayout = new QVBoxLayout();
    galleyLayout->addWidget(galleyTable);
    galleyBox->setLayout(galleyLayout);
    // Always show galley box
    mainLayout->addWidget(galleyBox);

    groupBox->setLayout(mainLayout);
    return groupBox;
}

QGroupBox* PlayerInfoWidget::createCapturedGeneralsSection(Player *player)
{
    QGroupBox *groupBox = new QGroupBox(QString("Captured Generals (%1)").arg(player->getCapturedGeneralCount()));
    QVBoxLayout *layout = new QVBoxLayout();

    if (player->getCapturedGeneralCount() == 0) {
        QLabel *emptyLabel = new QLabel("No captured generals");
        emptyLabel->setStyleSheet("font-style: italic; color: gray;");
        layout->addWidget(emptyLabel);
    } else {
        QTableWidget *table = new QTableWidget();
        table->setColumnCount(3);
        table->setHorizontalHeaderLabels({"Original Player", "Serial Number", "Territory"});
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setAlternatingRowColors(true);
        table->setRowCount(player->getCapturedGeneralCount());
        table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

        int row = 0;
        for (GeneralPiece *general : player->getCapturedGenerals()) {
            // Show original player ID
            table->setItem(row, 0, new QTableWidgetItem(QString("Player %1").arg(general->getPlayer())));
            table->setItem(row, 1, new QTableWidgetItem(general->getSerialNumber()));
            table->setItem(row, 2, new QTableWidgetItem(general->getTerritoryName()));
            row++;
        }

        layout->addWidget(table);
    }

    groupBox->setLayout(layout);
    return groupBox;
}

void PlayerInfoWidget::updatePlayerInfo(Player *player)
{
    if (!m_playerTabs.contains(player)) {
        return;
    }

    // Save current tab index to restore after update
    int currentTabIndex = m_tabWidget->currentIndex();

    // Recreate the tab for this player
    QWidget *oldTab = m_playerTabs[player];
    int tabIndex = m_tabWidget->indexOf(oldTab);

    if (tabIndex >= 0) {
        m_tabWidget->removeTab(tabIndex);
        delete oldTab;

        QWidget *newTab = createPlayerTab(player);
        m_playerTabs[player] = newTab;

        QString tabLabel = QString("Player %1").arg(player->getId());
        QIcon flagIcon;
        switch (player->getId().toLatin1()) {
            case 'A': flagIcon = QIcon(":/images/redFlag.png"); break;
            case 'B': flagIcon = QIcon(":/images/greenFlag.png"); break;
            case 'C': flagIcon = QIcon(":/images/blueFlag.png"); break;
            case 'D': flagIcon = QIcon(":/images/yellowFlag.png"); break;
            case 'E': flagIcon = QIcon(":/images/blackFlag.png"); break;
            case 'F': flagIcon = QIcon(":/images/orangeFlag.png"); break;
        }
        m_tabWidget->insertTab(tabIndex, newTab, flagIcon, tabLabel);

        // Restore the previously selected tab
        m_tabWidget->setCurrentIndex(currentTabIndex);
    }
}

void PlayerInfoWidget::updateAllPlayers()
{
    for (int i = 0; i < m_players.size(); ++i) {
        Player *player = m_players[i];
        updatePlayerInfo(player);

        // Enable/disable the tab's content widget based on whose turn it is
        // This allows viewing all tabs but only interacting with the current player's tab
        QWidget *tabWidget = m_tabWidget->widget(i);
        if (tabWidget) {
            tabWidget->setEnabled(player->isMyTurn());
        }
    }
}

void PlayerInfoWidget::showCaesarContextMenu(CaesarPiece *piece, const QPoint &pos)
{
    if (!piece || !m_mapWidget) return;

    // Find the player who owns this piece
    Player *player = nullptr;
    for (Player *p : m_players) {
        if (p->getId() == piece->getPlayer()) {
            player = p;
            break;
        }
    }
    if (!player) return;

    QString territoryName = piece->getTerritoryName();

    // Create menu with just this Caesar's movement options
    QMenu menu(this);
    QIcon caesarIcon(":/images/ceasarIcon.png");
    QString leaderName = QString("Caesar %1 (%2 moves)").arg(piece->getPlayer()).arg(piece->getMovesRemaining());

    // Get neighbors using MapGraph
    QList<QString> neighbors = m_mapWidget->getGraph()->getNeighbors(territoryName);

    // Get territories connected by roads
    QList<QString> roadConnectedTerritories;
    QSet<QString> visited;
    QList<QString> toVisit;

    visited.insert(territoryName);
    toVisit.append(territoryName);

    // BFS through road network
    while (!toVisit.isEmpty()) {
        QString currentTerritory = toVisit.takeFirst();

        for (Road *road : player->getRoads()) {
            QString territory1 = road->getTerritoryName();
            Position toPos = road->getToPosition();
            QString territory2 = m_mapWidget->getTerritoryNameAt(toPos.row, toPos.col);

            QString nextTerritory;
            if (territory1 == currentTerritory && !visited.contains(territory2)) {
                nextTerritory = territory2;
            } else if (territory2 == currentTerritory && !visited.contains(territory1)) {
                nextTerritory = territory1;
            }

            if (!nextTerritory.isEmpty()) {
                visited.insert(nextTerritory);
                toVisit.append(nextTerritory);
                if (!neighbors.contains(nextTerritory)) {
                    roadConnectedTerritories.append(nextTerritory);
                }
            }
        }
    }

    // Combine neighbors and road-connected territories
    QList<QString> allDestinations = neighbors + roadConnectedTerritories;

    // Add each destination as a movement option
    for (const QString &destinationName : allDestinations) {
        Position destPos = m_mapWidget->territoryNameToPosition(destinationName);
        int value = m_mapWidget->getTerritoryValueAt(destPos.row, destPos.col);
        bool isSea = m_mapWidget->isSeaTerritory(destPos.row, destPos.col);
        QChar owner = m_mapWidget->getTerritoryOwnerAt(destPos.row, destPos.col);

        // Build display text
        bool isViaRoad = roadConnectedTerritories.contains(destinationName);
        QString ownership = (owner == '\0') ? "[Unclaimed]" : (owner == piece->getPlayer()) ? "[You]" : QString("[Player %1]").arg(owner);
        QString troops = getTroopInfoAt(destPos.row, destPos.col);
        QString roadIndicator = isViaRoad ? " [via road]" : "";
        QString displayText = (value > 0) ? QString("%1 (%2) %3%4%5").arg(destinationName).arg(value).arg(ownership).arg(troops).arg(roadIndicator)
                                          : QString("%1 %2%3%4").arg(destinationName).arg(ownership).arg(troops).arg(roadIndicator);

        // Determine icon
        QIcon moveIcon;
        bool hasCombat = false;
        bool hasCity = false;

        // Check for combat
        for (Player *p : m_players) {
            if (p->getId() != player->getId()) {
                QList<GamePiece*> enemyPieces = p->getPiecesAtTerritory(destinationName);
                if (!enemyPieces.isEmpty()) {
                    hasCombat = true;
                    break;
                }
            }
        }
        if (!hasCombat && owner != '\0' && owner != player->getId()) {
            hasCombat = true;
        }

        // Check for city
        if (!hasCombat) {
            for (Player *p : m_players) {
                if (p->getCityAtTerritory(destinationName)) {
                    hasCity = true;
                    break;
                }
            }
        }

        // Set icon
        if (hasCombat) {
            moveIcon = QIcon(":/images/combatIcon.png");
        } else if (hasCity) {
            moveIcon = QIcon(":/images/newCityIcon.png");
        } else if (owner != '\0') {
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
                moveIcon = QIcon(flagPath);
            }
        }

        QAction *moveToAction = menu.addAction(moveIcon, displayText);
        moveToAction->setEnabled(!isSea && piece->getMovesRemaining() > 0);

        connect(moveToAction, &QAction::triggered, [this, piece, destinationName]() {
            moveLeaderToTerritory(piece, destinationName);
        });
    }

    menu.exec(pos);
}


void PlayerInfoWidget::showGeneralContextMenu(GeneralPiece *piece, const QPoint &pos)
{
    if (!piece || !m_mapWidget) return;

    // Find the player who owns this piece
    Player *player = nullptr;
    for (Player *p : m_players) {
        if (p->getId() == piece->getPlayer()) {
            player = p;
            break;
        }
    }
    if (!player) return;

    QString territoryName = piece->getTerritoryName();

    // Create menu with just this General's movement options
    QMenu menu(this);
    QIcon generalIcon(":/images/generalIcon.png");
    QString leaderName = QString("General %1 #%2 (%3 moves)").arg(piece->getPlayer()).arg(piece->getNumber()).arg(piece->getMovesRemaining());

    // Get neighbors using MapGraph
    QList<QString> neighbors = m_mapWidget->getGraph()->getNeighbors(territoryName);

    // Get territories connected by roads
    QList<QString> roadConnectedTerritories;
    QSet<QString> visited;
    QList<QString> toVisit;

    visited.insert(territoryName);
    toVisit.append(territoryName);

    // BFS through road network
    while (!toVisit.isEmpty()) {
        QString currentTerritory = toVisit.takeFirst();

        for (Road *road : player->getRoads()) {
            QString territory1 = road->getTerritoryName();
            Position toPos = road->getToPosition();
            QString territory2 = m_mapWidget->getTerritoryNameAt(toPos.row, toPos.col);

            QString nextTerritory;
            if (territory1 == currentTerritory && !visited.contains(territory2)) {
                nextTerritory = territory2;
            } else if (territory2 == currentTerritory && !visited.contains(territory1)) {
                nextTerritory = territory1;
            }

            if (!nextTerritory.isEmpty()) {
                visited.insert(nextTerritory);
                toVisit.append(nextTerritory);
                if (!neighbors.contains(nextTerritory)) {
                    roadConnectedTerritories.append(nextTerritory);
                }
            }
        }
    }

    // Combine neighbors and road-connected territories
    QList<QString> allDestinations = neighbors + roadConnectedTerritories;

    // Add each destination as a movement option
    for (const QString &destinationName : allDestinations) {
        Position destPos = m_mapWidget->territoryNameToPosition(destinationName);
        int value = m_mapWidget->getTerritoryValueAt(destPos.row, destPos.col);
        bool isSea = m_mapWidget->isSeaTerritory(destPos.row, destPos.col);
        QChar owner = m_mapWidget->getTerritoryOwnerAt(destPos.row, destPos.col);

        // Build display text
        bool isViaRoad = roadConnectedTerritories.contains(destinationName);
        QString ownership = (owner == '\0') ? "[Unclaimed]" : (owner == piece->getPlayer()) ? "[You]" : QString("[Player %1]").arg(owner);
        QString troops = getTroopInfoAt(destPos.row, destPos.col);
        QString roadIndicator = isViaRoad ? " [via road]" : "";
        QString displayText = (value > 0) ? QString("%1 (%2) %3%4%5").arg(destinationName).arg(value).arg(ownership).arg(troops).arg(roadIndicator)
                                          : QString("%1 %2%3%4").arg(destinationName).arg(ownership).arg(troops).arg(roadIndicator);

        // Determine icon
        QIcon moveIcon;
        bool hasCombat = false;
        bool hasCity = false;

        // Check for combat
        for (Player *p : m_players) {
            if (p->getId() != player->getId()) {
                QList<GamePiece*> enemyPieces = p->getPiecesAtTerritory(destinationName);
                if (!enemyPieces.isEmpty()) {
                    hasCombat = true;
                    break;
                }
            }
        }
        if (!hasCombat && owner != '\0' && owner != player->getId()) {
            hasCombat = true;
        }

        // Check for city
        if (!hasCombat) {
            for (Player *p : m_players) {
                if (p->getCityAtTerritory(destinationName)) {
                    hasCity = true;
                    break;
                }
            }
        }

        // Set icon
        if (hasCombat) {
            moveIcon = QIcon(":/images/combatIcon.png");
        } else if (hasCity) {
            moveIcon = QIcon(":/images/newCityIcon.png");
        } else if (owner != '\0') {
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
                moveIcon = QIcon(flagPath);
            }
        }

        QAction *moveToAction = menu.addAction(moveIcon, displayText);
        moveToAction->setEnabled(!isSea && piece->getMovesRemaining() > 0);

        connect(moveToAction, &QAction::triggered, [this, piece, destinationName]() {
            moveLeaderToTerritory(piece, destinationName);
        });
    }

    menu.exec(pos);
}

void PlayerInfoWidget::showTerritoryContextMenu(Player *player, const QString &territoryName, const QPoint &pos)
{
    if (!player) return;

    // Check if this territory has a city
    City *city = player->getCityAtTerritory(territoryName);

    if (!city) {
        // No city, no menu
        return;
    }

    QMenu menu(this);

    // Add checkable action for marking city for destruction
    QString cityType = city->isFortified() ? "Walled City" : "City";
    QIcon fireCityIcon(":/images/fireCityIcon.png");
    QAction *markAction = menu.addAction(fireCityIcon, QString("Mark %1 for Destruction").arg(cityType));
    markAction->setCheckable(true);
    markAction->setChecked(city->isMarkedForDestruction());

    connect(markAction, &QAction::triggered, [this, city, player](bool checked) {
        city->setMarkedForDestruction(checked);
        qDebug() << "City at" << city->getTerritoryName() << (checked ? "marked" : "unmarked") << "for destruction";

        // Update the display to reflect the change
        updatePlayerInfo(player);

        // Update the map to show visual change
        if (m_mapWidget) {
            m_mapWidget->update();
        }
    });

    menu.exec(pos);
}

void PlayerInfoWidget::handleTerritoryRightClick(const QString &territoryName, const QPoint &globalPos, QChar currentPlayer)
{
    if (!m_mapWidget) return;

    qDebug() << "Right-clicked on territory:" << territoryName << "by player" << currentPlayer;

    // Check if this territory is disputed (has troops from multiple players)
    bool isDisputed = false;
    QChar firstPlayerWithTroops = '\0';
    for (Player *player : m_players) {
        QList<GamePiece*> pieces = player->getPiecesAtTerritory(territoryName);

        // Check if this player has any troops (not just leaders)
        bool hasTroops = false;
        for (GamePiece *piece : pieces) {
            GamePiece::Type type = piece->getType();
            if (type == GamePiece::Type::Infantry ||
                type == GamePiece::Type::Cavalry ||
                type == GamePiece::Type::Catapult) {
                hasTroops = true;
                break;
            }
        }

        if (hasTroops) {
            if (firstPlayerWithTroops == '\0') {
                firstPlayerWithTroops = player->getId();
            } else if (firstPlayerWithTroops != player->getId()) {
                isDisputed = true;
                break;
            }
        }
    }

    // Don't show menu for disputed territories (combat zone)
    if (isDisputed) {
        qDebug() << "Territory is disputed - not showing movement menu";
        return;
    }

    // Find the current player
    Player *player = nullptr;
    for (Player *p : m_players) {
        if (p->getId() == currentPlayer) {
            player = p;
            break;
        }
    }

    if (!player) {
        qDebug() << "Could not find player" << currentPlayer;
        return;
    }

    // Find all leaders at this territory belonging to current player
    QList<GamePiece*> leaders;
    QList<GamePiece*> piecesAtTerritory = player->getPiecesAtTerritory(territoryName);

    for (GamePiece *piece : piecesAtTerritory) {
        GamePiece::Type type = piece->getType();
        if (type == GamePiece::Type::Caesar ||
            type == GamePiece::Type::General ||
            type == GamePiece::Type::Galley) {
            // Only include leaders with moves remaining
            if (piece->getMovesRemaining() > 0) {
                leaders.append(piece);
            }
        }
    }

    if (leaders.isEmpty()) {
        qDebug() << "No movable leaders found at" << territoryName;
        return;
    }

    qDebug() << "Found" << leaders.size() << "movable leaders at" << territoryName;

    // Create main menu
    QMenu menu(this);
    menu.setTitle(QString("Leaders at %1").arg(territoryName));

    // Add each leader with their movement submenu
    for (GamePiece *leader : leaders) {
        QString leaderName;
        QIcon leaderIcon;
        if (leader->getType() == GamePiece::Type::Caesar) {
            leaderName = QString("Caesar %1 (%2 moves)").arg(leader->getPlayer()).arg(leader->getMovesRemaining());
            leaderIcon = QIcon(":/images/ceasarIcon.png");
        } else if (leader->getType() == GamePiece::Type::General) {
            GeneralPiece *general = static_cast<GeneralPiece*>(leader);
            leaderName = QString("General %1 #%2 (%3 moves)").arg(leader->getPlayer()).arg(general->getNumber()).arg(leader->getMovesRemaining());
            leaderIcon = QIcon(":/images/generalIcon.png");
        } else if (leader->getType() == GamePiece::Type::Galley) {
            leaderName = QString("Galley %1 (%2 moves)").arg(leader->getPlayer()).arg(leader->getMovesRemaining());
            leaderIcon = QIcon(":/images/galleyIcon.png");
        }

        // Create submenu for this leader's movement options
        QMenu *leaderSubmenu = menu.addMenu(leaderIcon, leaderName);

        // Get neighbors using MapGraph
        QList<QString> neighbors = m_mapWidget->getGraph()->getNeighbors(territoryName);

        // Get territories connected by roads from this territory using BFS to find all reachable territories
        QList<QString> roadConnectedTerritories;
        QSet<QString> visited;
        QList<QString> toVisit;

        visited.insert(territoryName);
        toVisit.append(territoryName);

        // BFS through road network
        while (!toVisit.isEmpty()) {
            QString currentTerritory = toVisit.takeFirst();

            // Look through all roads for connections from currentTerritory
            for (Road *road : player->getRoads()) {
                QString territory1 = road->getTerritoryName();  // "from" territory
                Position toPos = road->getToPosition();
                QString territory2 = m_mapWidget->getTerritoryNameAt(toPos.row, toPos.col);  // "to" territory

                QString nextTerritory;
                if (territory1 == currentTerritory && !visited.contains(territory2)) {
                    nextTerritory = territory2;
                } else if (territory2 == currentTerritory && !visited.contains(territory1)) {
                    nextTerritory = territory1;
                }

                if (!nextTerritory.isEmpty()) {
                    visited.insert(nextTerritory);
                    toVisit.append(nextTerritory);
                    // Only add to destinations if it's not already a neighbor
                    if (!neighbors.contains(nextTerritory)) {
                        roadConnectedTerritories.append(nextTerritory);
                    }
                }
            }
        }

        // Combine neighbors and road-connected territories
        QList<QString> allDestinations = neighbors + roadConnectedTerritories;

        // Add each destination as a movement option
        for (const QString &destinationName : allDestinations) {
            Position destPos = m_mapWidget->territoryNameToPosition(destinationName);
            int value = m_mapWidget->getTerritoryValueAt(destPos.row, destPos.col);
            bool isSea = m_mapWidget->isSeaTerritory(destPos.row, destPos.col);
            QChar owner = m_mapWidget->getTerritoryOwnerAt(destPos.row, destPos.col);

            // Build display text - indicate if this is via road
            bool isViaRoad = roadConnectedTerritories.contains(destinationName);
            QString ownership = (owner == '\0') ? "[Unclaimed]" : (owner == leader->getPlayer()) ? "[You]" : QString("[Player %1]").arg(owner);
            QString troops = getTroopInfoAt(destPos.row, destPos.col);
            QString roadIndicator = isViaRoad ? " [via road]" : "";
            QString displayText = (value > 0) ? QString("%1 (%2) %3%4%5").arg(destinationName).arg(value).arg(ownership).arg(troops).arg(roadIndicator)
                                              : QString("%1 %2%3%4").arg(destinationName).arg(ownership).arg(troops).arg(roadIndicator);

            // Determine icon - combat has precedence over city, city over flag
            QIcon moveIcon;
            bool hasCombat = false;
            bool hasCity = false;

            // Check for combat (enemy pieces or enemy-owned territory)
            for (Player *p : m_players) {
                if (p->getId() != player->getId()) {
                    QList<GamePiece*> enemyPieces = p->getPiecesAtTerritory(destinationName);
                    if (!enemyPieces.isEmpty()) {
                        hasCombat = true;
                        qDebug() << "  Combat detected at" << destinationName << "- enemy pieces from player" << p->getId();
                        break;
                    }
                }
            }
            if (!hasCombat && owner != '\0' && owner != player->getId()) {
                hasCombat = true;  // Enemy-owned territory
                qDebug() << "  Combat detected at" << destinationName << "- enemy-owned territory by player" << owner;
            }

            qDebug() << "  Icon decision for" << destinationName << "- hasCombat:" << hasCombat << "hasCity:" << hasCity << "owner:" << owner;

            // Check for city (any player)
            if (!hasCombat) {
                for (Player *p : m_players) {
                    City *cityAtDest = p->getCityAtTerritory(destinationName);
                    if (cityAtDest) {
                        hasCity = true;
                        break;
                    }
                }
            }

            // Set icon based on what we found (combat > city > flag > nothing)
            if (hasCombat) {
                moveIcon = QIcon(":/images/combatIcon.png");
            } else if (hasCity) {
                moveIcon = QIcon(":/images/newCityIcon.png");
            } else if (owner != '\0') {
                // Territory is owned - show flag based on player color
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
                    moveIcon = QIcon(flagPath);
                }
            }

            QAction *moveToAction = leaderSubmenu->addAction(moveIcon, displayText);
            moveToAction->setEnabled(!isSea);

            // Connect to movement handler
            connect(moveToAction, &QAction::triggered, [this, leader, destinationName]() {
                moveLeaderToTerritory(leader, destinationName);
            });
        }
    }

    // Add separator if there are leaders (menu items above)
    if (!leaders.isEmpty()) {
        menu.addSeparator();
    }

    // Check if this territory has a city belonging to current player
    City *city = player->getCityAtTerritory(territoryName);
    if (city) {
        QString cityType = city->isFortified() ? "Walled City" : "City";
        QIcon fireCityIcon(":/images/fireCityIcon.png");
        QAction *markAction = menu.addAction(fireCityIcon, QString("Mark %1 for Destruction").arg(cityType));
        markAction->setCheckable(true);
        markAction->setChecked(city->isMarkedForDestruction());

        connect(markAction, &QAction::triggered, [this, city, player](bool checked) {
            city->setMarkedForDestruction(checked);
            qDebug() << "City at" << city->getTerritoryName() << (checked ? "marked" : "unmarked") << "for destruction";

            // Update the display to reflect the change
            updatePlayerInfo(player);

            // Update the map to show visual change
            if (m_mapWidget) {
                m_mapWidget->update();
            }
        });
    }

    // Show the menu (only if there are leaders or a city)
    if (!leaders.isEmpty() || city) {
        menu.exec(globalPos);
    }
}

// OLD GRID-BASED CODE REMOVED BELOW - Now using MapGraph for neighbor lookup
#if 0
// This old code is disabled and will be removed later
void OLD_UNUSED_GRID_CODE() {
    // Up (row - 1) - OLD CODE
    bool upIsSea = (currentPos.row > 0) && m_mapWidget->isSeaTerritory(currentPos.row - 1, currentPos.col);
    QChar upOwner = (currentPos.row > 0) ? m_mapWidget->getTerritoryOwnerAt(currentPos.row - 1, currentPos.col) : '\0';
    QString upOwnership = (upOwner == '\0') ? "[Unclaimed]" : (upOwner == piece->getPlayer()) ? "[You]" : QString("[Player %1]").arg(upOwner);
    QString upTroops = (currentPos.row > 0) ? getTroopInfoAt(currentPos.row - 1, currentPos.col) : "";
    QString upText = (upValue > 0) ? QString("%1 (%2) %3%4").arg(upTerritory).arg(upValue).arg(upOwnership).arg(upTroops) : QString("%1 %2%3").arg(upTerritory).arg(upOwnership).arg(upTroops);
    QIcon upIcon = style()->standardIcon(QStyle::SP_ArrowUp);

    // Check if there's a road connection upward
    Position upPos = {currentPos.row - 1, currentPos.col};
    QList<Position> upRoadConnections = (currentPos.row > 0) ? m_mapWidget->getTerritoriesConnectedByRoad(upPos, piece->getPlayer()) : QList<Position>();

    if (upRoadConnections.size() > 1) {
        // There's a road network - create submenu
        QMenu *upRoadSubmenu = new QMenu(upText, this);
        upRoadSubmenu->setIcon(upIcon);

        // First, add the adjacent territory itself (upPos) as an option
        QAction *adjacentAction = upRoadSubmenu->addAction(upIcon, upText);
        adjacentAction->setEnabled(piece->getMovesRemaining() > 0);
        connect(adjacentAction, &QAction::triggered, [this, piece, upPos]() { moveLeaderViaRoad(piece, upPos); });

        // Then add all other territories connected by road (except current position and upPos)
        for (const Position &roadPos : upRoadConnections) {
            if (roadPos == currentPos) continue; // Skip current position
            if (roadPos == upPos) continue; // Skip the adjacent territory (already added above)

            QString roadTerritory = getTerritoryNameAt(roadPos.row, roadPos.col);
            int roadValue = m_mapWidget->getTerritoryValueAt(roadPos.row, roadPos.col);
            QChar roadOwner = m_mapWidget->getTerritoryOwnerAt(roadPos.row, roadPos.col);
            QString roadOwnership = (roadOwner == '\0') ? "[Unclaimed]" : (roadOwner == piece->getPlayer()) ? "[You]" : QString("[Player %1]").arg(roadOwner);
            QString roadTroops = getTroopInfoAt(roadPos.row, roadPos.col);
            QString roadText = (roadValue > 0) ? QString("%1 (%2) %3%4 [Via Road]").arg(roadTerritory).arg(roadValue).arg(roadOwnership).arg(roadTroops) : QString("%1 %2%3 [Via Road]").arg(roadTerritory).arg(roadOwnership).arg(roadTroops);

            // Determine icon for road destination (combat > city > flag > nothing)
            QIcon roadIcon;
            bool roadHasCombat = false;
            bool roadHasCity = false;
            for (Player *p : m_players) {
                if (p->getId() != piece->getPlayer()) {
                    QList<GamePiece*> enemyPieces = p->getPiecesAtTerritory(roadTerritory);
                    if (!enemyPieces.isEmpty()) {
                        roadHasCombat = true;
                        break;
                    }
                }
            }
            if (!roadHasCombat && roadOwner != '\0' && roadOwner != piece->getPlayer()) {
                roadHasCombat = true;
            }
            if (!roadHasCombat) {
                for (Player *p : m_players) {
                    if (p->getCityAtTerritory(roadTerritory)) {
                        roadHasCity = true;
                        break;
                    }
                }
            }
            if (roadHasCombat) {
                roadIcon = QIcon(":/images/combatIcon.png");
            } else if (roadHasCity) {
                roadIcon = QIcon(":/images/newCityIcon.png");
            } else if (roadOwner != '\0') {
                QString flagPath;
                switch (roadOwner.toLatin1()) {
                    case 'A': flagPath = ":/images/redFlag.png"; break;
                    case 'B': flagPath = ":/images/greenFlag.png"; break;
                    case 'C': flagPath = ":/images/blueFlag.png"; break;
                    case 'D': flagPath = ":/images/yellowFlag.png"; break;
                    case 'E': flagPath = ":/images/blackFlag.png"; break;
                    case 'F': flagPath = ":/images/orangeFlag.png"; break;
                }
                if (!flagPath.isEmpty()) {
                    roadIcon = QIcon(flagPath);
                }
            }

            QAction *roadAction = upRoadSubmenu->addAction(roadIcon, roadText);
            roadAction->setEnabled(piece->getMovesRemaining() > 0);
            connect(roadAction, &QAction::triggered, [this, piece, roadPos]() { moveLeaderViaRoad(piece, roadPos); });
        }

        moveSubmenu->addMenu(upRoadSubmenu);
        upRoadSubmenu->setEnabled(currentPos.row > 0 && !upIsSea && piece->getMovesRemaining() > 0);
    } else {
        // No road network - regular move
        QAction *upAction = moveSubmenu->addAction(upIcon, upText);
        upAction->setEnabled(currentPos.row > 0 && !upIsSea && piece->getMovesRemaining() > 0);
        connect(upAction, &QAction::triggered, [this, piece]() { moveLeaderWithTroops(piece, -1, 0); });
    }

    // Down (row + 1)
    QString downTerritory = (currentPos.row < 7) ? getTerritoryNameAt(currentPos.row + 1, currentPos.col) : "Off Board";
    int downValue = (currentPos.row < 7) ? m_mapWidget->getTerritoryValueAt(currentPos.row + 1, currentPos.col) : 0;
    bool downIsSea = (currentPos.row < 7) && m_mapWidget->isSeaTerritory(currentPos.row + 1, currentPos.col);
    QChar downOwner = (currentPos.row < 7) ? m_mapWidget->getTerritoryOwnerAt(currentPos.row + 1, currentPos.col) : '\0';
    QString downOwnership = (downOwner == '\0') ? "[Unclaimed]" : (downOwner == piece->getPlayer()) ? "[You]" : QString("[Player %1]").arg(downOwner);
    QString downTroops = (currentPos.row < 7) ? getTroopInfoAt(currentPos.row + 1, currentPos.col) : "";
    QString downText = (downValue > 0) ? QString("%1 (%2) %3%4").arg(downTerritory).arg(downValue).arg(downOwnership).arg(downTroops) : QString("%1 %2%3").arg(downTerritory).arg(downOwnership).arg(downTroops);

    // Determine icon for down direction (combat > city > flag > arrow)
    QIcon downIcon;
    bool downHasCombat = false;
    bool downHasCity = false;
    if (currentPos.row < 7) {
        for (Player *p : m_players) {
            if (p->getId() != piece->getPlayer()) {
                QList<GamePiece*> enemyPieces = p->getPiecesAtTerritory(downTerritory);
                if (!enemyPieces.isEmpty()) {
                    downHasCombat = true;
                    break;
                }
            }
        }
        if (!downHasCombat && downOwner != '\0' && downOwner != piece->getPlayer()) {
            downHasCombat = true;
        }
        if (!downHasCombat) {
            for (Player *p : m_players) {
                if (p->getCityAtTerritory(downTerritory)) {
                    downHasCity = true;
                    break;
                }
            }
        }
    }
    if (downHasCombat) {
        downIcon = QIcon(":/images/combatIcon.png");
    } else if (downHasCity) {
        downIcon = QIcon(":/images/newCityIcon.png");
    } else if (downOwner != '\0') {
        QString flagPath;
        switch (downOwner.toLatin1()) {
            case 'A': flagPath = ":/images/redFlag.png"; break;
            case 'B': flagPath = ":/images/greenFlag.png"; break;
            case 'C': flagPath = ":/images/blueFlag.png"; break;
            case 'D': flagPath = ":/images/yellowFlag.png"; break;
            case 'E': flagPath = ":/images/blackFlag.png"; break;
            case 'F': flagPath = ":/images/orangeFlag.png"; break;
        }
        if (!flagPath.isEmpty()) {
            downIcon = QIcon(flagPath);
        } else {
            downIcon = style()->standardIcon(QStyle::SP_ArrowDown);
        }
    } else {
        downIcon = style()->standardIcon(QStyle::SP_ArrowDown);
    }

    // Check if there's a road connection downward
    Position downPos = {currentPos.row + 1, currentPos.col};
    QList<Position> downRoadConnections = (currentPos.row < 7) ? m_mapWidget->getTerritoriesConnectedByRoad(downPos, piece->getPlayer()) : QList<Position>();

    if (downRoadConnections.size() > 1) {
        // There's a road network - create submenu
        QMenu *downRoadSubmenu = new QMenu(downText, this);
        downRoadSubmenu->setIcon(downIcon);

        // First, add the adjacent territory itself (downPos) as an option
        QAction *adjacentAction = downRoadSubmenu->addAction(downText);
        adjacentAction->setEnabled(piece->getMovesRemaining() > 0);
        connect(adjacentAction, &QAction::triggered, [this, piece, downPos]() { moveLeaderViaRoad(piece, downPos); });

        // Then add all other territories connected by road (except current position and downPos)
        for (const Position &roadPos : downRoadConnections) {
            if (roadPos == currentPos) continue; // Skip current position
            if (roadPos == downPos) continue; // Skip the adjacent territory (already added above)

            QString roadTerritory = getTerritoryNameAt(roadPos.row, roadPos.col);
            int roadValue = m_mapWidget->getTerritoryValueAt(roadPos.row, roadPos.col);
            QChar roadOwner = m_mapWidget->getTerritoryOwnerAt(roadPos.row, roadPos.col);
            QString roadOwnership = (roadOwner == '\0') ? "[Unclaimed]" : (roadOwner == piece->getPlayer()) ? "[You]" : QString("[Player %1]").arg(roadOwner);
            QString roadTroops = getTroopInfoAt(roadPos.row, roadPos.col);
            QString roadText = (roadValue > 0) ? QString("%1 (%2) %3%4 [Via Road]").arg(roadTerritory).arg(roadValue).arg(roadOwnership).arg(roadTroops) : QString("%1 %2%3 [Via Road]").arg(roadTerritory).arg(roadOwnership).arg(roadTroops);

            QAction *roadAction = downRoadSubmenu->addAction(roadText);
            roadAction->setEnabled(piece->getMovesRemaining() > 0);
            connect(roadAction, &QAction::triggered, [this, piece, roadPos]() { moveLeaderViaRoad(piece, roadPos); });
        }

        moveSubmenu->addMenu(downRoadSubmenu);
        downRoadSubmenu->setEnabled(currentPos.row < 7 && !downIsSea && piece->getMovesRemaining() > 0);
    } else {
        // No road network - regular move
        QAction *downAction = moveSubmenu->addAction(downIcon, downText);
        downAction->setEnabled(currentPos.row < 7 && !downIsSea && piece->getMovesRemaining() > 0);
        connect(downAction, &QAction::triggered, [this, piece]() { moveLeaderWithTroops(piece, 1, 0); });
    }

    // Left (col - 1)
    QString leftTerritory = (currentPos.col > 0) ? getTerritoryNameAt(currentPos.row, currentPos.col - 1) : "Off Board";
    int leftValue = (currentPos.col > 0) ? m_mapWidget->getTerritoryValueAt(currentPos.row, currentPos.col - 1) : 0;
    bool leftIsSea = (currentPos.col > 0) && m_mapWidget->isSeaTerritory(currentPos.row, currentPos.col - 1);
    QChar leftOwner = (currentPos.col > 0) ? m_mapWidget->getTerritoryOwnerAt(currentPos.row, currentPos.col - 1) : '\0';
    QString leftOwnership = (leftOwner == '\0') ? "[Unclaimed]" : (leftOwner == piece->getPlayer()) ? "[You]" : QString("[Player %1]").arg(leftOwner);
    QString leftTroops = (currentPos.col > 0) ? getTroopInfoAt(currentPos.row, currentPos.col - 1) : "";
    QString leftText = (leftValue > 0) ? QString("%1 (%2) %3%4").arg(leftTerritory).arg(leftValue).arg(leftOwnership).arg(leftTroops) : QString("%1 %2%3").arg(leftTerritory).arg(leftOwnership).arg(leftTroops);

    // Determine icon for left direction (combat > city > flag > arrow)
    QIcon leftIcon;
    bool leftHasCombat = false;
    bool leftHasCity = false;
    if (currentPos.col > 0) {
        for (Player *p : m_players) {
            if (p->getId() != piece->getPlayer()) {
                QList<GamePiece*> enemyPieces = p->getPiecesAtTerritory(leftTerritory);
                if (!enemyPieces.isEmpty()) {
                    leftHasCombat = true;
                    break;
                }
            }
        }
        if (!leftHasCombat && leftOwner != '\0' && leftOwner != piece->getPlayer()) {
            leftHasCombat = true;
        }
        if (!leftHasCombat) {
            for (Player *p : m_players) {
                if (p->getCityAtTerritory(leftTerritory)) {
                    leftHasCity = true;
                    break;
                }
            }
        }
    }
    if (leftHasCombat) {
        leftIcon = QIcon(":/images/combatIcon.png");
    } else if (leftHasCity) {
        leftIcon = QIcon(":/images/newCityIcon.png");
    } else if (leftOwner != '\0') {
        QString flagPath;
        switch (leftOwner.toLatin1()) {
            case 'A': flagPath = ":/images/redFlag.png"; break;
            case 'B': flagPath = ":/images/greenFlag.png"; break;
            case 'C': flagPath = ":/images/blueFlag.png"; break;
            case 'D': flagPath = ":/images/yellowFlag.png"; break;
            case 'E': flagPath = ":/images/blackFlag.png"; break;
            case 'F': flagPath = ":/images/orangeFlag.png"; break;
        }
        if (!flagPath.isEmpty()) {
            leftIcon = QIcon(flagPath);
        } else {
            leftIcon = style()->standardIcon(QStyle::SP_ArrowBack);
        }
    } else {
        leftIcon = style()->standardIcon(QStyle::SP_ArrowBack);
    }

    // Check if there's a road connection leftward
    Position leftPos = {currentPos.row, currentPos.col - 1};
    QList<Position> leftRoadConnections = (currentPos.col > 0) ? m_mapWidget->getTerritoriesConnectedByRoad(leftPos, piece->getPlayer()) : QList<Position>();

    if (leftRoadConnections.size() > 1) {
        // There's a road network - create submenu
        QMenu *leftRoadSubmenu = new QMenu(leftText, this);
        leftRoadSubmenu->setIcon(leftIcon);

        // First, add the adjacent territory itself (leftPos) as an option
        QAction *adjacentAction = leftRoadSubmenu->addAction(leftText);
        adjacentAction->setEnabled(piece->getMovesRemaining() > 0);
        connect(adjacentAction, &QAction::triggered, [this, piece, leftPos]() { moveLeaderViaRoad(piece, leftPos); });

        // Then add all other territories connected by road (except current position and leftPos)
        for (const Position &roadPos : leftRoadConnections) {
            if (roadPos == currentPos) continue; // Skip current position
            if (roadPos == leftPos) continue; // Skip the adjacent territory (already added above)

            QString roadTerritory = getTerritoryNameAt(roadPos.row, roadPos.col);
            int roadValue = m_mapWidget->getTerritoryValueAt(roadPos.row, roadPos.col);
            QChar roadOwner = m_mapWidget->getTerritoryOwnerAt(roadPos.row, roadPos.col);
            QString roadOwnership = (roadOwner == '\0') ? "[Unclaimed]" : (roadOwner == piece->getPlayer()) ? "[You]" : QString("[Player %1]").arg(roadOwner);
            QString roadTroops = getTroopInfoAt(roadPos.row, roadPos.col);
            QString roadText = (roadValue > 0) ? QString("%1 (%2) %3%4 [Via Road]").arg(roadTerritory).arg(roadValue).arg(roadOwnership).arg(roadTroops) : QString("%1 %2%3 [Via Road]").arg(roadTerritory).arg(roadOwnership).arg(roadTroops);

            QAction *roadAction = leftRoadSubmenu->addAction(roadText);
            roadAction->setEnabled(piece->getMovesRemaining() > 0);
            connect(roadAction, &QAction::triggered, [this, piece, roadPos]() { moveLeaderViaRoad(piece, roadPos); });
        }

        moveSubmenu->addMenu(leftRoadSubmenu);
        leftRoadSubmenu->setEnabled(currentPos.col > 0 && !leftIsSea && piece->getMovesRemaining() > 0);
    } else {
        // No road network - regular move
        QAction *leftAction = moveSubmenu->addAction(leftIcon, leftText);
        leftAction->setEnabled(currentPos.col > 0 && !leftIsSea && piece->getMovesRemaining() > 0);
        connect(leftAction, &QAction::triggered, [this, piece]() { moveLeaderWithTroops(piece, 0, -1); });
    }

    // Right (col + 1)
    QString rightTerritory = (currentPos.col < 11) ? getTerritoryNameAt(currentPos.row, currentPos.col + 1) : "Off Board";
    int rightValue = (currentPos.col < 11) ? m_mapWidget->getTerritoryValueAt(currentPos.row, currentPos.col + 1) : 0;
    bool rightIsSea = (currentPos.col < 11) && m_mapWidget->isSeaTerritory(currentPos.row, currentPos.col + 1);
    QChar rightOwner = (currentPos.col < 11) ? m_mapWidget->getTerritoryOwnerAt(currentPos.row, currentPos.col + 1) : '\0';
    QString rightOwnership = (rightOwner == '\0') ? "[Unclaimed]" : (rightOwner == piece->getPlayer()) ? "[You]" : QString("[Player %1]").arg(rightOwner);
    QString rightTroops = (currentPos.col < 11) ? getTroopInfoAt(currentPos.row, currentPos.col + 1) : "";
    QString rightText = (rightValue > 0) ? QString("%1 (%2) %3%4").arg(rightTerritory).arg(rightValue).arg(rightOwnership).arg(rightTroops) : QString("%1 %2%3").arg(rightTerritory).arg(rightOwnership).arg(rightTroops);

    // Determine icon for right direction (combat > city > flag > arrow)
    QIcon rightIcon;
    bool rightHasCombat = false;
    bool rightHasCity = false;
    if (currentPos.col < 11) {
        for (Player *p : m_players) {
            if (p->getId() != piece->getPlayer()) {
                QList<GamePiece*> enemyPieces = p->getPiecesAtTerritory(rightTerritory);
                if (!enemyPieces.isEmpty()) {
                    rightHasCombat = true;
                    break;
                }
            }
        }
        if (!rightHasCombat && rightOwner != '\0' && rightOwner != piece->getPlayer()) {
            rightHasCombat = true;
        }
        if (!rightHasCombat) {
            for (Player *p : m_players) {
                if (p->getCityAtTerritory(rightTerritory)) {
                    rightHasCity = true;
                    break;
                }
            }
        }
    }
    if (rightHasCombat) {
        rightIcon = QIcon(":/images/combatIcon.png");
    } else if (rightHasCity) {
        rightIcon = QIcon(":/images/newCityIcon.png");
    } else if (rightOwner != '\0') {
        QString flagPath;
        switch (rightOwner.toLatin1()) {
            case 'A': flagPath = ":/images/redFlag.png"; break;
            case 'B': flagPath = ":/images/greenFlag.png"; break;
            case 'C': flagPath = ":/images/blueFlag.png"; break;
            case 'D': flagPath = ":/images/yellowFlag.png"; break;
            case 'E': flagPath = ":/images/blackFlag.png"; break;
            case 'F': flagPath = ":/images/orangeFlag.png"; break;
        }
        if (!flagPath.isEmpty()) {
            rightIcon = QIcon(flagPath);
        } else {
            rightIcon = style()->standardIcon(QStyle::SP_ArrowForward);
        }
    } else {
        rightIcon = style()->standardIcon(QStyle::SP_ArrowForward);
    }

    // Check if there's a road connection rightward
    Position rightPos = {currentPos.row, currentPos.col + 1};
    QList<Position> rightRoadConnections = (currentPos.col < 11) ? m_mapWidget->getTerritoriesConnectedByRoad(rightPos, piece->getPlayer()) : QList<Position>();

    if (rightRoadConnections.size() > 1) {
        // There's a road network - create submenu
        QMenu *rightRoadSubmenu = new QMenu(rightText, this);
        rightRoadSubmenu->setIcon(rightIcon);

        // First, add the adjacent territory itself (rightPos) as an option
        QAction *adjacentAction = rightRoadSubmenu->addAction(rightText);
        adjacentAction->setEnabled(piece->getMovesRemaining() > 0);
        connect(adjacentAction, &QAction::triggered, [this, piece, rightPos]() { moveLeaderViaRoad(piece, rightPos); });

        // Then add all other territories connected by road (except current position and rightPos)
        for (const Position &roadPos : rightRoadConnections) {
            if (roadPos == currentPos) continue; // Skip current position
            if (roadPos == rightPos) continue; // Skip the adjacent territory (already added above)

            QString roadTerritory = getTerritoryNameAt(roadPos.row, roadPos.col);
            int roadValue = m_mapWidget->getTerritoryValueAt(roadPos.row, roadPos.col);
            QChar roadOwner = m_mapWidget->getTerritoryOwnerAt(roadPos.row, roadPos.col);
            QString roadOwnership = (roadOwner == '\0') ? "[Unclaimed]" : (roadOwner == piece->getPlayer()) ? "[You]" : QString("[Player %1]").arg(roadOwner);
            QString roadTroops = getTroopInfoAt(roadPos.row, roadPos.col);
            QString roadText = (roadValue > 0) ? QString("%1 (%2) %3%4 [Via Road]").arg(roadTerritory).arg(roadValue).arg(roadOwnership).arg(roadTroops) : QString("%1 %2%3 [Via Road]").arg(roadTerritory).arg(roadOwnership).arg(roadTroops);

            QAction *roadAction = rightRoadSubmenu->addAction(roadText);
            roadAction->setEnabled(piece->getMovesRemaining() > 0);
            connect(roadAction, &QAction::triggered, [this, piece, roadPos]() { moveLeaderViaRoad(piece, roadPos); });
        }

        moveSubmenu->addMenu(rightRoadSubmenu);
        rightRoadSubmenu->setEnabled(currentPos.col < 11 && !rightIsSea && piece->getMovesRemaining() > 0);
    } else {
        // No road network - regular move
        QAction *rightAction = moveSubmenu->addAction(rightIcon, rightText);
        rightAction->setEnabled(currentPos.col < 11 && !rightIsSea && piece->getMovesRemaining() > 0);
        connect(rightAction, &QAction::triggered, [this, piece]() { moveLeaderWithTroops(piece, 0, 1); });
    }

}  // End of old grid code block
#endif  // End of disabled old code

void PlayerInfoWidget::movePiece(GamePiece *piece, int rowDelta, int colDelta)
{
    if (!piece || !m_mapWidget) return;

    Position currentPos = piece->getPosition();
    Position newPos = {currentPos.row + rowDelta, currentPos.col + colDelta};

    // Validate boundaries
    if (newPos.row < 0 || newPos.row >= 8 || newPos.col < 0 || newPos.col >= 12) {
        return;
    }

    // Get the new territory name
    QString newTerritoryName = getTerritoryNameAt(newPos.row, newPos.col);

    // Find which player owns this piece
    Player *owningPlayer = nullptr;
    for (Player *player : m_players) {
        if (player->getId() == piece->getPlayer()) {
            owningPlayer = player;
            break;
        }
    }

    if (!owningPlayer) {
        qDebug() << "ERROR: Could not find owner for piece ID:" << piece->getUniqueId();
        return;
    }

    // Handle territory ownership changes
    // ONLY claim territory if there are NO enemy pieces at the destination
    // If there are enemy pieces, combat will handle territory transfer
    bool hasEnemyPieces = false;
    for (Player *otherPlayer : m_players) {
        if (otherPlayer->getId() != owningPlayer->getId()) {
            QList<GamePiece*> enemyPieces = otherPlayer->getPiecesAtTerritory(newTerritoryName);
            if (!enemyPieces.isEmpty()) {
                hasEnemyPieces = true;
                break;
            }
        }
    }

    // Only transfer territory if no enemies present
    if (!hasEnemyPieces) {
        // Check if another player owns this territory
        for (Player *otherPlayer : m_players) {
            if (otherPlayer != owningPlayer && otherPlayer->ownsTerritory(newTerritoryName)) {
                // Remove ownership from the other player
                otherPlayer->unclaimTerritory(newTerritoryName);
                break;
            }
        }

        // Claim the new territory for the moving player
        if (!owningPlayer->ownsTerritory(newTerritoryName)) {
            owningPlayer->claimTerritory(newTerritoryName);
        }
    }

    // Update position
    piece->setPosition(newPos);

    // Update territory name
    piece->setTerritoryName(newTerritoryName);

    // Decrement movement
    piece->setMovesRemaining(piece->getMovesRemaining() - 1);

    // Mark that a move has been made (no longer at start of turn)
    if (m_mapWidget) {
        m_mapWidget->setAtStartOfTurn(false);
    }

    // Emit signal to notify MapWidget to redraw affected territories
    emit pieceMoved(currentPos.row, currentPos.col, newPos.row, newPos.col);

    // Note: We don't update the display here anymore - let the caller handle it
    // This prevents multiple updates when moving a leader + troops
}

// Helper function to move piece without decrementing movement (for road travel)
void PlayerInfoWidget::movePieceWithoutCost(GamePiece *piece, int rowDelta, int colDelta)
{
    if (!piece || !m_mapWidget) return;

    Position currentPos = piece->getPosition();
    Position newPos = {currentPos.row + rowDelta, currentPos.col + colDelta};

    // Validate boundaries
    if (newPos.row < 0 || newPos.row >= 8 || newPos.col < 0 || newPos.col >= 12) {
        return;
    }

    // Get the new territory name
    QString newTerritoryName = getTerritoryNameAt(newPos.row, newPos.col);

    // Find which player owns this piece
    Player *owningPlayer = nullptr;
    for (Player *player : m_players) {
        if (player->getId() == piece->getPlayer()) {
            owningPlayer = player;
            break;
        }
    }

    if (!owningPlayer) {
        qDebug() << "ERROR: Could not find owner for piece ID:" << piece->getUniqueId();
        return;
    }

    // Handle territory ownership changes
    // ONLY claim territory if there are NO enemy pieces at the destination
    // If there are enemy pieces, combat will handle territory transfer
    bool hasEnemyPieces = false;
    for (Player *otherPlayer : m_players) {
        if (otherPlayer->getId() != owningPlayer->getId()) {
            QList<GamePiece*> enemyPieces = otherPlayer->getPiecesAtTerritory(newTerritoryName);
            if (!enemyPieces.isEmpty()) {
                hasEnemyPieces = true;
                break;
            }
        }
    }

    // Only transfer territory if no enemies present
    if (!hasEnemyPieces) {
        // Check if another player owns this territory
        for (Player *otherPlayer : m_players) {
            if (otherPlayer != owningPlayer && otherPlayer->ownsTerritory(newTerritoryName)) {
                // Remove ownership from the other player
                otherPlayer->unclaimTerritory(newTerritoryName);
                break;
            }
        }

        // Claim the new territory for the moving player
        if (!owningPlayer->ownsTerritory(newTerritoryName)) {
            owningPlayer->claimTerritory(newTerritoryName);
        }
    }

    // Update position
    piece->setPosition(newPos);

    // Update territory name
    piece->setTerritoryName(newTerritoryName);

    // DO NOT decrement movement - caller will handle it

    // Mark that a move has been made (no longer at start of turn)
    if (m_mapWidget) {
        m_mapWidget->setAtStartOfTurn(false);
    }

    // Emit signal to notify MapWidget to redraw affected territories
    emit pieceMoved(currentPos.row, currentPos.col, newPos.row, newPos.col);
}

void PlayerInfoWidget::moveLeaderToTerritory(GamePiece *leader, const QString &destinationTerritory)
{
    if (!leader || !m_mapWidget) return;

    qDebug() << "Moving leader to territory:" << destinationTerritory;

    // Get current and destination positions
    QString currentTerritory = leader->getTerritoryName();
    Position currentPos = m_mapWidget->territoryNameToPosition(currentTerritory);
    Position destPos = m_mapWidget->territoryNameToPosition(destinationTerritory);

    qDebug() << "  From:" << currentTerritory << "pos" << currentPos.row << currentPos.col;
    qDebug() << "  To:" << destinationTerritory << "pos" << destPos.row << destPos.col;

    // Find which player owns this leader
    Player *owningPlayer = nullptr;
    for (Player *player : m_players) {
        if (player->getId() == leader->getPlayer()) {
            owningPlayer = player;
            break;
        }
    }

    if (!owningPlayer) return;

    // Get all troops at the same territory as the leader
    QList<GamePiece*> allPiecesAtTerritory = owningPlayer->getPiecesAtTerritory(currentTerritory);
    qDebug() << "  Found" << allPiecesAtTerritory.size() << "pieces at" << currentTerritory;

    // Get legion IDs based on leader type
    QList<int> legionIds;
    if (leader->getType() == GamePiece::Type::Caesar) {
        legionIds = static_cast<CaesarPiece*>(leader)->getLegion();
    } else if (leader->getType() == GamePiece::Type::General) {
        legionIds = static_cast<GeneralPiece*>(leader)->getLegion();
    } else if (leader->getType() == GamePiece::Type::Galley) {
        legionIds = static_cast<GalleyPiece*>(leader)->getLegion();
    }
    qDebug() << "  Leader's legion has" << legionIds.size() << "troops:" << legionIds;

    // Filter to only include actual troops in this leader's legion
    QList<GamePiece*> troopsToMove;
    for (GamePiece *piece : allPiecesAtTerritory) {
        if (legionIds.contains(piece->getUniqueId())) {
            troopsToMove.append(piece);
        }
    }
    qDebug() << "  Troops to move:" << troopsToMove.size();

    // Check if there are enemy pieces at the destination
    QString destTerritory = destinationTerritory;
    bool hasEnemies = false;
    for (Player *player : m_players) {
        if (player->getId() != owningPlayer->getId()) {
            QList<GamePiece*> enemyPieces = player->getPiecesAtTerritory(destTerritory);
            if (!enemyPieces.isEmpty()) {
                hasEnemies = true;
                break;
            }
        }
    }

    // Get leader name for display
    QString leaderName;
    if (leader->getType() == GamePiece::Type::Caesar) {
        leaderName = QString("Caesar %1").arg(leader->getPlayer());
    } else if (leader->getType() == GamePiece::Type::General) {
        GeneralPiece *general = static_cast<GeneralPiece*>(leader);
        leaderName = QString("General %1 #%2").arg(leader->getPlayer()).arg(general->getNumber());
    } else if (leader->getType() == GamePiece::Type::Galley) {
        leaderName = QString("Galley %1").arg(leader->getPlayer());
    }

    // Get all troops at this territory (regardless of moves remaining)
    QList<GamePiece*> allTroops;
    for (GamePiece *piece : allPiecesAtTerritory) {
        GamePiece::Type type = piece->getType();
        if (type == GamePiece::Type::Infantry ||
            type == GamePiece::Type::Cavalry ||
            type == GamePiece::Type::Catapult) {
            allTroops.append(piece);
        }
    }

    // Check if moving into combat - need to also check territory ownership
    bool movingIntoCombat = hasEnemies;
    if (!movingIntoCombat) {
        QChar destOwner = m_mapWidget->getTerritoryOwnerAt(destPos.row, destPos.col);
        if (destOwner != '\0' && destOwner != owningPlayer->getId()) {
            movingIntoCombat = true;
        }
    }

    // Always show troop selection dialog if there are ANY troops at this territory
    // Loop until user selects valid troops or cancels
    bool validSelection = false;
    QList<int> selectedTroopIds;

    while (!allTroops.isEmpty() && !validSelection) {
        TroopSelectionDialog dialog(leaderName, allTroops, legionIds, this);
        if (dialog.exec() == QDialog::Accepted) {
            selectedTroopIds = dialog.getSelectedTroopIds();

            // If moving into combat, validate that troops are selected
            if (movingIntoCombat && selectedTroopIds.isEmpty()) {
                QMessageBox::warning(this, "Cannot Move",
                    QString("%1 cannot move into combat without troops!\n\n"
                            "Leaders must have at least one troop in their legion to enter combat.\n\n"
                            "Please select at least one troop or cancel the move.")
                    .arg(leaderName));
                // Loop will continue - show dialog again
                continue;
            }

            // Validate that all selected troops have moves remaining
            QList<GamePiece*> troopsWithoutMoves;
            for (GamePiece *troop : allTroops) {
                if (selectedTroopIds.contains(troop->getUniqueId()) && troop->getMovesRemaining() <= 0) {
                    troopsWithoutMoves.append(troop);
                }
            }

            // If any selected troops don't have moves, show error and loop back
            if (!troopsWithoutMoves.isEmpty()) {
                QStringList troopNames;
                for (GamePiece *troop : troopsWithoutMoves) {
                    QString typeName;
                    if (troop->getType() == GamePiece::Type::Infantry) typeName = "Infantry";
                    else if (troop->getType() == GamePiece::Type::Cavalry) typeName = "Cavalry";
                    else if (troop->getType() == GamePiece::Type::Catapult) typeName = "Catapult";
                    troopNames.append(QString("%1 #%2").arg(typeName).arg(troop->getSerialNumber()));
                }

                QMessageBox::warning(this, "Cannot Move",
                    QString("The following troops have no moves remaining and cannot move:\n\n%1\n\n"
                            "Please deselect these troops or try again.")
                    .arg(troopNames.join("\n")));
                // Loop will continue - show dialog again
            } else {
                // Valid selection - all selected troops have moves
                validSelection = true;
            }
        } else {
            // User cancelled - abort the move
            return;
        }
    }

    // If we have a valid selection, proceed with the move
    if (validSelection) {
        // Update the leader's legion with the selected troops
        if (leader->getType() == GamePiece::Type::Caesar) {
            static_cast<CaesarPiece*>(leader)->setLegion(selectedTroopIds);
        } else if (leader->getType() == GamePiece::Type::General) {
            static_cast<GeneralPiece*>(leader)->setLegion(selectedTroopIds);
        } else if (leader->getType() == GamePiece::Type::Galley) {
            static_cast<GalleyPiece*>(leader)->setLegion(selectedTroopIds);
        }

        qDebug() << "Updated legion with" << selectedTroopIds.size() << "troops:" << selectedTroopIds;

        // Store last territory before moving (for retreat purposes)
        if (leader->getType() == GamePiece::Type::Caesar) {
            static_cast<CaesarPiece*>(leader)->setLastTerritory(currentPos);
        } else if (leader->getType() == GamePiece::Type::General) {
            static_cast<GeneralPiece*>(leader)->setLastTerritory(currentPos);
        } else if (leader->getType() == GamePiece::Type::Galley) {
            static_cast<GalleyPiece*>(leader)->setLastTerritory(currentPos);
        }

        // Move leader
        leader->setTerritoryName(destinationTerritory);
        leader->setPosition(destPos);
        leader->setMovesRemaining(leader->getMovesRemaining() - 1);

        qDebug() << "Moved leader" << leaderName;

        // Move selected troops
        for (GamePiece *troop : allTroops) {
            if (selectedTroopIds.contains(troop->getUniqueId())) {
                qDebug() << "Moving troop ID:" << troop->getUniqueId() << "to territory:" << destinationTerritory;
                troop->setTerritoryName(destinationTerritory);
                troop->setPosition(destPos);
                troop->setMovesRemaining(troop->getMovesRemaining() - 1);
            }
        }

        qDebug() << "Finished moving all troops";

        // Claim the destination territory for the owning player
        owningPlayer->claimTerritory(destinationTerritory);
        qDebug() << "Claimed territory:" << destinationTerritory << "for player" << owningPlayer->getId();

        // Update display
        updateAllPlayers();
        if (m_mapWidget) {
            m_mapWidget->update();
        }
    } else {
        // No troops available, check if moving into combat
        if (movingIntoCombat) {
            QMessageBox::warning(this, "Cannot Move",
                QString("%1 cannot move into combat without troops!\n\n"
                        "Leaders must have at least one troop in their legion to enter combat.")
                .arg(leaderName));
            return;
        }

        // Just move the leader (no combat)

        // Store last territory before moving (for retreat purposes)
        if (leader->getType() == GamePiece::Type::Caesar) {
            static_cast<CaesarPiece*>(leader)->setLastTerritory(currentPos);
        } else if (leader->getType() == GamePiece::Type::General) {
            static_cast<GeneralPiece*>(leader)->setLastTerritory(currentPos);
        } else if (leader->getType() == GamePiece::Type::Galley) {
            static_cast<GalleyPiece*>(leader)->setLastTerritory(currentPos);
        }

        leader->setTerritoryName(destinationTerritory);
        leader->setPosition(destPos);
        leader->setMovesRemaining(leader->getMovesRemaining() - 1);

        qDebug() << "Moved leader" << leaderName << "(no troops available)";

        // Claim the destination territory for the owning player
        owningPlayer->claimTerritory(destinationTerritory);
        qDebug() << "Claimed territory:" << destinationTerritory << "for player" << owningPlayer->getId();

        // Update display
        updateAllPlayers();
        if (m_mapWidget) {
            m_mapWidget->update();
        }
    }
}

void PlayerInfoWidget::moveLeaderWithTroops(GamePiece *leader, int rowDelta, int colDelta)
{
    if (!leader || !m_mapWidget) return;

    // Get the leader's current position
    Position currentPos = leader->getPosition();
    QString currentTerritory = leader->getTerritoryName();

    // Find the player who owns this leader
    Player *owningPlayer = nullptr;
    for (Player *player : m_players) {
        if (player->getId() == leader->getPlayer()) {
            owningPlayer = player;
            break;
        }
    }

    if (!owningPlayer) return;

    // Get all troops at the same territory as the leader
    QList<GamePiece*> allPiecesAtPosition = owningPlayer->getPiecesAtTerritory(currentTerritory);

    // Filter to only include actual troops (Infantry, Cavalry, Catapult)
    QList<GamePiece*> troopsAtPosition;
    for (GamePiece *piece : allPiecesAtPosition) {
        GamePiece::Type type = piece->getType();
        if (type == GamePiece::Type::Infantry ||
            type == GamePiece::Type::Cavalry ||
            type == GamePiece::Type::Catapult) {
            troopsAtPosition.append(piece);
        }
    }

    // Get the leader's current legion
    QList<int> currentLegion;
    if (leader->getType() == GamePiece::Type::Caesar) {
        currentLegion = static_cast<CaesarPiece*>(leader)->getLegion();
    } else if (leader->getType() == GamePiece::Type::General) {
        currentLegion = static_cast<GeneralPiece*>(leader)->getLegion();
    } else if (leader->getType() == GamePiece::Type::Galley) {
        currentLegion = static_cast<GalleyPiece*>(leader)->getLegion();
    }

    // Create leader name for dialog
    QString leaderName;
    if (leader->getType() == GamePiece::Type::Caesar) {
        leaderName = QString("Caesar %1").arg(leader->getPlayer());
    } else if (leader->getType() == GamePiece::Type::General) {
        leaderName = QString("General %1 #%2").arg(leader->getPlayer()).arg(static_cast<GeneralPiece*>(leader)->getNumber());
    } else if (leader->getType() == GamePiece::Type::Galley) {
        leaderName = QString("Galley %1").arg(leader->getPlayer());
    }

    // Calculate destination position
    Position destPos = {currentPos.row + rowDelta, currentPos.col + colDelta};
    QString destTerritory = getTerritoryNameAt(destPos.row, destPos.col);

    // Check if there are enemy pieces at the destination OR if destination is owned by enemy
    bool hasEnemies = false;
    QList<GamePiece*> enemyPiecesAtDest;

    // Check for enemy pieces
    for (Player *player : m_players) {
        if (player->getId() != owningPlayer->getId()) {
            QList<GamePiece*> enemyPieces = player->getPiecesAtTerritory(destTerritory);
            if (!enemyPieces.isEmpty()) {
                hasEnemies = true;
                enemyPiecesAtDest.append(enemyPieces);
            }
        }
    }

    // Also check if destination territory is owned by an enemy player
    if (!hasEnemies) {
        QChar destOwner = m_mapWidget->getTerritoryOwnerAt(destPos.row, destPos.col);
        if (destOwner != '\0' && destOwner != owningPlayer->getId()) {
            hasEnemies = true;  // Moving into enemy-owned territory
        }
    }

    // Show troop selection dialog and loop until valid or cancelled
    TroopSelectionDialog *dialog = new TroopSelectionDialog(leaderName, troopsAtPosition, currentLegion, this);

    bool validSelection = false;
    QList<int> selectedTroopIds;

    while (!validSelection) {
        if (dialog->exec() != QDialog::Accepted) {
            // User cancelled
            delete dialog;
            return;
        }

        selectedTroopIds = dialog->getSelectedTroopIds();

        // If moving into combat, validate that legion is not empty
        if (hasEnemies && selectedTroopIds.isEmpty()) {
            QMessageBox::warning(dialog, "Cannot Enter Combat",
                "Cannot enter combat without troops.\n\n"
                "A General/Caesar cannot fight alone. Please select at least one troop to form a legion.");
            // Dialog stays open, loop continues
            continue;
        }

        // Validate that all selected troops have moves remaining
        QStringList troopsWithoutMoves;
        for (int pieceId : selectedTroopIds) {
            for (GamePiece *piece : troopsAtPosition) {
                if (piece->getUniqueId() == pieceId && piece->getMovesRemaining() == 0) {
                    QString pieceName;
                    if (piece->getType() == GamePiece::Type::Infantry) {
                        pieceName = QString("Infantry ID:%1").arg(pieceId);
                    } else if (piece->getType() == GamePiece::Type::Cavalry) {
                        pieceName = QString("Cavalry ID:%1").arg(pieceId);
                    } else if (piece->getType() == GamePiece::Type::Catapult) {
                        pieceName = QString("Catapult ID:%1").arg(pieceId);
                    } else if (piece->getType() == GamePiece::Type::General) {
                        pieceName = QString("General #%1 ID:%2").arg(static_cast<GeneralPiece*>(piece)->getNumber()).arg(pieceId);
                    }
                    troopsWithoutMoves.append(pieceName);
                    break;
                }
            }
        }

        // If any selected troops have no moves, show error and loop again
        if (!troopsWithoutMoves.isEmpty()) {
            QString errorMsg = "The following troops have no moves remaining:\n\n";
            errorMsg += troopsWithoutMoves.join("\n");
            errorMsg += "\n\nPlease uncheck troops without moves and try again.";
            QMessageBox::warning(dialog, "Cannot Move", errorMsg);
            // Dialog stays open, loop continues
        } else {
            // All validation passed
            validSelection = true;
        }
    }

    // Now we have a valid selection - update the legion
    if (leader->getType() == GamePiece::Type::Caesar) {
        static_cast<CaesarPiece*>(leader)->setLegion(selectedTroopIds);
    } else if (leader->getType() == GamePiece::Type::General) {
        static_cast<GeneralPiece*>(leader)->setLegion(selectedTroopIds);
    } else if (leader->getType() == GamePiece::Type::Galley) {
        static_cast<GalleyPiece*>(leader)->setLegion(selectedTroopIds);
    }

    // If moving into combat, show warning dialog
    if (hasEnemies) {
        // Build description of our legion
        QStringList ourTroops;
        ourTroops << leaderName;
        int infantryCount = 0, cavalryCount = 0, catapultCount = 0;
        for (int pieceId : selectedTroopIds) {
            for (GamePiece *piece : troopsAtPosition) {
                if (piece->getUniqueId() == pieceId) {
                    if (piece->getType() == GamePiece::Type::Infantry) infantryCount++;
                    else if (piece->getType() == GamePiece::Type::Cavalry) cavalryCount++;
                    else if (piece->getType() == GamePiece::Type::Catapult) catapultCount++;
                    break;
                }
            }
        }
        if (infantryCount > 0) ourTroops << QString("%1 Infantry").arg(infantryCount);
        if (cavalryCount > 0) ourTroops << QString("%1 Cavalry").arg(cavalryCount);
        if (catapultCount > 0) ourTroops << QString("%1 Catapult").arg(catapultCount);

        // Build description of enemy forces
        QStringList enemyTroops;
        int enemyCaesars = 0, enemyGenerals = 0, enemyInfantry = 0;
        int enemyCavalry = 0, enemyCatapults = 0, enemyGalleys = 0;
        for (GamePiece *piece : enemyPiecesAtDest) {
            if (piece->getType() == GamePiece::Type::Caesar) enemyCaesars++;
            else if (piece->getType() == GamePiece::Type::General) enemyGenerals++;
            else if (piece->getType() == GamePiece::Type::Infantry) enemyInfantry++;
            else if (piece->getType() == GamePiece::Type::Cavalry) enemyCavalry++;
            else if (piece->getType() == GamePiece::Type::Catapult) enemyCatapults++;
            else if (piece->getType() == GamePiece::Type::Galley) enemyGalleys++;
        }
        if (enemyCaesars > 0) enemyTroops << QString("%1 Caesar").arg(enemyCaesars);
        if (enemyGenerals > 0) enemyTroops << QString("%1 General").arg(enemyGenerals);
        if (enemyInfantry > 0) enemyTroops << QString("%1 Infantry").arg(enemyInfantry);
        if (enemyCavalry > 0) enemyTroops << QString("%1 Cavalry").arg(enemyCavalry);
        if (enemyCatapults > 0) enemyTroops << QString("%1 Catapult").arg(enemyCatapults);
        if (enemyGalleys > 0) enemyTroops << QString("%1 Galley").arg(enemyGalleys);

        // Show warning
        QString warningMsg = QString("Your legion (%1) is about to enter combat with enemy forces (%2).\n\n"
                                     "Do you want to continue?")
                                 .arg(ourTroops.join(", "))
                                 .arg(enemyTroops.join(", "));

        QMessageBox::StandardButton reply = QMessageBox::question(this, "Enter Combat", warningMsg,
                                                                   QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            delete dialog;
            return;  // User cancelled combat entry
        }
    }

    // Save current tab index before any moves
    int currentTabIndex = m_tabWidget->currentIndex();

    // Store last territory before moving (for retreat purposes)
    if (leader->getType() == GamePiece::Type::Caesar) {
        static_cast<CaesarPiece*>(leader)->setLastTerritory(currentPos);
    } else if (leader->getType() == GamePiece::Type::General) {
        static_cast<GeneralPiece*>(leader)->setLastTerritory(currentPos);
    } else if (leader->getType() == GamePiece::Type::Galley) {
        static_cast<GalleyPiece*>(leader)->setLastTerritory(currentPos);
    }

    // Move the leader first
    movePiece(leader, rowDelta, colDelta);
    qDebug() << "Moved leader" << leaderName;

    // Move all selected troops
    for (int pieceId : selectedTroopIds) {
        for (GamePiece *piece : troopsAtPosition) {
            if (piece->getUniqueId() == pieceId) {
                qDebug() << "Moving troop ID:" << pieceId << "from" << piece->getPosition().row << piece->getPosition().col;
                movePiece(piece, rowDelta, colDelta);
                qDebug() << "  to" << piece->getPosition().row << piece->getPosition().col;
                break;
            }
        }
    }
    qDebug() << "Finished moving all troops";

    // If we entered combat, consume all remaining moves for the leader
    if (hasEnemies) {
        leader->setMovesRemaining(0);
        qDebug() << "Entered combat - all moves consumed for" << leaderName;
    }

    // Update display once after all moves
    updateAllPlayers();

    // Restore the tab index
    m_tabWidget->setCurrentIndex(currentTabIndex);

    delete dialog;
}

void PlayerInfoWidget::moveLeaderViaRoad(GamePiece *leader, const Position &destination)
{
    if (!leader || !m_mapWidget) return;

    // Get the leader's current position
    Position currentPos = leader->getPosition();

    // Calculate the delta to the destination
    int rowDelta = destination.row - currentPos.row;
    int colDelta = destination.col - currentPos.col;

    // Find the player who owns this leader
    Player *owningPlayer = nullptr;
    for (Player *player : m_players) {
        if (player->getId() == leader->getPlayer()) {
            owningPlayer = player;
            break;
        }
    }

    if (!owningPlayer) return;

    // Get all troops at the same territory as the leader
    QString currentTerritory = leader->getTerritoryName();
    QList<GamePiece*> allPiecesAtPosition = owningPlayer->getPiecesAtTerritory(currentTerritory);

    // Filter to only include actual troops (Infantry, Cavalry, Catapult)
    QList<GamePiece*> troopsAtPosition;
    for (GamePiece *piece : allPiecesAtPosition) {
        GamePiece::Type type = piece->getType();
        if (type == GamePiece::Type::Infantry ||
            type == GamePiece::Type::Cavalry ||
            type == GamePiece::Type::Catapult) {
            troopsAtPosition.append(piece);
        }
    }

    // Get the leader's current legion
    QList<int> currentLegion;
    if (leader->getType() == GamePiece::Type::Caesar) {
        currentLegion = static_cast<CaesarPiece*>(leader)->getLegion();
    } else if (leader->getType() == GamePiece::Type::General) {
        currentLegion = static_cast<GeneralPiece*>(leader)->getLegion();
    } else if (leader->getType() == GamePiece::Type::Galley) {
        currentLegion = static_cast<GalleyPiece*>(leader)->getLegion();
    }

    // Create leader name for dialog
    QString leaderName;
    if (leader->getType() == GamePiece::Type::Caesar) {
        leaderName = QString("Caesar %1").arg(leader->getPlayer());
    } else if (leader->getType() == GamePiece::Type::General) {
        leaderName = QString("General %1 #%2").arg(leader->getPlayer()).arg(static_cast<GeneralPiece*>(leader)->getNumber());
    } else if (leader->getType() == GamePiece::Type::Galley) {
        leaderName = QString("Galley %1").arg(leader->getPlayer());
    }

    // Check if there are enemy pieces at the destination OR if destination is owned by enemy
    bool hasEnemies = false;
    QList<GamePiece*> enemyPiecesAtDest;
    Position destPos = {destination.row, destination.col};
    QString destTerritory = getTerritoryNameAt(destPos.row, destPos.col);

    // Check for enemy pieces
    for (Player *player : m_players) {
        if (player->getId() != owningPlayer->getId()) {
            QList<GamePiece*> enemyPieces = player->getPiecesAtTerritory(destTerritory);
            if (!enemyPieces.isEmpty()) {
                hasEnemies = true;
                enemyPiecesAtDest.append(enemyPieces);
            }
        }
    }

    // Also check if destination territory is owned by an enemy player
    if (!hasEnemies) {
        QChar destOwner = m_mapWidget->getTerritoryOwnerAt(destPos.row, destPos.col);
        if (destOwner != '\0' && destOwner != owningPlayer->getId()) {
            hasEnemies = true;  // Moving into enemy-owned territory
        }
    }

    // Show troop selection dialog and loop until valid or cancelled
    TroopSelectionDialog *dialog = new TroopSelectionDialog(leaderName, troopsAtPosition, currentLegion, this);

    bool validSelection = false;
    QList<int> selectedTroopIds;

    while (!validSelection) {
        if (dialog->exec() != QDialog::Accepted) {
            // User cancelled
            delete dialog;
            return;
        }

        selectedTroopIds = dialog->getSelectedTroopIds();

        // If moving into combat, validate that legion is not empty
        if (hasEnemies && selectedTroopIds.isEmpty()) {
            QMessageBox::warning(dialog, "Cannot Enter Combat",
                "Cannot enter combat without troops.\n\n"
                "A General/Caesar cannot fight alone. Please select at least one troop to form a legion.");
            // Dialog stays open, loop continues
            continue;
        }

        // Validate that all selected troops have moves remaining (for road movement, only need 1 move)
        QStringList troopsWithoutMoves;
        for (int pieceId : selectedTroopIds) {
            for (GamePiece *piece : troopsAtPosition) {
                if (piece->getUniqueId() == pieceId && piece->getMovesRemaining() == 0) {
                    QString pieceName;
                    if (piece->getType() == GamePiece::Type::Infantry) {
                        pieceName = QString("Infantry ID:%1").arg(pieceId);
                    } else if (piece->getType() == GamePiece::Type::Cavalry) {
                        pieceName = QString("Cavalry ID:%1").arg(pieceId);
                    } else if (piece->getType() == GamePiece::Type::Catapult) {
                        pieceName = QString("Catapult ID:%1").arg(pieceId);
                    } else if (piece->getType() == GamePiece::Type::General) {
                        pieceName = QString("General #%1 ID:%2").arg(static_cast<GeneralPiece*>(piece)->getNumber()).arg(pieceId);
                    }
                    troopsWithoutMoves.append(pieceName);
                    break;
                }
            }
        }

        // If any selected troops have no moves, show error and loop again
        if (!troopsWithoutMoves.isEmpty()) {
            QString errorMsg = "The following troops have no moves remaining:\n\n";
            errorMsg += troopsWithoutMoves.join("\n");
            errorMsg += "\n\nPlease uncheck troops without moves and try again.";
            QMessageBox::warning(dialog, "Cannot Move", errorMsg);
            // Dialog stays open, loop continues
        } else {
            // All validation passed
            validSelection = true;
        }
    }

    // Now we have a valid selection - update the legion
    if (leader->getType() == GamePiece::Type::Caesar) {
        static_cast<CaesarPiece*>(leader)->setLegion(selectedTroopIds);
    } else if (leader->getType() == GamePiece::Type::General) {
        static_cast<GeneralPiece*>(leader)->setLegion(selectedTroopIds);
    } else if (leader->getType() == GamePiece::Type::Galley) {
        static_cast<GalleyPiece*>(leader)->setLegion(selectedTroopIds);
    }

    // If moving into combat, show warning dialog
    if (hasEnemies) {
        // Build description of our legion
        QStringList ourTroops;
        ourTroops << leaderName;
        int infantryCount = 0, cavalryCount = 0, catapultCount = 0;
        for (int pieceId : selectedTroopIds) {
            for (GamePiece *piece : troopsAtPosition) {
                if (piece->getUniqueId() == pieceId) {
                    if (piece->getType() == GamePiece::Type::Infantry) infantryCount++;
                    else if (piece->getType() == GamePiece::Type::Cavalry) cavalryCount++;
                    else if (piece->getType() == GamePiece::Type::Catapult) catapultCount++;
                    break;
                }
            }
        }
        if (infantryCount > 0) ourTroops << QString("%1 Infantry").arg(infantryCount);
        if (cavalryCount > 0) ourTroops << QString("%1 Cavalry").arg(cavalryCount);
        if (catapultCount > 0) ourTroops << QString("%1 Catapult").arg(catapultCount);

        // Build description of enemy forces
        QStringList enemyTroops;
        int enemyCaesars = 0, enemyGenerals = 0, enemyInfantry = 0;
        int enemyCavalry = 0, enemyCatapults = 0, enemyGalleys = 0;
        for (GamePiece *piece : enemyPiecesAtDest) {
            if (piece->getType() == GamePiece::Type::Caesar) enemyCaesars++;
            else if (piece->getType() == GamePiece::Type::General) enemyGenerals++;
            else if (piece->getType() == GamePiece::Type::Infantry) enemyInfantry++;
            else if (piece->getType() == GamePiece::Type::Cavalry) enemyCavalry++;
            else if (piece->getType() == GamePiece::Type::Catapult) enemyCatapults++;
            else if (piece->getType() == GamePiece::Type::Galley) enemyGalleys++;
        }
        if (enemyCaesars > 0) enemyTroops << QString("%1 Caesar").arg(enemyCaesars);
        if (enemyGenerals > 0) enemyTroops << QString("%1 General").arg(enemyGenerals);
        if (enemyInfantry > 0) enemyTroops << QString("%1 Infantry").arg(enemyInfantry);
        if (enemyCavalry > 0) enemyTroops << QString("%1 Cavalry").arg(enemyCavalry);
        if (enemyCatapults > 0) enemyTroops << QString("%1 Catapult").arg(enemyCatapults);
        if (enemyGalleys > 0) enemyTroops << QString("%1 Galley").arg(enemyGalleys);

        // Show warning (mention road travel)
        QString warningMsg = QString("Your legion (%1) is about to travel via road and enter combat with enemy forces (%2).\n\n"
                                     "Do you want to continue?")
                                 .arg(ourTroops.join(", "))
                                 .arg(enemyTroops.join(", "));

        QMessageBox::StandardButton reply = QMessageBox::question(this, "Enter Combat (Via Road)", warningMsg,
                                                                   QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            delete dialog;
            return;  // User cancelled combat entry
        }
    }

    // Save current tab index before any moves
    int currentTabIndex = m_tabWidget->currentIndex();

    // Store last territory before moving (for retreat purposes)
    if (leader->getType() == GamePiece::Type::Caesar) {
        static_cast<CaesarPiece*>(leader)->setLastTerritory(currentPos);
    } else if (leader->getType() == GamePiece::Type::General) {
        static_cast<GeneralPiece*>(leader)->setLastTerritory(currentPos);
    } else if (leader->getType() == GamePiece::Type::Galley) {
        static_cast<GalleyPiece*>(leader)->setLastTerritory(currentPos);
    }

    // Move the leader using the calculated delta (without consuming movement points yet)
    movePieceWithoutCost(leader, rowDelta, colDelta);
    qDebug() << "Moved leader" << leaderName << "via road to" << destination.row << destination.col;

    // Move all selected troops (without consuming movement points yet)
    for (int pieceId : selectedTroopIds) {
        for (GamePiece *piece : troopsAtPosition) {
            if (piece->getUniqueId() == pieceId) {
                qDebug() << "Moving troop ID:" << pieceId << "via road from" << piece->getPosition().row << piece->getPosition().col;
                movePieceWithoutCost(piece, rowDelta, colDelta);
                qDebug() << "  to" << piece->getPosition().row << piece->getPosition().col;
                break;
            }
        }
    }
    qDebug() << "Finished moving all troops via road";

    // IMPORTANT: Road movement only costs 1 movement point, regardless of distance
    // Deduct 1 move from leader
    if (leader->getMovesRemaining() > 0) {
        leader->setMovesRemaining(leader->getMovesRemaining() - 1);
    }

    // Deduct 1 move from all troops that moved
    for (int pieceId : selectedTroopIds) {
        for (GamePiece *piece : troopsAtPosition) {
            if (piece->getUniqueId() == pieceId && piece->getMovesRemaining() > 0) {
                piece->setMovesRemaining(piece->getMovesRemaining() - 1);
                break;
            }
        }
    }

    // If we entered combat, consume all remaining moves for the leader
    if (hasEnemies) {
        leader->setMovesRemaining(0);
        qDebug() << "Entered combat via road - all moves consumed for" << leaderName;
    }

    // Update display once after all moves
    updateAllPlayers();

    // Restore the tab index
    m_tabWidget->setCurrentIndex(currentTabIndex);

    delete dialog;
}

QString PlayerInfoWidget::getTerritoryNameAt(int row, int col) const
{
    if (!m_mapWidget) {
        return "Unknown";
    }

    return m_mapWidget->getTerritoryNameAt(row, col);
}

QString PlayerInfoWidget::getTroopInfoAt(int row, int col) const
{
    if (row < 0 || row >= 8 || col < 0 || col >= 12) {
        return "";
    }

    QString territoryName = getTerritoryNameAt(row, col);
    QStringList troopInfo;

    // Check all players for troops at this territory
    for (Player *player : m_players) {
        int caesarCount = 0;
        int generalCount = 0;
        int infantryCount = 0;
        int cavalryCount = 0;
        int catapultCount = 0;
        int galleyCount = 0;

        // Get all pieces at this territory
        QList<GamePiece*> piecesHere = player->getPiecesAtTerritory(territoryName);

        for (GamePiece *piece : piecesHere) {
            switch (piece->getType()) {
                case GamePiece::Type::Caesar:
                    caesarCount++;
                    break;
                case GamePiece::Type::General:
                    generalCount++;
                    break;
                case GamePiece::Type::Infantry:
                    infantryCount++;
                    break;
                case GamePiece::Type::Cavalry:
                    cavalryCount++;
                    break;
                case GamePiece::Type::Catapult:
                    catapultCount++;
                    break;
                case GamePiece::Type::Galley:
                    galleyCount++;
                    break;
            }
        }

        // Build troop summary for this player
        QStringList playerTroops;
        if (caesarCount > 0) playerTroops << QString("C:%1").arg(caesarCount);
        if (generalCount > 0) playerTroops << QString("G:%1").arg(generalCount);
        if (infantryCount > 0) playerTroops << QString("I:%1").arg(infantryCount);
        if (cavalryCount > 0) playerTroops << QString("Cv:%1").arg(cavalryCount);
        if (catapultCount > 0) playerTroops << QString("Ct:%1").arg(catapultCount);
        if (galleyCount > 0) playerTroops << QString("Gl:%1").arg(galleyCount);

        if (!playerTroops.isEmpty()) {
            troopInfo << QString("P%1[%2]").arg(player->getId()).arg(playerTroops.join(","));
        }
    }

    if (troopInfo.isEmpty()) {
        return "";
    }

    return " {" + troopInfo.join(" ") + "}";
}

QIcon PlayerInfoWidget::createTerritoryIcon(int row, int col, QChar currentPlayer) const
{
    if (!m_mapWidget) {
        return QIcon();
    }

    // Create a 32x32 pixmap for the icon
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Get territory owner
    QChar owner = m_mapWidget->getTerritoryOwnerAt(row, col);

    // Determine circle color based on ownership
    QColor circleColor;
    if (owner == '\0') {
        circleColor = Qt::white;  // Unowned
    } else {
        circleColor = m_mapWidget->getPlayerColor(owner);  // Owner's color
    }

    // Draw ownership circle (left side)
    painter.setBrush(circleColor);
    painter.setPen(QPen(Qt::black, 1));
    painter.drawEllipse(4, 8, 16, 16);

    // Check if there are enemy pieces
    bool hasEnemies = m_mapWidget->hasEnemyPiecesAt(row, col, currentPlayer);

    if (hasEnemies) {
        // Draw sword icon (right side) to indicate combat
        painter.setPen(QPen(Qt::darkRed, 2));
        painter.setBrush(Qt::gray);

        // Simple sword shape: handle + blade
        // Blade (vertical line)
        painter.drawLine(26, 10, 26, 20);
        // Crossguard (horizontal line)
        painter.drawLine(23, 12, 29, 12);
        // Handle
        painter.drawLine(26, 12, 26, 16);
        // Pommel (small circle)
        painter.drawEllipse(24, 16, 4, 4);
    }

    return QIcon(pixmap);
}

void PlayerInfoWidget::saveSettings()
{
    QSettings settings("ConquestOfTheEmpire", "PlayerInfoWidget");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveGeometry());
}

void PlayerInfoWidget::loadSettings()
{
    QSettings settings("ConquestOfTheEmpire", "PlayerInfoWidget");

    // Restore geometry if it was saved
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    } else {
        // Default size if no saved settings
        resize(800, 600);
    }
}

void PlayerInfoWidget::closeEvent(QCloseEvent *event)
{
    saveSettings();
    event->accept();
}

void PlayerInfoWidget::onEndTurnClicked()
{
    // Find the current player (whose turn it is)
    Player *currentPlayer = nullptr;
    int currentPlayerIndex = -1;
    for (int i = 0; i < m_players.size(); ++i) {
        if (m_players[i]->isMyTurn()) {
            currentPlayer = m_players[i];
            currentPlayerIndex = i;
            break;
        }
    }

    if (!currentPlayer) {
        return; // No player has a turn active
    }

    // Detect combat territories FIRST before taxes and purchases
    // Scan all territories for mixed player pieces
    QMap<QString, Position> combatTerritories;  // Map of territory name to position

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 12; ++col) {
            Position pos = {row, col};
            QString territoryName = getTerritoryNameAt(row, col);

            // Check if current player has pieces at this territory
            QList<GamePiece*> currentPlayerPieces = currentPlayer->getPiecesAtTerritory(territoryName);
            if (currentPlayerPieces.isEmpty()) {
                continue;  // No pieces from current player here
            }

            // Check if any other player has pieces at this territory
            bool hasEnemyPieces = false;
            for (Player *player : m_players) {
                if (player->getId() != currentPlayer->getId()) {
                    QList<GamePiece*> enemyPieces = player->getPiecesAtTerritory(territoryName);
                    if (!enemyPieces.isEmpty()) {
                        hasEnemyPieces = true;
                        break;
                    }
                }
            }

            // If we have both current player and enemy pieces, this is a combat territory
            if (hasEnemyPieces) {
                QString territoryName = m_mapWidget->getTerritoryNameAt(row, col);
                if (!combatTerritories.contains(territoryName)) {
                    combatTerritories[territoryName] = pos;
                }
            }
        }
    }

    // If there are combat territories, show the list to the player
    if (!combatTerritories.isEmpty()) {
        QStringList combatList;
        combatList << QString("Player %1 has %2 combat(s) to resolve:").arg(currentPlayer->getId()).arg(combatTerritories.size());
        combatList << "";

        for (auto it = combatTerritories.constBegin(); it != combatTerritories.constEnd(); ++it) {
            QString territoryName = it.key();
            Position pos = it.value();

            // Count pieces at this location
            QList<GamePiece*> currentPlayerPieces = currentPlayer->getPiecesAtTerritory(territoryName);

            // Count enemy pieces
            int enemyCount = 0;
            QStringList enemyPlayerIds;
            for (Player *player : m_players) {
                if (player->getId() != currentPlayer->getId()) {
                    QList<GamePiece*> enemyPieces = player->getPiecesAtTerritory(territoryName);
                    if (!enemyPieces.isEmpty()) {
                        enemyCount += enemyPieces.size();
                        if (!enemyPlayerIds.contains(QString(player->getId()))) {
                            enemyPlayerIds.append(QString(player->getId()));
                        }
                    }
                }
            }

            QString combatInfo = QString("  • %1 [%2,%3]: Your %4 piece(s) vs %5 enemy piece(s) (Player %6)")
                .arg(territoryName)
                .arg(pos.row)
                .arg(pos.col)
                .arg(currentPlayerPieces.size())
                .arg(enemyCount)
                .arg(enemyPlayerIds.join(","));

            combatList << combatInfo;
        }

        combatList << "";
        combatList << "You must resolve all combats before ending your turn.";

        QMessageBox combatMsgBox(this);
        combatMsgBox.setWindowTitle("Combat Detected");
        combatMsgBox.setText(combatList.join("\n"));
        combatMsgBox.setIconPixmap(QPixmap(":/images/combatIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        combatMsgBox.setStandardButtons(QMessageBox::Ok);
        combatMsgBox.exec();

        // Resolve each combat
        for (auto it = combatTerritories.constBegin(); it != combatTerritories.constEnd(); ++it) {
            QString territoryName = it.key();
            Position pos = it.value();

            // Find the enemy player at this territory (take the first one if multiple)
            Player *enemyPlayer = nullptr;
            for (Player *player : m_players) {
                if (player->getId() != currentPlayer->getId()) {
                    QList<GamePiece*> enemyPieces = player->getPiecesAtTerritory(territoryName);
                    if (!enemyPieces.isEmpty()) {
                        enemyPlayer = player;
                        break;
                    }
                }
            }

            if (enemyPlayer) {
                // Current player is the attacker (their turn), enemy player is the defender
                CombatDialog *combatDialog = new CombatDialog(currentPlayer, enemyPlayer, territoryName, m_mapWidget, this);
                combatDialog->exec();
                combatDialog->deleteLater();  // Use deleteLater() to avoid heap corruption
            }
        }

        // Update map display after all combats are resolved
        if (m_mapWidget) {
            m_mapWidget->update();
        }

        // After all combats are resolved, continue to taxes and purchases
        // (Fall through to the code below)
    }

    // Collect taxes from owned territories before ending turn
    int taxesCollected = currentPlayer->collectTaxes(m_mapWidget);
    qDebug() << "Player" << currentPlayer->getId() << "collected" << taxesCollected << "talents in taxes";

    // FIRST: Show city destruction dialog with ALL cities as checkboxes
    QList<City*> allCities = currentPlayer->getCities();
    if (!allCities.isEmpty()) {
        // Create custom dialog for selecting cities to destroy
        QDialog cityDestructionDialog(this);
        cityDestructionDialog.setWindowTitle("City Destruction Selection");

        QHBoxLayout *topLayout = new QHBoxLayout(&cityDestructionDialog);

        // Add fire city icon on the left side
        QLabel *iconLabel = new QLabel();
        QPixmap fireCityPixmap(":/images/fireCityIcon.png");
        iconLabel->setPixmap(fireCityPixmap.scaled(128, 128, Qt::KeepAspectRatio, Qt::FastTransformation));
        iconLabel->setAlignment(Qt::AlignTop);
        topLayout->addWidget(iconLabel);

        // Add spacing between icon and content
        topLayout->addSpacing(20);

        // Main content on the right
        QVBoxLayout *mainLayout = new QVBoxLayout();

        QLabel *headerLabel = new QLabel(
            QString("Player %1: Select cities to destroy (optional)").arg(currentPlayer->getId()));
        headerLabel->setStyleSheet("font-weight: bold; font-size: 12pt;");
        mainLayout->addWidget(headerLabel);

        QLabel *infoLabel = new QLabel(
            "Cities marked during your turn are pre-selected.\n"
            "You may change your selection before confirming.");
        mainLayout->addWidget(infoLabel);

        mainLayout->addSpacing(10);

        // Create checkboxes for each city
        QList<QCheckBox*> cityCheckboxes;
        for (City *city : allCities) {
            QString cityType = city->isFortified() ? "Walled City" : "City";
            QString cityLabel = QString("%1 at %2").arg(cityType).arg(city->getTerritoryName());

            QCheckBox *checkbox = new QCheckBox(cityLabel);
            checkbox->setChecked(city->isMarkedForDestruction());  // Pre-check marked cities
            checkbox->setProperty("cityPtr", QVariant::fromValue(static_cast<void*>(city)));
            cityCheckboxes.append(checkbox);
            mainLayout->addWidget(checkbox);
        }

        mainLayout->addSpacing(10);

        // Add OK button (no cancel - must proceed)
        QPushButton *okButton = new QPushButton("Continue");
        connect(okButton, &QPushButton::clicked, &cityDestructionDialog, &QDialog::accept);
        mainLayout->addWidget(okButton);

        topLayout->addLayout(mainLayout);

        // Show dialog and collect results
        if (cityDestructionDialog.exec() == QDialog::Accepted) {
            // First, update all cities' markedForDestruction flags based on checkbox state
            for (QCheckBox *checkbox : cityCheckboxes) {
                City *city = static_cast<City*>(checkbox->property("cityPtr").value<void*>());
                city->setMarkedForDestruction(checkbox->isChecked());
            }

            // Collect selected cities
            QList<City*> citiesToDestroy;
            QStringList cityNames;
            for (QCheckBox *checkbox : cityCheckboxes) {
                if (checkbox->isChecked()) {
                    City *city = static_cast<City*>(checkbox->property("cityPtr").value<void*>());
                    citiesToDestroy.append(city);
                    QString cityType = city->isFortified() ? "Walled City" : "City";
                    cityNames.append(QString("%1 at %2").arg(cityType).arg(city->getTerritoryName()));
                }
            }

            // Update the display to reflect any changes in marked cities
            updatePlayerInfo(currentPlayer);
            if (m_mapWidget) {
                m_mapWidget->update();
            }

            // If cities were selected, show confirmation dialog
            if (!citiesToDestroy.isEmpty()) {
                QMessageBox::StandardButton reply = QMessageBox::question(this,
                    "Confirm City Destruction",
                    QString("Are you sure you want to destroy the following cities?\n\n%1\n\n"
                            "This action cannot be undone!")
                    .arg(cityNames.join("\n")),
                    QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::No) {
                    // User said no to confirmation - don't destroy cities, but continue with turn end
                    qDebug() << "Player" << currentPlayer->getId() << "declined city destruction confirmation";
                } else {
                    // User confirmed - proceed with destruction
                    qDebug() << "Player" << currentPlayer->getId() << "destroying" << citiesToDestroy.size() << "cities";

                    for (City *city : citiesToDestroy) {
                        qDebug() << "  Destroying city at" << city->getTerritoryName()
                                 << "(" << city->getPosition().row << "," << city->getPosition().col << ")";

                        QString territoryName = city->getTerritoryName();
                        Position cityPosition = city->getPosition();

                        // Find and remove all roads connected to this city's territory
                        QList<Road*> roadsAtTerritory = currentPlayer->getRoadsAtTerritory(territoryName);
                        for (Road *road : roadsAtTerritory) {
                            qDebug() << "    Destroying road at" << road->getTerritoryName();
                            currentPlayer->removeRoad(road);
                            delete road;
                        }

                        // Also check for roads that have this position as either endpoint
                        QList<Road*> allRoads = currentPlayer->getRoads();
                        for (Road *road : allRoads) {
                            if (road->getFromPosition() == cityPosition || road->getToPosition() == cityPosition) {
                                qDebug() << "    Destroying connected road from"
                                         << road->getFromPosition().row << "," << road->getFromPosition().col
                                         << " to " << road->getToPosition().row << "," << road->getToPosition().col;
                                currentPlayer->removeRoad(road);
                                delete road;
                            }
                        }

                        // Remove city and fortification from MapWidget grids
                        if (m_mapWidget) {
                            m_mapWidget->removeCityAt(cityPosition.row, cityPosition.col);
                            m_mapWidget->removeFortificationAt(cityPosition.row, cityPosition.col);
                        }

                        // Remove city from player's inventory
                        currentPlayer->removeCity(city);

                        // Delete the city object
                        delete city;
                    }

                    // Update display after destroying cities
                    updateAllPlayers();
                    if (m_mapWidget) {
                        m_mapWidget->update();
                    }
                }
            } else {
                qDebug() << "Player" << currentPlayer->getId() << "chose not to destroy any cities";
            }
        }
    }

    // SECOND: Build options for PurchaseDialog
    QString homeProvinceName = currentPlayer->getHomeProvinceName();
    Position homePosition = m_mapWidget->territoryNameToPosition(homeProvinceName);

    // Build list of territories available for city placement
    QList<CityPlacementOption> cityOptions;
    const QList<QString> &ownedTerritories = currentPlayer->getOwnedTerritories();
    for (const QString &territoryName : ownedTerritories) {
        // Check if this territory already has a city
        QList<City*> citiesInTerritory = currentPlayer->getCitiesAtTerritory(territoryName);
        if (citiesInTerritory.isEmpty()) {
            // Find position for this territory
            for (int row = 0; row < 8; ++row) {
                for (int col = 0; col < 12; ++col) {
                    if (m_mapWidget->getTerritoryNameAt(row, col) == territoryName) {
                        CityPlacementOption option;
                        option.territoryName = territoryName;
                        option.position = {row, col};
                        cityOptions.append(option);
                        goto next_territory;  // Break out of nested loops
                    }
                }
            }
            next_territory:;
        }
    }

    // Build list of existing cities that can be fortified
    QList<FortificationOption> fortificationOptions;
    const QList<City*> &cities = currentPlayer->getCities();
    for (City *city : cities) {
        if (!city->isFortified()) {
            FortificationOption option;
            option.territoryName = city->getTerritoryName();
            option.position = city->getPosition();
            fortificationOptions.append(option);
        }
    }

    // Build list of sea borders for galley placement
    QList<GalleyPlacementOption> galleyOptions;
    QList<Position> adjacentSeaTerritories = m_mapWidget->getAdjacentSeaTerritories(homePosition);
    for (const Position &seaPos : adjacentSeaTerritories) {
        QString direction;
        if (seaPos.row < homePosition.row) direction = "North";
        else if (seaPos.row > homePosition.row) direction = "South";
        else if (seaPos.col < homePosition.col) direction = "West";
        else if (seaPos.col > homePosition.col) direction = "East";

        GalleyPlacementOption option;
        option.seaPosition = seaPos;
        option.seaTerritoryName = m_mapWidget->getTerritoryNameAt(seaPos.row, seaPos.col);
        option.direction = direction;
        galleyOptions.append(option);
    }

    // Get current galley count
    int currentGalleyCount = currentPlayer->getGalleys().size();

    // Calculate available pieces in the game box
    // Count how many pieces are currently in use across all players
    int totalInfantry = 0;
    int totalCavalry = 0;
    int totalCatapults = 0;
    int totalGalleys = 0;

    for (Player *player : m_players) {
        totalInfantry += player->getInfantry().size();
        totalCavalry += player->getCavalry().size();
        totalCatapults += player->getCatapults().size();
        totalGalleys += player->getGalleys().size();
    }

    // Define total pieces available in the physical game (1984 Milton Bradley edition)
    const int TOTAL_INFANTRY_PIECES = 60;    // Silver/generic infantry units
    const int TOTAL_CAVALRY_PIECES = 30;     // Gold cavalry units
    const int TOTAL_CATAPULT_PIECES = 20;    // Catapult pieces
    const int TOTAL_GALLEY_PIECES = 36;      // Galley/ship pieces

    // Calculate available pieces
    int availableInfantry = qMax(0, TOTAL_INFANTRY_PIECES - totalInfantry);
    int availableCavalry = qMax(0, TOTAL_CAVALRY_PIECES - totalCavalry);
    int availableCatapults = qMax(0, TOTAL_CATAPULT_PIECES - totalCatapults);
    int availableGalleys = qMax(0, TOTAL_GALLEY_PIECES - totalGalleys);

    // Open purchase dialog
    PurchaseDialog *purchaseDialog = new PurchaseDialog(
        currentPlayer->getId(),
        currentPlayer->getWallet(),
        1,  // inflation multiplier (1 = no inflation)
        cityOptions,
        fortificationOptions,
        galleyOptions,
        currentGalleyCount,
        availableInfantry,
        availableCavalry,
        availableCatapults,
        availableGalleys,
        this
    );

    if (purchaseDialog->exec() == QDialog::Accepted) {
        // Get purchase result
        PurchaseResult result = purchaseDialog->getPurchaseResult();

        // Deduct money from player's wallet
        if (result.totalCost > 0) {
            currentPlayer->spendMoney(result.totalCost);
            qDebug() << "Player" << currentPlayer->getId() << "spent" << result.totalCost << "talents";
        }

        // Create purchased cities
        for (const PurchaseResult::CityPurchase &cityPurchase : result.cities) {
            City *newCity = new City(
                currentPlayer->getId(),
                cityPurchase.position,
                cityPurchase.territoryName,
                cityPurchase.fortified,
                currentPlayer
            );
            currentPlayer->addCity(newCity);

            if (cityPurchase.fortified) {
                qDebug() << "Player" << currentPlayer->getId() << "placed fortified city at" << cityPurchase.territoryName;
            } else {
                qDebug() << "Player" << currentPlayer->getId() << "placed city at" << cityPurchase.territoryName;
            }
        }

        // Add fortifications to existing cities
        for (const QString &territoryName : result.fortifications) {
            // Find the city and add fortification
            const QList<City*> &playerCities = currentPlayer->getCities();
            for (City *city : playerCities) {
                if (city->getTerritoryName() == territoryName && !city->isFortified()) {
                    city->addFortification();
                    qDebug() << "Player" << currentPlayer->getId() << "fortified city at" << territoryName;
                    break;
                }
            }
        }

        // Create military units at home province
        QString homeProvince = currentPlayer->getHomeProvinceName();
        Position homePosForTroops = m_mapWidget->territoryNameToPosition(homeProvince);

        // Create infantry
        for (int i = 0; i < result.infantry; ++i) {
            InfantryPiece *infantry = new InfantryPiece(currentPlayer->getId(), homePosForTroops, currentPlayer);
            infantry->setTerritoryName(homeProvince);
            currentPlayer->addInfantry(infantry);
        }
        if (result.infantry > 0) {
            qDebug() << "Player" << currentPlayer->getId() << "created" << result.infantry << "infantry at" << homeProvince;
        }

        // Create cavalry
        for (int i = 0; i < result.cavalry; ++i) {
            CavalryPiece *cavalry = new CavalryPiece(currentPlayer->getId(), homePosForTroops, currentPlayer);
            cavalry->setTerritoryName(homeProvince);
            currentPlayer->addCavalry(cavalry);
        }
        if (result.cavalry > 0) {
            qDebug() << "Player" << currentPlayer->getId() << "created" << result.cavalry << "cavalry at" << homeProvince;
        }

        // Create catapults
        for (int i = 0; i < result.catapults; ++i) {
            CatapultPiece *catapult = new CatapultPiece(currentPlayer->getId(), homePosForTroops, currentPlayer);
            catapult->setTerritoryName(homeProvince);
            currentPlayer->addCatapult(catapult);
        }
        if (result.catapults > 0) {
            qDebug() << "Player" << currentPlayer->getId() << "created" << result.catapults << "catapults at" << homeProvince;
        }

        // Create galleys at specified sea borders
        for (const PurchaseResult::GalleyPurchase &galleyPurchase : result.galleys) {
            // Create the galleys at home position (they're on the border with the sea)
            for (int i = 0; i < galleyPurchase.count; ++i) {
                GalleyPiece *galley = new GalleyPiece(currentPlayer->getId(), homePosForTroops, currentPlayer);
                galley->setTerritoryName(homeProvince);
                currentPlayer->addGalley(galley);
            }

            QString seaTerritoryName = m_mapWidget->getTerritoryNameAt(galleyPurchase.seaBorder.row, galleyPurchase.seaBorder.col);
            qDebug() << "Player" << currentPlayer->getId() << "created" << galleyPurchase.count
                     << "galleys at" << homeProvince << "bordering sea territory" << seaTerritoryName;
        }
    }

    delete purchaseDialog;

    // End current player's turn
    currentPlayer->endTurn();

    // Start next player's turn (wrap around to first player after last)
    int nextPlayerIndex = (currentPlayerIndex + 1) % m_players.size();
    m_players[nextPlayerIndex]->startTurn();

    // Update all player displays
    updateAllPlayers();

    // Switch to next player's tab
    m_tabWidget->setCurrentIndex(nextPlayerIndex);

    // Trigger map redraw and update turn tracking
    if (m_mapWidget) {
        m_mapWidget->setCurrentPlayerIndex(nextPlayerIndex);
        m_mapWidget->setAtStartOfTurn(true);  // New turn is starting
        m_mapWidget->update();
    }

    // Update captured generals table
    updateCapturedGeneralsTable();
}

QGroupBox* PlayerInfoWidget::createAllCapturedGeneralsSection()
{
    QGroupBox *groupBox = new QGroupBox("All Captured Generals");
    QVBoxLayout *layout = new QVBoxLayout();

    m_capturedGeneralsTable = new QTableWidget();
    m_capturedGeneralsTable->setColumnCount(4);
    m_capturedGeneralsTable->setHorizontalHeaderLabels({"Original Player", "Serial Number", "Held By", "Territory"});
    m_capturedGeneralsTable->horizontalHeader()->setStretchLastSection(true);
    m_capturedGeneralsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_capturedGeneralsTable->setAlternatingRowColors(true);
    m_capturedGeneralsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_capturedGeneralsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_capturedGeneralsTable->setSelectionMode(QAbstractItemView::SingleSelection);

    // Set vertical size policy to minimize space when empty
    m_capturedGeneralsTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_capturedGeneralsTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    // Disable word wrap to keep rows compact
    m_capturedGeneralsTable->setWordWrap(false);

    // Connect context menu
    connect(m_capturedGeneralsTable, &QTableWidget::customContextMenuRequested, [this](const QPoint &pos) {
        int row = m_capturedGeneralsTable->rowAt(pos.y());
        if (row < 0) return;

        // Get the general from the row
        GeneralPiece *general = m_capturedGeneralsTable->item(row, 0)->data(Qt::UserRole).value<GeneralPiece*>();
        if (general) {
            showCapturedGeneralContextMenu(general, m_capturedGeneralsTable->mapToGlobal(pos));
        }
    });

    layout->addWidget(m_capturedGeneralsTable);
    layout->setContentsMargins(5, 5, 5, 5);  // Reduce margins
    groupBox->setLayout(layout);

    // Set size policy to minimize vertical space
    groupBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    return groupBox;
}

void PlayerInfoWidget::updateCapturedGeneralsTable()
{
    if (!m_capturedGeneralsTable) return;

    // Clear existing rows
    m_capturedGeneralsTable->setRowCount(0);

    // Collect all captured generals from all players
    QList<GeneralPiece*> capturedGenerals;
    for (Player *player : m_players) {
        for (GeneralPiece *general : player->getCapturedGenerals()) {
            if (general && general->isCaptured()) {
                capturedGenerals.append(general);
            }
        }
    }

    // Update row count
    m_capturedGeneralsTable->setRowCount(capturedGenerals.size());

    // Populate table
    int row = 0;
    for (GeneralPiece *general : capturedGenerals) {
        // Original player
        QTableWidgetItem *originalPlayerItem = new QTableWidgetItem(QString("Player %1").arg(general->getPlayer()));
        originalPlayerItem->setData(Qt::UserRole, QVariant::fromValue(general));  // Store pointer for context menu
        m_capturedGeneralsTable->setItem(row, 0, originalPlayerItem);

        // Serial number
        m_capturedGeneralsTable->setItem(row, 1, new QTableWidgetItem(general->getSerialNumber()));

        // Held by
        m_capturedGeneralsTable->setItem(row, 2, new QTableWidgetItem(QString("Player %1").arg(general->getCapturedBy())));

        // Territory
        m_capturedGeneralsTable->setItem(row, 3, new QTableWidgetItem(general->getTerritoryName()));

        row++;
    }

    // Update group box title with count
    if (m_capturedGeneralsGroupBox) {
        m_capturedGeneralsGroupBox->setTitle(QString("All Captured Generals (%1)").arg(capturedGenerals.size()));
    }

    // Hide the section if there are no captured generals
    if (m_capturedGeneralsGroupBox) {
        m_capturedGeneralsGroupBox->setVisible(capturedGenerals.size() > 0);
    }

    // Resize table to fit contents (minimize vertical space)
    if (capturedGenerals.size() > 0) {
        m_capturedGeneralsTable->resizeRowsToContents();

        // Calculate optimal height: header + all rows + small margin
        int tableHeight = m_capturedGeneralsTable->horizontalHeader()->height();
        for (int i = 0; i < capturedGenerals.size(); ++i) {
            tableHeight += m_capturedGeneralsTable->rowHeight(i);
        }
        tableHeight += 10;  // Small margin

        // Cap at reasonable maximum (e.g., 6 rows visible)
        int rowHeight = m_capturedGeneralsTable->rowHeight(0);
        if (rowHeight <= 0) rowHeight = 30;  // Default if not rendered yet
        int maxHeight = m_capturedGeneralsTable->horizontalHeader()->height() + (rowHeight * 6) + 10;

        m_capturedGeneralsTable->setMaximumHeight(qMin(tableHeight, maxHeight));
    } else {
        m_capturedGeneralsTable->setMaximumHeight(0);
    }
}

void PlayerInfoWidget::showCapturedGeneralContextMenu(GeneralPiece *general, const QPoint &pos)
{
    if (!general || !general->isCaptured()) return;

    QMenu menu(this);

    // Find current player (whose turn it is)
    Player *currentPlayer = nullptr;
    for (Player *player : m_players) {
        if (player->isMyTurn()) {
            currentPlayer = player;
            break;
        }
    }

    if (!currentPlayer) return;

    QChar heldBy = general->getCapturedBy();
    QChar originalPlayer = general->getPlayer();

    // If current player is holding this general
    if (currentPlayer->getId() == heldBy) {
        // Option 1: Offer general for ransom
        QMenu *offerSubmenu = menu.addMenu("Offer for Ransom");

        // Add menu items for each player (except self)
        for (Player *player : m_players) {
            if (player->getId() != currentPlayer->getId()) {
                QString playerLabel = QString("Player %1").arg(player->getId());
                if (player->getId() == originalPlayer) {
                    playerLabel += " (Original Owner)";
                }

                QAction *offerAction = offerSubmenu->addAction(playerLabel);
                connect(offerAction, &QAction::triggered, [this, general, player, currentPlayer]() {
                    // Ask the target player how much they're willing to offer
                    bool ok;
                    int maxOffer = player->getWallet();
                    int ransomAmount = QInputDialog::getInt(this, "Ransom Offer",
                        QString("Player %1 is offering to return General %2 #%3.\n\n"
                                "How much are you (Player %4) willing to pay?\n"
                                "Your wallet: %5 talents\n\n"
                                "Note: Amounts must be in increments of 5")
                        .arg(currentPlayer->getId())
                        .arg(general->getPlayer())
                        .arg(general->getNumber())
                        .arg(player->getId())
                        .arg(maxOffer),
                        0, 0, maxOffer, 5, &ok);  // Step size of 5

                    if (!ok) {
                        // User clicked cancel - no offer
                        return;
                    }

                    // Show confirmation dialog with Offer/Don't Offer buttons
                    QMessageBox confirmDialog(this);
                    confirmDialog.setWindowTitle("Confirm Ransom Offer");
                    confirmDialog.setText(QString("You (Player %1) are offering %2 talents for General %3 #%4.\n\n"
                                                   "Do you want to make this offer?")
                        .arg(player->getId())
                        .arg(ransomAmount)
                        .arg(general->getPlayer())
                        .arg(general->getNumber()));
                    QPushButton *offerButton = confirmDialog.addButton("Offer", QMessageBox::YesRole);
                    QPushButton *dontOfferButton = confirmDialog.addButton("Don't Offer", QMessageBox::NoRole);
                    confirmDialog.exec();

                    if (confirmDialog.clickedButton() != offerButton) {
                        // User chose "Don't Offer"
                        return;
                    }

                    // Ask the seller if they accept the offer
                    QMessageBox sellerMsg(this);
                    sellerMsg.setWindowTitle("Accept Ransom?");
                    sellerMsg.setText(QString("Player %1 is offering %2 talents for General %3 #%4.\n\n"
                                "Do you (Player %5) accept this offer?")
                        .arg(player->getId())
                        .arg(ransomAmount)
                        .arg(general->getPlayer())
                        .arg(general->getNumber())
                        .arg(currentPlayer->getId()));
                    sellerMsg.setIconPixmap(QPixmap(":/images/captureIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    sellerMsg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                    QMessageBox::StandardButton sellerResponse = (QMessageBox::StandardButton)sellerMsg.exec();

                    if (sellerResponse == QMessageBox::No) {
                        QMessageBox::information(this, "Ransom Declined",
                            QString("Player %1 declined the ransom offer.").arg(currentPlayer->getId()));
                        return;
                    }

                    // Transfer money
                    player->spendMoney(ransomAmount);
                    currentPlayer->addMoney(ransomAmount);

                    // Remove from current holder's captured list
                    currentPlayer->removeCapturedGeneral(general);

                    // Check if buyer is the original owner
                    if (player->getId() == general->getPlayer()) {
                        // Original owner buying back - return general to their inventory
                        general->clearCaptured();  // No longer captured

                        // Move general to buyer's home province
                        QString homeTerritoryName = player->getHomeProvinceName();
                        Position homePos = m_mapWidget->territoryNameToPosition(homeTerritoryName);
                        general->setPosition(homePos);
                        general->setTerritoryName(homeTerritoryName);

                        QMessageBox ransomMsg(this);
                        ransomMsg.setWindowTitle("General Ransomed");
                        ransomMsg.setText(QString("General %1 #%2 has been ransomed back to Player %3 for %4 talents.\n\n"
                                    "The general has been returned to their home province.")
                            .arg(general->getPlayer())
                            .arg(general->getNumber())
                            .arg(player->getId())
                            .arg(ransomAmount));
                        ransomMsg.setIconPixmap(QPixmap(":/images/captureIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                        ransomMsg.setStandardButtons(QMessageBox::Ok);
                        ransomMsg.exec();
                    } else {
                        // Non-original owner buying - general stays captured but changes captor
                        general->setCapturedBy(player->getId());
                        player->addCapturedGeneral(general);

                        // Move general to buyer's home province
                        QString homeTerritoryName = player->getHomeProvinceName();
                        Position homePos = m_mapWidget->territoryNameToPosition(homeTerritoryName);
                        general->setPosition(homePos);
                        general->setTerritoryName(homeTerritoryName);

                        QMessageBox soldMsg(this);
                        soldMsg.setWindowTitle("General Ransomed");
                        soldMsg.setText(QString("General %1 #%2 has been sold to Player %3 for %4 talents.\n\n"
                                    "The general is now held by Player %5.")
                            .arg(general->getPlayer())
                            .arg(general->getNumber())
                            .arg(player->getId())
                            .arg(ransomAmount)
                            .arg(player->getId()));
                        soldMsg.setIconPixmap(QPixmap(":/images/captureIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                        soldMsg.setStandardButtons(QMessageBox::Ok);
                        soldMsg.exec();
                    }

                    // Update displays
                    updateAllPlayers();
                    updateCapturedGeneralsTable();
                    if (m_mapWidget) {
                        m_mapWidget->update();
                    }
                });
            }
        }

        // Option 2: Kill the general
        QAction *killAction = menu.addAction("Kill General");
        connect(killAction, &QAction::triggered, [this, general, currentPlayer]() {
            QMessageBox::StandardButton reply = QMessageBox::question(this, "Kill General?",
                QString("Are you sure you want to kill General %1 #%2?\n\nThis action cannot be undone.")
                .arg(general->getPlayer())
                .arg(general->getNumber()),
                QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                // Remove from captured list
                currentPlayer->removeCapturedGeneral(general);

                // Find original player and remove from their list
                for (Player *player : m_players) {
                    if (player->getId() == general->getPlayer()) {
                        player->removeGeneral(general);
                        break;
                    }
                }

                // Delete the general
                general->deleteLater();

                // Update displays
                updateAllPlayers();
                updateCapturedGeneralsTable();

                QMessageBox::information(this, "General Killed",
                    QString("General %1 #%2 has been executed.")
                    .arg(general->getPlayer())
                    .arg(general->getNumber()));
            }
        });
    }
    // If current player is the original owner of this general
    else if (currentPlayer->getId() == originalPlayer) {
        QAction *ransomAction = menu.addAction("Request Ransom for Return");
        connect(ransomAction, &QAction::triggered, [this, general, currentPlayer, heldBy]() {
            // Find the player holding the general
            Player *holderPlayer = nullptr;
            for (Player *player : m_players) {
                if (player->getId() == heldBy) {
                    holderPlayer = player;
                    break;
                }
            }

            if (!holderPlayer) return;

            // Ask the original owner how much they're willing to pay
            bool ok;
            int maxOffer = currentPlayer->getWallet();
            int ransomAmount = QInputDialog::getInt(this, "Ransom Request",
                QString("You (Player %1) want your General %2 back from Player %3.\n\n"
                        "How much are you willing to pay?\n"
                        "Your wallet: %4 talents\n\n"
                        "Note: Amounts must be in increments of 5")
                .arg(currentPlayer->getId())
                .arg(general->getNumber())
                .arg(heldBy)
                .arg(maxOffer),
                0, 0, maxOffer, 5, &ok);  // Step size of 5

            if (!ok) {
                // User clicked cancel
                return;
            }

            // Show confirmation dialog with Offer/Don't Offer buttons
            QMessageBox confirmDialog(this);
            confirmDialog.setWindowTitle("Confirm Ransom Request");
            confirmDialog.setText(QString("You (Player %1) are offering %2 talents to buy back General %3.\n\n"
                                          "Do you want to make this offer?")
                .arg(currentPlayer->getId())
                .arg(ransomAmount)
                .arg(general->getNumber()));
            QPushButton *offerButton = confirmDialog.addButton("Offer", QMessageBox::YesRole);
            QPushButton *dontOfferButton = confirmDialog.addButton("Don't Offer", QMessageBox::NoRole);
            confirmDialog.exec();

            if (confirmDialog.clickedButton() != offerButton) {
                // User chose "Don't Offer"
                return;
            }

            // Ask the holder if they accept the offer
            QMessageBox holderMsg(this);
            holderMsg.setWindowTitle("Accept Ransom?");
            holderMsg.setText(QString("Player %1 is offering %2 talents to buy back their General %3.\n\n"
                        "Do you (Player %4) accept this offer?")
                .arg(currentPlayer->getId())
                .arg(ransomAmount)
                .arg(general->getNumber())
                .arg(heldBy));
            holderMsg.setIconPixmap(QPixmap(":/images/captureIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            holderMsg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            QMessageBox::StandardButton holderResponse = (QMessageBox::StandardButton)holderMsg.exec();

            if (holderResponse == QMessageBox::No) {
                QMessageBox::information(this, "Ransom Declined",
                    QString("Player %1 declined your ransom offer.").arg(heldBy));
                return;
            }

            // Transfer money
            currentPlayer->spendMoney(ransomAmount);
            holderPlayer->addMoney(ransomAmount);

            // Remove from holder's captured list
            holderPlayer->removeCapturedGeneral(general);

            // Return general to original owner
            general->clearCaptured();  // No longer captured

            // Move general to owner's home province
            QString homeTerritoryName = currentPlayer->getHomeProvinceName();
            Position homePos = m_mapWidget->territoryNameToPosition(homeTerritoryName);
            general->setPosition(homePos);
            general->setTerritoryName(homeTerritoryName);

            QMessageBox returnMsg(this);
            returnMsg.setWindowTitle("General Ransomed");
            returnMsg.setText(QString("General %1 #%2 has been ransomed back to you for %3 talents.\n\n"
                        "The general has been returned to your home province.")
                .arg(general->getPlayer())
                .arg(general->getNumber())
                .arg(ransomAmount));
            returnMsg.setIconPixmap(QPixmap(":/images/captureIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            returnMsg.setStandardButtons(QMessageBox::Ok);
            returnMsg.exec();

            // Update displays
            updateAllPlayers();
            updateCapturedGeneralsTable();
            if (m_mapWidget) {
                m_mapWidget->update();
            }
        });
    }

    if (!menu.isEmpty()) {
        menu.exec(pos);
    }
}
