#include "mapeditorwindow.h"
#include "editablegridwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QScrollArea>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QFileInfo>
#include <QRandomGenerator>
#include <algorithm>

MapEditorWindow::MapEditorWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Conquest of the Empire - Map Editor");
    setMinimumSize(800, 700);
    setupUI();
}

void MapEditorWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    createButtonsGroup();  // Create buttons first so onFlagsChanged can access them
    createMapSettingsGroup();
    createGameSettingsGroup();
    createMapEditorGroup();

    mainLayout->addWidget(m_mapSettingsGroup);
    mainLayout->addWidget(m_gameSettingsGroup);
    mainLayout->addWidget(m_mapEditorGroup, 1); // Give map editor stretch

    // Bottom buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_loadButton);
    buttonLayout->addWidget(m_clearButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    setCentralWidget(centralWidget);
}

void MapEditorWindow::createMapSettingsGroup()
{
    m_mapSettingsGroup = new QGroupBox("Map Settings", this);
    QHBoxLayout *layout = new QHBoxLayout(m_mapSettingsGroup);

    // Rows
    layout->addWidget(new QLabel("Rows:"));
    m_rowsSpinBox = new QSpinBox();
    m_rowsSpinBox->setRange(2, 20);
    m_rowsSpinBox->setValue(8);
    layout->addWidget(m_rowsSpinBox);

    layout->addSpacing(20);

    // Columns
    layout->addWidget(new QLabel("Columns:"));
    m_colsSpinBox = new QSpinBox();
    m_colsSpinBox->setRange(2, 20);
    m_colsSpinBox->setValue(8);
    layout->addWidget(m_colsSpinBox);

    layout->addSpacing(20);

    // Number of players
    layout->addWidget(new QLabel("Players:"));
    m_numPlayersSpinBox = new QSpinBox();
    m_numPlayersSpinBox->setRange(2, 6);
    m_numPlayersSpinBox->setValue(4);
    connect(m_numPlayersSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MapEditorWindow::onNumPlayersChanged);
    layout->addWidget(m_numPlayersSpinBox);

    layout->addSpacing(20);

    // Generate button
    m_generateButton = new QPushButton("Generate Map");
    connect(m_generateButton, &QPushButton::clicked, this, &MapEditorWindow::onGenerateMap);
    layout->addWidget(m_generateButton);

    layout->addStretch();
}

void MapEditorWindow::createGameSettingsGroup()
{
    m_gameSettingsGroup = new QGroupBox("Game Settings (Per Player)", this);
    QHBoxLayout *layout = new QHBoxLayout(m_gameSettingsGroup);

    // Starting Generals
    layout->addWidget(new QLabel("Generals:"));
    m_startingGeneralsSpinBox = new QSpinBox();
    m_startingGeneralsSpinBox->setRange(0, 10);
    m_startingGeneralsSpinBox->setValue(1);
    layout->addWidget(m_startingGeneralsSpinBox);

    layout->addSpacing(10);

    // Starting Infantry
    layout->addWidget(new QLabel("Infantry:"));
    m_startingInfantrySpinBox = new QSpinBox();
    m_startingInfantrySpinBox->setRange(0, 50);
    m_startingInfantrySpinBox->setValue(3);
    layout->addWidget(m_startingInfantrySpinBox);

    layout->addSpacing(10);

    // Starting Cavalry
    layout->addWidget(new QLabel("Cavalry:"));
    m_startingCavalrySpinBox = new QSpinBox();
    m_startingCavalrySpinBox->setRange(0, 50);
    m_startingCavalrySpinBox->setValue(2);
    layout->addWidget(m_startingCavalrySpinBox);

    layout->addSpacing(10);

    // Starting Catapults
    layout->addWidget(new QLabel("Catapults:"));
    m_startingCatapultsSpinBox = new QSpinBox();
    m_startingCatapultsSpinBox->setRange(0, 50);
    m_startingCatapultsSpinBox->setValue(1);
    layout->addWidget(m_startingCatapultsSpinBox);

    layout->addSpacing(10);

    // Starting Money
    layout->addWidget(new QLabel("Money:"));
    m_startingMoneySpinBox = new QSpinBox();
    m_startingMoneySpinBox->setRange(0, 10000);
    m_startingMoneySpinBox->setValue(100);
    m_startingMoneySpinBox->setSingleStep(10);
    layout->addWidget(m_startingMoneySpinBox);

    layout->addStretch();
}

