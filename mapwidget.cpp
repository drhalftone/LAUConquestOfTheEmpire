#include "mapwidget.h"
#include "mapgraph.h"
#include "gamepiece.h"
#include "player.h"
#include "building.h"
#include "playerinfowidget.h"
#include <QPainter>
#include <QPixmap>
#include <QRandomGenerator>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QToolTip>
#include <QHelpEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QMimeData>
#include <QDebug>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QApplication>
#include <QStyle>
#include <QSettings>
#include <QStandardPaths>
#include <algorithm>
#include <QtMath>
#include <QList>
#include <QDebug>

MapWidget::MapWidget(QWidget *parent)
    : QWidget(parent)
    , m_menuBar(nullptr)
    , m_playerInfoWidget(nullptr)
    , m_tileWidth(60)
    , m_tileHeight(60)
    , m_dragging(false)
    , m_draggedPiece(nullptr)
    , m_inflationMultiplier(1)  // Start with no inflation
    , m_highestWallet(0)
    , m_currentPlayerIndex(0)
    , m_isAtStartOfTurn(true)
    , m_graph(nullptr)
    , m_graphDebugMode(false)
{
    // Create menu bar
    createMenuBar();

    // Initialize scores to 0
    for (char c = 'A'; c <= 'F'; ++c) {
        m_scores[QChar(c)] = 0;
    }

    // Set minimum size based on grid dimensions (add menu bar height + score bar height)
    int scoreBarHeight = 80;
    setMinimumSize(COLUMNS * m_tileWidth, ROWS * m_tileHeight + (m_menuBar ? m_menuBar->height() : 25) + scoreBarHeight);

    // Enable mouse tracking to receive mouse move events
    setMouseTracking(true);

    // Enable drag and drop for placing units
    setAcceptDrops(true);

    // Initialize wallets to 0 for all players
    for (char c = 'A'; c <= 'F'; ++c) {
        m_playerWallets[QChar(c)] = 0;
    }

    // Initialize troop grids for all players
    for (char c = 'A'; c <= 'F'; ++c) {
        QChar player(c);
        m_playerTroops[player].resize(ROWS);
        for (int row = 0; row < ROWS; ++row) {
            m_playerTroops[player][row].resize(COLUMNS);
        }
    }

    // Initialize the map with random land/sea tiles
    initializeMap();

    // Assign names and values to territories
    assignTerritoryNames();

    // Build graph representation from grid (Phase 2: Dual system)
    m_graph = new MapGraph();
    buildGraphFromGrid();

    // Debug: Verify graph was built correctly
    qDebug() << "Graph built with" << m_graph->territoryCount() << "territories";
    qDebug() << "Land territories:" << m_graph->countByType(TerritoryType::Land);
    qDebug() << "Sea territories:" << m_graph->countByType(TerritoryType::Sea);

    // Place caesars on land tiles
    placeCaesars();

    // Emit initial scores
    emit scoresChanged();
}

void MapWidget::initializeMap()
{
    // Resize the 2D vectors
    m_tiles.resize(ROWS);
    m_ownership.resize(ROWS);
    m_hasCity.resize(ROWS);
    m_hasFortification.resize(ROWS);
    for (int row = 0; row < ROWS; ++row) {
        m_tiles[row].resize(COLUMNS);
        m_ownership[row].resize(COLUMNS);
        m_hasCity[row].resize(COLUMNS);
        m_hasFortification[row].resize(COLUMNS);
    }

    // Randomly assign each tile as land or sea
    QRandomGenerator *random = QRandomGenerator::global();
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLUMNS; ++col) {
            // 75% chance of land (green), 25% chance of sea (blue)
            m_tiles[row][col] = (random->bounded(100) < 75) ? TileType::Land : TileType::Sea;
            // Initialize as unowned
            m_ownership[row][col] = '\0';
            // No cities or fortifications yet
            m_hasCity[row][col] = false;
            m_hasFortification[row][col] = false;
        }
    }
}

void MapWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Get menu bar height and score bar height
    int menuBarHeight = m_menuBar ? m_menuBar->height() : 0;
    int scoreBarHeight = 80;

    // Calculate tile size based on current widget size (minus menu bar and score bar)
    m_tileWidth = width() / COLUMNS;
    m_tileHeight = (height() - menuBarHeight - scoreBarHeight) / ROWS;

    // Draw each tile (offset by menu bar height)
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLUMNS; ++col) {
            int x = col * m_tileWidth;
            int y = menuBarHeight + (row * m_tileHeight);

            // Set color based on tile type
            QColor tileColor;
            if (m_tiles[row][col] == TileType::Land) {
                tileColor = QColor(144, 238, 144); // Light green
            } else {
                tileColor = QColor(100, 149, 237); // Darker blue (cornflower blue)
            }

            // Fill the tile
            painter.fillRect(x, y, m_tileWidth, m_tileHeight, tileColor);

            // Draw border (normal thin border)
            painter.setPen(QPen(Qt::black, 1));
            painter.drawRect(x, y, m_tileWidth, m_tileHeight);

            // Draw thick colored border if square is owned by a player (query from Player objects)
            // Sea territories are never owned, so skip them
            if (!isSeaTerritory(row, col)) {
                QChar owner = getTerritoryOwnerAt(row, col);
                if (owner != '\0') {
                    QColor ownerColor = getPlayerColor(owner);
                    painter.setPen(QPen(ownerColor, 8));  // Thicker border
                    painter.drawRect(x + 4, y + 4, m_tileWidth - 8, m_tileHeight - 8);
                }
            }

            // Check if this territory is disputed (has TROOPS from multiple players)
            // Leaders alone don't count as disputed - only Infantry/Cavalry/Catapult
            bool isDisputed = false;
            QChar firstPlayer = '\0';
            QString territoryName = getTerritoryNameAt(row, col);
            for (Player *player : m_players) {
                QList<GamePiece*> pieces = player->getPiecesAtTerritory(territoryName);

                // Check if this player has any actual troops (not just leaders)
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
                    if (firstPlayer == '\0') {
                        firstPlayer = player->getId();
                    } else if (firstPlayer != player->getId()) {
                        isDisputed = true;
                        break;
                    }
                }
            }

            // Draw disputed territory indicator (diagonal stripes)
            if (isDisputed) {
                painter.save();
                painter.setPen(QPen(QColor(255, 0, 0), 3));  // Red, thick line

                // Draw X pattern to indicate combat
                painter.drawLine(x + 8, y + 8, x + m_tileWidth - 8, y + m_tileHeight - 8);
                painter.drawLine(x + m_tileWidth - 8, y + 8, x + 8, y + m_tileHeight - 8);

                painter.restore();
            }

            // Draw territory names for all tiles
            painter.setPen(Qt::black);

            // Calculate font size based on tile size
            QFont nameFont = painter.font();
            nameFont.setPointSize(qMax(8, m_tileHeight / 8));
            nameFont.setBold(false);
            painter.setFont(nameFont);

            // Draw territory name (centered for sea, upper part for land)
            QString name = m_territories[row][col].name;
            if (m_tiles[row][col] == TileType::Sea) {
                // Draw sea name centered in tile
                QRect nameRect(x + 2, y + 2, m_tileWidth - 4, m_tileHeight - 4);
                painter.drawText(nameRect, Qt::AlignCenter | Qt::TextWordWrap, name);
            } else {
                // Draw land name in upper part of tile
                QRect nameRect(x + 2, y + 2, m_tileWidth - 4, m_tileHeight / 2);
                painter.drawText(nameRect, Qt::AlignCenter | Qt::TextWordWrap, name);

                // Draw value below name for land tiles
                QFont valueFont = painter.font();
                valueFont.setPointSize(qMax(10, m_tileHeight / 6));
                valueFont.setBold(true);
                painter.setFont(valueFont);

                QString value = QString::number(m_territories[row][col].value);
                QRect valueRect(x + 2, y + m_tileHeight / 2, m_tileWidth - 4, m_tileHeight / 2 - 2);
                painter.drawText(valueRect, Qt::AlignHCenter | Qt::AlignTop, value);
            }

            // Draw cities for all players at this position
            City *cityAtPosition = nullptr;
            // territoryName already defined above at line 177
            for (Player *player : m_players) {
                City *city = player->getCityAtTerritory(territoryName);
                if (city) {
                    cityAtPosition = city;
                    break;
                }
            }

            if (cityAtPosition) {
                // Save painter state before drawing city
                painter.save();

                // Use the City's paint method to draw it (handles color for marked cities)
                cityAtPosition->paint(painter, x, y, m_tileWidth, m_tileHeight);

                // Restore painter state after drawing city
                painter.restore();
            }

            // NOTE: Troop drawing now handled by piece-based system below (getAllPieces)
            // The old m_playerTroops count-based system is deprecated
        }
    }

    // Draw roads for all players
    for (Player *player : m_players) {
        QColor playerColor = getPlayerColor(player->getId());
        painter.setPen(QPen(playerColor, 4));  // Thick line in player color

        for (Road *road : player->getRoads()) {
            ::Position from = road->getFromPosition();  // Road uses global Position
            ::Position to = road->getToPosition();

            // Calculate center points of tiles
            int fromX = from.col * m_tileWidth + m_tileWidth / 2;
            int fromY = menuBarHeight + (from.row * m_tileHeight) + m_tileHeight / 2;
            int toX = to.col * m_tileWidth + m_tileWidth / 2;
            int toY = menuBarHeight + (to.row * m_tileHeight) + m_tileHeight / 2;

            // Draw line connecting the two cities
            painter.drawLine(fromX, fromY, toX, toY);
        }
    }

    // Draw all pieces for all players
    for (Player *player : m_players) {
        QChar playerId = player->getId();

        // Get all pieces for this player
        QList<GamePiece*> allPieces = player->getAllPieces();

        // Group pieces by territory name (not position, since pieces now use territory names)
        QMap<QString, QVector<GamePiece*>> piecesAtTerritory;
        for (GamePiece *piece : allPieces) {
            QString territoryName = piece->getTerritoryName();
            if (territoryName.isEmpty()) {
                qDebug() << "WARNING: Piece has empty territory name, skipping";
                continue;
            }
            piecesAtTerritory[territoryName].append(piece);
        }

        // Draw each group of pieces at their territory
        for (auto territoryIt = piecesAtTerritory.begin(); territoryIt != piecesAtTerritory.end(); ++territoryIt) {
            QString territoryName = territoryIt.key();
            const QVector<GamePiece*> &piecesHere = territoryIt.value();

            // Convert territory name to position for drawing
            Position pos = territoryNameToPosition(territoryName);

            int x = pos.col * m_tileWidth;
            int y = menuBarHeight + (pos.row * m_tileHeight);

            // Calculate tile center
            int tileCenterX = x + m_tileWidth / 2;
            int tileCenterY = y + m_tileHeight / 2;

            // Distribute pieces in a circle pattern if multiple
            int count = piecesHere.size();
            for (int i = 0; i < count; ++i) {
                GamePiece *piece = piecesHere[i];

                // Calculate position offset for stacking
                int offsetX = 0, offsetY = 0;
                if (count > 1) {
                    double angle = (2.0 * M_PI * i) / count;
                    int stackRadius = m_tileWidth / 4;
                    offsetX = stackRadius * qCos(angle);
                    offsetY = stackRadius * qSin(angle);
                }

                int centerX = tileCenterX + offsetX;
                int centerY = tileCenterY + offsetY;

                // Determine piece size and icon based on type
                int radius;
                QString iconPath;
                GamePiece::Type pieceType = piece->getType();

                switch (pieceType) {
                    case GamePiece::Type::Caesar:
                        radius = qMin(m_tileWidth, m_tileHeight) * 0.35;
                        iconPath = ":/images/ceasarIcon.png";
                        break;
                    case GamePiece::Type::General:
                        radius = qMin(m_tileWidth, m_tileHeight) * 0.25;
                        iconPath = ":/images/generalIcon.png";
                        break;
                    case GamePiece::Type::Infantry:
                        radius = qMin(m_tileWidth, m_tileHeight) * 0.2;
                        iconPath = ":/images/infantryIcon.png";
                        break;
                    case GamePiece::Type::Cavalry:
                        radius = qMin(m_tileWidth, m_tileHeight) * 0.2;
                        iconPath = ":/images/cavalryIcon.png";
                        break;
                    case GamePiece::Type::Catapult:
                        radius = qMin(m_tileWidth, m_tileHeight) * 0.2;
                        iconPath = ":/images/catapultIcon.png";
                        break;
                    case GamePiece::Type::Galley:
                        radius = qMin(m_tileWidth, m_tileHeight) * 0.2;
                        iconPath = ":/images/galleyIcon.png";
                        break;
                    default:
                        radius = qMin(m_tileWidth, m_tileHeight) * 0.2;
                        iconPath = ":/images/infantryIcon.png";
                        qDebug() << "WARNING: Unknown piece type:" << static_cast<int>(pieceType);
                        break;
                }

                // Draw as ghost if not current player's turn
                bool isGhost = !player->isMyTurn();
                if (isGhost) {
                    painter.setOpacity(0.3);
                }

                // Get player color (gray for black player so icon is visible)
                QColor playerColor = getPlayerColor(playerId);
                if (playerId == 'E') {
                    playerColor = QColor(128, 128, 128);  // Gray instead of black
                }

                // Load and draw the icon
                QPixmap icon(iconPath);
                if (!icon.isNull()) {
                    // Scale icon to fit (about 70% of diameter)
                    int iconSize = static_cast<int>(radius * 1.4);
                    QPixmap scaledIcon = icon.scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

                    // Draw flat oval pedestal at bottom of icon (35% height, 80% width)
                    int ovalHeight = static_cast<int>(scaledIcon.height() * 0.35);
                    int ovalWidth = static_cast<int>(radius * 0.8);
                    int ovalCenterY = centerY + scaledIcon.height() / 2 - ovalHeight / 2;
                    painter.setBrush(playerColor);
                    painter.setPen(QPen(Qt::black, 2));
                    painter.drawEllipse(QPoint(centerX, ovalCenterY), ovalWidth, ovalHeight / 2);

                    // Draw the icon centered above the pedestal
                    int iconX = centerX - scaledIcon.width() / 2;
                    int iconY = centerY - scaledIcon.height() / 2;
                    painter.drawPixmap(iconX, iconY, scaledIcon);
                }

                if (isGhost) {
                    painter.setOpacity(1.0);
                }
            }
        }
    }

    // Draw the dragged piece at the mouse position
    // Dragging visualization removed - piece movement now handled by PlayerInfoWidget context menus
    Q_UNUSED(m_dragging);
    Q_UNUSED(m_draggedPiece);

    // Draw player scores at the bottom
    int scoreY = menuBarHeight + (ROWS * m_tileHeight);
    int numPlayers = m_players.size() > 0 ? m_players.size() : 1;
    int cellWidth = width() / numPlayers;
    int cellHeight = scoreBarHeight - 10;

    for (int i = 0; i < m_players.size(); ++i) {
        int x = i * cellWidth;
        int y = scoreY + 5;

        QChar player = m_players[i]->getId();

        // Get player color (dark)
        QColor playerColorDark = getPlayerColor(player);

        // Get player color (light)
        QColor playerColorLight;
        switch (player.toLatin1()) {
            case 'A': playerColorLight = QColor(255, 200, 200); break; // Light red
            case 'B': playerColorLight = QColor(200, 255, 200); break; // Light green
            case 'C': playerColorLight = QColor(200, 200, 255); break; // Light blue
            case 'D': playerColorLight = QColor(255, 255, 200); break; // Light yellow
            case 'E': playerColorLight = QColor(220, 220, 220); break; // Light gray
            case 'F': playerColorLight = QColor(255, 220, 180); break; // Light orange
            default: playerColorLight = Qt::lightGray; break;
        }

        // Draw cell border with dark player color and fill with light player color
        painter.setPen(QPen(playerColorDark, 3));
        painter.setBrush(playerColorLight);
        painter.drawRect(x + 5, y, cellWidth - 10, cellHeight);

        // Draw player letter
        QFont playerFont = painter.font();
        playerFont.setPointSize(11);
        playerFont.setBold(true);
        painter.setFont(playerFont);

        painter.setPen(playerColorDark);
        QRect letterRect(x + 5, y, cellWidth - 10, cellHeight / 2);
        painter.drawText(letterRect, Qt::AlignCenter, QString("Player %1").arg(player));

        // Draw score
        QFont scoreFont = painter.font();
        scoreFont.setPointSize(14);
        scoreFont.setBold(true);
        painter.setFont(scoreFont);

        painter.setPen(Qt::black);
        QRect scoreRect(x + 5, y + cellHeight / 2, cellWidth - 10, cellHeight / 2);
        painter.drawText(scoreRect, Qt::AlignCenter, QString::number(m_scores[player]));
    }

    // === Graph Debug Visualization (Phase 2) ===
    if (m_graphDebugMode && m_graph) {
        painter.save();

        // Draw territory boundaries (polygons)
        painter.setPen(QPen(QColor(255, 0, 255), 2));  // Magenta borders
        painter.setBrush(Qt::NoBrush);

        QList<QString> territoryNames = m_graph->getTerritoryNames();
        for (const QString &name : territoryNames) {
            QPolygonF boundary = m_graph->getBoundary(name);
            if (!boundary.isEmpty()) {
                // Offset by menu bar
                QPolygonF offsetBoundary;
                for (const QPointF &point : boundary) {
                    offsetBoundary << QPointF(point.x(), point.y() + menuBarHeight);
                }
                painter.drawPolygon(offsetBoundary);
            }
        }

        // Draw neighbor connections (lines between centroids)
        painter.setPen(QPen(QColor(255, 165, 0), 1));  // Orange lines
        QSet<QString> drawnConnections;  // Avoid drawing each edge twice

        for (const QString &name : territoryNames) {
            QPointF centroid = m_graph->getCentroid(name);
            QPointF offsetCentroid(centroid.x(), centroid.y() + menuBarHeight);

            QList<QString> neighbors = m_graph->getNeighbors(name);
            for (const QString &neighbor : neighbors) {
                // Create unique key for this edge (alphabetically sorted)
                QString edgeKey = (name < neighbor)
                    ? name + "_" + neighbor
                    : neighbor + "_" + name;

                if (!drawnConnections.contains(edgeKey)) {
                    drawnConnections.insert(edgeKey);

                    QPointF neighborCentroid = m_graph->getCentroid(neighbor);
                    QPointF offsetNeighborCentroid(neighborCentroid.x(), neighborCentroid.y() + menuBarHeight);
                    painter.drawLine(offsetCentroid, offsetNeighborCentroid);
                }
            }
        }

        // Draw centroids (small circles)
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 0, 0));  // Red dots
        for (const QString &name : territoryNames) {
            QPointF centroid = m_graph->getCentroid(name);
            QPointF offsetCentroid(centroid.x(), centroid.y() + menuBarHeight);
            painter.drawEllipse(offsetCentroid, 3, 3);
        }

        // Draw territory names from graph (in small yellow text)
        painter.setPen(QColor(255, 255, 0));  // Yellow text
        QFont debugFont = painter.font();
        debugFont.setPointSize(7);
        painter.setFont(debugFont);

        for (const QString &name : territoryNames) {
            QPointF centroid = m_graph->getCentroid(name);
            QPointF offsetCentroid(centroid.x(), centroid.y() + menuBarHeight);

            // Draw name slightly offset from centroid
            painter.drawText(QPointF(offsetCentroid.x() + 5, offsetCentroid.y() - 5), name);
        }

        // Draw debug info in corner
        painter.setPen(Qt::white);
        painter.setBrush(QColor(0, 0, 0, 180));  // Semi-transparent black background
        QRect infoBox(10, menuBarHeight + 10, 250, 80);
        painter.drawRect(infoBox);

        painter.setPen(Qt::yellow);
        QFont infoFont = painter.font();
        infoFont.setPointSize(9);
        painter.setFont(infoFont);

        QString debugInfo = QString("GRAPH DEBUG MODE\n"
                                    "Territories: %1\n"
                                    "Land: %2\n"
                                    "Sea: %3")
            .arg(m_graph->territoryCount())
            .arg(m_graph->countByType(TerritoryType::Land))
            .arg(m_graph->countByType(TerritoryType::Sea));
        painter.drawText(infoBox.adjusted(5, 5, -5, -5), Qt::AlignLeft | Qt::AlignVCenter, debugInfo);

        painter.restore();
    }
}

