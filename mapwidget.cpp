#include "mapwidget.h"
#include "gamepiece.h"
#include "player.h"
#include <QPainter>
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

MapWidget::MapWidget(QWidget *parent)
    : QWidget(parent)
    , m_menuBar(nullptr)
    , m_tileWidth(60)
    , m_tileHeight(60)
    , m_dragging(false)
    , m_draggedPiece(nullptr)
    , m_inflationMultiplier(1)  // Start with no inflation
    , m_highestWallet(0)
    , m_currentPlayerIndex(0)
    , m_isAtStartOfTurn(true)
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
            QChar owner = getTerritoryOwnerAt(row, col);
            if (owner != '\0') {
                QColor ownerColor = getPlayerColor(owner);
                painter.setPen(QPen(ownerColor, 8));  // Thicker border
                painter.drawRect(x + 4, y + 4, m_tileWidth - 8, m_tileHeight - 8);
            }

            // Check if this territory is disputed (has pieces from multiple players)
            bool isDisputed = false;
            QChar firstPlayer = '\0';
            ::Position pos = {row, col};  // Use global Position type
            for (Player *player : m_players) {
                QList<GamePiece*> pieces = player->getPiecesAtPosition(pos);
                if (!pieces.isEmpty()) {
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
            for (Player *player : m_players) {
                ::Position pos = {row, col};
                City *city = player->getCityAtPosition(pos);
                if (city) {
                    cityAtPosition = city;
                    break;
                }
            }

            if (cityAtPosition) {
                // Save painter state before drawing city
                painter.save();

                painter.setPen(Qt::black);
                painter.setBrush(QColor(150, 100, 50)); // Brown for city

                // Draw small building in top-right corner
                int citySize = m_tileWidth / 6;
                QRect cityRect(x + m_tileWidth - citySize - 2, y + 2, citySize, citySize);
                painter.drawRect(cityRect);

                // Draw roof (triangle)
                QPolygon roof;
                roof << QPoint(x + m_tileWidth - citySize - 2, y + 2)
                     << QPoint(x + m_tileWidth - 2, y + 2)
                     << QPoint(x + m_tileWidth - citySize/2 - 2, y - citySize/2 + 2);
                painter.drawPolygon(roof);

                // Draw fortification if present (wall around city)
                if (cityAtPosition->isFortified()) {
                    painter.setPen(QPen(QColor(80, 80, 80), 2)); // Dark gray thick line
                    painter.setBrush(Qt::NoBrush);

                    // Draw crenelated wall around the city icon
                    int wallMargin = 1;
                    QRect wallRect(x + m_tileWidth - citySize - 2 - wallMargin,
                                 y + 2 - wallMargin,
                                 citySize + wallMargin * 2,
                                 citySize + wallMargin * 2);
                    painter.drawRect(wallRect);

                    // Draw crenelations (small rectangles on top)
                    int crenelSize = citySize / 4;
                    for (int i = 0; i < 3; ++i) {
                        int crenelX = wallRect.left() + i * (citySize / 2);
                        painter.fillRect(crenelX, wallRect.top() - crenelSize/2,
                                       crenelSize, crenelSize/2, QColor(80, 80, 80));
                    }
                }

                // Restore painter state after drawing city
                painter.restore();
            }

            // Draw troop counts for all players at this tile using GamePiece classes
            // Save painter state to prevent color bleeding
            painter.save();

            // Iterate through all players to see if they have troops here
            for (char c = 'A'; c <= 'F'; ++c) {
                QChar player(c);
                if (!m_playerTroops.contains(player)) continue;

                const TroopCounts &troops = m_playerTroops[player][row][col];
                ::Position pos = {row, col};  // Use the global Position from gamepiece.h

                // Use GamePiece paint methods for each troop type with counts
                if (troops.infantry > 0) {
                    InfantryPiece infantryPiece(player, pos);
                    infantryPiece.paint(painter, x, y, m_tileWidth, m_tileHeight, troops.infantry);
                }

                if (troops.cavalry > 0) {
                    CavalryPiece cavalryPiece(player, pos);
                    cavalryPiece.paint(painter, x, y, m_tileWidth, m_tileHeight, troops.cavalry);
                }

                if (troops.catapult > 0) {
                    CatapultPiece catapultPiece(player, pos);
                    catapultPiece.paint(painter, x, y, m_tileWidth, m_tileHeight, troops.catapult);
                }

                if (troops.galley > 0) {
                    GalleyPiece galleyPiece(player, pos);
                    galleyPiece.paint(painter, x, y, m_tileWidth, m_tileHeight, troops.galley);
                }
            }

            // Restore painter state
            painter.restore();
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

        // Group pieces by position
        QMap<Position, QVector<GamePiece*>> piecesAtPosition;
        for (GamePiece *piece : allPieces) {
            // Create position key
            Position pos = piece->getPosition();
            piecesAtPosition[pos].append(piece);
        }

        // Draw each group of pieces at their position
        for (auto posIt = piecesAtPosition.begin(); posIt != piecesAtPosition.end(); ++posIt) {
            Position pos = posIt.key();
            const QVector<GamePiece*> &piecesHere = posIt.value();

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

                // Determine piece size based on type
                int radius;
                if (piece->getType() == GamePiece::Type::Caesar) {
                    radius = qMin(m_tileWidth, m_tileHeight) * 0.35;  // Larger
                } else {
                    radius = qMin(m_tileWidth, m_tileHeight) * 0.2;   // Smaller for generals
                }

                // Draw as ghost if not current player's turn
                bool isGhost = !player->isMyTurn();
                if (isGhost) {
                    painter.setOpacity(0.3);
                }

                // Get player color
                QColor playerColor = getPlayerColor(playerId);

                painter.setBrush(playerColor);
                painter.setPen(QPen(Qt::black, 2));
                painter.drawEllipse(QPoint(centerX, centerY), radius, radius);

                // Draw text inside circle
                QColor textColor = (playerId == 'E' || playerId == 'C') ? Qt::white : Qt::black;
                painter.setPen(textColor);

                QFont font = painter.font();
                font.setPointSize(qMax(6, static_cast<int>(radius * 0.7)));
                font.setBold(true);
                painter.setFont(font);

                QString label;
                if (piece->getType() == GamePiece::Type::Caesar) {
                    label = QString(playerId);
                } else if (piece->getType() == GamePiece::Type::General) {
                    // For generals, get the general number
                    GeneralPiece *general = static_cast<GeneralPiece*>(piece);
                    label = QString::number(general->getNumber());
                } else {
                    // For other pieces, just show player ID
                    label = QString(playerId);
                }

                painter.drawText(QRect(centerX - radius, centerY - radius, radius * 2, radius * 2),
                                Qt::AlignCenter, label);

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
    int cellWidth = width() / 6;
    int cellHeight = scoreBarHeight - 10;

    const QString players = "ABCDEF";
    for (int i = 0; i < 6; ++i) {
        int x = i * cellWidth;
        int y = scoreY + 5;

        QChar player = players[i];

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
    Q_UNUSED(event);
    // Mouse dragging removed - piece movement now handled by PlayerInfoWidget context menus
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
    Q_UNUSED(event);
    // Context menus removed - turn management now handled by PlayerInfoWidget
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
                City *city = player->getCityAtPosition(pos);
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
                QList<GamePiece*> piecesHere = player->getPiecesAtPosition(pos);

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

QVector<MapWidget::HomeProvinceInfo> MapWidget::getRandomHomeProvinces()
{
    QVector<HomeProvinceInfo> homeProvinces;
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

    // Shuffle and select first 6 as home provinces
    std::shuffle(landTiles.begin(), landTiles.end(), *random);

    for (int i = 0; i < 6 && i < landTiles.size(); ++i) {
        HomeProvinceInfo info;
        // Use Position from common.h
        info.position.row = landTiles[i].row;
        info.position.col = landTiles[i].col;
        info.name = m_territories[landTiles[i].row][landTiles[i].col].name;
        homeProvinces.append(info);
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

    // Create Position from common.h
    ::Position pos;
    pos.row = row;
    pos.col = col;

    // Check all players except the current player
    for (Player *player : m_players) {
        if (player->getId() == currentPlayer) {
            continue;
        }

        // Check if this player has any pieces at this position
        QList<GamePiece*> pieces = player->getPiecesAtPosition(pos);
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

        // Place the city
        m_hasCity[row][col] = true;
        qDebug() << "Placed city at row:" << row << "col:" << col;

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

        // Place the fortification
        m_hasFortification[row][col] = true;
        qDebug() << "Placed fortification at row:" << row << "col:" << col;

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

    QAction *loadAction = fileMenu->addAction(
        QApplication::style()->standardIcon(QStyle::SP_DialogOpenButton),
        "&Load Game..."
    );
    connect(loadAction, &QAction::triggered, this, &MapWidget::loadGame);

    fileMenu->addSeparator();

    QAction *exitAction = fileMenu->addAction(
        QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton),
        "E&xit"
    );
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

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
    QMessageBox::StandardButton reply;
    reply = QMessageBox::warning(this, "Exit Game",
                                "Closing the map will exit the game.\n\n"
                                "All unsaved progress will be lost!\n\n"
                                "Do you want to save your game before exiting?",
                                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (reply == QMessageBox::Save) {
        // Try to save the game
        saveGame();
        // Only exit if we're at start of turn (save would have succeeded)
        if (m_isAtStartOfTurn) {
            event->accept();
            qApp->quit();
        } else {
            // Save was blocked due to mid-turn, ask if they still want to exit
            QMessageBox::StandardButton confirmExit;
            confirmExit = QMessageBox::question(this, "Exit Without Saving",
                                                "Cannot save mid-turn.\n\n"
                                                "Do you still want to exit and lose your progress?",
                                                QMessageBox::Yes | QMessageBox::No);
            if (confirmExit == QMessageBox::Yes) {
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
        QMessageBox::warning(this, "Cannot Save",
                           "You can only save at the start of a player's turn.\n"
                           "Please finish the current turn before saving.");
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
        playerObj["homeRow"] = player->getHomeProvince().row;
        playerObj["homeCol"] = player->getHomeProvince().col;
        playerObj["homeName"] = player->getHomeProvinceName();

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
            citiesArray.append(cityObj);
        }
        playerObj["cities"] = citiesArray;

        // Save Roads
        QJsonArray roadsArray;
        for (Road *road : player->getRoads()) {
            QJsonObject roadObj;
            roadObj["fromRow"] = road->getFromPosition().row;
            roadObj["fromCol"] = road->getFromPosition().col;
            roadObj["toRow"] = road->getToPosition().row;
            roadObj["toCol"] = road->getToPosition().col;
            roadsArray.append(roadObj);
        }
        playerObj["roads"] = roadsArray;

        playersArray.append(playerObj);
    }
    gameState["players"] = playersArray;

    // Write to file
    QJsonDocument doc(gameState);
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        QMessageBox::information(this, "Game Saved",
                               QString("Game saved successfully to:\n%1").arg(fileName));
    } else {
        QMessageBox::critical(this, "Save Failed",
                            QString("Failed to save game to:\n%1").arg(fileName));
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
        QMessageBox::critical(this, "Load Failed",
                            QString("Failed to open file:\n%1").arg(fileName));
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        QMessageBox::critical(this, "Load Failed",
                            "Invalid save file format.");
        return;
    }

    QMessageBox::information(this, "Load Not Implemented",
                           "Loading games is not yet fully implemented.\n"
                           "This will require recreating the entire game state.");

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
    QMessageBox::about(this, "About Conquest of the Empire",
                      "<h3>Conquest of the Empire</h3>"
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
            ::Position pos1 = city1->getPosition();  // City uses global Position
            QString territory1 = city1->getTerritoryName();

            for (int j = i + 1; j < cities.size(); ++j) {
                City *city2 = cities[j];
                ::Position pos2 = city2->getPosition();  // City uses global Position
                QString territory2 = city2->getTerritoryName();

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