void MapEditorWindow::createMapEditorGroup()
{
    m_mapEditorGroup = new QGroupBox("Map Editor", this);
    QVBoxLayout *layout = new QVBoxLayout(m_mapEditorGroup);

    // Instructions
    m_instructionsLabel = new QLabel(
        "Left-click: Cycle Land(5) / Land(10) / Sea  |  Right-click: Add player home flag");
    m_instructionsLabel->setStyleSheet("color: #666; font-style: italic;");
    layout->addWidget(m_instructionsLabel);

    // Flag status label
    m_flagStatusLabel = new QLabel();
    m_flagStatusLabel->setStyleSheet("color: #cc0000; font-weight: bold;");
    layout->addWidget(m_flagStatusLabel);

    // Grid widget in a scroll area
    QScrollArea *scrollArea = new QScrollArea();
    m_gridWidget = new EditableGridWidget();
    m_gridWidget->setNumPlayers(m_numPlayersSpinBox->value());
    connect(m_gridWidget, &EditableGridWidget::flagsChanged,
            this, &MapEditorWindow::onFlagsChanged);
    scrollArea->setWidget(m_gridWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setMinimumHeight(300);
    layout->addWidget(scrollArea, 1);

    // Initial flag status update
    onFlagsChanged();
}

void MapEditorWindow::createButtonsGroup()
{
    m_saveButton = new QPushButton("Save Map...");
    connect(m_saveButton, &QPushButton::clicked, this, &MapEditorWindow::onSaveMap);

    m_loadButton = new QPushButton("Load Map...");
    connect(m_loadButton, &QPushButton::clicked, this, &MapEditorWindow::onLoadMap);

    m_clearButton = new QPushButton("Clear Map");
    connect(m_clearButton, &QPushButton::clicked, this, &MapEditorWindow::onClearMap);
}

void MapEditorWindow::onGenerateMap()
{
    int rows = m_rowsSpinBox->value();
    int cols = m_colsSpinBox->value();
    m_gridWidget->setGridSize(rows, cols);
    m_gridWidget->setNumPlayers(m_numPlayersSpinBox->value());
}

void MapEditorWindow::onNumPlayersChanged(int value)
{
    m_gridWidget->setNumPlayers(value);
    onFlagsChanged();
}

void MapEditorWindow::onFlagsChanged()
{
    QList<int> missingFlags = m_gridWidget->getMissingFlags();

    if (missingFlags.isEmpty()) {
        m_flagStatusLabel->setText("All player flags placed!");
        m_flagStatusLabel->setStyleSheet("color: #009900; font-weight: bold;");
        m_saveButton->setEnabled(true);
    } else {
        QStringList missingNames;
        static const QStringList playerNames = {"Red", "Blue", "Green", "Yellow", "Orange", "Black"};
        for (int idx : missingFlags) {
            if (idx >= 0 && idx < playerNames.size()) {
                missingNames.append(QString("Player %1 (%2)").arg(idx + 1).arg(playerNames[idx]));
            }
        }
        m_flagStatusLabel->setText("Missing flags: " + missingNames.join(", "));
        m_flagStatusLabel->setStyleSheet("color: #cc0000; font-weight: bold;");
        m_saveButton->setEnabled(false);
    }
}

void MapEditorWindow::onSaveMap()
{
    // Check if all flags are placed
    if (!m_gridWidget->allFlagsPlaced()) {
        QMessageBox::warning(this, "Cannot Save",
            "Please place all player home flags before saving the map.");
        return;
    }

    // Get last used directory from settings, default to Documents
    QSettings settings;
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString lastDir = settings.value("MapEditor/lastDirectory", defaultDir).toString();

    QString fileName = QFileDialog::getSaveFileName(this,
        "Save Map", lastDir, "JSON Files (*.json)");

    if (fileName.isEmpty()) {
        return;
    }

    // Save the directory for next time
    QFileInfo fileInfo(fileName);
    settings.setValue("MapEditor/lastDirectory", fileInfo.absolutePath());

    if (!fileName.endsWith(".json", Qt::CaseInsensitive)) {
        fileName += ".json";
    }

    QJsonObject root;

    // Map settings
    QJsonObject mapSettings;
    mapSettings["rows"] = m_gridWidget->rows();
    mapSettings["cols"] = m_gridWidget->cols();
    mapSettings["numPlayers"] = m_numPlayersSpinBox->value();
    root["mapSettings"] = mapSettings;

    // Game settings
    QJsonObject gameSettings;
    gameSettings["startingGenerals"] = m_startingGeneralsSpinBox->value();
    gameSettings["startingInfantry"] = m_startingInfantrySpinBox->value();
    gameSettings["startingCavalry"] = m_startingCavalrySpinBox->value();
    gameSettings["startingCatapults"] = m_startingCatapultsSpinBox->value();
    gameSettings["startingMoney"] = m_startingMoneySpinBox->value();
    root["gameSettings"] = gameSettings;

    // Territories (compatible with MapGraph format)
    QJsonArray territoriesArray;
    const auto &cells = m_gridWidget->getAllCells();
    int rows = m_gridWidget->rows();
    int cols = m_gridWidget->cols();
    int cellSize = 100; // Virtual cell size for coordinates

    // Generate random territory names (same as main game)
    QStringList animalNames = {
        "Lion", "Tiger", "Bear", "Wolf", "Eagle", "Hawk", "Falcon", "Owl",
        "Fox", "Deer", "Moose", "Elk", "Bison", "Buffalo", "Zebra", "Giraffe",
        "Elephant", "Rhino", "Hippo", "Crocodile", "Alligator", "Snake", "Cobra", "Viper",
        "Panther", "Leopard", "Cheetah", "Jaguar", "Cougar", "Lynx", "Bobcat", "Ocelot",
        "Monkey", "Gorilla", "Chimp", "Orangutan", "Lemur", "Baboon", "Mandrill", "Gibbon",
        "Rabbit", "Hare", "Squirrel", "Chipmunk", "Raccoon", "Badger", "Weasel", "Ferret",
        "Raven", "Crow", "Parrot", "Peacock", "Swan", "Goose", "Duck", "Crane",
        "Horse", "Stallion", "Mare", "Donkey", "Mule", "Camel", "Llama", "Alpaca",
        "Panda", "Koala", "Sloth", "Armadillo", "Anteater", "Platypus", "Echidna", "Wombat",
        "Kangaroo", "Wallaby", "Opossum", "Skunk", "Porcupine", "Hedgehog", "Mole", "Shrew",
        "Bat", "Condor", "Vulture", "Kite", "Osprey", "Harrier", "Buzzard", "Kestrel"
    };

    QStringList fishNames = {
        "Salmon", "Tuna", "Bass", "Trout", "Pike", "Carp", "Catfish", "Perch",
        "Cod", "Haddock", "Halibut", "Flounder", "Sole", "Mackerel", "Herring", "Sardine",
        "Anchovy", "Barracuda", "Marlin", "Swordfish", "Sailfish", "Mahi", "Grouper", "Snapper",
        "Sturgeon", "Eel", "Lamprey", "Pufferfish", "Angelfish", "Clownfish", "Tang", "Wrasse",
        "Seahorse", "Stingray", "Manta", "Jellyfish", "Octopus", "Squid", "Cuttlefish", "Nautilus",
        "Lobster", "Crab", "Shrimp", "Krill", "Starfish", "Urchin", "Anemone", "Coral"
    };

    // Shuffle both lists for random assignment
    QRandomGenerator *random = QRandomGenerator::global();
    std::shuffle(animalNames.begin(), animalNames.end(), *random);
    std::shuffle(fishNames.begin(), fishNames.end(), *random);

    int animalIndex = 0;
    int fishIndex = 0;

    // First pass: generate all territory names
    QVector<QVector<QString>> territoryNames(rows, QVector<QString>(cols));
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const CellData &cell = cells[r][c];
            if (cell.isLand()) {
                if (animalIndex < animalNames.size()) {
                    territoryNames[r][c] = animalNames[animalIndex++];
                } else {
                    territoryNames[r][c] = QString("Land%1").arg(animalIndex++);
                }
            } else {
                if (fishIndex < fishNames.size()) {
                    territoryNames[r][c] = fishNames[fishIndex++];
                } else {
                    territoryNames[r][c] = QString("Sea%1").arg(fishIndex++);
                }
            }
        }
    }

    // Second pass: build territory objects with neighbors using generated names
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const CellData &cell = cells[r][c];

            QJsonObject territory;

            QString name = territoryNames[r][c];
            territory["name"] = name;

            // Main game compatibility: row, col, isLand
            territory["row"] = r;
            territory["col"] = c;
            territory["isLand"] = cell.isLand();

            // Type: Land or Sea (for MapGraph format)
            territory["type"] = cell.isLand() ? "Land" : "Sea";

            // Territory value (5 or 10 for land, 0 for sea)
            territory["value"] = cell.value();

            // Home player (-1 if none)
            territory["homePlayer"] = cell.homePlayer;

            // Centroid (center of the cell)
            double centroidX = (c + 0.5) * cellSize;
            double centroidY = (r + 0.5) * cellSize;
            territory["centroidX"] = centroidX;
            territory["centroidY"] = centroidY;
            territory["labelX"] = centroidX;
            territory["labelY"] = centroidY;

            // Boundary polygon (rectangle corners)
            QJsonArray boundaryArray;
            QJsonObject p1, p2, p3, p4;
            p1["x"] = c * cellSize;
            p1["y"] = r * cellSize;
            p2["x"] = (c + 1) * cellSize;
            p2["y"] = r * cellSize;
            p3["x"] = (c + 1) * cellSize;
            p3["y"] = (r + 1) * cellSize;
            p4["x"] = c * cellSize;
            p4["y"] = (r + 1) * cellSize;
            boundaryArray.append(p1);
            boundaryArray.append(p2);
            boundaryArray.append(p3);
            boundaryArray.append(p4);
            territory["boundary"] = boundaryArray;

            // Neighbors (4-directional adjacency) using actual territory names
            QJsonArray neighborsArray;
            // Up
            if (r > 0) {
                neighborsArray.append(territoryNames[r - 1][c]);
            }
            // Down
            if (r < rows - 1) {
                neighborsArray.append(territoryNames[r + 1][c]);
            }
            // Left
            if (c > 0) {
                neighborsArray.append(territoryNames[r][c - 1]);
            }
            // Right
            if (c < cols - 1) {
                neighborsArray.append(territoryNames[r][c + 1]);
            }
            territory["neighbors"] = neighborsArray;

            territoriesArray.append(territory);
        }
    }
    root["territories"] = territoriesArray;

    // Create players array for main game compatibility
    QJsonArray playersArray;
    QList<QChar> playerIds = {'A', 'B', 'C', 'D', 'E', 'F'};
    int numPlayers = m_numPlayersSpinBox->value();
    int startingMoney = m_startingMoneySpinBox->value();

    int startingGenerals = m_startingGeneralsSpinBox->value();
    int startingInfantry = m_startingInfantrySpinBox->value();
    int startingCavalry = m_startingCavalrySpinBox->value();
    int startingCatapults = m_startingCatapultsSpinBox->value();
    int serialCounter = 1;

    for (int p = 0; p < numPlayers; ++p) {
        QJsonObject playerObj;
        QString odId = QString(playerIds[p]);
        playerObj["id"] = odId;
        playerObj["wallet"] = startingMoney;

        int homeRow = -1, homeCol = -1;
        QString homeName;

        // Find home territory for this player
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (cells[r][c].homePlayer == p) {
                    homeRow = r;
                    homeCol = c;
                    homeName = territoryNames[r][c];  // Use actual territory name
                    playerObj["homeRow"] = r;
                    playerObj["homeCol"] = c;
                    playerObj["homeName"] = homeName;
                    break;
                }
            }
            if (homeRow >= 0) break;
        }

        // Owned territories - start with home territory
        QJsonArray ownedTerritories;
        ownedTerritories.append(homeName);
        playerObj["ownedTerritories"] = ownedTerritories;

        // Create Caesar at home territory
        QJsonArray caesarsArray;
        QJsonObject caesar;
        caesar["row"] = homeRow;
        caesar["col"] = homeCol;
        caesar["territory"] = homeName;
        caesar["serialNumber"] = QString("C%1").arg(serialCounter++);
        caesar["movesRemaining"] = 0;
        caesar["onGalley"] = "";
        caesarsArray.append(caesar);
        playerObj["caesars"] = caesarsArray;

        // Create Generals at home territory
        QJsonArray generalsArray;
        for (int g = 0; g < startingGenerals; ++g) {
            QJsonObject general;
            general["row"] = homeRow;
            general["col"] = homeCol;
            general["territory"] = homeName;
            general["number"] = g + 1;
            general["serialNumber"] = QString("G%1").arg(serialCounter++);
            general["movesRemaining"] = 0;
            general["onGalley"] = "";
            generalsArray.append(general);
        }
        playerObj["generals"] = generalsArray;

        // Create Infantry at home territory
        QJsonArray infantryArray;
        for (int i = 0; i < startingInfantry; ++i) {
            QJsonObject infantry;
            infantry["row"] = homeRow;
            infantry["col"] = homeCol;
            infantry["territory"] = homeName;
            infantry["serialNumber"] = QString("I%1").arg(serialCounter++);
            infantry["movesRemaining"] = 0;
            infantry["onGalley"] = "";
            infantry["attachedTo"] = "";
            infantryArray.append(infantry);
        }
        playerObj["infantry"] = infantryArray;

        // Create Cavalry at home territory
        QJsonArray cavalryArray;
        for (int c = 0; c < startingCavalry; ++c) {
            QJsonObject cavalry;
            cavalry["row"] = homeRow;
            cavalry["col"] = homeCol;
            cavalry["territory"] = homeName;
            cavalry["serialNumber"] = QString("V%1").arg(serialCounter++);
            cavalry["movesRemaining"] = 0;
            cavalry["onGalley"] = "";
            cavalry["attachedTo"] = "";
            cavalryArray.append(cavalry);
        }
        playerObj["cavalry"] = cavalryArray;

        // Create Catapults at home territory
        QJsonArray catapultsArray;
        for (int t = 0; t < startingCatapults; ++t) {
            QJsonObject catapult;
            catapult["row"] = homeRow;
            catapult["col"] = homeCol;
            catapult["territory"] = homeName;
            catapult["serialNumber"] = QString("T%1").arg(serialCounter++);
            catapult["movesRemaining"] = 0;
            catapult["onGalley"] = "";
            catapult["attachedTo"] = "";
            catapultsArray.append(catapult);
        }
        playerObj["catapults"] = catapultsArray;

        // No galleys initially
        playerObj["galleys"] = QJsonArray();
        playerObj["capturedGenerals"] = QJsonArray();

        playersArray.append(playerObj);
    }
    root["players"] = playersArray;
    root["currentPlayerIndex"] = 0;

    // Write to file
    QJsonDocument doc(root);
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Error", "Could not save file: " + file.errorString());
        return;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    QMessageBox::information(this, "Success", "Map saved successfully!");
}