void MapWidget::placeCaesars()
{
    QRandomGenerator *random = QRandomGenerator::global();

    // Collect all land tiles
    QVector<Position> landTiles;
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLUMNS; ++col) {
            if (m_tiles[row][col] == TileType::Land) {
                landTiles.append({row, col});
            }
        }
    }

    // Make sure we have at least 6 land tiles
    if (landTiles.size() < 6) {
        qWarning("Not enough land tiles for 6 players!");
        return;
    }

    // Shuffle the land tiles and pick the first 6
    std::shuffle(landTiles.begin(), landTiles.end(), *random);

    // Create pieces for each player (A-F)
    const QString playerLetters = "ABCDEF";
    for (int i = 0; i < 6; ++i) {
        QChar player = playerLetters.at(i);
        Position startPos = landTiles[i];

        // Create 6 pieces: 1 caesar + 5 generals
        QVector<Piece> pieces;

        // Caesar
        Piece caesar;
        caesar.type = PieceType::Caesar;
        caesar.generalNumber = 0;
        caesar.position = startPos;
        caesar.movesRemaining = 2;
        pieces.append(caesar);

        // 5 Generals
        for (int g = 1; g <= 5; ++g) {
            Piece general;
            general.type = PieceType::General;
            general.generalNumber = g;
            general.position = startPos;
            general.movesRemaining = 2;
            pieces.append(general);
        }

        m_playerPieces[player] = pieces;

        // Remember this as the home province
        m_homeProvinces[player] = startPos;

        // Claim the starting territory
        m_ownership[startPos.row][startPos.col] = player;

        // Place a fortified city (walled city) at home province
        m_hasCity[startPos.row][startPos.col] = true;
        m_hasFortification[startPos.row][startPos.col] = true;
    }
}

void MapWidget::assignTerritoryNames()
{
    // Large list of animal names for land territories
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

    // List of fish names for sea territories
    QStringList fishNames = {
        "Salmon", "Tuna", "Bass", "Trout", "Pike", "Carp", "Catfish", "Perch",
        "Cod", "Haddock", "Halibut", "Flounder", "Sole", "Mackerel", "Herring", "Sardine",
        "Anchovy", "Barracuda", "Marlin", "Swordfish", "Sailfish", "Mahi", "Grouper", "Snapper",
        "Sturgeon", "Eel", "Lamprey", "Pufferfish", "Angelfish", "Clownfish", "Tang", "Wrasse",
        "Seahorse", "Stingray", "Manta", "Jellyfish", "Octopus", "Squid", "Cuttlefish", "Nautilus",
        "Lobster", "Crab", "Shrimp", "Krill", "Starfish", "Urchin", "Anemone", "Coral"
    };

    // Shuffle both lists
    QRandomGenerator *random = QRandomGenerator::global();
    std::shuffle(animalNames.begin(), animalNames.end(), *random);
    std::shuffle(fishNames.begin(), fishNames.end(), *random);

    // Resize territories array
    m_territories.resize(ROWS);
    for (int row = 0; row < ROWS; ++row) {
        m_territories[row].resize(COLUMNS);
    }

    // Assign names and values
    int animalIndex = 0;
    int fishIndex = 0;
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLUMNS; ++col) {
            if (m_tiles[row][col] == TileType::Land) {
                // Assign unique animal name
                if (animalIndex < animalNames.size()) {
                    m_territories[row][col].name = animalNames[animalIndex++];
                } else {
                    // Fallback if we run out of names (shouldn't happen with 96 names)
                    m_territories[row][col].name = QString("Territory%1").arg(animalIndex++);
                }

                // Randomly assign value of 5 or 10 for land territories only
                m_territories[row][col].value = (random->bounded(2) == 0) ? 5 : 10;
            } else {
                // Sea tiles get fish names
                if (fishIndex < fishNames.size()) {
                    m_territories[row][col].name = fishNames[fishIndex++];
                } else {
                    // Fallback if we run out of names
                    m_territories[row][col].name = QString("Sea%1").arg(fishIndex++);
                }
                // Sea territories have no tax value
                m_territories[row][col].value = 0;
            }
        }
    }
}

