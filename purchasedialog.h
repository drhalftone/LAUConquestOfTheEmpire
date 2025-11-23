#ifndef PURCHASEDIALOG_H
#define PURCHASEDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QMap>
#include <QString>
#include <QTimer>
#include "common.h"

// Structure to hold information about territories available for city placement
struct CityPlacementOption {
    QString territoryName;
    Position position;
};

// Structure to hold information about existing cities that can be fortified
struct FortificationOption {
    QString territoryName;
    Position position;
};

// Structure to hold information about sea borders for galley placement
struct GalleyPlacementOption {
    Position seaPosition;
    QString seaTerritoryName;
    QString direction;  // "North", "South", "East", "West"
};

// Structure to return what was purchased
struct PurchaseResult {
    // Troops (go to home province)
    int infantry;
    int cavalry;
    int catapults;

    // Cities with their locations
    struct CityPurchase {
        QString territoryName;
        Position position;
        bool fortified;
    };
    QList<CityPurchase> cities;

    // Fortifications for existing cities
    QStringList fortifications;  // List of territory names to fortify

    // Galleys with their sea border
    struct GalleyPurchase {
        Position seaBorder;
        int count;
    };
    QList<GalleyPurchase> galleys;

    int totalCost;
};

class PurchaseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PurchaseDialog(QChar player,
                           int availableMoney,
                           int inflationMultiplier,
                           const QList<CityPlacementOption> &cityOptions,
                           const QList<FortificationOption> &fortificationOptions,
                           const QList<GalleyPlacementOption> &galleyOptions,
                           int currentGalleyCount,
                           int availableInfantry,
                           int availableCavalry,
                           int availableCatapults,
                           int availableGalleys,
                           QWidget *parent = nullptr,
                           bool combatUnitsOnly = false);

    // Get the purchase result
    PurchaseResult getPurchaseResult() const;

    // === AI Integration ===
    // Structure representing an item the AI can purchase
    struct PurchaseMenuItem {
        QString itemType;       // "Infantry", "Cavalry", "Catapult", "Galley", "City", "FortifiedCity", "Fortification"
        int currentPrice;       // Price with inflation applied
        int maxQuantity;        // Max that can be bought (limited by money and availability)
        QString location;       // For placed items (city territory, galley sea border)
        Position position;      // Grid position for placed items
    };

    // Get the menu of available items for AI to read
    QList<PurchaseMenuItem> getAvailableItems() const;

    // AI auto-mode: programmatically make purchases and accept dialog
    // purchases maps item description to quantity (e.g., "Infantry" -> 2, "City:Roma" -> 1)
    void setupAIAutoMode(int delayMs, const QMap<QString, int> &purchases);

private slots:
    void updateTotals();
    void onPurchaseClicked();

private:
    void setupUI();
    int getCurrentPrice(int basePrice) const;
    QPixmap createIconCollage(const QString &iconPath, int count) const;

    QChar m_player;
    int m_availableMoney;
    int m_inflationMultiplier;
    int m_totalSpent;
    int m_currentGalleyCount;

    // Available pieces in the game box
    int m_availableInfantry;
    int m_availableCavalry;
    int m_availableCatapults;
    int m_availableGalleys;

    // Base prices
    static const int MAX_GALLEYS = 6;
    static const int INFANTRY_BASE_COST = 10;
    static const int CAVALRY_BASE_COST = 20;
    static const int CATAPULT_BASE_COST = 30;
    static const int GALLEY_BASE_COST = 20;
    static const int CITY_BASE_COST = 30;
    static const int FORTIFICATION_BASE_COST = 20;

    // Input data
    QList<CityPlacementOption> m_cityOptions;
    QList<FortificationOption> m_fortificationOptions;
    QList<GalleyPlacementOption> m_galleyOptions;

    // Troop spinboxes
    QSpinBox *m_infantrySpinBox;
    QSpinBox *m_cavalrySpinBox;
    QSpinBox *m_catapultSpinBox;

    // City checkboxes (maps checkbox to territory option)
    QMap<QCheckBox*, CityPlacementOption> m_cityCheckboxes;
    QMap<QCheckBox*, CityPlacementOption> m_fortifiedCityCheckboxes;

    // Fortification checkboxes (for existing cities)
    QMap<QCheckBox*, FortificationOption> m_fortificationCheckboxes;

    // Galley spinboxes (maps spinbox to sea border option)
    QMap<QSpinBox*, GalleyPlacementOption> m_galleySpinboxes;

    // Summary labels
    QLabel *m_availableLabel;
    QLabel *m_spendingLabel;
    QLabel *m_remainingLabel;

    // Purchase button
    QPushButton *m_purchaseButton;

    // AI mode flag - skip confirmation dialog
    bool m_aiAutoMode = false;

    // Combat units only mode - hide cities/galleys/fortifications
    bool m_combatUnitsOnly = false;
};

#endif // PURCHASEDIALOG_H
