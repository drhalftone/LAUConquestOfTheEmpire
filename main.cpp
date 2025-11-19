#include "mainwindow.h"
#include "playerinfowidget.h"
#include "mapwidget.h"
#include "player.h"
#include "scorewindow.h"
#include "walletwindow.h"
#include "combatdialog.h"
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QFileDialog>
#include <QStandardPaths>
#include <QSettings>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// Forward declaration
bool loadGameFromFile(const QString &fileName, MapWidget *&mapWidget, QList<Player*> &players, int &currentPlayerIndex);

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Show startup dialog: New Game or Load Game
    QMessageBox startupDialog;
    startupDialog.setWindowTitle("Conquest of the Empire");
    startupDialog.setText("Welcome to Conquest of the Empire!");
    startupDialog.setInformativeText("Would you like to start a new game or load a saved game?");
    startupDialog.setIcon(QMessageBox::Question);

    QPushButton *newGameButton = startupDialog.addButton("New Game", QMessageBox::AcceptRole);
    QPushButton *loadGameButton = startupDialog.addButton("Load Game", QMessageBox::ActionRole);
    QPushButton *exitButton = startupDialog.addButton("Exit", QMessageBox::RejectRole);

    startupDialog.exec();

    QString loadFileName;
    bool loadGame = false;

    if (startupDialog.clickedButton() == loadGameButton) {
        // Get last used directory from settings, default to Documents folder
        QSettings settings("ConquestOfTheEmpire", "MapWidget");
        QString lastDir = settings.value("lastSaveDirectory",
                                         QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();

        loadFileName = QFileDialog::getOpenFileName(nullptr,
                                                    "Load Game",
                                                    lastDir,
                                                    "JSON Files (*.json)");

        if (loadFileName.isEmpty()) {
            // User cancelled file dialog, exit application
            return 0;
        }

        // Save the directory for next time
        QFileInfo fileInfo(loadFileName);
        settings.setValue("lastSaveDirectory", fileInfo.absolutePath());

        loadGame = true;
    } else if (startupDialog.clickedButton() == exitButton) {
        // User chose to exit
        return 0;
    }
    // else: New Game button was clicked, continue with normal initialization

    // Reset the piece counter for a fresh game
    GamePiece::resetCounter();

    MapWidget *mapWidget = nullptr;
    QList<Player*> players;
    int currentPlayerIndex = 0;

    // If loading a game, load from file
    if (loadGame) {
        if (!loadGameFromFile(loadFileName, mapWidget, players, currentPlayerIndex)) {
            QMessageBox::critical(nullptr, "Load Failed",
                                 "Failed to load game from file.\n\n"
                                 "Starting a new game instead.");
            loadGame = false;
        }
    }

    // If not loading or load failed, create new game
    if (!loadGame) {
        // Create the map widget first - it will initialize the random map
        mapWidget = new MapWidget();

        // Get random home provinces from the map
        QVector<MapWidget::HomeProvinceInfo> homeProvinces = mapWidget->getRandomHomeProvinces();

        // Create players with the home provinces from the map
        QList<QChar> playerIds = {'A', 'B', 'C', 'D', 'E', 'F'};

        for (int i = 0; i < 6 && i < homeProvinces.size(); ++i) {
            Player *player = new Player(
                playerIds[i],
                homeProvinces[i].position,
                homeProvinces[i].name
            );
            players.append(player);
        }

        currentPlayerIndex = 0;
    }

    // Give the map a reference to the players so it can query them
    mapWidget->setPlayers(players);
    mapWidget->show();

    // Start the current player's turn
    if (!players.isEmpty() && currentPlayerIndex >= 0 && currentPlayerIndex < players.size()) {
        players[currentPlayerIndex]->startTurn();
        mapWidget->setCurrentPlayerIndex(currentPlayerIndex);
        mapWidget->setAtStartOfTurn(true);  // At the start of the turn
    }

    // FOR TESTING: Move Player A's troops next to Player B for immediate combat testing
    // DISABLED - uncomment to enable combat testing setup
    /*
    if (players.size() >= 2) {
        Player *playerA = players[0];
        Player *playerB = players[1];

        // Get Player B's home position
        Position playerBHome = playerB->getHomeProvince();

        // Calculate adjacent position (one tile to the left of Player B)
        Position adjacentPos = playerBHome;
        adjacentPos.col = (adjacentPos.col > 0) ? adjacentPos.col - 1 : adjacentPos.col + 1;

        // Move Player A's first general and some troops to the adjacent position
        if (playerA->getGenerals().size() > 0) {
            GeneralPiece *general = playerA->getGenerals()[0];

            // Get the adjacent territory name first
            QString adjacentTerritoryName = mapWidget->getTerritoryNameAt(adjacentPos.row, adjacentPos.col);

            // Move general to adjacent position
            general->setPosition(adjacentPos);
            general->setTerritoryName(adjacentTerritoryName);  // Update territory name
            general->setLastTerritory(playerA->getHomeProvince());

            // Move some infantry with the general
            QList<int> legion;
            int troopCount = 0;
            for (InfantryPiece *infantry : playerA->getInfantry()) {
                if (troopCount < 2) {
                    infantry->setPosition(adjacentPos);
                    infantry->setTerritoryName(adjacentTerritoryName);  // Update territory name for infantry too
                    legion.append(infantry->getUniqueId());
                    troopCount++;
                }
            }
            general->setLegion(legion);

            // Claim the adjacent territory for Player A (for testing)
            if (!adjacentTerritoryName.isEmpty() && !playerA->ownsTerritory(adjacentTerritoryName)) {
                playerA->claimTerritory(adjacentTerritoryName);
            }
        }
    }
    */

    // FOR TESTING ROADS: Create a chain of cities for Player A to test road movement
    // ENABLED - comment out to disable road testing setup
    if (players.size() >= 1) {
        Player *playerA = players[0];
        Position playerAHome = playerA->getHomeProvince();

        // Define a chain of territories to the right of Player A's home
        QVector<Position> cityChain;

        // Start from Player A's home and create a horizontal chain of 4 cities
        for (int i = 0; i < 4; ++i) {
            Position cityPos = playerAHome;
            cityPos.col = (playerAHome.col + i) % 12;  // Wrap around if needed
            cityChain.append(cityPos);
        }

        // Add cities and claim territories for Player A
        for (int i = 0; i < cityChain.size(); ++i) {
            Position pos = cityChain[i];
            QString territoryName = mapWidget->getTerritoryNameAt(pos.row, pos.col);

            // Claim the territory
            if (!territoryName.isEmpty() && !playerA->ownsTerritory(territoryName)) {
                playerA->claimTerritory(territoryName);
            }

            // Add a city building at this position
            City *city = new City(playerA->getId(), pos, territoryName);
            playerA->addCity(city);

            qDebug() << "Added city for Player A at" << pos.row << pos.col << territoryName;
        }

        // Also create a vertical branch from the second city to test road network
        Position branchPos1 = cityChain[1];
        branchPos1.row = (branchPos1.row + 1) % 8;  // One tile down
        QString branchName1 = mapWidget->getTerritoryNameAt(branchPos1.row, branchPos1.col);
        if (!branchName1.isEmpty() && !playerA->ownsTerritory(branchName1)) {
            playerA->claimTerritory(branchName1);
        }
        City *branchCity1 = new City(playerA->getId(), branchPos1, branchName1);
        playerA->addCity(branchCity1);
        qDebug() << "Added branch city for Player A at" << branchPos1.row << branchPos1.col << branchName1;

        Position branchPos2 = branchPos1;
        branchPos2.row = (branchPos2.row + 1) % 8;  // One more tile down
        QString branchName2 = mapWidget->getTerritoryNameAt(branchPos2.row, branchPos2.col);
        if (!branchName2.isEmpty() && !playerA->ownsTerritory(branchName2)) {
            playerA->claimTerritory(branchName2);
        }
        City *branchCity2 = new City(playerA->getId(), branchPos2, branchName2);
        playerA->addCity(branchCity2);
        qDebug() << "Added branch city for Player A at" << branchPos2.row << branchPos2.col << branchName2;

        // Update roads to connect all the cities
        mapWidget->updateRoads();

        qDebug() << "Road testing setup complete! Player A now has a network of 6 cities connected by roads.";
        qDebug() << "Right-click on Player A's Caesar or General to see road movement options.";
    }

    // Update the map to show initial territory ownership
    mapWidget->update();

    // Create and show the player info widget
    PlayerInfoWidget *infoWidget = new PlayerInfoWidget();
    infoWidget->setMapWidget(mapWidget);  // Connect to map for territory lookups
    infoWidget->setPlayers(players);
    infoWidget->show();

    // Create score window (kept for backward compatibility but can be removed)
    // Scores are now shown in the MapWidget
    ScoreWindow *scoreWindow = new ScoreWindow();
    scoreWindow->setWindowTitle("Territory Scores");
    // Don't show it by default since scores are in map widget now
    // scoreWindow->show();

    // Wallet window removed - wallets are shown in PlayerInfoWidget
    // Create wallet window (kept for backward compatibility but can be removed)
    WalletWindow *walletWindow = new WalletWindow();
    walletWindow->setWindowTitle("Player Wallets");
    // Don't show it by default since wallets are in player info widget now
    // walletWindow->show();

    // Initialize scores and wallets
    QMap<QChar, int> initialScores;
    QMap<QChar, int> initialWallets;
    for (Player *player : players) {
        // Calculate total tax value for owned territories
        int totalTaxValue = 0;
        const QList<QString> &territories = player->getOwnedTerritories();
        for (const QString &territoryName : territories) {
            for (int row = 0; row < 8; ++row) {
                for (int col = 0; col < 12; ++col) {
                    if (mapWidget->getTerritoryNameAt(row, col) == territoryName) {
                        totalTaxValue += mapWidget->getTerritoryValueAt(row, col);
                        break;
                    }
                }
            }
        }
        // Add 5 for each city owned
        totalTaxValue += player->getCityCount() * 5;
        initialScores[player->getId()] = totalTaxValue;
        initialWallets[player->getId()] = player->getWallet();
    }
    mapWidget->updateScores(initialScores);  // Update scores in map widget
    scoreWindow->updateScores(initialScores);  // Also update separate window if shown
    walletWindow->updateWallets(initialWallets);

    // Connect piece movement signal to map widget for redrawing
    QObject::connect(infoWidget, &PlayerInfoWidget::pieceMoved, mapWidget, [mapWidget](int fromRow, int fromCol, int toRow, int toCol) {
        Q_UNUSED(fromRow);
        Q_UNUSED(fromCol);
        Q_UNUSED(toRow);
        Q_UNUSED(toCol);
        mapWidget->update();  // Redraw the entire map
    });

    // Connect player wallet changes to wallet window
    for (Player *player : players) {
        QObject::connect(player, &Player::walletChanged, walletWindow, [walletWindow, &players](int newAmount) {
            Q_UNUSED(newAmount);
            // Update all players' wallets, not just the one that changed
            QMap<QChar, int> wallets;
            for (Player *p : players) {
                wallets[p->getId()] = p->getWallet();
            }
            walletWindow->updateWallets(wallets);
        });
    }

    // Connect territory changes to score display
    auto updateScores = [scoreWindow, &players, mapWidget]() {
        QMap<QChar, int> scores;
        for (Player *player : players) {
            // Calculate total tax value for owned territories
            int totalTaxValue = 0;
            const QList<QString> &territories = player->getOwnedTerritories();
            for (const QString &territoryName : territories) {
                for (int row = 0; row < 8; ++row) {
                    for (int col = 0; col < 12; ++col) {
                        if (mapWidget->getTerritoryNameAt(row, col) == territoryName) {
                            totalTaxValue += mapWidget->getTerritoryValueAt(row, col);
                            break;
                        }
                    }
                }
            }
            // Add 5 for each city owned
            totalTaxValue += player->getCityCount() * 5;
            scores[player->getId()] = totalTaxValue;
        }
        mapWidget->updateScores(scores);  // Update scores in map widget
        scoreWindow->updateScores(scores);  // Also update separate window if shown
    };

    for (Player *player : players) {
        QObject::connect(player, &Player::territoryClaimed, scoreWindow, updateScores);
        QObject::connect(player, &Player::territoryUnclaimed, scoreWindow, updateScores);
        QObject::connect(player, &Player::buildingAdded, scoreWindow, [updateScores, mapWidget](Building *building) {
            Q_UNUSED(building);
            updateScores();
            mapWidget->updateRoads();  // Check for new roads when buildings added
        });
        QObject::connect(player, &Player::buildingRemoved, scoreWindow, [updateScores](Building *building) {
            Q_UNUSED(building);
            updateScores();
        });

        // Also update roads when territory ownership changes
        QObject::connect(player, &Player::territoryClaimed, mapWidget, [mapWidget]() {
            mapWidget->updateRoads();
        });
        QObject::connect(player, &Player::territoryUnclaimed, mapWidget, [mapWidget]() {
            mapWidget->updateRoads();
        });
    }

    int result = a.exec();

    // Clean up
    qDeleteAll(players);
    delete infoWidget;
    delete mapWidget;
    delete scoreWindow;
    delete walletWindow;

    return result;
}