bool MapWidget::isInsidePiece(const QPoint &pos, const Position &piecePos, int radius) const
{
    int tileWidth = width() / COLUMNS;
    int tileHeight = height() / ROWS;

    int x = piecePos.col * tileWidth;
    int y = piecePos.row * tileHeight;

    int centerX = x + tileWidth / 2;
    int centerY = y + tileHeight / 2;

    // Calculate distance from center
    int dx = pos.x() - centerX;
    int dy = pos.y() - centerY;
    int distanceSquared = dx * dx + dy * dy;

    return distanceSquared <= radius * radius;
}

MapWidget::Piece* MapWidget::getPieceAt(const QPoint &pos, QChar player)
{
    if (!m_playerPieces.contains(player)) {
        return nullptr;
    }

    QVector<Piece> &pieces = m_playerPieces[player];

    // Check pieces in reverse order (draw order) so we pick the topmost piece
    for (int i = pieces.size() - 1; i >= 0; --i) {
        Piece &piece = pieces[i];

        // Determine radius based on piece type
        int radius;
        if (piece.type == PieceType::Caesar) {
            radius = qMin(m_tileWidth, m_tileHeight) * 0.35;
        } else {
            radius = qMin(m_tileWidth, m_tileHeight) * 0.2;
        }

        // For stacked pieces, we need to check offset positions
        QVector<Piece*> piecesAtPos = getPiecesAtPosition(piece.position, player);
        int count = piecesAtPos.size();

        for (int j = 0; j < count; ++j) {
            if (piecesAtPos[j] == &piece) {
                // Calculate position offset for stacking
                int offsetX = 0, offsetY = 0;
                if (count > 1) {
                    double angle = (2.0 * M_PI * j) / count;
                    int stackRadius = m_tileWidth / 4;
                    offsetX = stackRadius * qCos(angle);
                    offsetY = stackRadius * qSin(angle);
                }

                int x = piece.position.col * m_tileWidth;
                int y = piece.position.row * m_tileHeight;
                int centerX = x + m_tileWidth / 2 + offsetX;
                int centerY = y + m_tileHeight / 2 + offsetY;

                int dx = pos.x() - centerX;
                int dy = pos.y() - centerY;
                int distanceSquared = dx * dx + dy * dy;

                if (distanceSquared <= radius * radius) {
                    return &piece;
                }
                break;
            }
        }
    }

    return nullptr;
}

QVector<MapWidget::Piece*> MapWidget::getPiecesAtPosition(const Position &pos, QChar player)
{
    QVector<Piece*> result;

    if (!m_playerPieces.contains(player)) {
        return result;
    }

    QVector<Piece> &pieces = m_playerPieces[player];
    for (Piece &piece : pieces) {
        if (piece.position == pos) {
            result.append(&piece);
        }
    }

    return result;
}

void MapWidget::mousePressEvent(QMouseEvent *event)
{
    // Handle right-click for territory movement menu
    if (event->button() == Qt::RightButton) {
        // Accept the event to prevent contextMenuEvent from also firing
        event->accept();

        if (m_playerInfoWidget) {
            // Get menu bar height
            int menuBarHeight = m_menuBar ? m_menuBar->height() : 0;

            // Calculate which tile was clicked (adjust for menu bar)
            int clickX = event->pos().x();
            int clickY = event->pos().y() - menuBarHeight;

            int col = clickX / m_tileWidth;
            int row = clickY / m_tileHeight;

            // Check if click is within valid map bounds
            if (row >= 0 && row < ROWS && col >= 0 && col < COLUMNS) {
                QString territoryName = getTerritoryNameAt(row, col);

                // Get current player
                QChar currentPlayer = '\0';
                if (m_currentPlayerIndex >= 0 && m_currentPlayerIndex < m_players.size()) {
                    currentPlayer = m_players[m_currentPlayerIndex]->getId();
                }

                if (currentPlayer != '\0' && m_playerInfoWidget) {
                    // Convert click position to global screen coordinates for menu
                    QPoint globalPos = mapToGlobal(event->pos());

                    // Delegate to PlayerInfoWidget to handle the right-click
                    m_playerInfoWidget->handleTerritoryRightClick(territoryName, globalPos, currentPlayer);
                }
            }
        }
    }
}

void MapWidget::mouseMoveEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    // Mouse dragging removed - piece movement now handled by PlayerInfoWidget context menus
}

void MapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    // Mouse dragging removed - piece movement now handled by PlayerInfoWidget context menus
}

void MapWidget::contextMenuEvent(QContextMenuEvent *event)
{
    // Right-click context menus are handled by mousePressEvent -> handleTerritoryRightClick
    // This prevents duplicate menus from appearing
    event->accept();
}

bool MapWidget::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);

        // Get the tile position from mouse position
        int col = helpEvent->pos().x() / m_tileWidth;
        int row = helpEvent->pos().y() / m_tileHeight;

        if (row >= 0 && row < ROWS && col >= 0 && col < COLUMNS) {
            ::Position pos = {row, col};
            QString territoryName = getTerritoryNameAt(row, col);

            QStringList tooltipLines;
            tooltipLines << QString("Territory: %1").arg(territoryName);

            // Get owner
            QChar owner = getTerritoryOwnerAt(row, col);
            if (owner != '\0') {
                tooltipLines << QString("Owner: Player %1").arg(owner);
            } else {
                tooltipLines << "Owner: Unclaimed";
            }

            // Check for city
            bool hasCity = false;
            bool isFortified = false;
            for (Player *player : m_players) {
                City *city = player->getCityAtTerritory(territoryName);
                if (city) {
                    hasCity = true;
                    isFortified = city->isFortified();
                    tooltipLines << QString("City: Player %1 %2").arg(city->getOwner()).arg(isFortified ? "(Fortified)" : "");
                    break;
                }
            }

            tooltipLines << ""; // Blank line

            // Collect all pieces at this position from all players
            bool foundPieces = false;
            for (Player *player : m_players) {
                QList<GamePiece*> piecesHere = player->getPiecesAtTerritory(territoryName);

                if (!piecesHere.isEmpty()) {
                    foundPieces = true;
                    tooltipLines << QString("Player %1 Pieces:").arg(player->getId());

                    for (GamePiece *piece : piecesHere) {
                        QString pieceName;
                        switch (piece->getType()) {
                            case GamePiece::Type::Caesar:
                                pieceName = "Caesar";
                                break;
                            case GamePiece::Type::General:
                                pieceName = QString("General #%1").arg(static_cast<GeneralPiece*>(piece)->getNumber());
                                break;
                            case GamePiece::Type::Infantry:
                                pieceName = "Infantry";
                                break;
                            case GamePiece::Type::Cavalry:
                                pieceName = "Cavalry";
                                break;
                            case GamePiece::Type::Catapult:
                                pieceName = "Catapult";
                                break;
                            case GamePiece::Type::Galley:
                                pieceName = "Galley";
                                break;
                        }

                        tooltipLines << QString("  %1 - ID:%2 (%3 moves)")
                            .arg(pieceName)
                            .arg(piece->getUniqueId())
                            .arg(piece->getMovesRemaining());
                    }
                }
            }

            if (!foundPieces) {
                tooltipLines << "No pieces here";
            }

            QString tooltip = tooltipLines.join("\n");
            QToolTip::showText(helpEvent->globalPos(), tooltip);
            return true;
        }

        QToolTip::hideText();
        event->ignore();
        return true;
    }

    return QWidget::event(event);
}

// Turn management removed - now handled by PlayerInfoWidget and Player objects

bool MapWidget::isValidMove(const Position &from, const Position &to) const
{
    // Must be on a land tile (not sea)
    if (m_tiles[to.row][to.col] != TileType::Land) {
        return false;
    }

    // Calculate differences
    int rowDiff = to.row - from.row;
    int colDiff = to.col - from.col;
    int absRowDiff = qAbs(rowDiff);
    int absColDiff = qAbs(colDiff);

    // Total Manhattan distance must be <= 2
    int totalDistance = absRowDiff + absColDiff;
    if (totalDistance > 2) {
        return false;
    }

    // No movement (staying in same square) is valid
    if (totalDistance == 0) {
        return true;
    }

    // If moving in only one direction (horizontal or vertical), check all intermediate squares
    if (absRowDiff == 0 || absColDiff == 0) {
        // Moving horizontally
        if (absRowDiff == 0) {
            int start = qMin(from.col, to.col);
            int end = qMax(from.col, to.col);
            for (int c = start + 1; c < end; ++c) {
                if (m_tiles[from.row][c] != TileType::Land) {
                    return false;  // Path blocked by sea
                }
            }
            return true;
        }
        // Moving vertically
        else {
            int start = qMin(from.row, to.row);
            int end = qMax(from.row, to.row);
            for (int r = start + 1; r < end; ++r) {
                if (m_tiles[r][from.col] != TileType::Land) {
                    return false;  // Path blocked by sea
                }
            }
            return true;
        }
    }

    // If moving diagonally (both row and col change), need to check if there's
    // a valid path through land tiles. We can go either:
    // 1. Horizontal first, then vertical
    // 2. Vertical first, then horizontal

    // Check path 1: horizontal first, then vertical
    Position intermediate1 = {from.row, to.col};
    if (m_tiles[intermediate1.row][intermediate1.col] == TileType::Land) {
        return true;
    }

    // Check path 2: vertical first, then horizontal
    Position intermediate2 = {to.row, from.col};
    if (m_tiles[intermediate2.row][intermediate2.col] == TileType::Land) {
        return true;
    }

    // No valid path found
    return false;
}

