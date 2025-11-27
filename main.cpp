#include "playerinfowidget.h"
#ifdef USE_OPENGL_MAP
#include "gamemapwidget.h"
#else
#include "mapwidget.h"
#endif
#include "player.h"
#include "scorewindow.h"
#include "walletwindow.h"
#include "combatdialog.h"
#include "aiplayer.h"
#include "aidebugwidget.h"
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
#include <QSet>
#include <QInputDialog>
#include <QSurfaceFormat>

// Home provinces by player count (from Conquest of the Empire Classic Rules)
// Six provinces: Hispania, Italia, Macedonia, Numidia, Egyptus, Galatia
// Turn order follows clockwise around Mediterranean: Macedonia, Galatia, Egyptus, Numidia, Hispania, Italia
// Provinces are ordered clockwise - first in list goes first
QStringList getHomeProvincesForPlayerCount(int numPlayers)
{
    switch (numPlayers) {
        case 2:
            // Egyptus goes first (clockwise before Hispania)
            return {"Egyptus", "Hispania"};
        case 3:
            // Macedonia, then clockwise: Egyptus, Hispania
            return {"Macedonia", "Egyptus", "Hispania"};
        case 4:
            // Ordered clockwise: Macedonia, Galatia, Numidia, Hispania
            return {"Macedonia", "Galatia", "Numidia", "Hispania"};
        case 5:
            // Ordered clockwise: Macedonia, Galatia, Egyptus, Hispania, Italia
            return {"Macedonia", "Galatia", "Egyptus", "Hispania", "Italia"};
        case 6:
            // All six provinces ordered clockwise: Macedonia, Galatia, Egyptus, Numidia, Hispania, Italia
            return {"Macedonia", "Galatia", "Egyptus", "Numidia", "Hispania", "Italia"};
        default:
            return {"Egyptus", "Hispania"};  // Default to 2 players
    }
}

// Forward declaration
#ifdef USE_OPENGL_MAP
bool loadGameFromFile(const QString &fileName, GameMapWidget *&mapWidget, QList<Player*> &players, int &currentPlayerIndex);
#else
bool loadGameFromFile(const QString &fileName, MapWidget *&mapWidget, QList<Player*> &players, int &currentPlayerIndex);
#endif

