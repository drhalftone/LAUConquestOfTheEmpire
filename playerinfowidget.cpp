#include "playerinfowidget.h"
// mapwidget.h is included conditionally in playerinfowidget.h
#include "purchasedialog.h"
#include "troopselectiondialog.h"
#include "combatdialog.h"
#include "citydestructiondialog.h"
#include "gamepiece.h"
#include "building.h"
#include "aiplayer.h"
#include "ai/reachabilitycalculator.h"
#include <QScrollArea>
#include <QRegularExpression>
#include <QTimer>
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
#include <QRandomGenerator>
#include <QSet>
#include <QLabel>
#include <QListWidget>
#include <QTextEdit>

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

    // Add buttons at the bottom (no stretch - minimal space)
    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    QPushButton *reachabilityButton = buttonBox->addButton("Reachability", QDialogButtonBox::ActionRole);
    QPushButton *riskButton = buttonBox->addButton("Risk", QDialogButtonBox::ActionRole);
    QPushButton *endTurnButton = buttonBox->addButton("End Turn", QDialogButtonBox::ActionRole);
    connect(reachabilityButton, &QPushButton::clicked, this, &PlayerInfoWidget::onReachabilityClicked);
    connect(riskButton, &QPushButton::clicked, this, &PlayerInfoWidget::onRiskClicked);
    connect(endTurnButton, &QPushButton::clicked, this, &PlayerInfoWidget::onEndTurnClicked);
    mainLayout->addWidget(buttonBox, 0);  // No stretch

    setLayout(mainLayout);

    // Setup click sound for context menus
    m_clickSound = new QSoundEffect(this);
    m_clickSound->setSource(QUrl("qrc:/images/click.wav"));
    m_clickSound->setVolume(0.3f);  // Faint volume for context menus
    m_clickTimer.start();  // Start timer for throttling

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
    layout->addWidget(new QLabel(homeName), 2, 1);

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
    groupBox->setObjectName(QString("economicsSection_%1").arg(player->getId()));
    QGridLayout *layout = new QGridLayout();

    // Current Wallet
    layout->addWidget(new QLabel("<b>Current Money:</b>"), 0, 0);
    QLabel *walletLabel = new QLabel(QString("%1 talents").arg(player->getWallet()));
    walletLabel->setObjectName(QString("walletLabel_%1").arg(player->getId()));
    layout->addWidget(walletLabel, 0, 1);

    // Total territories owned
    layout->addWidget(new QLabel("<b>Territories Owned:</b>"), 1, 0);
    QLabel *territoryCountLabel = new QLabel(QString::number(player->getOwnedTerritoryCount()));
    territoryCountLabel->setObjectName(QString("territoryCountLabel_%1").arg(player->getId()));
    layout->addWidget(territoryCountLabel, 1, 1);

    // Calculate total tax value from all owned territories
    int totalTaxValue = 0;
    if (m_mapWidget && m_mapWidget->getGraph()) {
        const QList<QString> &territories = player->getOwnedTerritories();
        for (const QString &territoryName : territories) {
            // Use graph-based lookup (works for both grid and OpenGL maps)
            totalTaxValue += m_mapWidget->getGraph()->getValue(territoryName);
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
    groupBox->setObjectName(QString("territoriesSection_%1").arg(player->getId()));
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
            // Find the tax value for this territory using graph-based lookup
            int taxValue = 0;
            if (m_mapWidget && m_mapWidget->getGraph()) {
                taxValue = m_mapWidget->getGraph()->getValue(territoryName);
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
    groupBox->setObjectName(QString("piecesSection_%1").arg(player->getId()));
    QVBoxLayout *mainLayout = new QVBoxLayout();

    // Caesars - fixed height since there's exactly one
    QGroupBox *caesarBox = new QGroupBox(QString("Caesars (%1)").arg(player->getCaesarCount()));
    QTableWidget *caesarTable = new QTableWidget();
    caesarTable->setObjectName(QString("caesarTable_%1").arg(player->getId()));
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
    generalTable->setObjectName(QString("generalTable_%1").arg(player->getId()));
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
    infantryTable->setHorizontalHeaderLabels({"Serial Number", "Territory", "Movement", "In Legion"});
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
        // Find which leader this troop belongs to
        QString inLegion;
        for (CaesarPiece *caesar : player->getCaesars()) {
            if (caesar->getLegion().contains(piece->getUniqueId())) {
                inLegion = QString("Caesar %1").arg(caesar->getSerialNumber());
                break;
            }
        }
        if (inLegion.isEmpty()) {
            for (GeneralPiece *general : player->getGenerals()) {
                if (general->getLegion().contains(piece->getUniqueId())) {
                    inLegion = QString("General #%1").arg(general->getNumber());
                    break;
                }
            }
        }
        infantryTable->setItem(row, 3, new QTableWidgetItem(inLegion));
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
    cavalryTable->setHorizontalHeaderLabels({"Serial Number", "Territory", "Movement", "In Legion"});
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
        // Find which leader this troop belongs to
        QString inLegion;
        for (CaesarPiece *caesar : player->getCaesars()) {
            if (caesar->getLegion().contains(piece->getUniqueId())) {
                inLegion = QString("Caesar %1").arg(caesar->getSerialNumber());
                break;
            }
        }
        if (inLegion.isEmpty()) {
            for (GeneralPiece *general : player->getGenerals()) {
                if (general->getLegion().contains(piece->getUniqueId())) {
                    inLegion = QString("General #%1").arg(general->getNumber());
                    break;
                }
            }
        }
        cavalryTable->setItem(row, 3, new QTableWidgetItem(inLegion));
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
    catapultTable->setHorizontalHeaderLabels({"Serial Number", "Territory", "Movement", "In Legion"});
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
        // Find which leader this troop belongs to
        QString inLegion;
        for (CaesarPiece *caesar : player->getCaesars()) {
            if (caesar->getLegion().contains(piece->getUniqueId())) {
                inLegion = QString("Caesar %1").arg(caesar->getSerialNumber());
                break;
            }
        }
        if (inLegion.isEmpty()) {
            for (GeneralPiece *general : player->getGenerals()) {
                if (general->getLegion().contains(piece->getUniqueId())) {
                    inLegion = QString("General #%1").arg(general->getNumber());
                    break;
                }
            }
        }
        catapultTable->setItem(row, 3, new QTableWidgetItem(inLegion));
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
    // Save current tab index to restore after all updates
    int currentTabIndex = m_tabWidget->currentIndex();

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

    // Restore the tab index (prevents tab jumping during updates)
    m_tabWidget->setCurrentIndex(currentTabIndex);
}

void PlayerInfoWidget::showCaesarContextMenu(CaesarPiece *piece, const QPoint &pos)
{
    if (!piece || !m_mapWidget) return;

    // Create main menu
    QMenu menu(this);

    // Create Caesar menu item with submenu for movement
    QString caesarLabel = QString("Caesar (Player %1) at %2").arg(piece->getPlayer()).arg(piece->getTerritoryName());
    QMenu *caesarSubmenu = menu.addMenu(QIcon(":/images/ceasarIcon.png"), caesarLabel);

    // Get all valid moves using the shared method
    QList<MoveOption> moves = getMovesForLeader(piece);

    // Track actions to territory names for highlighting
    QMap<QAction*, QString> actionToTerritory;

    // Add each destination as a movement option in the submenu
    for (const MoveOption &option : moves) {
        // Build display text
        QString ownership = "";
        if (!option.isSea) {
            ownership = (option.owner == '\0') ? "[Unclaimed]"
                              : (option.owner == piece->getPlayer()) ? "[You]"
                              : QString("[Player %1]").arg(option.owner);
        }
        QString roadIndicator = option.isViaRoad ? " [via road]" : "";
        QString displayText = (option.territoryValue > 0)
            ? QString("%1 (%2) %3%4%5").arg(option.destinationTerritory).arg(option.territoryValue).arg(ownership).arg(option.troopInfo).arg(roadIndicator)
            : QString("%1%2%3%4").arg(option.destinationTerritory).arg(ownership).arg(option.troopInfo).arg(roadIndicator);

        // Determine icon based on move type
        QIcon moveIcon;
        if (option.hasCombat) {
            moveIcon = QIcon(":/images/combatIcon.png");
        } else if (option.hasCity) {
            moveIcon = QIcon(":/images/newCityIcon.png");
        } else if (option.owner != '\0' && !option.isSea) {
            QString flagPath;
            switch (option.owner.toLatin1()) {
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

        QAction *moveToAction = caesarSubmenu->addAction(moveIcon, displayText);
        moveToAction->setEnabled(!option.isSea && piece->getMovesRemaining() > 0);

        // Track this action for highlighting
        actionToTerritory[moveToAction] = option.destinationTerritory;

        connect(moveToAction, &QAction::triggered, [this, piece, option]() {
            moveLeaderToTerritory(piece, option.destinationTerritory);
        });
    }

    // Get the current territory for highlighting
    QString currentTerritory = piece->getTerritoryName();
    Territory currentTerritoryInfo = m_mapWidget->getGraph()->getTerritory(currentTerritory);
    int currentTerritoryId = currentTerritoryInfo.id;

    // Setup hover highlighting with timer
    QTimer hoverTimer;
    hoverTimer.setInterval(50);  // Check every 50ms

    connect(&hoverTimer, &QTimer::timeout, [this, &actionToTerritory, currentTerritoryId, caesarSubmenu]() {
        QAction *activeAction = nullptr;
        QWidget *activeWidget = QApplication::activePopupWidget();

        if (activeWidget) {
            QMenu *activeMenu = qobject_cast<QMenu*>(activeWidget);
            if (activeMenu) {
                activeAction = activeMenu->activeAction();
            }
        }

        // Check if hovering over the caesar submenu action itself
        if (activeAction == caesarSubmenu->menuAction()) {
            // Highlight current territory where Caesar is located
            m_mapWidget->setHoveredTerritoryById(currentTerritoryId);
        } else if (activeAction && actionToTerritory.contains(activeAction)) {
            // Highlight destination territory
            QString hoveredTerritoryName = actionToTerritory[activeAction];
            if (m_mapWidget->getGraph()) {
                Territory hoveredTerritory = m_mapWidget->getGraph()->getTerritory(hoveredTerritoryName);
                if (hoveredTerritory.id > 0) {
                    m_mapWidget->setHoveredTerritoryById(hoveredTerritory.id);
                }
            }
        }
    });

    hoverTimer.start();

    // Reset last hovered action when menu opens
    m_lastHoveredAction = nullptr;

    // Connect hover sound to menu
    connect(&menu, &QMenu::hovered, this, &PlayerInfoWidget::playMenuClickSound);
    connect(caesarSubmenu, &QMenu::hovered, this, &PlayerInfoWidget::playMenuClickSound);

    menu.exec(pos);
}


void PlayerInfoWidget::showGeneralContextMenu(GeneralPiece *piece, const QPoint &pos)
{
    if (!piece || !m_mapWidget) return;

    // Create main menu
    QMenu menu(this);

    // Create General menu item with submenu for movement
    QString generalLabel = QString("General %1 (Player %2) at %3").arg(piece->getNumber()).arg(piece->getPlayer()).arg(piece->getTerritoryName());
    QMenu *generalSubmenu = menu.addMenu(QIcon(":/images/generalIcon.png"), generalLabel);

    // Get all valid moves using the shared method
    QList<MoveOption> moves = getMovesForLeader(piece);

    // Track actions to territory names for highlighting
    QMap<QAction*, QString> actionToTerritory;

    // Add each destination as a movement option in the submenu
    for (const MoveOption &option : moves) {
        // Build display text
        QString ownership = "";
        if (!option.isSea) {
            ownership = (option.owner == '\0') ? "[Unclaimed]"
                              : (option.owner == piece->getPlayer()) ? "[You]"
                              : QString("[Player %1]").arg(option.owner);
        }
        QString roadIndicator = option.isViaRoad ? " [via road]" : "";
        QString displayText = (option.territoryValue > 0)
            ? QString("%1 (%2) %3%4%5").arg(option.destinationTerritory).arg(option.territoryValue).arg(ownership).arg(option.troopInfo).arg(roadIndicator)
            : QString("%1%2%3%4").arg(option.destinationTerritory).arg(ownership).arg(option.troopInfo).arg(roadIndicator);

        // Determine icon based on move type
        QIcon moveIcon;
        if (option.hasCombat) {
            moveIcon = QIcon(":/images/combatIcon.png");
        } else if (option.hasCity) {
            moveIcon = QIcon(":/images/newCityIcon.png");
        } else if (option.owner != '\0' && !option.isSea) {
            QString flagPath;
            switch (option.owner.toLatin1()) {
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

        QAction *moveToAction = generalSubmenu->addAction(moveIcon, displayText);
        moveToAction->setEnabled(!option.isSea && piece->getMovesRemaining() > 0);

        // Track this action for highlighting
        actionToTerritory[moveToAction] = option.destinationTerritory;

        connect(moveToAction, &QAction::triggered, [this, piece, option]() {
            moveLeaderToTerritory(piece, option.destinationTerritory);
        });
    }

    // Get the current territory for highlighting
    QString currentTerritory = piece->getTerritoryName();
    Territory currentTerritoryInfo = m_mapWidget->getGraph()->getTerritory(currentTerritory);
    int currentTerritoryId = currentTerritoryInfo.id;

    // Setup hover highlighting with timer
    QTimer hoverTimer;
    hoverTimer.setInterval(50);  // Check every 50ms

    connect(&hoverTimer, &QTimer::timeout, [this, &actionToTerritory, currentTerritoryId, generalSubmenu]() {
        QAction *activeAction = nullptr;
        QWidget *activeWidget = QApplication::activePopupWidget();

        if (activeWidget) {
            QMenu *activeMenu = qobject_cast<QMenu*>(activeWidget);
            if (activeMenu) {
                activeAction = activeMenu->activeAction();
            }
        }

        // Check if hovering over the general submenu action itself
        if (activeAction == generalSubmenu->menuAction()) {
            // Highlight current territory where General is located
            m_mapWidget->setHoveredTerritoryById(currentTerritoryId);
        } else if (activeAction && actionToTerritory.contains(activeAction)) {
            // Highlight destination territory
            QString hoveredTerritoryName = actionToTerritory[activeAction];
            if (m_mapWidget->getGraph()) {
                Territory hoveredTerritory = m_mapWidget->getGraph()->getTerritory(hoveredTerritoryName);
                if (hoveredTerritory.id > 0) {
                    m_mapWidget->setHoveredTerritoryById(hoveredTerritory.id);
                }
            }
        }
    });

    hoverTimer.start();

    // Reset last hovered action when menu opens
    m_lastHoveredAction = nullptr;

    // Connect hover sound to menu
    connect(&menu, &QMenu::hovered, this, &PlayerInfoWidget::playMenuClickSound);
    connect(generalSubmenu, &QMenu::hovered, this, &PlayerInfoWidget::playMenuClickSound);

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

    // Reset last hovered action when menu opens
    m_lastHoveredAction = nullptr;

    // Connect hover sound to menu
    connect(&menu, &QMenu::hovered, this, &PlayerInfoWidget::playMenuClickSound);

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
        // Still show basic menu even without valid player
        QMenu menu(this);
        menu.setTitle(QString("Territory: %1").arg(territoryName));
        menu.addAction("Show Player Info", this, [this]() {
            show(); raise(); activateWindow();
        });
        QAction *endTurnAction = menu.addAction("End Turn");
        connect(endTurnAction, &QAction::triggered, this, &PlayerInfoWidget::endTurn, Qt::QueuedConnection);
        // Reset last hovered action when menu opens
        m_lastHoveredAction = nullptr;
        // Connect hover sound to menu
        connect(&menu, &QMenu::hovered, this, &PlayerInfoWidget::playMenuClickSound);
        menu.exec(globalPos);
        return;
    }

    // Find all leaders at this territory belonging to current player
    // Skip movement options for disputed territories (combat zone)
    QList<GamePiece*> leaders;
    if (!isDisputed) {
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
    } else {
        qDebug() << "Territory is disputed - skipping movement options";
    }

    qDebug() << "Found" << leaders.size() << "movable leaders at" << territoryName;

    // Create main menu
    QMenu menu(this);
    menu.setTitle(QString("Territory: %1").arg(territoryName));

    // Track actions to territory names for hover highlighting
    QMap<QAction*, QString> actionToTerritory;
    QList<QMenu*> leaderSubmenus;

    // Add each leader with their movement submenu
    for (GamePiece *leader : leaders) {
        QString leaderName;
        QIcon leaderIcon;
        double moves = leader->getMovesRemaining();
        QString movesStr = (moves == static_cast<int>(moves)) ? QString::number(static_cast<int>(moves)) : QString::number(moves, 'f', 1);
        if (leader->getType() == GamePiece::Type::Caesar) {
            leaderName = QString("Caesar %1 (%2 moves)").arg(leader->getPlayer()).arg(movesStr);
            leaderIcon = QIcon(":/images/ceasarIcon.png");
        } else if (leader->getType() == GamePiece::Type::General) {
            GeneralPiece *general = static_cast<GeneralPiece*>(leader);
            leaderName = QString("General %1 #%2 (%3 moves)").arg(leader->getPlayer()).arg(general->getNumber()).arg(movesStr);
            leaderIcon = QIcon(":/images/generalIcon.png");
        } else if (leader->getType() == GamePiece::Type::Galley) {
            leaderName = QString("Galley %1 (%2 moves)").arg(leader->getPlayer()).arg(movesStr);
            leaderIcon = QIcon(":/images/galleyIcon.png");
        }

        // Create submenu for this leader's movement options
        QMenu *leaderSubmenu = menu.addMenu(leaderIcon, leaderName);
        leaderSubmenus.append(leaderSubmenu);

        // Check if leader is on a galley (for disembarking)
        bool isOnGalley = leader->isOnGalley();
        GalleyPiece *leaderGalley = nullptr;
        if (isOnGalley && leader->getType() != GamePiece::Type::Galley) {
            // Find the galley this leader is on
            for (GalleyPiece *galley : player->getGalleys()) {
                if (galley->getSerialNumber() == leader->getOnGalley()) {
                    leaderGalley = galley;
                    break;
                }
            }
        }

        // Check for beached galleys at the same territory that the leader can board
        // (Only for Caesar/General not already on a galley)
        if (!isOnGalley && leader->getType() != GamePiece::Type::Galley) {
            QList<GalleyPiece*> beachedGalleys;
            for (GalleyPiece *galley : player->getGalleys()) {
                if (galley->getTerritoryName() == territoryName &&
                    galley->isBeached() &&
                    !galley->hasLeaderAboard() &&
                    galley->getMovesRemaining() >= 1.0 &&  // Need at least 1 move to launch
                    galley->hasLastSeaZone()) {  // Must have a sea zone to launch to
                    beachedGalleys.append(galley);
                }
            }

            // Add "Board Galley" options for each beached galley
            for (GalleyPiece *galley : beachedGalleys) {
                QString seaZone = galley->getLastSeaZone();
                QString galleyText = QString("Board Galley %1 → %2 (%3 moves)")
                    .arg(galley->getSerialNumber())
                    .arg(seaZone)
                    .arg(galley->getMovesRemaining());
                QIcon galleyIcon(":/images/galleyIcon.png");
                QAction *boardAction = leaderSubmenu->addAction(galleyIcon, galleyText);

                connect(boardAction, &QAction::triggered, [this, leader, galley, seaZone]() {
                    boardGalleyFromBeach(leader, galley, seaZone);
                });
            }

            // Add separator if we added any galley options
            if (!beachedGalleys.isEmpty()) {
                leaderSubmenu->addSeparator();
            }
        }

        // Get neighbors using MapGraph
        QList<QString> neighbors = m_mapWidget->getGraph()->getNeighbors(territoryName);
        qDebug() << "Movement menu: territory=" << territoryName << "neighbors=" << neighbors;

        // Get territories connected by roads from this territory (computed on-the-fly)
        QStringList roadConnectedTerritories = m_mapWidget->getGraph()->getRoadConnectedTerritories(territoryName, player);
        qDebug() << "Movement menu: roadConnectedTerritories=" << roadConnectedTerritories;

        // Filter out territories that are already neighbors (roads are only useful for non-adjacent)
        QList<QString> roadOnlyTerritories;
        for (const QString &roadTerritory : roadConnectedTerritories) {
            if (!neighbors.contains(roadTerritory)) {
                roadOnlyTerritories.append(roadTerritory);
            }
        }
        qDebug() << "Movement menu: roadOnlyTerritories (non-adjacent)=" << roadOnlyTerritories;

        // Combine neighbors and road-connected territories
        QList<QString> allDestinations = neighbors + roadOnlyTerritories;
        qDebug() << "Movement menu: allDestinations=" << allDestinations;

        // Add each destination as a movement option
        for (const QString &destinationName : allDestinations) {
            // Use graph-based queries instead of grid-based
            int value = m_mapWidget->getGraph()->getValue(destinationName);
            bool isSea = m_mapWidget->getGraph()->isSeaTerritory(destinationName);

            // For beached galleys, they can only launch into the sea zone they came from
            if (leader->getType() == GamePiece::Type::Galley && isSea) {
                GalleyPiece *galley = static_cast<GalleyPiece*>(leader);
                if (galley->isBeached()) {
                    // Beached galley - can only go to lastSeaZone
                    if (!galley->hasLastSeaZone() || destinationName != galley->getLastSeaZone()) {
                        continue;  // Skip sea zones that aren't the last sea zone
                    }
                }
            }

            // Find owner by checking which player owns the territory
            QChar owner = '\0';
            for (Player *p : m_players) {
                if (p->ownsTerritory(destinationName)) {
                    owner = p->getId();
                    break;
                }
            }

            // Build display text - indicate if this is via road
            bool isViaRoad = roadOnlyTerritories.contains(destinationName);
            QString ownership = (owner == '\0') ? "[Unclaimed]" : (owner == leader->getPlayer()) ? "[You]" : QString("[Player %1]").arg(owner);

            // Get troop info by checking all players' pieces at this territory
            QStringList troopParts;
            for (Player *p : m_players) {
                QList<GamePiece*> pieces = p->getPiecesAtTerritory(destinationName);
                int inf = 0, cav = 0, cat = 0;
                for (GamePiece *piece : pieces) {
                    if (piece->getType() == GamePiece::Type::Infantry) inf++;
                    else if (piece->getType() == GamePiece::Type::Cavalry) cav++;
                    else if (piece->getType() == GamePiece::Type::Catapult) cat++;
                }
                if (inf > 0) troopParts << QString("%1I").arg(inf);
                if (cav > 0) troopParts << QString("%1C").arg(cav);
                if (cat > 0) troopParts << QString("%1T").arg(cat);
            }
            QString troops = troopParts.isEmpty() ? "" : QString(" [%1]").arg(troopParts.join(","));
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
                        break;
                    }
                }
            }
            if (!hasCombat && owner != '\0' && owner != player->getId()) {
                hasCombat = true;  // Enemy-owned territory
            }

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

            // Check if this is a sea zone with friendly galley (for boarding)
            bool hasGalleyToBoard = false;
            if (isSea && leader->getType() != GamePiece::Type::Galley) {
                for (GalleyPiece *galley : player->getGalleys()) {
                    if (galley->getTerritoryName() == destinationName &&
                        !galley->hasTransportedThisTurn() &&
                        !galley->hasLeaderAboard() &&
                        galley->getMovesRemaining() >= 0.5) {
                        hasGalleyToBoard = true;
                        break;
                    }
                }
            }

            // Set icon based on what we found (galley > combat > city > flag > nothing)
            if (hasGalleyToBoard) {
                moveIcon = QIcon(":/images/galleyIcon.png");
            } else if (hasCombat) {
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

            // Determine what moves are allowed based on piece type and galley status
            bool canMove = false;
            QList<GalleyPiece*> availableGalleys;  // For sea territories with multiple galleys

            if (leader->getType() == GamePiece::Type::Galley) {
                canMove = isSea;  // Galleys can only move to sea
            } else if (isOnGalley && leaderGalley) {
                // Leader is on a galley - can only disembark to land
                canMove = !isSea;  // Can only move to land territories
            } else {
                // Caesar/General on land - normally only land, but can board galleys
                if (!isSea) {
                    canMove = true;  // Normal land movement
                } else {
                    // Collect all friendly galleys at this sea zone that can transport
                    for (GalleyPiece *galley : player->getGalleys()) {
                        if (galley->getTerritoryName() == destinationName &&
                            !galley->hasTransportedThisTurn() &&
                            !galley->hasLeaderAboard() &&
                            galley->getMovesRemaining() >= 0.5) {
                            availableGalleys.append(galley);
                            canMove = true;
                        }
                    }
                }
            }

            // If multiple galleys available, create a submenu for galley selection
            if (availableGalleys.size() > 1) {
                QMenu *galleySubmenu = leaderSubmenu->addMenu(moveIcon, displayText);
                for (GalleyPiece *galley : availableGalleys) {
                    QString galleyText = QString("Board Galley %1 (%2 moves)")
                        .arg(galley->getSerialNumber())
                        .arg(galley->getMovesRemaining());
                    QAction *galleyAction = galleySubmenu->addAction(QIcon(":/images/galleyIcon.png"), galleyText);
                    actionToTerritory[galleyAction] = destinationName;  // Track for hover highlighting
                    connect(galleyAction, &QAction::triggered, [this, leader, destinationName, player, galley]() {
                        boardGalleySpecific(leader, destinationName, player, galley);
                    });
                }
            } else {
                // Single destination or single galley - use regular action
                QAction *moveToAction = leaderSubmenu->addAction(moveIcon, displayText);
                moveToAction->setEnabled(canMove);
                actionToTerritory[moveToAction] = destinationName;  // Track for hover highlighting

                // Connect to movement handler
                connect(moveToAction, &QAction::triggered, [this, leader, destinationName, isSea, player, isOnGalley, leaderGalley, availableGalleys]() {
                    if (isOnGalley && leaderGalley && !isSea) {
                        // Disembarking from galley to land
                        disembarkFromGalley(leader, destinationName, leaderGalley, player);
                    } else if (isSea && leader->getType() != GamePiece::Type::Galley) {
                        // Boarding a galley - use the single available galley directly
                        if (availableGalleys.size() == 1) {
                            boardGalleySpecific(leader, destinationName, player, availableGalleys.first());
                        } else {
                            boardGalley(leader, destinationName, player);
                        }
                    } else {
                        moveLeaderToTerritory(leader, destinationName);
                    }
                });
            }
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

    // Add separator before global actions
    menu.addSeparator();

    // Add "Show Player Info" action
    QAction *showPlayerInfoAction = menu.addAction("Show Player Info");
    connect(showPlayerInfoAction, &QAction::triggered, this, [this]() {
        show();
        raise();
        activateWindow();
    });

    // Add "End Turn" action (queued connection allows menu to close first)
    QAction *endTurnAction = menu.addAction("End Turn");
    connect(endTurnAction, &QAction::triggered, this, &PlayerInfoWidget::endTurn, Qt::QueuedConnection);

    // Reset last hovered action when menu opens
    m_lastHoveredAction = nullptr;

    // Connect hover sound to menu
    connect(&menu, &QMenu::hovered, this, &PlayerInfoWidget::playMenuClickSound);

    // Connect hover sound to leader submenus
    for (QMenu *submenu : leaderSubmenus) {
        connect(submenu, &QMenu::hovered, this, &PlayerInfoWidget::playMenuClickSound);
    }

    // Get the current territory for highlighting
    Territory currentTerritoryInfo = m_mapWidget->getGraph()->getTerritory(territoryName);
    int currentTerritoryId = currentTerritoryInfo.id;

    // Setup hover highlighting with timer
    QTimer hoverTimer;
    hoverTimer.setInterval(50);  // Check every 50ms

    connect(&hoverTimer, &QTimer::timeout, [this, &actionToTerritory, currentTerritoryId, &leaderSubmenus]() {
        QAction *activeAction = nullptr;
        QWidget *activeWidget = QApplication::activePopupWidget();

        if (activeWidget) {
            QMenu *activeMenu = qobject_cast<QMenu*>(activeWidget);
            if (activeMenu) {
                activeAction = activeMenu->activeAction();
            }
        }

        // Check if hovering over a leader submenu action itself (highlight current territory)
        bool isOnLeaderSubmenu = false;
        for (QMenu *submenu : leaderSubmenus) {
            if (activeAction == submenu->menuAction()) {
                m_mapWidget->setHoveredTerritoryById(currentTerritoryId);
                isOnLeaderSubmenu = true;
                break;
            }
        }

        if (!isOnLeaderSubmenu && activeAction && actionToTerritory.contains(activeAction)) {
            // Highlight destination territory
            QString hoveredTerritoryName = actionToTerritory[activeAction];
            if (m_mapWidget->getGraph()) {
                Territory hoveredTerritory = m_mapWidget->getGraph()->getTerritory(hoveredTerritoryName);
                if (hoveredTerritory.id > 0) {
                    m_mapWidget->setHoveredTerritoryById(hoveredTerritory.id);
                }
            }
        }
    });

    hoverTimer.start();

    // Always show the menu
    menu.exec(globalPos);
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

    // Get road-connected territories using MapGraph
    QString currentTerritoryName = m_mapWidget->getTerritoryNameAt(currentPos.row, currentPos.col);
    Player *owningPlayer = nullptr;
    for (Player *p : m_players) {
        if (p->getId() == piece->getPlayer()) {
            owningPlayer = p;
            break;
        }
    }
    QStringList roadConnectedNames = owningPlayer ? m_mapWidget->getGraph()->getRoadConnectedTerritories(currentTerritoryName, owningPlayer) : QStringList();

    // Filter road connections - keep only non-adjacent territories (true road travel)
    QStringList upRoadConnections;
    QStringList adjacentTerritories = m_mapWidget->getGraph() ? m_mapWidget->getGraph()->getNeighbors(currentTerritoryName) : QStringList();
    for (const QString &roadTerritory : roadConnectedNames) {
        // Skip current territory and adjacent territories - those aren't "via road" moves
        if (roadTerritory != currentTerritoryName && !adjacentTerritories.contains(roadTerritory)) {
            upRoadConnections.append(roadTerritory);
        }
    }

    if (!upRoadConnections.isEmpty()) {
        // There's a road network - create submenu
        QMenu *upRoadSubmenu = new QMenu(upText, this);
        upRoadSubmenu->setIcon(upIcon);

        // First, add the adjacent territory itself (upPos) as an option (regular move, not road travel)
        QAction *adjacentAction = upRoadSubmenu->addAction(upIcon, upText);
        adjacentAction->setEnabled(piece->getMovesRemaining() > 0);
        connect(adjacentAction, &QAction::triggered, [this, piece]() { moveLeaderWithTroops(piece, -1, 0); });

        // Then add all other territories connected by road (except current position and adjacent)
        for (const QString &roadTerritory : upRoadConnections) {
            int roadValue = m_mapWidget->getGraph() ? m_mapWidget->getGraph()->getTerritoryValue(roadTerritory) : 0;
            // Find owner by checking all players
            QChar roadOwner = '\0';
            for (Player *p : m_players) {
                if (p->ownsTerritory(roadTerritory)) {
                    roadOwner = p->getId();
                    break;
                }
            }
            QString roadOwnership = (roadOwner == '\0') ? "[Unclaimed]" : (roadOwner == piece->getPlayer()) ? "[You]" : QString("[Player %1]").arg(roadOwner);
            QString roadTroops = getTroopInfoAtTerritory(roadTerritory);
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
            connect(roadAction, &QAction::triggered, [this, piece, roadTerritory]() { moveLeaderViaRoad(piece, roadTerritory); });
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

    // Check if there's a road connection downward (using road network)
    // Use the same upRoadConnections list we already built (non-adjacent road destinations)

    if (!upRoadConnections.isEmpty()) {
        // There's a road network - create submenu
        QMenu *downRoadSubmenu = new QMenu(downText, this);
        downRoadSubmenu->setIcon(downIcon);

        // First, add the adjacent territory itself as an option (regular move, not road travel)
        QAction *adjacentAction = downRoadSubmenu->addAction(downText);
        adjacentAction->setEnabled(piece->getMovesRemaining() > 0);
        connect(adjacentAction, &QAction::triggered, [this, piece]() { moveLeaderWithTroops(piece, 1, 0); });

        // Then add all other territories connected by road
        for (const QString &roadTerritory : upRoadConnections) {
            int roadValue = m_mapWidget->getGraph() ? m_mapWidget->getGraph()->getTerritoryValue(roadTerritory) : 0;
            // Find owner by checking all players
            QChar roadOwner = '\0';
            for (Player *p : m_players) {
                if (p->ownsTerritory(roadTerritory)) {
                    roadOwner = p->getId();
                    break;
                }
            }
            QString roadOwnership = (roadOwner == '\0') ? "[Unclaimed]" : (roadOwner == piece->getPlayer()) ? "[You]" : QString("[Player %1]").arg(roadOwner);
            QString roadTroops = getTroopInfoAtTerritory(roadTerritory);
            QString roadText = (roadValue > 0) ? QString("%1 (%2) %3%4 [Via Road]").arg(roadTerritory).arg(roadValue).arg(roadOwnership).arg(roadTroops) : QString("%1 %2%3 [Via Road]").arg(roadTerritory).arg(roadOwnership).arg(roadTroops);

            QAction *roadAction = downRoadSubmenu->addAction(roadText);
            roadAction->setEnabled(piece->getMovesRemaining() > 0);
            connect(roadAction, &QAction::triggered, [this, piece, roadTerritory]() { moveLeaderViaRoad(piece, roadTerritory); });
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

    // Check if there's a road connection leftward (using road network)
    // Use the same upRoadConnections list we already built (non-adjacent road destinations)

    if (!upRoadConnections.isEmpty()) {
        // There's a road network - create submenu
        QMenu *leftRoadSubmenu = new QMenu(leftText, this);
        leftRoadSubmenu->setIcon(leftIcon);

        // First, add the adjacent territory itself as an option (regular move, not road travel)
        QAction *adjacentAction = leftRoadSubmenu->addAction(leftText);
        adjacentAction->setEnabled(piece->getMovesRemaining() > 0);
        connect(adjacentAction, &QAction::triggered, [this, piece]() { moveLeaderWithTroops(piece, 0, -1); });

        // Then add all other territories connected by road
        for (const QString &roadTerritory : upRoadConnections) {
            int roadValue = m_mapWidget->getGraph() ? m_mapWidget->getGraph()->getTerritoryValue(roadTerritory) : 0;
            // Find owner by checking all players
            QChar roadOwner = '\0';
            for (Player *p : m_players) {
                if (p->ownsTerritory(roadTerritory)) {
                    roadOwner = p->getId();
                    break;
                }
            }
            QString roadOwnership = (roadOwner == '\0') ? "[Unclaimed]" : (roadOwner == piece->getPlayer()) ? "[You]" : QString("[Player %1]").arg(roadOwner);
            QString roadTroops = getTroopInfoAtTerritory(roadTerritory);
            QString roadText = (roadValue > 0) ? QString("%1 (%2) %3%4 [Via Road]").arg(roadTerritory).arg(roadValue).arg(roadOwnership).arg(roadTroops) : QString("%1 %2%3 [Via Road]").arg(roadTerritory).arg(roadOwnership).arg(roadTroops);

            QAction *roadAction = leftRoadSubmenu->addAction(roadText);
            roadAction->setEnabled(piece->getMovesRemaining() > 0);
            connect(roadAction, &QAction::triggered, [this, piece, roadTerritory]() { moveLeaderViaRoad(piece, roadTerritory); });
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

    // Check if there's a road connection rightward (using road network)
    // Use the same upRoadConnections list we already built (non-adjacent road destinations)

    if (!upRoadConnections.isEmpty()) {
        // There's a road network - create submenu
        QMenu *rightRoadSubmenu = new QMenu(rightText, this);
        rightRoadSubmenu->setIcon(rightIcon);

        // First, add the adjacent territory itself as an option (regular move, not road travel)
        QAction *adjacentAction = rightRoadSubmenu->addAction(rightText);
        adjacentAction->setEnabled(piece->getMovesRemaining() > 0);
        connect(adjacentAction, &QAction::triggered, [this, piece]() { moveLeaderWithTroops(piece, 0, 1); });

        // Then add all other territories connected by road
        for (const QString &roadTerritory : upRoadConnections) {
            int roadValue = m_mapWidget->getGraph() ? m_mapWidget->getGraph()->getTerritoryValue(roadTerritory) : 0;
            // Find owner by checking all players
            QChar roadOwner = '\0';
            for (Player *p : m_players) {
                if (p->ownsTerritory(roadTerritory)) {
                    roadOwner = p->getId();
                    break;
                }
            }
            QString roadOwnership = (roadOwner == '\0') ? "[Unclaimed]" : (roadOwner == piece->getPlayer()) ? "[You]" : QString("[Player %1]").arg(roadOwner);
            QString roadTroops = getTroopInfoAtTerritory(roadTerritory);
            QString roadText = (roadValue > 0) ? QString("%1 (%2) %3%4 [Via Road]").arg(roadTerritory).arg(roadValue).arg(roadOwnership).arg(roadTroops) : QString("%1 %2%3 [Via Road]").arg(roadTerritory).arg(roadOwnership).arg(roadTroops);

            QAction *roadAction = rightRoadSubmenu->addAction(roadText);
            roadAction->setEnabled(piece->getMovesRemaining() > 0);
            connect(roadAction, &QAction::triggered, [this, piece, roadTerritory]() { moveLeaderViaRoad(piece, roadTerritory); });
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

    // Get current position from territory name (temporary until full graph migration)
    Position currentPos = m_mapWidget->territoryNameToPosition(piece->getTerritoryName());
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

    // Only transfer territory if no enemies present and not a sea territory
    bool isSea = m_mapWidget && m_mapWidget->isSeaTerritory(newPos.row, newPos.col);

    // Check if this piece can capture territory:
    // - Generals and Caesars cannot capture territory alone; they need at least one troop
    // - Troops (Infantry, Cavalry, Catapult) can capture territory
    bool canCapture = true;
    GamePiece::Type pieceType = piece->getType();
    if (pieceType == GamePiece::Type::General || pieceType == GamePiece::Type::Caesar) {
        // Leader moving alone - check if there are any friendly troops at destination
        QList<GamePiece*> friendlyPieces = owningPlayer->getPiecesAtTerritory(newTerritoryName);
        bool hasTroopsAtDestination = false;
        for (GamePiece *p : friendlyPieces) {
            GamePiece::Type t = p->getType();
            if (t == GamePiece::Type::Infantry || t == GamePiece::Type::Cavalry || t == GamePiece::Type::Catapult) {
                hasTroopsAtDestination = true;
                break;
            }
        }
        if (!hasTroopsAtDestination) {
            canCapture = false;
            qDebug() << "General/Caesar cannot capture territory" << newTerritoryName << "without troops";
        }
    }

    if (!hasEnemyPieces && !isSea && canCapture) {
        // Claim the new territory (and handle conquest from other owner if needed)
        conquestTerritory(newTerritoryName, owningPlayer);
    }

    // Update territory name (this is the primary location tracking now)
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

    // Get current position from territory name (temporary until full graph migration)
    Position currentPos = m_mapWidget->territoryNameToPosition(piece->getTerritoryName());
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

    // Only transfer territory if no enemies present and not a sea territory
    bool isSea = m_mapWidget && m_mapWidget->isSeaTerritory(newPos.row, newPos.col);

    // Check if this piece can capture territory:
    // - Generals and Caesars cannot capture territory alone; they need at least one troop
    // - Troops (Infantry, Cavalry, Catapult) can capture territory
    bool canCapture = true;
    GamePiece::Type pieceType = piece->getType();
    if (pieceType == GamePiece::Type::General || pieceType == GamePiece::Type::Caesar) {
        // Leader moving alone - check if there are any friendly troops at destination
        QList<GamePiece*> friendlyPieces = owningPlayer->getPiecesAtTerritory(newTerritoryName);
        bool hasTroopsAtDestination = false;
        for (GamePiece *p : friendlyPieces) {
            GamePiece::Type t = p->getType();
            if (t == GamePiece::Type::Infantry || t == GamePiece::Type::Cavalry || t == GamePiece::Type::Catapult) {
                hasTroopsAtDestination = true;
                break;
            }
        }
        if (!hasTroopsAtDestination) {
            canCapture = false;
            qDebug() << "General/Caesar cannot capture territory" << newTerritoryName << "without troops";
        }
    }

    if (!hasEnemyPieces && !isSea && canCapture) {
        // Claim the new territory (and handle conquest from other owner if needed)
        conquestTerritory(newTerritoryName, owningPlayer);
    }

    // Update territory name (this is the primary location tracking now)
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

    // Check if there are enemy pieces at the destination and collect info for display
    QString destTerritory = destinationTerritory;
    bool hasEnemies = false;
    QString enemyDescription;
    int enemyInfantryCount = 0;
    int enemyCavalryCount = 0;
    int enemyCatapultCount = 0;
    QStringList enemyLeaders;

    for (Player *player : m_players) {
        if (player->getId() != owningPlayer->getId()) {
            QList<GamePiece*> enemyPieces = player->getPiecesAtTerritory(destTerritory);
            if (!enemyPieces.isEmpty()) {
                hasEnemies = true;
                for (GamePiece *piece : enemyPieces) {
                    switch (piece->getType()) {
                        case GamePiece::Type::Infantry:
                            enemyInfantryCount++;
                            break;
                        case GamePiece::Type::Cavalry:
                            enemyCavalryCount++;
                            break;
                        case GamePiece::Type::Catapult:
                            enemyCatapultCount++;
                            break;
                        case GamePiece::Type::Caesar:
                            enemyLeaders.append(QString("Caesar %1").arg(player->getId()));
                            break;
                        case GamePiece::Type::General: {
                            GeneralPiece *gen = static_cast<GeneralPiece*>(piece);
                            enemyLeaders.append(QString("General %1 #%2").arg(player->getId()).arg(gen->getNumber()));
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
        }
    }

    // Build enemy description string
    if (hasEnemies) {
        QStringList parts;
        if (enemyInfantryCount > 0) parts.append(QString("%1 infantry").arg(enemyInfantryCount));
        if (enemyCavalryCount > 0) parts.append(QString("%1 cavalry").arg(enemyCavalryCount));
        if (enemyCatapultCount > 0) parts.append(QString("%1 catapult(s)").arg(enemyCatapultCount));
        if (!enemyLeaders.isEmpty()) parts.append(enemyLeaders.join(", "));

        int totalTroops = enemyInfantryCount + enemyCavalryCount + enemyCatapultCount;
        enemyDescription = QString("Enemy forces at %1: %2 troops (%3)")
            .arg(destTerritory)
            .arg(totalTroops)
            .arg(parts.join(", "));
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

    // Galleys move independently - they don't select troops
    // Generals/Caesars board galleys to transport their legions
    bool isGalley = (leader->getType() == GamePiece::Type::Galley);

    // For galleys, skip troop selection and just move
    if (isGalley) {
        // Store last territory before moving (for retreat purposes)
        GalleyPiece *galley = static_cast<GalleyPiece*>(leader);
        galley->setLastTerritoryName(currentTerritory);

        // Move galley
        leader->setTerritoryName(destinationTerritory);
        leader->setMovesRemaining(leader->getMovesRemaining() - 1);

        // Update galley's lastSeaZone for beach positioning
        bool destIsSea = destinationTerritory.startsWith("Mare") || destinationTerritory.startsWith("Oceanus");
        bool sourceIsSea = currentTerritory.startsWith("Mare") || currentTerritory.startsWith("Oceanus");

        if (destIsSea) {
            galley->setLastSeaZone(destinationTerritory);
        } else if (sourceIsSea) {
            galley->setLastSeaZone(currentTerritory);
        }

        // Move any leader and troops aboard the galley
        // Find leaders by checking if their onGalley matches this galley's serial number
        QString galleySerial = galley->getSerialNumber();
        qDebug() << "  Galley serial:" << galleySerial << "- looking for leaders aboard";

        for (Player *p : m_players) {
            if (p->getId() != galley->getPlayer()) continue;

            // Check caesars - if caesar's onGalley matches this galley, move them
            for (CaesarPiece *caesar : p->getCaesars()) {
                if (caesar->getOnGalley() == galleySerial) {
                    qDebug() << "  Found Caesar aboard galley";
                    caesar->setTerritoryName(destinationTerritory);

                    // Move troops in the caesar's legion
                    for (int troopId : caesar->getLegion()) {
                        GamePiece *troop = p->getPieceByUniqueId(troopId);
                        if (troop) {
                            troop->setTerritoryName(destinationTerritory);
                            qDebug() << "    Moved troop" << troopId << "to" << destinationTerritory;
                        }
                    }
                    qDebug() << "Moved Caesar aboard galley to" << destinationTerritory;
                }
            }

            // Check generals - if general's onGalley matches this galley, move them
            for (GeneralPiece *general : p->getGenerals()) {
                if (general->getOnGalley() == galleySerial) {
                    qDebug() << "  Found General" << general->getNumber() << "aboard galley with" << general->getLegion().size() << "troops";
                    general->setTerritoryName(destinationTerritory);

                    // Move troops in the general's legion
                    for (int troopId : general->getLegion()) {
                        GamePiece *troop = p->getPieceByUniqueId(troopId);
                        if (troop) {
                            troop->setTerritoryName(destinationTerritory);
                            qDebug() << "    Moved troop" << troopId << "to" << destinationTerritory;
                        }
                    }
                    qDebug() << "Moved General" << general->getNumber() << "aboard galley to" << destinationTerritory;
                }
            }
        }

        qDebug() << "Moved galley to" << destinationTerritory;

        // Update display
        updateAllPlayers();
        if (m_mapWidget) {
            m_mapWidget->update();
        }

        // Emit signal to notify that a piece moved (triggers heat map update)
        emit pieceMoved(currentPos.row, currentPos.col, destPos.row, destPos.col);
        return;
    }

    // For Caesars and Generals: show troop selection dialog if there are ANY troops at this territory
    // Loop until user selects valid troops or cancels
    bool validSelection = false;
    QList<int> selectedTroopIds;

    while (!allTroops.isEmpty() && !validSelection) {
        TroopSelectionDialog dialog(leaderName, allTroops, legionIds, this);

        // AI auto-mode: setup timer to interact with dialog and accept
        if (m_aiAutoMode && m_aiPlayer) {
            // Use AIPlayer's decideLegionComposition() for intelligent troop selection
            QList<int> troopsToSelect = m_aiPlayer->decideLegionComposition(leader, allTroops);
            qDebug() << "AI Auto-Mode: Legion composition decided -" << troopsToSelect.size() << "troop(s) selected";
            dialog.setupAIAutoMode(m_aiAutoModeDelayMs, troopsToSelect);
        } else if (m_aiAutoMode) {
            // Fallback: select all troops in the general's current legion that have moves
            QList<int> troopsToSelect;
            QList<int> currentLegion;
            // Cast leader to appropriate type to access getLegion()
            if (leader->getType() == GamePiece::Type::General) {
                currentLegion = static_cast<GeneralPiece*>(leader)->getLegion();
            } else if (leader->getType() == GamePiece::Type::Caesar) {
                currentLegion = static_cast<CaesarPiece*>(leader)->getLegion();
            } else if (leader->getType() == GamePiece::Type::Galley) {
                currentLegion = static_cast<GalleyPiece*>(leader)->getLegion();
            }
            for (GamePiece *troop : allTroops) {
                if (currentLegion.contains(troop->getUniqueId()) && troop->getMovesRemaining() > 0) {
                    troopsToSelect.append(troop->getUniqueId());
                }
            }
            qDebug() << "AI Auto-Mode (fallback): Selecting" << troopsToSelect.size() << "legion troop(s)";
            dialog.setupAIAutoMode(m_aiAutoModeDelayMs, troopsToSelect);
        }

        if (dialog.exec() == QDialog::Accepted) {
            selectedTroopIds = dialog.getSelectedTroopIds();

            // Check if we own the destination territory
            bool weOwnDestination = owningPlayer->ownsTerritory(destTerritory);

            // If moving into actual combat (enemies present), validate that troops are selected
            // Note: hasEnemies means actual enemy pieces, not just enemy-owned territory
            if (hasEnemies && selectedTroopIds.isEmpty()) {
                // AI auto-mode: just cancel the move instead of showing error dialog
                if (m_aiAutoMode) {
                    qDebug() << "AI Auto-Mode: Cannot move into combat without troops, cancelling";
                    qDebug() << "  " << leaderName << "at" << leader->getTerritoryName() << "-> " << destTerritory;
                    qDebug() << "  " << enemyDescription;
                    return;
                }

                QMessageBox msgBox(this);
                msgBox.setWindowTitle("Cannot Move");
                msgBox.setText(QString("%1 cannot move into combat without troops!\n\n"
                            "%2\n\n"
                            "Leaders must have at least one troop in their legion to enter combat.\n\n"
                            "Please select at least one troop or cancel the move.")
                    .arg(leaderName)
                    .arg(enemyDescription));
                msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                msgBox.exec();
                // Loop will continue - show dialog again
                continue;
            }

            // If moving into NON-OWNED territory (including unclaimed), validate that troops are selected
            // Generals/Caesars cannot capture territory without troops!
            if (!weOwnDestination && selectedTroopIds.isEmpty()) {
                // AI auto-mode: just cancel the move instead of showing error dialog
                if (m_aiAutoMode) {
                    qDebug() << "AI Auto-Mode: Cannot capture territory without troops, cancelling";
                    qDebug() << "  " << leaderName << "at" << leader->getTerritoryName() << "-> " << destTerritory;
                    return;
                }

                QMessageBox msgBox(this);
                msgBox.setWindowTitle("Cannot Capture Territory");
                msgBox.setText(QString("%1 cannot capture %2 without troops!\n\n"
                            "Generals and Caesars must have at least one troop in their legion to capture territory.\n\n"
                            "Please select at least one troop or cancel the move.")
                    .arg(leaderName)
                    .arg(destTerritory));
                msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                msgBox.exec();
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

                QMessageBox msgBox(this);
                msgBox.setWindowTitle("Cannot Move");
                msgBox.setText(QString("The following troops have no moves remaining and cannot move:\n\n%1\n\n"
                            "Please deselect these troops or try again.")
                    .arg(troopNames.join("\n")));
                msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                msgBox.exec();
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
            static_cast<CaesarPiece*>(leader)->setLastTerritoryName(currentTerritory);
        } else if (leader->getType() == GamePiece::Type::General) {
            static_cast<GeneralPiece*>(leader)->setLastTerritoryName(currentTerritory);
        } else if (leader->getType() == GamePiece::Type::Galley) {
            static_cast<GalleyPiece*>(leader)->setLastTerritoryName(currentTerritory);
        }

        // Move leader
        leader->setTerritoryName(destinationTerritory);
        leader->setMovesRemaining(leader->getMovesRemaining() - 1);

        // Update galley's lastSeaZone for beach positioning
        if (leader->getType() == GamePiece::Type::Galley) {
            GalleyPiece *galley = static_cast<GalleyPiece*>(leader);
            bool destIsSea = destinationTerritory.startsWith("Mare") || destinationTerritory.startsWith("Oceanus");
            bool sourceIsSea = currentTerritory.startsWith("Mare") || currentTerritory.startsWith("Oceanus");

            if (destIsSea) {
                // Moving into a sea zone - track this as the last sea zone
                galley->setLastSeaZone(destinationTerritory);
            } else if (sourceIsSea) {
                // Moving from sea to land (beaching) - track the sea we came from
                galley->setLastSeaZone(currentTerritory);
            }
            // If moving from land to land (shouldn't happen for galleys), keep existing lastSeaZone
        }

        qDebug() << "Moved leader" << leaderName;

        // Move selected troops
        for (GamePiece *troop : allTroops) {
            if (selectedTroopIds.contains(troop->getUniqueId())) {
                qDebug() << "Moving troop ID:" << troop->getUniqueId() << "to territory:" << destinationTerritory;
                troop->setTerritoryName(destinationTerritory);
                troop->setMovesRemaining(troop->getMovesRemaining() - 1);
            }
        }

        qDebug() << "Finished moving all troops";

        // Claim the destination territory for the owning player (but not sea territories)
        // Claim if NOT moving into combat, OR if moving into empty enemy territory (no enemy pieces)
        // Use conquestTerritory to handle building transfers when conquering
        // Check for sea territory by name (works with both grid and OpenGL map)
        bool destIsSea = destinationTerritory.startsWith("Mare") || destinationTerritory.startsWith("Oceanus");
        if ((!movingIntoCombat || !hasEnemies) && !destIsSea) {
            conquestTerritory(destinationTerritory, owningPlayer);
            qDebug() << "Claimed territory:" << destinationTerritory << "for player" << owningPlayer->getId();
        }

        // Update display
        updateAllPlayers();
        if (m_mapWidget) {
            m_mapWidget->update();
        }
    } else {
        // No troops available - generals/Caesars CANNOT capture territory without troops
        // They can still MOVE through friendly/own territory, but cannot claim new territory
        if (hasEnemies) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("Cannot Move");
            msgBox.setText(QString("%1 cannot move into combat without troops!\n\n"
                        "%2\n\n"
                        "Leaders must have at least one troop in their legion to enter combat.")
                .arg(leaderName)
                .arg(enemyDescription));
            msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            msgBox.exec();
            return;
        }

        // Check if destination is owned by us or unowned - generals can't capture without troops!
        bool weOwnDestination = owningPlayer->ownsTerritory(destinationTerritory);
        if (!weOwnDestination) {
            // Cannot capture territory without troops - show warning for human players
            if (!m_aiAutoMode) {
                QMessageBox msgBox(this);
                msgBox.setWindowTitle("Cannot Capture Territory");
                msgBox.setText(QString("%1 cannot capture %2 without troops!\n\n"
                            "Generals and Caesars must have at least one troop (Infantry, Cavalry, or Catapult) "
                            "in their legion to capture territory.")
                    .arg(leaderName)
                    .arg(destinationTerritory));
                msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                msgBox.exec();
            } else {
                qDebug() << "AI Auto-Mode:" << leaderName << "cannot capture" << destinationTerritory << "without troops - move cancelled";
            }
            return;
        }

        // Moving within own territory (no capture needed) - just move the leader

        // Store last territory before moving (for retreat purposes)
        if (leader->getType() == GamePiece::Type::Caesar) {
            static_cast<CaesarPiece*>(leader)->setLastTerritoryName(currentTerritory);
        } else if (leader->getType() == GamePiece::Type::General) {
            static_cast<GeneralPiece*>(leader)->setLastTerritoryName(currentTerritory);
        } else if (leader->getType() == GamePiece::Type::Galley) {
            static_cast<GalleyPiece*>(leader)->setLastTerritoryName(currentTerritory);
        }

        leader->setTerritoryName(destinationTerritory);
        leader->setMovesRemaining(leader->getMovesRemaining() - 1);

        // Update galley's lastSeaZone for beach positioning
        if (leader->getType() == GamePiece::Type::Galley) {
            GalleyPiece *galley = static_cast<GalleyPiece*>(leader);
            bool destIsSea = destinationTerritory.startsWith("Mare") || destinationTerritory.startsWith("Oceanus");
            bool sourceIsSea = currentTerritory.startsWith("Mare") || currentTerritory.startsWith("Oceanus");

            if (destIsSea) {
                galley->setLastSeaZone(destinationTerritory);
            } else if (sourceIsSea) {
                galley->setLastSeaZone(currentTerritory);
            }
        }

        qDebug() << "Moved leader" << leaderName << "(no troops, within own territory)";

        // NO territory claiming here - generals without troops cannot capture!

        // Update display
        updateAllPlayers();
        if (m_mapWidget) {
            m_mapWidget->update();
        }
    }
}

void PlayerInfoWidget::boardGalley(GamePiece *leader, const QString &seaTerritory, Player *player)
{
    if (!leader || !player || !m_mapWidget) return;

    // Verify leader has full moves (cannot move before embarking)
    double leaderFullMoves = 2.0;  // Generals and Caesars have 2 moves
    if (leader->getMovesRemaining() < leaderFullMoves) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Cannot Board");
        msgBox.setText("Leaders cannot move before embarking on a galley.\n"
                       "This leader has already moved this turn.");
        msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.exec();
        return;
    }

    qDebug() << "Boarding galley at" << seaTerritory;

    // Find all available galleys at this sea territory
    QList<GalleyPiece*> availableGalleys;
    for (GalleyPiece *galley : player->getGalleys()) {
        if (galley->getTerritoryName() == seaTerritory &&
            !galley->hasTransportedThisTurn() &&
            !galley->hasLeaderAboard() &&
            galley->getMovesRemaining() >= 0.5) {
            availableGalleys.append(galley);
        }
    }

    if (availableGalleys.isEmpty()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Cannot Board");
        msgBox.setText("No available galley at this sea zone.\n\n"
                       "A galley must have moves remaining and not have already transported a legion this turn.");
        msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.exec();
        return;
    }

    // Select which galley to board
    GalleyPiece *availableGalley = nullptr;
    if (availableGalleys.size() == 1) {
        // Only one galley available, use it
        availableGalley = availableGalleys.first();
    } else {
        // Multiple galleys - let user choose
        QStringList galleyOptions;
        for (GalleyPiece *galley : availableGalleys) {
            QString option = QString("Galley %1 (%2 moves remaining)")
                .arg(galley->getSerialNumber())
                .arg(galley->getMovesRemaining());
            galleyOptions.append(option);
        }

        bool ok;
        QString selected = QInputDialog::getItem(this, "Select Galley",
            QString("Multiple galleys available at %1.\nSelect which galley to board:").arg(seaTerritory),
            galleyOptions, 0, false, &ok);

        if (!ok || selected.isEmpty()) {
            return;  // User cancelled
        }

        // Find the selected galley
        int selectedIndex = galleyOptions.indexOf(selected);
        if (selectedIndex >= 0 && selectedIndex < availableGalleys.size()) {
            availableGalley = availableGalleys[selectedIndex];
        }
    }

    if (!availableGalley) {
        return;  // Should not happen, but safety check
    }

    // Get current position info
    QString currentTerritory = leader->getTerritoryName();
    Position currentPos = m_mapWidget->territoryNameToPosition(currentTerritory);
    Position seaPos = m_mapWidget->territoryNameToPosition(seaTerritory);

    // Get leader name for display
    QString leaderName;
    if (leader->getType() == GamePiece::Type::Caesar) {
        leaderName = QString("Caesar %1").arg(leader->getPlayer());
    } else if (leader->getType() == GamePiece::Type::General) {
        GeneralPiece *general = static_cast<GeneralPiece*>(leader);
        leaderName = QString("General %1 #%2").arg(leader->getPlayer()).arg(general->getNumber());
    }

    // Get all troops at current territory
    QList<GamePiece*> allPiecesAtTerritory = player->getPiecesAtTerritory(currentTerritory);
    QList<GamePiece*> allTroops;
    for (GamePiece *piece : allPiecesAtTerritory) {
        GamePiece::Type type = piece->getType();
        if (type == GamePiece::Type::Infantry ||
            type == GamePiece::Type::Cavalry ||
            type == GamePiece::Type::Catapult) {
            allTroops.append(piece);
        }
    }

    // Get current legion
    QList<int> legionIds;
    if (leader->getType() == GamePiece::Type::Caesar) {
        legionIds = static_cast<CaesarPiece*>(leader)->getLegion();
    } else if (leader->getType() == GamePiece::Type::General) {
        legionIds = static_cast<GeneralPiece*>(leader)->getLegion();
    }

    // Show troop selection dialog
    QList<int> selectedTroopIds;
    if (!allTroops.isEmpty()) {
        TroopSelectionDialog dialog(leaderName + " - Select troops to board galley", allTroops, legionIds, this);
        if (dialog.exec() != QDialog::Accepted) {
            return;  // User cancelled
        }
        selectedTroopIds = dialog.getSelectedTroopIds();

        // Validate troops have FULL moves remaining (cannot move before embarking)
        for (GamePiece *troop : allTroops) {
            if (selectedTroopIds.contains(troop->getUniqueId())) {
                // Check if troop has full movement (hasn't moved yet this turn)
                double fullMoves = 1.0;  // Default for infantry/catapult
                if (troop->getType() == GamePiece::Type::Cavalry) {
                    fullMoves = 2.0;
                }
                if (troop->getMovesRemaining() < fullMoves) {
                    QMessageBox msgBox(this);
                    msgBox.setWindowTitle("Cannot Board");
                    msgBox.setText("Troops cannot move before embarking on a galley.\n"
                                   "Please deselect troops that have already moved this turn.");
                    msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    msgBox.exec();
                    return;
                }
            }
        }
    }

    // Update legion
    if (leader->getType() == GamePiece::Type::Caesar) {
        static_cast<CaesarPiece*>(leader)->setLegion(selectedTroopIds);
        static_cast<CaesarPiece*>(leader)->setLastTerritoryName(currentTerritory);
    } else if (leader->getType() == GamePiece::Type::General) {
        static_cast<GeneralPiece*>(leader)->setLegion(selectedTroopIds);
        static_cast<GeneralPiece*>(leader)->setLastTerritoryName(currentTerritory);
    }

    // Move leader to sea territory (aboard galley)
    leader->setTerritoryName(seaTerritory);

    // Track that leader is on galley
    leader->setOnGalley(availableGalley->getSerialNumber());

    // Deduct 0.5 move from leader for boarding
    leader->setMovesRemaining(leader->getMovesRemaining() - 0.5);

    // Move selected troops to sea territory
    for (GamePiece *troop : allTroops) {
        if (selectedTroopIds.contains(troop->getUniqueId())) {
            troop->setTerritoryName(seaTerritory);
            troop->setOnGalley(availableGalley->getSerialNumber());
            troop->setMovesRemaining(troop->getMovesRemaining() - 0.5);  // 0.5 move for boarding
        }
    }

    // Mark galley as having leader aboard (but not yet transported - that happens on disembark)
    // Note: Embarking does NOT cost galley movement - only troops pay the embark cost
    availableGalley->setLeaderAboard(leader->getUniqueId());

    qDebug() << "Leader" << leaderName << "boarded galley" << availableGalley->getSerialNumber()
             << "with" << selectedTroopIds.size() << "troops";

    // Update display (no disembark dialog - player will choose to disembark later)
    updateAllPlayers();
    if (m_mapWidget) {
        m_mapWidget->update();
    }
}

void PlayerInfoWidget::boardGalleySpecific(GamePiece *leader, const QString &seaTerritory, Player *player, GalleyPiece *galley)
{
    if (!leader || !player || !galley || !m_mapWidget) return;

    // Verify leader has full moves (cannot move before embarking)
    double leaderFullMoves = 2.0;  // Generals and Caesars have 2 moves
    if (leader->getMovesRemaining() < leaderFullMoves) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Cannot Board");
        msgBox.setText("Leaders cannot move before embarking on a galley.\n"
                       "This leader has already moved this turn.");
        msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.exec();
        return;
    }

    qDebug() << "Boarding specific galley" << galley->getSerialNumber() << "at" << seaTerritory;

    // Verify the galley is still available
    if (galley->hasTransportedThisTurn() || galley->hasLeaderAboard() || galley->getMovesRemaining() < 0.5) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Cannot Board");
        msgBox.setText("This galley is no longer available for boarding.");
        msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.exec();
        return;
    }

    // Get current position info
    QString currentTerritory = leader->getTerritoryName();
    Position currentPos = m_mapWidget->territoryNameToPosition(currentTerritory);
    Position seaPos = m_mapWidget->territoryNameToPosition(seaTerritory);

    // Get leader name for display
    QString leaderName;
    if (leader->getType() == GamePiece::Type::Caesar) {
        leaderName = QString("Caesar %1").arg(leader->getPlayer());
    } else if (leader->getType() == GamePiece::Type::General) {
        GeneralPiece *general = static_cast<GeneralPiece*>(leader);
        leaderName = QString("General %1 #%2").arg(leader->getPlayer()).arg(general->getNumber());
    }

    // Get all troops at current territory
    QList<GamePiece*> allPiecesAtTerritory = player->getPiecesAtTerritory(currentTerritory);
    QList<GamePiece*> allTroops;
    for (GamePiece *piece : allPiecesAtTerritory) {
        GamePiece::Type type = piece->getType();
        if (type == GamePiece::Type::Infantry ||
            type == GamePiece::Type::Cavalry ||
            type == GamePiece::Type::Catapult) {
            allTroops.append(piece);
        }
    }

    // Get current legion
    QList<int> legionIds;
    if (leader->getType() == GamePiece::Type::Caesar) {
        legionIds = static_cast<CaesarPiece*>(leader)->getLegion();
    } else if (leader->getType() == GamePiece::Type::General) {
        legionIds = static_cast<GeneralPiece*>(leader)->getLegion();
    }

    // Show troop selection dialog
    QList<int> selectedTroopIds;
    if (!allTroops.isEmpty()) {
        TroopSelectionDialog dialog(leaderName + " - Select troops to board galley " + galley->getSerialNumber(), allTroops, legionIds, this);
        if (dialog.exec() != QDialog::Accepted) {
            return;  // User cancelled
        }
        selectedTroopIds = dialog.getSelectedTroopIds();

        // Validate troops have FULL moves remaining (cannot move before embarking)
        for (GamePiece *troop : allTroops) {
            if (selectedTroopIds.contains(troop->getUniqueId())) {
                // Check if troop has full movement (hasn't moved yet this turn)
                double fullMoves = 1.0;  // Default for infantry/catapult
                if (troop->getType() == GamePiece::Type::Cavalry) {
                    fullMoves = 2.0;
                }
                if (troop->getMovesRemaining() < fullMoves) {
                    QMessageBox msgBox(this);
                    msgBox.setWindowTitle("Cannot Board");
                    msgBox.setText("Troops cannot move before embarking on a galley.\n"
                                   "Please deselect troops that have already moved this turn.");
                    msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    msgBox.exec();
                    return;
                }
            }
        }
    }

    // Update legion
    if (leader->getType() == GamePiece::Type::Caesar) {
        static_cast<CaesarPiece*>(leader)->setLegion(selectedTroopIds);
        static_cast<CaesarPiece*>(leader)->setLastTerritoryName(currentTerritory);
    } else if (leader->getType() == GamePiece::Type::General) {
        static_cast<GeneralPiece*>(leader)->setLegion(selectedTroopIds);
        static_cast<GeneralPiece*>(leader)->setLastTerritoryName(currentTerritory);
    }

    // Move leader to sea territory (aboard galley)
    leader->setTerritoryName(seaTerritory);

    // Track that leader is on galley
    leader->setOnGalley(galley->getSerialNumber());

    // Deduct 0.5 move from leader for boarding
    leader->setMovesRemaining(leader->getMovesRemaining() - 0.5);

    // Move selected troops to sea territory
    for (GamePiece *troop : allTroops) {
        if (selectedTroopIds.contains(troop->getUniqueId())) {
            troop->setTerritoryName(seaTerritory);
            troop->setOnGalley(galley->getSerialNumber());
            troop->setMovesRemaining(troop->getMovesRemaining() - 0.5);  // 0.5 move for boarding
        }
    }

    // Mark galley as having leader aboard (but not yet transported - that happens on disembark)
    // Note: Embarking does NOT cost galley movement - only troops pay the embark cost
    galley->setLeaderAboard(leader->getUniqueId());

    qDebug() << "Leader" << leaderName << "boarded galley" << galley->getSerialNumber()
             << "with" << selectedTroopIds.size() << "troops";

    // Update display
    updateAllPlayers();
    if (m_mapWidget) {
        m_mapWidget->update();
    }
}

void PlayerInfoWidget::boardGalleyFromBeach(GamePiece *leader, GalleyPiece *galley, const QString &seaZone)
{
    if (!leader || !galley || !m_mapWidget) return;

    // Find the player who owns the leader
    Player *player = nullptr;
    for (Player *p : m_players) {
        if (p->getId() == leader->getPlayer()) {
            player = p;
            break;
        }
    }

    if (!player) return;

    qDebug() << "Boarding beached galley" << galley->getSerialNumber() << "from"
             << leader->getTerritoryName() << "launching to" << seaZone;

    // Verify leader has full moves (cannot move before embarking)
    double leaderFullMoves = 2.0;  // Generals and Caesars have 2 moves
    if (leader->getMovesRemaining() < leaderFullMoves) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Cannot Board");
        msgBox.setText("Leaders cannot move before embarking on a galley.\n"
                       "This leader has already moved this turn.");
        msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.exec();
        return;
    }

    // Verify the galley is available and beached in the same territory
    if (galley->hasTransportedThisTurn() || galley->hasLeaderAboard() || galley->getMovesRemaining() < 1.0) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Cannot Board");
        msgBox.setText("This galley is no longer available for boarding.");
        msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.exec();
        return;
    }

    if (!galley->isBeached() || galley->getTerritoryName() != leader->getTerritoryName()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Cannot Board");
        msgBox.setText("The galley must be beached in the same territory as the leader.");
        msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.exec();
        return;
    }

    // Get current position info
    QString currentTerritory = leader->getTerritoryName();
    Position currentPos = m_mapWidget->territoryNameToPosition(currentTerritory);
    Position seaPos = m_mapWidget->territoryNameToPosition(seaZone);

    // Get leader name for display
    QString leaderName;
    if (leader->getType() == GamePiece::Type::Caesar) {
        leaderName = QString("Caesar %1").arg(leader->getPlayer());
    } else if (leader->getType() == GamePiece::Type::General) {
        GeneralPiece *general = static_cast<GeneralPiece*>(leader);
        leaderName = QString("General %1 #%2").arg(leader->getPlayer()).arg(general->getNumber());
    }

    // Get all troops at current territory
    QList<GamePiece*> allPiecesAtTerritory = player->getPiecesAtTerritory(currentTerritory);
    QList<GamePiece*> allTroops;
    for (GamePiece *piece : allPiecesAtTerritory) {
        GamePiece::Type type = piece->getType();
        if (type == GamePiece::Type::Infantry ||
            type == GamePiece::Type::Cavalry ||
            type == GamePiece::Type::Catapult) {
            allTroops.append(piece);
        }
    }

    // Get current legion
    QList<int> legionIds;
    if (leader->getType() == GamePiece::Type::Caesar) {
        legionIds = static_cast<CaesarPiece*>(leader)->getLegion();
    } else if (leader->getType() == GamePiece::Type::General) {
        legionIds = static_cast<GeneralPiece*>(leader)->getLegion();
    }

    // Show troop selection dialog for boarding beached galley
    QList<int> selectedTroopIds;
    if (!allTroops.isEmpty()) {
        TroopSelectionDialog dialog(leaderName + " - Select troops to board galley " + galley->getSerialNumber(), allTroops, legionIds, this);

        // AI auto-mode: setup timer to interact with dialog and accept
        if (m_aiAutoMode && m_aiPlayer) {
            // Use AIPlayer's decideLegionComposition() for intelligent troop selection
            QList<int> troopsToSelect = m_aiPlayer->decideLegionComposition(leader, allTroops);

            // CRITICAL: If decideLegionComposition returned 0 troops but there are troops available,
            // we MUST pick up at least one troop for galley transport to unowned territories.
            // Galley boarding requires troops with FULL moves remaining.
            if (troopsToSelect.isEmpty() && !allTroops.isEmpty()) {
                qDebug() << "AI Auto-Mode (galley board): No troops from decideLegionComposition, falling back to available troops with full moves";
                for (GamePiece *troop : allTroops) {
                    // Check if troop has full movement (hasn't moved yet this turn)
                    double fullMoves = 1.0;  // Default for infantry/catapult
                    if (troop->getType() == GamePiece::Type::Cavalry) {
                        fullMoves = 2.0;
                    }
                    if (troop->getMovesRemaining() >= fullMoves) {
                        troopsToSelect.append(troop->getUniqueId());
                        if (troopsToSelect.size() >= 5) break;  // Max 5 troops per legion (leaving room for leader)
                    }
                }
            }

            qDebug() << "AI Auto-Mode (galley board from beach): Legion composition decided -" << troopsToSelect.size() << "troop(s) selected";
            dialog.setupAIAutoMode(m_aiAutoModeDelayMs, troopsToSelect);
        } else if (m_aiAutoMode) {
            // Fallback: select all troops in the general's current legion that have full moves
            QList<int> troopsToSelect;
            for (GamePiece *troop : allTroops) {
                if (legionIds.contains(troop->getUniqueId())) {
                    // Check if troop has full movement (hasn't moved yet this turn)
                    double fullMoves = 1.0;  // Default for infantry/catapult
                    if (troop->getType() == GamePiece::Type::Cavalry) {
                        fullMoves = 2.0;
                    }
                    if (troop->getMovesRemaining() >= fullMoves) {
                        troopsToSelect.append(troop->getUniqueId());
                    }
                }
            }
            qDebug() << "AI Auto-Mode (galley board from beach fallback): Selecting" << troopsToSelect.size() << "legion troop(s)";
            dialog.setupAIAutoMode(m_aiAutoModeDelayMs, troopsToSelect);
        }

        if (dialog.exec() != QDialog::Accepted) {
            return;  // User cancelled
        }
        selectedTroopIds = dialog.getSelectedTroopIds();

        // Validate troops have FULL moves remaining (cannot move before embarking)
        for (GamePiece *troop : allTroops) {
            if (selectedTroopIds.contains(troop->getUniqueId())) {
                // Check if troop has full movement (hasn't moved yet this turn)
                double fullMoves = 1.0;  // Default for infantry/catapult
                if (troop->getType() == GamePiece::Type::Cavalry) {
                    fullMoves = 2.0;
                }
                if (troop->getMovesRemaining() < fullMoves) {
                    // AI auto-mode: just skip this troop instead of showing error dialog
                    if (m_aiAutoMode) {
                        qDebug() << "AI Auto-Mode: Troop" << troop->getUniqueId() << "cannot board galley (already moved)";
                        selectedTroopIds.removeOne(troop->getUniqueId());
                        continue;
                    }
                    QMessageBox msgBox(this);
                    msgBox.setWindowTitle("Cannot Board");
                    msgBox.setText("Troops cannot move before embarking on a galley.\n"
                                   "Please deselect troops that have already moved this turn.");
                    msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    msgBox.exec();
                    return;
                }
            }
        }
    }

    // Update legion for beached galley boarding
    if (leader->getType() == GamePiece::Type::Caesar) {
        static_cast<CaesarPiece*>(leader)->setLegion(selectedTroopIds);
        static_cast<CaesarPiece*>(leader)->setLastTerritoryName(currentTerritory);
    } else if (leader->getType() == GamePiece::Type::General) {
        static_cast<GeneralPiece*>(leader)->setLegion(selectedTroopIds);
        static_cast<GeneralPiece*>(leader)->setLastTerritoryName(currentTerritory);
    }

    // Move leader to sea zone (aboard galley)
    leader->setTerritoryName(seaZone);

    // Track that leader is on galley
    leader->setOnGalley(galley->getSerialNumber());

    // Boarding consumes ALL of the leader's moves
    leader->setMovesRemaining(0);

    // Move selected troops to sea zone
    for (GamePiece *troop : allTroops) {
        if (selectedTroopIds.contains(troop->getUniqueId())) {
            troop->setTerritoryName(seaZone);
            troop->setOnGalley(galley->getSerialNumber());
            troop->setMovesRemaining(0);  // Boarding consumes all troop moves
        }
    }

    // Launch the galley to the sea zone - this costs 1 galley move
    galley->setTerritoryName(seaZone);
    galley->setLastSeaZone(seaZone);  // Update last sea zone to the new location
    galley->setMovesRemaining(galley->getMovesRemaining() - 1);  // Launch costs 1 move

    // Mark galley as having leader aboard
    // Note: Embarking troops does NOT cost galley movement - only the launch does
    galley->setLeaderAboard(leader->getUniqueId());

    qDebug() << "Leader" << leaderName << "boarded beached galley" << galley->getSerialNumber()
             << "with" << selectedTroopIds.size() << "troops, launching to" << seaZone;

    // Update display
    updateAllPlayers();
    if (m_mapWidget) {
        m_mapWidget->update();
    }
}

void PlayerInfoWidget::disembarkFromGalley(GamePiece *leader, const QString &landTerritory, GalleyPiece *galley, Player *player)
{
    if (!leader || !galley || !player || !m_mapWidget) return;

    qDebug() << "Disembarking to" << landTerritory;

    Position landPos = m_mapWidget->territoryNameToPosition(landTerritory);
    QString seaTerritory = galley->getTerritoryName();

    // Get leader name for display
    QString leaderName;
    if (leader->getType() == GamePiece::Type::Caesar) {
        leaderName = QString("Caesar %1").arg(leader->getPlayer());
    } else if (leader->getType() == GamePiece::Type::General) {
        GeneralPiece *general = static_cast<GeneralPiece*>(leader);
        leaderName = QString("General %1 #%2").arg(leader->getPlayer()).arg(general->getNumber());
    }

    // Get legion IDs
    QList<int> legionIds;
    if (leader->getType() == GamePiece::Type::Caesar) {
        legionIds = static_cast<CaesarPiece*>(leader)->getLegion();
    } else if (leader->getType() == GamePiece::Type::General) {
        legionIds = static_cast<GeneralPiece*>(leader)->getLegion();
    }

    // Move leader to land
    leader->setTerritoryName(landTerritory);
    leader->clearGalley();

    // Units cannot move after disembarking - set moves to 0
    leader->setMovesRemaining(0);

    // Move troops to land
    QList<GamePiece*> piecesAtSea = player->getPiecesAtTerritory(seaTerritory);
    qDebug() << "Disembark: Looking for troops at" << seaTerritory << "- found" << piecesAtSea.size() << "pieces";
    qDebug() << "Disembark: Legion has" << legionIds.size() << "troop IDs:" << legionIds;

    int troopsMoved = 0;
    for (GamePiece *piece : piecesAtSea) {
        qDebug() << "  Piece at sea:" << piece->getUniqueId() << "type:" << static_cast<int>(piece->getType())
                 << "in legion:" << legionIds.contains(piece->getUniqueId());
        if (legionIds.contains(piece->getUniqueId())) {
            piece->setTerritoryName(landTerritory);
            piece->clearGalley();
            piece->setMovesRemaining(0);  // Cannot move after disembarking
            troopsMoved++;
            qDebug() << "  -> Moved troop" << piece->getUniqueId() << "to" << landTerritory;
        }
    }
    qDebug() << "Disembark: Moved" << troopsMoved << "troops to" << landTerritory;

    // Mark galley as having completed transport
    // Note: Disembarking does NOT cost galley movement - only troops pay the disembark cost
    galley->setTransportedThisTurn(true);
    galley->setLeaderAboard(0);

    // Beach the galley at the land territory
    // Save the current sea zone so the galley knows which direction it came from
    // Note: isBeached() is computed from territory name - setting to land territory makes it beached
    galley->setLastSeaZone(seaTerritory);
    galley->setTerritoryName(landTerritory);
    qDebug() << "Galley" << galley->getSerialNumber() << "beached at" << landTerritory
             << "(lastSeaZone=" << galley->getLastSeaZone() << ", isBeached=" << galley->isBeached() << ")";

    // Check if there are enemies at the destination (combat will be triggered separately)
    bool hasEnemies = false;
    for (Player *p : m_players) {
        if (p->getId() != player->getId()) {
            QList<GamePiece*> enemyPieces = p->getPiecesAtTerritory(landTerritory);
            if (!enemyPieces.isEmpty()) {
                hasEnemies = true;
                break;
            }
        }
    }

    // Claim the land territory only if:
    // 1. No enemies present (territory claim happens after combat resolves if enemies are present)
    // 2. Leader has at least one troop (generals/Caesars cannot capture territory alone)
    bool hasTroops = !legionIds.isEmpty();
    if (!hasEnemies && hasTroops) {
        conquestTerritory(landTerritory, player);
    } else if (!hasEnemies && !hasTroops) {
        qDebug() << "Leader" << leaderName << "disembarked without troops - cannot capture" << landTerritory;
    }

    qDebug() << "Leader" << leaderName << "disembarked to" << landTerritory;

    // Update display
    updateAllPlayers();
    if (m_mapWidget) {
        m_mapWidget->update();
    }
}

void PlayerInfoWidget::showDisembarkDialog(GamePiece *leader, GalleyPiece *galley, Player *player)
{
    if (!leader || !galley || !player || !m_mapWidget) return;

    QString seaTerritory = galley->getTerritoryName();

    // Get adjacent land territories
    QList<QString> neighbors = m_mapWidget->getGraph()->getNeighbors(seaTerritory);

    // Create dialog to choose destination
    QDialog dialog(this);
    dialog.setWindowTitle("Disembark - Choose Destination");
    dialog.setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *label = new QLabel(QString("Choose a land territory to disembark to from %1:").arg(seaTerritory));
    layout->addWidget(label);

    QListWidget *listWidget = new QListWidget();
    for (const QString &neighborName : neighbors) {
        // Use graph-based lookup (works for both grid and OpenGL maps)
        bool isSea = m_mapWidget->getGraph() ? m_mapWidget->getGraph()->isSeaTerritory(neighborName) : false;

        if (!isSea) {
            // This is a land territory - valid destination
            int value = m_mapWidget->getGraph() ? m_mapWidget->getGraph()->getValue(neighborName) : 0;

            // Find owner by checking which player owns the territory
            QChar owner = '\0';
            for (Player *p : m_players) {
                if (p->ownsTerritory(neighborName)) {
                    owner = p->getId();
                    break;
                }
            }
            QString ownership = (owner == '\0') ? "[Unclaimed]" : (owner == player->getId()) ? "[You]" : QString("[Player %1]").arg(owner);

            QString displayText = (value > 0) ? QString("%1 (%2) %3").arg(neighborName).arg(value).arg(ownership)
                                              : QString("%1 %2").arg(neighborName).arg(ownership);

            QListWidgetItem *item = new QListWidgetItem(displayText);
            item->setData(Qt::UserRole, neighborName);
            listWidget->addItem(item);
        }
    }
    layout->addWidget(listWidget);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton("Disembark");
    QPushButton *cancelButton = new QPushButton("Stay on Galley");
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);

    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(listWidget, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);

    if (dialog.exec() == QDialog::Accepted && listWidget->currentItem()) {
        QString selectedTerritory = listWidget->currentItem()->data(Qt::UserRole).toString();
        disembarkFromGalley(leader, selectedTerritory, galley, player);
    } else {
        // Player chose to stay on galley - update display anyway
        updateAllPlayers();
        if (m_mapWidget) {
            m_mapWidget->update();
        }
    }
}

void PlayerInfoWidget::moveLeaderWithTroops(GamePiece *leader, int rowDelta, int colDelta)
{
    if (!leader || !m_mapWidget) return;

    // Get the leader's current position from territory name
    QString currentTerritory = leader->getTerritoryName();
    Position currentPos = m_mapWidget->territoryNameToPosition(currentTerritory);

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

        // Check if we own the destination territory
        bool weOwnDestination = owningPlayer->ownsTerritory(destTerritory);

        // If moving into combat, validate that legion is not empty
        if (hasEnemies && selectedTroopIds.isEmpty()) {
            QMessageBox msgBox(dialog);
            msgBox.setWindowTitle("Cannot Enter Combat");
            msgBox.setText("Cannot enter combat without troops.\n\n"
                "A General/Caesar cannot fight alone. Please select at least one troop to form a legion.");
            msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            msgBox.exec();
            // Dialog stays open, loop continues
            continue;
        }

        // If moving into NON-OWNED territory (including unclaimed), validate that legion is not empty
        // Generals/Caesars cannot capture territory without troops!
        if (!weOwnDestination && selectedTroopIds.isEmpty()) {
            QMessageBox msgBox(dialog);
            msgBox.setWindowTitle("Cannot Capture Territory");
            msgBox.setText("Cannot capture territory without troops.\n\n"
                "A General/Caesar cannot claim new territory alone. Please select at least one troop to capture the territory.");
            msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            msgBox.exec();
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
            QMessageBox msgBox(dialog);
            msgBox.setWindowTitle("Cannot Move");
            msgBox.setText(errorMsg);
            msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            msgBox.exec();
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

        // AI auto-mode: automatically confirm combat entry
        if (m_aiAutoMode) {
            qDebug() << "AI Auto-Mode: Auto-confirming combat entry";
        } else {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("Enter Combat");
            msgBox.setText(warningMsg);
            msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            if (msgBox.exec() != QMessageBox::Yes) {
                delete dialog;
                return;  // User cancelled combat entry
            }
        }
    }

    // Save current tab index before any moves
    int currentTabIndex = m_tabWidget->currentIndex();

    // Store last territory before moving (for retreat purposes)
    if (leader->getType() == GamePiece::Type::Caesar) {
        static_cast<CaesarPiece*>(leader)->setLastTerritoryName(currentTerritory);
    } else if (leader->getType() == GamePiece::Type::General) {
        static_cast<GeneralPiece*>(leader)->setLastTerritoryName(currentTerritory);
    } else if (leader->getType() == GamePiece::Type::Galley) {
        static_cast<GalleyPiece*>(leader)->setLastTerritoryName(currentTerritory);
    }

    // Move the leader first
    movePiece(leader, rowDelta, colDelta);
    qDebug() << "Moved leader" << leaderName;

    // Move all selected troops
    for (int pieceId : selectedTroopIds) {
        for (GamePiece *piece : troopsAtPosition) {
            if (piece->getUniqueId() == pieceId) {
                qDebug() << "Moving troop ID:" << pieceId << "from" << piece->getTerritoryName();
                movePiece(piece, rowDelta, colDelta);
                qDebug() << "  to" << piece->getTerritoryName();
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

    // Explicitly claim territory if there are no enemy PIECES (even if hasEnemies was set due to ownership)
    // This handles the case where we move into enemy-owned territory that has no defenders
    // Note: Generals/Caesars can only capture territory if they have troops with them
    if (enemyPiecesAtDest.isEmpty() && !m_mapWidget->isSeaTerritory(destPos.row, destPos.col) && !selectedTroopIds.isEmpty()) {
        conquestTerritory(destTerritory, owningPlayer);
        qDebug() << "Adjacent movement: Claimed territory" << destTerritory << "for player" << owningPlayer->getId();
    } else if (enemyPiecesAtDest.isEmpty() && !m_mapWidget->isSeaTerritory(destPos.row, destPos.col) && selectedTroopIds.isEmpty()) {
        qDebug() << "Adjacent movement: General/Caesar moved without troops - territory" << destTerritory << "NOT captured";
    }

    // Update display once after all moves
    updateAllPlayers();

    // Restore the tab index
    m_tabWidget->setCurrentIndex(currentTabIndex);

    delete dialog;
}

void PlayerInfoWidget::moveLeaderViaRoad(GamePiece *leader, const QString &destinationTerritory)
{
    // Territory name-based road movement (works with graph-based/OpenGL maps)
    if (!leader || !m_mapWidget || destinationTerritory.isEmpty()) return;

    QString currentTerritory = leader->getTerritoryName();
    qDebug() << "Road movement (by name):" << currentTerritory << "->" << destinationTerritory;

    // Find the player who owns this leader
    Player *owningPlayer = nullptr;
    for (Player *player : m_players) {
        if (player->getId() == leader->getPlayer()) {
            owningPlayer = player;
            break;
        }
    }

    if (!owningPlayer) {
        qDebug() << "Road movement: Could not find owning player";
        return;
    }

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

    // Check if there are enemy pieces at the destination OR if destination is owned by enemy
    bool hasEnemies = false;
    QList<GamePiece*> enemyPiecesAtDest;

    // Check for enemy pieces
    for (Player *player : m_players) {
        if (player->getId() != owningPlayer->getId()) {
            QList<GamePiece*> enemyPieces = player->getPiecesAtTerritory(destinationTerritory);
            if (!enemyPieces.isEmpty()) {
                hasEnemies = true;
                enemyPiecesAtDest.append(enemyPieces);
            }
        }
    }

    // Also check if destination territory is owned by an enemy player
    if (!hasEnemies) {
        for (Player *player : m_players) {
            if (player->getId() != owningPlayer->getId() && player->ownsTerritory(destinationTerritory)) {
                hasEnemies = true;  // Moving into enemy-owned territory
                break;
            }
        }
    }

    // Show troop selection dialog and loop until valid or cancelled
    TroopSelectionDialog *dialog = new TroopSelectionDialog(leaderName, troopsAtPosition, currentLegion, this);

    // AI Auto-Mode: setup timer to interact with dialog and accept
    if (m_aiAutoMode && m_aiPlayer) {
        // Use AIPlayer's decideLegionComposition() for intelligent troop selection
        QList<int> troopsToSelect = m_aiPlayer->decideLegionComposition(leader, troopsAtPosition);
        qDebug() << "AI Auto-Mode (road by name): Legion composition decided -" << troopsToSelect.size() << "troop(s) selected";
        dialog->setupAIAutoMode(m_aiAutoModeDelayMs, troopsToSelect);
    } else if (m_aiAutoMode) {
        // Fallback: select all troops in the general's current legion that have moves
        QList<int> troopsToSelect;
        for (GamePiece *troop : troopsAtPosition) {
            if (currentLegion.contains(troop->getUniqueId()) && troop->getMovesRemaining() > 0) {
                troopsToSelect.append(troop->getUniqueId());
            }
        }
        qDebug() << "AI Auto-Mode (road by name fallback): Selecting" << troopsToSelect.size() << "legion troop(s)";
        dialog->setupAIAutoMode(m_aiAutoModeDelayMs, troopsToSelect);
    }

    bool validSelection = false;
    QList<int> selectedTroopIds;

    while (!validSelection) {
        if (dialog->exec() != QDialog::Accepted) {
            // User cancelled
            delete dialog;
            return;
        }

        selectedTroopIds = dialog->getSelectedTroopIds();

        // Check if we own the destination territory
        bool weOwnDestination = owningPlayer->ownsTerritory(destinationTerritory);

        // If moving into combat, validate that legion is not empty
        if (hasEnemies && selectedTroopIds.isEmpty()) {
            // AI auto-mode: just cancel the move instead of showing error dialog
            if (m_aiAutoMode) {
                qDebug() << "AI Auto-Mode (road by name): Cannot move into combat without troops, cancelling";
                delete dialog;
                return;
            }

            QMessageBox msgBox(dialog);
            msgBox.setWindowTitle("Cannot Enter Combat");
            msgBox.setText("Cannot enter combat without troops.\n\n"
                "A General/Caesar cannot fight alone. Please select at least one troop to form a legion.");
            msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            msgBox.exec();
            continue;
        }

        // If moving into NON-OWNED territory (including unclaimed), validate that legion is not empty
        // Generals/Caesars cannot capture territory without troops!
        if (!weOwnDestination && selectedTroopIds.isEmpty()) {
            // AI auto-mode: just cancel the move instead of showing error dialog
            if (m_aiAutoMode) {
                qDebug() << "AI Auto-Mode (road by name): Cannot capture territory without troops, cancelling";
                delete dialog;
                return;
            }

            QMessageBox msgBox(dialog);
            msgBox.setWindowTitle("Cannot Capture Territory");
            msgBox.setText("Cannot capture territory without troops.\n\n"
                "A General/Caesar cannot claim new territory alone. Please select at least one troop to capture the territory.");
            msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            msgBox.exec();
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
                    }
                    troopsWithoutMoves.append(pieceName);
                    break;
                }
            }
        }

        if (!troopsWithoutMoves.isEmpty()) {
            QString errorMsg = "The following troops have no moves remaining:\n\n";
            errorMsg += troopsWithoutMoves.join("\n");
            errorMsg += "\n\nPlease uncheck troops without moves and try again.";
            QMessageBox msgBox(dialog);
            msgBox.setWindowTitle("Cannot Move");
            msgBox.setText(errorMsg);
            msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            msgBox.exec();
        } else {
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

    // If moving into combat, show warning dialog (skip for AI auto-mode)
    if (hasEnemies && !enemyPiecesAtDest.isEmpty() && !m_aiAutoMode) {
        // Build description of forces...
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

        QStringList enemyTroops;
        int enemyInfantry = 0, enemyCavalry = 0, enemyCatapults = 0;
        for (GamePiece *piece : enemyPiecesAtDest) {
            if (piece->getType() == GamePiece::Type::Infantry) enemyInfantry++;
            else if (piece->getType() == GamePiece::Type::Cavalry) enemyCavalry++;
            else if (piece->getType() == GamePiece::Type::Catapult) enemyCatapults++;
        }
        if (enemyInfantry > 0) enemyTroops << QString("%1 Infantry").arg(enemyInfantry);
        if (enemyCavalry > 0) enemyTroops << QString("%1 Cavalry").arg(enemyCavalry);
        if (enemyCatapults > 0) enemyTroops << QString("%1 Catapult").arg(enemyCatapults);

        QString warningMsg = QString("Your legion (%1) is about to travel via road and enter combat with enemy forces (%2).\n\n"
                                     "Do you want to continue?")
                                 .arg(ourTroops.join(", "))
                                 .arg(enemyTroops.join(", "));

        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Enter Combat (Via Road)");
        msgBox.setText(warningMsg);
        msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        if (msgBox.exec() != QMessageBox::Yes) {
            delete dialog;
            return;
        }
    }

    // Save current tab index before any moves
    int currentTabIndex = m_tabWidget->currentIndex();

    // Store last territory before moving (for retreat purposes)
    if (leader->getType() == GamePiece::Type::Caesar) {
        static_cast<CaesarPiece*>(leader)->setLastTerritoryName(currentTerritory);
    } else if (leader->getType() == GamePiece::Type::General) {
        static_cast<GeneralPiece*>(leader)->setLastTerritoryName(currentTerritory);
    } else if (leader->getType() == GamePiece::Type::Galley) {
        static_cast<GalleyPiece*>(leader)->setLastTerritoryName(currentTerritory);
    }

    // Move the leader by setting territory name directly (works with graph-based maps)
    leader->setTerritoryName(destinationTerritory);
    qDebug() << "Moved leader" << leaderName << "via road to" << destinationTerritory;

    // Move all selected troops by setting their territory names
    for (int pieceId : selectedTroopIds) {
        for (GamePiece *piece : troopsAtPosition) {
            if (piece->getUniqueId() == pieceId) {
                qDebug() << "Moving troop ID:" << pieceId << "via road from" << piece->getTerritoryName() << "to" << destinationTerritory;
                piece->setTerritoryName(destinationTerritory);
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
    if (hasEnemies && !enemyPiecesAtDest.isEmpty()) {
        leader->setMovesRemaining(0);
        qDebug() << "Entered combat via road - all moves consumed for" << leaderName;
    }

    // Claim territory if there are no enemy PIECES
    // Note: Generals/Caesars can only capture territory if they have troops with them
    if (enemyPiecesAtDest.isEmpty() && !selectedTroopIds.isEmpty()) {
        conquestTerritory(destinationTerritory, owningPlayer);
        qDebug() << "Road movement: Claimed territory" << destinationTerritory << "for player" << owningPlayer->getId();
    } else if (enemyPiecesAtDest.isEmpty() && selectedTroopIds.isEmpty()) {
        qDebug() << "Road movement: General/Caesar moved without troops - territory" << destinationTerritory << "NOT captured";
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
    if (!m_mapWidget) {
        return "";
    }

    // For grid-based maps, check bounds; for OpenGL maps (rows()==0), skip bounds check
    if (m_mapWidget->rows() > 0 && (row < 0 || row >= m_mapWidget->rows() || col < 0 || col >= m_mapWidget->cols())) {
        return "";
    }

    QString territoryName = getTerritoryNameAt(row, col);
    return getTroopInfoAtTerritory(territoryName);
}

QString PlayerInfoWidget::getTroopInfoAtTerritory(const QString &territoryName) const
{
    if (territoryName.isEmpty()) {
        return "";
    }

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

void PlayerInfoWidget::conquestTerritory(const QString &territoryName, Player *newOwner)
{
    if (!newOwner || territoryName.isEmpty() || !m_mapWidget) {
        return;
    }

    // Check if the new owner already owns this territory
    if (newOwner->ownsTerritory(territoryName)) {
        return;  // Nothing to do
    }

    // Find the previous owner (if any)
    Player *previousOwner = nullptr;
    for (Player *player : m_players) {
        if (player != newOwner && player->ownsTerritory(territoryName)) {
            previousOwner = player;
            break;
        }
    }

    if (previousOwner) {
        qDebug() << "Territory" << territoryName << "being conquered from player"
                 << previousOwner->getId() << "by player" << newOwner->getId();

        // Get the position for this territory
        Position territoryPos = m_mapWidget->territoryNameToPosition(territoryName);

        // Transfer or destroy any city at this territory
        // Note: Roads are computed on-the-fly from city positions, so no road cleanup needed
        City *city = previousOwner->getCityAtTerritory(territoryName);
        if (city) {
            qDebug() << "Transferring city at" << territoryName << "from"
                     << previousOwner->getId() << "to" << newOwner->getId();

            // Remove city from previous owner
            previousOwner->removeCity(city);
            // Change ownership to new owner
            city->setOwner(newOwner->getId());
            // Add to new owner
            newOwner->addCity(city);
        }

        // Unclaim from previous owner
        previousOwner->unclaimTerritory(territoryName);
    }

    // Claim for new owner
    newOwner->claimTerritory(territoryName);
    qDebug() << "Territory" << territoryName << "now owned by player" << newOwner->getId();
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

void PlayerInfoWidget::endTurn()
{
    // Public method to end turn - shows the widget and triggers the end turn logic
    show();
    raise();
    activateWindow();
    onEndTurnClicked();
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
    // Use MapGraph to get all territory names instead of fixed grid iteration
    QMap<QString, Position> combatTerritories;  // Map of territory name to position

    // Get all territory names from the graph
    QList<QString> allTerritories;
    if (m_mapWidget && m_mapWidget->getGraph()) {
        allTerritories = m_mapWidget->getGraph()->getTerritoryNames();
    }

    for (const QString &territoryName : allTerritories) {
        // Skip sea territories (no land combat there)
        if (territoryName.startsWith("Mare") || territoryName.startsWith("Oceanus")) {
            continue;
        }

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
            if (!combatTerritories.contains(territoryName)) {
                // Get position for display purposes (may be -1,-1 for graph-based maps)
                Position pos = m_mapWidget->territoryNameToPosition(territoryName);
                combatTerritories[territoryName] = pos;
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

        // Auto-dismiss for AI players
        AIPlayer *currentAI = getAIPlayerForPlayer(currentPlayer->getId());
        if (currentAI) {
            QTimer::singleShot(1500, &combatMsgBox, &QMessageBox::accept);
        }

        combatMsgBox.exec();

        // Resolve combats one at a time, letting the player choose order when multiple
        // (currentAI already declared above for auto-dismiss)
        QList<QString> remainingCombats = combatTerritories.keys();

        while (!remainingCombats.isEmpty()) {
            QString selectedTerritory;

            // If multiple combats and human player, let them choose which to resolve first
            if (remainingCombats.size() > 1 && !currentAI) {
                QDialog selectionDialog(this);
                selectionDialog.setWindowTitle("Select Battle to Resolve");
                QVBoxLayout *layout = new QVBoxLayout(&selectionDialog);

                QLabel *instructionLabel = new QLabel(QString("You have %1 battles remaining.\nSelect which battle to resolve next:\n\n"
                    "(Tip: Resolve battles strategically - results may affect retreat decisions)")
                    .arg(remainingCombats.size()));
                layout->addWidget(instructionLabel);

                QListWidget *combatList = new QListWidget();
                for (const QString &territory : remainingCombats) {
                    // Get info about this combat
                    QList<GamePiece*> yourPieces = currentPlayer->getPiecesAtTerritory(territory);
                    int enemyCount = 0;
                    QString enemyPlayerId;
                    for (Player *player : m_players) {
                        if (player->getId() != currentPlayer->getId()) {
                            QList<GamePiece*> enemyPieces = player->getPiecesAtTerritory(territory);
                            if (!enemyPieces.isEmpty()) {
                                enemyCount = enemyPieces.size();
                                enemyPlayerId = QString(player->getId());
                                break;
                            }
                        }
                    }
                    QString itemText = QString("%1: Your %2 pieces vs %3 enemy pieces (Player %4)")
                        .arg(territory)
                        .arg(yourPieces.size())
                        .arg(enemyCount)
                        .arg(enemyPlayerId);
                    combatList->addItem(itemText);
                }
                combatList->setCurrentRow(0);
                layout->addWidget(combatList);

                QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
                connect(buttonBox, &QDialogButtonBox::accepted, &selectionDialog, &QDialog::accept);
                layout->addWidget(buttonBox);

                selectionDialog.exec();

                int selectedIndex = combatList->currentRow();
                if (selectedIndex >= 0 && selectedIndex < remainingCombats.size()) {
                    selectedTerritory = remainingCombats[selectedIndex];
                } else {
                    selectedTerritory = remainingCombats.first();
                }
            } else {
                // AI player or only one combat - just take the first one
                selectedTerritory = remainingCombats.first();
            }

            // Remove from remaining list
            remainingCombats.removeOne(selectedTerritory);

            // Find the enemy player at this territory
            Player *enemyPlayer = nullptr;
            for (Player *player : m_players) {
                if (player->getId() != currentPlayer->getId()) {
                    QList<GamePiece*> enemyPieces = player->getPiecesAtTerritory(selectedTerritory);
                    if (!enemyPieces.isEmpty()) {
                        enemyPlayer = player;
                        break;
                    }
                }
            }

            if (enemyPlayer) {
                // Skip combat if disabled (test mode)
                if (m_combatDisabled) {
                    qDebug() << "Combat SKIPPED (test mode) at" << selectedTerritory;
                } else {
                    // Current player is the attacker (their turn), enemy player is the defender
                    CombatDialog *combatDialog = new CombatDialog(currentPlayer, enemyPlayer, selectedTerritory, m_mapWidget, this);

                    // Set up AI players for combat if either player is AI-controlled
                    AIPlayer *attackerAI = getAIPlayerForPlayer(currentPlayer->getId());
                    AIPlayer *defenderAI = getAIPlayerForPlayer(enemyPlayer->getId());
                    if (attackerAI || defenderAI) {
                        combatDialog->setupAIPlayers(attackerAI, defenderAI);
                    }

                    combatDialog->exec();
                    combatDialog->deleteLater();

                    // Update map display IMMEDIATELY after this combat resolves
                    // This lets the player see territory ownership changes before next combat
                    if (m_mapWidget) {
                        m_mapWidget->update();
                        // Process events to ensure the map repaints before next dialog
                        QApplication::processEvents();
                    }
                }
            }
        }

        // After all combats are resolved, continue to taxes and purchases
        // (Fall through to the code below)
    }

    // Collect taxes from owned territories before ending turn
    int taxesCollected = currentPlayer->collectTaxes(m_mapWidget);
    qDebug() << "Player" << currentPlayer->getId() << "collected" << taxesCollected << "talents in taxes";

    // Check inflation triggers - inflation is based on income (tribute), not wallet
    // When ANY player's income reaches 100 or 200, inflation triggers for all players
    // The triggering player gets to buy at the old price; new prices start next turn
    if (m_mapWidget) {
        int currentInflation = m_mapWidget->getInflationMultiplier();
        int highestIncome = 0;

        // Find the highest income among all players
        for (Player *player : m_players) {
            int income = player->calculateIncome(m_mapWidget);
            if (income > highestIncome) {
                highestIncome = income;
            }
        }

        // Check thresholds and trigger inflation if needed
        int newInflation = currentInflation;
        if (highestIncome >= 200 && currentInflation < 3) {
            newInflation = 3;  // Triple prices (double inflation)
            qDebug() << "DOUBLE INFLATION triggered! Highest income:" << highestIncome << "talents";
        } else if (highestIncome >= 100 && currentInflation < 2) {
            newInflation = 2;  // Double prices (single inflation)
            qDebug() << "SINGLE INFLATION triggered! Highest income:" << highestIncome << "talents";
        }

        // Note: inflation takes effect for the NEXT player's turn
        // Current player still buys at current prices
        if (newInflation != currentInflation) {
            // Store the pending inflation - it will apply after this player's purchase
            m_mapWidget->setInflationMultiplier(newInflation);
        }
    }

    // Build options for Purchase Dialog (with optional city destruction)
    QString homeProvinceName = currentPlayer->getHomeProvinceName();

    // Build list of territories available for city placement
    QList<CityPlacementOption> cityOptions;
    const QList<QString> &ownedTerritories = currentPlayer->getOwnedTerritories();
    for (const QString &territoryName : ownedTerritories) {
        // Check if this territory already has a city
        QList<City*> citiesInTerritory = currentPlayer->getCitiesAtTerritory(territoryName);
        if (citiesInTerritory.isEmpty()) {
            CityPlacementOption option;
            option.territoryName = territoryName;
            cityOptions.append(option);
        }
    }

    // Build list of existing cities that can be fortified
    QList<FortificationOption> fortificationOptions;
    const QList<City*> &cities = currentPlayer->getCities();
    for (City *city : cities) {
        if (!city->isFortified()) {
            FortificationOption option;
            option.territoryName = city->getTerritoryName();
            fortificationOptions.append(option);
        }
    }

    // Build list of sea territories for galley placement (adjacent to home province)
    QList<GalleyPlacementOption> galleyOptions;
    QList<QString> adjacentSeaTerritories = m_mapWidget->getAdjacentSeaTerritories(homeProvinceName);
    for (const QString &seaTerritoryName : adjacentSeaTerritories) {
        GalleyPlacementOption option;
        option.seaTerritoryName = seaTerritoryName;
        option.direction = "";  // Direction not needed for graph-based map
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

    // Get list of all cities for optional destruction
    QList<City*> allCities = currentPlayer->getCities();

    // Loop until user confirms their purchases and city destructions
    bool purchaseConfirmed = false;
    while (!purchaseConfirmed) {
        // Open purchase dialog (with optional city destruction)
        PurchaseDialog *purchaseDialog = new PurchaseDialog(
        currentPlayer->getId(),
        currentPlayer->getWallet(),
        m_mapWidget ? m_mapWidget->getInflationMultiplier() : 1,  // inflation multiplier
        cityOptions,
        fortificationOptions,
        galleyOptions,
        currentGalleyCount,
        availableInfantry,
        availableCavalry,
        availableCatapults,
        availableGalleys,
        allCities,  // Cities available for destruction
        m_mapWidget,  // For territory highlighting
        currentPlayer->getHomeProvinceName(),  // Home province name
        this
    );

    // AI Auto-Mode: Decide what to purchase and interact with dialog
    if (m_aiAutoMode) {
        // Check if there's an AIPlayer registered for this player - use their decision-making
        AIPlayer *currentAI = getAIPlayerForPlayer(currentPlayer->getId());
        if (currentAI) {
            qDebug() << "AI Auto-Mode: Using AIPlayer decision-making for purchases";
            currentAI->handlePurchaseDialog(purchaseDialog);
            // The AIPlayer will set up auto-mode and the dialog will be shown below
            // in the normal purchaseDialog->exec() call, then result is processed normally
        } else {
            // Fallback: No AIPlayer registered, use random selection
            qDebug() << "AI Auto-Mode: Deciding purchases (random selection)...";

        // Get available items from the dialog
        QList<PurchaseDialog::PurchaseMenuItem> menu = purchaseDialog->getAvailableItems();

        QMap<QString, int> purchases;
        int remainingMoney = currentPlayer->getWallet();

        // Track what we've already purchased (for one-time items like cities)
        QSet<QString> purchasedLocations;  // Tracks "City:Roma", "FortifiedCity:Roma", etc.

        // Track troop purchase counts against max quantity
        QMap<QString, int> troopPurchaseCounts;  // "Infantry" -> count purchased so far

        // Build a map of troop prices and max quantities for quick lookup
        QMap<QString, int> troopPrices;
        QMap<QString, int> troopMaxQuantities;
        for (const PurchaseDialog::PurchaseMenuItem &item : menu) {
            if (item.itemType == "Infantry" || item.itemType == "Cavalry" || item.itemType == "Catapult") {
                troopPrices[item.itemType] = item.currentPrice;
                troopMaxQuantities[item.itemType] = item.maxQuantity;
            }
        }

        // Keep buying random affordable items until we can't afford anything
        bool canBuySomething = true;
        while (canBuySomething && remainingMoney > 0) {
            // Build list of currently affordable items
            QList<int> affordableIndices;
            for (int i = 0; i < menu.size(); ++i) {
                const PurchaseDialog::PurchaseMenuItem &item = menu[i];

                // Skip if we can't afford it
                if (item.currentPrice > remainingMoney) continue;

                // Generate the purchase key for this item
                QString purchaseKey;
                if (item.itemType == "Infantry" || item.itemType == "Cavalry" || item.itemType == "Catapult") {
                    purchaseKey = item.itemType;
                    // Check if we've already bought the max quantity
                    if (troopPurchaseCounts.value(purchaseKey, 0) >= troopMaxQuantities.value(purchaseKey, 0)) {
                        continue;
                    }
                } else if (item.itemType == "City" || item.itemType == "FortifiedCity") {
                    purchaseKey = QString("%1:%2").arg(item.itemType).arg(item.location);
                    // One-time purchase - skip if already bought
                    if (purchasedLocations.contains(purchaseKey)) continue;
                    // Also skip FortifiedCity if we already bought City at same location
                    if (item.itemType == "FortifiedCity" && purchasedLocations.contains(QString("City:%1").arg(item.location))) continue;
                    // Also skip City if we already bought FortifiedCity at same location
                    if (item.itemType == "City" && purchasedLocations.contains(QString("FortifiedCity:%1").arg(item.location))) continue;
                } else if (item.itemType == "Fortification") {
                    purchaseKey = QString("Fortification:%1").arg(item.location);
                    // One-time purchase - skip if already bought
                    if (purchasedLocations.contains(purchaseKey)) continue;
                } else if (item.itemType == "Galley") {
                    // For galleys, extract just the sea territory name (not the direction)
                    QString seaTerritory = item.location.split(" (").first();
                    purchaseKey = QString("Galley:%1").arg(seaTerritory);
                    // TODO: Track galley counts per location if needed
                }

                affordableIndices.append(i);
            }

            if (affordableIndices.isEmpty()) {
                canBuySomething = false;
                break;
            }

            // Randomly select one affordable item
            int randomIndex = QRandomGenerator::global()->bounded(affordableIndices.size());
            int menuIndex = affordableIndices[randomIndex];
            const PurchaseDialog::PurchaseMenuItem &selectedItem = menu[menuIndex];

            // Generate the purchase key and add to purchases
            QString purchaseKey;
            if (selectedItem.itemType == "Infantry" || selectedItem.itemType == "Cavalry" || selectedItem.itemType == "Catapult") {
                purchaseKey = selectedItem.itemType;
                purchases[purchaseKey] = purchases.value(purchaseKey, 0) + 1;
                troopPurchaseCounts[purchaseKey] = troopPurchaseCounts.value(purchaseKey, 0) + 1;
                qDebug() << "AI Auto-Mode: Will buy 1" << purchaseKey << "for" << selectedItem.currentPrice;
            } else if (selectedItem.itemType == "City" || selectedItem.itemType == "FortifiedCity") {
                purchaseKey = QString("%1:%2").arg(selectedItem.itemType).arg(selectedItem.location);
                purchases[purchaseKey] = 1;
                purchasedLocations.insert(purchaseKey);
                qDebug() << "AI Auto-Mode: Will buy" << selectedItem.itemType << "at" << selectedItem.location << "for" << selectedItem.currentPrice;
            } else if (selectedItem.itemType == "Fortification") {
                purchaseKey = QString("Fortification:%1").arg(selectedItem.location);
                purchases[purchaseKey] = 1;
                purchasedLocations.insert(purchaseKey);
                qDebug() << "AI Auto-Mode: Will buy Fortification at" << selectedItem.location << "for" << selectedItem.currentPrice;
            } else if (selectedItem.itemType == "Galley") {
                QString seaTerritory = selectedItem.location.split(" (").first();
                purchaseKey = QString("Galley:%1").arg(seaTerritory);
                purchases[purchaseKey] = purchases.value(purchaseKey, 0) + 1;
                qDebug() << "AI Auto-Mode: Will buy Galley at" << seaTerritory << "for" << selectedItem.currentPrice;
            }

            remainingMoney -= selectedItem.currentPrice;
        }

        qDebug() << "AI Auto-Mode: Total purchases:" << purchases.size() << "types, remaining money:" << remainingMoney;
        purchaseDialog->setupAIAutoMode(m_aiAutoModeDelayMs, purchases);
        }  // End else (random selection fallback)
    }  // End if (m_aiAutoMode)

        int dialogResult = purchaseDialog->exec();

        // Clear territory hover highlight when dialog closes
        if (m_mapWidget) {
            m_mapWidget->setHoveredTerritoryById(0);  // Clear hover effect
        }

        if (dialogResult == QDialog::Accepted) {
            // Get purchase result
            PurchaseResult result = purchaseDialog->getPurchaseResult();

            // Debug: Check what cities are marked for destruction
            qDebug() << "Cities to destroy count:" << result.citiesToDestroy.size();
            for (City *city : result.citiesToDestroy) {
                qDebug() << "  - City at" << city->getTerritoryName();
            }

            // User confirmed in the purchase dialog's internal confirmation
            // Proceed with purchases and destructions
            purchaseConfirmed = true;

            // Deduct money from player's wallet
            if (result.totalCost > 0) {
                currentPlayer->spendMoney(result.totalCost);
                qDebug() << "Player" << currentPlayer->getId() << "spent" << result.totalCost << "talents";
            }

        // Create purchased cities
        for (const PurchaseResult::CityPurchase &cityPurchase : result.cities) {
            City *newCity = new City(
                currentPlayer->getId(),
                Position{-1, -1},  // Position not used for graph-based map
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

        // Create infantry
        for (int i = 0; i < result.infantry; ++i) {
            InfantryPiece *infantry = new InfantryPiece(currentPlayer->getId(), homeProvince, currentPlayer);
            currentPlayer->addInfantry(infantry);
        }
        if (result.infantry > 0) {
            qDebug() << "Player" << currentPlayer->getId() << "created" << result.infantry << "infantry at" << homeProvince;
        }

        // Create cavalry
        for (int i = 0; i < result.cavalry; ++i) {
            CavalryPiece *cavalry = new CavalryPiece(currentPlayer->getId(), homeProvince, currentPlayer);
            currentPlayer->addCavalry(cavalry);
        }
        if (result.cavalry > 0) {
            qDebug() << "Player" << currentPlayer->getId() << "created" << result.cavalry << "cavalry at" << homeProvince;
        }

        // Create catapults
        for (int i = 0; i < result.catapults; ++i) {
            CatapultPiece *catapult = new CatapultPiece(currentPlayer->getId(), homeProvince, currentPlayer);
            currentPlayer->addCatapult(catapult);
        }
        if (result.catapults > 0) {
            qDebug() << "Player" << currentPlayer->getId() << "created" << result.catapults << "catapults at" << homeProvince;
        }

        // Create galleys beached at home province, associated with selected sea zone
        // Galleys start on land (beached) and track which sea zone they face
        for (const PurchaseResult::GalleyPurchase &galleyPurchase : result.galleys) {
            for (int i = 0; i < galleyPurchase.count; ++i) {
                GalleyPiece *galley = new GalleyPiece(currentPlayer->getId(), homeProvince, currentPlayer);
                galley->setLastSeaZone(galleyPurchase.seaTerritoryName);  // Track which beach/sea it faces
                currentPlayer->addGalley(galley);
            }

            qDebug() << "Player" << currentPlayer->getId() << "created" << galleyPurchase.count
                     << "galleys at" << homeProvince << "facing" << galleyPurchase.seaTerritoryName;
        }

        // Destroy selected cities
        // Note: Roads are computed on-the-fly from city positions, so no road cleanup needed
        for (City *city : result.citiesToDestroy) {
            qDebug() << "  Destroying city at" << city->getTerritoryName()
                     << "(" << city->getPosition().row << "," << city->getPosition().col << ")";

            Position cityPosition = city->getPosition();

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

        if (!result.citiesToDestroy.isEmpty()) {
            qDebug() << "Player" << currentPlayer->getId() << "destroyed" << result.citiesToDestroy.size() << "cities";
            // Update display after destroying cities
            updateAllPlayers();
            if (m_mapWidget) {
                m_mapWidget->update();
            }
        }
        }  // End if (dialogResult == QDialog::Accepted)

        delete purchaseDialog;
    }  // End while (!purchaseConfirmed)

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
        m_mapWidget->updateHeatMap();  // Recalculate heat map for new player
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

void PlayerInfoWidget::onReachabilityClicked()
{
    // Find the current player (whose turn it is)
    Player *currentPlayer = nullptr;
    for (Player *player : m_players) {
        if (player->isMyTurn()) {
            currentPlayer = player;
            break;
        }
    }

    if (!currentPlayer) {
        QMessageBox::warning(this, "No Active Turn", "No player currently has an active turn.");
        return;
    }

    if (!m_mapWidget || !m_mapWidget->getGraph()) {
        QMessageBox::warning(this, "Error", "Map not available.");
        return;
    }

    // Generate reachability report
    ReachabilityCalculator calculator;
    QString report = calculator.generateReport(currentPlayer, m_mapWidget->getGraph());

    // Create dialog to display the report
    QDialog dialog(this);
    dialog.setWindowTitle(QString("Reachability Report - Player %1").arg(currentPlayer->getId()));
    dialog.resize(600, 500);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QTextEdit *textEdit = new QTextEdit();
    textEdit->setReadOnly(true);
    textEdit->setFont(QFont("Courier", 10));  // Monospace font for alignment
    textEdit->setText(report);
    layout->addWidget(textEdit);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttonBox);

    dialog.exec();
}

void PlayerInfoWidget::onRiskClicked()
{
    // Find the current player (whose turn it is)
    Player *currentPlayer = nullptr;
    for (Player *player : m_players) {
        if (player->isMyTurn()) {
            currentPlayer = player;
            break;
        }
    }

    if (!currentPlayer) {
        QMessageBox::warning(this, "No Active Turn", "No player currently has an active turn.");
        return;
    }

    if (!m_mapWidget || !m_mapWidget->getGraph()) {
        QMessageBox::warning(this, "Error", "Map not available.");
        return;
    }

    // Create dialog with tabs for different report types
    QDialog dialog(this);
    dialog.setWindowTitle(QString("Risk Assessment - Player %1").arg(currentPlayer->getId()));
    dialog.resize(650, 550);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);

    QTabWidget *tabWidget = new QTabWidget();

    ReachabilityCalculator calculator;

    // Tab 1: Dashboard (combined view)
    QTextEdit *dashboardEdit = new QTextEdit();
    dashboardEdit->setReadOnly(true);
    dashboardEdit->setFont(QFont("Courier", 10));
    dashboardEdit->setText(calculator.generateRiskDashboard(currentPlayer, m_players, m_mapWidget->getGraph()));
    tabWidget->addTab(dashboardEdit, "Dashboard");

    // Tab 2: Defensive Report
    QTextEdit *defenseEdit = new QTextEdit();
    defenseEdit->setReadOnly(true);
    defenseEdit->setFont(QFont("Courier", 10));
    defenseEdit->setText(calculator.generateDefensiveReport(currentPlayer, m_players, m_mapWidget->getGraph()));
    tabWidget->addTab(defenseEdit, "Defense");

    // Tab 3: Offensive Report
    QTextEdit *offenseEdit = new QTextEdit();
    offenseEdit->setReadOnly(true);
    offenseEdit->setFont(QFont("Courier", 10));
    offenseEdit->setText(calculator.generateOffensiveReport(currentPlayer, m_players, m_mapWidget->getGraph()));
    tabWidget->addTab(offenseEdit, "Offense");

    mainLayout->addWidget(tabWidget);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    mainLayout->addWidget(buttonBox);

    dialog.exec();
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
        // Reset last hovered action when menu opens
        m_lastHoveredAction = nullptr;
        // Connect hover sound to menu
        connect(&menu, &QMenu::hovered, this, &PlayerInfoWidget::playMenuClickSound);
        menu.exec(pos);
    }
}

// ============================================================================
// AI Auto-Mode
// ============================================================================

void PlayerInfoWidget::setAIAutoMode(bool enabled, int delayMs)
{
    m_aiAutoMode = enabled;
    m_aiAutoModeDelayMs = delayMs;
    qDebug() << "AI Auto-Mode:" << (enabled ? "ENABLED" : "DISABLED") << "delay:" << delayMs << "ms";
}

void PlayerInfoWidget::registerAIPlayer(QChar playerId, AIPlayer *aiPlayer)
{
    if (aiPlayer) {
        m_aiPlayers[playerId] = aiPlayer;
        qDebug() << "Registered AI player for player" << playerId;
    }
}

AIPlayer* PlayerInfoWidget::getAIPlayerForPlayer(QChar playerId) const
{
    return m_aiPlayers.value(playerId, nullptr);
}

bool PlayerInfoWidget::aiMoveLeaderToTerritory(GamePiece *leader, const QString &destinationTerritory)
{
    if (!leader || !m_mapWidget || destinationTerritory.isEmpty()) {
        qDebug() << "AI Move: Invalid parameters";
        return false;
    }

    // Check if leader has moves remaining
    if (leader->getMovesRemaining() <= 0) {
        qDebug() << "AI Move: Leader has no moves remaining";
        return false;
    }

    // Validate the move using getMovesForLeader and check movement type
    QList<MoveOption> validMoves = getMovesForLeader(leader);
    bool isValidMove = false;
    bool isViaRoad = false;
    bool isViaGalley = false;
    GalleyPiece *galley = nullptr;
    QString seaZone;
    for (const MoveOption &move : validMoves) {
        if (move.destinationTerritory == destinationTerritory) {
            isValidMove = true;
            isViaRoad = move.isViaRoad;
            isViaGalley = move.isViaGalley;
            galley = move.galley;
            seaZone = move.seaZone;
            break;
        }
    }

    if (!isValidMove) {
        qDebug() << "AI Move: Destination" << destinationTerritory << "is not a valid move for this leader";
        return false;
    }

    // NOTE: We don't check if the general has troops here because troops are selected
    // in the troop selection dialog (moveLeaderToTerritory, moveLeaderViaRoad, etc.)
    // Those dialogs will correctly validate and cancel the move if no troops are selected
    // for unowned/enemy territories.

    QString fromTerritory = leader->getTerritoryName();
    int movesBefore = leader->getMovesRemaining();

    QString moveType = isViaGalley ? "[via galley]" : (isViaRoad ? "[via road]" : "");
    qDebug() << "AI Move:" << leader->getSerialNumber() << "from" << fromTerritory << "to" << destinationTerritory
             << moveType;

    // Use appropriate movement method
    if (isViaGalley && galley) {
        // Galley transport - board galley, sail, and disembark
        // RULE: Leaders cannot move before boarding a galley!
        // Check if leader has already moved this turn (moves < full moves)
        double fullMoves = 2.0;  // Generals and Caesars have 2 moves
        if (leader->getMovesRemaining() < fullMoves) {
            qDebug() << "AI Move: Leader has already moved this turn (moves=" << leader->getMovesRemaining()
                     << ") - cannot board galley. Skipping galley route.";
            return false;
        }

        // seaZone now contains the DISEMBARK sea zone (where we land from)
        // We need to determine the LAUNCH sea zone from the galley's facing
        QString disembarkSeaZone = seaZone;
        QString launchSeaZone;

        if (galley->isBeached()) {
            // For beached galley, use its last sea zone or find adjacent sea
            if (galley->hasLastSeaZone()) {
                launchSeaZone = galley->getLastSeaZone();
            } else {
                // Find an adjacent sea zone to launch into
                QStringList leaderNeighbors = m_mapWidget->getGraph()->getNeighbors(fromTerritory);
                for (const QString &neighbor : leaderNeighbors) {
                    if (m_mapWidget->getGraph()->isSeaTerritory(neighbor)) {
                        launchSeaZone = neighbor;
                        break;
                    }
                }
            }
        } else {
            // Galley already at sea
            launchSeaZone = galley->getTerritoryName();
        }

        qDebug() << "AI Move: Using galley transport - launch to" << launchSeaZone << ", disembark from" << disembarkSeaZone;

        // Find the player who owns this leader
        Player *player = nullptr;
        for (Player *p : m_players) {
            if (p->getId() == leader->getPlayer()) {
                player = p;
                break;
            }
        }

        if (!player) {
            qDebug() << "AI Move: Could not find player for leader";
            return false;
        }

        // Board the galley (leader moves to launch sea zone)
        boardGalleyFromBeach(leader, galley, launchSeaZone);

        // Navigate galley from launch zone to disembark zone if they differ
        // Use BFS to find path through connected sea zones
        if (!disembarkSeaZone.isEmpty() && galley->getTerritoryName() != disembarkSeaZone) {
            // BFS to find path from current position to disembark zone
            QMap<QString, QString> cameFrom;
            QList<QString> queue;
            QString currentSeaZone = galley->getTerritoryName();
            queue.append(currentSeaZone);
            cameFrom[currentSeaZone] = "";

            bool found = false;
            while (!queue.isEmpty() && !found) {
                QString current = queue.takeFirst();
                if (current == disembarkSeaZone) {
                    found = true;
                    break;
                }
                QStringList neighbors = m_mapWidget->getGraph()->getNeighbors(current);
                for (const QString &neighbor : neighbors) {
                    if (m_mapWidget->getGraph()->isSeaTerritory(neighbor) && !cameFrom.contains(neighbor)) {
                        cameFrom[neighbor] = current;
                        queue.append(neighbor);
                    }
                }
            }

            // Trace path and move galley step by step
            if (found) {
                QList<QString> path;
                QString step = disembarkSeaZone;
                while (!step.isEmpty() && step != currentSeaZone) {
                    path.prepend(step);
                    step = cameFrom.value(step, "");
                }

                // Move through each sea zone in the path
                for (const QString &nextSea : path) {
                    if (galley->getMovesRemaining() < 1.0) {
                        qDebug() << "AI Move: Galley ran out of moves before reaching disembark zone";
                        break;
                    }

                    QString prevSea = galley->getTerritoryName();
                    galley->setLastTerritoryName(prevSea);
                    galley->setTerritoryName(nextSea);
                    galley->setMovesRemaining(galley->getMovesRemaining() - 1.0);

                    // Move leader and troops with the galley
                    leader->setTerritoryName(nextSea);

                    QList<int> legionIds;
                    if (leader->getType() == GamePiece::Type::Caesar) {
                        legionIds = static_cast<CaesarPiece*>(leader)->getLegion();
                    } else if (leader->getType() == GamePiece::Type::General) {
                        legionIds = static_cast<GeneralPiece*>(leader)->getLegion();
                    }

                    for (int troopId : legionIds) {
                        GamePiece *troop = player->getPieceByUniqueId(troopId);
                        if (troop && troop->getTerritoryName() == prevSea) {
                            troop->setTerritoryName(nextSea);
                        }
                    }

                    qDebug() << "Galley sailed from" << prevSea << "to" << nextSea;
                }
            }
        }

        // Disembark to destination
        disembarkFromGalley(leader, destinationTerritory, galley, player);

    } else if (isViaRoad) {
        // Road movement - use territory name-based overload (works with graph-based maps)
        moveLeaderViaRoad(leader, destinationTerritory);
    } else {
        // Normal adjacent movement
        moveLeaderToTerritory(leader, destinationTerritory);
    }

    // Check if move actually happened (territory changed)
    bool moveSucceeded = (leader->getTerritoryName() == destinationTerritory) &&
                         (leader->getMovesRemaining() < movesBefore);

    if (moveSucceeded) {
        qDebug() << "AI Move: Success! Moves remaining:" << leader->getMovesRemaining();
    } else {
        qDebug() << "AI Move: Move was cancelled or failed";
    }

    return moveSucceeded;
}

bool PlayerInfoWidget::hasEnemyPiecesAt(const QString &territory, Player *excludePlayer) const
{
    if (!excludePlayer) return false;

    for (Player *player : m_players) {
        if (player->getId() != excludePlayer->getId()) {
            QList<GamePiece*> enemyPieces = player->getPiecesAtTerritory(territory);
            if (!enemyPieces.isEmpty()) {
                return true;
            }
        }
    }
    return false;
}

QList<PlayerInfoWidget::MoveOption> PlayerInfoWidget::getMovesForLeader(GamePiece *leader) const
{
    QList<MoveOption> moves;

    if (!leader || !m_mapWidget) {
        qDebug() << "getMovesForLeader: Invalid leader or map widget";
        return moves;
    }

    // Find the player who owns this piece
    Player *player = nullptr;
    for (Player *p : m_players) {
        if (p->getId() == leader->getPlayer()) {
            player = p;
            break;
        }
    }
    if (!player) {
        qDebug() << "getMovesForLeader: Could not find player for leader";
        return moves;
    }

    QString territoryName = leader->getTerritoryName();
    bool isGeneral = (leader->getType() == GamePiece::Type::General);

    // Get neighbors using MapGraph
    QList<QString> neighbors = m_mapWidget->getGraph()->getNeighbors(territoryName);

    // Get territories connected by roads (computed on-the-fly)
    QStringList roadConnectedTerritories = m_mapWidget->getGraph()->getRoadConnectedTerritories(territoryName, player);

    // Filter out territories that are already neighbors
    QList<QString> roadOnlyTerritories;
    for (const QString &roadTerritory : roadConnectedTerritories) {
        if (!neighbors.contains(roadTerritory)) {
            roadOnlyTerritories.append(roadTerritory);
        }
    }

    // Combine neighbors and road-connected territories
    QList<QString> allDestinations = neighbors + roadOnlyTerritories;

    // Build MoveOption for each destination
    for (const QString &destinationName : allDestinations) {
        MoveOption option;
        option.destinationTerritory = destinationName;

        // Get territory info from graph
        if (m_mapWidget->getGraph()) {
            Territory destTerritory = m_mapWidget->getGraph()->getTerritory(destinationName);
            option.territoryValue = destTerritory.value;
            option.isSea = (destTerritory.value == 0);
        } else {
            Position destPos = m_mapWidget->territoryNameToPosition(destinationName);
            option.territoryValue = m_mapWidget->getTerritoryValueAt(destPos.row, destPos.col);
            option.isSea = m_mapWidget->isSeaTerritory(destPos.row, destPos.col);
        }

        // Find who owns this territory
        option.owner = '\0';
        for (Player *p : m_players) {
            if (p && p->ownsTerritory(destinationName)) {
                option.owner = p->getId();
                break;
            }
        }
        option.isOwnTerritory = (option.owner == leader->getPlayer());
        option.isViaRoad = roadOnlyTerritories.contains(destinationName);

        // Use territory name-based lookup (works for both grid and OpenGL maps)
        option.troopInfo = getTroopInfoAtTerritory(destinationName);

        // Check for combat (enemy pieces or enemy-owned territory)
        option.hasCombat = false;
        for (Player *p : m_players) {
            if (p->getId() != player->getId()) {
                QList<GamePiece*> enemyPieces = p->getPiecesAtTerritory(destinationName);
                if (!enemyPieces.isEmpty()) {
                    option.hasCombat = true;
                    break;
                }
            }
        }
        if (!option.hasCombat && option.owner != '\0' && option.owner != player->getId()) {
            option.hasCombat = true;
        }

        // Check for city
        option.hasCity = false;
        if (!option.hasCombat) {
            for (Player *p : m_players) {
                if (p->getCityAtTerritory(destinationName)) {
                    option.hasCity = true;
                    break;
                }
            }
        }

        // Skip sea territories for generals (they can't go there)
        if (isGeneral && option.isSea) {
            continue;
        }

        // Initialize galley fields
        option.isViaGalley = false;
        option.galley = nullptr;
        option.seaZone = QString();

        moves.append(option);
    }

    // === Add galley transport options ===
    // Check for available galleys that can transport this leader
    bool isCaesar = (leader->getType() == GamePiece::Type::Caesar);
    if (isGeneral || isCaesar) {
        // Find all adjacent sea zones (for checking galleys at sea or beached)
        QStringList adjacentSeaZones;
        for (const QString &neighbor : neighbors) {
            if (m_mapWidget->getGraph()->isSeaTerritory(neighbor)) {
                adjacentSeaZones.append(neighbor);
            }
        }

        // Check all player galleys
        for (GalleyPiece *galley : player->getGalleys()) {
            if (galley->hasTransportedThisTurn()) {
                continue;  // Galley already transported this turn
            }
            if (galley->hasLeaderAboard()) {
                continue;  // Galley already has a leader
            }
            if (galley->getMovesRemaining() < 1.0) {
                continue;  // Galley has no moves
            }

            QString galleySeaZone;
            bool isBeached = galley->isBeached();

            if (isBeached) {
                // Beached galley - must be at same territory as leader
                if (galley->getTerritoryName() != territoryName) {
                    continue;  // Beached galley not here
                }
                // Beached galley can launch to any adjacent sea zone
                // Use the last sea zone if available, otherwise pick first adjacent sea
                if (galley->hasLastSeaZone()) {
                    galleySeaZone = galley->getLastSeaZone();
                } else if (!adjacentSeaZones.isEmpty()) {
                    galleySeaZone = adjacentSeaZones.first();
                } else {
                    continue;  // No sea zones to launch to
                }
            } else {
                // Galley at sea - must be in adjacent sea zone
                galleySeaZone = galley->getTerritoryName();
                if (!adjacentSeaZones.contains(galleySeaZone)) {
                    continue;  // Galley not in adjacent sea zone
                }
            }

            // Found a usable galley - find all land territories it can reach
            double movesAfterLaunch = galley->getMovesRemaining() - 1.0;  // Launching costs 1.0
            qDebug() << "  Found usable galley" << galley->getSerialNumber()
                     << (isBeached ? "beached at" : "at sea in") << galley->getTerritoryName()
                     << "-> launching to" << galleySeaZone
                     << "with" << galley->getMovesRemaining() << "moves, after launch:" << movesAfterLaunch;

            // BFS through sea zones to find all reachable land territories
            // Movement costs per GalleyMovement_Plan.md:
            // - Launching from coast to sea = 1 movement
            // - Moving between sea zones = 1 movement
            // - Landing on coast = 1 movement
            // With 2 movement points: Coast→Sea→Coast (lands adjacent) or Coast→Sea→Sea (can't land)
            QSet<QString> visitedSeas;
            QList<QPair<QString, double>> toVisit;
            toVisit.append({galleySeaZone, movesAfterLaunch});
            visitedSeas.insert(galleySeaZone);

            while (!toVisit.isEmpty()) {
                auto current = toVisit.takeFirst();
                QString currentSea = current.first;
                double remainingMoves = current.second;

                // Check land neighbors for disembark options
                QStringList seaNeighbors = m_mapWidget->getGraph()->getNeighbors(currentSea);
                for (const QString &landNeighbor : seaNeighbors) {
                    if (m_mapWidget->getGraph()->isSeaTerritory(landNeighbor)) {
                        // Another sea zone - can sail there if moves remain
                        if (remainingMoves >= 1.0 && !visitedSeas.contains(landNeighbor)) {
                            visitedSeas.insert(landNeighbor);
                            toVisit.append({landNeighbor, remainingMoves - 1.0});
                        }
                    } else {
                        // Land territory - can disembark here if we have moves for landing
                        // Landing costs 1 movement point
                        if (remainingMoves < 1.0) {
                            continue;  // Not enough moves to land
                        }
                        // Skip if we're already at this territory
                        if (landNeighbor == territoryName) {
                            continue;
                        }
                        // Skip if already in normal moves
                        bool alreadyReachable = false;
                        for (const MoveOption &existingMove : moves) {
                            if (existingMove.destinationTerritory == landNeighbor) {
                                alreadyReachable = true;
                                break;
                            }
                        }
                        if (alreadyReachable) {
                            continue;
                        }

                        // Add this as a galley transport option
                        MoveOption option;
                        option.destinationTerritory = landNeighbor;
                        option.isViaGalley = true;
                        option.galley = galley;
                        // Store the sea zone we'd DISEMBARK from (currentSea), not the launch zone
                        // This is critical for proper navigation through connected sea zones
                        option.seaZone = currentSea;
                        option.isViaRoad = false;

                        // Get territory info
                        Territory destTerritory = m_mapWidget->getGraph()->getTerritory(landNeighbor);
                        option.territoryValue = destTerritory.value;
                        option.isSea = false;

                        // Find owner
                        option.owner = '\0';
                        for (Player *p : m_players) {
                            if (p && p->ownsTerritory(landNeighbor)) {
                                option.owner = p->getId();
                                break;
                            }
                        }
                        option.isOwnTerritory = (option.owner == leader->getPlayer());

                        // Get troop info
                        option.troopInfo = getTroopInfoAtTerritory(landNeighbor);

                        // Check for combat
                        option.hasCombat = false;
                        for (Player *p : m_players) {
                            if (p->getId() != player->getId()) {
                                QList<GamePiece*> enemyPieces = p->getPiecesAtTerritory(landNeighbor);
                                if (!enemyPieces.isEmpty()) {
                                    option.hasCombat = true;
                                    break;
                                }
                            }
                        }
                        if (!option.hasCombat && option.owner != '\0' && option.owner != player->getId()) {
                            option.hasCombat = true;
                        }

                        // Check for city
                        option.hasCity = false;
                        if (!option.hasCombat) {
                            for (Player *p : m_players) {
                                if (p->getCityAtTerritory(landNeighbor)) {
                                    option.hasCity = true;
                                    break;
                                }
                            }
                        }

                        moves.append(option);
                    }
                }
            }
        }
    }

    // Count galley routes for debug output
    int galleyRouteCount = 0;
    QStringList galleyDestinations;
    for (const MoveOption &m : moves) {
        if (m.isViaGalley) {
            galleyRouteCount++;
            galleyDestinations.append(m.destinationTerritory);
        }
    }
    qDebug() << "getMovesForLeader:" << leader->getSerialNumber() << "has" << moves.size() << "possible moves"
             << "(" << galleyRouteCount << "via galley:" << galleyDestinations.join(", ") << ")";
    return moves;
}

// ============================================================================
// AI Integration: Read state from UI
// ============================================================================

QChar PlayerInfoWidget::getCurrentDisplayedPlayerId() const
{
    int currentIndex = m_tabWidget->currentIndex();
    if (currentIndex >= 0 && currentIndex < m_players.size()) {
        return m_players[currentIndex]->getId();
    }
    return '\0';
}

Player* PlayerInfoWidget::getPlayerById(QChar playerId) const
{
    for (Player *player : m_players) {
        if (player->getId() == playerId) {
            return player;
        }
    }
    return nullptr;
}

int PlayerInfoWidget::getDisplayedWallet(QChar playerId) const
{
    // Find the wallet label by object name and parse its text
    QString labelName = QString("walletLabel_%1").arg(playerId);
    QLabel *walletLabel = findChild<QLabel*>(labelName);

    if (walletLabel) {
        // Text format is "X talents", extract the number
        QString text = walletLabel->text();
        QRegularExpression re("(\\d+)");
        QRegularExpressionMatch match = re.match(text);
        if (match.hasMatch()) {
            return match.captured(1).toInt();
        }
    }

    qDebug() << "AIReader: Could not find wallet label for player" << playerId;
    return -1;  // Indicate error
}

int PlayerInfoWidget::getDisplayedTerritoryCount(QChar playerId) const
{
    // Find the territory count label by object name
    QString labelName = QString("territoryCountLabel_%1").arg(playerId);
    QLabel *countLabel = findChild<QLabel*>(labelName);

    if (countLabel) {
        return countLabel->text().toInt();
    }

    qDebug() << "AIReader: Could not find territory count label for player" << playerId;
    return -1;
}

QStringList PlayerInfoWidget::getDisplayedTerritories(QChar playerId) const
{
    QStringList territories;

    // Find the territories section group box
    QString sectionName = QString("territoriesSection_%1").arg(playerId);
    QGroupBox *territoriesSection = findChild<QGroupBox*>(sectionName);

    if (territoriesSection) {
        // The group box title contains the count: "Owned Territories (X)"
        // We need to find the territory labels inside
        // They are QLabels with territory names

        // Find all labels within the section that look like territory names
        QList<QLabel*> labels = territoriesSection->findChildren<QLabel*>();
        for (QLabel *label : labels) {
            QString text = label->text();
            // Territory labels have format "TerritoryName (5)" or just the name
            // Skip labels that are styling hints like "(No territories owned)"
            if (!text.isEmpty() && !text.startsWith("(") && !text.contains("<b>")) {
                // Extract just the territory name (before any parenthetical)
                int parenPos = text.indexOf(" (");
                if (parenPos > 0) {
                    territories.append(text.left(parenPos));
                } else {
                    territories.append(text);
                }
            }
        }
    }

    return territories;
}

int PlayerInfoWidget::getDisplayedPieceCount(QChar playerId) const
{
    int count = 0;

    // Count from Caesar table
    QString caesarTableName = QString("caesarTable_%1").arg(playerId);
    QTableWidget *caesarTable = findChild<QTableWidget*>(caesarTableName);
    if (caesarTable) {
        count += caesarTable->rowCount();
    }

    // Count from General table
    QString generalTableName = QString("generalTable_%1").arg(playerId);
    QTableWidget *generalTable = findChild<QTableWidget*>(generalTableName);
    if (generalTable) {
        count += generalTable->rowCount();
    }

    // Note: This only counts leaders, not troops.
    // To count troops, we'd need to add object names to troop tables as well.

    return count;
}

QList<PlayerInfoWidget::DisplayedLeaderInfo> PlayerInfoWidget::getDisplayedLeaders(QChar playerId) const
{
    QList<DisplayedLeaderInfo> leaders;

    // Read from Caesar table
    QString caesarTableName = QString("caesarTable_%1").arg(playerId);
    QTableWidget *caesarTable = findChild<QTableWidget*>(caesarTableName);
    if (caesarTable) {
        for (int row = 0; row < caesarTable->rowCount(); ++row) {
            DisplayedLeaderInfo info;
            info.type = "Caesar";

            QTableWidgetItem *serialItem = caesarTable->item(row, 0);
            QTableWidgetItem *territoryItem = caesarTable->item(row, 1);
            QTableWidgetItem *movementItem = caesarTable->item(row, 2);
            QTableWidgetItem *galleyItem = caesarTable->item(row, 3);

            if (serialItem) info.serialNumber = serialItem->text();
            if (territoryItem) info.territory = territoryItem->text();
            if (movementItem) info.movesRemaining = movementItem->text().toInt();
            if (galleyItem) info.onGalley = galleyItem->text();

            leaders.append(info);
        }
    }

    // Read from General table
    QString generalTableName = QString("generalTable_%1").arg(playerId);
    QTableWidget *generalTable = findChild<QTableWidget*>(generalTableName);
    if (generalTable) {
        for (int row = 0; row < generalTable->rowCount(); ++row) {
            DisplayedLeaderInfo info;
            info.type = "General";

            QTableWidgetItem *serialItem = generalTable->item(row, 0);
            QTableWidgetItem *territoryItem = generalTable->item(row, 1);
            QTableWidgetItem *movementItem = generalTable->item(row, 2);
            QTableWidgetItem *galleyItem = generalTable->item(row, 3);

            if (serialItem) info.serialNumber = serialItem->text();
            if (territoryItem) info.territory = territoryItem->text();
            if (movementItem) info.movesRemaining = movementItem->text().toInt();
            if (galleyItem) info.onGalley = galleyItem->text();

            leaders.append(info);
        }
    }

    return leaders;
}

void PlayerInfoWidget::playMenuClickSound(QAction *action)
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