QColor MapWidget::getPlayerColor(QChar player) const
{
    switch (player.toLatin1()) {
        case 'A': return Qt::red;
        case 'B': return Qt::green;
        case 'C': return Qt::blue;
        case 'D': return Qt::yellow;
        case 'E': return Qt::black;
        case 'F': return QColor(255, 165, 0); // Orange
        default: return Qt::gray;
    }
}

QMap<QChar, int> MapWidget::calculateScores() const
{
    QMap<QChar, int> scores;

    // Initialize all scores to 0
    for (char c = 'A'; c <= 'F'; ++c) {
        scores[QChar(c)] = 0;
    }

    // Sum up territory values for each player
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLUMNS; ++col) {
            QChar owner = m_ownership[row][col];
            if (owner != '\0') {
                scores[owner] += m_territories[row][col].value;
            }
        }
    }

    return scores;
}

QString MapWidget::getTerritoryNameAt(int row, int col) const
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLUMNS) {
        return "Off Board";
    }

    return m_territories[row][col].name;
}

int MapWidget::getTerritoryValueAt(int row, int col) const
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLUMNS) {
        return 0;
    }

    return m_territories[row][col].value;
}

bool MapWidget::isSeaTerritory(int row, int col) const
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLUMNS) {
        return false;
    }

    return m_tiles[row][col] == TileType::Sea;
}

QList<Position> MapWidget::getAdjacentSeaTerritories(const Position &pos) const
{
    QList<Position> seaTerritories;

    // Check all 4 adjacent positions (up, down, left, right)
    QVector<Position> adjacentPositions = {
        {pos.row - 1, pos.col},     // up
        {pos.row + 1, pos.col},     // down
        {pos.row, pos.col - 1},     // left
        {pos.row, pos.col + 1}      // right
    };

    for (const Position &adjPos : adjacentPositions) {
        // Check if position is valid and is a sea territory
        if (adjPos.row >= 0 && adjPos.row < ROWS &&
            adjPos.col >= 0 && adjPos.col < COLUMNS &&
            isSeaTerritory(adjPos.row, adjPos.col)) {
            seaTerritories.append(adjPos);
        }
    }

    return seaTerritories;
}

QVector<MapWidget::HomeProvinceInfo> MapWidget::getRandomHomeProvinces()
{
    QVector<HomeProvinceInfo> homeProvinces;
    QRandomGenerator *random = QRandomGenerator::global();

    // Collect all land tiles that are adjacent to at least one sea territory
    QVector<Position> coastalLandTiles;
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLUMNS; ++col) {
            if (m_tiles[row][col] == TileType::Land) {
                Position pos = {row, col};
                // Check if this land tile is adjacent to any sea territory
                QList<Position> adjacentSeas = getAdjacentSeaTerritories(pos);
                if (!adjacentSeas.isEmpty()) {
                    coastalLandTiles.append(pos);
                }
            }
        }
    }

    // Shuffle and select first 6 as home provinces
    std::shuffle(coastalLandTiles.begin(), coastalLandTiles.end(), *random);

    for (int i = 0; i < 6 && i < coastalLandTiles.size(); ++i) {
        HomeProvinceInfo info;
        // Use Position from common.h
        info.position.row = coastalLandTiles[i].row;
        info.position.col = coastalLandTiles[i].col;
        info.name = m_territories[coastalLandTiles[i].row][coastalLandTiles[i].col].name;
        homeProvinces.append(info);

        qDebug() << "Selected home province for player" << (char)('A' + i) << "at"
                 << info.name << "(" << info.position.row << "," << info.position.col << ")"
                 << "- adjacent to sea";
    }

    if (coastalLandTiles.size() < 6) {
        qWarning() << "Warning: Only found" << coastalLandTiles.size()
                   << "coastal land tiles for home provinces. Need 6 for all players!";
    }

    return homeProvinces;
}

QChar MapWidget::getTerritoryOwnerAt(int row, int col) const
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLUMNS) {
        return '\0';
    }

    // Query players to find who owns this territory
    QString territoryName = m_territories[row][col].name;
    for (Player *player : m_players) {
        if (player->ownsTerritory(territoryName)) {
            return player->getId();
        }
    }

    return '\0';  // Unowned
}

bool MapWidget::hasEnemyPiecesAt(int row, int col, QChar currentPlayer) const
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLUMNS) {
        return false;
    }

    // Get territory name at this location
    QString territoryName = getTerritoryNameAt(row, col);

    // Check all players except the current player
    for (Player *player : m_players) {
        if (player->getId() == currentPlayer) {
            continue;
        }

        // Check if this player has any pieces at this territory
        QList<GamePiece*> pieces = player->getPiecesAtTerritory(territoryName);
        if (!pieces.isEmpty()) {
            return true;
        }
    }

    return false;
}

void MapWidget::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept if we have text data (item type from placement dialog)
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

void MapWidget::dragMoveEvent(QDragMoveEvent *event)
{
    // Accept if we have text data
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

void MapWidget::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasText()) {
        return;
    }

    QString itemType = event->mimeData()->text();
    QPoint dropPos = event->pos();

    // Calculate which tile was dropped on
    int col = dropPos.x() / m_tileWidth;
    int row = dropPos.y() / m_tileHeight;

    // Make sure drop is within bounds
    if (col < 0 || col >= COLUMNS || row < 0 || row >= ROWS) {
        return;
    }

    Position dropPosition = {row, col};

    // Handle different item types
    if (itemType == "Infantry" || itemType == "Cavalry" || itemType == "Catapult" || itemType == "Galley") {
        // Troops automatically go to home province - ignore where user dropped
        // For testing, we'll use player 'A' (in real game, use current purchasing player)
        QChar player = 'A'; // TODO: Track which player is currently placing

        if (m_homeProvinces.contains(player)) {
            Position homePos = m_homeProvinces[player];
            qDebug() << "Auto-placing" << itemType << "in home province at row:" << homePos.row << "col:" << homePos.col;

            // Add the troop to the home province
            if (itemType == "Infantry") {
                m_playerTroops[player][homePos.row][homePos.col].infantry++;
            } else if (itemType == "Cavalry") {
                m_playerTroops[player][homePos.row][homePos.col].cavalry++;
            } else if (itemType == "Catapult") {
                m_playerTroops[player][homePos.row][homePos.col].catapult++;
            } else if (itemType == "Galley") {
                m_playerTroops[player][homePos.row][homePos.col].galley++;
            }

            emit itemPlaced(itemType);
            event->acceptProposedAction();
            update();
        }
    }
    else if (itemType == "City") {
        // Cities can only be placed on owned land tiles without a city
        if (m_tiles[row][col] != TileType::Land) {
            qDebug() << "Cannot place city on sea tile!";
            return;
        }

        if (m_hasCity[row][col]) {
            qDebug() << "This tile already has a city!";
            return;
        }

        QChar owner = m_ownership[row][col];
        if (owner == '\0') {
            qDebug() << "Cannot place city on unowned territory!";
            return;
        }

        // Find the player who owns this territory
        Player *owningPlayer = nullptr;
        for (Player *player : m_players) {
            if (player->getId() == owner) {
                owningPlayer = player;
                break;
            }
        }

        if (!owningPlayer) {
            qDebug() << "Could not find player" << owner;
            return;
        }

        // Place the city on the map
        m_hasCity[row][col] = true;

        // Create City object and add to player
        Position pos{row, col};
        QString territoryName = m_territories[row][col].name;
        City *city = new City(owner, pos, territoryName, false, owningPlayer);
        owningPlayer->addCity(city);

        qDebug() << "Placed city at row:" << row << "col:" << col << "territory:" << territoryName;

        emit itemPlaced(itemType);
        event->acceptProposedAction();
        update();
    }
    else if (itemType == "Fortification") {
        // Fortifications can only be placed on existing cities
        if (!m_hasCity[row][col]) {
            qDebug() << "Cannot place fortification - no city here!";
            return;
        }

        if (m_hasFortification[row][col]) {
            qDebug() << "This city already has a fortification!";
            return;
        }

        QChar owner = m_ownership[row][col];
        if (owner == '\0') {
            qDebug() << "Cannot place fortification on unowned city!";
            return;
        }

        // Find the player who owns this city
        Player *owningPlayer = nullptr;
        for (Player *player : m_players) {
            if (player->getId() == owner) {
                owningPlayer = player;
                break;
            }
        }

        if (!owningPlayer) {
            qDebug() << "Could not find player" << owner;
            return;
        }

        // Find the city at this position and fortify it
        QString territoryName = m_territories[row][col].name;
        QList<City*> citiesAtTerritory = owningPlayer->getCitiesAtTerritory(territoryName);
        if (citiesAtTerritory.isEmpty()) {
            qDebug() << "ERROR: City exists on map but not in player's city list at" << territoryName;
            return;
        }

        // Fortify the city
        City *city = citiesAtTerritory.first();
        city->setFortified(true);

        // Place the fortification on the map
        m_hasFortification[row][col] = true;
        qDebug() << "Placed fortification at row:" << row << "col:" << col << "territory:" << territoryName;

        emit itemPlaced(itemType);
        event->acceptProposedAction();
        update();
    }
    else if (itemType == "Road") {
        // Roads connect adjacent cities - more complex, handle later
        qDebug() << "Road placement not yet implemented";
        // TODO: Implement road placement
    }
}