int main(int argc, char *argv[])
{
    // Set OpenGL version to 3.3 Core Profile (required for GLSL 330 shaders)
#ifdef USE_OPENGL_MAP
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
#endif

    QApplication a(argc, argv);

    // Set application icon
    a.setWindowIcon(QIcon(":/images/coeIcon.png"));

    // Show startup dialog: New Game, Single Player, or Load Game
    QMessageBox startupDialog;
    startupDialog.setWindowTitle("Conquest of the Empire");
    startupDialog.setText("Welcome to Conquest of the Empire!");
    startupDialog.setInformativeText("Would you like to start a new game or load a saved game?");
    startupDialog.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QPushButton *newGameButton = startupDialog.addButton("New Game", QMessageBox::AcceptRole);
    QPushButton *singlePlayerButton = startupDialog.addButton("Single Player", QMessageBox::AcceptRole);
    QPushButton *loadGameButton = startupDialog.addButton("Load Game", QMessageBox::ActionRole);
    QPushButton *exitButton = startupDialog.addButton("Exit", QMessageBox::RejectRole);

    startupDialog.exec();

    QString loadFileName;
    bool loadGame = false;
    bool singlePlayerMode = false;
    int humanPlayerIndex = 0;  // Index of the human player (0 = Macedonia in single player)

    int numPlayers = 2;  // Default

    if (startupDialog.clickedButton() == singlePlayerButton) {
        // Single player TEST mode - you play as Macedonia with AI opponents, no combat
        singlePlayerMode = true;
        numPlayers = 3;  // Macedonia + 2 AI opponents (Egyptus, Hispania)

        QMessageBox::information(nullptr, "Test Mode",
            "Starting TEST MODE:\n\n"
            "- You play as Macedonia (Red)\n"
            "- 2 AI opponents (Egyptus, Hispania)\n"
            "- Combat is DISABLED for testing movement and roads\n\n"
            "Use this to test movement, road drawing, and AI behavior.");

        qDebug() << "Starting single player TEST mode with 3 players (combat disabled)";
    } else if (startupDialog.clickedButton() == loadGameButton) {
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
    } else if (startupDialog.clickedButton() == newGameButton) {
        // Ask for number of players
        QStringList playerOptions = {"2 Players", "3 Players", "4 Players", "5 Players", "6 Players"};
        bool ok;
        QString selection = QInputDialog::getItem(nullptr,
                                                  "New Game",
                                                  "Select number of players:",
                                                  playerOptions,
                                                  0,  // Default to 2 players
                                                  false,  // Not editable
                                                  &ok);
        if (!ok) {
            // User cancelled, exit application
            return 0;
        }

        // Parse selection to get number
        numPlayers = selection.left(1).toInt();
        qDebug() << "Starting new game with" << numPlayers << "players";
    } else if (startupDialog.clickedButton() == exitButton) {
        // User chose to exit
        return 0;
    }

    // Reset the piece counter for a fresh game
    GamePiece::resetCounter();

#ifdef USE_OPENGL_MAP
    GameMapWidget *mapWidget = nullptr;
#else
    MapWidget *mapWidget = nullptr;
#endif
    QList<Player*> players;
    int currentPlayerIndex = 0;

    // If loading a game, load from file
    if (loadGame) {
        if (!loadGameFromFile(loadFileName, mapWidget, players, currentPlayerIndex)) {
            QMessageBox msgBox;
            msgBox.setWindowTitle("Load Failed");
            msgBox.setText("Failed to load game from file.\n\n"
                           "Starting a new game instead.");
            msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            msgBox.exec();
            loadGame = false;
        }
    }

    // If not loading or load failed, create new game
    if (!loadGame) {
        // Create the map widget
#ifdef USE_OPENGL_MAP
        mapWidget = new GameMapWidget();
#else
        mapWidget = new MapWidget();
#endif

        // Get home provinces for the selected number of players
        QStringList homeProvinces = getHomeProvincesForPlayerCount(numPlayers);

        // Create players with the correct starting territories
        QList<QChar> playerIds = {'A', 'B', 'C', 'D', 'E', 'F'};

        for (int i = 0; i < numPlayers && i < homeProvinces.size(); ++i) {
            Player *player = new Player(
                playerIds[i],
                homeProvinces[i]  // Home province name from rules
            );
            players.append(player);
            qDebug() << "Player" << playerIds[i] << "starts in" << homeProvinces[i];
        }

        currentPlayerIndex = 0;
    }

    // Give the map a reference to the players so it can query them
    mapWidget->setPlayers(players);
    mapWidget->show();
    mapWidget->raise();
    mapWidget->activateWindow();

    // NOTE: We delay startTurn() until after AI setup so the signal connection exists
    // Just set up the current player index for now
    if (!players.isEmpty() && currentPlayerIndex >= 0 && currentPlayerIndex < players.size()) {
        if (loadGame) {
            // Loaded game - set turn state without resetting movement points
            players[currentPlayerIndex]->setMyTurn(true);
        }
        mapWidget->setCurrentPlayerIndex(currentPlayerIndex);
    }

    // Update the map to show initial territory ownership
    mapWidget->update();

#ifdef USE_OPENGL_MAP
    // OpenGL map - with PlayerInfoWidget for game controls
    PlayerInfoWidget *infoWidget = new PlayerInfoWidget();
    infoWidget->setMapWidget(mapWidget);  // Connect to map for territory lookups
    infoWidget->setPlayers(players);
    mapWidget->setPlayerInfoWidget(infoWidget);  // Connect map to info widget
    infoWidget->setAttribute(Qt::WA_QuitOnClose, false);  // Don't quit app when this closes

    // Disable combat in single player test mode
    if (singlePlayerMode) {
        infoWidget->setCombatDisabled(true);
        qDebug() << "Combat DISABLED for test mode";
    }

    // Connect mapWidget close to infoWidget close
    QObject::connect(mapWidget, &QWidget::destroyed, infoWidget, &QWidget::deleteLater);

    infoWidget->show();

    ScoreWindow *scoreWindow = nullptr;
    WalletWindow *walletWindow = nullptr;
#else
    // Create and show the player info widget
    PlayerInfoWidget *infoWidget = new PlayerInfoWidget();
    infoWidget->setMapWidget(mapWidget);  // Connect to map for territory lookups
    infoWidget->setPlayers(players);
    mapWidget->setPlayerInfoWidget(infoWidget);  // Connect map to info widget for right-click movement
    infoWidget->setAttribute(Qt::WA_QuitOnClose, false);  // Don't quit app when this closes

    // Connect mapWidget close to infoWidget close
    QObject::connect(mapWidget, &QWidget::destroyed, infoWidget, &QWidget::deleteLater);

    infoWidget->show();

    // Create score window (kept for backward compatibility but can be removed)
    // Scores are now shown in the MapWidget
    ScoreWindow *scoreWindow = new ScoreWindow(players.size());
    scoreWindow->setWindowTitle("Territory Scores");
    // Don't show it by default since scores are in map widget now
    // scoreWindow->show();

    // Wallet window removed - wallets are shown in PlayerInfoWidget
    // Create wallet window (kept for backward compatibility but can be removed)
    WalletWindow *walletWindow = new WalletWindow();
    walletWindow->setWindowTitle("Player Wallets");
    // Don't show it by default since wallets are in player info widget now
    // walletWindow->show();
#endif

#ifndef USE_OPENGL_MAP
    // Initialize scores and wallets (grid-based only)
    QMap<QChar, int> initialScores;
    QMap<QChar, int> initialWallets;
    for (Player *player : players) {
        // Calculate total tax value for owned territories using MapGraph
        int totalTaxValue = 0;
        const QList<QString> &territories = player->getOwnedTerritories();
        for (const QString &territoryName : territories) {
            totalTaxValue += mapWidget->getGraph()->getValue(territoryName);
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
                // Use MapGraph for territory values (works for both grid and OpenGL)
                totalTaxValue += mapWidget->getGraph()->getValue(territoryName);
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
        QObject::connect(player, &Player::buildingAdded, scoreWindow, [updateScores](Building *building) {
            Q_UNUSED(building);
            updateScores();
            // Note: Roads are updated at start of turn, not when buildings change ownership
        });
        QObject::connect(player, &Player::buildingRemoved, scoreWindow, [updateScores](Building *building) {
            Q_UNUSED(building);
            updateScores();
        });

        // Note: Roads are updated at start of turn, not when territory ownership changes mid-turn
        // This prevents roads from appearing before combat is resolved
    }
#endif // !USE_OPENGL_MAP

#ifndef USE_OPENGL_MAP
    // ========================================================================
    // AI PLAYER SETUP (for testing) - grid-based only
    // ========================================================================
    // DISABLED: AI players are currently disabled for manual play
    QList<AIPlayer*> aiPlayers;
    QList<AIDebugWidget*> debugWidgets;

    // Create AI controller and debug widget for each player
    /*
    for (int i = 0; i < players.size(); ++i) {
        Player *player = players[i];

        // Create AI player controller
        AIPlayer *ai = new AIPlayer(player, infoWidget, mapWidget);
        ai->setStrategy(AIPlayer::Strategy::Economic);  // Prioritize highest value territories
        ai->setDelayMs(1000);   // 1 second delay so you can see dialogs
        ai->setStepMode(true);  // Step mode - click "Step" button to advance
        aiPlayers.append(ai);

        // Register AI player with PlayerInfoWidget for combat handling
        infoWidget->registerAIPlayer(player->getId(), ai);

        // Create debug widget for this AI
        AIDebugWidget *debugWidget = new AIDebugWidget();
        debugWidget->setAIPlayer(ai);
        debugWidget->move(900 + i * 50, 100 + i * 50);  // Offset each window slightly
        debugWidget->show();
        debugWidgets.append(debugWidget);

        // Connect player's turn signal to AI execution
        // When player's turn starts, trigger the AI
        QObject::connect(player, &Player::turnStarted, ai, &AIPlayer::executeTurn);

        qDebug() << "Created AI player and debug widget for Player" << player->getId();
    }
    */

    // NOW start the current player's turn (after AI connections are set up)
    // This applies to both new games and loaded games
    if (!players.isEmpty() && currentPlayerIndex >= 0 && currentPlayerIndex < players.size()) {
        qDebug() << "Starting player's turn (Player" << players[currentPlayerIndex]->getId() << ")";
        players[currentPlayerIndex]->startTurn();
        mapWidget->setAtStartOfTurn(true);
    }

    int result = a.exec();

    // Clean up
    qDeleteAll(aiPlayers);
    qDeleteAll(debugWidgets);
    qDeleteAll(players);
    delete infoWidget;
    delete mapWidget;
    delete scoreWindow;
    delete walletWindow;

    return result;
#else
    // ========================================================================
    // OpenGL MAP - with PlayerInfoWidget
    // ========================================================================

    // AI players for single player mode
    QList<AIPlayer*> aiPlayers;
    QList<AIDebugWidget*> debugWidgets;

    if (singlePlayerMode) {
        qDebug() << "Setting up AI players for single player mode...";

        // Create AI controller for each non-human player
        for (int i = 0; i < players.size(); ++i) {
            if (i == humanPlayerIndex) {
                qDebug() << "Player" << players[i]->getId() << "(" << players[i]->getHomeProvinceName() << ") is HUMAN";
                continue;  // Skip human player
            }

            Player *player = players[i];

            // Create AI player controller
            AIPlayer *ai = new AIPlayer(player, infoWidget, mapWidget);
            ai->setStrategy(AIPlayer::Strategy::Economic);  // Prioritize expansion and economy
            ai->setDelayMs(800);   // Delay so player can see AI actions
            ai->setStepMode(false);  // Auto-run, no stepping
            aiPlayers.append(ai);

            // Register AI player with PlayerInfoWidget for combat handling
            infoWidget->registerAIPlayer(player->getId(), ai);

            // Connect player's turn signal to AI execution
            QObject::connect(player, &Player::turnStarted, ai, &AIPlayer::executeTurn);

            qDebug() << "Player" << player->getId() << "(" << player->getHomeProvinceName() << ") is AI-controlled";

            // Optionally create debug widget for AI (uncomment to see AI decision-making)
            /*
            AIDebugWidget *debugWidget = new AIDebugWidget();
            debugWidget->setAIPlayer(ai);
            debugWidget->move(900 + i * 50, 100 + i * 50);
            debugWidget->show();
            debugWidgets.append(debugWidget);
            */
        }
    }

    // Start the current player's turn
    if (!players.isEmpty() && currentPlayerIndex >= 0 && currentPlayerIndex < players.size()) {
        qDebug() << "Starting player's turn (Player" << players[currentPlayerIndex]->getId() << ")";
        players[currentPlayerIndex]->startTurn();
        mapWidget->setAtStartOfTurn(true);
    }

    int result = a.exec();

    // Clean up
    qDeleteAll(aiPlayers);
    qDeleteAll(debugWidgets);
    qDeleteAll(players);
    delete infoWidget;
    delete mapWidget;

    return result;
#endif // !USE_OPENGL_MAP
}

#ifdef USE_OPENGL_MAP
// OpenGL map doesn't support loading grid-based save files yet
bool loadGameFromFile(const QString &fileName, GameMapWidget *&mapWidget, QList<Player*> &players, int &currentPlayerIndex)
{
    // Open and parse JSON file
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file:" << fileName;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "Failed to parse JSON:" << parseError.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qDebug() << "JSON is not an object";
        return false;
    }

    QJsonObject gameState = doc.object();

    // Get current player index
    currentPlayerIndex = gameState["currentPlayerIndex"].toInt(0);

    // Create map widget
    mapWidget = new GameMapWidget();

    // Load players
    QJsonArray playersArray = gameState["players"].toArray();
    qDebug() << "Loading" << playersArray.size() << "players from save file";

    for (const QJsonValue &playerValue : playersArray) {
        QJsonObject playerObj = playerValue.toObject();

        QChar playerId = playerObj["id"].toString().at(0);
        QString homeName = playerObj["homeName"].toString();
        int wallet = playerObj["wallet"].toInt(100);

        qDebug() << "Creating player" << playerId << "with home" << homeName;

        // Create player with minimal setup (we'll recreate pieces from save)
        Player *player = new Player(playerId, homeName, nullptr, true);  // minimalSetup=true

        // Remove the auto-created Caesar (constructor always creates one)
        for (CaesarPiece *caesar : player->getCaesars()) {
            player->removeCaesar(caesar);
            delete caesar;
        }

        // Set wallet
        player->setWallet(wallet);

        // Load owned territories
        QJsonArray territoriesArray = playerObj["ownedTerritories"].toArray();
        for (const QJsonValue &territoryValue : territoriesArray) {
            player->claimTerritory(territoryValue.toString());
        }

        // Load Caesars
        QJsonArray caesarsArray = playerObj["caesars"].toArray();
        for (const QJsonValue &caesarValue : caesarsArray) {
            QJsonObject caesarObj = caesarValue.toObject();
            QString territory = caesarObj["territory"].toString();

            CaesarPiece *caesar = new CaesarPiece(playerId, {0, 0}, player);
            caesar->setTerritoryName(territory);
            caesar->setMovesRemaining(caesarObj["movesRemaining"].toDouble(2));
            caesar->setOnGalley(caesarObj["onGalley"].toString());

            player->addCaesar(caesar);
            qDebug() << "  Added Caesar at" << territory;
        }

        // Load Generals
        QJsonArray generalsArray = playerObj["generals"].toArray();
        for (const QJsonValue &generalValue : generalsArray) {
            QJsonObject generalObj = generalValue.toObject();
            QString territory = generalObj["territory"].toString();
            int number = generalObj["number"].toInt(1);

            GeneralPiece *general = new GeneralPiece(playerId, {0, 0}, number, player);
            general->setTerritoryName(territory);
            general->setMovesRemaining(generalObj["movesRemaining"].toDouble(2));
            general->setOnGalley(generalObj["onGalley"].toString());

            player->addGeneral(general);
        }
        qDebug() << "  Added" << player->getGeneralCount() << "generals";

        // Load Infantry
        QJsonArray infantryArray = playerObj["infantry"].toArray();
        for (const QJsonValue &infantryValue : infantryArray) {
            QJsonObject infantryObj = infantryValue.toObject();
            QString territory = infantryObj["territory"].toString();

            InfantryPiece *infantry = new InfantryPiece(playerId, {0, 0}, player);
            infantry->setTerritoryName(territory);
            infantry->setMovesRemaining(infantryObj["movesRemaining"].toDouble(1));
            infantry->setOnGalley(infantryObj["onGalley"].toString());

            player->addInfantry(infantry);
        }
        qDebug() << "  Added" << player->getInfantryCount() << "infantry";

        // Load Cavalry
        QJsonArray cavalryArray = playerObj["cavalry"].toArray();
        for (const QJsonValue &cavalryValue : cavalryArray) {
            QJsonObject cavalryObj = cavalryValue.toObject();
            QString territory = cavalryObj["territory"].toString();

            CavalryPiece *cavalry = new CavalryPiece(playerId, {0, 0}, player);
            cavalry->setTerritoryName(territory);
            cavalry->setMovesRemaining(cavalryObj["movesRemaining"].toDouble(2));
            cavalry->setOnGalley(cavalryObj["onGalley"].toString());

            player->addCavalry(cavalry);
        }

        // Load Catapults
        QJsonArray catapultsArray = playerObj["catapults"].toArray();
        for (const QJsonValue &catapultValue : catapultsArray) {
            QJsonObject catapultObj = catapultValue.toObject();
            QString territory = catapultObj["territory"].toString();

            CatapultPiece *catapult = new CatapultPiece(playerId, {0, 0}, player);
            catapult->setTerritoryName(territory);
            catapult->setMovesRemaining(catapultObj["movesRemaining"].toDouble(1));
            catapult->setOnGalley(catapultObj["onGalley"].toString());

            player->addCatapult(catapult);
        }

        // Load Galleys
        QJsonArray galleysArray = playerObj["galleys"].toArray();
        for (const QJsonValue &galleyValue : galleysArray) {
            QJsonObject galleyObj = galleyValue.toObject();
            QString territory = galleyObj["territory"].toString();

            GalleyPiece *galley = new GalleyPiece(playerId, {0, 0}, player);
            galley->setTerritoryName(territory);
            galley->setMovesRemaining(galleyObj["movesRemaining"].toDouble(2));
            if (galleyObj["transportedThisTurn"].toBool()) {
                galley->setTransportedThisTurn(true);
            }
            if (galleyObj.contains("lastSeaZone")) {
                galley->setLastSeaZone(galleyObj["lastSeaZone"].toString());
            }

            player->addGalley(galley);
        }

        // Load Cities
        QJsonArray citiesArray = playerObj["cities"].toArray();
        for (const QJsonValue &cityValue : citiesArray) {
            QJsonObject cityObj = cityValue.toObject();
            QString territory = cityObj["territory"].toString();
            bool isFortified = cityObj["isFortified"].toBool();

            City *city = new City(playerId, {0, 0}, territory, isFortified, player);
            player->addCity(city);
        }
        qDebug() << "  Added" << player->getCityCount() << "cities";

        // Load Captured Generals
        QJsonArray capturedGeneralsArray = playerObj["capturedGenerals"].toArray();
        for (const QJsonValue &generalValue : capturedGeneralsArray) {
            QJsonObject generalObj = generalValue.toObject();
            QString territory = generalObj["territory"].toString();
            QChar originalPlayer = generalObj["originalPlayer"].toString().at(0);
            int number = generalObj["number"].toInt(1);

            GeneralPiece *general = new GeneralPiece(originalPlayer, {0, 0}, number, player);
            general->setTerritoryName(territory);
            general->setCapturedBy(playerId);

            player->addCapturedGeneral(general);
        }

        players.append(player);
    }

    // Clear invalid onGalley references (old serial numbers that don't exist anymore)
    qDebug() << "Clearing invalid galley references...";
    for (Player *player : players) {
        for (CaesarPiece *caesar : player->getCaesars()) {
            caesar->clearGalley();
        }
        for (GeneralPiece *general : player->getGenerals()) {
            general->clearGalley();
        }
        for (InfantryPiece *infantry : player->getInfantry()) {
            infantry->clearGalley();
        }
        for (CavalryPiece *cavalry : player->getCavalry()) {
            cavalry->clearGalley();
        }
        for (CatapultPiece *catapult : player->getCatapults()) {
            catapult->clearGalley();
        }
        for (GalleyPiece *galley : player->getGalleys()) {
            galley->setLeaderAboard(0);  // Clear invalid leader reference
        }
    }

    // Rebuild galley-leader relationships and legion membership
    // Leaders in sea territories should be on galleys in the same territory
    // Troops in sea territories should be in the legion of a leader in the same territory
    qDebug() << "Rebuilding galley-leader relationships and legions...";
    for (Player *player : players) {
        qDebug() << "  Player" << player->getId() << "has" << player->getGalleys().size() << "galleys";

        // Check caesars
        for (CaesarPiece *caesar : player->getCaesars()) {
            QString territory = caesar->getTerritoryName();
            // Check if territory is a sea zone (starts with "Mare" or "Oceanus")
            if (territory.startsWith("Mare") || territory.startsWith("Oceanus")) {
                // Find a galley in the same territory
                for (GalleyPiece *galley : player->getGalleys()) {
                    if (galley->getTerritoryName() == territory && !galley->hasLeaderAboard()) {
                        // Establish the relationship
                        caesar->setOnGalley(galley->getSerialNumber());
                        galley->setLeaderAboard(caesar->getUniqueId());
                        qDebug() << "  Linked Caesar to galley in" << territory;
                        break;
                    }
                }

                // Add troops in the same sea territory to the caesar's legion
                caesar->clearLegion();
                for (InfantryPiece *infantry : player->getInfantry()) {
                    if (infantry->getTerritoryName() == territory) {
                        caesar->addToLegion(infantry->getUniqueId());
                        infantry->setOnGalley(caesar->getOnGalley());
                    }
                }
                for (CavalryPiece *cavalry : player->getCavalry()) {
                    if (cavalry->getTerritoryName() == territory) {
                        caesar->addToLegion(cavalry->getUniqueId());
                        cavalry->setOnGalley(caesar->getOnGalley());
                    }
                }
                for (CatapultPiece *catapult : player->getCatapults()) {
                    if (catapult->getTerritoryName() == territory) {
                        caesar->addToLegion(catapult->getUniqueId());
                        catapult->setOnGalley(caesar->getOnGalley());
                    }
                }
                qDebug() << "    Caesar's legion now has" << caesar->getLegion().size() << "troops";
            }
        }

        // Check generals
        for (GeneralPiece *general : player->getGenerals()) {
            QString territory = general->getTerritoryName();
            // Check if territory is a sea zone (starts with "Mare" or "Oceanus")
            if (territory.startsWith("Mare") || territory.startsWith("Oceanus")) {
                // Find a galley in the same territory
                for (GalleyPiece *galley : player->getGalleys()) {
                    if (galley->getTerritoryName() == territory && !galley->hasLeaderAboard()) {
                        // Establish the relationship
                        general->setOnGalley(galley->getSerialNumber());
                        galley->setLeaderAboard(general->getUniqueId());
                        qDebug() << "  Linked General" << general->getNumber() << "to galley in" << territory;
                        break;
                    }
                }

                // Add troops in the same sea territory to the general's legion
                // But only if this general is on a galley (to avoid assigning troops to multiple leaders)
                if (general->isOnGalley()) {
                    general->clearLegion();
                    qDebug() << "    Looking for troops in" << territory;
                    for (InfantryPiece *infantry : player->getInfantry()) {
                        qDebug() << "      Infantry at" << infantry->getTerritoryName() << "onGalley:" << infantry->isOnGalley();
                        if (infantry->getTerritoryName() == territory) {
                            general->addToLegion(infantry->getUniqueId());
                            infantry->setOnGalley(general->getOnGalley());
                            qDebug() << "        Added infantry to legion";
                        }
                    }
                    for (CavalryPiece *cavalry : player->getCavalry()) {
                        if (cavalry->getTerritoryName() == territory) {
                            general->addToLegion(cavalry->getUniqueId());
                            cavalry->setOnGalley(general->getOnGalley());
                        }
                    }
                    for (CatapultPiece *catapult : player->getCatapults()) {
                        if (catapult->getTerritoryName() == territory) {
                            general->addToLegion(catapult->getUniqueId());
                            catapult->setOnGalley(general->getOnGalley());
                        }
                    }
                    qDebug() << "    General" << general->getNumber() << "'s legion now has" << general->getLegion().size() << "troops";
                }
            }
        }
    }

    qDebug() << "Game loaded successfully with" << players.size() << "players";
    return true;
}
#else
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

    // Read map dimensions from mapSettings (if present)
    int mapRows = MapWidget::DEFAULT_ROWS;
    int mapCols = MapWidget::DEFAULT_COLUMNS;
    if (gameState.contains("mapSettings")) {
        QJsonObject mapSettings = gameState["mapSettings"].toObject();
        mapRows = mapSettings["rows"].toInt(MapWidget::DEFAULT_ROWS);
        mapCols = mapSettings["cols"].toInt(MapWidget::DEFAULT_COLUMNS);
    }

    // Set map size and clear
    mapWidget->setMapSize(mapRows, mapCols);
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

    // Try to load graph from JSON object, otherwise rebuild from territory grid
    if (gameState.contains("graph") && mapWidget->getGraph()) {
        QJsonObject graphObj = gameState["graph"].toObject();
        if (mapWidget->getGraph()->loadFromJsonObject(graphObj)) {
            qDebug() << "Loaded graph from save file";
        } else {
            qDebug() << "Failed to load graph, rebuilding from territory grid";
            mapWidget->buildGraphFromGrid();
        }
    } else {
        // No graph data in save file, build from territory grid (backward compatibility)
        qDebug() << "No graph data in save file, rebuilding from territory grid";
        mapWidget->buildGraphFromGrid();
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
        Player *player = new Player(playerId, homeName);  // Only need territory name

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

            // Load last sea zone (for beach positioning)
            if (galleyObj.contains("lastSeaZone")) {
                galley->setLastSeaZone(galleyObj["lastSeaZone"].toString());
            }

            galleyMap[galleyObj["serialNumber"].toString()] = galley;
            player->addGalley(galley);
        }

        // Don't assign legions on load - let generals start with empty legions
        // They will pick up troops via the legion composition dialog when they move
        // This ensures fair distribution based on the AI's quota system

        // Restore legions for Galleys
        for (const QJsonValue &galleyValue : galleysArray) {
            QJsonObject galleyObj = galleyValue.toObject();
            GalleyPiece *galley = galleyMap[galleyObj["serialNumber"].toString()];

            if (galley) {
                QString galleyTerritory = galley->getTerritoryName();
                QList<int> legion;

                // Add all pieces on this galley (they have onGalley set)
                for (CaesarPiece *piece : player->getCaesars()) {
                    if (piece->getTerritoryName() == galleyTerritory &&
                        !piece->getOnGalley().isEmpty()) {
                        legion.append(piece->getUniqueId());
                    }
                }
                for (GeneralPiece *piece : player->getGenerals()) {
                    if (piece->getTerritoryName() == galleyTerritory &&
                        !piece->getOnGalley().isEmpty()) {
                        legion.append(piece->getUniqueId());
                    }
                }
                for (InfantryPiece *troop : player->getInfantry()) {
                    if (troop->getTerritoryName() == galleyTerritory &&
                        !troop->getOnGalley().isEmpty()) {
                        legion.append(troop->getUniqueId());
                    }
                }
                for (CavalryPiece *troop : player->getCavalry()) {
                    if (troop->getTerritoryName() == galleyTerritory &&
                        !troop->getOnGalley().isEmpty()) {
                        legion.append(troop->getUniqueId());
                    }
                }
                for (CatapultPiece *troop : player->getCatapults()) {
                    if (troop->getTerritoryName() == galleyTerritory &&
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

        // Roads are not loaded - they are automatically generated from cities after all players are loaded

        players.append(player);
    }

    // Set players on the map widget
    mapWidget->setPlayers(players);

    return true;
}
#endif // USE_OPENGL_MAP
