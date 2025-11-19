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

    // Add tab with player ID as label
    QString tabLabel = QString("Player %1").arg(player->getId());
    m_tabWidget->addTab(playerTab, tabLabel);
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
    Position homePos = player->getHomeProvince();
    QString homeText = QString("%1 [Row: %2, Col: %3]")
                       .arg(player->getHomeProvinceName())
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
            territoryLabel->setStyleSheet("padding: 5px; background-color: transparent;");

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
    caesarTable->setColumnCount(5);
    caesarTable->setHorizontalHeaderLabels({"Serial Number", "Territory", "Position", "Movement", "On Galley"});
    caesarTable->horizontalHeader()->setStretchLastSection(true);
    caesarTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    caesarTable->setAlternatingRowColors(true);
    caesarTable->setContextMenuPolicy(Qt::CustomContextMenu);
    caesarTable->setRowCount(player->getCaesarCount());

    // Set fixed height for Caesar table (1 row + header)
    caesarTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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
        caesarTable->setItem(row, 2, new QTableWidgetItem(QString("[%1, %2]").arg(piece->getPosition().row).arg(piece->getPosition().col)));
        caesarTable->setItem(row, 3, new QTableWidgetItem(QString::number(piece->getMovesRemaining())));
        caesarTable->setItem(row, 4, new QTableWidgetItem(piece->getOnGalley()));
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
    generalTable->setColumnCount(5);
    generalTable->setHorizontalHeaderLabels({"Serial Number", "Territory", "Position", "Movement", "On Galley"});
    generalTable->horizontalHeader()->setStretchLastSection(true);
    generalTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    generalTable->setAlternatingRowColors(true);
    generalTable->setContextMenuPolicy(Qt::CustomContextMenu);
    generalTable->setRowCount(player->getGeneralCount());

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
        generalTable->setItem(row, 2, new QTableWidgetItem(QString("[%1, %2]").arg(piece->getPosition().row).arg(piece->getPosition().col)));
        generalTable->setItem(row, 3, new QTableWidgetItem(QString::number(piece->getMovesRemaining())));
        generalTable->setItem(row, 4, new QTableWidgetItem(piece->getOnGalley()));

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
    infantryTable->setColumnCount(5);
    infantryTable->setHorizontalHeaderLabels({"Serial Number", "Territory", "Position", "Movement", "On Galley"});
    infantryTable->horizontalHeader()->setStretchLastSection(true);
    infantryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    infantryTable->setAlternatingRowColors(true);
    infantryTable->setRowCount(player->getInfantryCount());
    row = 0;
    for (InfantryPiece *piece : player->getInfantry()) {
        infantryTable->setItem(row, 0, new QTableWidgetItem(piece->getSerialNumber()));
        infantryTable->setItem(row, 1, new QTableWidgetItem(piece->getTerritoryName()));
        infantryTable->setItem(row, 2, new QTableWidgetItem(QString("[%1, %2]").arg(piece->getPosition().row).arg(piece->getPosition().col)));
        infantryTable->setItem(row, 3, new QTableWidgetItem(QString::number(piece->getMovesRemaining())));
        infantryTable->setItem(row, 4, new QTableWidgetItem(piece->getOnGalley()));
        row++;
    }
    // Resize infantry table to fit content (max 10 rows visible)
    if (player->getInfantryCount() > 0) {
        infantryTable->resizeRowsToContents();
        int visibleRows = qMin(player->getInfantryCount(), 10);
        int estimatedRowHeight = 25;
        int tableHeight = 30 + (visibleRows * estimatedRowHeight);
        infantryTable->setMaximumHeight(tableHeight);
    }

    QVBoxLayout *infantryLayout = new QVBoxLayout();
    infantryLayout->addWidget(infantryTable);
    infantryBox->setLayout(infantryLayout);
    if (player->getInfantryCount() > 0) {
        mainLayout->addWidget(infantryBox);
    }

    // Cavalry
    QGroupBox *cavalryBox = new QGroupBox(QString("Cavalry (%1)").arg(player->getCavalryCount()));
    QTableWidget *cavalryTable = new QTableWidget();
    cavalryTable->setColumnCount(5);
    cavalryTable->setHorizontalHeaderLabels({"Serial Number", "Territory", "Position", "Movement", "On Galley"});
    cavalryTable->horizontalHeader()->setStretchLastSection(true);
    cavalryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    cavalryTable->setAlternatingRowColors(true);
    cavalryTable->setRowCount(player->getCavalryCount());
    row = 0;
    for (CavalryPiece *piece : player->getCavalry()) {
        cavalryTable->setItem(row, 0, new QTableWidgetItem(piece->getSerialNumber()));
        cavalryTable->setItem(row, 1, new QTableWidgetItem(piece->getTerritoryName()));
        cavalryTable->setItem(row, 2, new QTableWidgetItem(QString("[%1, %2]").arg(piece->getPosition().row).arg(piece->getPosition().col)));
        cavalryTable->setItem(row, 3, new QTableWidgetItem(QString::number(piece->getMovesRemaining())));
        cavalryTable->setItem(row, 4, new QTableWidgetItem(piece->getOnGalley()));
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
    catapultTable->setColumnCount(5);
    catapultTable->setHorizontalHeaderLabels({"Serial Number", "Territory", "Position", "Movement", "On Galley"});
    catapultTable->horizontalHeader()->setStretchLastSection(true);
    catapultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    catapultTable->setAlternatingRowColors(true);
    catapultTable->setRowCount(player->getCatapultCount());
    row = 0;
    for (CatapultPiece *piece : player->getCatapults()) {
        catapultTable->setItem(row, 0, new QTableWidgetItem(piece->getSerialNumber()));
        catapultTable->setItem(row, 1, new QTableWidgetItem(piece->getTerritoryName()));
        catapultTable->setItem(row, 2, new QTableWidgetItem(QString("[%1, %2]").arg(piece->getPosition().row).arg(piece->getPosition().col)));
        catapultTable->setItem(row, 3, new QTableWidgetItem(QString::number(piece->getMovesRemaining())));
        catapultTable->setItem(row, 4, new QTableWidgetItem(piece->getOnGalley()));
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
    galleyTable->setColumnCount(5);
    galleyTable->setHorizontalHeaderLabels({"Serial Number", "Territory", "Position", "Movement", "On Galley"});
    galleyTable->horizontalHeader()->setStretchLastSection(true);
    galleyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    galleyTable->setAlternatingRowColors(true);
    galleyTable->setRowCount(player->getGalleyCount());
    row = 0;
    for (GalleyPiece *piece : player->getGalleys()) {
        galleyTable->setItem(row, 0, new QTableWidgetItem(piece->getSerialNumber()));
        galleyTable->setItem(row, 1, new QTableWidgetItem(piece->getTerritoryName()));
        galleyTable->setItem(row, 2, new QTableWidgetItem(QString("[%1, %2]").arg(piece->getPosition().row).arg(piece->getPosition().col)));
        galleyTable->setItem(row, 3, new QTableWidgetItem(QString::number(piece->getMovesRemaining())));
        galleyTable->setItem(row, 4, new QTableWidgetItem(piece->getOnGalley()));
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
        table->setColumnCount(4);
        table->setHorizontalHeaderLabels({"Original Player", "Serial Number", "Territory", "Position"});
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setAlternatingRowColors(true);
        table->setRowCount(player->getCapturedGeneralCount());

        int row = 0;
        for (GeneralPiece *general : player->getCapturedGenerals()) {
            // Show original player ID
            table->setItem(row, 0, new QTableWidgetItem(QString("Player %1").arg(general->getPlayer())));
            table->setItem(row, 1, new QTableWidgetItem(general->getSerialNumber()));
            table->setItem(row, 2, new QTableWidgetItem(general->getTerritoryName()));
            table->setItem(row, 3, new QTableWidgetItem(QString("[%1, %2]").arg(general->getPosition().row).arg(general->getPosition().col)));
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
        m_tabWidget->insertTab(tabIndex, newTab, tabLabel);

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
    if (!piece) return;

    QMenu menu(this);
    QIcon moveIcon = style()->standardIcon(QStyle::SP_ArrowForward);
    QAction *moveAction = menu.addAction(moveIcon, "Move");

    // Disable if no moves remaining
    moveAction->setEnabled(piece->getMovesRemaining() > 0);

    // Create submenu for directions
    QMenu *moveSubmenu = new QMenu("Move", this);
    Position currentPos = piece->getPosition();

    // Up (row - 1)
    QString upTerritory = (currentPos.row > 0) ? getTerritoryNameAt(currentPos.row - 1, currentPos.col) : "Off Board";
    int upValue = (currentPos.row > 0) ? m_mapWidget->getTerritoryValueAt(currentPos.row - 1, currentPos.col) : 0;
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
        QAction *adjacentAction = upRoadSubmenu->addAction(upText);
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

            QAction *roadAction = upRoadSubmenu->addAction(roadText);
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
    QIcon downIcon = style()->standardIcon(QStyle::SP_ArrowDown);

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
    QIcon leftIcon = style()->standardIcon(QStyle::SP_ArrowBack);

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
    QIcon rightIcon = style()->standardIcon(QStyle::SP_ArrowForward);

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

    moveAction->setMenu(moveSubmenu);

    menu.exec(pos);
}

void PlayerInfoWidget::showGeneralContextMenu(GeneralPiece *piece, const QPoint &pos)
{
    if (!piece) return;

    QMenu menu(this);
    QIcon moveIcon = style()->standardIcon(QStyle::SP_ArrowForward);
    QAction *moveAction = menu.addAction(moveIcon, "Move");

    // Disable if no moves remaining
    moveAction->setEnabled(piece->getMovesRemaining() > 0);

    // Create submenu for directions
    QMenu *moveSubmenu = new QMenu("Move", this);
    Position currentPos = piece->getPosition();

    // Up (row - 1)
    QString upTerritory = (currentPos.row > 0) ? getTerritoryNameAt(currentPos.row - 1, currentPos.col) : "Off Board";
    int upValue = (currentPos.row > 0) ? m_mapWidget->getTerritoryValueAt(currentPos.row - 1, currentPos.col) : 0;
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
        QAction *adjacentAction = upRoadSubmenu->addAction(upText);
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

            QAction *roadAction = upRoadSubmenu->addAction(roadText);
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
    QIcon downIcon = style()->standardIcon(QStyle::SP_ArrowDown);

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
    QIcon leftIcon = style()->standardIcon(QStyle::SP_ArrowBack);

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
    QIcon rightIcon = style()->standardIcon(QStyle::SP_ArrowForward);

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

    moveAction->setMenu(moveSubmenu);

    menu.exec(pos);
}

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
            QList<GamePiece*> enemyPieces = otherPlayer->getPiecesAtPosition(newPos);
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
            QList<GamePiece*> enemyPieces = otherPlayer->getPiecesAtPosition(newPos);
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

void PlayerInfoWidget::moveLeaderWithTroops(GamePiece *leader, int rowDelta, int colDelta)
{
    if (!leader || !m_mapWidget) return;

    // Get the leader's current position
    Position currentPos = leader->getPosition();

    // Find the player who owns this leader
    Player *owningPlayer = nullptr;
    for (Player *player : m_players) {
        if (player->getId() == leader->getPlayer()) {
            owningPlayer = player;
            break;
        }
    }

    if (!owningPlayer) return;

    // Get all troops at the same position as the leader
    QList<GamePiece*> allPiecesAtPosition = owningPlayer->getPiecesAtPosition(currentPos);

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

    // Check if there are enemy pieces at the destination
    bool hasEnemies = false;
    QList<GamePiece*> enemyPiecesAtDest;
    for (Player *player : m_players) {
        if (player->getId() != owningPlayer->getId()) {
            QList<GamePiece*> enemyPieces = player->getPiecesAtPosition(destPos);
            if (!enemyPieces.isEmpty()) {
                hasEnemies = true;
                enemyPiecesAtDest.append(enemyPieces);
            }
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

    // Get all troops at the same position as the leader
    QList<GamePiece*> allPiecesAtPosition = owningPlayer->getPiecesAtPosition(currentPos);

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

    // Check if there are enemy pieces at the destination
    bool hasEnemies = false;
    QList<GamePiece*> enemyPiecesAtDest;
    Position destPos = {destination.row, destination.col};
    for (Player *player : m_players) {
        if (player->getId() != owningPlayer->getId()) {
            QList<GamePiece*> enemyPieces = player->getPiecesAtPosition(destPos);
            if (!enemyPieces.isEmpty()) {
                hasEnemies = true;
                enemyPiecesAtDest.append(enemyPieces);
            }
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

    Position pos = {row, col};
    QStringList troopInfo;

    // Check all players for troops at this position
    for (Player *player : m_players) {
        int caesarCount = 0;
        int generalCount = 0;
        int infantryCount = 0;
        int cavalryCount = 0;
        int catapultCount = 0;
        int galleyCount = 0;

        // Get all pieces at this position
        QList<GamePiece*> piecesHere = player->getPiecesAtPosition(pos);

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

            // Check if current player has pieces at this position
            QList<GamePiece*> currentPlayerPieces = currentPlayer->getPiecesAtPosition(pos);
            if (currentPlayerPieces.isEmpty()) {
                continue;  // No pieces from current player here
            }

            // Check if any other player has pieces at this position
            bool hasEnemyPieces = false;
            for (Player *player : m_players) {
                if (player->getId() != currentPlayer->getId()) {
                    QList<GamePiece*> enemyPieces = player->getPiecesAtPosition(pos);
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
            QList<GamePiece*> currentPlayerPieces = currentPlayer->getPiecesAtPosition(pos);

            // Count enemy pieces
            int enemyCount = 0;
            QStringList enemyPlayerIds;
            for (Player *player : m_players) {
                if (player->getId() != currentPlayer->getId()) {
                    QList<GamePiece*> enemyPieces = player->getPiecesAtPosition(pos);
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

        QMessageBox::information(this, "Combat Detected", combatList.join("\n"));

        // Resolve each combat
        for (auto it = combatTerritories.constBegin(); it != combatTerritories.constEnd(); ++it) {
            Position pos = it.value();

            // Find the enemy player at this position (take the first one if multiple)
            Player *enemyPlayer = nullptr;
            for (Player *player : m_players) {
                if (player->getId() != currentPlayer->getId()) {
                    QList<GamePiece*> enemyPieces = player->getPiecesAtPosition(pos);
                    if (!enemyPieces.isEmpty()) {
                        enemyPlayer = player;
                        break;
                    }
                }
            }

            if (enemyPlayer) {
                // Current player is the attacker (their turn), enemy player is the defender
                CombatDialog *combatDialog = new CombatDialog(currentPlayer, enemyPlayer, pos, m_mapWidget, this);
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

    // FIRST: Allow player to destroy their own cities (before purchase phase)
    CityDestructionDialog *destructionDialog = new CityDestructionDialog(
        currentPlayer->getId(),
        currentPlayer->getCities(),
        this
    );

    if (destructionDialog->exec() == QDialog::Accepted) {
        QList<City*> citiesToDestroy = destructionDialog->getCitiesToDestroy();

        if (!citiesToDestroy.isEmpty()) {
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
    }

    delete destructionDialog;

    // Calculate maximum cities and fortifications player can purchase
    // Ensure maxCities is never negative (can happen if player has more cities than territories)
    int maxCities = qMax(0, currentPlayer->getOwnedTerritoryCount() - currentPlayer->getCityCount());

    // Count unfortified cities
    int unfortifiedCities = 0;
    for (City *city : currentPlayer->getCities()) {
        if (!city->isFortified()) {
            unfortifiedCities++;
        }
    }
    // Max fortifications = unfortified cities + cities being purchased (we'll update this dynamically)
    int maxFortifications = unfortifiedCities + maxCities;

    // Check if player's home territory is adjacent to sea (required for galley purchases)
    Position homePosition = currentPlayer->getHomeProvince();
    QList<Position> adjacentSeaTerritories = m_mapWidget->getAdjacentSeaTerritories(homePosition);
    bool canPurchaseGalleys = !adjacentSeaTerritories.isEmpty();

    // SECOND: Open purchase dialog to allow player to buy items
    // Note: Inflation multiplier is fixed at 1 for now (can be enhanced later)
    PurchaseDialog *purchaseDialog = new PurchaseDialog(
        currentPlayer->getId(),
        currentPlayer->getWallet(),
        1,  // inflation multiplier (1 = no inflation)
        maxCities,
        maxFortifications,
        canPurchaseGalleys,
        this
    );

    if (purchaseDialog->exec() == QDialog::Accepted) {
        int totalSpent = purchaseDialog->getTotalSpent();

        // Deduct money from player's wallet
        if (totalSpent > 0) {
            currentPlayer->spendMoney(totalSpent);
            qDebug() << "Player" << currentPlayer->getId() << "spent" << totalSpent << "talents";
        }

        // Get purchased quantities
        int citiesPurchased = purchaseDialog->getCityCount();
        int fortificationsPurchased = purchaseDialog->getFortificationCount();
        int infantryPurchased = purchaseDialog->getInfantryCount();
        int cavalryPurchased = purchaseDialog->getCavalryCount();
        int catapultsPurchased = purchaseDialog->getCatapultCount();
        int galleysPurchased = purchaseDialog->getGalleyCount();

        // Place cities one at a time
        for (int i = 0; i < citiesPurchased; ++i) {
            // Get list of owned territories without cities
            QStringList availableTerritories;
            const QList<QString> &ownedTerritories = currentPlayer->getOwnedTerritories();

            for (const QString &territoryName : ownedTerritories) {
                // Check if this territory already has a city
                QList<City*> citiesInTerritory = currentPlayer->getCitiesAtTerritory(territoryName);
                if (citiesInTerritory.isEmpty()) {
                    // Find the position of this territory to check if it's a sea territory
                    bool isSea = false;
                    for (int row = 0; row < 8; ++row) {
                        for (int col = 0; col < 12; ++col) {
                            if (m_mapWidget->getTerritoryNameAt(row, col) == territoryName) {
                                isSea = m_mapWidget->isSeaTerritory(row, col);
                                goto foundTerritory;  // Break out of nested loops
                            }
                        }
                    }
                    foundTerritory:

                    // Only add land territories to the available list
                    if (!isSea) {
                        availableTerritories.append(territoryName);
                    }
                }
            }

            if (availableTerritories.isEmpty()) {
                QMessageBox::warning(this, "Cannot Place City",
                    QString("Player %1 has no territories available for city placement.").arg(currentPlayer->getId()));
                break;
            }

            // Show dialog to select territory
            bool ok;
            QString selectedTerritory = QInputDialog::getItem(
                this,
                QString("Place City %1/%2").arg(i + 1).arg(citiesPurchased),
                QString("Player %1: Select territory for new city:").arg(currentPlayer->getId()),
                availableTerritories,
                0,
                false,
                &ok
            );

            if (ok && !selectedTerritory.isEmpty()) {
                // Find the position for this territory
                Position cityPosition = {-1, -1};
                bool foundPosition = false;

                for (int row = 0; row < 8; ++row) {
                    for (int col = 0; col < 12; ++col) {
                        if (m_mapWidget->getTerritoryNameAt(row, col) == selectedTerritory) {
                            cityPosition = {row, col};
                            foundPosition = true;
                            break;
                        }
                    }
                    if (foundPosition) break;
                }

                if (foundPosition) {
                    // Validate that this is not a sea territory
                    if (m_mapWidget->isSeaTerritory(cityPosition.row, cityPosition.col)) {
                        QMessageBox::warning(this, "Cannot Place City",
                            QString("Cannot place city in sea territory: %1").arg(selectedTerritory));
                        // Don't break, let player try again with another territory
                        i--; // Decrement to retry this city placement
                        continue;
                    }

                    // Create and add the city
                    City *newCity = new City(currentPlayer->getId(), cityPosition, selectedTerritory, false, currentPlayer);
                    currentPlayer->addCity(newCity);
                    qDebug() << "Player" << currentPlayer->getId() << "placed city at" << selectedTerritory;
                } else {
                    QMessageBox::warning(this, "Error",
                        QString("Could not find position for territory: %1").arg(selectedTerritory));
                }
            } else {
                // User cancelled
                break;
            }
        }

        // Place fortifications one at a time
        for (int i = 0; i < fortificationsPurchased; ++i) {
            // Get list of cities without fortifications
            QStringList availableCities;
            const QList<City*> &cities = currentPlayer->getCities();

            for (City *city : cities) {
                if (!city->isFortified()) {
                    availableCities.append(city->getTerritoryName());
                }
            }

            if (availableCities.isEmpty()) {
                QMessageBox::warning(this, "Cannot Place Fortification",
                    QString("Player %1 has no cities available for fortification.").arg(currentPlayer->getId()));
                break;
            }

            // Show dialog to select city
            bool ok;
            QString selectedCity = QInputDialog::getItem(
                this,
                QString("Place Fortification %1/%2").arg(i + 1).arg(fortificationsPurchased),
                QString("Player %1: Select city to fortify:").arg(currentPlayer->getId()),
                availableCities,
                0,
                false,
                &ok
            );

            if (ok && !selectedCity.isEmpty()) {
                // Find the city and add fortification
                for (City *city : cities) {
                    if (city->getTerritoryName() == selectedCity && !city->isFortified()) {
                        city->addFortification();
                        qDebug() << "Player" << currentPlayer->getId() << "fortified city at" << selectedCity;
                        break;
                    }
                }
            } else {
                // User cancelled
                break;
            }
        }

        // Create military units at home province
        QString homeProvince = currentPlayer->getHomeProvinceName();
        Position homePosition = currentPlayer->getHomeProvince();

        // Create infantry
        for (int i = 0; i < infantryPurchased; ++i) {
            InfantryPiece *infantry = new InfantryPiece(currentPlayer->getId(), homePosition, currentPlayer);
            currentPlayer->addInfantry(infantry);
        }
        if (infantryPurchased > 0) {
            qDebug() << "Player" << currentPlayer->getId() << "created" << infantryPurchased << "infantry at" << homeProvince;
        }

        // Create cavalry
        for (int i = 0; i < cavalryPurchased; ++i) {
            CavalryPiece *cavalry = new CavalryPiece(currentPlayer->getId(), homePosition, currentPlayer);
            currentPlayer->addCavalry(cavalry);
        }
        if (cavalryPurchased > 0) {
            qDebug() << "Player" << currentPlayer->getId() << "created" << cavalryPurchased << "cavalry at" << homeProvince;
        }

        // Create catapults
        for (int i = 0; i < catapultsPurchased; ++i) {
            CatapultPiece *catapult = new CatapultPiece(currentPlayer->getId(), homePosition, currentPlayer);
            currentPlayer->addCatapult(catapult);
        }
        if (catapultsPurchased > 0) {
            qDebug() << "Player" << currentPlayer->getId() << "created" << catapultsPurchased << "catapults at" << homeProvince;
        }

        // Create galleys - they must be placed on the border with a sea territory
        if (galleysPurchased > 0) {
            // We already checked adjacentSeaTerritories earlier, but recalculate for safety
            QList<Position> seaBorders = m_mapWidget->getAdjacentSeaTerritories(homePosition);

            if (seaBorders.isEmpty()) {
                QMessageBox::warning(this, "Cannot Place Galleys",
                    QString("Player %1's home territory is not adjacent to any sea territories. Galleys cannot be placed.").arg(currentPlayer->getId()));
            } else {
                Position selectedSeaBorder = homePosition;  // Default to home position

                // If multiple sea borders exist, ask the player which one to use
                if (seaBorders.size() > 1) {
                    QStringList seaBorderOptions;
                    for (const Position &seaPos : seaBorders) {
                        QString direction;
                        if (seaPos.row < homePosition.row) direction = "North";
                        else if (seaPos.row > homePosition.row) direction = "South";
                        else if (seaPos.col < homePosition.col) direction = "West";
                        else if (seaPos.col > homePosition.col) direction = "East";

                        QString seaTerritoryName = m_mapWidget->getTerritoryNameAt(seaPos.row, seaPos.col);
                        seaBorderOptions.append(QString("%1 (%2 at %3,%4)").arg(direction).arg(seaTerritoryName).arg(seaPos.row).arg(seaPos.col));
                    }

                    bool ok;
                    QString selectedOption = QInputDialog::getItem(
                        this,
                        QString("Place Galleys - Player %1").arg(currentPlayer->getId()),
                        QString("Your home territory borders multiple sea territories.\nSelect which sea border to place your %1 galley(s) at:").arg(galleysPurchased),
                        seaBorderOptions,
                        0,
                        false,
                        &ok
                    );

                    if (ok && !selectedOption.isEmpty()) {
                        int selectedIndex = seaBorderOptions.indexOf(selectedOption);
                        if (selectedIndex >= 0 && selectedIndex < seaBorders.size()) {
                            selectedSeaBorder = seaBorders[selectedIndex];
                        }
                    } else {
                        // User cancelled, don't create galleys
                        QMessageBox::information(this, "Galleys Not Placed",
                            QString("Galley placement cancelled. Player %1 did not receive %2 galley(s), but money was already spent.").arg(currentPlayer->getId()).arg(galleysPurchased));
                        galleysPurchased = 0;  // Set to 0 so we don't create them
                    }
                }

                // Create the galleys at home position (which represents the border with the selected sea territory)
                for (int i = 0; i < galleysPurchased; ++i) {
                    GalleyPiece *galley = new GalleyPiece(currentPlayer->getId(), homePosition, currentPlayer);
                    currentPlayer->addGalley(galley);
                }

                if (galleysPurchased > 0) {
                    QString seaTerritoryName = m_mapWidget->getTerritoryNameAt(selectedSeaBorder.row, selectedSeaBorder.col);
                    qDebug() << "Player" << currentPlayer->getId() << "created" << galleysPurchased
                             << "galleys at" << homeProvince << "bordering sea territory" << seaTerritoryName;
                }
            }
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
    m_capturedGeneralsTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

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
                    QMessageBox::StandardButton sellerResponse = QMessageBox::question(this, "Accept Ransom?",
                        QString("Player %1 is offering %2 talents for General %3 #%4.\n\n"
                                "Do you (Player %5) accept this offer?")
                        .arg(player->getId())
                        .arg(ransomAmount)
                        .arg(general->getPlayer())
                        .arg(general->getNumber())
                        .arg(currentPlayer->getId()),
                        QMessageBox::Yes | QMessageBox::No);

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
                        Position homePos = player->getHomeProvince();
                        general->setPosition(homePos);
                        QString homeTerritoryName = m_mapWidget->getTerritoryNameAt(homePos.row, homePos.col);
                        general->setTerritoryName(homeTerritoryName);

                        QMessageBox::information(this, "General Ransomed",
                            QString("General %1 #%2 has been ransomed back to Player %3 for %4 talents.\n\n"
                                    "The general has been returned to their home province.")
                            .arg(general->getPlayer())
                            .arg(general->getNumber())
                            .arg(player->getId())
                            .arg(ransomAmount));
                    } else {
                        // Non-original owner buying - general stays captured but changes captor
                        general->setCapturedBy(player->getId());
                        player->addCapturedGeneral(general);

                        // Move general to buyer's home province
                        Position homePos = player->getHomeProvince();
                        general->setPosition(homePos);
                        QString homeTerritoryName = m_mapWidget->getTerritoryNameAt(homePos.row, homePos.col);
                        general->setTerritoryName(homeTerritoryName);

                        QMessageBox::information(this, "General Ransomed",
                            QString("General %1 #%2 has been sold to Player %3 for %4 talents.\n\n"
                                    "The general is now held by Player %5.")
                            .arg(general->getPlayer())
                            .arg(general->getNumber())
                            .arg(player->getId())
                            .arg(ransomAmount)
                            .arg(player->getId()));
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
            QMessageBox::StandardButton holderResponse = QMessageBox::question(this, "Accept Ransom?",
                QString("Player %1 is offering %2 talents to buy back their General %3.\n\n"
                        "Do you (Player %4) accept this offer?")
                .arg(currentPlayer->getId())
                .arg(ransomAmount)
                .arg(general->getNumber())
                .arg(heldBy),
                QMessageBox::Yes | QMessageBox::No);

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
            Position homePos = currentPlayer->getHomeProvince();
            general->setPosition(homePos);
            QString homeTerritoryName = m_mapWidget->getTerritoryNameAt(homePos.row, homePos.col);
            general->setTerritoryName(homeTerritoryName);

            QMessageBox::information(this, "General Ransomed",
                QString("General %1 #%2 has been ransomed back to you for %3 talents.\n\n"
                        "The general has been returned to your home province.")
                .arg(general->getPlayer())
                .arg(general->getNumber())
                .arg(ransomAmount));

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
