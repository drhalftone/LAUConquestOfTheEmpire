#ifndef PLAYER_H
#define PLAYER_H

#include <QObject>
#include <QList>
#include <QColor>
#include <QString>
#include "common.h"
#include "gamepiece.h"
#include "building.h"

class Player : public QObject
{
    Q_OBJECT

public:
    // Constructor - automatically creates Caesar, 5 Generals, and fortified city at home province
    explicit Player(QChar id, const QString &homeProvinceName, QObject *parent = nullptr);
    ~Player();

    // Player identification
    QChar getId() const { return m_id; }
    QColor getColor() const { return m_color; }

    // Piece inventory management - separate lists for each type
    const QList<CaesarPiece*>& getCaesars() const { return m_caesars; }
    const QList<GeneralPiece*>& getGenerals() const { return m_generals; }
    const QList<GeneralPiece*>& getCapturedGenerals() const { return m_capturedGenerals; }
    const QList<InfantryPiece*>& getInfantry() const { return m_infantry; }
    const QList<CavalryPiece*>& getCavalry() const { return m_cavalry; }
    const QList<CatapultPiece*>& getCatapults() const { return m_catapults; }
    const QList<GalleyPiece*>& getGalleys() const { return m_galleys; }

    // Building inventory management
    const QList<City*>& getCities() const { return m_cities; }
    const QList<Road*>& getRoads() const { return m_roads; }

    // Get all pieces (combined from all lists)
    QList<GamePiece*> getAllPieces() const;

    // Get all buildings (combined from all lists)
    QList<Building*> getAllBuildings() const;

    // Add pieces to inventory
    void addCaesar(CaesarPiece *piece);
    void addGeneral(GeneralPiece *piece);
    void addCapturedGeneral(GeneralPiece *piece);
    void addInfantry(InfantryPiece *piece);
    void addCavalry(CavalryPiece *piece);
    void addCatapult(CatapultPiece *piece);
    void addGalley(GalleyPiece *piece);

    // Add buildings to inventory
    void addCity(City *city);
    void addRoad(Road *road);

    // Remove pieces from inventory
    bool removeCaesar(CaesarPiece *piece);
    bool removeGeneral(GeneralPiece *piece);
    bool removeCapturedGeneral(GeneralPiece *piece);
    bool removeInfantry(InfantryPiece *piece);
    bool removeCavalry(CavalryPiece *piece);
    bool removeCatapult(CatapultPiece *piece);
    bool removeGalley(GalleyPiece *piece);

    // Remove buildings from inventory
    bool removeCity(City *city);
    bool removeRoad(Road *road);

    // Query pieces by location (territory name)
    QList<GamePiece*> getPiecesAtTerritory(const QString &territoryName) const;
    QList<CaesarPiece*> getCaesarsAtTerritory(const QString &territoryName) const;
    QList<GeneralPiece*> getGeneralsAtTerritory(const QString &territoryName) const;
    QList<InfantryPiece*> getInfantryAtTerritory(const QString &territoryName) const;
    QList<CavalryPiece*> getCavalryAtTerritory(const QString &territoryName) const;
    QList<CatapultPiece*> getCatapultsAtTerritory(const QString &territoryName) const;
    QList<GalleyPiece*> getGalleysAtTerritory(const QString &territoryName) const;

    // Query buildings by location (territory name)
    QList<Building*> getBuildingsAtTerritory(const QString &territoryName) const;
    QList<City*> getCitiesAtTerritory(const QString &territoryName) const;
    QList<Road*> getRoadsAtTerritory(const QString &territoryName) const;
    City* getCityAtTerritory(const QString &territoryName) const;  // Returns first city found or nullptr

    // Count pieces
    int getTotalPieceCount() const;
    int getCaesarCount() const { return m_caesars.size(); }
    int getGeneralCount() const { return m_generals.size(); }
    int getCapturedGeneralCount() const { return m_capturedGenerals.size(); }
    int getInfantryCount() const { return m_infantry.size(); }
    int getCavalryCount() const { return m_cavalry.size(); }
    int getCatapultCount() const { return m_catapults.size(); }
    int getGalleyCount() const { return m_galleys.size(); }

    // Count buildings
    int getTotalBuildingCount() const;
    int getCityCount() const { return m_cities.size(); }
    int getRoadCount() const { return m_roads.size(); }

    // Count pieces at a specific location
    int getPieceCountAtTerritory(const QString &territoryName) const;