bool loadGameFromFile(const QString &fileName, MapWidget *&mapWidget, QList<Player*> &players, int &currentPlayerIndex)
{
    // Open and parse JSON file
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        return false;
    }

    QJsonObject gameState = doc.object();

    // Get current player index
    currentPlayerIndex = gameState["currentPlayerIndex"].toInt(0);

    // Create map widget (will initialize with random map, but we'll override it)
    mapWidget = new MapWidget();

    // Clear the random map and restore territories from save file
    mapWidget->clearMap();

    QJsonArray territoriesArray = gameState["territories"].toArray();
    for (const QJsonValue &territoryValue : territoriesArray) {
        QJsonObject territoryObj = territoryValue.toObject();
        int row = territoryObj["row"].toInt(0);
        int col = territoryObj["col"].toInt(0);
        QString name = territoryObj["name"].toString();
        int value = territoryObj["value"].toInt(0);
        bool isLand = territoryObj["isLand"].toBool(true);

        mapWidget->setTerritoryAt(row, col, name, value, isLand);
    }

    // Load players
    QJsonArray playersArray = gameState["players"].toArray();
    for (const QJsonValue &playerValue : playersArray) {
        QJsonObject playerObj = playerValue.toObject();

        QChar playerId = playerObj["id"].toString().at(0);
        int wallet = playerObj["wallet"].toInt(0);
        Position homePos;
        homePos.row = playerObj["homeRow"].toInt(0);
        homePos.col = playerObj["homeCol"].toInt(0);
        QString homeName = playerObj["homeName"].toString();

        // Create player (this will auto-create Caesar, 5 generals, and home city)
        Player *player = new Player(playerId, homePos, homeName);

        // Set wallet
        player->setWallet(wallet);

        // Clear default pieces created by constructor - we'll recreate from save
        // Remove the auto-created pieces
        for (CaesarPiece *caesar : player->getCaesars()) {
            player->removeCaesar(caesar);
            delete caesar;
        }
        for (GeneralPiece *general : player->getGenerals()) {
            player->removeGeneral(general);
            delete general;
        }
        for (InfantryPiece *infantry : player->getInfantry()) {
            player->removeInfantry(infantry);
            delete infantry;
        }

        // Load owned territories
        QJsonArray territoriesArray = playerObj["ownedTerritories"].toArray();
        for (const QJsonValue &territoryValue : territoriesArray) {
            player->claimTerritory(territoryValue.toString());
        }

        // Load Caesars
        QJsonArray caesarsArray = playerObj["caesars"].toArray();
        QMap<QString, CaesarPiece*> caesarMap;  // Track by old serial for legion restoration
        for (const QJsonValue &caesarValue : caesarsArray) {
            QJsonObject caesarObj = caesarValue.toObject();

            Position pos;
            pos.row = caesarObj["row"].toInt(0);
            pos.col = caesarObj["col"].toInt(0);

            CaesarPiece *caesar = new CaesarPiece(playerId, pos, player);
            caesar->setTerritoryName(caesarObj["territory"].toString());
            caesar->setMovesRemaining(caesarObj["movesRemaining"].toInt(0));
            caesar->setOnGalley(caesarObj["onGalley"].toString());

            if (caesarObj.contains("lastTerritoryRow")) {
                Position lastPos;
                lastPos.row = caesarObj["lastTerritoryRow"].toInt(0);
                lastPos.col = caesarObj["lastTerritoryCol"].toInt(0);
                caesar->setLastTerritory(lastPos);
            }

            caesarMap[caesarObj["serialNumber"].toString()] = caesar;
            player->addCaesar(caesar);
        }

        // Load Generals
        QJsonArray generalsArray = playerObj["generals"].toArray();
        QMap<QString, GeneralPiece*> generalMap;
        for (const QJsonValue &generalValue : generalsArray) {
            QJsonObject generalObj = generalValue.toObject();

            Position pos;
            pos.row = generalObj["row"].toInt(0);
            pos.col = generalObj["col"].toInt(0);
            int number = generalObj["number"].toInt(1);

            GeneralPiece *general = new GeneralPiece(playerId, pos, number, player);
            general->setTerritoryName(generalObj["territory"].toString());
            general->setMovesRemaining(generalObj["movesRemaining"].toInt(0));
            general->setOnGalley(generalObj["onGalley"].toString());

            if (generalObj.contains("lastTerritoryRow")) {
                Position lastPos;
                lastPos.row = generalObj["lastTerritoryRow"].toInt(0);
                lastPos.col = generalObj["lastTerritoryCol"].toInt(0);
                general->setLastTerritory(lastPos);
            }

            generalMap[generalObj["serialNumber"].toString()] = general;
            player->addGeneral(general);
        }

        // Load Captured Generals
        QJsonArray capturedGeneralsArray = playerObj["capturedGenerals"].toArray();
        for (const QJsonValue &generalValue : capturedGeneralsArray) {
            QJsonObject generalObj = generalValue.toObject();

            Position pos;
            pos.row = generalObj["row"].toInt(0);
            pos.col = generalObj["col"].toInt(0);
            int number = generalObj["number"].toInt(1);
            QChar originalPlayer = generalObj["originalPlayer"].toString().at(0);

            GeneralPiece *general = new GeneralPiece(originalPlayer, pos, number, player);
            general->setTerritoryName(generalObj["territory"].toString());
            general->setMovesRemaining(generalObj["movesRemaining"].toInt(0));
            general->setOnGalley(generalObj["onGalley"].toString());
            general->setCapturedBy(playerId);

            player->addCapturedGeneral(general);
        }

        // Load Infantry
        QJsonArray infantryArray = playerObj["infantry"].toArray();
        QMap<QString, InfantryPiece*> infantryMap;
        for (const QJsonValue &infantryValue : infantryArray) {
            QJsonObject infantryObj = infantryValue.toObject();

            Position pos;
            pos.row = infantryObj["row"].toInt(0);
            pos.col = infantryObj["col"].toInt(0);

            InfantryPiece *infantry = new InfantryPiece(playerId, pos, player);
            infantry->setTerritoryName(infantryObj["territory"].toString());
            infantry->setMovesRemaining(infantryObj["movesRemaining"].toInt(0));
            infantry->setOnGalley(infantryObj["onGalley"].toString());

            infantryMap[infantryObj["serialNumber"].toString()] = infantry;
            player->addInfantry(infantry);
        }

        // Load Cavalry
        QJsonArray cavalryArray = playerObj["cavalry"].toArray();
        QMap<QString, CavalryPiece*> cavalryMap;
        for (const QJsonValue &cavalryValue : cavalryArray) {
            QJsonObject cavalryObj = cavalryValue.toObject();

            Position pos;
            pos.row = cavalryObj["row"].toInt(0);
            pos.col = cavalryObj["col"].toInt(0);

            CavalryPiece *cavalry = new CavalryPiece(playerId, pos, player);
            cavalry->setTerritoryName(cavalryObj["territory"].toString());
            cavalry->setMovesRemaining(cavalryObj["movesRemaining"].toInt(0));
            cavalry->setOnGalley(cavalryObj["onGalley"].toString());

            cavalryMap[cavalryObj["serialNumber"].toString()] = cavalry;
            player->addCavalry(cavalry);
        }

        // Load Catapults
        QJsonArray catapultsArray = playerObj["catapults"].toArray();
        QMap<QString, CatapultPiece*> catapultMap;
        for (const QJsonValue &catapultValue : catapultsArray) {
            QJsonObject catapultObj = catapultValue.toObject();

            Position pos;
            pos.row = catapultObj["row"].toInt(0);
            pos.col = catapultObj["col"].toInt(0);

            CatapultPiece *catapult = new CatapultPiece(playerId, pos, player);
            catapult->setTerritoryName(catapultObj["territory"].toString());
            catapult->setMovesRemaining(catapultObj["movesRemaining"].toInt(0));
            catapult->setOnGalley(catapultObj["onGalley"].toString());

            catapultMap[catapultObj["serialNumber"].toString()] = catapult;
            player->addCatapult(catapult);
        }

        // Load Galleys
        QJsonArray galleysArray = playerObj["galleys"].toArray();
        QMap<QString, GalleyPiece*> galleyMap;
        for (const QJsonValue &galleyValue : galleysArray) {
            QJsonObject galleyObj = galleyValue.toObject();

            Position pos;
            pos.row = galleyObj["row"].toInt(0);
            pos.col = galleyObj["col"].toInt(0);

            GalleyPiece *galley = new GalleyPiece(playerId, pos, player);
            galley->setTerritoryName(galleyObj["territory"].toString());
            galley->setMovesRemaining(galleyObj["movesRemaining"].toInt(0));

            if (galleyObj.contains("lastTerritoryRow")) {
                Position lastPos;
                lastPos.row = galleyObj["lastTerritoryRow"].toInt(0);
                lastPos.col = galleyObj["lastTerritoryCol"].toInt(0);
                galley->setLastTerritory(lastPos);
            }

            galleyMap[galleyObj["serialNumber"].toString()] = galley;
            player->addGalley(galley);
        }

        // Restore legions for Caesars (note: we can't use saved piece IDs since pieces have new IDs)
        // We need to rebuild legions based on position matching
        // This is a limitation - we'll restore legion composition by finding pieces at the same location
        for (const QJsonValue &caesarValue : caesarsArray) {
            QJsonObject caesarObj = caesarValue.toObject();
            CaesarPiece *caesar = caesarMap[caesarObj["serialNumber"].toString()];

            if (caesar) {
                // Find all troops at same position as Caesar
                Position caesarPos = caesar->getPosition();
                QList<int> legion;

                for (InfantryPiece *troop : player->getInfantry()) {
                    if (troop->getPosition().row == caesarPos.row && troop->getPosition().col == caesarPos.col) {
                        legion.append(troop->getUniqueId());
                    }
                }
                for (CavalryPiece *troop : player->getCavalry()) {
                    if (troop->getPosition().row == caesarPos.row && troop->getPosition().col == caesarPos.col) {
                        legion.append(troop->getUniqueId());
                    }
                }
                for (CatapultPiece *troop : player->getCatapults()) {
                    if (troop->getPosition().row == caesarPos.row && troop->getPosition().col == caesarPos.col) {
                        legion.append(troop->getUniqueId());
                    }
                }

                caesar->setLegion(legion);
            }
        }

        // Restore legions for Generals
        for (const QJsonValue &generalValue : generalsArray) {
            QJsonObject generalObj = generalValue.toObject();
            GeneralPiece *general = generalMap[generalObj["serialNumber"].toString()];

            if (general) {
                Position generalPos = general->getPosition();
                QList<int> legion;

                for (InfantryPiece *troop : player->getInfantry()) {
                    if (troop->getPosition().row == generalPos.row && troop->getPosition().col == generalPos.col) {
                        legion.append(troop->getUniqueId());
                    }
                }
                for (CavalryPiece *troop : player->getCavalry()) {
                    if (troop->getPosition().row == generalPos.row && troop->getPosition().col == generalPos.col) {
                        legion.append(troop->getUniqueId());
                    }
                }
                for (CatapultPiece *troop : player->getCatapults()) {
                    if (troop->getPosition().row == generalPos.row && troop->getPosition().col == generalPos.col) {
                        legion.append(troop->getUniqueId());
                    }
                }

                general->setLegion(legion);
            }
        }

        // Restore legions for Galleys
        for (const QJsonValue &galleyValue : galleysArray) {
            QJsonObject galleyObj = galleyValue.toObject();
            GalleyPiece *galley = galleyMap[galleyObj["serialNumber"].toString()];

            if (galley) {
                Position galleyPos = galley->getPosition();
                QList<int> legion;

                // Add all pieces on this galley
                for (CaesarPiece *piece : player->getCaesars()) {
                    if (piece->getPosition().row == galleyPos.row && piece->getPosition().col == galleyPos.col &&
                        !piece->getOnGalley().isEmpty()) {
                        legion.append(piece->getUniqueId());
                    }
                }
                for (GeneralPiece *piece : player->getGenerals()) {
                    if (piece->getPosition().row == galleyPos.row && piece->getPosition().col == galleyPos.col &&
                        !piece->getOnGalley().isEmpty()) {
                        legion.append(piece->getUniqueId());
                    }
                }
                for (InfantryPiece *troop : player->getInfantry()) {
                    if (troop->getPosition().row == galleyPos.row && troop->getPosition().col == galleyPos.col &&
                        !troop->getOnGalley().isEmpty()) {
                        legion.append(troop->getUniqueId());
                    }
                }
                for (CavalryPiece *troop : player->getCavalry()) {
                    if (troop->getPosition().row == galleyPos.row && troop->getPosition().col == galleyPos.col &&
                        !troop->getOnGalley().isEmpty()) {
                        legion.append(troop->getUniqueId());
                    }
                }
                for (CatapultPiece *troop : player->getCatapults()) {
                    if (troop->getPosition().row == galleyPos.row && troop->getPosition().col == galleyPos.col &&
                        !troop->getOnGalley().isEmpty()) {
                        legion.append(troop->getUniqueId());
                    }
                }

                galley->setLegion(legion);
            }
        }

        // Load Cities
        QJsonArray citiesArray = playerObj["cities"].toArray();
        for (const QJsonValue &cityValue : citiesArray) {
            QJsonObject cityObj = cityValue.toObject();

            Position pos;
            pos.row = cityObj["row"].toInt(0);
            pos.col = cityObj["col"].toInt(0);
            QString territory = cityObj["territory"].toString();
            bool isFortified = cityObj["isFortified"].toBool(false);

            City *city = new City(playerId, pos, territory, isFortified, player);
            player->addCity(city);
        }

        // Load Roads
        QJsonArray roadsArray = playerObj["roads"].toArray();
        for (const QJsonValue &roadValue : roadsArray) {
            QJsonObject roadObj = roadValue.toObject();

            Position fromPos;
            fromPos.row = roadObj["fromRow"].toInt(0);
            fromPos.col = roadObj["fromCol"].toInt(0);

            Position toPos;
            toPos.row = roadObj["toRow"].toInt(0);
            toPos.col = roadObj["toCol"].toInt(0);

            // Validate that neither endpoint is a sea territory
            if (mapWidget->isSeaTerritory(fromPos.row, fromPos.col) ||
                mapWidget->isSeaTerritory(toPos.row, toPos.col)) {
                qDebug() << "Skipping road from sea territory: (" << fromPos.row << "," << fromPos.col
                         << ") to (" << toPos.row << "," << toPos.col << ")";
                continue;  // Skip this road
            }

            // Road constructor takes one position, we'll set the "to" position after
            Road *road = new Road(playerId, fromPos, "", player);
            road->setToPosition(toPos);
            player->addRoad(road);
        }

        players.append(player);
    }

    return true;
}