void MapEditorWindow::onLoadMap()
{
    // Get last used directory from settings, default to Documents
    QSettings settings;
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString lastDir = settings.value("MapEditor/lastDirectory", defaultDir).toString();

    QString fileName = QFileDialog::getOpenFileName(this,
        "Load Map", lastDir, "JSON Files (*.json)");

    if (fileName.isEmpty()) {
        return;
    }

    // Save the directory for next time
    QFileInfo fileInfo(fileName);
    settings.setValue("MapEditor/lastDirectory", fileInfo.absolutePath());

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Error", "Could not open file: " + file.errorString());
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        QMessageBox::critical(this, "Error", "Invalid JSON file.");
        return;
    }

    QJsonObject root = doc.object();

    // Load map settings
    if (root.contains("mapSettings")) {
        QJsonObject mapSettings = root["mapSettings"].toObject();
        int rows = mapSettings["rows"].toInt(8);
        int cols = mapSettings["cols"].toInt(8);
        int numPlayers = mapSettings["numPlayers"].toInt(4);

        m_rowsSpinBox->setValue(rows);
        m_colsSpinBox->setValue(cols);
        m_numPlayersSpinBox->setValue(numPlayers);
        m_gridWidget->setGridSize(rows, cols);
        m_gridWidget->setNumPlayers(numPlayers);
    }

    // Load game settings
    if (root.contains("gameSettings")) {
        QJsonObject gameSettings = root["gameSettings"].toObject();
        m_startingGeneralsSpinBox->setValue(gameSettings["startingGenerals"].toInt(1));
        m_startingInfantrySpinBox->setValue(gameSettings["startingInfantry"].toInt(3));
        m_startingCavalrySpinBox->setValue(gameSettings["startingCavalry"].toInt(2));
        m_startingCatapultsSpinBox->setValue(gameSettings["startingCatapults"].toInt(1));
        m_startingMoneySpinBox->setValue(gameSettings["startingMoney"].toInt(100));
    }

    // Load territories
    if (root.contains("territories")) {
        QJsonArray territoriesArray = root["territories"].toArray();

        for (const QJsonValue &terrVal : territoriesArray) {
            QJsonObject terrObj = terrVal.toObject();

            // Get grid position - try multiple field names for compatibility
            int row = terrObj["row"].toInt(-1);
            int col = terrObj["col"].toInt(-1);

            // Fallback to gridRow/gridCol (old format)
            if (row < 0) row = terrObj["gridRow"].toInt(-1);
            if (col < 0) col = terrObj["gridCol"].toInt(-1);

            // If still not found, parse from name
            if (row < 0 || col < 0) {
                QString name = terrObj["name"].toString();
                // Parse "R{row}_C{col}"
                QRegularExpression rx("R(\\d+)_C(\\d+)");
                QRegularExpressionMatch match = rx.match(name);
                if (match.hasMatch()) {
                    row = match.captured(1).toInt();
                    col = match.captured(2).toInt();
                }
            }

            if (row >= 0 && row < m_gridWidget->rows() &&
                col >= 0 && col < m_gridWidget->cols()) {

                CellData cellData;

                // Determine terrain type - support multiple formats
                bool isLand = true;
                if (terrObj.contains("isLand")) {
                    isLand = terrObj["isLand"].toBool(true);
                } else if (terrObj.contains("type")) {
                    isLand = (terrObj["type"].toString("Land") != "Sea");
                }

                int value = terrObj["value"].toInt(5);

                if (!isLand) {
                    cellData.terrain = TerrainType::Sea;
                } else if (value >= 10) {
                    cellData.terrain = TerrainType::Land10;
                } else {
                    cellData.terrain = TerrainType::Land5;
                }

                cellData.homePlayer = terrObj["homePlayer"].toInt(-1);

                m_gridWidget->setCellData(row, col, cellData);
            }
        }
    }

    QMessageBox::information(this, "Success", "Map loaded successfully!");
}

void MapEditorWindow::onClearMap()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "Clear Map", "Are you sure you want to clear the map?",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_gridWidget->clearGrid();
    }
}