    // Count buildings at a specific location
    int getBuildingCountAtTerritory(const QString &territoryName) const;

    // Economic data - Money/Wallet management
    int getWallet() const { return m_wallet; }
    int getMoney() const { return m_wallet; }  // Alias for getWallet()

    // Add money (from taxes, selling, etc.)
    void addMoney(int amount);
    void collectTax(int amount) { addMoney(amount); }  // Alias for tax collection

    // Remove money (for purchases, ransoms, etc.)
    bool spendMoney(int amount);  // Returns true if successful, false if insufficient funds
    bool payRansom(int amount) { return spendMoney(amount); }  // Alias for ransom payment
    bool purchaseItem(int cost) { return spendMoney(cost); }  // Alias for purchases

    // Check if player can afford something
    bool canAfford(int amount) const { return m_wallet >= amount; }
    bool hasEnoughMoney(int amount) const { return m_wallet >= amount; }

    // Set wallet directly (for game initialization or loading saved games)
    void setWallet(int amount);

    // Home province and structures (set in constructor)
    QString getHomeProvinceName() const { return m_homeProvinceName; }

    // The home province automatically has a fortified city (created in constructor)
    bool hasCity() const { return m_hasHomeFortifiedCity; }
    bool hasFortification() const { return m_hasHomeFortifiedCity; }

    // Territory ownership (icon markers) - territories claimed by this player
    const QList<QString>& getOwnedTerritories() const { return m_ownedTerritories; }
    bool ownsTerritory(const QString &territoryName) const;
    int getOwnedTerritoryCount() const { return m_ownedTerritories.size(); }

    // Claim/unclaim territories
    void claimTerritory(const QString &territoryName);
    void unclaimTerritory(const QString &territoryName);
    bool removeTerritory(const QString &territoryName) { unclaimTerritory(territoryName); return true; }  // Alias

    // Claim multiple territories at once
    void claimTerritories(const QList<QString> &territoryNames);

    // Clear all owned territories (for conquest/defeat scenarios)
    void clearAllTerritories();

    // Turn management
    void startTurn();  // Called at the beginning of player's turn - resets movement for all pieces
    void endTurn();    // Called at the end of player's turn
    bool isMyTurn() const { return m_isMyTurn; }

    // Tax collection - called at end of turn to collect taxes from owned territories
    // Returns the amount collected
    int collectTaxes(class MapWidget *mapWidget);

signals:
    void turnStarted();
    void turnEnded();
    void pieceAdded(GamePiece *piece);
    void pieceRemoved(GamePiece *piece);
    void buildingAdded(Building *building);
    void buildingRemoved(Building *building);

    // Economic signals
    void walletChanged(int newAmount);
    void moneyAdded(int amount, int newTotal);
    void moneySpent(int amount, int newTotal);
    void insufficientFunds(int required, int available);  // Emitted when trying to spend more than available

    // Territory ownership signals
    void territoryClaimed(QString territoryName);
    void territoryUnclaimed(QString territoryName);
    void territoriesCleared();  // All territories lost

private:
    QChar m_id;                           // Player ID: 'A' through 'F'
    QColor m_color;                       // Player color

    // Separate inventory lists for each piece type
    QList<CaesarPiece*> m_caesars;        // Should only have 1
    QList<GeneralPiece*> m_generals;      // Should have up to 5
    QList<GeneralPiece*> m_capturedGenerals;  // Captured enemy generals (preserves serial numbers)
    QList<InfantryPiece*> m_infantry;     // Can have many
    QList<CavalryPiece*> m_cavalry;       // Can have many
    QList<CatapultPiece*> m_catapults;    // Can have many
    QList<GalleyPiece*> m_galleys;        // Can have many

    // Building inventory lists
    QList<City*> m_cities;                // Can have many cities
    QList<Road*> m_roads;                 // Can have many roads

    // Economic data
    int m_wallet;                         // Accumulated wealth in talents

    // Home province data
    QString m_homeProvinceName;           // Name of home territory
    bool m_hasHomeFortifiedCity;          // Every player starts with a fortified city at home

    // Territory ownership (icon markers)
    QList<QString> m_ownedTerritories;    // List of territory names owned by this player

    // Turn management
    bool m_isMyTurn;                      // Is it currently this player's turn?

    // Helper function to get color based on player ID
    QColor getColorForPlayer(QChar playerId) const;
};

#endif // PLAYER_H