void MapWidget::createMenuBar()
{
    m_menuBar = new QMenuBar(this);

    // File menu
    QMenu *fileMenu = m_menuBar->addMenu("&File");

    QAction *saveAction = fileMenu->addAction(
        QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton),
        "&Save Game..."
    );
    connect(saveAction, &QAction::triggered, this, &MapWidget::saveGame);

    fileMenu->addSeparator();

    QAction *exitAction = fileMenu->addAction(
        QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton),
        "E&xit"
    );
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

    // View menu (for debug options)
    QMenu *viewMenu = m_menuBar->addMenu("&View");

    QAction *graphDebugAction = viewMenu->addAction("Show &Graph Debug Overlay");
    graphDebugAction->setCheckable(true);
    graphDebugAction->setChecked(false);
    graphDebugAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(graphDebugAction, &QAction::toggled, this, &MapWidget::setGraphDebugMode);

    // Help menu
    QMenu *helpMenu = m_menuBar->addMenu("&Help");

    QAction *aboutAction = helpMenu->addAction(
        QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation),
        "&About..."
    );
    connect(aboutAction, &QAction::triggered, this, &MapWidget::showAbout);

    // Position the menu bar at the top, full width
    m_menuBar->setGeometry(0, 0, width(), m_menuBar->sizeHint().height());
    m_menuBar->show();
}

void MapWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    // Reposition menu bar to span full width
    if (m_menuBar) {
        m_menuBar->setGeometry(0, 0, width(), m_menuBar->sizeHint().height());
    }
}

void MapWidget::closeEvent(QCloseEvent *event)
{
    // Warn user about losing game progress
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Exit Game");
    msgBox.setText("Closing the map will exit the game.\n\n"
                   "All unsaved progress will be lost!\n\n"
                   "Do you want to save your game before exiting?");
    msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);

    int reply = msgBox.exec();

    if (reply == QMessageBox::Save) {
        // Try to save the game
        saveGame();
        // Only exit if we're at start of turn (save would have succeeded)
        if (m_isAtStartOfTurn) {
            event->accept();
            qApp->quit();
        } else {
            // Save was blocked due to mid-turn, ask if they still want to exit
            QMessageBox confirmBox(this);
            confirmBox.setWindowTitle("Exit Without Saving");
            confirmBox.setText("Cannot save mid-turn.\n\n"
                               "Do you still want to exit and lose your progress?");
            confirmBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            confirmBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            confirmBox.setDefaultButton(QMessageBox::No);

            if (confirmBox.exec() == QMessageBox::Yes) {
                event->accept();
                qApp->quit();
            } else {
                event->ignore();
            }
        }
    } else if (reply == QMessageBox::Discard) {
        // Exit without saving
        event->accept();
        qApp->quit();
    } else {
        // Cancel - don't close
        event->ignore();
    }
}

