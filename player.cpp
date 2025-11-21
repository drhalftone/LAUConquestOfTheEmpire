#include "player.h"
#include "mapwidget.h"
#include <QDebug>

Player::Player(QChar id, const QString &homeProvinceName, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_color(getColorForPlayer(id))
    , m_wallet(100)  // Start each player with 100 talents
    , m_homeProvinceName(homeProvinceName)
    , m_hasHomeFortifiedCity(true)  // Every player starts with a fortified city
    , m_isMyTurn(false)  // Starts as false, first player's turn is set in main()
{
    qDebug() << "Creating Player" << m_id << "with home province:" << m_homeProvinceName;

    // TEMPORARY: Create dummy position for piece/building constructors
    // This will be removed when GamePiece/Building are updated to not need Position
    Position tempPos = {0, 0};

    // Create Caesar at home province
    CaesarPiece *caesar = new CaesarPiece(m_id, tempPos, this);
    caesar->setTerritoryName(m_homeProvinceName);
    qDebug() << "  Caesar created, territory name:" << caesar->getTerritoryName();
    m_caesars.append(caesar);

    // Create 6 Generals at home province (per 1984 rules)
    for (int i = 1; i <= 6; ++i) {
        GeneralPiece *general = new GeneralPiece(m_id, tempPos, i, this);
        general->setTerritoryName(m_homeProvinceName);
        m_generals.append(general);
    }

    // Create 4 Infantry at home province (per 1984 rules)
    for (int i = 1; i <= 4; ++i) {
        InfantryPiece *infantry = new InfantryPiece(m_id, tempPos, this);
        infantry->setTerritoryName(m_homeProvinceName);
        m_infantry.append(infantry);
    }

    // Create fortified city at home province
    City *homeCity = new City(m_id, tempPos, m_homeProvinceName, true, this);
    m_cities.append(homeCity);

    // Claim the home province territory
    m_ownedTerritories.append(m_homeProvinceName);

    // Note: m_hasHomeFortifiedCity flag is also set to true for quick checking
}

Player::~Player()
{
    // Clean up all pieces - we own them
    qDeleteAll(m_caesars);
    qDeleteAll(m_generals);
    qDeleteAll(m_capturedGenerals);
    qDeleteAll(m_infantry);
    qDeleteAll(m_cavalry);
    qDeleteAll(m_catapults);
    qDeleteAll(m_galleys);

    // Clean up all buildings - we own them
    qDeleteAll(m_cities);
    qDeleteAll(m_roads);
}

QColor Player::getColorForPlayer(QChar playerId) const
{
    switch (playerId.toLatin1()) {
        case 'A': return Qt::red;
        case 'B': return Qt::green;
        case 'C': return Qt::blue;
        case 'D': return Qt::yellow;
        case 'E': return Qt::black;
        case 'F': return QColor(255, 165, 0); // Orange
        default: return Qt::gray;
    }
}

// ========== Get All Pieces ==========

QList<GamePiece*> Player::getAllPieces() const
{
    QList<GamePiece*> allPieces;

    // Add all caesars
    for (CaesarPiece *piece : m_caesars) {
        allPieces.append(piece);
    }

    // Add all generals
    for (GeneralPiece *piece : m_generals) {
        allPieces.append(piece);
    }

    // Add all infantry
    for (InfantryPiece *piece : m_infantry) {
        allPieces.append(piece);
    }

    // Add all cavalry
    for (CavalryPiece *piece : m_cavalry) {
        allPieces.append(piece);
    }

    // Add all catapults
    for (CatapultPiece *piece : m_catapults) {
        allPieces.append(piece);
    }

    // Add all galleys
    for (GalleyPiece *piece : m_galleys) {
        allPieces.append(piece);
    }

    return allPieces;
}

QList<Building*> Player::getAllBuildings() const
{
    QList<Building*> allBuildings;

    // Add all cities
    for (City *building : m_cities) {
        allBuildings.append(building);
    }

    // Add all roads
    for (Road *building : m_roads) {
        allBuildings.append(building);
    }

    return allBuildings;
}

// ========== Add Pieces ==========

void Player::addCaesar(CaesarPiece *piece)
{
    if (piece && piece->getPlayer() == m_id) {
        m_caesars.append(piece);
        emit pieceAdded(piece);
    }
}

void Player::addGeneral(GeneralPiece *piece)
{
    if (piece && piece->getPlayer() == m_id) {
        m_generals.append(piece);
        emit pieceAdded(piece);
    }
}

void Player::addCapturedGeneral(GeneralPiece *piece)
{
    if (piece) {
        // Don't check player ID - captured generals keep their original player ID
        m_capturedGenerals.append(piece);
        emit pieceAdded(piece);
    }
}

void Player::addInfantry(InfantryPiece *piece)
{
    if (piece && piece->getPlayer() == m_id) {
        m_infantry.append(piece);
        emit pieceAdded(piece);
    }
}

void Player::addCavalry(CavalryPiece *piece)
{
    if (piece && piece->getPlayer() == m_id) {
        m_cavalry.append(piece);
        emit pieceAdded(piece);
    }
}

void Player::addCatapult(CatapultPiece *piece)
{
    if (piece && piece->getPlayer() == m_id) {
        m_catapults.append(piece);
        emit pieceAdded(piece);
    }
}

void Player::addGalley(GalleyPiece *piece)
{
    if (piece && piece->getPlayer() == m_id) {
        m_galleys.append(piece);
        emit pieceAdded(piece);
    }
}

// ========== Add Buildings ==========

void Player::addCity(City *city)
{
    if (city && city->getOwner() == m_id) {
        m_cities.append(city);
        emit buildingAdded(city);
    }
}

void Player::addRoad(Road *road)
{
    if (road && road->getOwner() == m_id) {
        m_roads.append(road);
        emit buildingAdded(road);
    }
}

// ========== Remove Pieces ==========

bool Player::removeCaesar(CaesarPiece *piece)
{
    if (m_caesars.removeOne(piece)) {
        emit pieceRemoved(piece);
        return true;
    }
    return false;
}

bool Player::removeGeneral(GeneralPiece *piece)
{
    if (m_generals.removeOne(piece)) {
        emit pieceRemoved(piece);
        return true;
    }
    return false;
}

bool Player::removeCapturedGeneral(GeneralPiece *piece)
{
    if (m_capturedGenerals.removeOne(piece)) {
        emit pieceRemoved(piece);
        return true;
    }
    return false;
}

bool Player::removeInfantry(InfantryPiece *piece)
{
    if (m_infantry.removeOne(piece)) {
        emit pieceRemoved(piece);
        return true;
    }
    return false;
}

bool Player::removeCavalry(CavalryPiece *piece)
{
    if (m_cavalry.removeOne(piece)) {
        emit pieceRemoved(piece);
        return true;
    }
    return false;
}

bool Player::removeCatapult(CatapultPiece *piece)
{
    if (m_catapults.removeOne(piece)) {
        emit pieceRemoved(piece);
        return true;
    }
    return false;
}

bool Player::removeGalley(GalleyPiece *piece)
{
    if (m_galleys.removeOne(piece)) {
        emit pieceRemoved(piece);
        return true;
    }
    return false;
}

// ========== Remove Buildings ==========

bool Player::removeCity(City *city)
{
    if (m_cities.removeOne(city)) {
        emit buildingRemoved(city);
        return true;
    }
    return false;
}

bool Player::removeRoad(Road *road)
{
    if (m_roads.removeOne(road)) {
        emit buildingRemoved(road);
        return true;
    }
    return false;
}

// ========== Query Pieces by Territory Name ==========

QList<GamePiece*> Player::getPiecesAtTerritory(const QString &territoryName) const
{
    QList<GamePiece*> pieces;

    // Check all piece types
    for (CaesarPiece *piece : m_caesars) {
        if (piece->getTerritoryName() == territoryName) {
            pieces.append(piece);
        }
    }

    for (GeneralPiece *piece : m_generals) {
        if (piece->getTerritoryName() == territoryName) {
            pieces.append(piece);
        }
    }

    for (InfantryPiece *piece : m_infantry) {
        if (piece->getTerritoryName() == territoryName) {
            pieces.append(piece);
        }
    }

    for (CavalryPiece *piece : m_cavalry) {
        if (piece->getTerritoryName() == territoryName) {
            pieces.append(piece);
        }
    }

    for (CatapultPiece *piece : m_catapults) {
        if (piece->getTerritoryName() == territoryName) {
            pieces.append(piece);
        }
    }

    for (GalleyPiece *piece : m_galleys) {
        if (piece->getTerritoryName() == territoryName) {
            pieces.append(piece);
        }
    }

    return pieces;
}

QList<CaesarPiece*> Player::getCaesarsAtTerritory(const QString &territoryName) const
{
    QList<CaesarPiece*> pieces;
    for (CaesarPiece *piece : m_caesars) {
        if (piece->getTerritoryName() == territoryName) {
            pieces.append(piece);
        }
    }
    return pieces;
}

QList<GeneralPiece*> Player::getGeneralsAtTerritory(const QString &territoryName) const
{
    QList<GeneralPiece*> pieces;
    for (GeneralPiece *piece : m_generals) {
        if (piece->getTerritoryName() == territoryName) {
            pieces.append(piece);
        }
    }
    return pieces;
}

QList<InfantryPiece*> Player::getInfantryAtTerritory(const QString &territoryName) const
{
    QList<InfantryPiece*> pieces;
    for (InfantryPiece *piece : m_infantry) {
        if (piece->getTerritoryName() == territoryName) {
            pieces.append(piece);
        }
    }
    return pieces;
}

QList<CavalryPiece*> Player::getCavalryAtTerritory(const QString &territoryName) const
{
    QList<CavalryPiece*> pieces;
    for (CavalryPiece *piece : m_cavalry) {
        if (piece->getTerritoryName() == territoryName) {
            pieces.append(piece);
        }
    }
    return pieces;
}

QList<CatapultPiece*> Player::getCatapultsAtTerritory(const QString &territoryName) const
{
    QList<CatapultPiece*> pieces;
    for (CatapultPiece *piece : m_catapults) {
        if (piece->getTerritoryName() == territoryName) {
            pieces.append(piece);
        }
    }
    return pieces;
}

QList<GalleyPiece*> Player::getGalleysAtTerritory(const QString &territoryName) const
{
    QList<GalleyPiece*> pieces;
    for (GalleyPiece *piece : m_galleys) {
        if (piece->getTerritoryName() == territoryName) {
            pieces.append(piece);
        }
    }
    return pieces;
}

// ========== Query Buildings by Territory Name ==========

QList<Building*> Player::getBuildingsAtTerritory(const QString &territoryName) const
{
    QList<Building*> buildings;

    // Check cities
    for (City *city : m_cities) {
        if (city->getTerritoryName() == territoryName) {
            buildings.append(city);
        }
    }

    // Check roads
    for (Road *road : m_roads) {
        if (road->getTerritoryName() == territoryName) {
            buildings.append(road);
        }
    }

    return buildings;
}

QList<City*> Player::getCitiesAtTerritory(const QString &territoryName) const
{
    QList<City*> cities;
    for (City *city : m_cities) {
        if (city->getTerritoryName() == territoryName) {
            cities.append(city);
        }
    }
    return cities;
}

QList<Road*> Player::getRoadsAtTerritory(const QString &territoryName) const
{
    QList<Road*> roads;
    for (Road *road : m_roads) {
        if (road->getTerritoryName() == territoryName) {
            roads.append(road);
        }
    }
    return roads;
}

City* Player::getCityAtTerritory(const QString &territoryName) const
{
    for (City *city : m_cities) {
        if (city->getTerritoryName() == territoryName) {
            return city;  // Return first city found
        }
    }
    return nullptr;  // No city found
}

// Note: Position-based query methods removed - use territory-based versions instead:
// - getPiecesAtTerritory()
// - getBuildingsAtTerritory()
// - getCityAtTerritory()

// ========== Count Methods ==========

int Player::getTotalPieceCount() const
{
    return m_caesars.size() + m_generals.size() + m_infantry.size() +
           m_cavalry.size() + m_catapults.size() + m_galleys.size();
}

int Player::getTotalBuildingCount() const
{
    return m_cities.size() + m_roads.size();
}

int Player::getPieceCountAtTerritory(const QString &territoryName) const
{
    return getPiecesAtTerritory(territoryName).size();
}

int Player::getBuildingCountAtTerritory(const QString &territoryName) const
{
    return getBuildingsAtTerritory(territoryName).size();
}

// ========== Money/Wallet Management ==========

void Player::addMoney(int amount)
{
    if (amount <= 0) {
        return;  // Don't add negative or zero amounts
    }

    m_wallet += amount;
    emit walletChanged(m_wallet);
    emit moneyAdded(amount, m_wallet);
}

bool Player::spendMoney(int amount)
{
    if (amount <= 0) {
        return true;  // Nothing to spend
    }

    if (m_wallet < amount) {
        // Insufficient funds
        emit insufficientFunds(amount, m_wallet);
        return false;
    }

    m_wallet -= amount;
    emit walletChanged(m_wallet);
    emit moneySpent(amount, m_wallet);
    return true;
}

void Player::setWallet(int amount)
{
    if (amount < 0) {
        amount = 0;  // Don't allow negative wallet
    }

    m_wallet = amount;
    emit walletChanged(m_wallet);
}

// ========== Territory Ownership Management ==========

bool Player::ownsTerritory(const QString &territoryName) const
{
    return m_ownedTerritories.contains(territoryName);
}

void Player::claimTerritory(const QString &territoryName)
{
    // Don't add duplicates
    if (!m_ownedTerritories.contains(territoryName)) {
        m_ownedTerritories.append(territoryName);
        emit territoryClaimed(territoryName);
    }
}

void Player::unclaimTerritory(const QString &territoryName)
{
    if (m_ownedTerritories.removeOne(territoryName)) {
        emit territoryUnclaimed(territoryName);
    }
}

void Player::claimTerritories(const QList<QString> &territoryNames)
{
    for (const QString &territoryName : territoryNames) {
        claimTerritory(territoryName);
    }
}

void Player::clearAllTerritories()
{
    m_ownedTerritories.clear();
    emit territoriesCleared();
}

// ========== Turn Management ==========

void Player::startTurn()
{
    m_isMyTurn = true;

    // Reset movement for all pieces to their default values
    // Caesars: 2 moves
    for (CaesarPiece *piece : m_caesars) {
        piece->setMovesRemaining(2);
    }

    // Generals: 2 moves
    for (GeneralPiece *piece : m_generals) {
        piece->setMovesRemaining(2);
    }

    // Infantry: 1 move
    for (InfantryPiece *piece : m_infantry) {
        piece->setMovesRemaining(1);
    }

    // Cavalry: 2 moves
    for (CavalryPiece *piece : m_cavalry) {
        piece->setMovesRemaining(2);
    }

    // Catapults: 1 move
    for (CatapultPiece *piece : m_catapults) {
        piece->setMovesRemaining(1);
    }

    // Galleys: 2 moves, and reset transport flag
    for (GalleyPiece *piece : m_galleys) {
        piece->setMovesRemaining(2);
        piece->resetTransportFlag();
        // Clear any leader aboard from previous turn (they should have disembarked)
        piece->setLeaderAboard(0);
    }

    emit turnStarted();
}

void Player::endTurn()
{
    m_isMyTurn = false;
    // Future: Could add end-of-turn logic here (cleanup, etc.)
    emit turnEnded();
}

int Player::collectTaxes(MapWidget *mapWidget)
{
    if (!mapWidget) {
        return 0;
    }

    int totalTaxes = 0;

    // Iterate through all owned territories and sum their tax values
    for (const QString &territoryName : m_ownedTerritories) {
        // Find the territory on the map and get its value
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 12; ++col) {
                if (mapWidget->getTerritoryNameAt(row, col) == territoryName) {
                    int territoryValue = mapWidget->getTerritoryValueAt(row, col);
                    totalTaxes += territoryValue;
                    break; // Found the territory, move to next one
                }
            }
        }
    }

    // Add 5 talents for each city owned (cities are worth 5 each)
    int cityTaxes = m_cities.size() * 5;
    totalTaxes += cityTaxes;

    // Add taxes to wallet
    if (totalTaxes > 0) {
        addMoney(totalTaxes);
    }

    return totalTaxes;
}