void MapWidget::saveGame()
{
    // Check if we're at the start of a turn
    if (!m_isAtStartOfTurn) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Cannot Save");
        msgBox.setText("You can only save at the start of a player's turn.\n"
                       "Please finish the current turn before saving.");
        msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.exec();
        return;
    }

    // Get last used directory from settings, default to Documents folder
    QSettings settings("ConquestOfTheEmpire", "MapWidget");
    QString lastDir = settings.value("lastSaveDirectory",
                                     QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();

    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "Save Game",
                                                    lastDir,
                                                    "JSON Files (*.json)");

    if (fileName.isEmpty()) {
        return;
    }

    // Save the directory for next time
    QFileInfo fileInfo(fileName);
    settings.setValue("lastSaveDirectory", fileInfo.absolutePath());

    QJsonObject gameState;

    // Save current player index
    gameState["currentPlayerIndex"] = m_currentPlayerIndex;

    // Save map state (territories with their names and values)
    QJsonArray territoriesArray;
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLUMNS; ++col) {
            QJsonObject territoryObj;
            territoryObj["row"] = row;
            territoryObj["col"] = col;
            territoryObj["name"] = m_territories[row][col].name;
            territoryObj["value"] = m_territories[row][col].value;
            territoryObj["isLand"] = (m_tiles[row][col] == TileType::Land);
            territoriesArray.append(territoryObj);
        }
    }
    gameState["territories"] = territoriesArray;

    // Save all players
    QJsonArray playersArray;
    for (Player *player : m_players) {
        QJsonObject playerObj;
        playerObj["id"] = QString(player->getId());
        playerObj["wallet"] = player->getWallet();

        // Convert home territory name to position for backward compatibility with save files
        QString homeName = player->getHomeProvinceName();
        Position homePos = territoryNameToPosition(homeName);
        playerObj["homeRow"] = homePos.row;
        playerObj["homeCol"] = homePos.col;
        playerObj["homeName"] = homeName;

        // Save owned territories
        QJsonArray territoriesArray;
        for (const QString &territory : player->getOwnedTerritories()) {
            territoriesArray.append(territory);
        }
        playerObj["ownedTerritories"] = territoriesArray;

        // Save Caesar
        QJsonArray caesarsArray;
        for (CaesarPiece *caesar : player->getCaesars()) {
            QJsonObject caesarObj;
            caesarObj["serialNumber"] = caesar->getSerialNumber();
            caesarObj["row"] = caesar->getPosition().row;
            caesarObj["col"] = caesar->getPosition().col;
            caesarObj["territory"] = caesar->getTerritoryName();
            caesarObj["movesRemaining"] = caesar->getMovesRemaining();
            caesarObj["onGalley"] = caesar->getOnGalley();

            // Save legion
            QJsonArray legionArray;
            for (int pieceId : caesar->getLegion()) {
                legionArray.append(pieceId);
            }
            caesarObj["legion"] = legionArray;

            // Save last territory
            if (caesar->hasLastTerritory()) {
                caesarObj["lastTerritoryRow"] = caesar->getLastTerritory().row;
                caesarObj["lastTerritoryCol"] = caesar->getLastTerritory().col;
            }

            caesarsArray.append(caesarObj);
        }
        playerObj["caesars"] = caesarsArray;

        // Save Generals
        QJsonArray generalsArray;
        for (GeneralPiece *general : player->getGenerals()) {
            QJsonObject generalObj;
            generalObj["serialNumber"] = general->getSerialNumber();
            generalObj["number"] = general->getNumber();
            generalObj["row"] = general->getPosition().row;
            generalObj["col"] = general->getPosition().col;
            generalObj["territory"] = general->getTerritoryName();
            generalObj["movesRemaining"] = general->getMovesRemaining();
            generalObj["onGalley"] = general->getOnGalley();

            // Save legion
            QJsonArray legionArray;
            for (int pieceId : general->getLegion()) {
                legionArray.append(pieceId);
            }
            generalObj["legion"] = legionArray;

            // Save last territory
            if (general->hasLastTerritory()) {
                generalObj["lastTerritoryRow"] = general->getLastTerritory().row;
                generalObj["lastTerritoryCol"] = general->getLastTerritory().col;
            }

            generalsArray.append(generalObj);
        }
        playerObj["generals"] = generalsArray;

        // Save Captured Generals
        QJsonArray capturedGeneralsArray;
        for (GeneralPiece *general : player->getCapturedGenerals()) {
            QJsonObject generalObj;
            generalObj["serialNumber"] = general->getSerialNumber();
            generalObj["originalPlayer"] = QString(general->getPlayer());
            generalObj["number"] = general->getNumber();
            generalObj["row"] = general->getPosition().row;
            generalObj["col"] = general->getPosition().col;
            generalObj["territory"] = general->getTerritoryName();
            generalObj["movesRemaining"] = general->getMovesRemaining();
            generalObj["onGalley"] = general->getOnGalley();
            capturedGeneralsArray.append(generalObj);
        }
        playerObj["capturedGenerals"] = capturedGeneralsArray;

        // Save Infantry
        QJsonArray infantryArray;
        for (InfantryPiece *infantry : player->getInfantry()) {
            QJsonObject infantryObj;
            infantryObj["serialNumber"] = infantry->getSerialNumber();
            infantryObj["row"] = infantry->getPosition().row;
            infantryObj["col"] = infantry->getPosition().col;
            infantryObj["territory"] = infantry->getTerritoryName();
            infantryObj["movesRemaining"] = infantry->getMovesRemaining();
            infantryObj["onGalley"] = infantry->getOnGalley();
            infantryArray.append(infantryObj);
        }
        playerObj["infantry"] = infantryArray;

        // Save Cavalry
        QJsonArray cavalryArray;
        for (CavalryPiece *cavalry : player->getCavalry()) {
            QJsonObject cavalryObj;
            cavalryObj["serialNumber"] = cavalry->getSerialNumber();
            cavalryObj["row"] = cavalry->getPosition().row;
            cavalryObj["col"] = cavalry->getPosition().col;
            cavalryObj["territory"] = cavalry->getTerritoryName();
            cavalryObj["movesRemaining"] = cavalry->getMovesRemaining();
            cavalryObj["onGalley"] = cavalry->getOnGalley();
            cavalryArray.append(cavalryObj);
        }
        playerObj["cavalry"] = cavalryArray;

        // Save Catapults
        QJsonArray catapultsArray;
        for (CatapultPiece *catapult : player->getCatapults()) {
            QJsonObject catapultObj;
            catapultObj["serialNumber"] = catapult->getSerialNumber();
            catapultObj["row"] = catapult->getPosition().row;
            catapultObj["col"] = catapult->getPosition().col;
            catapultObj["territory"] = catapult->getTerritoryName();
            catapultObj["movesRemaining"] = catapult->getMovesRemaining();
            catapultObj["onGalley"] = catapult->getOnGalley();
            catapultsArray.append(catapultObj);
        }
        playerObj["catapults"] = catapultsArray;

        // Save Galleys
        QJsonArray galleysArray;
        for (GalleyPiece *galley : player->getGalleys()) {
            QJsonObject galleyObj;
            galleyObj["serialNumber"] = galley->getSerialNumber();
            galleyObj["row"] = galley->getPosition().row;
            galleyObj["col"] = galley->getPosition().col;
            galleyObj["territory"] = galley->getTerritoryName();
            galleyObj["movesRemaining"] = galley->getMovesRemaining();

            // Save legion
            QJsonArray legionArray;
            for (int pieceId : galley->getLegion()) {
                legionArray.append(pieceId);
            }
            galleyObj["legion"] = legionArray;

            // Save last territory
            if (galley->hasLastTerritory()) {
                galleyObj["lastTerritoryRow"] = galley->getLastTerritory().row;
                galleyObj["lastTerritoryCol"] = galley->getLastTerritory().col;
            }

            galleysArray.append(galleyObj);
        }
        playerObj["galleys"] = galleysArray;

        // Save Cities
        QJsonArray citiesArray;
        for (City *city : player->getCities()) {
            QJsonObject cityObj;
            cityObj["row"] = city->getPosition().row;
            cityObj["col"] = city->getPosition().col;
            cityObj["territory"] = city->getTerritoryName();
            cityObj["isFortified"] = city->isFortified();
            cityObj["markedForDestruction"] = city->isMarkedForDestruction();
            citiesArray.append(cityObj);
        }
        playerObj["cities"] = citiesArray;

        // Roads are not saved - they are automatically generated from cities

        playersArray.append(playerObj);
    }
    gameState["players"] = playersArray;

    // Save graph data under "graph" key
    if (m_graph) {
        gameState["graph"] = m_graph->saveToJsonObject();
    }

    // Write to file
    QJsonDocument doc(gameState);
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Game Saved");
        msgBox.setText(QString("Game saved successfully to:\n%1").arg(fileName));
        msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.exec();
    } else {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Save Failed");
        msgBox.setText(QString("Failed to save game to:\n%1").arg(fileName));
        msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.exec();
    }
}

void MapWidget::loadGame()
{
    // Get last used directory from settings, default to Documents folder
    QSettings settings("ConquestOfTheEmpire", "MapWidget");
    QString lastDir = settings.value("lastSaveDirectory",
                                     QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();

    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "Load Game",
                                                    lastDir,
                                                    "JSON Files (*.json)");

    if (fileName.isEmpty()) {
        return;
    }

    // Save the directory for next time
    QFileInfo fileInfo(fileName);
    settings.setValue("lastSaveDirectory", fileInfo.absolutePath());

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Load Failed");
        msgBox.setText(QString("Failed to open file:\n%1").arg(fileName));
        msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.exec();
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Load Failed");
        msgBox.setText("Invalid save file format.");
        msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        msgBox.exec();
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Load Not Implemented");
    msgBox.setText("Loading games is not yet fully implemented.\n"
                   "This will require recreating the entire game state.");
    msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    msgBox.exec();

    // TODO: Implement full game state restoration
    // This is complex because we need to:
    // 1. Clear all existing players and their pieces
    // 2. Recreate all players with their saved data
    // 3. Recreate all pieces with correct serial numbers
    // 4. Restore all relationships (legions, captured generals, etc.)
    // 5. Update all UI windows
}

void MapWidget::showAbout()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("About Conquest of the Empire");
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText("<h3>Conquest of the Empire</h3>"
                   "<p>A strategic board game of territorial conquest.</p>"
                   "<p><b>Game Features:</b></p>"
                   "<ul>"
                   "<li>6 Player support (A-F)</li>"
                   "<li>Multiple unit types: Caesar, Generals, Infantry, Cavalry, Catapults, Galleys</li>"
                   "<li>Territory control and taxation</li>"
                   "<li>Cities, roads, and fortifications</li>"
                   "<li>Combat system with general capture and ransom</li>"
                   "<li>Economic management</li>"
                   "</ul>"
                   "<p><b>How to Play:</b></p>"
                   "<ul>"
                   "<li>Move your pieces by dragging them on the map</li>"
                   "<li>Right-click pieces for context menus with special actions</li>"
                   "<li>Collect taxes from owned territories at the end of your turn</li>"
                   "<li>Purchase new units and buildings with your wealth</li>"
                   "<li>Capture enemy generals and negotiate ransoms</li>"
                   "</ul>"
                   "<p>Developed with Qt C++</p>");
    msgBox.setIconPixmap(QPixmap(":/images/coeIcon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    msgBox.exec();
}

void MapWidget::setTerritoryAt(int row, int col, const QString &name, int value, bool isLand)
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLUMNS) {
        return;
    }

    m_territories[row][col].name = name;
    m_territories[row][col].value = value;
    m_tiles[row][col] = isLand ? TileType::Land : TileType::Sea;
}

void MapWidget::removeCityAt(int row, int col)
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLUMNS) {
        return;
    }

    m_hasCity[row][col] = false;
    qDebug() << "Removed city at grid position (" << row << "," << col << ")";
}

void MapWidget::removeFortificationAt(int row, int col)
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLUMNS) {
        return;
    }

    m_hasFortification[row][col] = false;
    qDebug() << "Removed fortification at grid position (" << row << "," << col << ")";
}

void MapWidget::clearMap()
{
    // Clear territories
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLUMNS; ++col) {
            m_territories[row][col].name = "";
            m_territories[row][col].value = 0;
            m_tiles[row][col] = TileType::Land;
            m_ownership[row][col] = '\0';
            m_hasCity[row][col] = false;
            m_hasFortification[row][col] = false;
        }
    }
}

void MapWidget::updateScores(const QMap<QChar, int> &scores)
{
    m_scores = scores;
    update();  // Trigger repaint to show updated scores
}

QList<Position> MapWidget::getTerritoriesConnectedByRoad(const Position &startPos, QChar playerId)
{
    QList<Position> connectedTerritories;

    // Find the player
    Player *player = nullptr;
    for (Player *p : m_players) {
        if (p->getId() == playerId) {
            player = p;
            break;
        }
    }

    if (!player) {
        return connectedTerritories;
    }

    // Use breadth-first search to find all territories connected by roads
    QList<Position> toVisit;
    QSet<QString> visited;  // Use position string as key (row,col)

    toVisit.append(startPos);
    visited.insert(QString("%1,%2").arg(startPos.row).arg(startPos.col));

    while (!toVisit.isEmpty()) {
        Position current = toVisit.takeFirst();

        // Look through all roads for this player
        for (Road *road : player->getRoads()) {
            ::Position from = road->getFromPosition();  // Road uses global Position
            ::Position to = road->getToPosition();

            Position next;
            bool foundNext = false;

            // Check if current position matches one end of the road
            if (from.row == current.row && from.col == current.col) {
                next.row = to.row;
                next.col = to.col;
                foundNext = true;
            } else if (to.row == current.row && to.col == current.col) {
                next.row = from.row;
                next.col = from.col;
                foundNext = true;
            }

            if (foundNext) {
                QString nextKey = QString("%1,%2").arg(next.row).arg(next.col);
                if (!visited.contains(nextKey)) {
                    visited.insert(nextKey);
                    toVisit.append(next);
                    connectedTerritories.append(next);
                }
            }
        }
    }

    return connectedTerritories;
}

void MapWidget::updateRoads()
{
    if (m_players.isEmpty()) {
        return;
    }

    // For each player, check all their cities and create roads to adjacent cities
    for (Player *player : m_players) {
        QList<City*> cities = player->getCities();

        // Check each pair of cities to see if they're adjacent
        for (int i = 0; i < cities.size(); ++i) {
            City *city1 = cities[i];
            QString territory1 = city1->getTerritoryName();

            // Get position from territory name (MapWidget knows the grid layout)
            ::Position pos1 = territoryNameToPosition(territory1);

            for (int j = i + 1; j < cities.size(); ++j) {
                City *city2 = cities[j];
                QString territory2 = city2->getTerritoryName();

                // Get position from territory name
                ::Position pos2 = territoryNameToPosition(territory2);

                // Check if territories are different (roads connect different territories)
                if (territory1 == territory2) {
                    continue;
                }

                // Check if positions are adjacent (horizontally or vertically, not diagonally)
                bool isAdjacent = false;
                if ((qAbs(pos1.row - pos2.row) == 1 && pos1.col == pos2.col) ||  // Vertical
                    (qAbs(pos1.col - pos2.col) == 1 && pos1.row == pos2.row)) {  // Horizontal
                    isAdjacent = true;
                }

                if (!isAdjacent) {
                    continue;
                }

                // Check if both territories are owned by this player
                if (!player->ownsTerritory(territory1) || !player->ownsTerritory(territory2)) {
                    continue;
                }

                // Check if either position is a sea territory - roads can't be built in sea
                if (isSeaTerritory(pos1.row, pos1.col) || isSeaTerritory(pos2.row, pos2.col)) {
                    continue;
                }

                // Check if road already exists between these two positions
                bool roadExists = false;
                for (Road *road : player->getRoads()) {
                    ::Position from = road->getFromPosition();  // Road uses global Position
                    ::Position to = road->getToPosition();

                    // Check both directions
                    if ((from.row == pos1.row && from.col == pos1.col &&
                         to.row == pos2.row && to.col == pos2.col) ||
                        (from.row == pos2.row && from.col == pos2.col &&
                         to.row == pos1.row && to.col == pos1.col)) {
                        roadExists = true;
                        break;
                    }
                }

                if (!roadExists) {
                    // Create a new road
                    Road *road = new Road(player->getId(), pos1, territory1, player);
                    road->setToPosition(pos2);
                    player->addRoad(road);
                }
            }
        }
    }

    update();  // Redraw map to show roads
}

// === Graph-based Map System (Phase 2) ===

void MapWidget::buildGraphFromGrid()
{
    if (!m_graph) {
        return;
    }

    // Clear any existing graph data
    m_graph->clear();

    // Create a territory for each grid cell
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLUMNS; ++col) {
            // Use actual territory name from m_territories array
            QString territoryName = m_territories[row][col].name;

            // Create territory
            Territory territory;
            territory.name = territoryName;

            // Calculate centroid (center of the grid cell in pixel coordinates)
            qreal centerX = (col + 0.5) * m_tileWidth;
            qreal centerY = (row + 0.5) * m_tileHeight;
            territory.centroid = QPointF(centerX, centerY);
            territory.labelPosition = territory.centroid;

            // Create rectangular boundary polygon for this cell
            qreal left = col * m_tileWidth;
            qreal right = (col + 1) * m_tileWidth;
            qreal top = row * m_tileHeight;
            qreal bottom = (row + 1) * m_tileHeight;

            territory.boundary = QPolygonF()
                << QPointF(left, top)
                << QPointF(right, top)
                << QPointF(right, bottom)
                << QPointF(left, bottom);

            // Set territory type based on grid tile type
            if (row < m_tiles.size() && col < m_tiles[row].size()) {
                territory.type = (m_tiles[row][col] == TileType::Sea)
                    ? TerritoryType::Sea
                    : TerritoryType::Land;
            } else {
                territory.type = TerritoryType::Land;
            }

            // Add optional color based on type
            territory.color = (territory.type == TerritoryType::Sea)
                ? QColor(100, 150, 200)  // Blue for sea
                : QColor(200, 180, 150);  // Tan for land

            // Add territory to graph
            m_graph->addTerritory(territory);
        }
    }

    // Now add edges between adjacent cells (4-directional: up, down, left, right)
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLUMNS; ++col) {
            QString currentTerritory = m_territories[row][col].name;

            // Check right neighbor
            if (col + 1 < COLUMNS) {
                QString rightNeighbor = m_territories[row][col + 1].name;
                m_graph->addEdge(currentTerritory, rightNeighbor);
            }

            // Check down neighbor
            if (row + 1 < ROWS) {
                QString downNeighbor = m_territories[row + 1][col].name;
                m_graph->addEdge(currentTerritory, downNeighbor);
            }
        }
    }
}

QString MapWidget::positionToTerritoryName(const Position &pos) const
{
    // Convert grid Position to territory name
    if (pos.row >= 0 && pos.row < ROWS && pos.col >= 0 && pos.col < COLUMNS) {
        return QString("T_%1_%2").arg(pos.row).arg(pos.col);
    }
    return QString();  // Invalid position
}

Position MapWidget::territoryNameToPosition(const QString &territoryName) const
{
    // Search through all territories to find matching name
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLUMNS; ++col) {
            if (m_territories[row][col].name == territoryName) {
                return Position{row, col};
            }
        }
    }

    // Fallback: Parse territory name format "T_row_col" (for backward compatibility)
    if (territoryName.startsWith("T_")) {
        QStringList parts = territoryName.mid(2).split("_");
        if (parts.size() == 2) {
            bool ok1, ok2;
            int row = parts[0].toInt(&ok1);
            int col = parts[1].toInt(&ok2);
            if (ok1 && ok2 && row >= 0 && row < ROWS && col >= 0 && col < COLUMNS) {
                return Position{row, col};
            }
        }
    }

    qDebug() << "WARNING: Could not find position for territory:" << territoryName;
    return Position{-1, -1};  // Invalid territory name
}
